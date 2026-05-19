import serial
import json
import uuid
import paho.mqtt.client as mqtt
from datetime import datetime, timezone

SERIAL_PORT = 'COM6'
BAUD_RATE = 115200
node_registry = {
    "COM6":"CA_01"
}

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="gateway_bridge")
try:
    client.connect("localhost", 1883)
    client.loop_start()
    print("--- Connected to Mosquitto MQTT Broker ---")
except Exception as e:
    print(f"Failed to connect to MQTT: {e}")
    exit(1)

def start_gateway():
    ser = None
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"--- Gateway Started on {SERIAL_PORT} ---")
        
        while True:
            raw_line = ser.readline().decode('utf-8', errors='ignore').strip()

            if not raw_line:
                continue
            
            if not raw_line.startswith('{'):
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

                path = f"nodes/{node}/{msg_type}/{subject}"

                send_message(data, path)

            except json.JSONDecodeError:
                print(f"Error: Could not parse line: {raw_line}")

    except serial.SerialException as e:
        node_name = node_registry.get(SERIAL_PORT, "unknown_node")
        error_message = {
        "type": "system_log", 
        "id": str(uuid.uuid4()), 
        "timestamp": datetime.now(timezone.utc).isoformat(), 
        "node": node_name, 
        "subject": SERIAL_PORT, 
        "status": "error",
        "payload": {
            "message": str(e),
            "online": False
        }}
        path = f"nodes/{node_name}/{error_message['type']}/{error_message['subject']}"
    
        send_message(error_message, path)
        print(f"Alert: Connection to {node_name} on {SERIAL_PORT} was lost!", e)

    except KeyboardInterrupt:
        node_name = node_registry.get(SERIAL_PORT, "unknown_node")
        error_message = {
        "type": "system_log", 
        "id": str(uuid.uuid4()), 
        "timestamp": datetime.now(timezone.utc).isoformat(), 
        "node": node_name, 
        "subject": "gateway_script", 
        "status": "error",
        "payload": {
            "message": "keyboard_interrupt",
            "online": False
        }}
        path = f"nodes/{node_name}/{error_message['type']}/{error_message['subject']}"
    
        send_message(error_message, path)
        print("\n--- Gateway Shutting Down ---")
    
    finally:
        if ser is not None and ser.is_open:
            ser.close()
            print(f"Released COM Port: {SERIAL_PORT}")
        
        client.loop_stop()
        client.disconnect()
        print("Disconnected from MQTT Broker")

def send_message(data, path):
    data_string = json.dumps(data)
    print("Sending data...")
    client.publish(path, data_string)

if __name__ == "__main__":
    start_gateway()