#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Wi-Fi Credentials
const char* ssid = "JioFiber-J9c3t_2-4";
const char* password = "yoccO123";

// Google Gemini API Key (Get free from https://aistudio.google.com/)
const char* gemini_api_key = "AQ.Ab8RN6KPVzqxCY-QH1y6x2npHTA-iuqXcTnfh3htQXLG_Cz6mw"; 

// Hardware Pin Definitions
#define TDS_PIN 32
#define VREF 3.3

WebServer server(80);

float tdsValue = 0;
float voltage = 0;
int adcValue = 0;

String getStatus(float tds) {
  if(tds < 150) return "Water quality is Excellent";
  if(tds < 300) return "Water quality is Good";
  if(tds < 500) return "Water quality is Acceptable";
  if(tds < 1000) return "Water quality is Poor";

  return "Unsafe Water Quality";
}

void readSensor() {
  long sum = 0;
  for(int i = 0; i < 30; i++) {
    sum += analogRead(TDS_PIN);
    delay(10);
  }
  adcValue = sum / 30;

  // Convert ADC to voltage (ESP32 12-bit ADC: 0 - 4095)
  voltage = adcValue * VREF / 4095.0;

  // TDS calculation polynomial equation
  tdsValue = (133.42 * voltage * voltage * voltage
            - 255.86 * voltage * voltage
            + 857.39 * voltage) * 0.5;

  if(tdsValue < 0) tdsValue = 0;
}

