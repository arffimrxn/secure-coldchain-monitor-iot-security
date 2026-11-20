# Stages 7–8 — Gateway (`bridge.py`), Mobius oneM2M & Dashboards

Full write-ups and verified evidence are documented in:
- **Stage 7 (Gateway + Mobius 3.0.1):** [`docs/evidence/stage_7/07-gateway-mobius.md`](evidence/stage_7/07-gateway-mobius.md)
- **Stage 8 (Dashboards + Remote Command Path):** [`docs/evidence/stage_8/08-dashboards-cmd.md`](evidence/stage_8/08-dashboards-cmd.md)

## Summary
- **Stage 7:** Raspberry Pi 4 (`192.168.0.24`) runs [`gateway/bridge.py`](../gateway/bridge.py), forwarding MQTT telemetry from `sensor01` and `sensor02` to Mobius 3.0.1 (`192.168.0.12:7579`) with `201 / RSC 2001` responses. Baseline lack of oneM2M ACP (`X-M2M-Origin: Cfakehacker` returning `200 OK`) is recorded for Stage 11.
- **Stage 8:** [`firmware/node1_fridge/node1_fridge.ino`](../firmware/node1_fridge/node1_fridge.ino) subscribes to `campus/sensor01/cmd` (`"breach"` / `"clear"`), and [`gateway/nodered_flow.json`](../gateway/nodered_flow.json) provides both the **Cold-Chain Monitor** and **Security Ops (SOC)** dashboards.
