import streamlit as st
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from datetime import datetime
import requests
import pytz

# Zona horaria Colombia
COL_TZ = pytz.timezone("America/Bogota")

# ─── PAGE CONFIG ───────────────────────────────────────────────────────────────
st.set_page_config(
    page_title="Motor DC · Monitor & Control",
    page_icon="⚡",
    layout="wide",
    initial_sidebar_state="expanded",
)

# ─── CONFIGURACIÓN ─────────────────────────────────────────────────────────────
# NOTA: debe ser la IP elástica de la instancia EC2 (misma que SERVER_HOST en el
# firmware). Si la cambias aquí, cámbiala también en el .ino.
SERVER_URL = "http://18.226.103.103:5000"
REFRESH_SEC = 1

# ─── CONSTANTES DE CONTROL (replicadas del proyecto de teoría de control) ──────
CONTROLLER_TYPES  = ["P", "PI", "PD", "PID"]
CONTROLLER_COLORS = {
    "P":   "#FF6B6B",   # Rojo coral
    "PI":  "#4DA6FF",   # Azul cielo
    "PD":  "#51CF66",   # Verde menta
    "PID": "#FF922B",   # Naranja vivo
}
MIN_SETPOINT_RPM = 11.0
MAX_SETPOINT_RPM = 35.0
DEFAULT_SETPOINT = 25.0
DEFAULT_KP, DEFAULT_KI, DEFAULT_KD = 2.0, 0.4, 0.0
RPM_Y_MAX = 40
COLOR_SETPOINT = "#8b949e"

# ─── BANDAS LTE (Claro Colombia) — frecuencia desde EARFCN (3GPP) ──────────────
# F_DL = F_DL_low + 0.1*(EARFCN - offset);  F_UL = F_DL - duplex
LTE_BANDS = {
    2:  {"f_dl_low": 1930.0, "offset": 600,  "duplex": 80.0},
    4:  {"f_dl_low": 2110.0, "offset": 1950, "duplex": 400.0},
    7:  {"f_dl_low": 2620.0, "offset": 2750, "duplex": 120.0},
    28: {"f_dl_low": 758.0,  "offset": 9210, "duplex": 55.0},
}
# Operadores por MCC-MNC (Colombia)
OPERATORS = {
    (732, 101): "Claro Colombia", (732, 102): "Claro Colombia",
    (732, 103): "Tigo Colombia",  (732, 111): "Tigo Colombia",
    (732, 123): "Movistar Colombia", (732, 130): "Avantel",
}

def _band_number(band_str):
    """'EUTRAN-BAND28' → 28; None si no hay dígitos."""
    digits = "".join(c for c in str(band_str or "") if c.isdigit())
    return int(digits) if digits else None

def lte_frequencies(band_str, earfcn):
    """(F_DL, F_UL) en MHz desde banda + EARFCN(DL). (None, None) si no se puede."""
    b = _band_number(band_str)
    try:
        earfcn = int(earfcn)
    except (TypeError, ValueError):
        return None, None
    if b not in LTE_BANDS or earfcn <= 0:
        return None, None
    p = LTE_BANDS[b]
    f_dl = p["f_dl_low"] + 0.1 * (earfcn - p["offset"])
    return f_dl, f_dl - p["duplex"]

def operator_name(mcc, mnc, fallback=""):
    try:
        key = (int(mcc), int(mnc))
    except (TypeError, ValueError):
        return fallback or "—"
    return OPERATORS.get(key, fallback or (f"{mcc}-{mnc}" if mcc and mnc else "—"))

def network_type(lte):
    if "EUTRAN" in str(lte.get("band", "")).upper() or "LTE" in str(lte.get("tech", "")).upper():
        return "4G LTE"
    return lte.get("tech") or "—"

