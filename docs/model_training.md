# SymbolCast Model Training Procedure

This document describes the recommended training pipeline for SymbolCast. It covers both quick on-the-fly symbol learning and more comprehensive offline training for a robust base model.

## 1. Base Model Pretraining

The base model should be prepared ahead of time using a labeled dataset of common symbols and gestures. This ensures fast and accurate recognition for frequently used commands.

### Symbol Set
- Geometric primitives: triangle (up/down/left/right), circle, square
- English letters and digits (A–Z, 0–9)
- Braille patterns for 6-dot and 8-dot grids
- Palm-drawn sign language shapes
- Any additional symbols useful across the system

### Steps
1. **Collect Data** – capture many examples using trackpads, mice or VR controllers. Include variations in stroke speed, pressure and orientation.
2. **Preprocess** – normalize point sequences, augment with random rotation or scaling and trim/pad to a fixed length.
3. **Train** – use the provided `scripts/training/train_symbol_model.py` to train a small KNN or other lightweight model. For larger datasets consider a CNN+RNN or transformer architecture.
4. **Export** – save the trained network to `models/` as an ONNX or TFLite file. Include a JSON label map for symbol names.

Once trained, this model is shipped with SymbolCast and loaded on startup for high performance recognition.

## 2. Live Custom Symbol Training

Users can define their own symbols without rebuilding the model. The process is intended to be lightweight so gestures can be saved during normal use.

1. **Record** – tap three times to enter record mode and draw the new gesture.
2. **Finish** – tap twice to stop and assign a label plus an optional command category.
3. **Store** – the gesture points and label are saved to `user_gestures.json` (or an SQLite database) and indexed in memory.
4. **Predict** – incoming gestures are first compared against these custom entries. Matching uses a small-distance metric such as cosine similarity or DTW.
5. **Persist** – user symbols remain available across sessions and can be exported or synced between devices.

## 3. Extended Training Sessions

For higher accuracy and support for additional languages or complex gestures, repeat the collection and preprocessing steps with a larger dataset. Train periodically offline and redistribute updated models to users as downloads.

Consider tracking different symbol categories (commands, runes, navigation, etc.) in separate files for easier management.

## 4. Evaluation Targets
- Prediction latency under 100 ms for single gestures
- Base model accuracy above 95% on the test set
- Minimal memory footprint so custom indices load instantly

## 5. Further Reading
Refer to `README.md` for command-line instructions on running the existing training scripts.

