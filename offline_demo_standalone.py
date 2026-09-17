import os
import sys
import json
import base64
import asyncio
import re
import urllib.request
import zipfile
import threading

try:
    from fastapi import FastAPI, WebSocket, Request
    from fastapi.responses import HTMLResponse
    from fastapi.middleware.cors import CORSMiddleware
    import uvicorn
except ImportError:
    print("Dependencies missing. Please run: pip install fastapi uvicorn websockets vosk")
    sys.exit(1)

try:
    from vosk import Model, KaldiRecognizer
    HAS_VOSK = True
except ImportError:
    HAS_VOSK = False
    print("WARNING: Vosk not installed. Using mocked offline extraction.")

# ======================================================================
# HTML TEMPLATES (Embedded for standalone deployment)
# ======================================================================

DASHBOARD_HTML = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>XBud AI Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&family=Outfit:wght@400;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #050505;
            --glass-bg: rgba(255, 255, 255, 0.03);
            --glass-border: rgba(255, 255, 255, 0.08);
            --accent: #00E5FF;
            --accent-gradient: linear-gradient(135deg, #00E5FF 0%, #0077FF 100%);
            --text-main: #FFFFFF;
            --text-muted: #8892B0;
        }
        body {
            background-color: var(--bg-color); color: var(--text-main); font-family: 'Inter', sans-serif;
            margin: 0; padding: 40px 20px; min-height: 100vh; display: flex; flex-direction: column; align-items: center;
            background-image: radial-gradient(circle at 15% 50%, rgba(0, 229, 255, 0.05), transparent 25%), radial-gradient(circle at 85% 30%, rgba(0, 119, 255, 0.05), transparent 25%);
            background-attachment: fixed;
        }
        .container { width: 100%; max-width: 1000px; }
        h2 {
            font-family: 'Outfit', sans-serif; text-align: center; font-size: 2.5em; letter-spacing: 2px; margin-bottom: 40px;
            background: var(--accent-gradient); -webkit-background-clip: text; -webkit-text-fill-color: transparent;
        }
        .card {
            background: var(--glass-bg); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); border: 1px solid var(--glass-border);
            padding: 30px; border-radius: 20px; margin-bottom: 30px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3); transition: transform 0.3s ease, box-shadow 0.3s ease;
        }
        .card:hover { transform: translateY(-2px); box-shadow: 0 12px 40px 0 rgba(0, 229, 255, 0.1); }
        .status-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; }
        .status-title { font-size: 1.2em; color: var(--text-muted); font-weight: 600; }
        .status-badge {
            background: rgba(0, 229, 255, 0.1); color: var(--accent); padding: 8px 16px; border-radius: 50px;
            font-weight: 600; font-size: 0.9em; border: 1px solid rgba(0, 229, 255, 0.2); animation: pulse 2s infinite;
        }
        @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(0, 229, 255, 0.4); } 70% { box-shadow: 0 0 0 10px rgba(0, 229, 255, 0); } 100% { box-shadow: 0 0 0 0 rgba(0, 229, 255, 0); } }
        .transcript-box {
            background: rgba(0,0,0,0.4); border: 1px solid var(--glass-border); padding: 25px; border-radius: 12px;
            min-height: 120px; font-size: 1.1em; line-height: 1.6; color: #E2E8F0;
        }
        .section-title { font-family: 'Outfit', sans-serif; font-size: 1.5em; margin: 0 0 20px 0; color: #FFF; }
        .action-item { background: rgba(255, 255, 255, 0.02); padding: 20px; border-left: 4px solid var(--accent); margin-bottom: 15px; border-radius: 0 12px 12px 0; transition: all 0.2s ease; }
        .action-item:hover { background: rgba(255, 255, 255, 0.05); border-left-color: #0077FF; }
        .action-title { font-size: 1.2em; font-weight: 600; margin-bottom: 8px; }
        .action-meta { display: flex; gap: 15px; color: var(--text-muted); font-size: 0.9em; }
        .meta-tag { background: rgba(255,255,255,0.05); padding: 4px 10px; border-radius: 6px; }
        .priority-high { color: #FF4D4D; border: 1px solid rgba(255,77,77,0.3); }
        .priority-normal { color: #00E5FF; border: 1px solid rgba(0,229,255,0.3); }
    </style>
</head>
<body>
    <div class="container">
        <h2>EAR-BRAIN INTELLIGENCE</h2>
        <div class="card"><div class="status-header"><div class="status-title">System Status</div><div class="status-badge" id="status">Awaiting Local Client...</div></div></div>
        <div class="card">
            <h3 class="section-title">Live Transcript</h3>
            <div class="transcript-box" id="transcript"><span style="color: var(--text-muted); font-style: italic;">Open http://127.0.0.1:8000/client to begin local audio streaming...</span></div>
        </div>
        <div class="card">
            <h3 class="section-title">Extracted Action Items</h3>
            <div id="action-feed"><div style="color: var(--text-muted); padding: 20px; text-align: center; border: 1px dashed var(--glass-border); border-radius: 8px;">Listening for actionable commitments...</div></div>
        </div>
    </div>
    <script>
        let lastActionCount = 0;
        setInterval(async () => {
            try {
                const res = await fetch('/api/data'); const data = await res.json();
                const statusEl = document.getElementById('status'); statusEl.innerText = data.status;
                if(data.status.includes('Active') || data.status.includes('Streaming')) {
                    statusEl.style.animation = 'pulse 2s infinite'; statusEl.style.background = 'rgba(0, 229, 255, 0.1)'; statusEl.style.color = '#00E5FF'; statusEl.style.borderColor = 'rgba(0, 229, 255, 0.2)';
                } else {
                    statusEl.style.animation = 'none'; statusEl.style.background = 'rgba(255, 255, 255, 0.05)'; statusEl.style.color = '#8892B0'; statusEl.style.borderColor = 'rgba(255, 255, 255, 0.1)';
                }
                if (data.transcript && data.transcript.trim() !== "") document.getElementById('transcript').innerText = data.transcript;
                if (data.action_items && data.action_items.length > 0) {
                    if (data.action_items.length !== lastActionCount) {
                        let html = '';
                        data.action_items.forEach(item => {
                            const pClass = item.priority.toLowerCase() === 'high' ? 'priority-high' : 'priority-normal';
                            html += `<div class="action-item" style="animation: slideIn 0.3s ease-out;"><div class="action-title">${item.what}</div><div class="action-meta"><span class="meta-tag">👤 ${item.who}</span><span class="meta-tag">⏰ ${item.when}</span><span class="meta-tag ${pClass}">⚡ ${item.priority.toUpperCase()}</span></div></div>`;
                        });
                        document.getElementById('action-feed').innerHTML = html; lastActionCount = data.action_items.length;
                        if(!document.getElementById('dynamic-styles')) {
                            const style = document.createElement('style'); style.id = 'dynamic-styles'; style.innerHTML = `@keyframes slideIn { from { opacity: 0; transform: translateX(-20px); } to { opacity: 1; transform: translateX(0); } }`; document.head.appendChild(style);
                        }
                    }
                }
            } catch (e) { console.error("Dashboard fetch error:", e); }
        }, 1000);
    </script>
</body>
</html>
"""

CLIENT_HTML = """
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>XBud AI Audio Client</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&family=Outfit:wght@400;700&display=swap" rel="stylesheet">
    <style>
        :root { --bg-color: #050505; --glass-bg: rgba(255, 255, 255, 0.03); --glass-border: rgba(255, 255, 255, 0.08); --accent: #00E5FF; --accent-gradient: linear-gradient(135deg, #00E5FF 0%, #0077FF 100%); --text-main: #FFFFFF; --text-muted: #8892B0; }
        body { font-family: 'Inter', sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; background-color: var(--bg-color); color: var(--text-main); margin: 0; background-image: radial-gradient(circle at 50% 50%, rgba(0, 229, 255, 0.03), transparent 40%); }
        .card { background: var(--glass-bg); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); border: 1px solid var(--glass-border); padding: 40px; border-radius: 20px; text-align: center; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3); max-width: 500px; width: 90%; }
        h1 { font-family: 'Outfit', sans-serif; margin-top: 0; background: var(--accent-gradient); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        p { color: var(--text-muted); line-height: 1.6; margin-bottom: 30px; }
        .btn { padding: 16px 32px; font-size: 1.1em; font-family: 'Outfit', sans-serif; font-weight: 700; border: none; border-radius: 12px; cursor: pointer; transition: all 0.3s ease; text-transform: uppercase; letter-spacing: 1px; width: 100%; }
        .start { background: var(--accent-gradient); color: #000; box-shadow: 0 4px 15px rgba(0, 229, 255, 0.3); }
        .start:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0, 229, 255, 0.4); }
        .stop { background: rgba(255, 77, 77, 0.1); color: #FF4D4D; border: 1px solid rgba(255, 77, 77, 0.3); display: none; }
        .stop:hover { background: rgba(255, 77, 77, 0.2); transform: translateY(-2px); }
        .status { margin-top: 25px; font-size: 0.95em; color: var(--text-muted); font-weight: 600; padding: 10px; border-radius: 8px; background: rgba(255, 255, 255, 0.02); border: 1px solid transparent; }
        .status.active { color: var(--accent); background: rgba(0, 229, 255, 0.05); border-color: rgba(0, 229, 255, 0.2); animation: pulse 2s infinite; }
        @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(0, 229, 255, 0.2); } 70% { box-shadow: 0 0 0 10px rgba(0, 229, 255, 0); } 100% { box-shadow: 0 0 0 0 rgba(0, 229, 255, 0); } }
    </style>
</head>
<body>
    <div class="card">
        <h1>Offline Audio Capture</h1>
        <p>This simulates the XBud hardware sending live audio chunks to the local Python server via WebSocket, operating entirely without internet.</p>
        <button class="btn start" id="startBtn" onclick="startStreaming()">Start Streaming</button>
        <button class="btn stop" id="stopBtn" onclick="stopStreaming()">Stop Streaming</button>
        <div class="status" id="statusText">Ready to connect...</div>
    </div>
    <script>
        let ws; let audioContext; let processor; let input; window.audioProcessor = null;
        async function startStreaming() {
            const statusText = document.getElementById("statusText");
            try {
                const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
                ws = new WebSocket(`ws://${window.location.host}/stream`);
                ws.onopen = () => {
                    document.getElementById("startBtn").style.display = "none"; document.getElementById("stopBtn").style.display = "block";
                    statusText.innerText = "Streaming audio to AI pipeline..."; statusText.className = "status active";
                    audioContext = new AudioContext({ sampleRate: 8000 });
                    input = audioContext.createMediaStreamSource(stream);
                    processor = audioContext.createScriptProcessor(4096, 1, 1);
                    window.audioProcessor = processor;
                    processor.onaudioprocess = (e) => {
                        if (ws.readyState === WebSocket.OPEN) {
                            const floatData = e.inputBuffer.getChannelData(0); const pcmData = new Int16Array(floatData.length);
                            for (let i = 0; i < floatData.length; i++) {
                                let s = Math.max(-1, Math.min(1, floatData[i])); pcmData[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
                            }
                            let binary = ''; let bytes = new Uint8Array(pcmData.buffer);
                            for (let i = 0; i < bytes.byteLength; i++) binary += String.fromCharCode(bytes[i]);
                            ws.send(JSON.stringify({ event: "media", media: { payload: btoa(binary) } }));
                        }
                    };
                    input.connect(processor); processor.connect(audioContext.destination);
                };
            } catch (err) { statusText.innerText = "Error: " + err.message; console.error(err); }
        }
        function stopStreaming() {
            if (processor) { processor.disconnect(); input.disconnect(); audioContext.close(); }
            if (ws) { ws.send(JSON.stringify({ event: "stop" })); ws.close(); }
            document.getElementById("startBtn").style.display = "block"; document.getElementById("stopBtn").style.display = "none";
            document.getElementById("statusText").innerText = "Stream stopped."; document.getElementById("statusText").className = "status";
        }
    </script>
</body>
</html>
"""

# ======================================================================
# MODEL DOWNLOADER
# ======================================================================

def ensure_model():
    model_dir = "model"
    if os.path.exists(model_dir):
        return True
    
    print("Vosk model not found locally. Attempting to download (~40MB)...")
    url = "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
    zip_path = "vosk-model.zip"
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req) as response, open(zip_path, 'wb') as out_file:
            print("Downloading...")
            out_file.write(response.read())
            
        print("Extracting model...")
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(".")
        
        extracted_folder = "vosk-model-small-en-us-0.15"
        os.rename(extracted_folder, model_dir)
        os.remove(zip_path)
        print("Model downloaded and extracted successfully!")
        return True
    except Exception as e:
        print(f"Error downloading model: {e}")
        return False

# ======================================================================
# SERVER LOGIC
# ======================================================================

app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

latest_data = {
    "status": "Awaiting Local Connection...",
    "transcript": "",
    "action_items": []
}

rec = None
if HAS_VOSK and ensure_model():
    try:
        vosk_model = Model("model")
        rec = KaldiRecognizer(vosk_model, 8000)
    except Exception as e:
        print(f"Failed to initialize Vosk: {e}")

def scrub_pii(text):
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def extract_actions_mock(text):
    global latest_data
    lower = text.lower()
    if any(x in lower for x in ["send", "need", "urgent", "call", "schedule", "remind"]):
        priority = "high" if any(w in lower for w in ["urgent", "asap", "immediately"]) else "normal"
        latest_data["action_items"].insert(0, {
            "who": "Local User",
            "what": text.capitalize(),
            "when": "ASAP",
            "priority": priority
        })
        latest_data["action_items"] = latest_data["action_items"][:5]

def process_offline_audio(pcm_data):
    global latest_data
    if not rec:
        import random
        text = random.choice(["Mocked local offline text.", "Vosk model not loaded.", "Testing the UI updates."])
        latest_data["transcript"] = text
        extract_actions_mock(text)
        return

    if rec.AcceptWaveform(pcm_data):
        res = json.loads(rec.Result())
        text = res.get('text', '')
        if text:
            scrubbed = scrub_pii(text)
            latest_data["transcript"] = scrubbed
            extract_actions_mock(scrubbed)
    else:
        res = json.loads(rec.PartialResult())
        partial_text = res.get('partial', '')
        if partial_text:
            latest_data["transcript"] = scrub_pii(partial_text)

@app.websocket("/stream")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    latest_data["status"] = "Call Active (Local Streaming)"
    try:
        while True:
            message = await websocket.receive_text()
            data = json.loads(message)
            if data.get('event') == 'media':
                pcm_bytes = base64.b64decode(data['media']['payload'])
                await asyncio.to_thread(process_offline_audio, pcm_bytes)
            elif data.get('event') == 'stop':
                latest_data["status"] = "Call Ended (Local)"
                break
    except Exception as e:
        latest_data["status"] = "Client Disconnected"

@app.get("/api/data")
async def get_data():
    return latest_data

@app.get("/")
async def get_dashboard():
    return HTMLResponse(content=DASHBOARD_HTML)

@app.get("/client")
async def get_client():
    return HTMLResponse(content=CLIENT_HTML)

if __name__ == "__main__":
    print("=========================================================")
    print("XBud Local Offline Server Started")
    print("Dashboard: http://127.0.0.1:8000/")
    print("Audio Capture Client: http://127.0.0.1:8000/client")
    print("=========================================================")
    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="warning")
