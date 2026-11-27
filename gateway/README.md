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
