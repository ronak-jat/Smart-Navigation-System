#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h> 
#include <WebServer.h>

// WiFi credentials
const char* ssid = "etti";
const char* password = "12345678";

// Google Maps API key (replace with your valid key)
String googleApiKey = "AIzaSyAHDynmeyBdsg-xgDbgcOkbxeZCa7PwRNY";

// GPS module
TinyGPSPlus gps;
HardwareSerial SerialGPS(2);

// LEDs
#define LEFT_LED 27
#define RIGHT_LED 26

// WebServer
WebServer server(80);

// Globals
String destinationName = "";
double destLat = 0.0;
double destLon = 0.0;
double currentLat = 0.0;
double currentLon = 0.0;
String routeGeometry = "{}"; // Google Directions JSON

// Pseudo traveler for testing
bool pseudoTravelerEnabled = false;
double pseudoLat = 0.0;
double pseudoLon = 0.0;
unsigned long lastPseudoUpdate = 0;
const unsigned long PSEUDO_UPDATE_INTERVAL = 500; // Update every 500ms for smoother movement
double pseudoSpeed = 0.15; // Movement speed (0.15 = 15% of the way to next point)

// Route points for pseudo traveler - INCREASED for detailed polyline points
#define MAX_ROUTE_POINTS 1000  // Increased from 200 to handle detailed polyline
struct RoutePoint {
  double lat;
  double lon;
} routePoints[MAX_ROUTE_POINTS];
int totalRoutePoints = 0;
int currentRouteIndex = 0;

// TurnSteps for real-time turn detection
#define MAX_STEPS 50
enum LedSignal { NONE, LEFT, RIGHT };
struct TurnStep {
  double lat;
  double lon;
  LedSignal signal;
  bool triggered;
} turnSteps[MAX_STEPS];
int totalSteps = 0;
int currentStepIndex = 0;

// Distance threshold (meters) to trigger turn LED
#define TURN_TRIGGER_DISTANCE 30.0 // Alert at 30m
const double TURN_PASS_DISTANCE = 10.0; // Done when within 10m

// For blinking timing - Fixed timing logic
const unsigned long BLINK_DURATION = 10000; // 10 seconds blinking duration
const unsigned long LED_BLINK_INTERVAL = 500; // Blink every 500ms
LedSignal activeBlink = NONE;
unsigned long blinkStartTime = 0;
unsigned long lastLedToggle = 0;
bool ledState = false;

// Forward Declarations
void handleRoot();
void handleSetDest();
void handleMap();
void handleGPS();
void handleRoute();
void handlePseudo();
void handleTogglePseudo();
void handleStatus();
void geocodeDestination(String place);
void buildRoute();
void updatePseudoTraveler();
double degreesToRadians(double deg);
double distanceMeters(double lat1, double lon1, double lat2, double lon2);
void checkTurnSteps(double lat, double lon);
void updateLEDs();
String urlEncode(String str);
void decodePolyline(String encoded, RoutePoint* points, int* totalPoints, int maxPoints); // New function

