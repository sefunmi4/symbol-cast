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

apps/           # Desktop and VR apps
core/           # Core logic for input, symbol recognition, command mapping
data/           # Symbol dataset (raw, labeled, and processed)
models/         # Trained ML models (ONNX, TFLite)
scripts/        # Training scripts and utilities

---

## 🚀 Quick Start

> **Note:** The project is still in early development. Follow these steps to build the desktop version:

### 1. Clone the repo

```bash
git clone https://github.com/yourusername/symbolcast.git
cd symbolcast

2. Build with CMake

mkdir build && cd build
cmake ..
make

3. Run the desktop app

./symbolcast-desktop



⸻

🧪 Training a Model
	1.	Draw and label symbols via the app
	2.	Export the dataset to data/labeled/
	3.	Run training script:

cd scripts/training
python train_symbol_model.py --data_dir ../../data/labeled --output_model ../../models/symbolcast-v1.onnx



⸻

📦 Dependencies
	•	C++17 or later
	•	Qt 6+ (for GUI)
	•	OpenXR / SteamVR (for VR support)
	•	ONNX Runtime (for model inference)
	•	Python (for training scripts)

⸻

🗺️ Roadmap
	•	Input capture (mouse, trackpad, keyboard)
	•	Symbol recording + labeling
	•	Dataset export + augmentation
	•	Model training and inference
	•	VR input drawing (OpenXR)
	•	Real-time gesture-to-command mapping
	•	EtherOS integration as system-level service

⸻

🤝 Contributing

Pull requests are welcome! For major changes, please open an issue to discuss what you’d like to add or improve.

⸻

📜 License

This project is licensed under the MIT License.

⸻

🌌 Vision

SymbolCast is more than an input engine — it’s a step toward a world where users interact with their OS like spellcasters, using gestures, voice, and intention. Whether on a desktop or in virtual reality, SymbolCast reimagines computing as a symbolic dialogue between human and machine.