void handleAPI() {
  readSensor();

  String json = "{";
  json += "\"adc\":" + String(adcValue) + ",";
  json += "\"voltage\":" + String(voltage, 3) + ",";
  json += "\"tds\":" + String(tdsValue, 1) + ",";
  json += "\"status\":\"" + getStatus(tdsValue) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// Function to call Google Gemini 1.5 Flash API
String queryGeminiAI(String userQuestion) {
  readSensor();

  // If no key provided, return smart fallback answer
  if (String(gemini_api_key) == "YOUR_GEMINI_API_KEY_HERE" || String(gemini_api_key) == "") {
    String backup = "<b>[AquaSense AI Engine]</b><br>";
    backup += "Current TDS Reading: <b>" + String(tdsValue, 1) + " PPM</b> (" + getStatus(tdsValue) + ").<br><br>";
    if (tdsValue < 150) {
      backup += "• <b>Safety:</b> Excellent for drinking. Highly pure water with balanced mineral content.<br>";
      backup += "• <b>Recommendation:</b> No filtration needed. Ideal for daily hydration & infants.";
    } else if (tdsValue < 300) {
      backup += "• <b>Safety:</b> Safe tap water. Moderate mineral hardness detected.<br>";
      backup += "• <b>Recommendation:</b> Good for cooking and drinking. Optional carbon filter for taste.";
    } else if (tdsValue < 500) {
      backup += "• <b>Safety:</b> Acceptable, but high dissolved solids (calcium/magnesium).<br>";
      backup += "• <b>Recommendation:</b> RO (Reverse Osmosis) purification is recommended before drinking.";
    } else {
      backup += "• <b>CRITICAL WARNING:</b> Water TDS is dangerously high (>500 PPM)!<br>";
      backup += "• <b>Recommendation:</b> DO NOT DRINK. High risk of heavy metals or contaminants. Requires industrial RO treatment.";
    }
    return backup;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return "Error: Wi-Fi disconnected. Cannot reach Gemini AI servers.";
  }

  WiFiClientSecure client;
  client.setInsecure(); // Skip SSL certificate verification for fast response

  HTTPClient http;
  String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(gemini_api_key);

  http.begin(client, apiUrl);
  http.addHeader("Content-Type", "application/json");

  // Construct Prompt with Live Sensor Data
  String prompt = "You are AquaSense AI Water Quality Expert for Reliance Foundation School Exhibition. ";
  prompt += "Live Sensor Data: TDS = " + String(tdsValue, 1) + " PPM, Voltage = " + String(voltage, 3) + "V, Status = " + getStatus(tdsValue) + ". ";
  prompt += "User Question: '" + userQuestion + "'. ";
  prompt += "Give a concise, professional 3-sentence expert diagnosis and health recommendation. Use bullet points.";

  // Escape quotes for JSON payload
  prompt.replace("\"", "\\\"");

  String payload = "{\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}]}";

  int httpCode = http.POST(payload);
  String responseText = "";

  if (httpCode == 200) {
    String response = http.getString();
    
    // Parse response JSON for text
    int textIndex = response.indexOf("\"text\": \"");
    if (textIndex != -1) {
      int start = textIndex + 9;
      int end = response.indexOf("\"", start);
      responseText = response.substring(start, end);
      
      // Unescape JSON formatting
      responseText.replace("\\n", "<br>");
      responseText.replace("\\\"", "\"");
      responseText.replace("**", "<b>");
      responseText.replace("* ", "• ");
    } else {
      responseText = "AI response received, but could not parse response.";
    }
  } else {
    responseText = "Gemini API Error (HTTP " + String(httpCode) + "). Check API key or Wi-Fi internet connection.";
  }

  http.end();
  return responseText;
}

void handleAIEndpoint() {
  String question = "Analyze this water sample and give a health report.";
  if (server.hasArg("q")) {
    question = server.arg("q");
  }

  String aiResponse = queryGeminiAI(question);
  
  String json = "{\"answer\":\"" + aiResponse + "\"}";
  server.send(200, "application/json", json);
}

// Webpage HTML stored in Flash Memory (PROGMEM)
static const char PAGE_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="theme-color" content="#0b132b">
    <title>Aqua Sense - Smart Water Monitor</title>
    <style>
        :root {
            --bg-color: #0b132b;
            --card-bg: rgba(28, 37, 65, 0.7);
            --accent-blue: #00f2fe;
            --status-color: #10B981;
            --text-main: #ffffff;
            --text-sub: #a0aec0;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; -webkit-tap-highlight-color: transparent; }

        body {
            background: radial-gradient(circle at top, #1c2541 0%, #0b132b 100%);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            padding: 12px;
        }

        .dashboard {
            width: 100%;
            max-width: 1080px;
            display: flex;
            flex-direction: column;
            gap: 16px;
            margin: 0 auto;
        }

        @media (min-width: 820px) {
            body { align-items: center; padding: 24px; }
            .dashboard {
                display: grid;
                grid-template-columns: 380px 1fr;
                gap: 24px;
            }
        }

        .card {
            background: var(--card-bg);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid rgba(255, 255, 255, 0.12);
            border-radius: 20px;
            padding: 18px;
            box-shadow: 0 15px 35px rgba(0,0,0,0.4);
            text-align: center;
            position: relative;
        }

        .school-title {
            font-size: 0.68rem;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 1.2px;
            color: var(--accent-blue);
            margin-bottom: 4px;
        }

        .header {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            margin-bottom: 12px;
        }

        .header svg { fill: var(--accent-blue); width: 28px; height: 28px; filter: drop-shadow(0 0 6px var(--accent-blue)); }
        .header h1 {
            font-size: 1.75rem;
            font-weight: 800;
            background: linear-gradient(45deg, #00f2fe, #4facfe);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .btn-row {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 8px;
            margin-bottom: 14px;
        }

        @media (min-width: 400px) {
            .btn-row { grid-template-columns: repeat(4, 1fr); }
        }

        .action-btn {
            background: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255, 255, 255, 0.15);
            color: #fff;
            padding: 10px 8px;
            border-radius: 12px;
            font-size: 0.75rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 4px;
        }

        .action-btn:active { transform: scale(0.96); }
        .action-btn.active { background: rgba(0, 242, 254, 0.25); border-color: var(--accent-blue); color: var(--accent-blue); }
        .action-btn.ai-btn { background: linear-gradient(45deg, #7F00FF, #E100FF); border: none; color: white; font-weight: 700; grid-column: span 2; }

        .pulse-badge {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            background: rgba(16, 185, 129, 0.15);
            color: var(--status-color);
            padding: 6px 14px;
            border-radius: 20px;
            font-size: 0.8rem;
            font-weight: 600;
            margin-bottom: 12px;
            border: 1px solid var(--status-color);
            transition: all 0.4s ease;
        }

        .dot {
            width: 8px; height: 8px;
            background-color: var(--status-color);
            border-radius: 50%;
            box-shadow: 0 0 8px var(--status-color);
            animation: pulse 1.5s infinite;
        }

        @keyframes pulse { 0% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(1.2); } 100% { opacity: 1; transform: scale(1); } }

        .gauge-container {
            position: relative;
            width: 190px;
            height: 145px;
            margin: 0 auto 5px;
        }

        .gauge-svg { width: 100%; height: 100%; }
        .gauge-bg {
            fill: none;
            stroke: rgba(255, 255, 255, 0.08);
            stroke-width: 14;
            stroke-dasharray: 330 440;
            stroke-linecap: round;
            transform: rotate(135deg);
            transform-origin: 50% 50%;
        }

        .gauge-progress {
            fill: none;
            stroke: var(--status-color);
            stroke-width: 14;
            stroke-dasharray: 0 440;
            stroke-linecap: round;
            transform: rotate(135deg);
            transform-origin: 50% 50%;
            transition: stroke-dasharray 0.8s ease, stroke 0.5s ease;
            filter: drop-shadow(0 0 8px var(--status-color));
        }

        .gauge-center {
            position: absolute;
            top: 55%; left: 50%;
            transform: translate(-50%, -50%);
            text-align: center;
        }

        .tds-value { font-size: 2.6rem; font-weight: 800; color: #fff; line-height: 1; }
        .tds-unit { font-size: 0.7rem; color: var(--text-sub); margin-top: 2px; letter-spacing: 2px; }

        .spectrum-box {
            background: rgba(0, 0, 0, 0.25);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 14px;
            padding: 10px 12px;
            margin-bottom: 12px;
            text-align: left;
        }

        .spectrum-title { font-size: 0.65rem; color: var(--text-sub); text-transform: uppercase; margin-bottom: 6px; }

        .spectrum-bar {
            height: 7px;
            border-radius: 4px;
            background: linear-gradient(90deg, #10B981 0%, #3B82F6 30%, #F59E0B 60%, #EF4444 100%);
            position: relative;
        }

        .spectrum-marker {
            width: 13px;
            height: 13px;
            background: #fff;
            border: 2px solid var(--bg-color);
            border-radius: 50%;
            position: absolute;
            top: -3px;
            left: 0%;
            transform: translateX(-50%);
            transition: left 0.5s ease;
            box-shadow: 0 0 6px #fff;
        }

        .spectrum-labels {
            display: flex;
            justify-content: space-between;
            font-size: 0.62rem;
            color: var(--text-sub);
            margin-top: 5px;
        }

        .metrics-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px;
        }

        .metric-card {
            background: rgba(255, 255, 255, 0.03);
            border: 1px solid rgba(255, 255, 255, 0.06);
            padding: 10px;
            border-radius: 12px;
            text-align: left;
        }

        .metric-label { font-size: 0.65rem; color: var(--text-sub); text-transform: uppercase; }
        .metric-value { font-size: 1.05rem; font-weight: 600; color: #fff; margin-top: 2px; }

        .chart-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }

        .chart-title { font-size: 0.82rem; font-weight: 700; color: var(--accent-blue); text-transform: uppercase; letter-spacing: 1px; }

        .chart-container {
            width: 100%;
            height: 180px;
            position: relative;
        }

        @media (min-width: 820px) {
            .chart-container { height: 240px; }
        }

        canvas {
            width: 100% !important;
            height: 100% !important;
        }

        .analytics-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            margin-top: 12px;
        }

        .stat-box {
            background: rgba(0, 0, 0, 0.2);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 10px;
            padding: 8px;
            text-align: center;
        }

        .stat-title { font-size: 0.6rem; color: var(--text-sub); text-transform: uppercase; }
        .stat-val { font-size: 0.95rem; font-weight: 700; color: #fff; margin-top: 2px; }

        .modal {
            display: none;
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(0,0,0,0.85);
            backdrop-filter: blur(10px);
            z-index: 100;
            justify-content: center;
            align-items: center;
            padding: 16px;
        }

        .modal-content {
            background: #1c2541;
            border: 1px solid rgba(255,255,255,0.2);
            border-radius: 18px;
            padding: 18px;
            max-width: 420px;
            width: 100%;
            text-align: left;
            color: #fff;
        }

        .modal-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
        .modal-header h3 { font-size: 1.1rem; color: #E100FF; }
        .close-btn { cursor: pointer; font-size: 1.4rem; color: var(--text-sub); }

        .preset-btn {
            background: rgba(255, 255, 255, 0.06);
            border: 1px solid rgba(255, 255, 255, 0.12);
            color: #fff;
            padding: 8px 10px;
            border-radius: 10px;
            font-size: 0.72rem;
            margin-bottom: 6px;
            cursor: pointer;
            width: 100%;
            text-align: left;
            transition: all 0.2s ease;
        }

        .preset-btn:hover { background: rgba(225, 0, 255, 0.2); border-color: #E100FF; }

        .ai-input-box {
            display: flex;
            gap: 6px;
            margin-top: 10px;
        }

        .ai-input-box input {
            flex: 1;
            background: rgba(0,0,0,0.3);
            border: 1px solid rgba(255,255,255,0.15);
            border-radius: 10px;
            padding: 8px 12px;
            color: #fff;
            font-size: 0.8rem;
            outline: none;
        }

        .ai-send-btn {
            background: linear-gradient(45deg, #7F00FF, #E100FF);
            border: none;
            color: #fff;
            padding: 8px 14px;
            border-radius: 10px;
            font-size: 0.8rem;
            font-weight: 700;
            cursor: pointer;
        }

        .ai-response-box {
            background: rgba(0, 0, 0, 0.35);
            border: 1px solid rgba(225, 0, 255, 0.3);
            border-radius: 12px;
            padding: 12px;
            margin-top: 12px;
            font-size: 0.8rem;
            line-height: 1.45;
            min-height: 80px;
            max-height: 200px;
            overflow-y: auto;
            color: #e2e8f0;
        }

        footer { margin-top: 14px; font-size: 0.68rem; color: rgba(255, 255, 255, 0.35); text-align: center; }
    </style>
</head>
<body>

<div class="dashboard">
    <div class="card">
        <div class="school-title">Reliance Foundation School Exhibition</div>
        
        <div class="header">
            <svg viewBox="0 0 24 24"><path d="M12,2.69L5.5,10.2C4.5,11.37 4,12.7 4,14A8,8 0 0,0 12,22A8,8 0 0,0 20,14C20,12.7 19.5,11.37 18.5,10.2L12,2.69Z" /></svg>
            <h1>Aqua Sense</h1>
        </div>

        <div class="btn-row">
            <button class="action-btn ai-btn" onclick="openAiModal()">✨ Gemini AI Advisor</button>
            <button class="action-btn" id="audioToggleBtn" onclick="toggleAudioAlert()">&#128277; Alarm</button>
            <button class="action-btn" onclick="playAlarmSound()">&#128266; Sound</button>
            <button class="action-btn" onclick="openModal()">&#8505; Guide</button>
            <button class="action-btn" onclick="exportCSV()">&#128229; CSV</button>
        </div>

        <div class="pulse-badge" id="statusBadge">
            <div class="dot"></div>
            <span id="status">Connecting...</span>
        </div>

        <div class="gauge-container">
            <svg class="gauge-svg" viewBox="0 0 180 180">
                <circle class="gauge-bg" cx="90" cy="90" r="70"></circle>
                <circle class="gauge-progress" id="gaugeRing" cx="90" cy="90" r="70"></circle>
            </svg>
            <div class="gauge-center">
                <div class="tds-value" id="tds">--</div>
                <div class="tds-unit">PPM</div>
            </div>
        </div>

        <div class="spectrum-box">
            <div class="spectrum-title">Purity Spectrum</div>
            <div class="spectrum-bar">
                <div class="spectrum-marker" id="spectrumMarker"></div>
            </div>
            <div class="spectrum-labels">
                <span>0 RO</span>
                <span>150 Pure</span>
                <span>300 Hard</span>
                <span>500+ Unsafe</span>
            </div>
        </div>

        <div class="metrics-grid">
            <div class="metric-card">
                <div class="metric-label">Voltage</div>
                <div class="metric-value" id="voltage">-- V</div>
            </div>
            <div class="metric-card">
                <div class="metric-label">ADC Signal</div>
                <div class="metric-value" id="adc">--</div>
            </div>
        </div>
    </div>

    <div class="card" style="text-align: left;">
        <div class="chart-header">
            <div class="chart-title">&#128200; Live Quality Trend</div>
            <span style="font-size: 0.65rem; color: var(--text-sub);" id="updateCounter">Connecting...</span>
        </div>

        <div class="chart-container">
            <canvas id="realtimeChart"></canvas>
        </div>

        <div class="analytics-row">
            <div class="stat-box">
                <div class="stat-title">Min TDS</div>
                <div class="stat-val" id="minTds">--</div>
            </div>
            <div class="stat-box">
                <div class="stat-title">Avg TDS</div>
                <div class="stat-val" id="avgTds">--</div>
            </div>
            <div class="stat-box">
                <div class="stat-title">Max TDS</div>
                <div class="stat-val" id="maxTds">--</div>
            </div>
        </div>

        <footer>Smart Water Monitoring System • ESP32 Powered</footer>
    </div>
</div>

<!-- Gemini AI Modal -->
<div class="modal" id="aiModal">
    <div class="modal-content">
        <div class="modal-header">
            <h3>✨ Gemini 1.5 AI Water Advisor</h3>
            <span class="close-btn" onclick="closeAiModal()">&times;</span>
        </div>
        
        <p style="font-size: 0.72rem; color: var(--text-sub); margin-bottom: 8px;">Ask Gemini AI to evaluate your live water sample:</p>
        
        <button class="preset-btn" onclick="askAi('Is this water sample safe for drinking and cooking?')">🩺 Is this safe for drinking?</button>
        <button class="preset-btn" onclick="askAi('What are the health risks of this TDS level?')">⚠️ Health risk evaluation</button>
        <button class="preset-btn" onclick="askAi('What purification method should I use for this sample?')">🔬 Recommended purification method</button>

        <div class="ai-input-box">
            <input type="text" id="customAiPrompt" placeholder="Type a question for Gemini AI...">
            <button class="ai-send-btn" onclick="askAi(document.getElementById('customAiPrompt').value)">Ask</button>
        </div>

        <div class="ai-response-box" id="aiResponse">
            Tap a question above to get an instant AI diagnosis powered by Google Gemini 1.5 Flash.
        </div>
    </div>
</div>

<!-- Reference Modal -->
<div class="modal" id="guideModal">
    <div class="modal-content">
        <div class="modal-header">
            <h3>TDS Water Quality Reference</h3>
            <span class="close-btn" onclick="closeModal()">&times;</span>
        </div>
        <table class="guide-table" style="width: 100%; border-collapse: collapse; margin-top: 8px; font-size: 0.75rem;">
            <tr style="border-bottom: 1px solid rgba(255,255,255,0.1);"><th style="padding: 6px; text-align: left;">TDS (PPM)</th><th style="padding: 6px; text-align: left;">Category</th><th style="padding: 6px; text-align: left;">Source</th></tr>
            <tr style="color: #10B981; border-bottom: 1px solid rgba(255,255,255,0.1);"><td style="padding: 6px;">0 - 50</td><td style="padding: 6px;">Ultra Pure</td><td style="padding: 6px;">RO / Distilled</td></tr>
            <tr style="color: #3B82F6; border-bottom: 1px solid rgba(255,255,255,0.1);"><td style="padding: 6px;">50 - 150</td><td style="padding: 6px;">Excellent</td><td style="padding: 6px;">Pure Tap Water</td></tr>
            <tr style="color: #F59E0B; border-bottom: 1px solid rgba(255,255,255,0.1);"><td style="padding: 6px;">150 - 300</td><td style="padding: 6px;">Acceptable</td><td style="padding: 6px;">Hard Tap Water</td></tr>
            <tr style="color: #EF4444; border-bottom: 1px solid rgba(255,255,255,0.1);"><td style="padding: 6px;">500+</td><td style="padding: 6px;">Unsafe</td><td style="padding: 6px;">Contaminated</td></tr>
        </table>
    </div>
</div>

<script>
    const maxTDS = 1000;
    const maxDashArray = 330;
    const dataLog = [["Timestamp", "TDS (PPM)", "Voltage (V)", "ADC", "Status"]];
    const chartHistory = [];
    const maxHistoryPoints = 25;

    let audioCtx = null;
    let audioAlertEnabled = false;
    let lastBeepTime = 0;

    function initAudio() {
        if (!audioCtx) {
            audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        }
        if (audioCtx.state === 'suspended') {
            audioCtx.resume();
        }
    }

    function toggleAudioAlert() {
        initAudio();
        audioAlertEnabled = !audioAlertEnabled;
        const btn = document.getElementById("audioToggleBtn");
        btn.classList.toggle("active", audioAlertEnabled);
        btn.innerHTML = audioAlertEnabled ? "&#128276; Alarm: ON" : "&#128277; Alarm";
    }

    function playAlarmSound() {
        initAudio();
        try {
            const now = audioCtx.currentTime;

            let osc1 = audioCtx.createOscillator();
            let gain1 = audioCtx.createGain();
            osc1.type = 'sawtooth';
            osc1.frequency.setValueAtTime(1050, now);
            gain1.gain.setValueAtTime(0.25, now);
            gain1.gain.exponentialRampToValueAtTime(0.001, now + 0.12);
            osc1.connect(gain1);
            gain1.connect(audioCtx.destination);
            osc1.start(now);
            osc1.stop(now + 0.12);

            let osc2 = audioCtx.createOscillator();
            let gain2 = audioCtx.createGain();
            osc2.type = 'sawtooth';
            osc2.frequency.setValueAtTime(1320, now + 0.14);
            gain2.gain.setValueAtTime(0.25, now + 0.14);
            gain2.gain.exponentialRampToValueAtTime(0.001, now + 0.28);
            osc2.connect(gain2);
            gain2.connect(audioCtx.destination);
            osc2.start(now + 0.14);
            osc2.stop(now + 0.28);
        } catch(e) {}
    }

    function openModal() { document.getElementById("guideModal").style.display = "flex"; }
    function closeModal() { document.getElementById("guideModal").style.display = "none"; }

    function openAiModal() { document.getElementById("aiModal").style.display = "flex"; }
    function closeAiModal() { document.getElementById("aiModal").style.display = "none"; }

    function askAi(promptText) {
        if (!promptText || promptText.trim() === "") return;
        const box = document.getElementById("aiResponse");
        box.innerHTML = "⚡ <i>Gemini 1.5 AI analyzing live sample data...</i>";

        fetch('/ask-ai?q=' + encodeURIComponent(promptText))
            .then(r => r.json())
            .then(data => {
                box.innerHTML = data.answer;
            })
            .catch(err => {
                box.innerHTML = "Error reaching AI server. Make sure ESP32 is connected to Wi-Fi.";
            });
    }

    function exportCSV() {
        let csvContent = "data:text/csv;charset=utf-8," + dataLog.map(e => e.join(",")).join("\n");
        let encodedUri = encodeURI(csvContent);
        let link = document.createElement("a");
        link.setAttribute("href", encodedUri);
        link.setAttribute("download", "aquasense_water_log.csv");
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }

    function getColorForTDS(tds) {
        if (tds < 150) return '#10B981';
        if (tds < 300) return '#3B82F6';
        if (tds < 500) return '#F59E0B';
        return '#EF4444';
    }

    function drawChart() {
        const canvas = document.getElementById("realtimeChart");
        const ctx = canvas.getContext("2d");
        
        const rect = canvas.getBoundingClientRect();
        canvas.width = rect.width * window.devicePixelRatio;
        canvas.height = rect.height * window.devicePixelRatio;
        ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

        const w = rect.width;
        const h = rect.height;

        ctx.clearRect(0, 0, w, h);

        if (chartHistory.length < 2) return;

        ctx.strokeStyle = "rgba(255, 255, 255, 0.05)";
        ctx.lineWidth = 1;
        for (let i = 0; i <= 3; i++) {
            let y = (h / 3) * i;
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        const maxVal = Math.max(300, ...chartHistory);
        const points = chartHistory.map((val, idx) => {
            let x = (w / (maxHistoryPoints - 1)) * idx;
            let y = h - ((val / maxVal) * (h - 20)) - 10;
            return { x, y, val };
        });

        let grad = ctx.createLinearGradient(0, 0, 0, h);
        grad.addColorStop(0, 'rgba(0, 242, 254, 0.3)');
        grad.addColorStop(1, 'rgba(0, 242, 254, 0.0)');

        ctx.beginPath();
        ctx.moveTo(points[0].x, h);
        points.forEach(p => ctx.lineTo(p.x, p.y));
        ctx.lineTo(points[points.length - 1].x, h);
        ctx.closePath();
        ctx.fillStyle = grad;
        ctx.fill();

        ctx.beginPath();
        points.forEach((p, i) => {
            if (i === 0) ctx.moveTo(p.x, p.y);
            else ctx.lineTo(p.x, p.y);
        });
        ctx.strokeStyle = "#00f2fe";
        ctx.lineWidth = 2.5;
        ctx.stroke();

        const lastP = points[points.length - 1];
        ctx.beginPath();
        ctx.arc(lastP.x, lastP.y, 5, 0, Math.PI * 2);
        ctx.fillStyle = "#00f2fe";
        ctx.shadowColor = "#00f2fe";
        ctx.shadowBlur = 8;
        ctx.fill();
        ctx.shadowBlur = 0;
    }

    setInterval(() => {
        const fetchStart = Date.now();
        fetch('/tds')
            .then(r => r.json())
            .then(data => {
                const tds = Math.max(0, data.tds);
                const color = getColorForTDS(tds);

                document.getElementById("tds").innerHTML = tds.toFixed(1);
                document.getElementById("voltage").innerHTML = data.voltage + " V";
                document.getElementById("adc").innerHTML = data.adc;
                document.getElementById("status").innerHTML = data.status;
                document.getElementById("updateCounter").innerHTML = "Ping: " + (Date.now() - fetchStart) + "ms";

                document.documentElement.style.setProperty('--status-color', color);

                let percentage = Math.min(tds / maxTDS, 1);
                let strokeLength = percentage * maxDashArray;
                document.getElementById("gaugeRing").style.strokeDasharray = strokeLength + " 440";

                document.getElementById("spectrumMarker").style.left = (percentage * 100) + "%";

                if (dataLog.length < 100) {
                    dataLog.push([new Date().toLocaleTimeString(), tds.toFixed(1), data.voltage, data.adc, data.status]);
                }

                chartHistory.push(tds);
                if (chartHistory.length > maxHistoryPoints) chartHistory.shift();
                drawChart();

                const min = Math.min(...chartHistory).toFixed(1);
                const max = Math.max(...chartHistory).toFixed(1);
                const avg = (chartHistory.reduce((a, b) => a + b, 0) / chartHistory.length).toFixed(1);
                document.getElementById("minTds").innerHTML = min;
                document.getElementById("avgTds").innerHTML = avg;
                document.getElementById("maxTds").innerHTML = max;

                if (tds >= 500 && audioAlertEnabled) {
                    let now = Date.now();
                    if (now - lastBeepTime > 600) {
                        playAlarmSound();
                        lastBeepTime = now;
                    }
                }
            })
            .catch(err => {
                document.getElementById("status").innerHTML = "Connecting...";
            });
    }, 1000);

    window.addEventListener('resize', drawChart);
</script>

</body>
</html>
)=====";

void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);

  Serial.println("\n--- Aqua Sense Starting ---");
  Serial.print("Attempting WiFi connection to SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  while(WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("Open this IP in your browser: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ WiFi Connection Failed! Starting Hotspot (AP Mode)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AquaSense-Hotspot", "12345678");
    
    Serial.println("------------------------------------------------");
    Serial.println("📲 CONNECT TO: AquaSense-Hotspot | Pass: 12345678");
    Serial.print("🌐 OPEN IP IN BROWSER: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("------------------------------------------------");
  }

  server.on("/", handleRoot);
  server.on("/tds", handleAPI);
  server.on("/ask-ai", handleAIEndpoint);

  server.begin();
  Serial.println("AquaSense Web Server Started");
}

void loop() {
  server.handleClient();
}