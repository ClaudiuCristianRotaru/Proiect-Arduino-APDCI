import cv2
import json
import time
import os
import paho.mqtt.client as mqtt
import firebase_admin
from firebase_admin import credentials, storage, firestore
from pathlib import Path

CAMERA_INDEX = 0 
PHOTO_DIR = "./alert_photos"
TOPIC = "nodes/+/alert/pir" 
FIREBASE_BUCKET = "monitor-f6151.firebasestorage.app"

os.makedirs(PHOTO_DIR, exist_ok=True)

script_dir = Path(__file__).resolve().parent
key_path = (script_dir / ".." / "serviceAccountKey.json").resolve()
cred = credentials.Certificate(str(key_path))
firebase_admin.initialize_app(cred, {'storageBucket': FIREBASE_BUCKET})

db = firestore.client()

def capture_photo(node_id, alert_id):
    print(f"Motion detected on {node_id}! Waking camera...")
    
    capture = cv2.VideoCapture(CAMERA_INDEX)
    time.sleep(1) 
    for _ in range(3):
        capture.read()
        
    return_value, frame = capture.read()
    capture.release()
    
    if return_value:
        filename = f"{node_id}_{alert_id}.jpg"
        filepath = f"{PHOTO_DIR}/{filename}"
        cv2.imwrite(filepath, frame)
        print(f"Saved locally as: {filename}")
        return { "filename": filename, "filepath": filepath}
    else:
        print(f"Failed to create and save photo")
        return None

def save_to_cloud(filename, node_id, alert_id):
    print("Uploading to Firebase Storage...")
    bucket = storage.bucket()
    blob = bucket.blob(f"security_alerts/{node_id}/{filename}")
    blob.upload_from_filename(f"{PHOTO_DIR}/{filename}")
    
    blob.make_public()
    image_url = blob.public_url
    
    doc_ref = db.collection("alerts").document(alert_id)
    doc_ref.update({"payload.photo_url": image_url})
    
    print(f"Sync complete")

def on_message(client, userdata, message):
    try:
        data = json.loads(message.payload.decode("utf-8"))

        msg_id = data.get("id")
        node = data.get("node")
        
        if msg_id and node:
            photo_result = capture_photo(node, msg_id)
            if photo_result:
                save_to_cloud(photo_result.get("filename"), node, msg_id)
                
    except json.JSONDecodeError:
        pass
    except Exception as e:
        print(f"Vision Script Error: {e}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="vision_service")
client.on_message = on_message

print("--- Vision Service Starting ---")
client.connect("localhost", 1883)
client.subscribe(TOPIC, qos=1)
print(f"Listening for triggers on: {TOPIC}")

client.loop_forever()