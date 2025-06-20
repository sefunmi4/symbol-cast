#!/usr/bin/env python3
"""Split labeled CSV dataset into train and test folders."""
import argparse
import random
import shutil
from pathlib import Path
from collections import defaultdict

# Stratified split by symbol label


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--data_dir', required=True)
    p.add_argument('--out_dir', required=True)
    p.add_argument('--test_ratio', type=float, default=0.2)
    args = p.parse_args()

    data_dir = Path(args.data_dir)
    out_dir = Path(args.out_dir)
    train_dir = out_dir / 'train'
    test_dir = out_dir / 'test'
    train_dir.mkdir(parents=True, exist_ok=True)
    test_dir.mkdir(parents=True, exist_ok=True)

    files = list(data_dir.glob('*.csv'))
    labels = defaultdict(list)
    for f in files:
        label = f.stem.split('_')[0]
        labels[label].append(f)
    train_count = 0
    test_count = 0
    for lbl, lst in labels.items():
        random.shuffle(lst)
        split = int(len(lst) * (1 - args.test_ratio))
        for f in lst[:split]:
            shutil.copy(f, train_dir / f.name)
        for f in lst[split:]:
            shutil.copy(f, test_dir / f.name)
        train_count += split
        test_count += len(lst) - split
    print(f'Split {len(files)} files -> {train_dir} ({train_count}) / {test_dir} ({test_count})')


if __name__ == '__main__':
    main()
