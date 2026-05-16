import serial
import json
import uuid
import paho.mqtt.client as mqtt
from datetime import datetime, timezone

SERIAL_PORT = 'COM6'
BAUD_RATE = 115200

def start_gateway():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"--- Gateway Started on {SERIAL_PORT} ---")
        
        while True:
            if ser.in_waiting > 0:
                raw_line = ser.readline().decode('utf-8').strip()
                
                if not raw_line.startswith('{'):
                    print(f"System Message: {raw_line}")
                    continue

                try:
                    data = json.loads(raw_line)
                    data["timestamp"] = datetime.now(timezone.utc).isoformat();
                    data["id"] = str(uuid.uuid4())
                    print(json.dumps(data, default=str))

                    msg_type = data.get("type")
                    node = data.get("node")
                    subject = data.get("subject")
                    status = data.get("status")
                    payload = data.get("payload", {})

                    if (status == 'error'):
                        print(f"Sending the message to topic: nodes/{node}/system/error" )
                        continue

                    if (msg_type == "telemetry"):
                        handle_telemetry(data)

                    print(f"Sending the message to topic: nodes/{node}/{msg_type}/{subject}" )


                except json.JSONDecodeError:
                    print(f"Error: Could not parse line: {raw_line}")

    except serial.SerialException as e:
        print(f"Connection Error: {e}")
    except KeyboardInterrupt:
        print("\n--- Gateway Shutting Down ---")

def handle_alert(sensor, payload):
    state = "ACTIVE" if payload.get("active") else "CLEARED"
    print(f"ALERT [{sensor.upper()}]: System is {state}!")

def handle_telemetry(data):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="Test_Publisher")

    client.connect("localhost", 1883)

    data_string = json.dumps(data);

    print("Sending test data...")
    client.publish(f"nodes/{data.get("node")}/{data.get("type")}/{data.get("subject")}", data_string)

    client.disconnect()
    print("Done.")

if __name__ == "__main__":
    start_gateway()