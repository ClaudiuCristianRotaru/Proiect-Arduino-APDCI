import serial
import json
import uuid
import time
import paho.mqtt.client as mqtt
from datetime import datetime, timezone

SERIAL_PORT = "COM6"
BAUD_RATE = 115200
node_registry = {"COM6": "CA_01"}

heartbeat_deadline = 90  # seconds
last_heard = {}


def build_message(type, node, subject, status, payload):
    return {
        "type": type,
        "id": str(uuid.uuid4()),
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "node": node,
        "subject": subject,
        "status": status,
        "payload": payload,
    }


def start_gateway():
    ser = None
    last_check_time = time.time()
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"--- Gateway Started on {SERIAL_PORT} ---")

        while True:
            if time.time() - last_check_time > 10:
                check_dead_nodes()
                last_check_time = time.time()

            raw_line = ser.readline().decode("utf-8", errors="ignore").strip()

            if not raw_line:
                continue

            if not raw_line.startswith("{"):
                print(f"System Message: {raw_line}")
                continue

            try:
                data = json.loads(raw_line)
                data["timestamp"] = datetime.now(timezone.utc).isoformat()
                data["id"] = str(uuid.uuid4())
                print(json.dumps(data, default=str))

                msg_type = data.get("type")
                node = data.get("node")
                subject = data.get("subject")
                status = data.get("status")

                if node:
                    last_heard[node] = time.time()

                path = f"nodes/{node}/{msg_type}/{subject}"

                send_message(data, path)

            except json.JSONDecodeError:
                print(f"Error: Could not parse line: {raw_line}")

    except serial.SerialException as e:
        error_message = build_message(
            type="system_log",
            node="gateway_script",
            subject="serial_exception",
            status="error",
            payload={"port": SERIAL_PORT, "message": str(e), "online": False},
        )
        path = f"nodes/{error_message['node']}/{error_message['type']}/{error_message['subject']}"

        send_message(error_message, path)
        print(f"Alert: Connection on {SERIAL_PORT} was lost!", e)

    except KeyboardInterrupt as e:
        error_message = build_message(
            type="system_log",
            node="gateway_script",
            subject="keyboard_interrupt",
            status="error",
            payload={"message": str(e), "online": False},
        )
        path = f"nodes/{error_message['node']}/{error_message['type']}/{error_message['subject']}"

        send_message(error_message, path)
        print("\n--- Gateway Shutting Down ---")

    finally:
        if ser is not None and ser.is_open:
            ser.close()
            print(f"Released COM Port: {SERIAL_PORT}")

        client.loop_stop()
        client.disconnect()
        print("Disconnected from MQTT Broker")


def check_dead_nodes():
    now = time.time()
    for node, last_message_time in list(last_heard.items()):
        if now - last_message_time > heartbeat_deadline:
            print(f"Node {node} has missed a heartbeat!")
            error_message = build_message(
                type="system_log",
                node=node,
                subject="heartbeat",
                status="error",
                payload={"online": False, "message": "No heartbeat for 90 seconds"},
            )
            path = f"nodes/{error_message['node']}/{error_message['type']}/{error_message['subject']}"
            send_message(error_message, path)

            del last_heard[node]


def send_message(data, path):
    qos = 0
    if data.get("type") in ["alert", "system_log"] or data.get("status") != "ok":
        qos = 1
    data_string = json.dumps(data)
    print("Sending data...")
    client.publish(path, data_string, qos=qos)


if __name__ == "__main__":
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="gateway_bridge")

    last_will_message = build_message(
        type="system_log",
        node="gateway_script",
        subject="last_will",
        status="error",
        payload={
            "message": "The gateway host computer lost power or crashed.",
            "online": False,
        },
    )
    will_payload = json.dumps(last_will_message)
    will_topic = "nodes/gateway_script/system_log/last_will"

    client.will_set(will_topic, payload=will_payload, qos=1, retain=True)
    try:
        client.connect("localhost", 1883)
        client.loop_start()
        print("--- Connected to Mosquitto MQTT Broker ---")
    except Exception as e:
        print(f"Failed to connect to MQTT: {e}")
        exit(1)
    start_gateway()
