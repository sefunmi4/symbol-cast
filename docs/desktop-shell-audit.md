# Desktop Shell Audit and Migration Notes

## 1. Current Qt Entry Point and Window Responsibilities

### Application bootstrap and tray controls
* `main.cpp` wires command-line options for ripple animation, stroke styling, detection overlay color, fullscreen mode, and cursor animation toggles before instantiating the canvas window, so downstream shells must expose equivalent configuration knobs.【F:apps/desktop/main.cpp†L54-L111】
* The helper `presentWindow` centralizes show/restore/fullscreen transitions and performs Mac-specific app activation to ensure the window surfaces above other apps.【F:apps/desktop/main.cpp†L104-L117】
* System-tray integration governs lifetime: when available, the tray menu exposes a toggle for showing the "Symbol Keyboard" window and a quit action; it also keeps the process alive when the main window hides. The `CanvasWindow::visibilityChanged` signal keeps the tray toggle in sync.【F:apps/desktop/main.cpp†L119-L176】

### CanvasWindow surface, chrome, and layout
* The widget runs frameless, translucent, and always-on-top with hidden cursor, custom border styling, and Mac-style traffic-light controls (close/minimize/maximize) that collapse in fullscreen.【F:apps/desktop/CanvasWindow.hpp†L70-L150】
* Idle instructions render via a QLabel overlay that hides on interaction and reappears after a five-second timer; a separate hover label and timer provide transient feedback for predictions and macro execution.【F:apps/desktop/CanvasWindow.hpp†L113-L170】【F:apps/desktop/CanvasWindow.hpp†L736-L742】
* Settings and macro UI: a right-aligned settings toolbutton opens a menu that toggles a macro panel, while the panel itself contains per-symbol combo boxes and glyph labels for mapping gestures to clipboard actions or custom commands.【F:apps/desktop/CanvasWindow.hpp†L754-L833】【F:apps/desktop/CanvasWindow.hpp†L781-L804】

### Gesture capture, window movement, and resizing
* Mouse handlers multiplex between window management (dragging, resizing via custom hit-testing) and gesture capture; multi-tap state is derived from `InputManager::onTapSequence`, enabling double-tap start/submit semantics and training triggers.【F:apps/desktop/CanvasWindow.hpp†L227-L389】
* Cursor animations (ripples and traces) append points on press/move, while the idle label resets whenever interaction occurs.【F:apps/desktop/CanvasWindow.hpp†L250-L297】【F:apps/desktop/CanvasWindow.hpp†L643-L647】

### Rendering and animation loop
* `paintEvent` draws the rounded backdrop, cursor effects, stroke paths, detection bounding box, and dashed prediction overlays, using `CanvasWindowOptions` styling parameters.【F:apps/desktop/CanvasWindow.hpp†L397-L471】
* A 10ms timer drives `onFrame`, which advances ripple radii, decays cursor trace life, fades completed strokes, and eases out the prediction overlay before repainting.【F:apps/desktop/CanvasWindow.hpp†L157-L161】【F:apps/desktop/CanvasWindow.hpp†L596-L627】

### Recognition pipeline, macros, and persistence
* `onSubmit` first queries `GestureRecognizer` for command bindings, falls back to `RecognizerRouter` for symbol inference, and optionally runs TrOCR decoding to obtain glyph suggestions, feeding hover feedback and logging. Macro bindings can override symbol outputs or clipboard behavior.【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L480-L548】
* Users can label new gestures (`Ctrl+T` shortcut) via dialogs that save into `data/user_gestures.json`, with optional augmentation to enrich samples.【F:apps/desktop/CanvasWindow.hpp†L92-L104】【F:apps/desktop/CanvasWindow.hpp†L557-L592】
* Macro bindings load from `config/commands.json` plus user overrides in `data/macro_bindings.json`, defaulting when unspecified; updates persist back to disk and update combo-box state without recursive signal loops.【F:apps/desktop/CanvasWindow.hpp†L920-L1003】【F:apps/desktop/CanvasWindow.hpp†L876-L915】
* Palette configuration feeds both macro glyph displays and TrOCR post-processing, allowing overrides per codepoint or fallback cycling when undefined.【F:apps/desktop/CanvasWindow.hpp†L1004-L1067】【F:apps/desktop/CanvasWindow.hpp†L930-L999】
* Shortcuts cover quitting (Esc/Ctrl+C), prediction toggling (P), training (Ctrl+T), and undo/redo for gesture profiles, ensuring parity requirements for any alternative shell.【F:apps/desktop/CanvasWindow.hpp†L81-L105】

### Visibility semantics
* Overridden close/hide/show events emit `visibilityChanged`, and `setHideOnClose` lets the tray logic transform window closes into hides without quitting, a behavior new shells must replicate to keep tray toggles coherent.【F:apps/desktop/CanvasWindow.hpp†L183-L225】【F:apps/desktop/main.cpp†L138-L156】

## 2. Rehost Decision: Native Qt vs. Tauri/Electron

