import streamlit as st
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from datetime import datetime
import requests

# ─── PAGE CONFIG ───────────────────────────────────────────────────────────────
st.set_page_config(
    page_title="Motor DC · Monitor",
    page_icon="⚡",
    layout="wide",
    initial_sidebar_state="expanded",
)

# ─── PALETTE  #FFAF45 · #FB6D48 · #D74B76 · #673F69 ──────────────────────────
CSS = """
<style>
@import url('https://fonts.googleapis.com/css2?family=Rajdhani:wght@400;500;600;700&family=Nunito:wght@300;400;600;700&display=swap');
:root {
  --col1:#FFAF45;--col2:#FB6D48;--col3:#D74B76;--col4:#673F69;
  --bg:#fdf6ee;--bg-card:#ffffff;--bg-sidebar:#fff8f0;
  --border:#f0e4d4;--text:#2b1e1e;--text-mid:#4a3030;--text-light:#7a5050;
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
[data-testid="stSidebar"] .stCheckbox label{font-size:0.88rem!important;color:var(--text)!important;font-family:'Nunito',sans-serif!important;font-weight:600!important;}
[data-testid="stSidebar"] .stButton button{background:var(--col4)!important;color:#fff!important;border:none!important;border-radius:8px!important;font-family:'Rajdhani',sans-serif!important;font-weight:700!important;letter-spacing:0.08em!important;text-transform:uppercase!important;font-size:0.82rem!important;transition:all .2s!important;}
[data-testid="stSidebar"] .stButton:first-of-type button{background:linear-gradient(135deg,var(--col1),var(--col2))!important;box-shadow:0 2px 8px rgba(251,109,72,0.25)!important;}
[data-testid="stSidebar"] .stButton:nth-of-type(2) button{background:linear-gradient(135deg,var(--col3),var(--col4))!important;box-shadow:0 2px 8px rgba(103,63,105,0.25)!important;}
[data-testid="stSidebar"] .stSelectbox label{color:var(--text)!important;font-weight:600!important;}
[data-testid="stSidebar"] .stSelectbox div{color:var(--text)!important;}
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
.status-label{font-family:'Rajdhani',sans-serif;font-size:0.72rem;font-weight:700;text-transform:uppercase;letter-spacing:0.12em;color:var(--text-light);}
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
.chart-wrap-title{font-family:'Rajdhani',sans-serif;font-size:0.78rem;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;color:var(--text-mid);margin-bottom:0.3rem;}
.stDataFrame thead th{background:#fdf0e4!important;font-family:'Rajdhani',sans-serif!important;font-size:0.7rem!important;font-weight:700!important;text-transform:uppercase;letter-spacing:0.1em;color:var(--text-mid)!important;}
.stDataFrame tbody td{font-family:'Nunito',sans-serif!important;font-size:0.82rem!important;color:var(--text)!important;}
.footer-txt{font-family:'Rajdhani',sans-serif;font-size:0.68rem;letter-spacing:0.08em;color:var(--text-light);text-align:right;margin-top:1rem;}
.error-box{background:#fff0f0;border:1px solid #f0d0d0;border-radius:10px;padding:0.8rem 1.2rem;color:#c0392b;font-size:0.82rem;margin-bottom:1rem;}
.vars-activas{font-family:'Rajdhani',sans-serif;font-size:0.75rem;letter-spacing:0.08em;color:var(--text-light);text-align:center;}
</style>
"""
st.markdown(CSS, unsafe_allow_html=True)

# ─── CONFIGURACIÓN ─────────────────────────────────────────────────────────────
SERVER_URL = "http://18.219.119.53:5000"
REFRESH_SEC = 5

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
        paper_bgcolor="rgba(0,0,0,0)", plot_bgcolor="rgba(0,0,0,0)",
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
        paper_bgcolor="rgba(0,0,0,0)", plot_bgcolor="rgba(0,0,0,0)",
        font=dict(family="Nunito", color="#4a3030", size=10),
        xaxis=dict(showgrid=False, zeroline=False, tickfont=dict(size=9), tickformat="%H:%M:%S"),
        yaxis=dict(showgrid=True, gridcolor="rgba(240,228,212,0.9)", zeroline=False, tickfont=dict(size=9)),
        hovermode="x unified",
        hoverlabel=dict(bgcolor="#fff", bordercolor="#f0e4d4", font=dict(family="Nunito",size=11)),
    )
    return fig

# ─── SESSION STATE ─────────────────────────────────────────────────────────────
if "auto_refresh" not in st.session_state:
    st.session_state.auto_refresh = True

# ─── SIDEBAR ───────────────────────────────────────────────────────────────────
with st.sidebar:
    st.markdown('<div class="sb-brand">⚡ MOTOR<span>DC</span></div>', unsafe_allow_html=True)

    st.markdown('<div class="sb-section">Variables</div>', unsafe_allow_html=True)
    show_rpm   = st.checkbox("⚡ Velocidad (RPM)",        value=True)
    show_curr  = st.checkbox("🔌 Corriente · INA219 (A)", value=True)
    show_volt  = st.checkbox("🔋 Voltaje (V)",            value=True)
    show_power = st.checkbox("⚡ Potencia (W)",            value=True)

    ca, cb = st.columns(2)
    with ca:
        if st.button("✦ Todas",   use_container_width=True): st.session_state["_q"]="all";  st.rerun()
    with cb:
        if st.button("✕ Ninguna", use_container_width=True): st.session_state["_q"]="none"; st.rerun()
    sel = st.session_state.pop("_q", None)
    if sel == "all":  show_rpm=show_curr=show_volt=show_power=True
    if sel == "none": show_rpm=show_curr=show_volt=show_power=False

    st.markdown('<div class="sb-section">Historial</div>', unsafe_allow_html=True)
    hist_window = st.selectbox(
        "Ventana", [30,60,120,240], index=1,
        format_func=lambda x: f"Últimas {x} lecturas",
    )

    st.markdown('<div class="sb-section">Actualización</div>', unsafe_allow_html=True)
    auto_refresh = st.checkbox("Auto-refresh cada 5 s", value=st.session_state.auto_refresh)
    st.session_state.auto_refresh = auto_refresh

    st.markdown("---")
    an = sum([show_rpm, show_curr, show_volt, show_power])
    st.markdown(
        f'<div class="vars-activas">{an} / 4 variables activas</div>',
        unsafe_allow_html=True
    )

