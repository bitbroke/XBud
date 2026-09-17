import os
import json
import base64
import asyncio
import re
from fastapi import FastAPI, WebSocket, Request
from fastapi.responses import HTMLResponse, PlainTextResponse
from fastapi.middleware.cors import CORSMiddleware

try:
    from vosk import Model, KaldiRecognizer
    HAS_VOSK = True
except ImportError:
    HAS_VOSK = False
    print("WARNING: Vosk not installed. Please install it for offline transcription.")

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global State for the Dashboard
latest_data = {
    "status": "Awaiting Local Connection...",
    "transcript": "",
    "action_items": []
}

# Initialize Vosk
if HAS_VOSK:
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        model_path = os.path.join(os.path.dirname(current_dir), "model")
        if os.path.exists(model_path):
            print(f"Loading Vosk model from {model_path}...")
            vosk_model = Model(model_path)
            # We use 8000 Hz because client.html captures at 8000 Hz
            rec = KaldiRecognizer(vosk_model, 8000)
            print("Vosk model loaded successfully.")
        else:
            HAS_VOSK = False
            print(f"WARNING: Vosk model not found at {model_path}. Download and extract it to a folder named 'model'.")
    except Exception as e:
        HAS_VOSK = False
        print(f"WARNING: Failed to load Vosk model: {e}")

def scrub_pii(text):
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def extract_actions_mock(text):
    """Simple offline mock for SLM extraction"""
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
        # Keep only the latest 5 actions
        latest_data["action_items"] = latest_data["action_items"][:5]

def process_offline_audio(pcm_data):
    """Processes 16-bit 8000Hz PCM data using local Vosk model"""
    global latest_data
    
    if not HAS_VOSK:
        # Fallback Mock
        import random
        text = random.choice([
            "Hello, this is a local offline test.",
            "Testing the completely disconnected pipeline.",
            "Action item: confirm offline functionality.",
            "The system is processing audio without the internet."
        ])
        print(f"[MOCKED TRANSCRIPT] {text}")
        latest_data["transcript"] = text
        extract_actions_mock(text)
        return

    # Feed data to Vosk
    if rec.AcceptWaveform(pcm_data):
        res = json.loads(rec.Result())
        text = res.get('text', '')
        if text:
            print(f"[TRANSCRIPT] {text}")
            scrubbed = scrub_pii(text)
            latest_data["transcript"] = scrubbed
            extract_actions_mock(scrubbed)
    else:
        # Update with partial transcript for real-time feel
        res = json.loads(rec.PartialResult())
        partial_text = res.get('partial', '')
        if partial_text:
            latest_data["transcript"] = scrubbed_partial = scrub_pii(partial_text)

@app.websocket("/stream")
async def websocket_endpoint(websocket: WebSocket):
    """Receives live audio from the local browser client"""
    await websocket.accept()
    print("[SERVER] Local client connected to Media Stream.")
    global latest_data
    latest_data["status"] = "Call Active (Local Streaming)"
    
    try:
        while True:
            message = await websocket.receive_text()
            data = json.loads(message)
            
            if data.get('event') == 'media':
                payload = data['media']['payload']
                pcm_bytes = base64.b64decode(payload)
                
                # Send to processing (running in thread to avoid blocking WebSocket)
                await asyncio.to_thread(process_offline_audio, pcm_bytes)
                    
            elif data.get('event') == 'stop':
                print("[SERVER] Local stream stopped.")
                latest_data["status"] = "Call Ended (Local)"
                break
                
    except Exception as e:
        print(f"[WS ERROR] {e}")
        latest_data["status"] = "Client Disconnected"

@app.get("/api/data")
async def get_data():
    """Endpoint for the Dashboard UI"""
    return latest_data

@app.get("/")
async def get_dashboard():
    """Serve the static HTML dashboard"""
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        dashboard_path = os.path.join(current_dir, "dashboard.html")
        with open(dashboard_path, "r") as f:
            html = f.read()
            return HTMLResponse(content=html)
    except FileNotFoundError:
        return PlainTextResponse(content="Dashboard HTML not found.")

@app.get("/client")
async def get_client():
    """Serve the static HTML client"""
    try:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        client_path = os.path.join(current_dir, "client.html")
        with open(client_path, "r") as f:
            html = f.read()
            return HTMLResponse(content=html)
    except FileNotFoundError:
        return PlainTextResponse(content="Client HTML not found.")
