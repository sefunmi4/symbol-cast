#!/usr/bin/env python3
"""Split labeled CSV dataset into train and test folders."""
import argparse
import random
import shutil
from pathlib import Path

# TODO: support stratified sampling by symbol type


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
    random.shuffle(files)
    split = int(len(files) * (1 - args.test_ratio))
    for f in files[:split]:
        shutil.copy(f, train_dir / f.name)
    for f in files[split:]:
        shutil.copy(f, test_dir / f.name)
    print(f'Split {len(files)} files -> {train_dir} ({split}) / {test_dir} ({len(files)-split})')


if __name__ == '__main__':
    main()