void setup() {
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);
  
  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);
  
  // Turn off LEDs initially
  digitalWrite(LEFT_LED, LOW);
  digitalWrite(RIGHT_LED, LOW);
  
  // Print SSID for debug
  Serial.println("=== ESP32 Starting ===");
  Serial.println("SSID: " + String(ssid));
  
  // WiFi connection with timeout
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  int maxAttempts = 20; // 10 seconds timeout (20 * 500ms)
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println("Signal strength: " + String(WiFi.RSSI()) + " dBm");
    Serial.println("*** Copy the IP address now! Starting main loop in 5 seconds... ***");
    delay(5000); // 5 second delay to allow copying IP address
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("WiFi status: " + String(WiFi.status()));
    Serial.println("Continuing without WiFi...");
  }
  
  server.on("/", handleRoot);
  server.on("/setdest", handleSetDest);
  server.on("/map", handleMap);
  server.on("/gps.json", handleGPS);
  server.on("/route.json", handleRoute);
  server.on("/pseudo.json", handlePseudo);
  server.on("/togglepseudo", handleTogglePseudo);
  server.on("/status.json", handleStatus);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  
  // Update GPS data
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
    if (gps.location.isUpdated()) {
      currentLat = gps.location.lat();
      currentLon = gps.location.lng();
      Serial.printf("GPS Update: %.6f, %.6f\n", currentLat, currentLon);
    }
  }
  
  // Update pseudo traveler if enabled and use its position for LED control
  if (pseudoTravelerEnabled) {
    updatePseudoTraveler();
    // Use pseudo location for turn detection - LEDs follow red marker
    checkTurnSteps(pseudoLat, pseudoLon);
  } else if (currentLat != 0 && currentLon != 0) {
    // Use real GPS for turn detection - LEDs follow blue marker  
    checkTurnSteps(currentLat, currentLon);
  }
  
  // Handle LED blinking
  updateLEDs();
}

// IMPROVED: Now follows exact polyline points for smooth route following
void updatePseudoTraveler() {
  unsigned long now = millis();
  if (now - lastPseudoUpdate >= PSEUDO_UPDATE_INTERVAL && totalRoutePoints > 0) {
    lastPseudoUpdate = now;
    
    if (currentRouteIndex < totalRoutePoints - 1) {
      // Move towards next point
      double targetLat = routePoints[currentRouteIndex + 1].lat;
      double targetLon = routePoints[currentRouteIndex + 1].lon;
      
      double latDiff = targetLat - pseudoLat;
      double lonDiff = targetLon - pseudoLon;
      
      // Move a step towards target - smaller steps for smoother movement
      pseudoLat += latDiff * pseudoSpeed;
      pseudoLon += lonDiff * pseudoSpeed;
      
      // Check if we've reached the target point - smaller threshold for detailed points
      double distToTarget = distanceMeters(pseudoLat, pseudoLon, targetLat, targetLon);
      if (distToTarget < 2.0) { // Within 2 meters (reduced from 5m)
        currentRouteIndex++;
        Serial.printf("Pseudo traveler reached polyline point %d/%d\n", currentRouteIndex, totalRoutePoints);
        
        // Show progress every 50 points
        if (currentRouteIndex % 50 == 0) {
          float progress = (float)currentRouteIndex / totalRoutePoints * 100;
          Serial.printf("Route progress: %.1f%%\n", progress);
        }
      }
    } else {
      Serial.println("Pseudo traveler reached destination!");
    }
  }
}

void checkTurnSteps(double lat, double lon) {
  if (currentStepIndex < totalSteps) {
    TurnStep &step = turnSteps[currentStepIndex];
    double dist = distanceMeters(lat, lon, step.lat, step.lon);
    
    // Start blinking if approaching and not triggered yet
    if (!step.triggered && dist <= TURN_TRIGGER_DISTANCE && activeBlink == NONE) {
      String mode = pseudoTravelerEnabled ? "PSEUDO" : "GPS";
      Serial.printf("[%s] Approaching turn %d at %.1fm, blinking LED %s\n", 
                   mode.c_str(), currentStepIndex, dist, (step.signal == LEFT) ? "LEFT" : "RIGHT");
      
      blinkStartTime = millis();
      lastLedToggle = millis();
      activeBlink = step.signal;
      ledState = true;
      step.triggered = true; // Mark as triggered immediately
    }
    
    // Move to next step when passed
    if (step.triggered && dist <= TURN_PASS_DISTANCE) {
      currentStepIndex++;
      String mode = pseudoTravelerEnabled ? "PSEUDO" : "GPS";
      Serial.printf("[%s] Passed turn step %d, moving to next\n", mode.c_str(), currentStepIndex - 1);
    }
  }
}

