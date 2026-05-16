from flask import Flask, request, jsonify
from collections import deque
from datetime import datetime
import threading

app = Flask(__name__)

# ─── Almacenamiento en memoria (últimas 500 lecturas) ──────────────────────────
MAX_READINGS = 500
readings = deque(maxlen=MAX_READINGS)
lock = threading.Lock()

# ─── Endpoint: recibir datos del ESP32 ────────────────────────────────────────
@app.route("/data", methods=["POST"])
def receive_data():
    try:
        payload = request.get_json(force=True)
        if not payload:
            return jsonify({"error": "No JSON received"}), 400

        entry = {
            "timestamp": datetime.now().isoformat(),
            "rpm":       float(payload.get("rpm",       0)),
            "current_A": float(payload.get("current_A", 0)),
            "voltage_V": float(payload.get("voltage_V", 0)),
            "power_W":   float(payload.get("power_W",   0)),
            "pwm":       int(payload.get("pwm", 0)),
            "status":    "ON",
        }

        with lock:
            readings.append(entry)

        return jsonify({"ok": True}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ─── Endpoint: Streamlit obtiene los datos ────────────────────────────────────
@app.route("/readings", methods=["GET"])
def get_readings():
    n = request.args.get("n", 120, type=int)
    with lock:
        data = list(readings)[-n:]
    return jsonify(data), 200

# ─── Endpoint: último dato (live) ─────────────────────────────────────────────
@app.route("/latest", methods=["GET"])
def get_latest():
    with lock:
        if not readings:
            return jsonify({"status": "OFF"}), 200
        return jsonify(readings[-1]), 200

# ─── Health check ─────────────────────────────────────────────────────────────
@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "count": len(readings)}), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
