# SymbolCast

**SymbolCast** is a cross-platform gesture input engine that enables users to draw shapes and symbols using trackpads, mice, VR controllers, or keyboards — transforming those gestures into executable commands or programmable logic.

Designed to feel like a native extension of the OS, SymbolCast supports both 2D desktop environments and immersive VR interfaces, allowing intuitive input through gestures and symbols. The system also supports symbol recording for dataset generation and model training, powering an intelligent, custom gesture recognition pipeline.

---

## ✨ Features

- 🖱️ **Multi-input Drawing**: Trackpad, mouse, keyboard stroke path, or VR controller.
- 🧠 **Model Training Pipeline**: Collect labeled symbol data and train recognition models.
- ⚙️ **Command Mapping**: Bind recognized symbols to OS commands, macros, or scripts.
- 🧑‍💻 **Native C++ Core**: Built using Qt, OpenXR, and ONNX Runtime for fast performance and full control.
- 🌐 **Cross-Platform**: Designed for desktop and VR environments with future OS-level integration.
- 🔄 **VR Support (WIP)**: Symbol casting in 3D space using OpenXR and motion controllers.

---

## 🧱 Architecture Overview
```bash
apps/           # Desktop and VR apps
core/           # Core logic for input, symbol recognition, command mapping
data/           # Symbol dataset (raw, labeled, and processed)
models/         # Trained ML models (ONNX, TFLite)
scripts/        # Training scripts and utilities
```
Raw gesture captures are stored as CSV files under `data/raw`. Labeled training
samples belong in `data/labeled`, one CSV per symbol instance. Each CSV contains
lines of `x,y` pairs representing a drawing stroke. The filename prefix denotes
the symbol label, e.g. `triangle_01.csv`. 3D VR captures may contain `x,y,z`
triples.
---

## 🚀 Quick Start

> **Note:** The project is still in early development. Follow these steps to build the desktop version:

### 1. Clone the repo

```bash
git clone https://github.com/yourusername/symbolcast.git
cd symbolcast
```

### 2. Build with CMake
Install Qt (and optionally ONNX Runtime) for your platform. On macOS you can use
Homebrew: `brew install qt`. On Windows, install Qt via the official
installer and ensure `qmake` is in your PATH.

```bash
mkdir build && cd build
cmake ..
make
```

### 3. Run the desktop app
```bash
./symbolcast-desktop
```

An overlay window sized to your trackpad will appear with rounded corners and a thin border. Instructions are shown in light gray until you start drawing. You can drag the window by clicking and dragging anywhere when capture is off or resize it by grabbing an edge. Your system cursor disappears inside the overlay, and any finger motion creates a fading ripple. Double tap (or double click) to begin drawing; your movements are captured even without holding the button. Double tap again to submit the glowing trace for recognition. The instructions reappear after a few seconds of inactivity. Press **Esc** or **Ctrl+C** at any time to exit.

### Build and run the VR app
```bash
make symbolcast-vr
./symbolcast-vr
```
VR captures are written in 3D and can later be exported to CSV for training. The
VR example connects to a mock controller and writes the gesture to
`captured_vr_gesture.csv` using `VRInputManager::exportCSV`.

To enable ONNX Runtime support, download the prebuilt package and set the
environment variable `ONNXRUNTIME_ROOT` to its location. Then pass
`-DSC_USE_ONNXRUNTIME=ON` to CMake:

```bash
cmake -DSC_USE_ONNXRUNTIME=ON ..
```



---

### 🧪 Training a Model
1. Draw and label symbols via the app
2. Export the dataset to `data/labeled/` (CSV files containing `x,y` points)
3. Run the training script to generate an ONNX model:

```bash
cd scripts/training
python train_symbol_model.py \
    --data_dir ../../data/labeled \
    --output_model ../../models/symbolcast-v1.onnx
```
The generated model will be written to `models/symbolcast-v1.onnx`. Run this script before launching the apps so a model is available for inference.

You can split the labeled dataset into training and test sets with
`scripts/training/split_dataset.py`:

```bash
python split_dataset.py --data_dir ../../data/labeled --out_dir ../../data/split
```
You can preview any CSV file using the `visualize_gesture.py` helper:

```bash
python visualize_gesture.py ../../data/labeled/triangle_01.csv
```


---

### 📦 Dependencies
- C++17 or later
- Qt 6+ (for GUI)
- OpenXR / SteamVR (for VR support)
  - install the OpenXR runtime for your headset and make sure the loader
    libraries are discoverable by CMake
- ONNX Runtime (for model inference)
- Python (for training scripts)

---

### 🗺️ Roadmap
- Input capture (mouse, trackpad, keyboard)
- Symbol recording + labeling
- Dataset export + augmentation
- Model training and inference
- VR input drawing (OpenXR)
- Real-time gesture-to-command mapping
- EtherOS integration as system-level service

---

### 🤝 Contributing

Pull requests are welcome! For major changes, please open an issue to discuss what you’d like to add or improve.

Continuous integration builds and tests the project on Linux, macOS and Windows via GitHub Actions.

---

### 📜 License

This project is licensed under the MIT License.

---

### 🌌 Vision

SymbolCast is more than an input engine — it’s a step toward a world where users interact with their OS like spellcasters, using gestures, voice, and intention. Whether on a desktop or in virtual reality, SymbolCast reimagines computing as a symbolic dialogue between human and machine.

## TODO Before Reddit Release

<!-- TODO: Expand dataset with more symbols and capture utilities -->
- Document VR controller setup with OpenXR
- Improve CI matrix with sanitizer builds
- Add GUI for easier data collection