void updateLEDs() {
  unsigned long now = millis();
  if (activeBlink != NONE) {
    // Check if blink duration has expired
    if (now - blinkStartTime >= BLINK_DURATION) {
      // Stop blinking
      activeBlink = NONE;
      ledState = false;
      digitalWrite(LEFT_LED, LOW);
      digitalWrite(RIGHT_LED, LOW);
      String mode = pseudoTravelerEnabled ? "PSEUDO" : "GPS";
      Serial.printf("[%s] LED blinking stopped\n", mode.c_str());
    } else {
      // Handle blinking
      if (now - lastLedToggle >= LED_BLINK_INTERVAL) {
        ledState = !ledState;
        lastLedToggle = now;
        
        if (activeBlink == LEFT) {
          digitalWrite(LEFT_LED, ledState ? HIGH : LOW);
          digitalWrite(RIGHT_LED, LOW);
        } else if (activeBlink == RIGHT) {
          digitalWrite(RIGHT_LED, ledState ? HIGH : LOW);
          digitalWrite(LEFT_LED, LOW);
        }
      }
    }
  }
}

// NEW FUNCTION: Decode Google polyline to get detailed route points
void decodePolyline(String encoded, RoutePoint* points, int* totalPoints, int maxPoints) {
  int len = encoded.length();
  int index = 0;
  int lat = 0, lon = 0;
  *totalPoints = 0;
  
  while (index < len && *totalPoints < maxPoints) {
    int b, shift = 0, result = 0;
    
    // Decode latitude
    do {
      b = encoded.charAt(index++) - 63;
      result |= (b & 0x1f) << shift;
      shift += 5;
    } while (b >= 0x20);
    int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lat += dlat;
    
    shift = 0;
    result = 0;
    
    // Decode longitude
    do {
      b = encoded.charAt(index++) - 63;
      result |= (b & 0x1f) << shift;
      shift += 5;
    } while (b >= 0x20);
    int dlon = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lon += dlon;
    
    // Convert to degrees and store
    points[*totalPoints].lat = (double)lat / 1e5;
    points[*totalPoints].lon = (double)lon / 1e5;
    (*totalPoints)++;
  }
}

