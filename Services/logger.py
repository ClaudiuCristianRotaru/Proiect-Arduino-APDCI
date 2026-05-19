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
topic = "nodes/+/+/#"

def print_message(data):
    print(f"node: {data.get("node")}")
    print(f"type: {data.get("type")}")
    print(f"status: { "✅" if data.get("status") == "ok" else "❌"}")
    print(f"subject: {data.get("subject")}")
    print(f"timestamp: {data.get("timestamp").strftime("%H:%M:%S")}")
    print(f"payload: {data.get("payload")}")
    print("\n")


def save_to_firestore(collection_name, document_name, data):
    doc_ref = db.collection(collection_name).document(document_name)
    
    doc_ref.set(data)
    print(f"Data synced to Cloud")

def update_node_status(node_name, data):
    doc_ref = db.collection("nodes").document(node_name)
    payload = data.get("payload")
    node_status = {
        "last_seen": data.get("timestamp"),
        "status": data.get("status")
    }
    if "online" in payload:
        node_status["online"] = payload.get("online")

    if "uptime" in payload:
        node_status["uptime"] = payload.get("uptime")

    if "free_ram" in payload:
        node_status["free_ram"] = payload.get("free_ram")

    doc_ref.set(node_status, merge= True)
    print("Updating node state in cloud")


def on_message(client, userdata, message):
    data = json.loads(message.payload.decode("utf-8"))
    if "timestamp" in data:
        data["timestamp"] = datetime.fromisoformat(data["timestamp"])

    id = data.get("id")
    type = data.get("type")
    node = data.get("node")
    subject = data.get("subject")
    status = data.get("status")
    payload = data.get("payload", {})
    timestamp = data.get("timestamp")

    type_to_collection = {
        "telemetry": "telemetry",
        "alert": "alerts",
        "system_log": "system_logs"
    }

    collection_name = type_to_collection.get(type, "unknown")
    document_name = id

    if status != "ok":
        collection_name = "errors"
    elif subject == "heartbeat":
        print("------HEARTBEAT------")
        update_node_status(node, data)
        return
    
    if "online" in payload:
        update_node_status(node, data)
        
    save_to_firestore(collection_name, document_name, data)
    print("saving to", collection_name, "document", document_name, "data", payload)

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="logger_service")
client.on_message = on_message
client.connect("localhost", 1883)
client.subscribe(topic)

print(f"Waiting for messages on topic {topic}...")
client.loop_forever()
