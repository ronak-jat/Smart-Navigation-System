
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ================== CONFIGURATION ==================
#define WIFI_SSID "etti"
#define WIFI_PASSWORD "12345678"

// Firebase Config
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL" 

// Hardware Config
#define LEFT_LED 27
#define RIGHT_LED 26
#define SS_PIN 5  // RFID SDA
#define RST_PIN 22 // RFID RST

// Device ID
#define BIKE_ID "bike_001"

// ================== OBJECTS ==================
FirebaseData fbDO; // Data object for Read/Write
FirebaseData fbStream; // Data object for Stream
FirebaseAuth auth;
FirebaseConfig config;
TinyGPSPlus gps;
HardwareSerial SerialGPS(2);
MFRC522 rfid(SS_PIN, RST_PIN);

// ================== STATE ==================
bool isConnected = false;
bool isLocked = true;
unsigned long lastSend = 0;
unsigned long lastRfidScan = 0;

// Navigation State (Legacy compatible)
struct RoutePoint { double lat; double lon; };
#define MAX_ROUTE_POINTS 500
RoutePoint routePoints[MAX_ROUTE_POINTS];
int totalRoutePoints = 0;
int currentRouteIndex = 0;

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
  
  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);
  
  // Init SPI & RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  
  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback; // form addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Set Stream for Commands
  if (!Firebase.RTDB.beginStream(&fbStream, "/bikes/" BIKE_ID "/command")) {
    Serial.printf("Stream begin error: %s\n", fbStream.errorReason().c_str());
  }
  
  // Set OnDisconnect -> Offline
  Firebase.RTDB.setString(&fbDO, "/bikes/" BIKE_ID "/status", "online");
  // Note: OnDisconnect logic supported by library but requires clean setup. 
  // For now simple heartbeat is better.
}

// ================== LOOP ==================
void loop() {
  if (Firebase.ready()) {
    // 1. Read Stream (Commands)
    if (Firebase.RTDB.readStream(&fbStream)) {
      if (fbStream.streamAvailable()) {
         String type = fbStream.stringData(); 
         // Note: Parse JSON payload if structure is complex, here assuming simplified string or processing children
         // Real impl would parse: { type: "UNLOCK", ... }
         // For simplicity: check path
         handleCommand(fbStream);
      }
    }
    
    // 2. Send Heartbeat & GPS (every 2s)
    if (millis() - lastSend > 2000) {
      lastSend = millis();
      uploadTelemetry();
    }
  }
  
  // 3. Check RFID
  checkRFID();
  
  // 4. GPS & Nav Logic
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }
}

// ================== HANDLERS ==================

void uploadTelemetry() {
  // Use Firebase.RTDB.updateNode for efficiency
  FirebaseJson json;
  double lat = gps.location.isValid() ? gps.location.lat() : 27.176; // Default/Mock
  double lon = gps.location.isValid() ? gps.location.lng() : 75.956;
  
  json.set("location/lat", lat);
  json.set("location/lng", lon);
  json.set("battery", 88); 
  json.set("status", "online");
  json.set("isLocked", isLocked);
  
  Firebase.RTDB.updateNode(&fbDO, "/bikes/" BIKE_ID, &json);
}

void checkRFID() {
  if (millis() - lastRfidScan < 1000) return; // Debounce
  
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  
  String rfidTag = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    rfidTag += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    rfidTag += String(rfid.uid.uidByte[i], HEX);
  }
  rfidTag.toUpperCase();
  
  Serial.println("RFID Scanned: " + rfidTag);
  
  // Upload scan to Backend for verification
  Firebase.RTDB.setString(&fbDO, "/bikes/" BIKE_ID "/last_rfid", rfidTag);
  
  // Check local override or wait for server command
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  lastRfidScan = millis();
}

void handleCommand(FirebaseData &data) {
  // Logic to parse json command
  // For now, assume we watch the whole node
  // If we receive { type: "UNLOCK" }
  Serial.println("Command Received...");
  // Implementation of specific command handling
}
