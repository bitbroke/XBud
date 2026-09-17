"""
evaluate_indicwhisper.py — Evaluate IndicWhisper on Indian audio content.

Uses ai4bharat/indicwhisper-hindi-small (fine-tuned on 10,400+ hrs of Indian audio)
instead of vanilla Whisper base. This is Phase 1 of the accuracy improvement plan.
"""
import os
import sys
import time
import re

sys.stdout.reconfigure(encoding='utf-8')

try:
    import torch
    from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor, pipeline
except ImportError:
    import subprocess
    print("Installing required packages...")
    subprocess.check_call([sys.executable, "-m", "pip", "install",
                           "transformers", "accelerate", "torch", "torchaudio", "psutil"])
    import torch
    from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor, pipeline

import psutil

# Add local directory to PATH for ffmpeg
os.environ["PATH"] = os.getcwd() + os.pathsep + os.environ.get("PATH", "")

# Audio file to evaluate
AUDIO_FILE = r"c:\Ankshit\project_xbud\test_dl.m4a"

# IndicWhisper model — fine-tuned on Indian languages by AI4Bharat
MODEL_ID = "ai4bharat/indicwhisper-hindi-small"

# Fallback: If IndicWhisper is unavailable, use Whisper small with Hindi forced decoding
FALLBACK_MODEL_ID = "openai/whisper-small"


def scrub_pii(text):
    """Basic Regex-based PII scrubber simulating the DistilBERT hybrid pipeline."""
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b\d{4}\s\d{4}\s\d{4}\b', '[ID_CARD]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text


def load_model(device):
    """Load IndicWhisper or fallback to Whisper small with Hindi."""
    process = psutil.Process()
    ram_before = process.memory_info().rss / (1024 * 1024)

    torch_dtype = torch.float16 if torch.cuda.is_available() else torch.float32

    # Try IndicWhisper first
    model_id = MODEL_ID
    try:
        print(f"Attempting to load IndicWhisper: {MODEL_ID}...")
        processor = AutoProcessor.from_pretrained(MODEL_ID)
        model = AutoModelForSpeechSeq2Seq.from_pretrained(
            MODEL_ID,
            torch_dtype=torch_dtype,
            low_cpu_mem_usage=True,
        ).to(device)
        print(f"✅ IndicWhisper loaded successfully!")
    except Exception as e:
        print(f"⚠️  IndicWhisper unavailable ({e})")
        print(f"Falling back to Whisper Small with Hindi language forcing...")
        model_id = FALLBACK_MODEL_ID
        processor = AutoProcessor.from_pretrained(FALLBACK_MODEL_ID)
        model = AutoModelForSpeechSeq2Seq.from_pretrained(
            FALLBACK_MODEL_ID,
            torch_dtype=torch_dtype,
            low_cpu_mem_usage=True,
        ).to(device)
        print(f"✅ Whisper Small loaded as fallback.")

    ram_after = process.memory_info().rss / (1024 * 1024)
    model_ram = ram_after - ram_before
    print(f"[Model Loaded] RAM Footprint: {model_ram:.1f} MB")

    # Build the ASR pipeline
    pipe = pipeline(
        "automatic-speech-recognition",
        model=model,
        tokenizer=processor.tokenizer,
        feature_extractor=processor.feature_extractor,
        torch_dtype=torch_dtype,
        device=device,
    )

    return pipe, model_id, model_ram


