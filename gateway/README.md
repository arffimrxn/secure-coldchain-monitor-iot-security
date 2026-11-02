# Edge gateway (Raspberry Pi 4)

- `mosquitto/` — broker config + ACL (NO keys/certs committed)
- `bridge.py` — validates each reading and forwards to Mobius over HTTPS

Baseline first (port 1883, no auth) to measure, then harden to MQTTS 8883 + per-topic ACL.