# ─── PALETTE  #FFAF45 · #FB6D48 · #D74B76 · #673F69 ──────────────────────────
CSS = """
<style>
@import url('https://fonts.googleapis.com/css2?family=Rajdhani:wght@400;500;600;700&family=Nunito:wght@300;400;600;700&display=swap');
:root {
  --col1:#FFAF45;--col2:#FB6D48;--col3:#D74B76;--col4:#673F69;
  --bg:#fdf6ee;--bg-card:#ffffff;--bg-sidebar:#fff8f0;
  --border:#f0e4d4;--text:#1a1010;--text-mid:#3a2020;--text-light:#5a3a3a;
  --online:#2ecc71;--offline:#e74c3c;--shadow:0 2px 12px rgba(103,63,105,0.08);
}
html,body,.stApp{background-color:var(--bg)!important;font-family:'Nunito',sans-serif;color:var(--text);}
#MainMenu,footer,header{visibility:hidden;}
.block-container{padding:1.4rem 2rem 2rem 2rem!important;max-width:100%!important;}
[data-testid="stSidebar"]{background-color:var(--bg-sidebar)!important;border-right:1px solid var(--border)!important;}
[data-testid="stSidebar"] .block-container{padding:1.4rem 1rem!important;}
.sb-brand{font-family:'Rajdhani',sans-serif;font-size:1.25rem;font-weight:700;letter-spacing:0.06em;color:var(--col4);padding-bottom:1rem;border-bottom:2px solid var(--border);margin-bottom:1.2rem;}
.sb-brand span{color:var(--col2);}
.sb-section{font-family:'Rajdhani',sans-serif;font-size:0.72rem;font-weight:700;text-transform:uppercase;letter-spacing:0.14em;color:var(--col4);margin:1.1rem 0 0.45rem 0;}
[data-testid="stSidebar"] .stCheckbox label{font-size:0.88rem!important;color:#1a1010!important;font-family:'Nunito',sans-serif!important;font-weight:700!important;}
[data-testid="stSidebar"] .stCheckbox label p{color:#1a1010!important;font-weight:700!important;}
[data-testid="stSidebar"] .stRadio label{color:#1a1010!important;font-weight:700!important;}
[data-testid="stSidebar"] .stRadio div[role="radiogroup"] label p{color:#1a1010!important;font-weight:600!important;font-family:'Rajdhani',sans-serif!important;letter-spacing:0.04em;}
[data-testid="stSidebar"] .stButton button{background:var(--col4)!important;color:#fff!important;border:none!important;border-radius:8px!important;font-family:'Rajdhani',sans-serif!important;font-weight:700!important;letter-spacing:0.08em!important;text-transform:uppercase!important;font-size:0.82rem!important;transition:all .2s!important;}
[data-testid="stSidebar"] .stButton:first-of-type button{background:linear-gradient(135deg,var(--col1),var(--col2))!important;box-shadow:0 2px 8px rgba(251,109,72,0.25)!important;}
[data-testid="stSidebar"] .stButton:nth-of-type(2) button{background:linear-gradient(135deg,var(--col3),var(--col4))!important;box-shadow:0 2px 8px rgba(103,63,105,0.25)!important;}
[data-testid="stSidebar"] .stSelectbox label{color:#1a1010!important;font-weight:700!important;}
[data-testid="stSidebar"] .stSelectbox div[data-baseweb="select"] > div{background:#fff!important;color:#1a1010!important;border:1px solid var(--border)!important;}
[data-testid="stSidebar"] .stSelectbox span{color:#1a1010!important;}
[data-testid="stSidebar"] .stSelectbox svg{color:#1a1010!important;fill:#1a1010!important;}
.dash-header{display:flex;align-items:center;justify-content:space-between;padding:0.85rem 1.5rem;background:var(--bg-card);border:1px solid var(--border);border-radius:14px;box-shadow:var(--shadow);margin-bottom:1.4rem;}
.dash-title{font-family:'Rajdhani',sans-serif;font-size:1.35rem;font-weight:700;color:var(--col4);letter-spacing:0.05em;}
.dash-title span{color:var(--col2);}
.dash-sub{font-size:0.72rem;color:var(--text-light);margin-top:1px;}
.live-pill{display:flex;align-items:center;gap:6px;font-family:'Rajdhani',sans-serif;font-size:0.78rem;font-weight:600;color:var(--online);letter-spacing:0.08em;background:rgba(46,204,113,0.09);border:1px solid rgba(46,204,113,0.25);padding:5px 12px;border-radius:20px;}
.live-dot{width:7px;height:7px;border-radius:50%;background:var(--online);box-shadow:0 0 6px var(--online);animation:blink 1.5s infinite;}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0.25}}
.sec-heading{font-family:'Rajdhani',sans-serif;font-size:0.8rem;font-weight:700;text-transform:uppercase;letter-spacing:0.14em;color:var(--col4);margin:0.2rem 0 0.7rem 0;display:flex;align-items:center;gap:8px;}
.sec-heading::after{content:'';flex:1;height:1px;background:var(--border);}
.status-panel{background:var(--bg-card);border:1px solid var(--border);border-radius:12px;padding:1rem 1.25rem;box-shadow:var(--shadow);display:flex;align-items:center;justify-content:space-between;margin-bottom:1.1rem;}
.status-label{font-family:'Rajdhani',sans-serif;font-size:0.72rem;font-weight:700;text-transform:uppercase;letter-spacing:0.12em;color:#5a3a3a;}
.status-val{font-family:'Rajdhani',sans-serif;font-size:1.6rem;font-weight:700;letter-spacing:0.05em;}
.status-val.on{color:var(--online);}.status-val.off{color:var(--offline);}
.sdot{width:13px;height:13px;border-radius:50%;display:inline-block;margin-right:7px;vertical-align:middle;}
.sdot-on{background:var(--online);box-shadow:0 0 10px var(--online);}
.sdot-off{background:var(--offline);box-shadow:0 0 10px var(--offline);}
.spark-label{font-family:'Rajdhani',sans-serif;font-size:1rem;font-weight:700;letter-spacing:0.08em;color:var(--text);padding-top:10px;}
.spark-info{display:flex;flex-direction:column;align-items:flex-end;gap:3px;padding-top:8px;}
.spark-value{font-family:'Rajdhani',sans-serif;font-size:1.05rem;font-weight:700;color:var(--text);letter-spacing:0.03em;white-space:nowrap;}
.spark-badge{font-family:'Rajdhani',sans-serif;font-size:0.75rem;font-weight:700;letter-spacing:0.04em;padding:2px 8px;border-radius:5px;text-align:center;display:inline-block;white-space:nowrap;}
.badge-pos{background:var(--col1);color:#2b1e1e;}.badge-neg{background:var(--col3);color:#fff;}
.chart-wrap{background:var(--bg-card);border:1px solid var(--border);border-radius:12px;padding:1rem 1.2rem;box-shadow:var(--shadow);}
.chart-wrap-title{font-family:'Rajdhani',sans-serif;font-size:0.78rem;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;color:#3a2020;margin-bottom:0.3rem;}
.stDataFrame{background:#ffffff!important;border-radius:10px!important;}
.stDataFrame thead th{background:#fdf0e4!important;font-family:'Rajdhani',sans-serif!important;font-size:0.7rem!important;font-weight:700!important;text-transform:uppercase;letter-spacing:0.1em;color:#1a1010!important;}
.stDataFrame tbody tr{background:#ffffff!important;}
.stDataFrame tbody tr:nth-child(even){background:#fdf6ee!important;}
.stDataFrame tbody td{font-family:'Nunito',sans-serif!important;font-size:0.82rem!important;color:#1a1010!important;background:transparent!important;}
.stDownloadButton button{background:#fff!important;color:#1a1010!important;border:1px solid var(--border)!important;border-radius:8px!important;font-family:'Rajdhani',sans-serif!important;font-weight:700!important;font-size:0.9rem!important;}
.stDownloadButton button:hover{background:#fdf0e4!important;}
.footer-txt{font-family:'Rajdhani',sans-serif;font-size:0.68rem;letter-spacing:0.08em;color:#5a3a3a;text-align:right;margin-top:1rem;}
.error-box{background:#fff0f0;border:1px solid #f0d0d0;border-radius:10px;padding:0.8rem 1.2rem;color:#c0392b;font-size:0.82rem;margin-bottom:1rem;}
.vars-activas{font-family:'Rajdhani',sans-serif;font-size:0.75rem;letter-spacing:0.08em;color:#5a3a3a;text-align:center;font-weight:600;}
/* ── Tarjetas de control ── */
.ctrl-pill{display:inline-block;font-family:'Rajdhani',sans-serif;font-weight:700;font-size:0.8rem;letter-spacing:0.06em;padding:4px 12px;border-radius:6px;color:#fff;}
.ctrl-legend{display:flex;gap:14px;flex-wrap:wrap;margin:0.4rem 0 0.6rem 0;}
.ctrl-legend-item{display:flex;align-items:center;gap:6px;font-family:'Rajdhani',sans-serif;font-weight:700;font-size:0.8rem;color:#3a2020;}
.ctrl-legend-dot{width:14px;height:14px;border-radius:3px;display:inline-block;}
/* ── Tarjetas de red LTE (estilo G-NetTrack) ── */
.lte-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;margin-bottom:1rem;}
.lte-card{background:var(--bg-card);border:1px solid var(--border);border-radius:10px;padding:0.7rem 0.9rem;box-shadow:var(--shadow);}
.lte-k{font-family:'Rajdhani',sans-serif;font-size:0.68rem;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;color:#5a3a3a;}
.lte-v{font-family:'Rajdhani',sans-serif;font-size:1.35rem;font-weight:700;color:var(--col4);line-height:1.2;}
.lte-u{font-family:'Nunito',sans-serif;font-size:0.66rem;color:#8a6a6a;}
.lte-banner{background:linear-gradient(135deg,var(--col4),var(--col3));color:#fff;border-radius:12px;padding:0.9rem 1.4rem;margin-bottom:1rem;display:flex;align-items:center;justify-content:space-between;}
.lte-banner-op{font-family:'Rajdhani',sans-serif;font-size:1.5rem;font-weight:700;letter-spacing:0.04em;}
.lte-banner-tech{font-family:'Rajdhani',sans-serif;font-size:0.9rem;font-weight:600;background:rgba(255,255,255,0.2);padding:4px 14px;border-radius:20px;}
</style>
"""
st.markdown(CSS, unsafe_allow_html=True)

