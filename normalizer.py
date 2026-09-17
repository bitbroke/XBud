"""
normalizer.py — Post-ASR Text Normalizer for Indian languages.

Runs between ASR output and NER/SLM stages to fix systematic errors:
1. Hotword dictionary replacement (exact match)
2. Fuzzy name correction (for proper nouns)
3. Slang normalization

Designed to be lightweight (<5ms per utterance on RK3576).
"""
import json
import os
import re
from typing import Optional

try:
    from rapidfuzz import fuzz, process
    HAS_RAPIDFUZZ = True
except ImportError:
    HAS_RAPIDFUZZ = False
    print("WARNING: rapidfuzz not installed. Fuzzy matching disabled.")
    print("         Install with: pip install rapidfuzz")


class TextNormalizer:
    """Post-ASR text normalizer with hotword boosting and fuzzy name correction."""

    def __init__(self, hotwords_path: Optional[str] = None):
        self.exact_replacements = {}
        self.known_entities = []
        self.slang_normalizations = {}
        self.fuzzy_threshold = 80  # Minimum similarity score (0-100)

        # Load hotwords config
        if hotwords_path is None:
            hotwords_path = os.path.join(os.path.dirname(__file__), "hotwords.json")

        if os.path.exists(hotwords_path):
            self._load_hotwords(hotwords_path)
            print(f"[Normalizer] Loaded {len(self.exact_replacements)} hotword rules, "
                  f"{len(self.known_entities)} known entities")
        else:
            print(f"[Normalizer] No hotwords file found at {hotwords_path}")

    def _load_hotwords(self, path: str):
        """Load hotword configuration from JSON file."""
        with open(path, "r", encoding="utf-8") as f:
            config = json.load(f)

        self.exact_replacements = config.get("exact_replacements", {})
        self.known_entities = config.get("known_entities", [])
        self.slang_normalizations = config.get("slang_normalizations", {})

    def _apply_exact_replacements(self, text: str) -> str:
        """Apply case-insensitive exact string replacements."""
        for wrong, correct in self.exact_replacements.items():
            # Case-insensitive replacement
            pattern = re.compile(re.escape(wrong), re.IGNORECASE)
            text = pattern.sub(correct, text)
        return text

    def _apply_fuzzy_name_correction(self, text: str) -> str:
        """Use fuzzy matching to correct proper nouns against known entities."""
        if not HAS_RAPIDFUZZ or not self.known_entities:
            return text

        words = text.split()
        corrected_words = []
        i = 0

        while i < len(words):
            matched = False

            # Try matching 2-word and 3-word sequences (for full names)
            for n in [3, 2]:
                if i + n <= len(words):
                    candidate = " ".join(words[i:i+n])
                    # Skip short candidates and common English words
                    if len(candidate) < 4:
                        continue

                    result = process.extractOne(
                        candidate,
                        self.known_entities,
                        scorer=fuzz.ratio,
                        score_cutoff=self.fuzzy_threshold,
                    )
                    if result:
                        match, score, _ = result
                        corrected_words.append(match)
                        i += n
                        matched = True
                        break

            if not matched:
                corrected_words.append(words[i])
                i += 1

        return " ".join(corrected_words)

    def _apply_slang_normalization(self, text: str) -> str:
        """Normalize slang/abusive words to coded forms."""
        for slang, code in self.slang_normalizations.items():
            pattern = re.compile(re.escape(slang), re.IGNORECASE)
            text = pattern.sub(code, text)
        return text

    def normalize(self, text: str) -> str:
        """
        Run the full normalization pipeline.

        Order matters:
        1. Exact replacements first (cheapest, most reliable)
        2. Fuzzy name correction (catches remaining name errors)
        3. Slang normalization (cleanup)
        """
        if not text or not text.strip():
            return text

        # Step 1: Exact hotword replacements
        text = self._apply_exact_replacements(text)

        # Step 2: Fuzzy name correction
        text = self._apply_fuzzy_name_correction(text)

        # Step 3: Slang normalization
        text = self._apply_slang_normalization(text)

        return text


def demo():
    """Demo the normalizer on sample ASR errors from the India's Got Latent test."""
    normalizer = TextNormalizer()

    test_cases = [
        "Welcome to India's Gold Lady. Are we ready to start the show?",
        "Make some noise for Ashneel Grohan.",
        "Rakhishaman, thank you so much for joining us.",
        "I was just putting a episode in a lot of time.",
        "No, my brain will kill you by the chair. You don't get your audition, okay, Benchur?",
        "Samay Rena is the host of the show.",
        "Ashneel Bay, I'm starting to sing.",
    ]

    print("=" * 70)
    print(" Text Normalizer Demo")
    print("=" * 70)

    for i, text in enumerate(test_cases):
        corrected = normalizer.normalize(text)
        changed = text != corrected
        print(f"\n[{'CORRECTED' if changed else 'NO CHANGE'}]")
        if changed:
            print(f"  BEFORE: {text}")
            print(f"  AFTER:  {corrected}")
        else:
            print(f"  TEXT:   {text}")

    print("\n" + "=" * 70)


if __name__ == "__main__":
    demo()
