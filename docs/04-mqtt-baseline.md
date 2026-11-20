# Stage 6 — MQTT baseline (insecure, measured)

Full write-up and all verified Stage 6 evidence (`30_`–`36_`) are documented in:
- [`docs/evidence/stage_6/06-mqtt-baseline.md`](evidence/stage_6/06-mqtt-baseline.md)

## Summary
- **Gateway:** Physical Raspberry Pi 4 (`192.168.0.24`), Mosquitto 2.0.21 listening on plaintext `1883` (`allow_anonymous true`).
- **Nodes:** `sensor01` (`192.168.0.34`) and `sensor02` (`192.168.0.35`) publishing JSON every 5 s and immediately on events, with 15 s keepalive + Last Will (`"status":"offline"`).
- **Baseline findings captured:** Cleartext JSON visible in Wireshark (`33_plaintext_mqtt.pcapng` / `.png`) and unauthenticated spoofing accepted (`36_spoofed_publish.png`), ready to be mitigated in Stage 9.
