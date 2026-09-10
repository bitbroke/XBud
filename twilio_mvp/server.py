import os
import json
import base64
import asyncio
import re
from io import BytesIO
from fastapi import FastAPI, WebSocket, Request
from fastapi.responses import HTMLResponse, PlainTextResponse
from fastapi.middleware.cors import CORSMiddleware

try:
    import audioop
    import speech_recognition as sr
    HAS_AUDIO_DEPS = True
except ImportError:
    HAS_AUDIO_DEPS = False
    print("WARNING: Audio dependencies missing. Falling back to mocked AI processing.")

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global State for the Dashboard
latest_data = {
    "status": "Awaiting Call...",
    "transcript": "",
    "action_items": []
}

def scrub_pii(text):
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def process_audio_buffer(pcm_data):
    """Processes 16-bit 8000Hz PCM data using SpeechRecognition or mocks it"""
    global latest_data
    
    if not HAS_AUDIO_DEPS:
        # Mocked processing because dependencies failed to install
        import random
        responses = [
            "Hello, I am calling about the X-Bud demo.",
            "The system is currently analyzing your voice in real time.",
            "I can clearly hear the audio streaming over the WebSocket.",
            "This is a fantastic proof of concept for our architecture.",
            "Action item: follow up with the team regarding the successful test."
        ]
        text = random.choice(responses)
        print(f"[MOCKED TRANSCRIPT] {text}")
        latest_data["transcript"] = text
        latest_data["action_items"] = [{
            "who": "System",
            "what": "Install missing audio dependencies",
            "when": "ASAP",
            "priority": "high"
        }]
        return

    recognizer = sr.Recognizer()
    
    # Create an in-memory WAV file for SpeechRecognition
    import wave
    wav_io = BytesIO()
    with wave.open(wav_io, 'wb') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2) # 16-bit
        wav_file.setframerate(8000)
        wav_file.writeframes(pcm_data)
    
    wav_io.seek(0)
    
    try:
        with sr.AudioFile(wav_io) as source:
            audio = recognizer.record(source)
            text = recognizer.recognize_google(audio)
            if text:
                print(f"[TRANSCRIPT] {text}")
                scrubbed = scrub_pii(text)
                latest_data["transcript"] = scrubbed
                
                # Mock SLM extraction
                lower = scrubbed.lower()
                if any(x in lower for x in ["send", "need", "urgent", "call"]):
                    latest_data["action_items"].insert(0, {
                        "who": "Caller",
                        "what": scrubbed.capitalize(),
                        "when": "ASAP",
                        "priority": "high" if "urgent" in lower else "normal"
                    })
    except Exception as e:
        pass


@app.post("/answer")
async def answer_call(request: Request):
    """TwiML Webhook to answer incoming calls and start Media Stream"""
    host = request.headers.get("host")
    
    # Instruct Twilio to connect a WebSocket stream
    twiml = f"""<?xml version="1.0" encoding="UTF-8"?>
    <Response>
        <Say>Welcome to the Ear-Brain AI test line. Your call is being processed in real time.</Say>
        <Connect>
            <Stream url="wss://{host}/stream" />
        </Connect>
        <Pause length="40" />
    </Response>
    """
    return PlainTextResponse(content=twiml, media_type="text/xml")


@app.websocket("/stream")
async def websocket_endpoint(websocket: WebSocket):
    """Receives live audio from Twilio"""
    await websocket.accept()
    print("[SERVER] Twilio connected to Media Stream.")
    global latest_data
    latest_data["status"] = "Call Active (Streaming)"
    
    audio_buffer = bytearray()
    
    try:
        while True:
            message = await websocket.receive_text()
            data = json.loads(message)
            
            if data['event'] == 'media':
                payload = data['media']['payload']
                mu_law_bytes = base64.b64decode(payload)
                
                if HAS_AUDIO_DEPS:
                    pcm_bytes = audioop.ulaw2lin(mu_law_bytes, 2)
                    audio_buffer.extend(pcm_bytes)
                else:
                    audio_buffer.extend(b'\x00' * 160) # dummy bytes
                
                if len(audio_buffer) >= 24000:
                    chunk_to_process = bytes(audio_buffer)
                    audio_buffer.clear()
                    
                    asyncio.create_task(asyncio.to_thread(process_audio_buffer, chunk_to_process))
                    
            elif data['event'] == 'stop':
                print("[SERVER] Twilio stream stopped.")
                latest_data["status"] = "Call Ended"
                break
                
    except Exception as e:
        print(f"[WS ERROR] {e}")
        latest_data["status"] = "Call Disconnected"


@app.get("/api/data")
async def get_data():
    """Endpoint for the Dashboard UI"""
    return latest_data

@app.get("/")
async def get_dashboard():
    """Serve the static HTML dashboard"""
    try:
        # Resolve the absolute path to the directory containing server.py
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
