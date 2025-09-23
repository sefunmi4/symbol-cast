# SymbolCast

**SymbolCast** is a cross-platform gesture input engine that enables users to draw shapes and symbols using trackpads, mice, VR controllers, or keyboards — transforming those gestures into executable commands or programmable logic.

Designed to feel like a native extension of the OS, SymbolCast supports both 2D desktop environments and immersive VR interfaces, allowing intuitive input through gestures and symbols. The system also supports symbol recording for dataset generation and model training, powering an intelligent, custom gesture recognition pipeline.

---

## ✨ Features

- 🖱️ **Multi-input Drawing**: Trackpad, mouse, keyboard stroke path, or VR controller.
- 🌀 **Multi-tap Capture**: Double-tap to start, single-tap to separate symbols, double-tap to submit.
- 🧠 **Model Training Pipeline**: Collect labeled symbol data and train recognition models.
- 🧩 **Hybrid Recognition**: Built-in core model plus live-trained custom gestures.
- 📚 **Modular Model Router**: Routes symbols to shape, letter, or gesture models automatically.
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
Install Qt (5 or 6) if you wish to build the desktop GUI. The core library and
tests build without it. On macOS you can use Homebrew: `brew install qt`. On
Windows, install Qt via the official installer and ensure `qmake` is in your
PATH.

```bash
mkdir build && cd build
cmake ..
make
```

### 3. Run the desktop app
```bash
./symbolcast-desktop
```
An overlay window sized to your trackpad will appear and your system cursor will be hidden inside it. Any finger motion creates a fading ripple so you can practice moving on the pad. Double tap (or double click) to begin drawing; your movements are captured even without holding the button. Tap once to submit the glowing trace for recognition.
Use single taps to separate symbols while recording and double tap again to finish the sequence.
Lifting your finger while drawing clears the current trace so you can reposition and start a new one without leaving drawing mode.

Press **Ctrl+T** after drawing to label the gesture for training. The dialog also lets you choose how many random variations to generate for one-shot learning.

When a system tray is available, the desktop app adds a tray icon labeled **Symbol Keyboard** that keeps running after you hide the canvas. Single- or double-click the tray icon (or choose the menu item) to show or hide the window without quitting. Closing the window now hides it in tray mode; use the tray menu’s **Quit** action or the existing keyboard shortcuts (Esc/Ctrl+C) to exit completely. On macOS and Windows the app continues running in the background so you can recall the keyboard later.

### Configuration

SymbolCast loads command mappings from `config/commands.json` when it starts.
Edit this file to change which command is triggered for each recognized symbol.
Models are listed in `config/models.json` and loaded on demand by the router.
After you export a network, update the JSON entries so each symbol points at the
exact ONNX or TorchScript file you produced (for example,
`models/symbolcast-v1.onnx` or `models/symbolcast-v1.ts`).


#### Command-line options

You can customize the appearance of ripples and strokes:

```
--ripple-growth <rate>   Ripple growth per frame (default: 2.0)
--ripple-max <radius>    Maximum ripple radius (default: 80)
--ripple-color <hex>     Ripple color in hex (default: #fffbe096)
--stroke-width <px>      Stroke width in pixels (default: 3)
--stroke-color <hex>     Stroke color in hex (default: #fffbe0)
--fade-rate <rate>       Stroke fade per frame (default: 0.005)
--detection-color <hex>  Detection box color in hex (default: #ffffff66)
--fullscreen             Launch the board fullscreen
--no-cursor-animation    Disable cursor ripple animation (enabled by default)
```

An overlay window sized to your trackpad will appear with rounded corners and a thin border. Instructions are shown in light gray until you start drawing. You can drag the window by pressing and then moving while capture is off (dragging only begins after you move, so double taps won't shift the window) or resize it by grabbing an edge. Your system cursor disappears inside the overlay, and any finger motion creates a fading ripple. Double tap (or double click) to begin drawing; a soft yellow trace follows your finger and gradually fades away so you can write multi-stroke symbols. Tap once to submit the trace for recognition. The instructions reappear after a few seconds of inactivity. Press **Esc** or **Ctrl+C** at any time to exit. The console logs when capture starts, each point is recorded, and when a symbol is detected.
#### Logging

SymbolCast prints timestamped log messages to the console. Control the verbosity with the `SC_LOG_LEVEL` environment variable (`DEBUG`, `INFO`, `WARN`, or `ERROR`). Example:

```bash
SC_LOG_LEVEL=DEBUG ./symbolcast-desktop
```

Use this to capture detailed events when troubleshooting gesture input or model issues.

### HTTP Error Tracking

Use `scripts/track_404.py` to find frequently requested paths that return a
specific status code (404 by default). The script strips query parameters and
anonymizes any digits so personal data is not leaked. Multiple log files can be
analyzed at once and gzipped logs are supported. Results may also be written to
a file using ``-o``.

```bash
python scripts/track_404.py access.log other.log.gz -m 10 -n 20 \
    -s 404 -o results.txt
```
The options control the minimum number of hits (`-m`), how many results to
display (`-n`, `0` for all), and which status code to track (`-s`). Paths
containing numbers are reported with `<num>` placeholders to avoid exposing
IDs. Pass `-` in place of a filename to read from standard input.

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
    --augment 10
```
Use `--augment` to synthesize jittered copies of each sample during training. The generated model will be written to `models/symbolcast-v1.onnx`. Run this script before launching the apps so a model is available for inference.
Additional weights can be placed in the `models/` directory and mapped in `config/models.json` for the router to load.

You can split the labeled dataset into training and test sets with
`scripts/training/split_dataset.py`:

```bash
python split_dataset.py --data_dir ../../data/labeled --out_dir ../../data/split
```
You can preview any CSV file using the `visualize_gesture.py` helper:

```bash
python visualize_gesture.py ../../data/labeled/triangle_01.csv
```
The generated model will be written to `models/symbolcast-v1.onnx` (ignored from
version control).


### TrOCR Export and C++ Inference

Use `scripts/export_trocr.py` to convert the [TrOCR](https://huggingface.co/microsoft/trocr-base-stage1)
model to TorchScript along with its processor files. The traced model and tokenizer can then be
consumed from C++.

Install the script's Python dependencies before running the export script:

```bash
pip install -r scripts/requirements.txt
# or
pip install transformers pillow
```

The example `apps/trocr_infer.cpp` shows how to load the TorchScript module with LibTorch,
preprocess an image using OpenCV, and decode the output tokens using the Hugging Face tokenizers
library:

```bash
g++ trocr_infer.cpp -o trocr_infer \
    -I/path/to/libtorch/include -I/path/to/libtorch/include/torch/csrc/api/include \
    -L/path/to/libtorch/lib -ltorch_cpu -lc10 \
    `pkg-config --cflags --libs opencv4` \
    -ltokenizers
```


---

### 📦 Dependencies
- C++17 or later
- Qt 5 or 6 (optional, for GUI)
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

#### Contributor Tasks

The following items are open for community contributions:

- Expand the dataset with more symbols and capture utilities.
- Document VR controller setup with OpenXR.
- Improve the CI matrix with sanitizer builds.
- Add a GUI for easier data collection.


---

### 📜 License

This project is licensed under the MIT License.

---

### 🌌 Vision

SymbolCast is more than an input engine — it’s a step toward a world where users interact with their OS like spellcasters, using gestures, voice, and intention. Whether on a desktop or in virtual reality, SymbolCast reimagines computing as a symbolic dialogue between human and machine.