# ─── FUNCIONES DE DATOS ────────────────────────────────────────────────────────
@st.cache_data(ttl=REFRESH_SEC)
def fetch_readings(n=120):
    try:
        r = requests.get(f"{SERVER_URL}/readings", params={"n": n}, timeout=3)
        if r.status_code == 200:
            return pd.DataFrame(r.json()), None
        return pd.DataFrame(), f"Error del servidor: {r.status_code}"
    except Exception as e:
        return pd.DataFrame(), str(e)

@st.cache_data(ttl=REFRESH_SEC)
def fetch_latest():
    try:
        r = requests.get(f"{SERVER_URL}/latest", timeout=3)
        if r.status_code == 200:
            return r.json(), None
        return {}, f"Error: {r.status_code}"
    except Exception as e:
        return {"status": "OFF"}, str(e)

@st.cache_data(ttl=REFRESH_SEC)
def fetch_control():
    try:
        r = requests.get(f"{SERVER_URL}/control", timeout=3)
        if r.status_code == 200:
            return r.json(), None
        return {}, f"Error: {r.status_code}"
    except Exception as e:
        return {}, str(e)

def push_control(payload):
    """Envía la configuración de control al servidor (servidor → ESP32)."""
    try:
        r = requests.post(f"{SERVER_URL}/control", json=payload, timeout=3)
        fetch_control.clear()  # invalida la cache para reflejar el cambio
        return r.status_code == 200, None
    except Exception as e:
        return False, str(e)

def _rgb(h):
    h = h.lstrip("#")
    return ",".join(str(int(h[i:i+2],16)) for i in (0,2,4))

def pct_delta(now, prev):
    if prev == 0: return 0.0
    return round((now - prev) / abs(prev) * 100, 2)

def spark_fig(series, color, height=54):
    fig = go.Figure(go.Scatter(
        y=series, mode="lines",
        line=dict(color=color, width=2),
        fill="tozeroy", fillcolor=f"rgba({_rgb(color)},0.13)",
        hoverinfo="skip",
    ))
    fig.update_layout(
        margin=dict(l=0,r=0,t=4,b=4), height=height,
        paper_bgcolor="#ffffff", plot_bgcolor="#ffffff",
        xaxis=dict(visible=False), yaxis=dict(visible=False), showlegend=False,
    )
    return fig

