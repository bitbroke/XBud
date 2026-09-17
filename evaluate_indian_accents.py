"""
evaluate_indian_accents.py — Multi-accent, multi-language Indian ASR evaluation.

Downloads short clips from YouTube spanning different Indian accents and languages,
runs Whisper ASR, applies the normalizer, and produces a comprehensive performance matrix.
"""
import os
import sys
import time
import re
import json
import glob

sys.stdout.reconfigure(encoding='utf-8')

try:
    import whisper
    import torch
    import yt_dlp
    import psutil
except ImportError:
    import subprocess
    print("Installing required packages...")
    subprocess.check_call([sys.executable, "-m", "pip", "install",
                           "openai-whisper", "torch", "yt-dlp", "psutil", "imageio-ffmpeg"])
    import whisper
    import torch
    import yt_dlp
    import psutil

# Add local directory to PATH for ffmpeg
os.environ["PATH"] = os.getcwd() + os.pathsep + os.environ.get("PATH", "")

# Try to import normalizer
try:
    from normalizer import TextNormalizer
    normalizer = TextNormalizer()
    HAS_NORMALIZER = True
except Exception as e:
    print(f"[WARNING] Normalizer not available: {e}")
    HAS_NORMALIZER = False

# ============================================================================
# TEST VIDEOS — Curated for different Indian accents and languages
# ============================================================================
TEST_VIDEOS = [
    {
        "name": "Hindi News (Standard Hindi)",
        "language": "Hindi",
        "accent": "Standard Hindi",
        "search": "ytsearch1:Aaj Tak Hindi news bulletin 2024 short",
        "max_duration": 120,  # seconds - limit to 2 min
    },
    {
        "name": "South Indian English (Tech Talk)",
        "language": "English",
        "accent": "South Indian",
        "search": "ytsearch1:Sundar Pichai interview short clip",
        "max_duration": 120,
    },
    {
        "name": "Hinglish Stand-up Comedy",
        "language": "Hinglish",
        "accent": "Delhi/North Indian",
        "search": "ytsearch1:Zakir Khan stand up comedy short clip",
        "max_duration": 120,
    },
    {
        "name": "Bengali English (Interview)",
        "language": "English",
        "accent": "Bengali",
        "search": "ytsearch1:Sourav Ganguly interview English short",
        "max_duration": 120,
    },
    {
        "name": "Punjabi English (Interview)",
        "language": "English",
        "accent": "Punjabi",
        "search": "ytsearch1:Navjot Singh Sidhu interview English short",
        "max_duration": 120,
    },
    {
        "name": "Tamil Speech (Political)",
        "language": "Tamil",
        "accent": "Tamil",
        "search": "ytsearch1:Tamil Nadu political speech short 2024",
        "max_duration": 120,
    },
    {
        "name": "Gujarati English (Business)",
        "language": "English",
        "accent": "Gujarati",
        "search": "ytsearch1:Mukesh Ambani speech English short",
        "max_duration": 120,
    },
    {
        "name": "Indian Call Center English",
        "language": "English",
        "accent": "Mixed Indian",
        "search": "ytsearch1:Indian customer service call recording sample",
        "max_duration": 120,
    },
]


def scrub_pii(text):
    """Basic Regex-based PII scrubber."""
    text = re.sub(r'\+?\d[\d -]{8,12}\d', '[PHONE]', text)
    text = re.sub(r'\b\d{4}\s\d{4}\s\d{4}\b', '[ID_CARD]', text)
    text = re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '[EMAIL]', text)
    return text


def download_audio(search_query, output_prefix, max_duration=120):
    """Download audio from YouTube, limited to max_duration seconds."""
    # Clean up previous downloads
    for f in glob.glob(output_prefix + ".*"):
        try:
            os.remove(f)
        except:
            pass

    ydl_opts = {
        'format': 'bestaudio[ext=m4a]/bestaudio/best',
        'outtmpl': output_prefix + '.%(ext)s',
        'quiet': True,
        'no_warnings': True,
        'js_runtimes': {'node': {}},
    }

    try:
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(search_query, download=True)
            # Get the actual entry from search results
            if 'entries' in info:
                entry = info['entries'][0] if info['entries'] else None
                if entry:
                    title = entry.get('title', 'Unknown')
                    duration = entry.get('duration', 0)
                else:
                    title = 'Unknown'
                    duration = 0
            else:
                title = info.get('title', 'Unknown')
                duration = info.get('duration', 0)

            files = glob.glob(output_prefix + ".*")
            if files:
                return files[0], title, duration
            return None, title, duration
    except Exception as e:
        print(f"  Download error: {e}")
        return None, "Error", 0


