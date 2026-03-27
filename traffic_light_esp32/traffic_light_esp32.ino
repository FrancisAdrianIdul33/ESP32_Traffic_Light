// ================================================================
// ESP32 Traffic Light → Firebase Realtime Database
//
// Required Library (install via Arduino Library Manager):
//   Firebase ESP32 Client  →  by Mobizt
// ================================================================
#include <WiFi.h>
#include <FirebaseESP32.h>        // Firebase ESP32 Client by Mobizt

// ----------------------------------------------------------------
// Pin Definitions
// ----------------------------------------------------------------
#define PIN_RED    2    // D2  → RED LED
#define PIN_YELLOW 4    // D4  → YELLOW LED
#define PIN_GREEN  16   // D16 → GREEN LED

// ----------------------------------------------------------------
// WiFi & Firebase credentials
// ----------------------------------------------------------------
#define WIFI_SSID     "shinakira"
#define WIFI_PASSWORD "12312309"

#define FIREBASE_HOST "traffic-light-a4480-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyAgbmV5PMjWZ_4Av4tfHVsRN-X7lxRaGy4"

// How often to poll Firebase for commands (ms)
#define POLL_INTERVAL 2000

// ----------------------------------------------------------------
FirebaseData   fbData;
FirebaseAuth   fbAuth;
FirebaseConfig fbConfig;

// ----------------------------------------------------------------
// Traffic light state
// ----------------------------------------------------------------
String        currentSignal = "red";
String        currentMode   = "auto";
int           durRed        = 30;
int           durYellow     = 5;
int           durGreen      = 25;
int           autoPhase     = 0;   // 0=red 1=yellow 2=green
unsigned long phaseStart    = 0;
unsigned long lastPoll      = 0;

String phaseNames[] = {"red", "yellow", "green"};

// ----------------------------------------------------------------
// Set LEDs
// ----------------------------------------------------------------
void applySignal(String signal) {
  digitalWrite(PIN_RED,    signal == "red"    ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, signal == "yellow" ? HIGH : LOW);
  digitalWrite(PIN_GREEN,  signal == "green"  ? HIGH : LOW);
  Serial.println("Signal → " + signal);
}

// ----------------------------------------------------------------
// Auto cycle duration helper
// ----------------------------------------------------------------
int phaseDuration(int phase) {
  if (phase == 0) return durRed;
  if (phase == 1) return durYellow;
  return durGreen;
}

// ================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);

  // Boot blink test
  digitalWrite(PIN_RED,    HIGH); delay(300); digitalWrite(PIN_RED,    LOW);
  digitalWrite(PIN_YELLOW, HIGH); delay(300); digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_GREEN,  HIGH); delay(300); digitalWrite(PIN_GREEN,  LOW);

  // --- Connect to WiFi ---
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  // --- Connect to Firebase ---
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase connected!");

  // Write initial state
  Firebase.setString(fbData, "/trafficLight/signal",    currentSignal);
  Firebase.setString(fbData, "/trafficLight/mode",      currentMode);
  Firebase.setInt   (fbData, "/trafficLight/durRed",    durRed);
  Firebase.setInt   (fbData, "/trafficLight/durYellow", durYellow);
  Firebase.setInt   (fbData, "/trafficLight/durGreen",  durGreen);
  Firebase.setBool  (fbData, "/trafficLight/esp32Online", true);

  applySignal(currentSignal);
  phaseStart = millis();
  Serial.println("Traffic Light System Ready!");
}

// ================================================================
void loop() {

  // ── Poll Firebase every POLL_INTERVAL ────────────────────────
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();

    // Read mode (auto / manual)
    if (Firebase.getString(fbData, "/trafficLight/mode")) {
      currentMode = fbData.stringData();
    }

    // Read manual signal (only applied if mode == manual)
    if (currentMode == "manual") {
      if (Firebase.getString(fbData, "/trafficLight/signal")) {
        String demanded = fbData.stringData();
        if (demanded != currentSignal) {
          currentSignal = demanded;
          applySignal(currentSignal);
        }
      }
    }

    // Read durations
    if (Firebase.getInt(fbData, "/trafficLight/durRed"))    durRed    = fbData.intData();
    if (Firebase.getInt(fbData, "/trafficLight/durYellow")) durYellow = fbData.intData();
    if (Firebase.getInt(fbData, "/trafficLight/durGreen"))  durGreen  = fbData.intData();

    // Log result
    if (fbData.httpCode() == 200) {
      Serial.println("✅ Firebase poll OK");
    } else {
      Serial.println("❌ Firebase error: " + fbData.errorReason());
    }
  }

  // ── Auto cycle ───────────────────────────────────────────────
  if (currentMode == "auto") {
    if ((millis() - phaseStart) / 1000 >= (unsigned long)phaseDuration(autoPhase)) {
      autoPhase     = (autoPhase + 1) % 3;
      phaseStart    = millis();
      currentSignal = phaseNames[autoPhase];
      applySignal(currentSignal);
      Firebase.setString(fbData, "/trafficLight/signal", currentSignal);
    }
  }

  // ── Heartbeat every 10s ──────────────────────────────────────
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    Firebase.setInt (fbData, "/trafficLight/uptime",      (int)(millis() / 1000));
    Firebase.setBool(fbData, "/trafficLight/esp32Online", true);
    Firebase.setString(fbData, "/trafficLight/ip",        WiFi.localIP().toString());
  }

  // ── WiFi watchdog ────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost — reconnecting...");
    WiFi.reconnect();
    delay(3000);
  }
}