def full_fig(df, col, color, unit, height=220):
    fig = go.Figure(go.Scatter(
        x=pd.to_datetime(df["timestamp"]), y=df[col], mode="lines",
        line=dict(color=color, width=2.5),
        fill="tozeroy", fillcolor=f"rgba({_rgb(color)},0.10)",
        hovertemplate=f"%{{y:.2f}} {unit}<extra></extra>",
    ))
    fig.update_layout(
        margin=dict(l=6,r=6,t=6,b=28), height=height,
        paper_bgcolor="#ffffff", plot_bgcolor="#fdf6ee",
        font=dict(family="Nunito", color="#1a1010", size=11),
        xaxis=dict(showgrid=True, gridcolor="rgba(120,80,80,0.2)", zeroline=False,
                   tickfont=dict(size=10, color="#1a1010"), tickformat="%H:%M:%S",
                   linecolor="rgba(120,80,80,0.4)", linewidth=1, showline=True),
        yaxis=dict(showgrid=True, gridcolor="rgba(120,80,80,0.2)", zeroline=False,
                   tickfont=dict(size=10, color="#1a1010"),
                   linecolor="rgba(120,80,80,0.4)", linewidth=1, showline=True),
        hovermode="x unified",
        hoverlabel=dict(bgcolor="#ffffff", bordercolor="#f0e4d4",
                        font=dict(family="Nunito", size=11, color="#1a1010")),
    )
    return fig

def _controller_segments(df):
    """Divide el DataFrame en tramos consecutivos de mismo controlador.

    Cada cambio de controlador abre un tramo nuevo; los tramos comparten el
    punto frontera para que la línea sea continua (igual que en el proyecto de
    control original). Devuelve lista de (sub_df, controlador).
    """
    segs = []
    if df.empty or "controller" not in df.columns:
        return segs
    ctrl = df["controller"].fillna("PID").tolist()
    start = 0
    for i in range(1, len(df)):
        if ctrl[i] != ctrl[i - 1]:
            segs.append((df.iloc[start:i + 1], ctrl[start]))  # overlap de 1 punto
            start = i
    segs.append((df.iloc[start:], ctrl[start]))
    return segs

def control_chart(df, height=340):
    """Gráfica RPM real (segmentos de color por controlador) + setpoint."""
    x = pd.to_datetime(df["timestamp"])
    fig = go.Figure()

    # Setpoint (línea discontinua gris, comportamiento de escalón)
    fig.add_trace(go.Scatter(
        x=x, y=df["setpoint"], mode="lines", name="Setpoint",
        line=dict(color=COLOR_SETPOINT, width=2, dash="dash"),
        line_shape="hv", hovertemplate="SP %{y:.1f} rpm<extra></extra>",
    ))

    # Velocidad real por segmentos de controlador
    seen = set()
    for sub, ctrl in _controller_segments(df):
        color = CONTROLLER_COLORS.get(ctrl, "#673F69")
        show = ctrl not in seen
        seen.add(ctrl)
        fig.add_trace(go.Scatter(
            x=pd.to_datetime(sub["timestamp"]), y=sub["rpm"], mode="lines",
            name=f"RPM · {ctrl}", legendgroup=ctrl, showlegend=show,
            line=dict(color=color, width=2.6),
            hovertemplate=f"{ctrl}: %{{y:.2f}} rpm<extra></extra>",
        ))

    fig.update_layout(
        margin=dict(l=6, r=6, t=6, b=28), height=height,
        paper_bgcolor="#ffffff", plot_bgcolor="#fdf6ee",
        font=dict(family="Nunito", color="#1a1010", size=11),
        xaxis=dict(showgrid=True, gridcolor="rgba(120,80,80,0.2)", zeroline=False,
                   tickfont=dict(size=10, color="#1a1010"), tickformat="%H:%M:%S",
                   linecolor="rgba(120,80,80,0.4)", showline=True),
        yaxis=dict(range=[0, RPM_Y_MAX], showgrid=True, gridcolor="rgba(120,80,80,0.2)",
                   zeroline=False, tickfont=dict(size=10, color="#1a1010"),
                   linecolor="rgba(120,80,80,0.4)", showline=True, title="RPM"),
        legend=dict(orientation="h", yanchor="bottom", y=1.0, xanchor="right", x=1.0,
                    font=dict(size=10, color="#3a2020")),
        hovermode="x unified",
    )
    return fig

def error_chart(df, height=200):
    """Gráfica de error (setpoint - rpm) coloreada por controlador."""
    fig = go.Figure()
    fig.add_hline(y=0, line=dict(color="rgba(120,80,80,0.5)", width=1, dash="dash"))
    for sub, ctrl in _controller_segments(df):
        color = CONTROLLER_COLORS.get(ctrl, "#673F69")
        fig.add_trace(go.Scatter(
            x=pd.to_datetime(sub["timestamp"]), y=sub["error"], mode="lines",
            line=dict(color=color, width=2.0), showlegend=False,
            hovertemplate=f"{ctrl}: %{{y:.2f}} rpm<extra></extra>",
        ))
    fig.update_layout(
        margin=dict(l=6, r=6, t=6, b=28), height=height,
        paper_bgcolor="#ffffff", plot_bgcolor="#fdf6ee",
        font=dict(family="Nunito", color="#1a1010", size=11),
        xaxis=dict(showgrid=True, gridcolor="rgba(120,80,80,0.2)", zeroline=False,
                   tickfont=dict(size=10, color="#1a1010"), tickformat="%H:%M:%S",
                   linecolor="rgba(120,80,80,0.4)", showline=True),
        yaxis=dict(showgrid=True, gridcolor="rgba(120,80,80,0.2)", zeroline=False,
                   tickfont=dict(size=10, color="#1a1010"),
                   linecolor="rgba(120,80,80,0.4)", showline=True, title="Error"),
        hovermode="x unified",
    )
    return fig

