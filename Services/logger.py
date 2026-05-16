import firebase_admin
import json
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
from firebase_admin import credentials, firestore
from pathlib import Path

script_dir = Path(__file__).resolve().parent
key_path = (script_dir / ".." / "serviceAccountKey.json").resolve()
cred = credentials.Certificate(str(key_path))

firebase_admin.initialize_app(cred)

db = firestore.client()
topic = "nodes/+/telemetry/#"

def save_to_firestore(node_id, data):
    doc_ref = db.collection("telemetry").document(data['id'])
    
    doc_ref.set(data)
    print(f"Data synced to Cloud for node {node_id}")


def on_message(client, userdata, message):
    print(f"Received: {message.payload.decode()} on {message.topic}")

    raw_data = json.loads(message.payload.decode("utf-8"))
    if "timestamp" in raw_data:
        raw_data["timestamp"] = datetime.fromisoformat(raw_data["timestamp"])

    node = raw_data["node"] if "node" in raw_data else "Unknown node"

    save_to_firestore(node, raw_data)

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="Test_Subscriber")

client.on_message = on_message

client.connect("localhost", 1883)
client.subscribe(topic)

print(f"Waiting for messages on topic {topic}...")
client.loop_forever()
