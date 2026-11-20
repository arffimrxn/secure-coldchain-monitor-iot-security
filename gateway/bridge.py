import json
import os
import time
import requests
import paho.mqtt.client as mqtt

# Stage 7 baseline: plain HTTP to Mobius CSE (switched to HTTPS + certs in Stage 11)
CSE = os.environ.get("MOBIUS_CSE", "http://192.168.0.12:7579/Mobius")
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))


def post_cin(device, payload):
    headers = {
        "X-M2M-Origin": "C" + device,
        "X-M2M-RI": f"rq-{device}-{int(time.time() * 1000)}",
        "X-M2M-RVI": "3",
        "Content-Type": "application/json;ty=4",
        "Accept": "application/json",
    }
    try:
        r = requests.post(
            f"{CSE}/{device}/telemetry",
            headers=headers,
            json={"m2m:cin": {"con": payload}},
            timeout=5,
        )
        print(device, r.status_code, r.headers.get("X-M2M-RSC"), flush=True)
    except Exception as e:
        print(device, "ERROR:", e, flush=True)


def on_connect(client, userdata, flags, reason_code, properties):
    print("Connected to Mosquitto:", reason_code, flush=True)
    client.subscribe("campus/+/telemetry")


def on_message(client, userdata, msg):
    device = msg.topic.split("/")[1]
    try:
        payload = json.loads(msg.payload)
    except Exception:
        payload = msg.payload.decode("utf-8", errors="replace")
    post_cin(device, payload)


c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="bridge")
c.on_connect = on_connect
c.on_message = on_message
c.connect(MQTT_HOST, MQTT_PORT)
c.loop_forever()
