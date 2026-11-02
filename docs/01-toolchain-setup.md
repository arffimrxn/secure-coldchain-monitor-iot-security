# Stage 1 — Toolchain + blink

**Goal:** Arduino IDE flashes the ESP32; onboard LED blinks.

## What I did
- Installed CH340 driver (ESP32 is CH340C)
- Installed Arduino IDE 2.x + esp32 board package (Espressif)
- Board: ESP32 Dev Module · Port: COM__
- Uploaded Blink / node1_fridge.ino (Stage 1)

## 📸 Evidence to capture
- [ ] `evidence/01-boards-manager-esp32.png` — Boards Manager showing esp32 installed
- [ ] `evidence/01-upload-done.png` — IDE "Done uploading"
- [ ] `evidence/01-blink.jpg` — photo/short clip of the blinking onboard LED

## Problems & fixes
- (note any COM-port / "Connecting..." / BOOT-button issues here)

## Result
- [ ] Onboard LED blinks at 1 Hz ✅
