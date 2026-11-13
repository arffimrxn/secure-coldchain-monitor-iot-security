# Stages 2–4: Node 1 sensors (DS18B20, reed switch, buzzer)

The sketch in `firmware/node1_fridge/node1_fridge.ino` is the finished Node 1 (Stage 4). Each
section below describes what that stage added and how it was tested.

**Node 1 pin map**

| ESP32 pin | Part | Stage |
|---|---|---|
| GPIO23 | 220 Ω → red LED → GND (security-breach indicator) | 1 |
| GPIO4 | DS18B20 module DAT | 2 |
| GPIO19 | reed switch module DO | 3 |
| GPIO18 | bare active buzzer (+ leg) | 4 |
| 3V3 / GND | bottom + / − rails | 2 |

---

## Stage 2: DS18B20 temperature probe

**Goal:** read the fridge temperature on Node 1, prove the reading tracks real changes
(positive tests), and prove a broken probe is noticed instead of reporting a wrong number
(negative test).

### Hardware

| Part | Detail |
|------|--------|
| DS18B20 waterproof probe | 1-Wire digital temperature sensor, ±0.5 °C |
| DS18B20 adapter module | screw terminal for the probe, pull-up resistor on DAT built in |
| 6 female-to-male jumpers | 3 from the ESP32, 3 from the module |

### Wiring