void buildRoute() {
  double originLat = currentLat != 0 ? currentLat : 26.9124; // Default to Jaipur
  double originLon = currentLon != 0 ? currentLon : 75.7873;
  
  if (destLat == 0 && destLon == 0) {
    Serial.println("[Route] Destination coordinates missing");
    return;
  }
  
  HTTPClient http;
  String originStr = String(originLat, 6) + "," + String(originLon, 6);
  String destStr = String(destLat, 6) + "," + String(destLon, 6);
  String url = "https://maps.googleapis.com/maps/api/directions/json?origin=" + originStr + "&destination=" + destStr + "&mode=driving&key=" + googleApiKey;
  
  Serial.println("[Route] " + url);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    routeGeometry = http.getString();
    Serial.println("[Route] Route JSON received");
    
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, routeGeometry);
    
    if (!err && doc["status"] == "OK") {
      JsonObject route = doc["routes"][0];
      JsonArray legs = route["legs"];
      
      // Reset counters
      totalSteps = 0;
      currentStepIndex = 0;
      totalRoutePoints = 0;
      
      // Stop any active blinking when new route is loaded
      activeBlink = NONE;
      digitalWrite(LEFT_LED, LOW);
      digitalWrite(RIGHT_LED, LOW);
      
      if (legs.size() > 0) {
        JsonArray steps = legs[0]["steps"];
        
        // IMPROVED: Extract detailed route points from polyline
        String overviewPolyline = route["overview_polyline"]["points"].as<String>();
        if (overviewPolyline.length() > 0) {
          Serial.println("[Route] Decoding overview polyline...");
          decodePolyline(overviewPolyline, routePoints, &totalRoutePoints, MAX_ROUTE_POINTS);
          Serial.printf("[Route] Decoded %d detailed polyline points\n", totalRoutePoints);
        } else {
          Serial.println("[Route] No overview polyline found, using step points");
          // Fallback to step points if polyline not available
          for (JsonObject step : steps) {
            if (totalRoutePoints < MAX_ROUTE_POINTS - 1) {
              routePoints[totalRoutePoints].lat = step["start_location"]["lat"].as<double>();
              routePoints[totalRoutePoints].lon = step["start_location"]["lng"].as<double>();
              totalRoutePoints++;
            }
          }
        }
        
        // Extract turn steps (unchanged)
        for (JsonObject step : steps) {
          String instr = step["html_instructions"].as<String>();
          String lowerInstr = instr;
          lowerInstr.toLowerCase();
          double lat = step["start_location"]["lat"].as<double>();
          double lon = step["start_location"]["lng"].as<double>();
          LedSignal sig = NONE;
          
          if (lowerInstr.indexOf("left") >= 0)
            sig = LEFT;
          else if (lowerInstr.indexOf("right") >= 0)
            sig = RIGHT;
            
          if (sig != NONE && totalSteps < MAX_STEPS) {
            turnSteps[totalSteps].lat = lat;
            turnSteps[totalSteps].lon = lon;
            turnSteps[totalSteps].signal = sig;
            turnSteps[totalSteps].triggered = false;
            totalSteps++;
            Serial.printf("Added turn step %d: %s at %.6f, %.6f\n", totalSteps, (sig == LEFT) ? "LEFT" : "RIGHT", lat, lon);
          }
        }
        
        Serial.printf("Route built with %d polyline points and %d turn steps\n", totalRoutePoints, totalSteps);
        String mode = pseudoTravelerEnabled ? "PSEUDO TRAVELER" : "REAL GPS";
        Serial.printf("LED control mode: %s\n", mode.c_str());
      }
    }
  } else {
    Serial.printf("[Route] HTTP Error: %d\n", httpCode);
    routeGeometry = "{}";
  }
  
  http.end();
}

void handleTogglePseudo() {
  pseudoTravelerEnabled = !pseudoTravelerEnabled;
  if (pseudoTravelerEnabled) {
    // Reset pseudo traveler to start of route
    if (totalRoutePoints > 0) {
      pseudoLat = routePoints[0].lat;  // Start from first polyline point
      pseudoLon = routePoints[0].lon;
    } else {
      pseudoLat = currentLat != 0 ? currentLat : 26.9124; // Default to Jaipur if no GPS
      pseudoLon = currentLon != 0 ? currentLon : 75.7873;
    }
    
    currentRouteIndex = 0;
    currentStepIndex = 0; // Reset turn detection
    
    // Reset all turn steps
    for (int i = 0; i < totalSteps; i++) {
      turnSteps[i].triggered = false;
    }
    
    // Stop any active LED blinking when switching modes
    activeBlink = NONE;
    digitalWrite(LEFT_LED, LOW);
    digitalWrite(RIGHT_LED, LOW);
    
    Serial.println("=== PSEUDO TRAVELER MODE ENABLED ===");
    Serial.println("LEDs will now follow RED marker position");
    Serial.printf("Starting from polyline point: %.6f, %.6f\n", pseudoLat, pseudoLon);
  } else {
    // Reset turn detection for real GPS mode
    currentStepIndex = 0;
    for (int i = 0; i < totalSteps; i++) {
      turnSteps[i].triggered = false;
    }
    
    // Stop any active LED blinking when switching modes
    activeBlink = NONE;
    digitalWrite(LEFT_LED, LOW);
    digitalWrite(RIGHT_LED, LOW);
    
    Serial.println("=== REAL GPS MODE ENABLED ===");
    Serial.println("LEDs will now follow BLUE marker position");
  }
  
  server.send(200, "text/plain", pseudoTravelerEnabled ? "enabled" : "disabled");
}

