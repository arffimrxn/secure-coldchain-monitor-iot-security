/*
 * Node 1 — Fridge monitor (ESP32-DevKitC)
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 * STAGE 1 (this file): blink the onboard LED to prove the toolchain + board + USB driver.
 * Later stages add: DS18B20 temperature, reed switch (door), buzzer (over-temp alarm),
 * red security-breach LED, Wi-Fi + MQTT publish, then TLS.
 *
 * No secrets live in this file — they go in secrets.h (git-ignored) from Stage 4 onward.
 */

#define ONBOARD_LED 2   // GPIO2 = built-in LED on most ESP32 DevKits

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(ONBOARD_LED, OUTPUT);
  Serial.println("Node 1 — Stage 1: blink. Toolchain OK if this LED blinks.");
}

void loop() {
  digitalWrite(ONBOARD_LED, HIGH);
  Serial.println("LED ON");
  delay(1000);
  digitalWrite(ONBOARD_LED, LOW);
  Serial.println("LED OFF");
  delay(1000);
}
