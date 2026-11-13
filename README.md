# Secure Cold-Chain Vaccine-Fridge Monitor

An IoT system that monitors a vaccine refrigerator (temperature, door, mains power) and
**treats security as a first-class requirement** — device identity, encrypted transport,
least-privilege access, monitoring and incident response — built and tested end to end.

> Course: IIB20804 IoT Application Security · Mini Project
> Author: **Ariff Imran bin Alwi** (edge gateway & integration) · team project (see Credits)
> Status: 🚧 in development — this README grows as each stage lands.

---

## Why this project

Vaccines spoil silently. A fridge that drifts above 8 °C, a door left ajar, or a power cut
can destroy a whole batch — and an *attacker* who can forge or suppress those alerts is a
safety problem, not just a security one. This project builds the monitor **and** the
defences, then proves both with evidence.

## Architecture (target)

```
2× ESP32 nodes  ──MQTTS──►  Raspberry Pi 4 (edge gateway)  ──HTTPS──►  Backend server      ──►  Dashboards
  Node 1: temp,              Mosquitto broker (mTLS, ACL)              Mobius oneM2M IN-CSE       - clinic staff
  door, buzzer,              bridge.py (validate + forward)           MySQL (retention)          - security ops
  security LED               device auth, revocation                  Node-RED / Grafana
  Node 2: power-loss                                                  (monitoring, ≥3 alerts)
```

Full diagram: `docs/00-architecture.md` (and the editable `target_architecture.drawio`).

## Security highlights (what to look at)

| Area | Control | Evidence |
|------|---------|----------|
| Identity | Unique X.509 per device, no shared creds | `docs/06-security-hardening.md` |
| Transport | Mutual TLS; plaintext MQTT (1883) closed | before/after Wireshark capture |
| Access | Per-topic broker ACLs + oneM2M ACP (default-deny) | denied-origin RSC 4103 |
| Hardening | Host firewall, before/after port scans | nmap before/after |
| Monitoring | ≥3 alert rules across ≥2 layers, <60 s detect | dashboard screenshots |
| Response | Incident playbook + device revocation rehearsal | `security/incident-playbook.md` |
| Privacy | Data minimisation, ≤90-day retention | `docs/` + retention job |

## Repo layout

```
docs/            build log, one file per stage + evidence/ (photos, screenshots, pcaps)
firmware/        ESP32 sketches (node1_fridge, node2_power) — secrets kept OUT of git
gateway/         Mosquitto config/ACL + bridge.py (Raspberry Pi 4)
platform/        Mobius / oneM2M notes, ACP examples
security/        threat model (STRIDE + MTSFB), RTM, incident playbook
```

## 🔒 Secrets policy (read before your first commit)

**No Wi-Fi passwords, MQTT passwords, or private keys/certs ever go in git.**
Each node has a git-ignored `secrets.h`; copy it from the committed `secrets.h.example`:

```bash
cp firmware/node1_fridge/secrets.h.example firmware/node1_fridge/secrets.h
# edit secrets.h with your real values — it is already in .gitignore
```

`.gitignore` blocks `secrets.h`, `*.key`, `*.pem`, private `*.crt`, and build output.

## Build stages

- [x] Stages 0–1 — Toolchain + security LED (`docs/evidence/stage_1/01-toolchain-setup.md`)
- [x] Stage 2 — DS18B20 temperature probe (`docs/evidence/stage_2/02-ds18b20-temperature.md`)
- [x] Stage 3 — Reed switch door sensor (`docs/evidence/stage_3/03-reed-switch-door.md`)
- [x] Stage 4 — Buzzer alarms, Node 1 complete (`docs/evidence/stage_4/04-buzzer-alarms.md`)
- [x] Stage 5 — Node 2: 5 V power-loss detection (`docs/evidence/stage_5/05-node2-power.md`)
- [ ] Stage 6 — Wi-Fi + MQTT baseline (insecure, measured)
- [ ] Stages 7–8 — Gateway + Mobius oneM2M, dashboards and command path
- [ ] Stages 9–11 — Security hardening (mTLS, ACL, signed updates, firewall, ACP, retention)
- [ ] Stage 12 — Monitoring + incident response; threat model

## Standards referenced

oneM2M / ITU-T Y.4500.3 (Mobius) · ITU-T X.1352 & X.1361 · MTSFB IoT SSP · ETSI EN 303 645 ·
NIST IR 8259A · IEC 62443 · PDPA 2010.

## Credits

Team project — roles: Ariff (edge gateway & integration), Wan (ESP32 nodes/firmware),
Luqman (backend platform & dashboards), Zainul (security testing, threat model & RTM).
