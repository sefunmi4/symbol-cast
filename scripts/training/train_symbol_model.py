#!/usr/bin/env python3
"""Dummy training script for SymbolCast models."""
import argparse
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Train symbol recognition model")
    parser.add_argument("--data_dir", required=True)
    parser.add_argument("--output_model", required=True)
    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    samples = list(data_dir.glob("*.csv"))
    point_count = 0
    for csv_file in samples:
        with open(csv_file) as f:
            for line in f:
                if line.strip():
                    point_count += 1

    output = Path(args.output_model)
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w") as f:
        f.write(f"sample_count={len(samples)}\npoint_count={point_count}\n")
    print(f"Wrote model placeholder with {len(samples)} samples to {output}")

if __name__ == "__main__":
    main()
