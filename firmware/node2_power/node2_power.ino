/*
 * Node 2 — Power monitor (ESP32-DevKitC) · STAGE 5b: 5 V power sensing
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 * A USB charger plugged into the fridge's switched socket feeds 5 V through a
 * 10 kΩ / 20 kΩ divider: about 3.3 V on GPIO19 while the socket has power,
 * 0 V after a cut. Node 2 itself runs from a power bank (or the laptop while
 * testing), so it stays on to report the cut (SR-14).
 *
 * No secrets live in this file — they go in secrets.h (git-ignored) from the MQTT stage onward.
 */

#define POWER_PIN 19

const unsigned long STABLE_MS = 200;    // ignore flicker shorter than 0.2 s
const unsigned long STATUS_MS = 2000;   // one status line every 2 s

bool powerOn = false;
int  lastRaw = -1;
unsigned long rawChangedAt = 0, powerLostAt = 0, lastStatus = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(POWER_PIN, INPUT_PULLDOWN);   // a broken signal wire reads as "power lost" (fail-safe)

  uint64_t mac = ESP.getEfuseMac();
  Serial.println();
  Serial.println("Node 2 - Stage 5b: 5 V power sensing on GPIO19");
  Serial.printf("MAC:   %02X:%02X:%02X:%02X:%02X:%02X\n",
                (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
                (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

  powerOn = (digitalRead(POWER_PIN) == HIGH);
  powerLostAt = millis();
  Serial.printf("Mains power is %s at start\n", powerOn ? "ON" : "OFF");
}

void loop() {
  unsigned long now = millis();

  // Accept a change once the pin has been steady for 200 ms
  int raw = digitalRead(POWER_PIN);
  if (raw != lastRaw) { lastRaw = raw; rawChangedAt = now; }
  bool onNow = (raw == HIGH);
  if (onNow != powerOn && now - rawChangedAt >= STABLE_MS) {
    powerOn = onNow;
    if (!powerOn) {
      powerLostAt = now;
      Serial.println(">>> POWER LOST");
    } else {
      Serial.printf(">>> POWER RESTORED (was off %.1f s)\n", (now - powerLostAt) / 1000.0);
    }
  }

  if (now - lastStatus >= STATUS_MS) {
    lastStatus = now;
    if (powerOn) Serial.printf("Mains power: ON  | uptime %lu s\n", now / 1000);
    else Serial.printf("Mains power: OFF for %lu s | uptime %lu s\n", (now - powerLostAt) / 1000, now / 1000);
  }
}
