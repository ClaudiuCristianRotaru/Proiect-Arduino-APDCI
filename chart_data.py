import firebase_admin
from firebase_admin import credentials, firestore
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timezone, timedelta
from pathlib import Path

# --- 1. FIREBASE SETUP ---
script_dir = Path(__file__).resolve().parent
key_path = (script_dir / "serviceAccountKey.json").resolve()

if not firebase_admin._apps:
    cred = credentials.Certificate(str(key_path))
    firebase_admin.initialize_app(cred)

db = firestore.client()

def plot_telemetry(node_id, subject):
    print(f"📡 Fetching latest {subject} data for {node_id}...")

    # --- 2. THE TIME FILTER ---
    start_date = datetime(2026, 5, 19, tzinfo=timezone.utc)

    try:
        docs = db.collection("telemetry") \
                 .where("node", "==", node_id) \
                 .where("subject", "==", subject) \
                 .where("timestamp", ">=", start_date) \
                 .order_by("timestamp", direction=firestore.Query.DESCENDING) \
                 .limit(100) \
                 .stream()
    except Exception as e:
        print(f"Query failed. Error: {e}")
        return

    raw_timestamps = []
    raw_values = []
    
    # NEW: Separate lists to track exactly where the errors occurred
    error_timestamps = []
    error_values = []

    # --- 3. EXTRACT THE DATA ---
    for doc in docs:
        data = doc.to_dict()
        
        ts = data.get("timestamp")
        status = data.get("status", "ok") # Default to "ok" if missing
        payload = data.get("payload", {})
        val = payload.get("temperatureC")

        if ts is not None and val is not None:
            raw_timestamps.append(ts)
            raw_values.append(val)
            
            # If this specific reading was flagged as an error, save its coordinates
            if status == "error":
                error_timestamps.append(ts)
                error_values.append(val)

    if not raw_timestamps:
        print("❌ No data found matching this query.")
        return

    # Reverse to chronological order (left to right)
    raw_timestamps.reverse()
    raw_values.reverse()

    # --- 4. THE GAP DETECTOR (60 Seconds) ---
    plot_ts = []
    plot_vals = []

    for i in range(len(raw_timestamps)):
        plot_ts.append(raw_timestamps[i])
        plot_vals.append(raw_values[i])
        
        if i < len(raw_timestamps) - 1:
            time_diff = raw_timestamps[i+1] - raw_timestamps[i]
            if time_diff.total_seconds() > 60:
                plot_ts.append(raw_timestamps[i] + timedelta(seconds=1))
                plot_vals.append(None) 

    # --- 5. DRAW THE GRAPH ---
    print(f"📊 Plotting data with {plot_vals.count(None)} connection breaks...")
    if error_timestamps:
        print(f"⚠️ Found {len(error_timestamps)} error readings. Highlighting them...")

    plt.figure(figsize=(12, 6)) 
    
    # 1. Plot the standard baseline graph (Green)
    # Added a 'label' so it shows up in the legend
    plt.plot(plot_ts, plot_vals, marker='o', markersize=1, linestyle='-', linewidth=1, color='#2ca02c', alpha=0.8, label="Normal Reading") 

    # 2. OVERLAY THE ERRORS (Red 'X')
    # zorder=5 ensures the red X is painted ON TOP of the green line, not under it.
    if error_timestamps:
        plt.scatter(error_timestamps, error_values, color='#d62728', marker='X', s=80, zorder=5, label="System Error")

    clean_subject = subject.replace('_', ' ').title()
    plt.title(f"{clean_subject} Levels over Time ({node_id})")
    plt.xlabel("Time")
    plt.ylabel("Temperature (°C)")
    
    plt.ylim(15, 40) 
    plt.grid(True, linestyle='--', alpha=0.7)
    
    # Add the legend to the top left corner (or let matplotlib pick the 'best' spot)
    plt.legend(loc="best")

    # Accurate X-Axis formatting
    ax = plt.gca()
    locator = mdates.AutoDateLocator(minticks=5, maxticks=15)
    formatter = mdates.DateFormatter("%H:%M:%S \n%b %d") 
    
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(formatter)
    plt.gcf().autofmt_xdate()

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    plot_telemetry(node_id="CA_01", subject="climate")