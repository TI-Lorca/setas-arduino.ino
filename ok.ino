#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <ESPSupabase.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT Sensor
#define DHTPIN 14     // Pin del DHT
#define DHTTYPE DHT11 // Tipo de DHT
DHT dht(DHTPIN, DHTTYPE);

// WiFi settings
const char* ssid = "gal";
const char* password = "12345678";

// Supabase settings
Supabase db;
String supabase_url = "https://oeocwylsvlgimdldlzad.supabase.co";
String anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9lb2N3eWxzdmxnaW1kbGRsemFkIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjY2Nzk1MDIsImV4cCI6MjA0MjI1NTUwMn0.O3m2eAEzhcKjJvdgYp_EnPXcBQFYS7phN801axJeyR0";

String table = "mediciones";
bool upsert = false;

// Relays for heater and fan
#define RELAY_HEATER 5
#define RELAY_FAN 4

// MQ-135 pin for CO2 sensor
#define MQ135_PIN 32 // GPIO 36 (VP)

// Global variables
float temperatura, humedad, luz, co2;

void setup() {
  Serial.begin(115200);
  
  // Setup OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed to start"));
    for (;;);
  }
  
  conectarWiFi();  // Connect to Wi-Fi
  display.clearDisplay();
  
  // Start DHT sensor
  dht.begin();
  
  // Initialize Supabase
  db.begin(supabase_url, anon_key);

  // Initialize relays
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

  // Initialize CO2 sensor pin
  pinMode(MQ135_PIN, INPUT);
}

void loop() {
  // Measure temperature, humidity, light, and CO2
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
  int rawLuz = analogRead(34); // Leer valor de luz del sensor (GPIO 34)
  luz = map(rawLuz, 4096, 0, 0, 100); // Convertir 0-4096 a 0-100
  co2 = analogRead(MQ135_PIN); // Read CO2 level from MQ-135

  // Handle errors in DHT reading
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println(F("Error reading DHT sensor"));
    temperatura = 0;
    humedad = 0;
  }

  // Display data on OLED
  mostrarDatosEnOLED(temperatura, humedad, luz, co2);

  // Control environment based on temperature and humidity
  controlarAmbiente(temperatura, humedad);

  // Send data to Supabase
  if (WiFi.status() == WL_CONNECTED) {
    enviarDatosASupabase(temperatura, humedad, luz, co2);
  } else {
    conectarWiFi();
  }

  delay(60000); // Wait 10 seconds before the next measurement
  display.clearDisplay();
  delay(1000); 

}

void mostrarDatosEnOLED(float temp, float hum, float light, float co2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Display temperature
  display.setCursor(32, 26);
  display.print("Temp: ");
  display.print(temp, 1);
  display.println(" C");

  // Display humidity
  display.setCursor(32, 36);
  display.print("Hum: ");
  display.print(hum, 0);
  display.println(" %");

  // Display light
  display.setCursor(32, 46);
  display.print("Luz: ");
  display.print(light, 0);
  display.println("");

  // Display CO2 level
  display.setCursor(32, 56);
  display.print("CO2:");
  display.print(co2, 0);
  display.println("ppm");

  display.display();
}

void controlarAmbiente(float temp, float hum) {
  if (temp < 20) {
    digitalWrite(RELAY_HEATER, HIGH);  // Turn on heater
  } else {
    digitalWrite(RELAY_HEATER, LOW);   // Turn off heater
  }

  if (hum < 60) {
    digitalWrite(RELAY_FAN, HIGH);     // Turn on fan
  } else {
    digitalWrite(RELAY_FAN, LOW);      // Turn off fan
  }
}

void conectarWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}

void enviarDatosASupabase(float temp, float hum, float light, float co2) {
  if (WiFi.status() == WL_CONNECTED) {
    String JSON = "{\"temperatura\":" + String(temp, 1) + 
                  ",\"humedad\":" + String(hum, 1) + 
                  ",\"luz\":" + String(light, 0) + 
                  ",\"co2\":" + String(co2, 0) + "}";
    int code = db.insert(table, JSON, upsert);
    Serial.println("Data sent with code: " + String(code));
    db.urlQuery_reset();
  }
}
