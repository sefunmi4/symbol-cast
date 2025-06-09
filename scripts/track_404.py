#!/usr/bin/env python3
"""Summarize repeated 404 errors without exposing user data."""

import argparse
import re
from collections import Counter
from pathlib import Path

HTTP_PATTERN = re.compile(r'"(?:GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS) ([^ ]+) HTTP/[0-9.]+" 404')


def sanitize_path(path: str) -> str:
    """Remove query params and replace digits with '<num>'."""
    path = path.split('?', 1)[0]
    return re.sub(r'\d+', '<num>', path)


def summarize_404(log_path: Path, min_count: int = 5, top: int = 0) -> None:
    counts: Counter[str] = Counter()
    with log_path.open('r', errors='ignore') as handle:
        for line in handle:
            m = HTTP_PATTERN.search(line)
            if m:
                counts[sanitize_path(m.group(1))] += 1

    items = sorted(counts.items(), key=lambda x: -x[1])
    shown = 0
    for path, count in items:
        if count < min_count:
            break
        print(f"{path}: {count} hits")
        shown += 1
        if top and shown >= top:
            break


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze repeated 404s in an access log")
    parser.add_argument("log", type=Path, help="Path to access log file")
    parser.add_argument("--min-count", "-m", type=int, default=5, help="Minimum hits required to display")
    parser.add_argument("--top", "-n", type=int, default=0, help="Show only the top N paths (0 for all)")
    args = parser.parse_args()
    summarize_404(args.log, args.min_count, args.top)


if __name__ == "__main__":
    main()
