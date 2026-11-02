# Architecture

See the editable diagram `target_architecture.drawio` (open at app.diagrams.net) and the
rendered PNG in `evidence/`.

- **Device tier:** Node 1 (temp, door, buzzer, security LED), Node 2 (power-loss)
- **Edge gateway (Pi 4):** Mosquitto broker (mTLS, per-topic ACL) + bridge.py
- **Backend server:** Mobius oneM2M IN-CSE + MySQL + Node-RED/Grafana
- **Users:** clinic staff dashboard, security operations dashboard

## 📸 Evidence to capture
- [ ] `evidence/00-architecture.png`