# ─── SESSION STATE ─────────────────────────────────────────────────────────────
if "auto_refresh" not in st.session_state:
    st.session_state.auto_refresh = True

# ─── SIDEBAR ───────────────────────────────────────────────────────────────────
with st.sidebar:
    st.markdown('<div class="sb-brand">⚡ MOTOR<span>DC</span></div>', unsafe_allow_html=True)

    st.markdown('<div class="sb-section">Vista</div>', unsafe_allow_html=True)
    view = st.radio("nav", ["📊 Monitoreo", "🎛️ Control", "📡 Red LTE"],
                    label_visibility="collapsed", key="nav")

    st.markdown('<div class="sb-section">Variables monitoreo</div>', unsafe_allow_html=True)
    show_rpm   = st.checkbox("⚡ Velocidad (RPM)",        value=True)
    show_curr  = st.checkbox("🔌 Corriente · INA219 (A)", value=True)
    show_volt  = st.checkbox("🔋 Voltaje (V)",            value=True)
    show_power = st.checkbox("⚡ Potencia (W)",            value=True)
    show_gps   = st.checkbox("📍 Ubicación GPS",          value=True)

    st.markdown('<div class="sb-section">Historial</div>', unsafe_allow_html=True)
    hist_window = st.selectbox(
        "Ventana", [30, 60, 120, 240, 480], index=2,
        format_func=lambda x: f"Últimas {x} lecturas",
    )

    st.markdown('<div class="sb-section">Actualización</div>', unsafe_allow_html=True)
    auto_refresh = st.checkbox("Auto-refresh", value=st.session_state.auto_refresh)
    st.session_state.auto_refresh = auto_refresh

    st.markdown("---")
    an = sum([show_rpm, show_curr, show_volt, show_power, show_gps])
    st.markdown(f'<div class="vars-activas">{an} / 5 variables activas</div>',
                unsafe_allow_html=True)

# ─── OBTENER DATOS REALES ──────────────────────────────────────────────────────
df, err_hist   = fetch_readings(hist_window)
live, err_live = fetch_latest()
ctrl_cfg, _    = fetch_control()

connected = (not err_live) and live.get("status") == "ON"

VAR = {
    "rpm":   {"label":"RPM",      "col":"rpm",       "color":"#FB6D48","unit":"rpm", "lk":"rpm"},
    "curr":  {"label":"CURR",     "col":"current_A", "color":"#D74B76","unit":"A",   "lk":"current_A"},
    "volt":  {"label":"VOLT",     "col":"voltage_V", "color":"#673F69","unit":"V",   "lk":"voltage_V"},
    "power": {"label":"POTENCIA", "col":"power_W",   "color":"#FFAF45","unit":"W",   "lk":"power_W"},
}

# ─── HEADER ────────────────────────────────────────────────────────────────────
pill_html = (
    '<div class="live-pill"><div class="live-dot"></div>LIVE</div>'
    if connected else
    '<div class="live-pill" style="color:var(--offline);border-color:rgba(231,76,60,0.3);background:rgba(231,76,60,0.07);">SIN SEÑAL</div>'
)
st.markdown(f"""
<div class="dash-header">
  <div>
    <div class="dash-title">⚡ MOTOR<span>DC</span> · MONITOR & CONTROL</div>
    <div class="dash-sub">ESP32-S3 + INA219 + Hall Encoder · SIM7670G LTE · Claro CO</div>
  </div>
  <div style="display:flex;align-items:center;gap:14px;">
    <span style="font-family:Rajdhani,sans-serif;font-size:0.72rem;letter-spacing:0.07em;color:#5a3a3a;">
      {datetime.now(COL_TZ).strftime('%d %b %Y · %H:%M:%S')}
    </span>
    {pill_html}
  </div>
</div>
""", unsafe_allow_html=True)

if err_live:
    st.markdown(
        f'<div class="error-box">⚠️ No se puede conectar al servidor: <code>{SERVER_URL}</code> — {err_live}</div>',
        unsafe_allow_html=True)


