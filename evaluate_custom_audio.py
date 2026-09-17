import os
import sys
import time
import re
import json

sys.stdout.reconfigure(encoding='utf-8')

# Auto-install dependencies if missing
try:
    import whisper
    import imageio_ffmpeg
    import torch
except ImportError:
    import subprocess
    print("Installing required packages (whisper, imageio-ffmpeg, torch)...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "openai-whisper", "imageio-ffmpeg", "torch", "psutil"])
    import whisper
    import imageio_ffmpeg
    import torch

import psutil

# Add imageio-ffmpeg executable to PATH so Whisper can load MP3s without system ffmpeg
ffmpeg_path = os.path.dirname(imageio_ffmpeg.get_ffmpeg_exe())
os.environ["PATH"] = ffmpeg_path + os.pathsep + os.environ.get("PATH", "")

# Files to process
AUDIO_FILES = [
    r"c:\Ankshit\project_xbud\test_dl.m4a"
]

def scrub_pii(text):
    """Basic Regex-based PII scrubber simulating the DistilBERT hybrid pipeline."""
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b\d{4}\s\d{4}\s\d{4}\b', '[ID_CARD]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def main():
    print("=" * 70)
    print(" Ear-Brain Audio Evaluation")
    print("=" * 70)
    
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Loading Whisper model on {device}...")
    
    # Load model and measure RAM impact
    process = psutil.Process()
    ram_before = process.memory_info().rss / (1024 * 1024)
    
    model = whisper.load_model("base", device=device)
    
    ram_after = process.memory_info().rss / (1024 * 1024)
    model_ram = ram_after - ram_before
    print(f"[Model Loaded] RAM Footprint: {model_ram:.1f} MB")
    
    results = []

    for file_path in AUDIO_FILES:
        print(f"\n--- Processing: {os.path.basename(file_path)} ---")
        if not os.path.exists(file_path):
            print(f"ERROR: File not found -> {file_path}")
            continue
            
        try:
            # 1. Load Audio (also getting duration)
            audio = whisper.load_audio(file_path)
            duration_sec = len(audio) / 16000.0
            print(f"Audio Duration: {duration_sec:.2f} seconds")
            
            # 2. Run ASR
            start_time = time.time()
            result = model.transcribe(audio, language="en")
            asr_time = time.time() - start_time
            
            # 3. Process Transcript
            raw_transcript = result["text"].strip()
            scrubbed_transcript = scrub_pii(raw_transcript)
            
            # 4. Calculate Metrics
            rtf = asr_time / duration_sec if duration_sec > 0 else 0
            
            print(f"ASR Inference Time: {asr_time:.2f}s")
            print(f"Real-Time Factor (RTF): {rtf:.3f}x (Lower is better, < 1.0 is realtime)")
            print("\n[RAW TRANSCRIPT]")
            print(raw_transcript)
            
            if scrubbed_transcript != raw_transcript:
                print("\n[SCRUBBED TRANSCRIPT]")
                print(scrubbed_transcript)
            else:
                print("\n[NER Status: No PII Detected]")
                
            results.append({
                "file": os.path.basename(file_path),
                "duration_s": round(duration_sec, 2),
                "asr_time_s": round(asr_time, 2),
                "rtf": round(rtf, 3),
                "transcript": scrubbed_transcript
            })
            
        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    # Print Summary Matrix
    print("\n" + "=" * 70)
    print(" Performance Matrix (Specific Files)")
    print("=" * 70)
    print("Note: Exact 'Accuracy/WER' requires a ground-truth reference text.")
    print("The transcriptions above demonstrate the subjective accuracy.\n")
    
    print(f"| {'Filename':<40} | {'Duration':<10} | {'Latency':<10} | {'RTF':<8} |")
    print(f"| {'-'*40} | {'-'*10} | {'-'*10} | {'-'*8} |")
    for r in results:
        fname = r['file']
        if len(fname) > 37:
            fname = fname[:34] + "..."
        print(f"| {fname:<40} | {r['duration_s']:<8.2f}s | {r['asr_time_s']:<8.2f}s | {r['rtf']:<8.3f} |")
    
    print("=" * 70)

if __name__ == "__main__":
    main()