| Option | Pros | Cons | Required Bridging |
| --- | --- | --- | --- |
| **Embed existing Qt window** | Minimal rewrite; retains mature gesture capture, recognition, and macro UI. Qt already handles timers, rendering, and native tray integration.【F:apps/desktop/CanvasWindow.hpp†L227-L627】【F:apps/desktop/main.cpp†L119-L176】 | Mixed technology stack if paired with web front end; harder to style with modern web UI; larger binary footprint; platform-specific tray quirks remain.【F:apps/desktop/CanvasWindow.hpp†L754-L833】 | Provide an embedding surface (e.g., Qt inside native host window) and expose control hooks (show/hide, option updates). Maintain existing file-based configs and ensure the host drives command-line analogues.【F:apps/desktop/main.cpp†L54-L111】 |
| **Rewrite shell in Tauri/Electron** | Unlocks web-based UI/UX iteration, easier theming, cross-platform auto-update ecosystem, integration with system accessibility APIs via web stack. | Must reimplement gesture capture surface with hardware-accelerated canvas, custom cursor effects, and macro panel; requires bridging to C++ recognition/runtime for InputManager, recognizer router, and TrOCR decoding; tray behavior must be re-coded per platform using JS APIs; duplicating high-frequency rendering in JS may introduce latency unless native modules handle sampling.【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L227-L548】 | Establish IPC between the web UI and native gesture/recognition backend; stream pointer events at high frequency; expose commands for macro persistence and palette configuration; mirror tray toggles by forwarding visibility events through IPC.【F:apps/desktop/main.cpp†L119-L176】【F:apps/desktop/CanvasWindow.hpp†L876-L1003】 |

Recommendation: If rapid UI iteration or integration with other web tooling is paramount, migrate to Tauri/Electron with a native Rust/Node sidecar that wraps the existing C++ recognition libraries. Otherwise, embedding Qt preserves functionality with far less risk.

## 3. Tauri/Electron Module Map and Native Service Requirements

### Proposed module structure
1. **Frontend (React/Vue/Svelte or vanilla)**
   * Canvas component with WebGL/Canvas2D rendering for strokes, ripples, prediction overlays, detection rectangles, and hover feedback, matching `paintEvent` behavior.【F:apps/desktop/CanvasWindow.hpp†L397-L471】
   * UI chrome reproducing custom window controls, settings button, macro panel (combo boxes and glyph labels), and idle instructions with timers.【F:apps/desktop/CanvasWindow.hpp†L113-L150】【F:apps/desktop/CanvasWindow.hpp†L754-L833】
   * Shortcut handling replicating Esc, Ctrl+C, Ctrl+T, Ctrl+Z/Y, and P toggles, forwarding actions to backend IPC channels.【F:apps/desktop/CanvasWindow.hpp†L81-L105】

2. **Frontend state management**
   * Gesture capture state machine implementing tap-sequence semantics (double-tap start/submit, reset, training) that mirrors `InputManager::onTapSequence` decisions via backend feedback.【F:apps/desktop/CanvasWindow.hpp†L250-L296】
   * Animation loop to decay ripples, traces, strokes, and prediction opacity akin to `onFrame`. Consider running this logic client-side to avoid IPC jitter.【F:apps/desktop/CanvasWindow.hpp†L596-L627】

3. **Backend (Tauri Rust command handlers / Electron native addon)**
   * **Pointer sampling service**: optional native module to collect high-frequency pointer events and feed them to the C++ `InputManager` if web timers prove insufficient.【F:apps/desktop/CanvasWindow.hpp†L227-L373】
   * **Recognition service**: wrap `sc::GestureRecognizer`, `sc::RecognizerRouter`, and associated model runners to accept point arrays, return predicted symbols, commands, and macro suggestions, and handle training persistence to `data/user_gestures.json`.【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L480-L592】
   * **Macro/Palette config service**: expose read/write APIs for `config/commands.json`, `data/macro_bindings.json`, and palette overrides, ensuring concurrency-safe updates similar to `loadMacroBindingsFromConfig` and `saveMacroBindings`.【F:apps/desktop/CanvasWindow.hpp†L876-L1003】
   * **TrOCR inference adapter**: if TrOCR is enabled, bind `sc::TrocrDecoder` to accept rasterized glyphs from the frontend and return codepoint suggestions leveraging palette mapping.【F:apps/desktop/CanvasWindow.hpp†L174-L175】【F:apps/desktop/CanvasWindow.hpp†L484-L531】
   * **Tray/visibility controller**: manage system tray menu, toggle events, and propagate visibility changes to the web UI to match current hide-on-close semantics.【F:apps/desktop/main.cpp†L119-L176】【F:apps/desktop/CanvasWindow.hpp†L183-L225】

### Native C++ components to surface as services
* `core/input/InputManager` – maintains tap sequences and gesture capture states; should process streamed pointer samples and emit control actions (start/end/reset/train).【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L250-L283】
* `core/recognition/GestureRecognizer` (plus model runners) – handles command gestures, undo/redo, training persistence, and recognition confidence APIs.【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L504-L592】
* `core/recognition/RecognizerRouter` – resolves symbols to commands/macros and supplies prediction previews.【F:apps/desktop/CanvasWindow.hpp†L3-L8】【F:apps/desktop/CanvasWindow.hpp†L690-L733】
* `core/recognition/TrocrDecoder` – optional OCR for handwriting-to-text mapping used during submission.【F:apps/desktop/CanvasWindow.hpp†L174-L175】【F:apps/desktop/CanvasWindow.hpp†L484-L531】
* Macro persistence helpers (currently inline in `CanvasWindow`) may migrate to a standalone service to avoid duplicating JSON handling in JavaScript.【F:apps/desktop/CanvasWindow.hpp†L876-L1003】

Bridging these modules over IPC allows the web shell to focus on rendering and UX while preserving the proven recognition pipeline implemented in C++.
