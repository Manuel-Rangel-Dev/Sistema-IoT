from flask import Flask, request, jsonify
from collections import deque
from datetime import datetime
import threading
import pytz

app = Flask(__name__)

# Zona horaria Colombia
COL_TZ = pytz.timezone("America/Bogota")

# ─── Almacenamiento en memoria (últimas 500 lecturas) ──────────────────────────
MAX_READINGS = 500
readings = deque(maxlen=MAX_READINGS)
lock = threading.Lock()

# ─── Estado de control (servidor → ESP32) ─────────────────────────────────────
# El ESP32 lee esta configuración en la respuesta de cada POST /data y la aplica.
# El dashboard la modifica con POST /control. El campo "version" se incrementa en
# cada cambio para que el ESP32 sepa cuándo volver a aplicar la configuración.
control_state = {
    "running":    False,     # True = lazo de control activo en el ESP32
    "mode":       "control", # "control" (PID sobre RPM) o "manual" (PWM directo)
    "ctrl_type":  "PID",     # P / PI / PD / PID
    "setpoint":   25.0,      # RPM objetivo
    "kp":         2.0,
    "ki":         0.4,
    "kd":         0.0,
    "pwm_manual": 0,         # PWM 0-255 usado en modo manual
    "version":    0,         # se incrementa con cada cambio
}
control_lock = threading.Lock()

ALLOWED_CTRL = {"P", "PI", "PD", "PID"}
ALLOWED_MODE = {"control", "manual"}


# ─── Endpoint: recibir datos del ESP32 ────────────────────────────────────────
@app.route("/data", methods=["POST"])
def receive_data():
    try:
        payload = request.get_json(force=True)
        if not payload:
            return jsonify({"error": "No JSON received"}), 400

        entry = {
            "timestamp": datetime.now(COL_TZ).isoformat(),
            "rpm":       float(payload.get("rpm",       0)),
            "current_A": float(payload.get("current_A", 0)),
            "voltage_V": float(payload.get("voltage_V", 0)),
            "power_W":   float(payload.get("power_W",   0)),
            "pwm":       int(payload.get("pwm", 0)),
            "status":    "ON",
            # ─── Control (lazo cerrado) ─────────────────────────────
            "setpoint":   float(payload.get("setpoint", 0)),
            "error":      float(payload.get("error", 0)),
            "controller": str(payload.get("controller", "PID")),
            "running":    bool(payload.get("running", False)),
            "ip":         payload.get("ip", ""),
            # ─── GPS (grados decimales; 0 = sin fix) ────────────────
            "lat":     float(payload.get("lat", 0)),
            "lon":     float(payload.get("lon", 0)),
            # ─── Parámetros de red LTE (estilo G-NetTrack) ──────────
            "lte": payload.get("lte", {}),
        }

        with lock:
            readings.append(entry)

        # Devolver la configuración de control vigente para que el ESP la aplique.
        with control_lock:
            cfg = dict(control_state)

        return jsonify({"ok": True, "control": cfg}), 200

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


# ─── Endpoint: configuración de control (dashboard ↔ servidor) ────────────────
@app.route("/control", methods=["GET", "POST"])
def control():
    if request.method == "GET":
        with control_lock:
            return jsonify(control_state), 200

    # POST → el dashboard actualiza la configuración de control
    try:
        payload = request.get_json(force=True) or {}
    except Exception as e:
        return jsonify({"error": str(e)}), 400

    with control_lock:
        if "running" in payload:
            control_state["running"] = bool(payload["running"])
        if "mode" in payload and payload["mode"] in ALLOWED_MODE:
            control_state["mode"] = payload["mode"]
        if "ctrl_type" in payload and payload["ctrl_type"] in ALLOWED_CTRL:
            control_state["ctrl_type"] = payload["ctrl_type"]
        if "setpoint" in payload:
            control_state["setpoint"] = float(payload["setpoint"])
        if "kp" in payload:
            control_state["kp"] = float(payload["kp"])
        if "ki" in payload:
            control_state["ki"] = float(payload["ki"])
        if "kd" in payload:
            control_state["kd"] = float(payload["kd"])
        if "pwm_manual" in payload:
            control_state["pwm_manual"] = max(0, min(255, int(payload["pwm_manual"])))
        control_state["version"] += 1
        cfg = dict(control_state)

    return jsonify({"ok": True, "control": cfg}), 200


# ─── Health check ─────────────────────────────────────────────────────────────
@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "count": len(readings)}), 200


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