# ════════════════════════════════════════════════════════════════════════════════
#  VISTA: MONITOREO
# ════════════════════════════════════════════════════════════════════════════════
def render_monitoreo():
    on = connected
    pwm_val = live.get("pwm", "--")
    st.markdown(f"""
    <div class="status-panel">
      <div>
        <div class="status-label">Estado del Motor</div>
        <div class="status-val {'on' if on else 'off'}">
          <span class="sdot {'sdot-on' if on else 'sdot-off'}"></span>{'ON' if on else 'OFF'}
        </div>
      </div>
      <div style="text-align:center;">
        <div class="status-label">PWM Actual</div>
        <div style="font-family:Rajdhani,sans-serif;font-size:1.6rem;font-weight:700;color:var(--col2);">{pwm_val}</div>
        <div style="font-family:Nunito,sans-serif;font-size:0.68rem;color:#5a3a3a;">/ 255</div>
      </div>
      <div style="text-align:right;">
        <div class="status-label">Dispositivo</div>
        <div style="font-family:Rajdhani,sans-serif;font-size:0.95rem;font-weight:600;color:#673F69;letter-spacing:0.04em;">ESP32 · SIM7670G</div>
        <div style="font-family:Nunito,sans-serif;font-size:0.7rem;color:#5a3a3a;">Barranquilla, CO</div>
      </div>
    </div>
    """, unsafe_allow_html=True)

    # ── SPARKLINES ──
    active = [k for k, show in [("rpm",show_rpm),("curr",show_curr),("volt",show_volt),("power",show_power)] if show]
    if active and not df.empty:
        st.markdown('<div class="sec-heading">Estado actual</div>', unsafe_allow_html=True)
        lcol, rcol = st.columns(2)
        for keys, container in [(active[:2], lcol), (active[2:], rcol)]:
            with container:
                for k in keys:
                    m       = VAR[k]
                    series  = df[m["col"]].values if m["col"] in df.columns else []
                    val_now = live.get(m["lk"], 0) if live else 0
                    val_old = float(df.iloc[-5][m["col"]]) if (len(df) > 5 and m["col"] in df.columns) else val_now
                    delta   = pct_delta(val_now, val_old)
                    bcls    = "badge-pos" if delta >= 0 else "badge-neg"
                    sign    = "+" if delta >= 0 else ""
                    c1, c2, c3 = st.columns([1,3,1])
                    with c1:
                        st.markdown(f'<div class="spark-label">{m["label"]}</div>', unsafe_allow_html=True)
                    with c2:
                        if len(series) > 0:
                            st.plotly_chart(spark_fig(series, m["color"]), use_container_width=True,
                                            config={"displayModeBar":False}, key=f"sp_{k}")
                    with c3:
                        if m["unit"] == "rpm":  val_str = f"{val_now:.0f} {m['unit']}"
                        elif m["unit"] == "A":  val_str = f"{val_now:.3f} {m['unit']}"
                        else:                   val_str = f"{val_now:.2f} {m['unit']}"
                        st.markdown(f'<div class="spark-info"><span class="spark-value">{val_str}</span>'
                                    f'<span class="spark-badge {bcls}">{sign}{delta}%</span></div>',
                                    unsafe_allow_html=True)
    elif active and df.empty:
        st.info("⏳ Esperando datos del ESP32...")

    # ── GPS (variable conmutable como las demás) ──
    if show_gps:
        st.markdown('<div class="sec-heading">📍 Ubicación GPS</div>', unsafe_allow_html=True)
        lat = live.get("lat") or 0
        lon = live.get("lon") or 0
        has_fix = (lat != 0) or (lon != 0)
        if has_fix:
            gcol, mcol = st.columns([1, 2])
            with gcol:
                st.markdown(f"""
                <div class="chart-wrap">
                  <div class="lte-k">Latitud</div><div class="lte-v">{lat:.6f}</div>
                  <div class="lte-k" style="margin-top:8px;">Longitud</div><div class="lte-v">{lon:.6f}</div>
                </div>""", unsafe_allow_html=True)
            with mcol:
                if not df.empty and "lat" in df.columns:
                    g = df[["lat", "lon"]].dropna()
                    g = g[(g["lat"] != 0) | (g["lon"] != 0)]
                    if not g.empty:
                        st.map(g.rename(columns={"lat": "latitude", "lon": "longitude"}), size=30, zoom=15)
        else:
            st.info("📡 Sin señal GPS todavía. El módulo GNSS puede tardar en fijar posición (requiere cielo abierto).")

    # ── GRÁFICAS ──
    charts = [(k,VAR[k]) for k,show in [("rpm",show_rpm),("curr",show_curr),("volt",show_volt),("power",show_power)] if show]
    if charts and not df.empty:
        st.markdown('<div class="sec-heading">Tendencias históricas</div>', unsafe_allow_html=True)
        for i in range(0, len(charts), 2):
            pair = charts[i:i+2]
            cc   = st.columns(len(pair))
            for j,(k,m) in enumerate(pair):
                with cc[j]:
                    if m["col"] in df.columns:
                        st.markdown('<div class="chart-wrap">', unsafe_allow_html=True)
                        st.markdown(f'<div class="chart-wrap-title">{m["label"]} ({m["unit"]})</div>', unsafe_allow_html=True)
                        st.plotly_chart(full_fig(df, m["col"], m["color"], m["unit"]),
                                        use_container_width=True, config={"displayModeBar":False}, key=f"ch_{k}")
                        st.markdown('</div>', unsafe_allow_html=True)

    # ── TABLA ──
    if not df.empty:
        st.markdown('<div class="sec-heading">Registros recientes</div>', unsafe_allow_html=True)
        tdf = df.tail(20).copy().sort_values("timestamp", ascending=False)
        tdf["timestamp"] = pd.to_datetime(tdf["timestamp"]).dt.strftime("%d %b %Y  %H:%M:%S")
        keep = ["timestamp","status","pwm"]; rn={"timestamp":"Timestamp","status":"Estado","pwm":"PWM"}; cc={}
        if show_rpm:   keep.append("rpm");       rn["rpm"]="RPM";           cc["RPM"]=st.column_config.NumberColumn(format="%.1f")
        if show_curr:  keep.append("current_A"); rn["current_A"]="Corr (A)";cc["Corr (A)"]=st.column_config.NumberColumn(format="%.3f")
        if show_volt:  keep.append("voltage_V"); rn["voltage_V"]="Volt (V)";cc["Volt (V)"]=st.column_config.NumberColumn(format="%.2f")
        if show_power: keep.append("power_W");   rn["power_W"]="Potencia (W)";cc["Potencia (W)"]=st.column_config.NumberColumn(format="%.2f")
        keep = [c for c in keep if c in tdf.columns]
        st.dataframe(tdf[keep].rename(columns=rn), use_container_width=True, hide_index=True, column_config=cc)

        st.markdown('<div class="sec-heading">Descargar datos</div>', unsafe_allow_html=True)
        df_dl = df.copy()
        df_dl["timestamp"] = pd.to_datetime(df_dl["timestamp"]).dt.strftime("%d %b %Y  %H:%M:%S")
        csv = df_dl.to_csv(index=False).encode("utf-8")
        st.download_button("⬇️ Descargar todos los datos (CSV)", data=csv,
            file_name=f"motor_dc_{datetime.now(COL_TZ).strftime('%Y%m%d_%H%M%S')}.csv",
            mime="text/csv", use_container_width=True)