def main():
    print("=" * 70)
    print(" Ear-Brain IndicWhisper Evaluation (Phase 1)")
    print("=" * 70)

    device = "cuda:0" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}")

    # Load model
    pipe, model_id, model_ram = load_model(device)

    # Check audio file
    if not os.path.exists(AUDIO_FILE):
        print(f"ERROR: Audio file not found -> {AUDIO_FILE}")
        print("Please run the download first (evaluate_youtube.py or yt-dlp).")
        sys.exit(1)

    print(f"\n--- Processing: {os.path.basename(AUDIO_FILE)} ---")

    # Run ASR with chunked processing for long audio
    print("Running ASR (chunked long-form transcription)...")
    start_time = time.time()

    # Generate kwargs for Hindi language forcing if using fallback
    generate_kwargs = {}
    if "indicwhisper" not in model_id:
        generate_kwargs["language"] = "hi"
        generate_kwargs["task"] = "transcribe"
        print("  [Hindi language forcing enabled for fallback model]")

    result = pipe(
        AUDIO_FILE,
        chunk_length_s=30,          # Process in 30-second chunks
        batch_size=8,               # Parallel chunks on GPU
        return_timestamps=True,     # Get word-level timestamps
        generate_kwargs=generate_kwargs,
    )

    asr_time = time.time() - start_time

    # Get audio duration
    try:
        import whisper
        audio = whisper.load_audio(AUDIO_FILE)
        duration_sec = len(audio) / 16000.0
    except:
        # Estimate from file size (~128kbps audio)
        file_size = os.path.getsize(AUDIO_FILE)
        duration_sec = file_size / (128 * 1024 / 8)

    # Process transcript
    raw_transcript = result["text"].strip()
    scrubbed_transcript = scrub_pii(raw_transcript)

    # Calculate metrics
    rtf = asr_time / duration_sec if duration_sec > 0 else 0

    # Print results
    print(f"\nAudio Duration: {duration_sec:.2f}s ({duration_sec/60:.1f} min)")
    print(f"ASR Inference Time: {asr_time:.2f}s ({asr_time/60:.1f} min)")
    print(f"Real-Time Factor (RTF): {rtf:.3f}x")

    print("\n" + "-" * 70)
    print("[RAW TRANSCRIPT]")
    print("-" * 70)

    # Print first 3000 chars for readability
    if len(raw_transcript) > 3000:
        print(raw_transcript[:3000])
        print(f"\n... [{len(raw_transcript) - 3000} more characters] ...")
    else:
        print(raw_transcript)

    if scrubbed_transcript != raw_transcript:
        print("\n[SCRUBBED TRANSCRIPT]")
        print(scrubbed_transcript[:2000])
    else:
        print("\n[NER Status: No PII Detected]")

    # Print timestamped chunks if available
    if "chunks" in result and result["chunks"]:
        print("\n" + "-" * 70)
        print("[TIMESTAMPED SEGMENTS — First 20]")
        print("-" * 70)
        for i, chunk in enumerate(result["chunks"][:20]):
            ts = chunk.get("timestamp", (None, None))
            start = f"{ts[0]:.1f}s" if ts[0] is not None else "?"
            end = f"{ts[1]:.1f}s" if ts[1] is not None else "?"
            print(f"  [{start} -> {end}] {chunk['text'].strip()}")

    # Spot-check key terms
    print("\n" + "-" * 70)
    print("[ACCURACY SPOT-CHECK — Key Indian Terms]")
    print("-" * 70)
    spot_checks = {
        "India's Got Latent": ["india's got latent", "indias got latent", "india got latent"],
        "Samay Raina": ["samay raina", "samay rana"],
        "Ashneer Grover": ["ashneer grover", "ashneer", "grover"],
        "Rakhi Sawant": ["rakhi sawant", "rakhi"],
        "Shark Tank": ["shark tank"],
    }
    transcript_lower = raw_transcript.lower()
    for term, variants in spot_checks.items():
        found = any(v in transcript_lower for v in variants)
        status = "✅ FOUND" if found else "❌ MISSED"
        print(f"  {term:25s} -> {status}")

    # Summary
    print("\n" + "=" * 70)
    print(" Performance Matrix — IndicWhisper Evaluation")
    print("=" * 70)
    print(f"| {'Metric':<30} | {'Value':<30} |")
    print(f"| {'-'*30} | {'-'*30} |")
    print(f"| {'Model':<30} | {model_id:<30} |")
    print(f"| {'Device':<30} | {device:<30} |")
    print(f"| {'Model RAM':<30} | {model_ram:<28.1f} MB |")
    print(f"| {'Audio Duration':<30} | {duration_sec:<25.2f} sec |")
    print(f"| {'Inference Time':<30} | {asr_time:<25.2f} sec |")
    print(f"| {'RTF':<30} | {rtf:<30.3f} |")
    print(f"| {'Transcript Length':<30} | {len(raw_transcript):<24} chars |")
    print("=" * 70)

    # Save full transcript to file
    output_file = os.path.join(os.path.dirname(AUDIO_FILE), "indicwhisper_transcript.txt")
    with open(output_file, "w", encoding="utf-8") as f:
        f.write(f"Model: {model_id}\n")
        f.write(f"Duration: {duration_sec:.2f}s\n")
        f.write(f"RTF: {rtf:.3f}\n")
        f.write(f"---\n")
        f.write(raw_transcript)
    print(f"\nFull transcript saved to: {output_file}")


if __name__ == "__main__":
    main()
