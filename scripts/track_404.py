#!/usr/bin/env python3
"""Track repeated 404 errors without storing user info."""

import re
import sys
from collections import Counter

HTTP_PATTERN = re.compile(r'"(?:GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS) ([^ ]+) HTTP/[0-9.]+" 404')


def summarize_404(log_path: str, threshold: int = 5):
    counts = Counter()
    with open(log_path, 'r', errors='ignore') as f:
        for line in f:
            m = HTTP_PATTERN.search(line)
            if m:
                path = m.group(1).split('?')[0]
                counts[path] += 1
    for path, count in sorted(counts.items(), key=lambda x: -x[1]):
        if count >= threshold:
            print(f"{path}: {count} hits")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: track_404.py <access_log> [threshold]", file=sys.stderr)
        sys.exit(1)
    file_path = sys.argv[1]
    thr = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    summarize_404(file_path, thr)