# ════════════════════════════════════════════════════════════════════════════════
#  VISTA: CONTROL  (replica del proyecto de teoría de control)
# ════════════════════════════════════════════════════════════════════════════════
def render_control():
    running = bool(ctrl_cfg.get("running", False))
    cur_rpm = float(live.get("rpm", 0)) if live else 0.0
    cur_sp  = float(ctrl_cfg.get("setpoint", DEFAULT_SETPOINT))

    # ── Panel de estado del control ──
    st.markdown(f"""
    <div class="status-panel">
      <div>
        <div class="status-label">Lazo de Control</div>
        <div class="status-val {'on' if running else 'off'}">
          <span class="sdot {'sdot-on' if running else 'sdot-off'}"></span>{'CORRIENDO' if running else 'DETENIDO'}
        </div>
      </div>
      <div style="text-align:center;">
        <div class="status-label">RPM Real</div>
        <div style="font-family:Rajdhani,sans-serif;font-size:1.8rem;font-weight:700;color:var(--col2);">{cur_rpm:.2f}</div>
      </div>
      <div style="text-align:center;">
        <div class="status-label">Setpoint</div>
        <div style="font-family:Rajdhani,sans-serif;font-size:1.8rem;font-weight:700;color:var(--col4);">{cur_sp:.1f}</div>
        <div style="font-family:Nunito,sans-serif;font-size:0.68rem;color:#5a3a3a;">RPM</div>
      </div>
      <div style="text-align:right;">
        <div class="status-label">Controlador activo</div>
        <div style="margin-top:4px;"><span class="ctrl-pill" style="background:{CONTROLLER_COLORS.get(ctrl_cfg.get('ctrl_type','PID'),'#673F69')};">{ctrl_cfg.get('ctrl_type','PID')}</span></div>
      </div>
    </div>
    """, unsafe_allow_html=True)

    left, right = st.columns([1, 2.4])

    # ── Panel de parámetros de control ──
    with left:
        st.markdown('<div class="sec-heading">Variables de control</div>', unsafe_allow_html=True)
        ctype = st.selectbox("Tipo de controlador", CONTROLLER_TYPES,
                             index=CONTROLLER_TYPES.index(ctrl_cfg.get("ctrl_type", "PID")),
                             key="ctrl_type")
        setpoint = st.slider("Setpoint (RPM)", MIN_SETPOINT_RPM, MAX_SETPOINT_RPM,
                             float(ctrl_cfg.get("setpoint", DEFAULT_SETPOINT)), 0.5, key="ctrl_sp")
        kp = st.number_input("Kp", 0.0, 100.0, float(ctrl_cfg.get("kp", DEFAULT_KP)), 0.1, key="ctrl_kp")
        ki = st.number_input("Ki", 0.0, 100.0, float(ctrl_cfg.get("ki", DEFAULT_KI)), 0.1,
                             key="ctrl_ki", disabled=ctype not in ("PI", "PID"))
        kd = st.number_input("Kd", 0.0, 100.0, float(ctrl_cfg.get("kd", DEFAULT_KD)), 0.1,
                             key="ctrl_kd", disabled=ctype not in ("PD", "PID"))

        base = {"mode": "control", "ctrl_type": ctype, "setpoint": setpoint,
                "kp": kp, "ki": ki, "kd": kd}

        if st.button("📤 Enviar parámetros", use_container_width=True):
            ok, e = push_control(base)
            st.success("Parámetros enviados al ESP32.") if ok else st.error(f"Error: {e}")

        if not running:
            if st.button("▶ Iniciar control", use_container_width=True):
                ok, e = push_control({**base, "running": True})
                if ok: st.rerun()
                else:  st.error(f"Error: {e}")
        else:
            if st.button("■ Frenar motor", use_container_width=True):
                ok, e = push_control({**base, "running": False})
                if ok: st.rerun()
                else:  st.error(f"Error: {e}")

        with st.expander("PWM manual (lazo abierto)"):
            pwm_manual = st.slider("PWM", 0, 255, int(ctrl_cfg.get("pwm_manual", 0)), key="ctrl_pwm")
            if st.button("Aplicar PWM manual", use_container_width=True):
                ok, e = push_control({"mode": "manual", "running": True, "pwm_manual": pwm_manual})
                st.success("PWM manual aplicado.") if ok else st.error(f"Error: {e}")

    # ── Gráficas de control ──
    with right:
        legend = "".join(
            f'<div class="ctrl-legend-item"><span class="ctrl-legend-dot" style="background:{c};"></span>{t}</div>'
            for t, c in CONTROLLER_COLORS.items())
        st.markdown(f'<div class="sec-heading">Respuesta del sistema</div>'
                    f'<div class="ctrl-legend">{legend}'
                    f'<div class="ctrl-legend-item"><span class="ctrl-legend-dot" style="background:{COLOR_SETPOINT};"></span>Setpoint</div>'
                    f'</div>', unsafe_allow_html=True)

        if not df.empty and "setpoint" in df.columns:
            st.markdown('<div class="chart-wrap">', unsafe_allow_html=True)
            st.markdown('<div class="chart-wrap-title">Velocidad (RPM) vs Tiempo</div>', unsafe_allow_html=True)
            st.plotly_chart(control_chart(df), use_container_width=True,
                            config={"displayModeBar":False}, key="ctrl_rpm")
            st.markdown('</div>', unsafe_allow_html=True)

            st.markdown('<div class="chart-wrap" style="margin-top:10px;">', unsafe_allow_html=True)
            st.markdown('<div class="chart-wrap-title">Error (RPM) vs Tiempo</div>', unsafe_allow_html=True)
            st.plotly_chart(error_chart(df), use_container_width=True,
                            config={"displayModeBar":False}, key="ctrl_err")
            st.markdown('</div>', unsafe_allow_html=True)
        else:
            st.info("⏳ Esperando datos de control del ESP32...")


