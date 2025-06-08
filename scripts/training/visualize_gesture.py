#!/usr/bin/env python3
"""Visualize gesture CSV files as simple plots."""
import argparse
from pathlib import Path
import matplotlib.pyplot as plt


def main():
    p = argparse.ArgumentParser()
    p.add_argument('csv', help='Path to gesture CSV file')
    args = p.parse_args()
    points = []
    with open(args.csv) as f:
        for line in f:
            if line.strip():
                x, y = line.strip().split(',')
                points.append((float(x), float(y)))
    if not points:
        print('No points loaded')
        return
    xs, ys = zip(*points)
    plt.plot(xs, ys, marker='o')
    plt.gca().set_aspect('equal', 'box')
    plt.title(Path(args.csv).name)
    plt.show()

if __name__ == '__main__':
    main()
