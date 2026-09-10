#!/usr/bin/env python3
"""
evaluate_schema.py — SLM JSON schema compliance evaluation.

Tests whether GBNF-constrained output from Gemma 3 1B produces
valid JSON matching the expected schemas.

Usage:
    python3 evaluate_schema.py --results-dir <dir> --schema <schema.json>
"""

import argparse
import json
import sys
from pathlib import Path


def validate_json_string(json_str: str) -> dict:
    """Validate that a string is parseable JSON."""
    result = {
        "is_valid_json": False,
        "parse_error": None,
        "json_type": None,
    }

    try:
        parsed = json.loads(json_str)
        result["is_valid_json"] = True
        result["json_type"] = type(parsed).__name__
    except json.JSONDecodeError as e:
        result["parse_error"] = str(e)

    return result


def validate_action_item(obj: dict) -> dict:
    """Validate an action item object has required fields."""
    required = ["who", "what"]
    optional = ["when", "priority", "confidence"]

    errors = []
    for field in required:
        if field not in obj:
            errors.append(f"missing required field: {field}")
        elif not isinstance(obj[field], str):
            errors.append(f"field '{field}' must be string, got {type(obj[field]).__name__}")

    if "priority" in obj and obj["priority"] not in ["high", "medium", "low"]:
        errors.append(f"invalid priority: {obj['priority']}")

    if "confidence" in obj:
        if not isinstance(obj["confidence"], (int, float)) or not 0 <= obj["confidence"] <= 1:
            errors.append(f"confidence must be [0,1], got {obj.get('confidence')}")

    return {
        "valid": len(errors) == 0,
        "errors": errors,
        "fields_present": list(obj.keys()),
    }


def validate_call_summary(obj: dict) -> dict:
    """Validate a call summary object."""
    required = ["executive_overview", "key_takeaways", "sentiment"]
    errors = []

    for field in required:
        if field not in obj:
            errors.append(f"missing required field: {field}")

    if "sentiment" in obj and obj["sentiment"] not in ["positive", "negative", "neutral", "mixed"]:
        errors.append(f"invalid sentiment: {obj['sentiment']}")

    if "key_takeaways" in obj and not isinstance(obj["key_takeaways"], list):
        errors.append("key_takeaways must be an array")

    return {
        "valid": len(errors) == 0,
        "errors": errors,
    }


def evaluate_results_dir(results_dir: str, schema_type: str) -> list:
    """Evaluate all JSON result files in a directory."""
    results = []
    dir_path = Path(results_dir)

    for json_file in sorted(dir_path.glob("*.json")):
        with open(json_file, "r", encoding="utf-8") as f:
            content = f.read()

        result = {
            "file": json_file.name,
            "byte_size": len(content),
        }

        # Stage 1: JSON validity
        validity = validate_json_string(content)
        result.update(validity)

        # Stage 2: Schema compliance
        if validity["is_valid_json"]:
            parsed = json.loads(content)

            if schema_type == "action_item":
                items = parsed if isinstance(parsed, list) else [parsed]
                schema_results = [validate_action_item(item) for item in items]
                result["items_count"] = len(items)
                result["items_valid"] = sum(1 for r in schema_results if r["valid"])
                result["schema_errors"] = [
                    e for r in schema_results for e in r.get("errors", [])
                ]
            elif schema_type == "call_summary":
                schema_result = validate_call_summary(parsed)
                result.update(schema_result)

            result["schema_compliant"] = len(result.get("schema_errors", result.get("errors", []))) == 0

        results.append(result)

    return results


def print_summary(results: list):
    """Print evaluation summary."""
    total = len(results)
    valid_json = sum(1 for r in results if r.get("is_valid_json"))
    schema_compliant = sum(1 for r in results if r.get("schema_compliant"))

    json_rate = valid_json / total if total > 0 else 0
    schema_rate = schema_compliant / total if total > 0 else 0

    print(f"\n{'='*60}")
    print(f"SLM Schema Compliance Report ({total} samples)")
    print(f"{'='*60}")
    print(f"  Valid JSON:        {valid_json}/{total} ({json_rate:.1%})")
    print(f"  Schema Compliant:  {schema_compliant}/{total} ({schema_rate:.1%})")
    print(f"  KPI Target:        > 95.00%")
    print(f"  KPI Status:        {'✅ PASS' if schema_rate > 0.95 else '❌ FAIL'}")
    print(f"{'='*60}")

    # Show common errors
    all_errors = []
    for r in results:
        all_errors.extend(r.get("schema_errors", r.get("errors", [])))

    if all_errors:
        from collections import Counter
        error_counts = Counter(all_errors).most_common(5)
        print(f"\n  Top Errors:")
        for err, count in error_counts:
            print(f"    [{count}x] {err}")


def main():
    parser = argparse.ArgumentParser(description="SLM Schema Compliance Evaluator")
    parser.add_argument("--results-dir", required=True, help="Directory with JSON outputs")
    parser.add_argument("--schema", default="action_item",
                       choices=["action_item", "call_summary", "calendar_event",
                                "crm_payload", "email_draft", "decision_entry"],
                       help="Schema type to validate against")
    parser.add_argument("--output", "-o", help="Output JSON report file")
    args = parser.parse_args()

    results = evaluate_results_dir(args.results_dir, args.schema)
    print_summary(results)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2)
        print(f"\nFull report: {args.output}")


if __name__ == "__main__":
    main()
