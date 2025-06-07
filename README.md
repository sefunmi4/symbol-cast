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
lines of `x,y` pairs representing a drawing stroke.
---

## 🚀 Quick Start

> **Note:** The project is still in early development. Follow these steps to build the desktop version:

### 1. Clone the repo

```bash
git clone https://github.com/yourusername/symbolcast.git
cd symbolcast
```

### 2. Build with CMake

```bash
mkdir build && cd build
cmake ..
make
```

### 3. Run the desktop app
```bash
./symbolcast-desktop
```

An overlay window will appear on the desktop. Use your trackpad or mouse to draw a symbol and press **Submit**. The predicted symbol is printed in the console.

### Build and run the VR app
```bash
make symbolcast-vr
./symbolcast-vr
```
VR captures are written in 3D and can later be exported to CSV for training. The
VR example writes the gesture to `captured_vr_gesture.csv` using `VRInputManager`.

To enable ONNX Runtime support set the environment variable `ONNXRUNTIME_ROOT`
and pass `-DSC_USE_ONNXRUNTIME=ON` to CMake:

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
The generated model will be written to `models/symbolcast-v1.onnx` (ignored from
version control).

You can split the labeled dataset into training and test sets with
`scripts/training/split_dataset.py`:

```bash
python split_dataset.py --data_dir ../../data/labeled --out_dir ../../data/split
```
The generated model will be written to `models/symbolcast-v1.onnx` (ignored from
version control).

You can split the labeled dataset into training and test sets with
`scripts/training/split_dataset.py`:

```bash
python split_dataset.py --data_dir ../../data/labeled --out_dir ../../data/split

```
The generated model will be written to `models/symbolcast-v1.onnx` (ignored from
version control).


---

### 📦 Dependencies
- C++17 or later
- Qt 6+ (for GUI)
- OpenXR / SteamVR (for VR support)
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
- Provide pre-trained demo model in `models/`
- Improve CI matrix with sanitizer builds
- Add GUI for easier data collection
