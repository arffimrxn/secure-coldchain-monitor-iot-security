/*
 * Node 1 — Fridge monitor (ESP32-DevKitC) · STAGE 4 (Node 1 complete)
 * Secure Cold-Chain Vaccine-Fridge Monitor · IIB20804
 *
 *   GPIO23  red security LED      (Stage 1)
 *   GPIO4   DS18B20 temperature   (Stage 2)
 *   GPIO19  reed switch, door     (Stage 3)
 *   GPIO18  active buzzer (bare)  (Stage 4)
 *
 * Alarms (SR-13): over-temperature held too long, door open too long, and a
 * security-breach pattern (triggered from the Serial Monitor for now, from the
 * backend on campus/sensor01/cmd at Stage 8).
 *
 * No secrets live in this file — they go in secrets.h (git-ignored) from the MQTT stage onward.
 */
#include <OneWire.h>
#include <DallasTemperature.h>

#define LED_PIN     23
#define TEMP_PIN     4
#define DOOR_PIN    19
#define BUZZER_PIN  18

#define DOOR_CLOSED_LEVEL LOW   // keep whatever worked in Stage 3

// Bare active buzzer between pin 18 (+ leg) and GND (− leg): HIGH = sound.
// It draws roughly 15–25 mA at 3.3 V, within what one ESP32 pin can supply.
#define BUZZER_ON_LEVEL HIGH

// 1 = short bench timings so the alarms can be tested in seconds
// 0 = the real SR-13 values (over 8 °C for 10 min, door open 5 min)
#define BENCH_TIMING 1
#if BENCH_TIMING
const float         TEMP_HIGH_C   = 28.0;               // warm the probe in your hand to pass this
const unsigned long TEMP_ALARM_MS = 10UL * 1000;        // over the limit for 10 s
const unsigned long DOOR_ALARM_MS = 15UL * 1000;        // door open for 15 s
#else
const float         TEMP_HIGH_C   = 8.0;                // vaccines must stay at 2–8 °C
const unsigned long TEMP_ALARM_MS = 10UL * 60 * 1000;   // over 8 °C for 10 min (SR-13)
const unsigned long DOOR_ALARM_MS =  5UL * 60 * 1000;   // door open for 5 min (SR-13)
#endif
const float TEMP_CLEAR_MARGIN = 0.5;   // must fall 0.5 °C below the limit before it clears

const unsigned long TEMP_PERIOD_MS = 2000;   // one temperature reading every 2 s
const unsigned long CONVERSION_MS  = 800;    // DS18B20 needs up to 750 ms per reading
const unsigned long DEBOUNCE_MS    = 50;     // ignore reed contact bounce

// Buzzer patterns: alternating on/off times in ms, repeated while the alarm lasts
const uint16_t PAT_DOOR[]   = {150, 150, 150, 1550};   // double chirp every 2 s
const uint16_t PAT_TEMP[]   = {1000, 1000};            // slow: 1 s on, 1 s off
const uint16_t PAT_BREACH[] = {100, 100};              // fast and continuous

enum Alarm { NONE, DOOR, TEMP, BREACH };
const char* ALARM_NAME[] = {"NONE", "DOOR OPEN TOO LONG", "OVER-TEMPERATURE", "SECURITY BREACH"};

OneWire oneWire(TEMP_PIN);
DallasTemperature probe(&oneWire);

// door
bool doorOpen = false;
int  lastRaw = -1;
unsigned long rawChangedAt = 0, doorOpenedAt = 0;
// temperature
unsigned long tempRequestedAt = 0, overSince = 0;
bool tempPending = false, overTemp = false;
// alarms
bool breach = false;
Alarm current = NONE;
const uint16_t* pat = nullptr;
uint8_t patLen = 0, patStep = 0;
unsigned long stepStartedAt = 0;
String cmd;

void buzzer(bool on) {
  digitalWrite(BUZZER_PIN, on ? BUZZER_ON_LEVEL : !BUZZER_ON_LEVEL);
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
  buzzer(pat != nullptr);   // step 0 of every pattern is "on"
}

void runBuzzer(unsigned long now) {
  if (!pat || now - stepStartedAt < pat[patStep]) return;
  patStep = (patStep + 1) % patLen;
  stepStartedAt = now;
  buzzer(patStep % 2 == 0);   // even steps on, odd steps off
}

// Test commands typed in the Serial Monitor (line ending: New Line)
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
  buzzer(false);   // silence the buzzer before anything else
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);      // a cut DO wire reads as OPEN (fail-safe)
  probe.begin();
  probe.setWaitForConversion(false);    // keep checking the door while the probe converts
  doorOpen = (digitalRead(DOOR_PIN) != DOOR_CLOSED_LEVEL);
  doorOpenedAt = millis();

  Serial.printf("Node 1 - Stage 4: temp GPIO%d (%d found), door GPIO%d, buzzer GPIO%d\n",
                TEMP_PIN, probe.getDeviceCount(), DOOR_PIN, BUZZER_PIN);
  Serial.printf("Alarm limits: over %.1f C for %lu s, door open %lu s%s\n",
                TEMP_HIGH_C, TEMP_ALARM_MS / 1000, DOOR_ALARM_MS / 1000,
                BENCH_TIMING ? " (bench timing)" : "");
  Serial.println("Start-up chirp: you should hear 2 short beeps");
  for (int i = 0; i < 2; i++) { buzzer(true); delay(120); buzzer(false); delay(120); }
  Serial.println("Type 'breach' or 'clear' to test the breach alarm");
}

void loop() {
  unsigned long now = millis();
  readSerialCommand();

  // Door: accept a change once the reading has been steady for 50 ms
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
  }

  // Temperature: start a reading every 2 s, collect it 800 ms later
  if (!tempPending && now - tempRequestedAt >= TEMP_PERIOD_MS) {
    probe.requestTemperatures();
    tempRequestedAt = now;
    tempPending = true;
  }
  if (tempPending && now - tempRequestedAt >= CONVERSION_MS) {
    tempPending = false;
    float c = probe.getTempCByIndex(0);
    if (c != DEVICE_DISCONNECTED_C) {
      if (c > TEMP_HIGH_C && !overTemp) { overTemp = true; overSince = now; }
      else if (c < TEMP_HIGH_C - TEMP_CLEAR_MARGIN) overTemp = false;
    }
    if (!breach) digitalWrite(LED_PIN, HIGH);   // heartbeat flash
    if (c == DEVICE_DISCONNECTED_C) Serial.print("Probe not found");
    else Serial.printf("Temperature: %.2f C", c);
    if (overTemp) Serial.printf(" (over %.1f C for %lu s)", TEMP_HIGH_C, (now - overSince) / 1000);
    if (doorOpen) Serial.printf(" | door OPEN for %lu s", (now - doorOpenedAt) / 1000);
    else Serial.print(" | door CLOSED");
    Serial.printf(" | alarm: %s\n", ALARM_NAME[current]);
  }

  // Pick the alarm: breach beats temperature beats door
  bool tempAlarm = overTemp && now - overSince >= TEMP_ALARM_MS;
  bool doorAlarm = doorOpen && now - doorOpenedAt >= DOOR_ALARM_MS;
  setAlarm(breach ? BREACH : tempAlarm ? TEMP : doorAlarm ? DOOR : NONE, now);
  runBuzzer(now);

  // LED: solid on during a breach, otherwise a short heartbeat flash per reading
  if (breach) digitalWrite(LED_PIN, HIGH);
  else if (now - tempRequestedAt >= CONVERSION_MS + 100) digitalWrite(LED_PIN, LOW);
}
