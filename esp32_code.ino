#include <DHT.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ========== CONFIGURATION WiFi ==========
#define WIFI_SSID "Abdelmajid"              // Remplacez par votre SSID WiFi
#define WIFI_PASSWORD "sami sami sami"      // Remplacez par votre mot de passe

// ========== CONFIGURATION FIREBASE (NOUVELLE CLÉ) ==========
#define API_KEY "AIzaSyBC4Z3qMepUxtRoOTCHgFzXFB_JzAwG3H1"  // ← NOUVELLE CLÉ
#define DATABASE_URL "https://station-meteo-233d4-default-rtdb.europe-west1.firebasedatabase.app/"  // ← NOUVELLE URL

// ========== BROCHES DES CAPTEURS ==========
#define DHTPIN 4
#define DHTTYPE DHT11
#define RAIN_PIN 34
#define LIGHT_PIN 35
#define WIND_PIN 18

DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

float temperature = 0, humidity = 0, windSpeed = 0;
int rainValue = 0, lightPercent = 0;

int lastWindState = HIGH;
int windPulseCount = 0;
unsigned long lastWindCalc = 0;
const float DISTANCE_PER_PULSE = 0.377;

unsigned long lastSendTime = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n==========================================");
  Serial.println("      STATION METEO IoT - ESP32");
  Serial.println("==========================================\n");
  
  dht.begin();
  pinMode(RAIN_PIN, INPUT);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(WIND_PIN, INPUT_PULLUP);
  
  Serial.println("✅ Capteurs initialises");
  lastWindCalc = millis();
  
  Serial.print("📡 Connexion WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connecte !");
  Serial.print("📍 IP: ");
  Serial.println(WiFi.localIP());
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  Serial.println("\n🔗 Connexion a Firebase...");
  
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Firebase connecte avec succes !");
    signupOK = true;
  } else {
    Serial.println("❌ Erreur de connexion Firebase");
    Serial.println("Verifiez que l'authentification anonyme est activee");
  }
  
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("\n📊 Demarrage des mesures...\n");
}

void loop() {
  int currentState = digitalRead(WIND_PIN);
  if (lastWindState == HIGH && currentState == LOW) {
    windPulseCount++;
  }
  lastWindState = currentState;
  
  if (millis() - lastWindCalc >= 2000) {
    windSpeed = (windPulseCount * DISTANCE_PER_PULSE) / 2.0;
    windPulseCount = 0;
    lastWindCalc = millis();
  }
  
  if (millis() - lastSendTime >= 5000) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    rainValue = analogRead(RAIN_PIN);
    int lightRaw = analogRead(LIGHT_PIN);
    lightPercent = map(lightRaw, 4095, 0, 0, 100);
    
    if (isnan(temperature)) temperature = 0;
    if (isnan(humidity)) humidity = 0;
    if (lightPercent < 0) lightPercent = 0;
    if (lightPercent > 100) lightPercent = 100;
    
    Serial.println("-----------------------------------------");
    Serial.print("🌡️ Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("💧 Humidite: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("☀️ Luminosite: ");
    Serial.print(lightPercent);
    Serial.println(" %");
    Serial.print("💨 Vent: ");
    Serial.print(windSpeed);
    Serial.println(" m/s");
    
    if (rainValue > 3000) {
      Serial.println("☔ Pluie: SEC - Pas de pluie");
    } else if (rainValue > 1500) {
      Serial.println("☔ Pluie: Humidite elevee");
    } else if (rainValue > 500) {
      Serial.println("☔ Pluie: Pluie moderee");
    } else {
      Serial.println("☔ Pluie: PLUIE INTENSE !");
    }
    
    if (signupOK) {
      FirebaseJson json;
      json.set("temperature", temperature);
      json.set("humidity", humidity);
      json.set("light", lightPercent);
      json.set("rain", rainValue);
      json.set("wind", windSpeed);
      json.set("timestamp", millis());
      
      if (Firebase.RTDB.push(&fbdo, "/mesures", &json)) {
        Serial.println("✅ Donnees envoyees a Firebase !");
      } else {
        Serial.print("❌ Erreur Firebase: ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      Serial.println("❌ Firebase non connecte");
    }
    
    Serial.println("-----------------------------------------\n");
    lastSendTime = millis();
  }
  
  delay(10);
}
