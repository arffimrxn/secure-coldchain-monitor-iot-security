# Edge gateway (Raspberry Pi 4)

- `mosquitto/` — broker config + ACL (NO keys/certs committed)
- `bridge.py` — validates each reading and forwards to Mobius over HTTPS

- `dashboard.json` — Node-RED flow (node-red-dashboard 3.x): Smart Fridge Monitor + Security Ops (SOC) tabs

Baseline first (port 1883, no auth) to measure, then harden to MQTTS 8883 + per-topic ACL.

## Dashboard setup

`dashboard.json` keeps the SOC event log (90 days) and a 24 h fridge temperature log in a
`file` context store, so it survives restarts. Enable it in `~/.node-red/settings.js` on the Pi,
then `sudo systemctl restart nodered`:

```js
contextStorage: {
    default: { module: "memory" },
    file:    { module: "localfilesystem" }
},
```

Alert thresholds live in the **Alert Config (BENCH_TIMING)** function node. Keep its
`BENCH_TIMING` equal to the firmware's `#define BENCH_TIMING` in `firmware/node1_fridge/node1_fridge.ino`.

## SOC detection rules (Stage 12 / M10)

| Rule | Layer | Fires on | Time to alert |
|---|---|---|---|
| R1 Device integrity | 1 | seq replay/gap, implausible or `fake` reading, malformed/retained telemetry, LWT, watchdog STALE 15 s / OFFLINE 30 s | on the message; watchdog 15–30 s |
| R2 Gateway auth / ACL / SSH | 2 | any message on `secmon/#` (from `logshipper.py`); extra CRITICAL when 3 arrive within 60 s | on the message |
| R3 Statistical anomaly | 1 | > 6 messages from one sensor in a 10 s window (baseline ~2); temperature more than 3 σ from the mean of the last 10 readings (σ floor 0.2 °C) | on the message |

Trigger each rule for evidence (run on the Pi while the broker still accepts 1883; afterwards
add the 8883 TLS options). Test telemetry is also forwarded by `bridge.py` to Mobius.

```bash
# R2 - three alerts within 60 s also fire the burst alert
for i in 1 2 3; do mosquitto_pub -h localhost -t secmon/gateway/alerts \
  -m '{"layer":"gateway","rule":"ssh_failed_login","severity":"HIGH","msg":"Failed password for root (test)"}'; done

# R3 message flood - 8 messages in about 2 s
for i in $(seq 1 8); do mosquitto_pub -h localhost -t campus/sensor01/telemetry \
  -m "{\"device\":\"sensor01\",\"seq\":$((9000+i)),\"temp\":5.0,\"door\":\"closed\",\"door_open_s\":0,\"alarm\":\"NONE\"}"; sleep 0.25; done
```
