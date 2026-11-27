# Stage 9 — PKI, Mutual TLS (`8883`), Topic ACLs & Signed Updates (`M1–M4`)

> **Status: 🟡 IN PROGRESS — checklist.** This file is the Person 2 checklist for Stage 9 and becomes the
> evidence write-up as each box is ticked. Results below are **expected** values until a test is run;
> replace them with what you actually observed and add the screenshot.

**Goal:** Remove plaintext MQTT (`1883`) and anonymous publishing: unique X.509 identity per device (`M1`),
mutual TLS on `8883` (`M2`), per-device topic ACLs (`M3`), and ECDSA-signed config updates with
anti-rollback (`M4`).

| | Person 1 (Pi: PKI, Mosquitto) | Person 2 (this checklist: ESP32, Node-RED, Wireshark, updater) |
| :--- | :--- | :--- |
| Tasks | 9.1 PKI · 9.2 close 1883 + ACL · 9.3 `bridge.py` TLS · 9.4 signing key | **9.5** firmware mTLS · **9.6** Node-RED mTLS · **9.7** signed update tests |
| Evidence | `60_` certs | `61_`–`65_` |

---

## 0. Before you start (blocked until Person 1 finishes 9.1 – 9.2)

- [ ] Person 1 confirms `ss -tlnp | grep -E "1883|8883"` shows **only 8883**.
- [ ] `/etc/hosts` on the Pi contains `127.0.0.1 gw.grp3.lab` (Node-RED and `bridge.py` connect by name).
- [ ] Received **privately (USB / direct)** — never GitHub or group chat:
  `ca.crt`, `sensor01.crt/.key`, `sensor02.crt/.key`, `dashboard.crt/.key`.
  Test-only, for evidence `64_` and the Stage 12 re-test: `logshipper.crt/.key` (or ask Person 1 to run those commands).
  **Never** `ca.key` or `fw_sign.key`.
- [ ] Received for 9.7: `fw_sign.pub`, `config.json` (v3), `config.sig`.
- [x] `.gitignore` extended for PKI files: `*.key`, `*.pem` plus `*.csr`, `*.srl`, `*.crl`, `pki/`, `fw_sign.*` (except `fw_sign.pub`).
  Still check `git status` shows **no** certificate or key files before every commit.
- [ ] Capture the **Before** side now if not already done: Stage 6 `33_plaintext_mqtt.png` (readable JSON on 1883) and `36_spoofed_publish.png`.

---

## 1. Task 9.5 — ESP32 firmware on `WiFiClientSecure` (`M1`, `M2`)

Both `firmware/node1_fridge/node1_fridge.ino` and `firmware/node2_power/node2_power.ino` currently use
`WiFiClient` + `PubSubClient` on `MQTT_PORT 1883`.

- [ ] In each node's **git-ignored** `secrets.h` (template: `secrets.h.example`):
  - `MQTT_HOST` = the Pi's **LAN** IP (the ESP32 is not on Tailscale), `MQTT_PORT 8883`
  - `BROKER_TLS_NAME "gw.grp3.lab"` (the ESP32 cannot verify an IP against the cert; it verifies this name)
  - `CA_CERT`, `CLIENT_CERT`, `CLIENT_KEY` as `R"EOF( ... )EOF"` raw strings — `sensor01.*` on Node 1, `sensor02.*` on Node 2
- [ ] `#include <WiFiClientSecure.h>` and replace `WiFiClient wifiClient;` with `WiFiClientSecure wifiClient;`.
- [ ] **Wait for NTP before TLS.** `configTime()` is already called on Wi-Fi connect, but the code does not wait
  for a valid clock. Skip the MQTT attempt until `time(nullptr) > 1700000000` (a 2023+ date). Without this the
  ESP32 thinks it is 1970 and rejects the certificate as "not yet valid". Keep the sensor/buzzer loop running while waiting.
- [ ] Open the TLS socket yourself, then let `PubSubClient` reuse it (it only reconnects when the socket is closed):
  ```cpp
  IPAddress brokerIP; brokerIP.fromString(MQTT_HOST);
  if (!wifiClient.connected()) {
    wifiClient.connect(brokerIP, MQTT_PORT, BROKER_TLS_NAME, CA_CERT, CLIENT_CERT, CLIENT_KEY);
  }
  if (wifiClient.connected() && mqtt.connect(DEVICE_ID, TOPIC_TELEMETRY, 1, false, lwt)) { ... }
  ```
  Keep `DEVICE_ID` = certificate CN (`sensor01` / `sensor02`): with `use_identity_as_username` the CN is the ACL username.
- [ ] Flash both boards; Serial Monitor shows the connection on `8883` and telemetry keeps flowing.
- [ ] Dashboard shows both nodes **ONLINE** again (System Overview links animated).

**If it fails:** `rc=-2` / handshake error → check the clock first, then that `BROKER_TLS_NAME` matches the
server cert SAN (`DNS:gw.grp3.lab`), then that the right client cert/key pair is loaded.

---

## 2. Task 9.6 — Node-RED dashboard on mTLS (`M2`)

All MQTT nodes in `gateway/dashboard.json` (sensor01, sensor02, `secmon/#`, Publish cmd) share **one** broker
config node, **Raspberry Pi 4 Mosquitto** — only that node changes.

- [ ] In the editor, open the broker config node:
  - Server `gw.grp3.lab`, Port `8883`, tick **Use TLS**
  - New TLS config: upload `ca.crt` (CA), `dashboard.crt` (certificate), `dashboard.key` (private key);
    tick **Verify server certificate**; Server Name `gw.grp3.lab`