# ════════════════════════════════════════════════════════════════════════════════
#  VISTA: RED LTE  (estilo G-NetTrack)
# ════════════════════════════════════════════════════════════════════════════════
def render_lte():
    lte = live.get("lte", {}) if isinstance(live.get("lte", {}), dict) else {}

    mcc = lte.get("mcc")
    mnc = lte.get("mnc")
    oper = operator_name(mcc, mnc, fallback=lte.get("operator") or "")
    net_type = network_type(lte)
    f_dl, f_ul = lte_frequencies(lte.get("band"), lte.get("earfcn"))
    st.markdown(f"""
    <div class="lte-banner">
      <div><div style="font-family:Rajdhani,sans-serif;font-size:0.75rem;letter-spacing:0.1em;opacity:0.85;">OPERADOR</div>
      <div class="lte-banner-op">{oper}</div></div>
      <div class="lte-banner-tech">{net_type}</div>
    </div>
    """, unsafe_allow_html=True)

    def card(k, v, unit=""):
        u = f'<span class="lte-u"> {unit}</span>' if unit else ""
        return f'<div class="lte-card"><div class="lte-k">{k}</div><div class="lte-v">{v}{u}</div></div>'

    def g(key, default="—"):
        val = lte.get(key, default)
        return default if val in (None, "", 0) and key not in ("rsrp","rsrq","rssi","sinr","csq_dbm") else val

    cells = ""
    cells += card("Operador", oper)
    cells += card("Tipo de red", net_type)
    cells += card("IP asignada", live.get("ip") or "—")
    cells += card("MCC", g("mcc"))
    cells += card("MNC", g("mnc"))
    cells += card("TAC", g("tac"))
    cells += card("Cell ID", g("cid"))
    cells += card("PCI", g("pci"))
    cells += card("Banda", g("band"))
    cells += card("EARFCN", g("earfcn"))
    cells += card("Frec. DL", f"{f_dl:.1f}" if f_dl is not None else "—", "MHz")
    cells += card("Frec. UL", f"{f_ul:.1f}" if f_ul is not None else "—", "MHz")
    cells += card("RSRP", lte.get("rsrp", "—"), "dBm")
    cells += card("RSRQ", lte.get("rsrq", "—"), "dB")
    cells += card("RSSI", lte.get("rssi", "—"), "dBm")
    cells += card("SINR", lte.get("sinr", "—"), "dB")
    cells += card("CSQ", f'{lte.get("csq", "—")}', "")
    cells += card("Señal", lte.get("csq_dbm", "—"), "dBm")
    _ping = lte.get("ping_ms", -1)
    cells += card("Ping", f"{_ping}" if isinstance(_ping, (int, float)) and _ping >= 0 else "—", "ms")
    st.markdown(f'<div class="sec-heading">Parámetros de celda (LTE Cat-1)</div>'
                f'<div class="lte-grid">{cells}</div>', unsafe_allow_html=True)

    # GPS junto a la red (igual que en G-NetTrack)
    glat = live.get("lat") or 0
    glon = live.get("lon") or 0
    has_fix = (glat != 0) or (glon != 0)
    gcells = ""
    gcells += card("Latitud",  f'{glat:.6f}' if has_fix else "Sin señal")
    gcells += card("Longitud", f'{glon:.6f}' if has_fix else "Sin señal")
    st.markdown(f'<div class="sec-heading">Posición GNSS</div>'
                f'<div class="lte-grid">{gcells}</div>', unsafe_allow_html=True)

    if lte.get("raw_cpsi"):
        st.markdown(f'<div class="lte-k">Respuesta cruda AT+CPSI?</div>', unsafe_allow_html=True)
        st.code(lte.get("raw_cpsi"), language="text")

    if not lte:
        st.info("📡 Aún no se reciben parámetros de red del ESP32.")


# ─── ROUTER ────────────────────────────────────────────────────────────────────
if view.endswith("Control"):
    render_control()
elif view.endswith("Red LTE"):
    render_lte()
else:
    render_monitoreo()

# ─── AUTO REFRESH ──────────────────────────────────────────────────────────────
if st.session_state.auto_refresh:
    import time
    time.sleep(REFRESH_SEC)
    st.rerun()

st.markdown(
    f'<div class="footer-txt">Motor DC Monitor & Control · ESP32 + INA219 + SIM7670G · {datetime.now(COL_TZ).strftime("%Y-%m-%d")}</div>',
    unsafe_allow_html=True)
