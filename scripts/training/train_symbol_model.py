#!/usr/bin/env python3
"""Basic training routine for SymbolCast gesture models."""
import argparse
from pathlib import Path
import numpy as np
from sklearn.neighbors import KNeighborsClassifier
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
# Basic model training using scikit-learn. Provides optional augmentation and
# reports cross-validation accuracy before exporting the model to ONNX.
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType
from sklearn.model_selection import train_test_split, cross_val_score

# TODO: add data augmentation and cross-validation

def main():
    parser = argparse.ArgumentParser(description="Train symbol recognition model")
    parser.add_argument("--data_dir", required=True, help="Directory of labeled CSV files")
    parser.add_argument("--output_model", required=True, help="Path to output ONNX model")
    parser.add_argument("--max_points", type=int, default=3, help="Number of points to use per sample")
    parser.add_argument("--augment", type=int, default=0,
                        help="Number of jittered copies to add per sample")
    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    samples = list(data_dir.glob("*.csv"))
    X = []
    y = []
    for csv_file in samples:
        label = csv_file.stem.split("_")[0]
        points = []
        with open(csv_file) as f:
            for line in f:
                if line.strip():
                    x_str, y_str = line.strip().split(",")
                    points.extend([float(x_str), float(y_str)])
        # pad or truncate
        points = points[: args.max_points * 2]
        points += [0.0] * (args.max_points * 2 - len(points))
        X.append(points)
        y.append(label)
        # augmentation via random jitter
        for _ in range(args.augment):
            jitter = np.random.normal(scale=0.02, size=len(points))
            X.append((np.array(points) + jitter).tolist())
            y.append(label)

    X = np.array(X, dtype=np.float32)
    pipeline = Pipeline([
        ("scaler", StandardScaler()),
        ("knn", KNeighborsClassifier(n_neighbors=1)),
    ])
    scores = cross_val_score(pipeline, X, y, cv=3)
    print(f"Cross-val accuracy: {scores.mean():.2f} (+/- {scores.std():.2f})")
    pipeline.fit(X, y)

    initial_type = [("input", FloatTensorType([None, args.max_points * 2]))]
    onnx_model = convert_sklearn(pipeline, initial_types=initial_type)

    output = Path(args.output_model)
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "wb") as f:
        f.write(onnx_model.SerializeToString())
    print(f"Wrote ONNX model with {len(samples)} samples to {output}")

if __name__ == "__main__":
    main()
