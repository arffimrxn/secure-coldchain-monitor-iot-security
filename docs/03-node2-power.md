# Stage 5: Node 2 power-loss detection (5 V charger method)

**Goal:** detect a mains power cut at the fridge's socket and report it within 60 s (SR-14),
without wiring anything to mains, from a second ESP32 that keeps running through the cut.

**Node 2 pin map**

| ESP32 pin | Part | Stage |
|---|---|---|
| GPIO19 | 5 V power sense from the charger, through a 10 kΩ / 20 kΩ divider | 5b |
| GND (first pin) | divider ground, joined to the charger's GND | 5b |

## Why a phone charger is the sensor

The first plan used an AC220V optocoupler module wired to mains. I replaced it with a safer method:
a USB phone charger plugged into the **same switched socket as the fridge**. While the socket has
power, the charger gives 5 V; after a cut, it gives 0 V. The charger does the mains isolation, so
nothing on the breadboard ever touches 230 V.

The charger only provides a **signal**. Node 2 itself runs from a power bank (the laptop while
testing), so it stays alive to report the cut. Powering the ESP32 from the charger would switch it
off at the same moment as the fridge, and it could never send the alert.

---

## Stage 5a: smoke test

Flashed the second board on its own, labelled both boards ("N1 fridge", "N2 power") so the wrong one
never gets flashed, and recorded Node 2's identity (chip, flash size, factory MAC). The MAC will be
tied to Node 2's certificate at Stage 9.

![Node 2 smoke test](20_node2_smoke_test.png)

![Boards labelled](21_boards_labelled.jpg)

---

## Stage 5b: 5 V power sensing

### Hardware

| Part | Detail |
|------|--------|
| USB-A phone charger | in the fridge's switched wall socket |
| Old USB-A cable, cut | pink = +5 V, white = GND (charge-only cable, see Problems & fixes) |
| 3 × 10 kΩ resistors | one on top, two in series (= 20 kΩ) to GND |
| MB102 mini breadboard (170 holes) | no power rails needed |
| 2 F-M jumpers (whole) + 2 cut in half | the halves' male pins finish the USB leads |

### The voltage divider

The ESP32's pins only take 3.3 V, so the charger's 5 V goes through a divider first:

```
+5 V ── 10 kΩ ──┬── 10 kΩ ── 10 kΩ ── GND
                │
             GPIO19   (5 V × 20 kΩ / 30 kΩ ≈ 3.3 V)
```

### Wiring

| # | Part | From | To |
|---|------|------|----|
| 1 | 10 kΩ | b3 | b7 |
| 2 | 10 kΩ | c7 | c11 |
| 3 | 10 kΩ (top) | d11 | d15 |
| 4 | Pink lead (+5 V) | charger | a15 |
| 5 | White lead (GND) | charger | a3 |
| 6 | Black jumper | ESP32 GND | e3 |
| 7 | Yellow jumper | ESP32 pin 19 | e11 |

Column 3 joins the charger's GND to the ESP32's GND, so both sides measure from the same 0 V.

![USB lead](23_usb_lead.jpg)

![Stage 5b wiring](24_stage5b_wiring.jpg)

![Node 2 setup](25_node2_setup.jpg)

### Firmware

- Reads GPIO19: HIGH = mains on, LOW = power cut.
- **Debounce:** a change only counts once it has lasted 200 ms, so a flicker doesn't raise an alarm.
- Prints `>>> POWER LOST` and `>>> POWER RESTORED (was off x s)` the moment the state changes, plus a
  status line every 2 s with the time the power has been off.
- **Fail-safe input:** GPIO19 uses the internal pull-down, so a broken signal wire reads as a power
  cut instead of "all fine".

### Tests

| # | Test | Type | Expected | Result |
|---|------|------|----------|--------|
| 1 | Socket on | Positive | `Mains power: ON` | ✅ |
| 2 | Switch the socket off | Positive | `>>> POWER LOST` within 60 s (SR-14) | ✅ after __ s |
| 3 | Switch it back on | Positive | `>>> POWER RESTORED (was off x s)` | ✅ |
| 4 | Pull the pin-19 jumper out of e11 (socket still on) | Negative | `POWER LOST` (fail-safe) | ✅ reported as a cut; restored after reconnecting |

Test 2's delay comes from the charger: its output capacitors keep the 5 V up for a moment after the
socket goes off. Even so, the cut is reported well inside the 60 s that SR-14 allows.

The rest of SR-14, noticing when Node 2 itself goes silent (for example, its power bank removed),
needs the network, so it is tested from Stage 6 onward.

### 📸 Evidence

| File | Shows |
|------|-------|
| `20_node2_smoke_test.png` | Node 2 identity and "alive" lines |
| `21_boards_labelled.jpg` | Both boards labelled |
| `22_usb_polarity_test.jpg` | The LED test that found which wire is +5 V |
| `23_usb_lead.jpg` | The finished, labelled USB lead |
| `24_stage5b_wiring.jpg` | The divider and jumpers on the mini breadboard |
| `25_node2_setup.jpg` | Socket, charger, lead, breadboard, Node 2 and laptop |
| `26_serial_power_on.png` | Start-up and steady power-on readings |
| `27_serial_power_cut.png` | Power lost, off-timer, power restored |
| `28_power_cut_timing.mp4` | Switch and screen together, proving the detection time ([watch](28_power_cut_timing.mp4)) |
| `29_serial_signal_unplugged.png` | Negative test: a broken signal wire is reported as a cut |

![Power on](26_serial_power_on.png)

![Power cut and restore](27_serial_power_cut.png)

![Signal wire unplugged (negative test)](29_serial_signal_unplugged.png)

---

## Problems & fixes

- **The cable's wires were pink and white, not red and black.** It was a charge-only cable with
  non-standard colours, so I didn't guess. A spare LED and 220 Ω resistor in series across the two
  wires only lit one way round, which showed **pink = +5 V**. Both leads were then labelled with tape.
- **Replaced the AC220V opto module with the 5 V charger method**, so no mains wiring is needed
  anywhere in the project.

![USB polarity test](22_usb_polarity_test.jpg)

## Result

- [x] Stage 5: Node 2 detects a mains cut at the fridge's socket and reports it well within
  60 s, reports the restore with the outage length, and treats a broken signal wire as a fault.
