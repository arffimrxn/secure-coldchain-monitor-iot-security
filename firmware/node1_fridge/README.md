# Node 1 — fridge monitor (`sensor01`)

ESP32 firmware for the node that sits at the vaccine fridge. It reads the fridge temperature and the
door (lid) state and sounds local alarms with a buzzer and a red security LED.

**Status:** Stage 4 — Node 1 complete, runs stand-alone. Wi-Fi and MQTT are added at Stage 6.

## Hardware

| Part | Notes |
|---|---|
| ESP32-DevKitC (ESP32-D0WD-V3, CH340C USB, USB-C) | labelled "N1 fridge"; only two GND pins |
| DS18B20 waterproof probe + adapter module | the module has the 1-Wire pull-up built in |
| Reed switch module (LM393) + neodymium magnet | magnet on the lid edge, ≤ 1 cm from the reed |
| Bare active buzzer | sounds when its + leg is HIGH |
| Red 5 mm LED + 220 Ω resistor | security-breach indicator |
| MB102 breadboard, F-M jumpers, 1 M-M jumper | |

## Pin map

| ESP32 pin | Connected to | Mode | Stage |
|---|---|---|---|
| GPIO23 | 220 Ω → red LED → GND | output | 1 |
| GPIO4 | DS18B20 module DAT | 1-Wire | 2 |
| GPIO19 | reed switch module DO | `INPUT_PULLUP` | 3 |
| GPIO18 | buzzer + leg (− leg to the − rail) | output, HIGH = sound | 4 |
| 3V3 / GND | breadboard + / − rails | power | 2 |

All modules run at 3.3 V. Wire by the label printed on the board, not by position: the pins on this
board sit in different places from the standard drawing.

Wiring diagrams, photos and test evidence for each stage:
[Stage 1](../../docs/evidence/stage_1/01-toolchain-setup.md) ·
[Stage 2](../../docs/evidence/stage_2/02-ds18b20-temperature.md) ·
[Stage 3](../../docs/evidence/stage_3/03-reed-switch-door.md) ·
[Stage 4](../../docs/evidence/stage_4/04-buzzer-alarms.md)

## Build and upload

- Arduino IDE 2.3.10 · board package **esp32 by Espressif Systems** 3.3.12 · board **ESP32 Dev Module**
- Libraries (Library Manager): **OneWire** by Paul Stoffregen, **DallasTemperature** by Miles Burton
- Open `node1_fridge.ino` from this folder (one sketch per folder; the folder name must match the
  sketch name), pick the CH340 COM port, upload.
- Serial Monitor: **115200 baud**, line ending **New Line**.

## Settings at the top of the sketch

| Setting | Default | Meaning |
|---|---|---|
| `BENCH_TIMING` | `1` | `1` = short limits for desk testing; `0` = the real SR-13 limits. **Set to `0` for the demo.** |
| `TEMP_HIGH_C` | 28.0 (bench) / 8.0 (real) | over-temperature limit in °C |
| `TEMP_ALARM_MS` | 10 s (bench) / 10 min (real) | how long the temperature must stay over the limit |
| `DOOR_ALARM_MS` | 15 s (bench) / 5 min (real) | how long the door may stay open |
| `TEMP_CLEAR_MARGIN` | 0.5 °C | hysteresis: the alarm clears 0.5 °C below the limit |
| `DOOR_CLOSED_LEVEL` | `LOW` | DO level with the magnet near; flip to `HIGH` if OPEN/CLOSED read backwards |
| `BUZZER_ON_LEVEL` | `HIGH` | bare active buzzer; a low-level-trigger module would need `LOW` |

## What it does

- **Temperature** every 2 s, read without blocking (the DS18B20 converts in the background), so the
  door is checked continuously.
- **Door** changes are reported once each, after 50 ms of debounce, with an open-time counter.
- **Alarms (SR-13)**, each with its own sound, priority breach > temperature > door:

  | Alarm | Trigger (real limits) | Sound | LED |
  |---|---|---|---|
  | Door open too long | open for 5 min | double chirp every 2 s | heartbeat |
  | Over-temperature | above 8 °C for 10 min | 1 s on, 1 s off | heartbeat |
  | Security breach | `breach` command (backend from Stage 8) | fast, continuous | solid on |

- **Hold-off timers** stop short events (a quick door opening, a brief warm spike) causing false alarms.
- **LED heartbeat:** one short flash per temperature reading; if it stops, the firmware has frozen.

## Serial Monitor

Commands (type and press Enter): `breach` starts the breach alarm, `clear` stops it.

```
Node 1 - Stage 4: temp GPIO4 (1 found), door GPIO19, buzzer GPIO18
Alarm limits: over 28.0 C for 10 s, door open 15 s (bench timing)
Temperature: 21.44 C | door CLOSED | alarm: NONE
>>> DOOR OPENED
Temperature: 21.50 C | door OPEN for 15 s | alarm: NONE
>>> ALARM NONE -> DOOR OPEN TOO LONG
>>> DOOR CLOSED (was open 17.3 s)
```

## Fail-safe behaviour

- A disconnected probe prints **"Probe not found"** instead of a plausible-looking number.
- The reed input uses the internal pull-up, so a **cut DO wire reads as OPEN** and raises the alarm.
- The buzzer is silenced first thing at start-up, then gives **two short beeps** as a self-test.
- The local alarms don't depend on any network, and must stay that way when Wi-Fi is added.

## Secrets

This sketch holds no secrets. From Stage 6, Wi-Fi and broker settings go in `secrets.h`, which is
git-ignored: copy `secrets.h.example` to `secrets.h` in this folder and fill it in. Never commit
`secrets.h`, keys or certificates.

## Next

- **Stage 6:** Wi-Fi + MQTT — JSON telemetry on `campus/sensor01/telemetry`, Last Will for offline detection
- **Stage 8:** messages on `campus/sensor01/cmd` drive the breach alarm
- **Stage 9:** mutual TLS on 8883 (certificate CN `sensor01`), NTP time before TLS
