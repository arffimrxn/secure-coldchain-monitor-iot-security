/*
 * Node 2 — Power monitor (ESP32-DevKitC) · STAGE 6 (Wi-Fi + Plaintext MQTT)
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 *   GPIO19  5 V power sense (10 kΩ / 20 kΩ divider from the USB charger)
 *   GPIO18  bare active buzzer, + leg (− leg to GND)
 *
 * Local buzzer keeps working independently even if Wi-Fi or the broker is down.
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define POWER_PIN       19
#define BUZZER_PIN      18
#define BUZZER_ON_LEVEL HIGH

const unsigned long STABLE_MS     = 200;
const unsigned long STATUS_MS     = 2000;
const unsigned long PUBLISH_MS    = 5000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long WIFI_RETRY_MS = 15000;

const uint16_t PAT_POWER[] = {600, 200, 150, 2050};
const uint8_t  PAT_LEN = 4;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool powerOn = false, alarmOn = false, muted = false;
int  lastRaw = -1;
unsigned long rawChangedAt = 0, powerLostAt = 0, lastStatus = 0, stepStartedAt = 0;
unsigned long lastPublishAt = 0, lastMqttTryAt = 0, lastWifiTryAt = 0;
uint8_t patStep = 0;
uint32_t seq = 0;
bool wifiWasConnected = false;
String cmd;

void buzzer(bool on) {
  digitalWrite(BUZZER_PIN, on ? BUZZER_ON_LEVEL : !BUZZER_ON_LEVEL);
}

void chirp(int times) {
  for (int i = 0; i < times; i++) { buzzer(true); delay(120); buzzer(false); delay(120); }
}

void startAlarm(unsigned long now) {
  alarmOn = true;
  patStep = 0;
  stepStartedAt = now;
  buzzer(true);
}

void stopAlarm() {
  alarmOn = false;
  buzzer(false);
}

void runBuzzer(unsigned long now) {
  if (!alarmOn || now - stepStartedAt < PAT_POWER[patStep]) return;
  patStep = (patStep + 1) % PAT_LEN;
  stepStartedAt = now;
  buzzer(patStep % 2 == 0);
}

void publishTelemetry(unsigned long now) {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["device"] = DEVICE_ID;
  time_t ts = time(nullptr);
  doc["ts"]    = (long)ts;
  doc["seq"]   = ++seq;
  doc["mains"] = powerOn ? "on" : "off";
  doc["off_s"] = powerOn ? 0UL : (unsigned long)((now - powerLostAt) / 1000);

  char buf[200];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(TOPIC_TELEMETRY, buf, len);
  lastPublishAt = now;
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
      if (cmd == "mute") {
        if (alarmOn) { muted = true; stopAlarm(); Serial.println("Command: mute - alarm silenced until the power returns"); }
        else Serial.println("Command: mute - nothing is sounding");
      } else if (cmd.length()) {
        Serial.println("Commands: mute");
      }
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
  pinMode(POWER_PIN, INPUT_PULLDOWN);

  uint64_t mac = ESP.getEfuseMac();
  Serial.println();
  Serial.println("Node 2 - Stage 6: power sensing on GPIO19, buzzer on GPIO18 + Wi-Fi/MQTT");
  Serial.printf("MAC:   %02X:%02X:%02X:%02X:%02X:%02X\n",
                (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
                (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  Serial.printf("MQTT target: %s:%d -> %s\n", MQTT_HOST, MQTT_PORT, TOPIC_TELEMETRY);
  chirp(2);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiTryAt = millis();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(15);   // 15 s keepalive -> LWT triggers within ~23 s

  powerOn = (digitalRead(POWER_PIN) == HIGH);
  powerLostAt = millis();
  Serial.printf("Mains power is %s at start\n", powerOn ? "ON" : "OFF");
  if (!powerOn) {
    Serial.println(">>> POWER LOST (already off at start)");
    startAlarm(millis());
  }
}

void loop() {
  unsigned long now = millis();
  readSerialCommand();
  maintainNetwork(now);

  int raw = digitalRead(POWER_PIN);
  if (raw != lastRaw) { lastRaw = raw; rawChangedAt = now; }
  bool onNow = (raw == HIGH);
  if (onNow != powerOn && now - rawChangedAt >= STABLE_MS) {
    powerOn = onNow;
    if (!powerOn) {
      powerLostAt = now;
      muted = false;
      Serial.println(">>> POWER LOST");
      startAlarm(now);
    } else {
      Serial.printf(">>> POWER RESTORED (was off %.1f s)\n", (now - powerLostAt) / 1000.0);
      stopAlarm();
      muted = false;
      chirp(1);
    }
    publishTelemetry(now);   // immediate event message
  }

  runBuzzer(now);

  if (now - lastPublishAt >= PUBLISH_MS) {
    publishTelemetry(now);
  }

  if (now - lastStatus >= STATUS_MS) {
    lastStatus = now;
    const char* alarm = alarmOn ? "SOUNDING" : (muted ? "MUTED" : "off");
    if (powerOn) Serial.printf("Mains power: ON  | alarm: %s | uptime %lu s\n", alarm, now / 1000);
    else Serial.printf("Mains power: OFF for %lu s | alarm: %s | uptime %lu s\n",
                       (now - powerLostAt) / 1000, alarm, now / 1000);
  }
}