# ─── OBTENER DATOS REALES ──────────────────────────────────────────────────────
df, err_hist   = fetch_readings(hist_window)
live, err_live = fetch_latest()

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
    <div class="dash-title">⚡ MOTOR<span>DC</span> · MONITOR</div>
    <div class="dash-sub">ESP32 + INA219 + Hall Encoder · SIM7670G · Claro CO</div>
  </div>
  <div style="display:flex;align-items:center;gap:14px;">
    <span style="font-family:Rajdhani,sans-serif;font-size:0.72rem;letter-spacing:0.07em;color:#7a5050;">
      {datetime.now().strftime('%d %b %Y · %H:%M:%S')}
    </span>
    {pill_html}
  </div>
</div>
""", unsafe_allow_html=True)

# Mostrar error de conexión si existe
if err_live:
    st.markdown(
        f'<div class="error-box">⚠️ No se puede conectar al servidor: <code>{SERVER_URL}</code> — {err_live}</div>',
        unsafe_allow_html=True
    )

# ─── STATUS ────────────────────────────────────────────────────────────────────
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
    <div style="font-family:Nunito,sans-serif;font-size:0.68rem;color:#7a5050;">/ 255</div>
  </div>
  <div style="text-align:right;">
    <div class="status-label">Dispositivo</div>
    <div style="font-family:Rajdhani,sans-serif;font-size:0.95rem;font-weight:600;color:#673F69;letter-spacing:0.04em;">ESP32 · SIM7670G</div>
    <div style="font-family:Nunito,sans-serif;font-size:0.7rem;color:#7a5050;">Barranquilla, CO</div>
  </div>
</div>
""", unsafe_allow_html=True)

# ─── SPARKLINES ────────────────────────────────────────────────────────────────
active = [k for k, show in [("rpm",show_rpm),("curr",show_curr),("volt",show_volt),("power",show_power)] if show]

if active and not df.empty:
    st.markdown('<div class="sec-heading">Estado actual</div>', unsafe_allow_html=True)
    left_keys  = active[:2]
    right_keys = active[2:]
    lcol, rcol = st.columns(2)

    for keys, container in [(left_keys, lcol), (right_keys, rcol)]:
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
                        st.plotly_chart(spark_fig(series, m["color"]),
                                        use_container_width=True,
                                        config={"displayModeBar":False}, key=f"sp_{k}")
                with c3:
                    if m["unit"] == "rpm":
                        val_str = f"{val_now:.0f} {m['unit']}"
                    elif m["unit"] == "A":
                        val_str = f"{val_now:.3f} {m['unit']}"
                    else:
                        val_str = f"{val_now:.2f} {m['unit']}"
                    st.markdown(
                        f'<div class="spark-info">'
                        f'  <span class="spark-value">{val_str}</span>'
                        f'  <span class="spark-badge {bcls}">{sign}{delta}%</span>'
                        f'</div>',
                        unsafe_allow_html=True
                    )
elif active and df.empty:
    st.info("⏳ Esperando datos del ESP32...")
else:
    st.info("Selecciona al menos una variable en la barra lateral.")

# ─── GRÁFICAS ──────────────────────────────────────────────────────────────────
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
                                    use_container_width=True,
                                    config={"displayModeBar":False}, key=f"ch_{k}")
                    st.markdown('</div>', unsafe_allow_html=True)

# ─── TABLA ─────────────────────────────────────────────────────────────────────
if not df.empty:
    st.markdown('<div class="sec-heading">Registros recientes</div>', unsafe_allow_html=True)
    tdf = df.tail(20).copy().sort_values("timestamp", ascending=False)
    tdf["timestamp"] = pd.to_datetime(tdf["timestamp"]).dt.strftime("%d %b %Y  %H:%M:%S")
    keep = ["timestamp","status","pwm"]; rn={"timestamp":"Timestamp","status":"Estado","pwm":"PWM"}; cc={}
    if show_rpm:   keep.append("rpm");       rn["rpm"]="RPM";           cc["RPM"]=st.column_config.NumberColumn(format="%.1f")
    if show_curr:  keep.append("current_A"); rn["current_A"]="Corr (A)";cc["Corr (A)"]=st.column_config.NumberColumn(format="%.3f")
    if show_volt:  keep.append("voltage_V"); rn["voltage_V"]="Volt (V)";cc["Volt (V)"]=st.column_config.NumberColumn(format="%.2f")
    if show_power: keep.append("power_W");   rn["power_W"]="Potencia (W)";cc["Potencia (W)"]=st.column_config.NumberColumn(format="%.2f")
    st.dataframe(tdf[keep].rename(columns=rn), use_container_width=True, hide_index=True, column_config=cc)

# ─── AUTO REFRESH ──────────────────────────────────────────────────────────────
if st.session_state.auto_refresh:
    import time
    time.sleep(REFRESH_SEC)
    st.rerun()

st.markdown(
    f'<div class="footer-txt">Motor DC Monitor · ESP32 + INA219 + SIM7670G · {datetime.now().strftime("%Y-%m-%d")}</div>',
    unsafe_allow_html=True
)
