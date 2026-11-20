/*
 * Node 1 — Fridge monitor (ESP32-DevKitC) · STAGE 8 (Wi-Fi + MQTT + Remote CMD)
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 *   GPIO23  red security LED      (Stage 1)
 *   GPIO4   DS18B20 temperature   (Stage 2)
 *   GPIO19  reed switch, door     (Stage 3)
 *   GPIO18  active buzzer (bare)  (Stage 4)
 *
 * Stage 8: subscribes to campus/sensor01/cmd ("breach" / "clear" or JSON)
 * to drive the same breach alarm flag remotely, while local alarms still work offline.
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <math.h>
#include "secrets.h"

#define LED_PIN     23
#define TEMP_PIN     4
#define DOOR_PIN    19
#define BUZZER_PIN  18

#define DOOR_CLOSED_LEVEL LOW
#define BUZZER_ON_LEVEL   HIGH

// 1 = short bench timings so alarms can be tested in seconds
// 0 = real SR-13 values (over 8 °C for 10 min, door open 5 min)
#define BENCH_TIMING 1
#if BENCH_TIMING
const float         TEMP_HIGH_C   = 28.0;
const unsigned long TEMP_ALARM_MS = 10UL * 1000;
const unsigned long DOOR_ALARM_MS = 15UL * 1000;
#else
const float         TEMP_HIGH_C   = 8.0;
const unsigned long TEMP_ALARM_MS = 10UL * 60 * 1000;
const unsigned long DOOR_ALARM_MS =  5UL * 60 * 1000;
#endif
const float TEMP_CLEAR_MARGIN = 0.5;

const unsigned long TEMP_PERIOD_MS     = 2000;
const unsigned long CONVERSION_MS      = 800;
const unsigned long DEBOUNCE_MS        = 50;
const unsigned long PUBLISH_MS         = 5000;
const unsigned long MQTT_RETRY_MS      = 5000;
const unsigned long WIFI_RETRY_MS      = 15000;

const uint16_t PAT_DOOR[]   = {150, 150, 150, 1550};   // double chirp every 2 s
const uint16_t PAT_TEMP[]   = {1000, 1000};            // slow: 1 s on, 1 s off
const uint16_t PAT_BREACH[] = {100, 100};              // fast and continuous

enum Alarm { NONE, DOOR, TEMP, BREACH };
const char* ALARM_NAME[] = {"NONE", "DOOR OPEN TOO LONG", "OVER-TEMPERATURE", "SECURITY BREACH"};
const char* ALARM_JSON[] = {"NONE", "DOOR", "TEMP", "BREACH"};

OneWire oneWire(TEMP_PIN);
DallasTemperature probe(&oneWire);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// door
bool doorOpen = false;
int  lastRaw = -1;
unsigned long rawChangedAt = 0, doorOpenedAt = 0;

// temperature
unsigned long tempRequestedAt = 0, overSince = 0;
bool tempPending = false, overTemp = false;
float lastTempC = DEVICE_DISCONNECTED_C;

// alarms
bool breach = false;
Alarm current = NONE;
const uint16_t* pat = nullptr;
uint8_t patLen = 0, patStep = 0;
unsigned long stepStartedAt = 0;
String cmd;

// network / telemetry
unsigned long lastPublishAt = 0, lastMqttTryAt = 0, lastWifiTryAt = 0;
uint32_t seq = 0;
bool wifiWasConnected = false;

void buzzer(bool on) {
  digitalWrite(BUZZER_PIN, on ? BUZZER_ON_LEVEL : !BUZZER_ON_LEVEL);
}

void publishTelemetry(unsigned long now) {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["device"] = DEVICE_ID;
  time_t ts = time(nullptr);
  doc["ts"]  = (long)ts;
  doc["seq"] = ++seq;
  if (lastTempC != DEVICE_DISCONNECTED_C) {
    doc["temp"] = roundf(lastTempC * 100.0f) / 100.0f;
  }
  doc["door"]        = doorOpen ? "open" : "closed";
  doc["door_open_s"] = doorOpen ? (unsigned long)((now - doorOpenedAt) / 1000) : 0UL;
  doc["alarm"]       = ALARM_JSON[current];

  char buf[256];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(TOPIC_TELEMETRY, buf, len);
  lastPublishAt = now;
}

void onMqttCmd(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  String lower = msg;
  lower.toLowerCase();

  Serial.printf(">>> MQTT CMD [%s]: %s\n", topic, msg.c_str());

  if (lower == "breach") {
    breach = true;
  } else if (lower == "clear") {
    breach = false;
  } else if (msg.startsWith("{")) {
    JsonDocument doc;
    if (!deserializeJson(doc, msg)) {
      const char* c = doc["cmd"] | "";
      String cmdStr = String(c);
      cmdStr.toLowerCase();
      if (cmdStr == "breach" || doc["breach"] == true) breach = true;
      else if (cmdStr == "clear" || (doc["breach"].is<bool>() && doc["breach"] == false)) breach = false;
    }
  }
}

void setAlarm(Alarm a, unsigned long now) {
  if (a == current) return;
  Serial.printf(">>> ALARM %s -> %s\n", ALARM_NAME[current], ALARM_NAME[a]);
  current = a;
  switch (a) {
    case DOOR:   pat = PAT_DOOR;   patLen = 4; break;
    case TEMP:   pat = PAT_TEMP;   patLen = 2; break;
    case BREACH: pat = PAT_BREACH; patLen = 2; break;
    default:     pat = nullptr;    patLen = 0; break;
  }
  patStep = 0;
  stepStartedAt = now;
  buzzer(pat != nullptr);
  publishTelemetry(now);   // immediate event message
}

void runBuzzer(unsigned long now) {
  if (!pat || now - stepStartedAt < pat[patStep]) return;
  patStep = (patStep + 1) % patLen;
  stepStartedAt = now;
  buzzer(patStep % 2 == 0);
}

void maintainNetwork(unsigned long now) {
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.printf(">>> Wi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    }
  } else {
    wifiWasConnected = false;
    if (now - lastWifiTryAt >= WIFI_RETRY_MS) {
      lastWifiTryAt = now;
      Serial.println(">>> Wi-Fi reconnecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  if (now - lastMqttTryAt >= MQTT_RETRY_MS) {
    lastMqttTryAt = now;
    const char* lwt = "{\"device\":\"" DEVICE_ID "\",\"status\":\"offline\"}";
    Serial.printf(">>> Connecting MQTT %s:%d as %s... ", MQTT_HOST, MQTT_PORT, DEVICE_ID);
    if (mqtt.connect(DEVICE_ID, TOPIC_TELEMETRY, 1, false, lwt)) {
      Serial.println("OK");
      mqtt.subscribe(TOPIC_CMD);
      Serial.printf(">>> Subscribed to %s\n", TOPIC_CMD);
      publishTelemetry(now);
    } else {
      Serial.printf("failed (rc=%d)\n", mqtt.state());
    }
  }
}

void readSerialCommand() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      cmd.trim();
      cmd.toLowerCase();
      if (cmd == "breach")     { breach = true;  Serial.println("Command: breach (test)"); }
      else if (cmd == "clear") { breach = false; Serial.println("Command: clear"); }
      else if (cmd.length())   Serial.println("Commands: breach | clear");
      cmd = "";
    } else {
      cmd += ch;
    }
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  buzzer(false);
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  probe.begin();
  probe.setWaitForConversion(false);
  doorOpen = (digitalRead(DOOR_PIN) != DOOR_CLOSED_LEVEL);
  doorOpenedAt = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiTryAt = millis();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttCmd);
  mqtt.setKeepAlive(15);   // 15 s keepalive -> LWT triggers within ~23 s

  Serial.printf("Node 1 - Stage 8: temp GPIO%d (%d found), door GPIO%d, buzzer GPIO%d\n",
                TEMP_PIN, probe.getDeviceCount(), DOOR_PIN, BUZZER_PIN);
  Serial.printf("MQTT target: %s:%d -> pub %s | sub %s\n",
                MQTT_HOST, MQTT_PORT, TOPIC_TELEMETRY, TOPIC_CMD);
  for (int i = 0; i < 2; i++) { buzzer(true); delay(120); buzzer(false); delay(120); }
}

void loop() {
  unsigned long now = millis();
  readSerialCommand();
  maintainNetwork(now);

  // Door
  int raw = digitalRead(DOOR_PIN);
  if (raw != lastRaw) { lastRaw = raw; rawChangedAt = now; }
  bool openNow = (raw != DOOR_CLOSED_LEVEL);
  if (openNow != doorOpen && now - rawChangedAt >= DEBOUNCE_MS) {
    doorOpen = openNow;
    if (doorOpen) {
      doorOpenedAt = now;
      Serial.println(">>> DOOR OPENED");
    } else {
      Serial.printf(">>> DOOR CLOSED (was open %.1f s)\n", (now - doorOpenedAt) / 1000.0);
    }
    publishTelemetry(now);   // immediate event message
  }

  // Temperature
  if (!tempPending && now - tempRequestedAt >= TEMP_PERIOD_MS) {
    probe.requestTemperatures();
    tempRequestedAt = now;
    tempPending = true;
  }
  if (tempPending && now - tempRequestedAt >= CONVERSION_MS) {
    tempPending = false;
    float c = probe.getTempCByIndex(0);
    lastTempC = c;
    if (c != DEVICE_DISCONNECTED_C) {
      if (c > TEMP_HIGH_C && !overTemp) { overTemp = true; overSince = now; }
      else if (c < TEMP_HIGH_C - TEMP_CLEAR_MARGIN) overTemp = false;
    }
    if (!breach) digitalWrite(LED_PIN, HIGH);
    if (c == DEVICE_DISCONNECTED_C) Serial.print("Probe not found");
    else Serial.printf("Temperature: %.2f C", c);
    if (overTemp) Serial.printf(" (over %.1f C for %lu s)", TEMP_HIGH_C, (now - overSince) / 1000);
    if (doorOpen) Serial.printf(" | door OPEN for %lu s", (now - doorOpenedAt) / 1000);
    else Serial.print(" | door CLOSED");
    Serial.printf(" | alarm: %s\n", ALARM_NAME[current]);
  }

  // Alarms (independent of Wi-Fi/MQTT)
  bool tempAlarm = overTemp && now - overSince >= TEMP_ALARM_MS;
  bool doorAlarm = doorOpen && now - doorOpenedAt >= DOOR_ALARM_MS;
  setAlarm(breach ? BREACH : tempAlarm ? TEMP : doorAlarm ? DOOR : NONE, now);
  runBuzzer(now);

  // Periodic 5 s telemetry
  if (now - lastPublishAt >= PUBLISH_MS) {
    publishTelemetry(now);
  }

  // LED heartbeat / breach
  if (breach) digitalWrite(LED_PIN, HIGH);
  else if (now - tempRequestedAt >= CONVERSION_MS + 100) digitalWrite(LED_PIN, LOW);
}
