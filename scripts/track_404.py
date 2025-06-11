#!/usr/bin/env python3
"""Summarize repeated HTTP errors without exposing user data.

The script aggregates paths that generated a chosen status code across one or
more access logs. Query parameters are stripped and all numeric segments are
replaced with ``<num>`` so that private identifiers do not appear in the
output.
"""

import argparse
import gzip
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Iterable, List, Tuple


def build_pattern(status: int) -> re.Pattern[str]:
    """Return a compiled regex for *status* codes in access logs."""
    pattern = (
        r'"(?:GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS) ([^ ]+) '
        r'HTTP/[0-9.]+" {status}'.format(status=status)
    )
    return re.compile(pattern)


def sanitize_path(path: str) -> str:
    """Strip query parameters and anonymize numbers in *path*."""
    path = path.split("?", 1)[0]
    return re.sub(r"\d+", "<num>", path)


def _iter_lines(log_path: Path) -> Iterable[str]:
    """Yield lines from ``log_path`` supporting ``-`` and gzip files."""
    if str(log_path) == "-":
        for line in sys.stdin:
            yield line
    elif str(log_path).endswith(".gz"):
        with gzip.open(
            log_path,
            "rt",
            encoding="utf-8",
            errors="ignore",
        ) as fh:
            for line in fh:
                yield line
    else:
        with log_path.open("r", encoding="utf-8", errors="ignore") as fh:
            for line in fh:
                yield line


def summarize_404(
    log_paths: Iterable[Path],
    min_count: int = 5,
    top: int = 0,
    status: int = 404,
) -> List[Tuple[str, int]]:
    """Return a sorted list of ``(path, count)`` tuples for repeated errors."""
    pattern = build_pattern(status)
    counts: Counter[str] = Counter()
    for file_path in log_paths:
        for line in _iter_lines(file_path):
            match = pattern.search(line)
            if match:
                counts[sanitize_path(match.group(1))] += 1

    items = sorted(counts.items(), key=lambda x: -x[1])
    results: List[Tuple[str, int]] = []
    shown = 0
    for p, count in items:
        if count < min_count:
            break
        results.append((p, count))
        shown += 1
        if top and shown >= top:
            break
    return results


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Analyze repeated 404s in one or more access logs"
    )
    parser.add_argument(
        "logs",
        nargs="+",
        type=Path,
        help="Log file(s) to read or '-' for standard input",
    )
    parser.add_argument(
        "--min-count",
        "-m",
        type=int,
        default=5,
        help="Minimum hits required to display",
    )
    parser.add_argument(
        "--limit",
        "-n",
        dest="limit",
        type=int,
        default=0,
        help="Show only the top N paths (0 for all)",
    )
    parser.add_argument(
        "--status",
        "-s",
        type=int,
        default=404,
        help="HTTP status code to analyze",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="Write results to a file instead of stdout",
    )
    args = parser.parse_args()
    results = summarize_404(
        args.logs, args.min_count, args.limit, status=args.status
    )
    out_lines = [f"{p}: {c} hits\n" for p, c in results]
    if args.output:
        args.output.write_text("".join(out_lines), encoding="utf-8")
    else:
        for line in out_lines:
            print(line, end="")


if __name__ == "__main__":
    main()