def main():
    print("=" * 80)
    print(" Ear-Brain Multi-Accent Indian ASR Evaluation")
    print("=" * 80)
    print(f" Testing {len(TEST_VIDEOS)} videos across different Indian accents & languages")
    print(f" Normalizer: {'Enabled' if HAS_NORMALIZER else 'Disabled'}")
    print("=" * 80)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"\nLoading Whisper model on {device}...")

    process = psutil.Process()
    ram_before = process.memory_info().rss / (1024 * 1024)

    model = whisper.load_model("base", device=device)

    ram_after = process.memory_info().rss / (1024 * 1024)
    model_ram = ram_after - ram_before
    print(f"[Model Loaded] Whisper base | RAM: {model_ram:.1f} MB")

    results = []
    transcripts = {}

    for i, video in enumerate(TEST_VIDEOS):
        print(f"\n{'─' * 80}")
        print(f" [{i+1}/{len(TEST_VIDEOS)}] {video['name']}")
        print(f" Language: {video['language']} | Accent: {video['accent']}")
        print(f"{'─' * 80}")

        # 1. Download audio
        output_prefix = f"accent_test_{i}"
        print(f"  Downloading: {video['search']}...")
        audio_file, yt_title, yt_duration = download_audio(
            video['search'], output_prefix, video['max_duration']
        )

        if audio_file is None or not os.path.exists(audio_file):
            print(f"  ❌ FAILED to download. Skipping.")
            results.append({
                "name": video['name'],
                "language": video['language'],
                "accent": video['accent'],
                "status": "DOWNLOAD_FAILED",
                "yt_title": yt_title,
            })
            continue

        print(f"  YouTube Title: {yt_title}")

        try:
            # 2. Load audio
            audio = whisper.load_audio(audio_file)
            duration_sec = len(audio) / 16000.0

            # Trim to max_duration if needed
            max_samples = video['max_duration'] * 16000
            if len(audio) > max_samples:
                audio = audio[:max_samples]
                duration_sec = video['max_duration']
                print(f"  [Trimmed to {video['max_duration']}s]")

            print(f"  Audio Duration: {duration_sec:.1f}s")

            # 3. Run ASR (auto-detect language — do NOT force English)
            start_time = time.time()
            result = model.transcribe(audio)
            asr_time = time.time() - start_time

            raw_transcript = result["text"].strip()

            # 4. Apply normalizer
            if HAS_NORMALIZER:
                normalized_transcript = normalizer.normalize(raw_transcript)
            else:
                normalized_transcript = raw_transcript

            # 5. PII scrubbing
            scrubbed = scrub_pii(normalized_transcript)

            # 6. Compute metrics
            rtf = asr_time / duration_sec if duration_sec > 0 else 0
            word_count = len(raw_transcript.split())
            words_per_sec = word_count / duration_sec if duration_sec > 0 else 0

            # Detected language from Whisper
            detected_lang = result.get("language", "unknown")

            # Print transcript preview
            preview = raw_transcript[:200]
            if len(raw_transcript) > 200:
                preview += "..."
            print(f"  ASR Time: {asr_time:.2f}s | RTF: {rtf:.3f}x")
            print(f"  Detected Language: {detected_lang}")
            print(f"  Words: {word_count} | Words/sec: {words_per_sec:.1f}")
            print(f"  Transcript: {preview}")

            if HAS_NORMALIZER and normalized_transcript != raw_transcript:
                norm_preview = normalized_transcript[:200]
                if len(normalized_transcript) > 200:
                    norm_preview += "..."
                print(f"  [Normalized]: {norm_preview}")

            results.append({
                "name": video['name'],
                "language": video['language'],
                "accent": video['accent'],
                "yt_title": yt_title,
                "status": "OK",
                "duration_s": round(duration_sec, 2),
                "asr_time_s": round(asr_time, 2),
                "rtf": round(rtf, 3),
                "word_count": word_count,
                "words_per_sec": round(words_per_sec, 1),
                "detected_lang": detected_lang,
                "transcript_len": len(raw_transcript),
                "normalizer_corrections": normalized_transcript != raw_transcript,
            })

            transcripts[video['name']] = {
                "raw": raw_transcript,
                "normalized": normalized_transcript,
                "scrubbed": scrubbed,
            }

        except Exception as e:
            print(f"  ❌ Error: {e}")
            results.append({
                "name": video['name'],
                "language": video['language'],
                "accent": video['accent'],
                "status": f"ERROR: {str(e)[:80]}",
                "yt_title": yt_title,
            })
        finally:
            # Cleanup downloaded file
            if audio_file and os.path.exists(audio_file):
                try:
                    os.remove(audio_file)
                except:
                    pass

    # ========================================================================
    # PRINT FINAL PERFORMANCE MATRIX
    # ========================================================================
    ok_results = [r for r in results if r.get("status") == "OK"]
    failed_results = [r for r in results if r.get("status") != "OK"]

    print("\n\n" + "=" * 100)
    print(" PERFORMANCE MATRIX — Multi-Accent Indian ASR Evaluation")
    print("=" * 100)
    print(f" Model: Whisper base | Device: {device} | RAM: {model_ram:.0f} MB")
    print(f" Normalizer: {'Enabled' if HAS_NORMALIZER else 'Disabled'}")
    print(f" Successful: {len(ok_results)}/{len(results)} videos")
    print("=" * 100)

    if ok_results:
        # Header
        print(f"\n| {'#':>2} | {'Name':<35} | {'Lang':<10} | {'Accent':<15} | {'Duration':>8} | {'Latency':>8} | {'RTF':>6} | {'Words':>6} | {'W/s':>5} | {'Det.Lang':>8} |")
        print(f"| {'--':>2} | {'-'*35} | {'-'*10} | {'-'*15} | {'-'*8} | {'-'*8} | {'-'*6} | {'-'*6} | {'-'*5} | {'-'*8} |")

        for i, r in enumerate(ok_results):
            name = r['name'][:35]
            print(f"| {i+1:>2} | {name:<35} | {r['language']:<10} | {r['accent']:<15} | {r['duration_s']:>6.1f}s | {r['asr_time_s']:>6.1f}s | {r['rtf']:>6.3f} | {r['word_count']:>6} | {r['words_per_sec']:>5.1f} | {r['detected_lang']:>8} |")

        # Aggregate stats
        avg_rtf = sum(r['rtf'] for r in ok_results) / len(ok_results)
        min_rtf = min(r['rtf'] for r in ok_results)
        max_rtf = max(r['rtf'] for r in ok_results)
        total_audio = sum(r['duration_s'] for r in ok_results)
        total_asr = sum(r['asr_time_s'] for r in ok_results)
        avg_wps = sum(r['words_per_sec'] for r in ok_results) / len(ok_results)

        print(f"\n{'─' * 100}")
        print(f" AGGREGATE STATS")
        print(f"{'─' * 100}")
        print(f"  Total Audio Processed:    {total_audio:.1f}s ({total_audio/60:.1f} min)")
        print(f"  Total Inference Time:     {total_asr:.1f}s ({total_asr/60:.1f} min)")
        print(f"  Average RTF:              {avg_rtf:.3f}x")
        print(f"  Best RTF (fastest):       {min_rtf:.3f}x")
        print(f"  Worst RTF (slowest):      {max_rtf:.3f}x")
        print(f"  Avg Words/Second:         {avg_wps:.1f}")
        print(f"  RTF < 1.0 (realtime)?     {'YES' if max_rtf < 1.0 else 'NO'}")

    if failed_results:
        print(f"\n{'─' * 100}")
        print(f" FAILED DOWNLOADS ({len(failed_results)})")
        print(f"{'─' * 100}")
        for r in failed_results:
            print(f"  ❌ {r['name']}: {r['status']}")

    print("\n" + "=" * 100)

    # Save results to JSON
    output_json = os.path.join(os.getcwd(), "accent_evaluation_results.json")
    with open(output_json, "w", encoding="utf-8") as f:
        json.dump({
            "model": "whisper-base",
            "device": device,
            "model_ram_mb": round(model_ram, 1),
            "normalizer_enabled": HAS_NORMALIZER,
            "results": results,
            "transcripts": transcripts,
        }, f, indent=2, ensure_ascii=False)
    print(f"\nFull results saved to: {output_json}")


if __name__ == "__main__":
    main()
