import http.server
import socketserver
import json
import threading
import time
import os
import re
import sys
import subprocess

# ==============================================================================
# Ear-Brain Mobile Companion Server
# ==============================================================================

PORT = 8080
latest_data = {
    "status": "Listening for speech...",
    "transcript": "",
    "action_items": []
}

# --- 1. Mobile Web UI (Looks exactly like our Flutter App) ---
HTML_UI = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Ear-Brain Companion</title>
    <style>
        body {
            background-color: #0A0A0A;
            color: white;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            margin: 0;
            padding: 16px;
        }
        .header {
            text-align: center;
            font-size: 20px;
            font-weight: 600;
            letter-spacing: 1.2px;
            padding: 16px 0 24px 0;
            color: white;
            border-bottom: 1px solid #1E1E1E;
            margin-bottom: 24px;
        }
        .card {
            background-color: #1E1E1E;
            border-radius: 16px;
            padding: 20px;
            margin-bottom: 24px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
            text-align: center;
        }
        .status {
            color: #00E5FF;
            font-size: 16px;
            margin-top: 12px;
        }
        .section-title {
            font-size: 12px;
            font-weight: bold;
            color: #888;
            letter-spacing: 1.5px;
            margin-bottom: 12px;
        }
        .transcript-box {
            background-color: #141414;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 12px;
            font-size: 14px;
            color: #CCC;
            min-height: 50px;
            margin-bottom: 24px;
            font-style: italic;
        }
        .action-item {
            background-color: #1E1E1E;
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 12px;
            border-left: 4px solid #00E5FF;
            text-align: left;
        }
        .action-title {
            font-size: 16px;
            font-weight: bold;
            margin-bottom: 4px;
        }
        .action-subtitle {
            font-size: 13px;
            color: #AAA;
        }
    </style>
</head>
<body>
    <div class="header">EAR-BRAIN</div>
    
    <div class="card">
        <div style="font-size: 48px;">🎧</div>
        <div class="status" id="status-text">Connected to Case</div>
    </div>

    <div class="section-title">LIVE TRANSCRIPT (SCRUBBED)</div>
    <div class="transcript-box" id="transcript-text">Waiting for speech...</div>

    <div class="section-title">INTELLIGENCE FEED</div>
    <div id="action-feed">
        <div style="text-align: center; color: #555; padding: 20px;">No action items detected yet.</div>
    </div>

    <script>
        async function pollData() {
            try {
                const response = await fetch('/api/data');
                const data = await response.json();
                
                document.getElementById('status-text').innerText = data.status;
                if(data.transcript) {
                    document.getElementById('transcript-text').innerText = '"' + data.transcript + '"';
                }
                
                if (data.action_items.length > 0) {
                    let html = '';
                    data.action_items.forEach(item => {
                        let color = item.priority === 'high' ? '#FF4444' : '#00E5FF';
                        html += `
                            <div class="action-item" style="border-left-color: ${color}">
                                <div class="action-title">${item.what}</div>
                                <div class="action-subtitle">Assignee: ${item.who} • Due: ${item.when}</div>
                            </div>
                        `;
                    });
                    document.getElementById('action-feed').innerHTML = html;
                }
            } catch (e) {
                document.getElementById('status-text').innerText = "Disconnected from Server";
                document.getElementById('status-text').style.color = "#FF4444";
            }
        }
        
        // Poll every 1 second
        setInterval(pollData, 1000);
    </script>
</body>
</html>
"""

# --- 2. HTTP Server ---
class RequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(HTML_UI.encode('utf-8'))
        elif self.path == '/api/data':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            # Add CORS headers so phone can access it
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(latest_data).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass  # Suppress logging to keep console clean

def start_server():
    with socketserver.TCPServer(("0.0.0.0", PORT), RequestHandler) as httpd:
        httpd.serve_forever()

# --- 3. Pipeline Simulator (Runs locally on PC) ---
def scrub_pii(text):
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def simulate_pipeline():
    global latest_data
    
    # Try to load SpeechRecognition if Whisper is not installed
    import speech_recognition as sr
    recognizer = sr.Recognizer()
    
    try:
        mic = sr.Microphone()
    except Exception as e:
        print("Microphone not found. Simulating data feed...")
        # Simulation fallback
        time.sleep(5)
        latest_data["transcript"] = "Send the Q3 report to john.doe@example.com by tomorrow morning."
        latest_data["transcript"] = scrub_pii(latest_data["transcript"])
        latest_data["action_items"] = [{
            "who": "User", "what": "Send Q3 report", "when": "Tomorrow morning", "priority": "high"
        }]
        return

    print("\n[AUDIO] Microphone active. Speak clearly into your PC mic!")
    
    with mic as source:
        recognizer.adjust_for_ambient_noise(source)
        while True:
            latest_data["status"] = "Listening..."
            try:
                audio = recognizer.listen(source, timeout=5, phrase_time_limit=10)
                latest_data["status"] = "Processing..."
                
                # Use Google Web Speech API (Requires no heavy local models/VRAM!)
                text = recognizer.recognize_google(audio)
                
                if text:
                    print(f"\n[RAW] {text}")
                    scrubbed = scrub_pii(text)
                    latest_data["transcript"] = scrubbed
                    
                    # Simulated SLM extraction
                    lower_text = scrubbed.lower()
                    if "send" in lower_text or "call" in lower_text or "need to" in lower_text:
                        latest_data["action_items"].insert(0, {
                            "who": "User",
                            "what": scrubbed.capitalize(),
                            "when": "ASAP",
                            "priority": "high" if "urgent" in lower_text else "normal"
                        })
                        print(f"[SLM] Extracted action item!")
                        
            except sr.WaitTimeoutError:
                pass
            except sr.UnknownValueError:
                pass
            except Exception as e:
                print(f"[ERR] {e}")
                time.sleep(1)

if __name__ == "__main__":
    # Check dependencies
    try:
        import speech_recognition
        import pyaudio
    except ImportError:
        import subprocess
        print("Installing lightweight audio dependencies (no heavy AI models required)...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "SpeechRecognition", "pyaudio"])

    # Start Web UI server in background
    threading.Thread(target=start_server, daemon=True).start()
    
    print("=" * 60)
    print(" EAR-BRAIN MOBILE TESTING DEPLOYMENT")
    print("=" * 60)
    print("1. Ensure your Mobile Phone is connected to the SAME Wi-Fi network as this PC.")
    print("2. Open your phone's web browser (Safari/Chrome).")
    print("3. Type this EXACT address into the URL bar on your phone:")
    print(f"\n      ---->    http://10.208.240.207:8080    <----\n")
    print("4. Speak into your PC's microphone to see the Intelligence Feed update live on your phone!")
    print("=" * 60)
    
    simulate_pipeline()
