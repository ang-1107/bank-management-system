#!/usr/bin/env python3
"""Convert a CSV file to JSON.

Usage:
    python scripts/csv_to_json.py <input_csv_path> <output_json_path>
"""

import argparse
import csv
import json
from pathlib import Path


def _coerce_value(value: str):
    text = value.strip()
    if text == "":
        return ""

    try:
        if any(ch in text for ch in (".", "e", "E")):
            return float(text)
        return int(text)
    except ValueError:
        return text


def convert_csv_to_json(input_csv: Path, output_json: Path) -> None:
    with input_csv.open("r", encoding="utf-8", newline="") as src:
        reader = csv.DictReader(src)
        rows = []
        for row in reader:
            converted = {}
            for key, value in row.items():
                if key is None:
                    continue
                converted[key.strip()] = _coerce_value(value or "")
            rows.append(converted)

    output_json.parent.mkdir(parents=True, exist_ok=True)
    with output_json.open("w", encoding="utf-8") as dst:
        json.dump(rows, dst, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert CSV data to JSON")
    parser.add_argument("input_csv_path", help="Path to the source CSV file")
    parser.add_argument("output_json_path", help="Path to write the JSON output")
    args = parser.parse_args()

    input_csv = Path(args.input_csv_path)
    output_json = Path(args.output_json_path)

    if not input_csv.exists() or not input_csv.is_file():
        raise SystemExit(f"Input CSV file not found: {input_csv}")

    convert_csv_to_json(input_csv, output_json)
    print(f"Wrote JSON output to: {output_json}")


if __name__ == "__main__":
    main()