- [ ] Set `credentialSecret` in `~/.node-red/settings.js` (uploaded keys are stored in `flows_cred.json`,
  encrypted with this secret; without it Node-RED uses a generated key). Keep the secret out of Git.
- [ ] Deploy → all MQTT nodes show **connected**; telemetry, rules R1–R3 and the **Trigger / Clear breach** buttons still work.
- [ ] Export the flow to `gateway/dashboard.json` and commit. Check the diff: the broker node now has
  `port 8883` and a `tls` reference; **no certificate or key text** may appear in the file.

---

## 3. Evidence tests (`61_` – `64_`)

Run on the Pi in `~/pki` (where the certs are). `S="-h gw.grp3.lab -p 8883 --cafile ca.crt"`.

| # | Test | Type | Command | Expected | Result |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 61 | All identities connect on 8883 | Positive (`M1`, `M2`) | ESP32 Serial Monitors + `sudo journalctl -u mosquitto` / `mosquitto.log` | `sensor01`, `sensor02`, `bridge`, `dashboard` connected with their CN as username | ⬜ |
| 62 | Traffic is encrypted | Positive (`M2`) | `sudo tcpdump -i wlan0 -w 62_tls_8883.pcapng port 8883` (Ctrl+C after ~30 s), open in Wireshark | TLS handshake + **Application Data** only, no readable JSON (compare `33_`) | ⬜ |
| 63a | Plaintext port closed | Negative (`M2`) | `mosquitto_sub -h <PI_LAN_IP> -p 1883 -t '#' -W 5` | `Connection refused` | ⬜ |
| 63b | TLS without client cert | Negative (`M2`) | `mosquitto_sub $S -t '#' -W 5` | TLS handshake / connection rejected | ⬜ |
| 64 | Spoofed publish with another device's cert | Negative (`M3`) | `mosquitto_pub $S --cert sensor02.crt --key sensor02.key -V mqttv5 -q 1 -t campus/sensor01/telemetry -m '{"device":"sensor01","seq":1,"temp":99}'` | `Not authorized` (PUBACK 0x87); nothing appears on the dashboard; `Denied PUBLISH from sensor02` in the broker log (may need `log_type all`) | ⬜ |
| 64b | Allowed publish for comparison | Positive (`M3`) | same with `--cert sensor01.crt --key sensor01.key` | Accepted; reading appears on the dashboard | ⬜ |

**Stage 12 re-test after mTLS** (the 1883 commands in `stage_12` no longer work):
R2 must be sent as `logshipper` (`--cert logshipper.crt --key logshipper.key -t secmon/gateway/alerts`) and the R3 flood as
`sensor01`; the dashboard identity is **not** allowed to publish to either topic.

---

## 4. Task 9.7 — Signed config update & anti-rollback (`M4`)

- [ ] **Write `updater.py`** — the handoff mentions `src/updater.py`, but it is not in this repo yet. Minimum behaviour:
  1. load `config.json` and `config.sig`, verify the ECDSA-P256/SHA-256 signature with `fw_sign.pub`
     (e.g. Python `cryptography`, or `openssl dgst -sha256 -verify fw_sign.pub -signature config.sig config.json`);
  2. if invalid → print `REJECTED: invalid signature`, exit non-zero;
  3. if `version <= CURRENT_VERSION` (start at `2`) → print `REJECTED: rollback attempt`;
  4. else → print `ACCEPTED: applying version N` and store `N` as the new current version.
  Verify the signature **before** parsing/using any field.
- [ ] Tests for `65_updater_m4.png`:

| # | Test | Type | Expected | Result |
| :--- | :--- | :--- | :--- | :--- |
| 65a | Valid `config.json` v3 + `config.sig` | Positive | `ACCEPTED: applying version 3` | ⬜ |
| 65b | Change one character in `config.json`, keep old signature | Negative (tamper) | `REJECTED: invalid signature` | ⬜ |
| 65c | Correctly signed `version: 2` (or 1) config | Negative (rollback) | `REJECTED: rollback attempt` | ⬜ |

(65c needs Person 1 to sign an old-version config — `fw_sign.key` stays with Person 1.)

---

## 5. 📸 Evidence

| File | Shows | Owner | Status |
| :--- | :--- | :--- | :--- |
| `60_pki_certs.png` | `openssl x509 -noout -subject -ext subjectAltName` for server + client certs | Person 1 | ⬜ |
| `61_mtls_nodes_connected.png` | All four identities authenticated on 8883 | Person 1 + 2 | ⬜ |
| `62_wireshark_tls_8883.png` + `62_tls_8883.pcapng` | Encrypted Application Data (After; Before = `33_`) | Person 2 | ⬜ |
| `63_neg_1883_closed_nocert.png` | 63a + 63b | Person 2 | ⬜ |
| `64_neg_acl_spoof_denied.png` | 64 (+ 64b) — mitigates `36_spoofed_publish.png` | Person 2 | ⬜ |
| `65_updater_m4.png` | 65a – 65c | Person 2 | ⬜ |

---

## 6. Result

- [ ] `M1` unique X.509 identity per device and service
- [ ] `M2` mutual TLS on 8883, 1883 closed
- [ ] `M3` per-device topic ACLs enforced
- [ ] `M4` signed updates with anti-rollback

**Next:** Stage 10 — gateway & server hardening, RBAC (`M5`, `M6`).
