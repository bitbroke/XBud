#!/usr/bin/env python3
"""
compute_wer.py — Word Error Rate computation for ASR evaluation.

Uses jiwer library to compute WER, CER, and other metrics.
Supports Hinglish (mixed Hindi-English) transcripts.

Usage:
    python3 compute_wer.py <reference.txt> <hypothesis.txt>
    python3 compute_wer.py --corpus-dir <dir> --output results.json
"""

import argparse
import json
import sys
from pathlib import Path

try:
    from jiwer import wer, cer, mer, wil
except ImportError:
    print("ERROR: pip install jiwer", file=sys.stderr)
    sys.exit(1)


def normalize_text(text: str) -> str:
    """Normalize text for WER computation."""
    # Lowercase
    text = text.lower().strip()
    # Remove extra whitespace
    text = " ".join(text.split())
    # Remove common punctuation (keep Devanagari characters)
    for ch in ".,;:!?\"'()[]{}—–-":
        text = text.replace(ch, "")
    return text


def compute_metrics(reference: str, hypothesis: str) -> dict:
    """Compute all ASR metrics."""
    ref = normalize_text(reference)
    hyp = normalize_text(hypothesis)

    if not ref:
        return {"wer": 1.0, "cer": 1.0, "mer": 1.0, "wil": 1.0, "error": "empty_reference"}

    return {
        "wer": round(wer(ref, hyp), 4),
        "cer": round(cer(ref, hyp), 4),
        "mer": round(mer(ref, hyp), 4),
        "wil": round(wil(ref, hyp), 4),
        "ref_words": len(ref.split()),
        "hyp_words": len(hyp.split()),
    }


def evaluate_pair(ref_path: str, hyp_path: str) -> dict:
    """Evaluate a single reference/hypothesis pair."""
    with open(ref_path, "r", encoding="utf-8") as f:
        ref = f.read()
    with open(hyp_path, "r", encoding="utf-8") as f:
        hyp = f.read()

    metrics = compute_metrics(ref, hyp)
    metrics["reference_file"] = str(ref_path)
    metrics["hypothesis_file"] = str(hyp_path)
    return metrics


def evaluate_corpus(corpus_dir: str) -> list:
    """Evaluate all .txt pairs in a corpus directory.

    Expected structure:
        corpus_dir/
            sample_001.txt          # Reference transcript
            sample_001.hyp.txt      # Hypothesis (ASR output)
    """
    results = []
    corpus_path = Path(corpus_dir)

    ref_files = sorted(corpus_path.glob("*.txt"))
    ref_files = [f for f in ref_files if not f.stem.endswith(".hyp")]

    for ref_file in ref_files:
        hyp_file = ref_file.with_suffix("").with_suffix(".hyp.txt")
        if hyp_file.exists():
            metrics = evaluate_pair(str(ref_file), str(hyp_file))
            results.append(metrics)
        else:
            print(f"WARNING: No hypothesis for {ref_file.name}", file=sys.stderr)

    return results


def print_summary(results: list):
    """Print aggregate statistics."""
    if not results:
        print("No results to summarize.")
        return

    avg_wer = sum(r["wer"] for r in results) / len(results)
    avg_cer = sum(r["cer"] for r in results) / len(results)
    min_wer = min(r["wer"] for r in results)
    max_wer = max(r["wer"] for r in results)

    print(f"\n{'='*60}")
    print(f"ASR Evaluation Summary ({len(results)} samples)")
    print(f"{'='*60}")
    print(f"  Average WER:  {avg_wer:.2%}")
    print(f"  Average CER:  {avg_cer:.2%}")
    print(f"  Min WER:      {min_wer:.2%}")
    print(f"  Max WER:      {max_wer:.2%}")
    print(f"  KPI Target:   < 14.00%")
    print(f"  KPI Status:   {'✅ PASS' if avg_wer < 0.14 else '❌ FAIL'}")
    print(f"{'='*60}")


def main():
    parser = argparse.ArgumentParser(description="ASR WER Evaluation Tool")
    parser.add_argument("reference", nargs="?", help="Reference transcript file")
    parser.add_argument("hypothesis", nargs="?", help="Hypothesis transcript file")
    parser.add_argument("--corpus-dir", help="Directory containing ref/hyp pairs")
    parser.add_argument("--output", "-o", help="Output JSON file")
    args = parser.parse_args()

    if args.corpus_dir:
        results = evaluate_corpus(args.corpus_dir)
        print_summary(results)
    elif args.reference and args.hypothesis:
        results = [evaluate_pair(args.reference, args.hypothesis)]
        print(json.dumps(results[0], indent=2))
    else:
        parser.print_help()
        sys.exit(1)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2)
        print(f"\nResults saved to: {args.output}")


if __name__ == "__main__":
    main()
