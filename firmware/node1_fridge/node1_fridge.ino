/*
 * Node 1 — Fridge monitor (ESP32-DevKitC)
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 * STAGE 1 (this file): the red security-breach LED on GPIO23 (through a 220 ohm resistor)
 * blinks at 1 Hz, proving the first external output works.
 * Later stages add: DS18B20 temperature (GPIO4), reed switch door sensor (GPIO19),
 * buzzer (GPIO18), Wi-Fi + MQTT publish, then TLS.
 *
 * No secrets live in this file — they go in secrets.h (git-ignored) from the MQTT stage onward.
 */

#define LED_PIN 23   // red security LED: GPIO23 -> 220 ohm -> LED -> GND

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Node 1 - Stage 1: security LED on GPIO23");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(1000);
}