// Rest of the functions with updated CSS...
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'/>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'/>";
  html += "<title>Smart Navigation System</title>";
  html += "<style>";
  
  // Main body and layout styles
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; ";
  html += "background: linear-gradient(135deg, #1f2937, #111827); color: #d1d5db; ";
  html += "min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 1rem; }";
  
  // Container styles
  html += ".container { background-color: #1f2937; padding: 1.5rem; border-radius: 1rem; ";
  html += "box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25); width: 100%; max-width: 64rem; ";
  html += "border: 1px solid #374151; }";
  
  // Typography
  html += ".main-title { font-size: 2.25rem; font-weight: 800; text-align: center; color: white; margin-bottom: 1rem; }";
  html += ".title-accent { color: #60a5fa; }";
  html += ".status-text { text-align: center; color: #9ca3af; margin-bottom: 1rem; }";
  html += ".section-title { font-size: 1.25rem; font-weight: bold; margin-bottom: 0.5rem; color: white; }";
  
  // Status indicator styles
  html += ".status-indicator { padding: 0.5rem; border-radius: 0.5rem; margin: 0.5rem 0; text-align: center; font-weight: bold; }";
  html += ".pseudo-active { background-color: #dc2626; color: white; }";
  html += ".gps-active { background-color: #2563eb; color: white; }";
  
  // Form and input styles
  html += ".input-group { display: flex; gap: 0.5rem; margin-bottom: 1.5rem; }";
  html += ".destination-input { flex: 1; padding: 0.75rem; border-radius: 0.5rem; background-color: #374151; ";
  html += "color: #e5e7eb; border: none; outline: none; transition: all 0.2s; }";
  html += ".destination-input:focus { box-shadow: 0 0 0 2px #3b82f6; }";
  
  // Button styles
  html += ".btn { padding: 0.5rem 1rem; border-radius: 0.5rem; border: none; cursor: pointer; ";
  html += "font-weight: 500; transition: all 0.2s; color: white; }";
  html += ".btn-primary { background-color: #2563eb; }";
  html += ".btn-primary:hover { background-color: #1d4ed8; }";
  html += ".btn-success { background-color: #16a34a; }";
  html += ".btn-success:hover { background-color: #15803d; }";
  html += ".btn-danger { background-color: #dc2626; }";
  html += ".btn-danger:hover { background-color: #b91c1c; }";
  
  // Map styles
  html += "#map { width: 100%; height: 400px; border-radius: 1rem; margin: 1.5rem 0; }";
  
  // Directions panel styles
  html += ".directions-panel { background-color: #374151; padding: 1rem; border-radius: 0.5rem; ";
  html += "max-height: 16rem; overflow-y: auto; }";
  html += ".directions-list { list-style: none; }";
  html += ".direction-item { padding: 0.5rem; margin: 0.5rem 0; border-radius: 0.5rem; background-color: #4b5563; }";
  html += ".active-step { background-color: #374151; border-left: 4px solid #3b82f6; }";
  
  // Responsive design
  html += "@media (max-width: 768px) {";
  html += "  .container { padding: 1rem; }";
  html += "  .main-title { font-size: 1.875rem; }";
  html += "  .input-group { flex-direction: column; }";
  html += "  .btn { width: 100%; margin: 0.25rem 0; }";
  html += "  #map { height: 300px; }";
  html += "}";
  
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1 class='main-title'>Smart <span class='title-accent'>Navigation System</span></h1>";
  html += "<p id='status' class='status-text'>Fetching your location...</p>";
  html += "<div id='ledStatus' class='status-indicator gps-active'>LED Control: Real GPS Mode</div>";
  html += "<div class='input-group'>";
  html += "<input id='destination' type='text' placeholder='Enter destination' class='destination-input'/>";
  html += "<button id='routeBtn' class='btn btn-primary'>Go</button>";
  html += "<button id='pseudoBtn' class='btn btn-success'>Test</button>";
  html += "</div>";
  html += "<div id='map'></div>";
  html += "<div class='directions-panel'>";
  html += "<h2 class='section-title'>Directions</h2>";
  html += "<ul id='directions' class='directions-list'></ul>";
  html += "</div>";
  html += "</div>";
  html += "<script src='https://maps.googleapis.com/maps/api/js?key=" + googleApiKey + "&libraries=geometry'></script>";
  html += "<script>";
  html += "let map, realMarker, pseudoMarker, routePolyline;";
  html += "let pseudoEnabled = false;";
  
  html += "function initMap() {";
  html += "  map = new google.maps.Map(document.getElementById('map'), { center: { lat: 20.5937, lng: 78.9629 }, zoom: 6 });";
  html += "  realMarker = new google.maps.Marker({ position: { lat: 20.5937, lng: 78.9629 }, map: map, title: 'Real GPS Location', icon: 'http://maps.google.com/mapfiles/ms/icons/blue-dot.png' });";
  html += "  pseudoMarker = new google.maps.Marker({ position: { lat: 20.5937, lng: 78.9629 }, map: null, title: 'Test Traveler', icon: 'http://maps.google.com/mapfiles/ms/icons/red-dot.png' });";
  html += "  routePolyline = new google.maps.Polyline({ path: [], strokeColor: '#3b82f6', strokeWeight: 4, map: map });";
  html += "}";
  
  html += "function updateLEDStatus() {";
  html += "  let statusDiv = document.getElementById('ledStatus');";
  html += "  if (pseudoEnabled) {";
  html += "    statusDiv.textContent = 'LED Control: Test Traveler Mode (Red Marker)';";
  html += "    statusDiv.className = 'status-indicator pseudo-active';";
  html += "  } else {";
  html += "    statusDiv.textContent = 'LED Control: Real GPS Mode (Blue Marker)';";
  html += "    statusDiv.className = 'status-indicator gps-active';";
  html += "  }";
  html += "}";
  
  html += "function updateGPS() {";
  html += "  fetch('/gps.json').then(res => res.json()).then(data => {";
  html += "    let latlng = { lat: data.lat, lng: data.lon };";
  html += "    realMarker.setPosition(latlng);";
  html += "    if (!pseudoEnabled) {";
  html += "      map.setCenter(latlng);";
  html += "      map.setZoom(16);";
  html += "    }";
  html += "  });";
  html += "}";
  
  html += "function updatePseudo() {";
  html += "  if (pseudoEnabled) {";
  html += "    fetch('/pseudo.json').then(res => res.json()).then(data => {";
  html += "      let latlng = { lat: data.lat, lng: data.lon };";
  html += "      pseudoMarker.setPosition(latlng);";
  html += "      map.setCenter(latlng);";
  html += "    });";
  html += "  }";
  html += "}";
  
  html += "function updateRoute() {";
  html += "  fetch('/route.json').then(res => res.json()).then(gdata => {";
  html += "    if (!gdata.routes || gdata.routes.length === 0) return;";
  html += "    let polyline = gdata.routes[0].overview_polyline.points;";
  html += "    let decodedPath = google.maps.geometry.encoding.decodePath(polyline);";
  html += "    routePolyline.setPath(decodedPath);";
  html += "    let bounds = new google.maps.LatLngBounds();";
  html += "    decodedPath.forEach(function(latlng) { bounds.extend(latlng); });";
  html += "    map.fitBounds(bounds);";
  html += "    let directionsList = document.getElementById('directions');";
  html += "    directionsList.innerHTML = '';";
  html += "    let steps = gdata.routes[0].legs[0].steps;";
  html += "    steps.forEach(step => {";
  html += "      let li = document.createElement('li');";
  html += "      li.innerHTML = step.html_instructions;";
  html += "      li.className = 'direction-item';";
  html += "      directionsList.appendChild(li);";
  html += "    });";
  html += "  });";
  html += "}";
  
  html += "document.getElementById('routeBtn').addEventListener('click', () => {";
  html += "  let dest = document.getElementById('destination').value;";
  html += "  if (!dest) { alert('Please enter a destination.'); return; }";
  html += "  fetch('/setdest?place=' + encodeURIComponent(dest)).then(() => {";
  html += "    setTimeout(() => { updateRoute(); }, 2000);";
  html += "  });";
  html += "  document.getElementById('status').textContent = 'Fetching route...';";
  html += "});";
  
  html += "document.getElementById('pseudoBtn').addEventListener('click', () => {";
  html += "  pseudoEnabled = !pseudoEnabled;";
  html += "  let btn = document.getElementById('pseudoBtn');";
  html += "  if (pseudoEnabled) {";
  html += "    btn.textContent = 'Stop Test';";
  html += "    btn.className = 'btn btn-danger';";
  html += "    pseudoMarker.setMap(map);";
  html += "  } else {";
  html += "    btn.textContent = 'Test';";
  html += "    btn.className = 'btn btn-success';";
  html += "    pseudoMarker.setMap(null);";
  html += "  }";
  html += "  updateLEDStatus();";
  html += "  fetch('/togglepseudo');";
  html += "});";
  
  html += "window.onload = () => {";
  html += "  initMap();";
  html += "  updateLEDStatus();";
  html += "  setInterval(updateGPS, 2000);";
  html += "  setInterval(updatePseudo, 1000);";
  html += "  setInterval(updateRoute, 5000);";
  html += "};";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSetDest() {
  if (server.hasArg("place")) {
    destinationName = server.arg("place");
    Serial.println("[Dest] " + destinationName);
    geocodeDestination(destinationName);
  }
  server.send(200, "text/plain", "OK");
}

