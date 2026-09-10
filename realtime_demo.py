import queue
import sys
import threading
import time
import re
import json

try:
    import numpy as np
    import sounddevice as sd
    from transformers import pipeline
except ImportError:
    print("Missing dependencies! Please run: pip install -r requirements.txt")
    sys.exit(1)

# ==============================================================================
# Ear-Brain Realtime Simulator Pipeline
# ==============================================================================

# Audio settings
SAMPLE_RATE = 16000
CHUNK_DURATION = 3  # seconds per chunk
CHUNK_SAMPLES = SAMPLE_RATE * CHUNK_DURATION

audio_queue = queue.Queue()

# --- 1. Audio Capture Thread ---
def audio_callback(indata, frames, time_info, status):
    """Callback for sounddevice to capture audio chunks."""
    if status:
        print(f"Audio Status: {status}", file=sys.stderr)
    # Put a copy of the audio data in the queue
    audio_queue.put(indata.copy())

def start_audio_stream():
    print("[audio] Starting microphone stream (16kHz, Mono)...")
    stream = sd.InputStream(
        samplerate=SAMPLE_RATE,
        channels=1,
        dtype='float32',
        callback=audio_callback,
        blocksize=CHUNK_SAMPLES
    )
    return stream

# --- 2. VAD (Voice Activity Detection) Mock ---
def is_speech(audio_chunk, threshold=0.01):
    """Simple energy-based VAD for the demo."""
    energy = np.mean(np.abs(audio_chunk))
    return energy > threshold

# --- 3. NER / PII Scrubber Mock ---
def scrub_pii(text):
    """Basic Regex-based PII scrubber simulating the DistilBERT hybrid pipeline."""
    # Redact phone numbers
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    # Redact Aadhaar/SSN formats
    text = re.sub(r'\b\d{4}\s\d{4}\s\d{4}\b', '[ID_CARD]', text)
    # Redact email addresses
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

# --- 4. SLM JSON Generator Mock ---
def generate_slm_json(transcript):
    """
    Simulates the Gemma 3 1B SLM generating GBNF constrained JSON.
    In a real environment, this runs on the NPU using llama.cpp.
    """
    if not transcript.strip():
        return None

    # Extremely basic mock heuristic to simulate action item extraction
    action_items = []
    lower_text = transcript.lower()
    
    if "will" in lower_text or "need to" in lower_text or "send" in lower_text:
        action_items.append({
            "who": "User",
            "what": transcript.strip(),
            "when": "ASAP",
            "priority": "high",
            "confidence": 0.85
        })

    if not action_items:
        return None

    return json.dumps(action_items, indent=2)

# ==============================================================================
# Main Pipeline Loop
# ==============================================================================
def main():
    print("=" * 60)
    print(" Ear-Brain Realtime Pipeline Simulator")
    print("=" * 60)
    print("Loading ASR Model (Whisper Tiny) - This may take a moment...")
    
    # Load ASR (Whisper Tiny fits comfortably in 4GB VRAM)
    try:
        asr_pipeline = pipeline(
            "automatic-speech-recognition",
            model="openai/whisper-tiny",
            device=0 if np.cuda is not None else -1 # Try GPU if available
        )
        print("[asr] Whisper Tiny loaded successfully.")
    except Exception as e:
        print(f"Error loading ASR model: {e}")
        print("Falling back to CPU if CUDA failed...")
        asr_pipeline = pipeline("automatic-speech-recognition", model="openai/whisper-tiny", device=-1)

    print("\nListening... Speak into your microphone! (Press Ctrl+C to stop)\n")
    
    stream = start_audio_stream()
    
    try:
        with stream:
            while True:
                # Wait for 3 seconds of audio to accumulate
                chunk = audio_queue.get()
                
                # 1. Run VAD
                if not is_speech(chunk):
                    print("[vad] Silence detected, skipping...")
                    continue
                
                print("\n[vad] Speech detected, processing...")
                
                # 2. Transcribe (ASR)
                # Whisper expects 1D float32 array
                audio_1d = chunk.flatten()
                result = asr_pipeline({"sampling_rate": SAMPLE_RATE, "raw": audio_1d}, generate_kwargs={"language": "english"})
                transcript = result["text"].strip()
                
                if not transcript:
                    continue
                
                print(f"[asr] RAW: {transcript}")
                
                # 3. PII Scrubbing (NER)
                scrubbed_transcript = scrub_pii(transcript)
                if scrubbed_transcript != transcript:
                    print(f"[ner] PII SCRUBBED: {scrubbed_transcript}")
                
                # 4. SLM Generation
                slm_output = generate_slm_json(scrubbed_transcript)
                if slm_output:
                    print(f"[slm] ACTION ITEMS EXTRACTED (JSON):")
                    print(slm_output)
                    
    except KeyboardInterrupt:
        print("\n\nStopping Ear-Brain Simulator...")
        sys.exit(0)

if __name__ == "__main__":
    main()
