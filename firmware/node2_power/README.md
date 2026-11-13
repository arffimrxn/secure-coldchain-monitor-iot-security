# Node 2 — power monitor (`sensor02`)

ESP32 firmware for the node that watches the fridge's mains supply. It detects a power cut at the
fridge's socket and reports it, while running from its own power bank so it stays on through the cut.

**Status:** Stage 5b — power sensing complete, runs stand-alone. Wi-Fi and MQTT are added at Stage 6.

## How it senses mains power (no mains wiring)

A USB phone charger is plugged into **the same switched socket as the fridge**. While the socket has
power the charger gives 5 V; after a cut it gives 0 V. The charger does the mains isolation, so
nothing on the breadboard ever touches 230 V. The charger only provides a **signal**; the ESP32 is
powered separately (power bank in the demo, the laptop while testing).

The ESP32's pins only take 3.3 V, so the 5 V goes through a divider:

```
charger +5 V ── 10 kΩ ──┬── 10 kΩ ── 10 kΩ ── charger GND ── ESP32 GND
                        │
                     GPIO19     5 V × 20 kΩ / 30 kΩ ≈ 3.3 V  (HIGH = power on)
```

## Hardware

| Part | Notes |
|---|---|
| ESP32-DevKitC (ESP32-D0WD-V3, CH340C USB, USB-C) | labelled "N2 power" |
| USB-A phone charger | in the fridge's switched socket |
| Old USB-A cable, cut | **pink = +5 V, white = GND** (found with an LED test; leads taped and labelled) |
| 3 × 10 kΩ resistors | one on top, two in series (= 20 kΩ) to GND |
| MB102 mini breadboard (170 holes) | |
| Power bank + USB-C cable | keeps Node 2 running during a cut |

## Pin map and breadboard

| ESP32 pin | Connected to | Mode |
|---|---|---|
| GPIO19 | divider midpoint (hole e11) | `INPUT_PULLDOWN` |
| GND | divider ground / charger GND (hole e3) | ground |

| Hole | Part |
|---|---|
| b3 → b7 | 10 kΩ |
| c7 → c11 | 10 kΩ |
| d11 → d15 | 10 kΩ (top) |
| a15 | pink lead, +5 V |
| a3 | white lead, GND |

Wiring diagram, photos and test evidence: [Stage 5](../../docs/evidence/stage_5/05-node2-power.md)

## Build and upload

- Arduino IDE 2.3.10 · board package **esp32 by Espressif Systems** 3.3.12 · board **ESP32 Dev Module**
- No extra libraries.
- Open `node2_power.ino` from this folder, pick Node 2's COM port (not Node 1's), upload.
- Serial Monitor: **115200 baud**.

**Power-up order:** socket switched off → connect Node 2 → plug the charger in → USB-A lead into the
charger → switch the socket on.

## Settings at the top of the sketch

| Setting | Default | Meaning |
|---|---|---|
| `POWER_PIN` | 19 | divider input |
| `STABLE_MS` | 200 ms | a change must last this long to count (ignores flicker) |
| `STATUS_MS` | 2000 ms | how often the status line prints |

## What it does

- **Power cut:** prints `>>> POWER LOST` once the pin has been LOW for 200 ms, then counts the outage.
- **Restore:** prints `>>> POWER RESTORED (was off x s)`.
- A status line every 2 s, and the board's factory MAC at start-up (tied to its certificate at Stage 9).
- The cut is reported well within the **60 s that SR-14 allows**. The short delay after switching
  off comes from the charger's output capacitors fading, not from the firmware.

## Serial Monitor

```
Node 2 - Stage 5b: 5 V power sensing on GPIO19
MAC:   xx:xx:xx:xx:xx:xx
Mains power is ON at start
Mains power: ON  | uptime 4 s
>>> POWER LOST
Mains power: OFF for 6 s | uptime 12 s
>>> POWER RESTORED (was off 8.4 s)
```

## Fail-safe behaviour

- GPIO19 uses the internal pull-down, so a **broken signal wire reads as a power cut** instead of
  "all fine".

## Safety

- Never wire anything to mains; only ever flip the socket's switch.
- Check the **pink (+5 V) lead is in a15 and the white (GND) lead in a3** before plugging the charger
  in. Swapping them puts a negative voltage on the ESP32's pin.
- Keep the two bare lead ends from touching while the charger is plugged in.

## Secrets

This sketch holds no secrets. From Stage 6, Wi-Fi and broker settings go in `secrets.h`, which is
git-ignored: copy `secrets.h.example` to `secrets.h` in this folder and fill it in. Never commit
`secrets.h`, keys or certificates.

## Next

- **Stage 5c (planned):** a local power-cut buzzer on GPIO18
- **Stage 6:** Wi-Fi + MQTT — JSON on `campus/sensor02/telemetry`, and a Last Will so the broker
  reports Node 2 going silent (the rest of SR-14)
- **Stage 9:** mutual TLS on 8883 (certificate CN `sensor02`), NTP time before TLS
