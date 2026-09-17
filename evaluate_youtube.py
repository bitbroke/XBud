import os
import sys
import time
import re

try:
    import whisper
    import imageio_ffmpeg
    import torch
    import yt_dlp
except ImportError:
    import subprocess
    print("Installing required packages (whisper, imageio-ffmpeg, torch, yt-dlp)...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "openai-whisper", "imageio-ffmpeg", "torch", "psutil", "yt-dlp"])
    import whisper
    import imageio_ffmpeg
    import torch
    import yt_dlp

import psutil

# Add local directory to PATH so Whisper can find ffmpeg.exe
os.environ["PATH"] = os.getcwd() + os.pathsep + os.environ.get("PATH", "")

# Default YouTube URLs to test (short informative clips)
YOUTUBE_URLS = [
    "ytsearch1:India's Got Latent episode", # Dynamically fetches the first search result
]

def scrub_pii(text):
    """Basic Regex-based PII scrubber simulating the DistilBERT hybrid pipeline."""
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b\d{4}\s\d{4}\s\d{4}\b', '[ID_CARD]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text

def download_youtube_audio(url, output_path="downloaded_audio"):
    """Downloads audio from a youtube video"""
    import glob
    # Clean up previous downloads with this prefix
    for f in glob.glob(output_path + ".*"):
        try: os.remove(f)
        except: pass
        
    ydl_opts = {
        'format': 'bestaudio[ext=m4a]/bestaudio/best',
        'outtmpl': output_path + '.%(ext)s',
        'quiet': True,
        'no_warnings': True,
    }
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        print(f"Downloading audio from {url}...")
        ydl.extract_info(url, download=True)
        files = glob.glob(output_path + ".*")
        if files:
            return files[0]
        return output_path + ".m4a"

def main():
    print("=" * 70)
    print(" Ear-Brain YouTube Audio Evaluation")
    print("=" * 70)
    
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Loading Whisper model on {device}...")
    
    process = psutil.Process()
    ram_before = process.memory_info().rss / (1024 * 1024)
    
    model = whisper.load_model("tiny.en", device=device)
    
    ram_after = process.memory_info().rss / (1024 * 1024)
    model_ram = ram_after - ram_before
    print(f"[Model Loaded] RAM Footprint: {model_ram:.1f} MB")
    
    results = []

    for i, url in enumerate(YOUTUBE_URLS):
        print(f"\n--- Processing Video {i+1}: {url} ---")
        try:
            # 1. Download audio from YouTube
            audio_file = download_youtube_audio(url, f"yt_audio_{i}")
            if not os.path.exists(audio_file):
                print(f"ERROR: Downloaded file not found -> {audio_file}")
                continue
            
            print(f"Audio downloaded to: {audio_file}")
            
            # 2. Load Audio (also getting duration)
            audio = whisper.load_audio(audio_file)
            duration_sec = len(audio) / 16000.0
            print(f"Audio Duration: {duration_sec:.2f} seconds")
            
            # 3. Run ASR
            start_time = time.time()
            result = model.transcribe(audio, language="en")
            asr_time = time.time() - start_time
            
            # 4. Process Transcript
            raw_transcript = result["text"].strip()
            scrubbed_transcript = scrub_pii(raw_transcript)
            
            # 5. Calculate Metrics
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
                "url": url,
                "duration_s": round(duration_sec, 2),
                "asr_time_s": round(asr_time, 2),
                "rtf": round(rtf, 3),
            })
            
            # Cleanup downloaded file
            os.remove(audio_file)
            
        except Exception as e:
            print(f"Error processing {url}: {e}")

    # Print Summary Matrix
    print("\n" + "=" * 70)
    print(" Performance Matrix (YouTube Files)")
    print("=" * 70)
    
    print(f"| {'URL':<40} | {'Duration':<10} | {'Latency':<10} | {'RTF':<8} |")
    print(f"| {'-'*40} | {'-'*10} | {'-'*10} | {'-'*8} |")
    for r in results:
        url_disp = r['url']
        if len(url_disp) > 37:
            url_disp = url_disp[:34] + "..."
        print(f"| {url_disp:<40} | {r['duration_s']:<8.2f}s | {r['asr_time_s']:<8.2f}s | {r['rtf']:<8.3f} |")
    
    print("=" * 70)

if __name__ == "__main__":
    main()