| # | Wire | From | To |
|---|------|------|----|
| 1 | Red | ESP32 3V3 | bottom + rail |
| 2 | Black | ESP32 GND (the second GND pin, not the LED's) | bottom − rail |
| 3 | Red | module VCC | bottom + rail |
| 4 | Black | module GND | bottom − rail |
| 5 | Blue | module DAT | hole j14 |
| 6 | Blue | ESP32 pin 4 | hole f14 (same column as j14) |

Probe into the module's screw terminal: red → VCC, black → GND, yellow → DAT.

![Stage 2 wiring](01_stage2_wiring.jpg)

**Why GPIO4:** a general-purpose pin that is not a strapping pin (0, 2, 5, 12, 15), not wired to
the flash chip (6–11) and not input-only (34–39).

**Why the adapter module:** the 1-Wire bus needs a pull-up resistor (usually 4.7 kΩ) on the data
line. The module has it built in, so the breadboard needs no extra resistor.

**Why it matters:** temperature is the safety-critical value in this system, because vaccines
must stay between 2 and 8 °C. Requirement SR-13 says an over-temperature lasting more than
10 minutes must raise an alarm within 60 s; later stages protect this reading in transit.

### Firmware

Reads the probe with the OneWire and DallasTemperature libraries and prints each reading to the
Serial Monitor at 115200 baud.

**LED heartbeat:** the red LED flashes once per reading (0.1 s on, 1.9 s off, plus up to 0.75 s
while the probe converts, so about every 2.75 s). A steady rhythm shows the board is alive; if it
stops, the firmware has frozen. From Stage 8 the LED takes on its real job as the
security-breach indicator.

### Tests

| # | Test | Type | Expected | Result |
|---|------|------|----------|--------|
| 1 | Room temperature | Positive | Steady reading, "1 sensor(s) found" at boot | ✅ 21.4 °C (air-conditioned room) |
| 2 | Hold the probe tip | Positive | Reading rises, then levels off | ✅ rose to __ °C |
| 3 | Probe on an ice pack | Positive | Reading falls | ✅ dropped to __ °C |
| 4 | Pull DAT out of j14 | Negative | "Probe not found" | ✅ reported on the next reading (within about 3 s); readings resumed after reconnecting |

Test 4 matters for security: a cut or loose probe wire is reported as a fault, not hidden behind
a plausible-looking temperature. In later stages this becomes an alert to the backend.

### 📸 Evidence

| File | Shows |
|------|-------|
| `01_stage2_wiring.jpg` | Wiring, with the pin and module labels readable |
| `02_serial_room_temp.png` | Boot line "1 sensor(s) found" and steady room readings |
| `03_serial_hand_temp.png` | Readings rising while the probe is held |
| `04_serial_ice_pack.png` | Readings falling on the ice pack |
| `05_serial_dat_unplugged.png` | Negative test: "Probe not found", then recovery |

![Room temperature](02_serial_room_temp.png)

![Hand test](03_serial_hand_temp.png)

![Ice pack test](04_serial_ice_pack.png)

![DAT unplugged (negative test)](05_serial_dat_unplugged.png)

### Problems & fixes

- **Three COM ports listed (COM3, COM4, COM5).** Found the ESP32 by unplugging and replugging it
  and watching which port disappeared: the CH340 was COM5.
- **The sketch was saved outside the repo as `node1_fridge.ino.ino`.** The IDE had saved it as a
  new sketch named "node1_fridge.ino". Fix: opened the repo's
  `firmware/node1_fridge/node1_fridge.ino`, pasted the code in, saved, and deleted the stray
  sketch folder.

### Result

- [x] Stage 2: the DS18B20 reads correctly at room, hand and ice-pack temperatures, and a
  disconnected probe is reported as "Probe not found".

---

## Stage 3: Reed switch for the door

**Goal:** detect the fridge door (the box lid) opening and closing, report each change straight
away, time how long the door stays open, and make a cut sensor wire fail safe.

### Hardware

| Part | Detail |
|------|--------|
| Reed switch module (LM393) | glass reed switch + comparator, digital output DO, signal LED |
| Neodymium magnet | 10 mm disc; goes on the lid edge, closes the reed when the lid shuts |
| 4 female-to-male jumpers | 3 from the module, 1 from the ESP32 |

### Wiring

| # | Wire | From | To |
|---|------|------|----|
| 1 | Red | module VCC | bottom + rail |
| 2 | Black | module GND | bottom − rail |
| 3 | Green | module DO | hole j18 |
| 4 | Green | ESP32 pin 19 | hole f18 (same column as j18) |

No new power wires from the ESP32: the Stage 2 3V3 and GND wires already feed the rails.

![Stage 3 wiring](06_stage3_wiring.jpg)

**Why GPIO19:** general purpose, and not a strapping, flash or input-only pin.

### Firmware

- **Debounce:** a reed contact bounces for a few milliseconds when it moves, so a change is only
  accepted once the reading has been steady for 50 ms. One door movement = one event.
- **Non-blocking temperature reads:** the probe now converts in the background
  (`setWaitForConversion(false)`), so the door is checked continuously instead of being frozen for
  0.75 s per reading. The LED heartbeat is now every 2 s.
- **Door-open timer:** each status line shows how long the door has been open, and the close
  event reports the total. Stage 4 uses this for the door-open alarm (SR-13: open > 5 min).
- **Fail-safe input:** pin 19 uses the internal pull-up, so a disconnected DO wire reads as
  **OPEN**. A cut wire raises the alarm instead of hiding an open door.
- Door polarity: magnet near = DO __ (LOW/HIGH) = CLOSED. _(Had to flip `DOOR_CLOSED_LEVEL`: yes / no)_

### Tests

| # | Test | Type | Expected | Result |
|---|------|------|----------|--------|
| 1 | Magnet against the reed, then away | Positive | CLOSED, then OPEN | ✅ module signal LED __ when the magnet is near |
| 2 | Open and close | Positive | `>>> DOOR OPENED`, open timer counting, `>>> DOOR CLOSED (was open x s)` | ✅ |
| 3 | Switching distance | Measurement | Switches within ~1 cm | ✅ switches to CLOSED at __ mm |
| 4 | Fast open/close | Robustness | One line per change, no duplicates | ✅ debounce works |
| 5 | Pull DO out of j18 | Negative | Reads OPEN (fail-safe) | ✅ OPEN while unplugged, CLOSED again after reconnecting |

The switching distance sets where the magnet goes on the lid: the gap between magnet and reed
must stay below __ mm when the lid is shut.

### 📸 Evidence

| File | Shows |
|------|-------|
| `06_stage3_wiring.jpg` | Wiring, with the module and pin labels readable |
| `07_serial_door_states.png` | Start-up lines, door opened, open timer, door closed |
| `08_switching_distance.jpg` | Magnet beside a ruler at the switching point |
| `09_serial_bounce.png` | Fast open/close giving one line per change |
| `10_serial_do_unplugged.png` | Negative test: OPEN while DO is unplugged, then recovery |

![Door states](07_serial_door_states.png)

![Switching distance](08_switching_distance.jpg)

![Debounce](09_serial_bounce.png)

![DO unplugged (negative test)](10_serial_do_unplugged.png)

### Result

- [x] Stage 3: door changes are reported once each and straight away, the open time is tracked,
  and a disconnected sensor wire fails safe as OPEN.

---

## Stage 4: Buzzer alarms (Node 1 complete)

**Goal:** sound a local alarm when the fridge needs attention (SR-13), with a different sound for
each cause, and prove that short, harmless events do not set it off.

### Hardware

| Part | Detail |
|------|--------|
| Active buzzer (bare, 2 legs) | built-in tone generator: sounds whenever its + leg is driven HIGH |
| 1 female-to-male jumper + 1 male-to-male jumper | pin 18 to the buzzer, buzzer to the − rail |

### Wiring

| # | Part | From | To |
|---|------|------|----|
| – | Buzzer long leg (+) | | hole f22 |
| – | Buzzer short leg (−) | | hole f25 |
| 1 | Purple F-M jumper | ESP32 pin 18 | hole j22 |
| 2 | Black M-M jumper | hole j25 | bottom − rail |

My ESP32 has only two GND pins and both were already in use (LED and − rail), so the buzzer takes
GND from the − rail. An active buzzer draws roughly 15–25 mA at 3.3 V, within what one ESP32 pin
can supply, so it needs no transistor.

![Stage 4 wiring](12_stage4_wiring.jpg)

![Node 1 complete](13_node1_complete.jpg)

### Firmware

Three alarms, each with its own sound, so staff can tell them apart without looking:

| Alarm | Trigger (bench timing) | Trigger (real, SR-13) | Sound |
|-------|------------------------|------------------------|-------|
| Door open too long | door open 15 s | door open 5 min | double chirp every 2 s |
| Over-temperature | above 28 °C for 10 s | above 8 °C for 10 min | slow: 1 s on, 1 s off |
| Security breach | `breach` typed in the Serial Monitor | command from the backend (Stage 8) | fast and continuous, red LED on solid |

- **Priority:** breach beats over-temperature, which beats door. Clearing a breach falls back to
  whatever alarm is still active.
- **Hold-off timers:** an alarm only sounds once its condition has lasted the full time, so a
  quick door opening or a brief warm spike does not cause a false alarm.
- **Hysteresis:** the over-temperature alarm clears only once the reading falls 0.5 °C below the
  limit, so it doesn't flicker on and off at the threshold.
- **Bench vs real timing:** `#define BENCH_TIMING 1` uses short limits for testing at a desk;
  `0` switches to the real SR-13 values for the fridge.
- **Non-blocking:** the buzzer patterns run on `millis()`, so the door and temperature are still
  checked while an alarm is sounding.
- Bench limit used: __ °C _(28 °C, or 26 °C if the hand test couldn't reach 28)_

### Tests

Timestamps come from the Serial Monitor's clock option, so the delays can be read straight off the
screenshots.

| # | Test | Type | Expected | Result |
|---|------|------|----------|--------|
| 1 | Press EN | Positive | 2 short beeps at start-up | ✅ |
| 2 | Door open 15 s | Positive | `DOOR OPEN TOO LONG`, double chirp; stops when closed | ✅ alarm __ s after opening |
| 3 | Probe warmed past the limit for 10 s | Positive | `OVER-TEMPERATURE`, slow beeps; clears after cooling | ✅ peak __ °C |
| 4 | `breach`, then `clear` | Positive | Fast beeps + LED solid; stops on `clear` | ✅ |
| 5 | `breach` during a door alarm | Priority | Breach takes over; door alarm returns after `clear` | ✅ |
| 6 | Door open < 15 s; probe warm < 10 s | Negative | No alarm at all | ✅ `alarm: NONE` throughout |

### 📸 Evidence

| File | Shows |
|------|-------|
| `12_stage4_wiring.jpg` | Buzzer and jumper positions |
| `13_node1_complete.jpg` | The finished Node 1 |
| `14_serial_startup.png` | Start-up lines and the alarm limits |
| `15_serial_door_alarm.png` | Door alarm and recovery |
| `16_serial_temp_alarm.png` | Over-temperature countdown, alarm and recovery |
| `17_serial_breach.png` | Breach taking priority, then `clear` |
| `18_serial_no_false_alarm.png` | Negative test: no false alarms |
| `19_alarm_patterns.mp4` | The three alarm sounds ([watch](19_alarm_patterns.mp4)) |

![Start-up](14_serial_startup.png)

![Door alarm](15_serial_door_alarm.png)

![Over-temperature alarm](16_serial_temp_alarm.png)

![Breach priority](17_serial_breach.png)

![No false alarms (negative test)](18_serial_no_false_alarm.png)

### Problems & fixes

- **The parts list said "buzzer module", but I had a bare buzzer.** A bare active buzzer plugs
  straight into the breadboard and is driven HIGH = on (the module version was LOW = on).
- **Only two GND pins on the ESP32.** Took the buzzer's GND from the − rail instead.

### Result

- [x] Stage 4: all three alarms sound with their own patterns, breach takes priority, and short
  events cause no false alarms. **Node 1 is complete.**