void handleMap() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting...");
}

void handleGPS() {
  String json = "{\"lat\":" + String(currentLat, 6) + ",\"lon\":" + String(currentLon, 6) + "}";
  server.send(200, "application/json", json);
}

void handlePseudo() {
  String json = "{\"lat\":" + String(pseudoLat, 6) + ",\"lon\":" + String(pseudoLon, 6) + "}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  String mode = pseudoTravelerEnabled ? "pseudo" : "gps";
  String json = "{\"mode\":\"" + mode + "\",\"pseudoEnabled\":" + String(pseudoTravelerEnabled ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleRoute() {
  server.send(200, "application/json", routeGeometry);
}

void geocodeDestination(String place) {  
  HTTPClient http;
  String url = "https://maps.googleapis.com/maps/api/geocode/json?address=" + urlEncode(place) + "&key=" + googleApiKey;
  Serial.println("[Geo] " + url);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, response);
    
    if (!err && doc["status"] == "OK" && doc["results"].size() > 0) {
      destLat = doc["results"][0]["geometry"]["location"]["lat"].as<double>();
      destLon = doc["results"][0]["geometry"]["location"]["lng"].as<double>();
      Serial.printf("[Geo] Destination coordinates: %f, %f\n", destLat, destLon);
      buildRoute();
    } else {
      Serial.println("[Geo] Geocoding failed or no results");
    }
  } else {
    Serial.printf("[Geo] HTTP error: %d\n", httpCode);
  }
  
  http.end();
}

String urlEncode(String str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9')) {
      encoded += c;
    } else {
      encoded += '%';
      code0 = (c >> 4) & 0xF;
      code1 = c & 0xF;
      encoded += char(code0 > 9 ? code0 - 10 + 'A' : code0 + '0');
      encoded += char(code1 > 9 ? code1 - 10 + 'A' : code1 + '0');
    }
  }
  return encoded;
}

double degreesToRadians(double deg) {
  return deg * (3.14159265358979323846 / 180.0);
}

double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  const double earthRadius = 6371000.0; // meters
  double dLat = degreesToRadians(lat2 - lat1);
  double dLon = degreesToRadians(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(degreesToRadians(lat1)) * cos(degreesToRadians(lat2)) *
             sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return earthRadius * c;
}
