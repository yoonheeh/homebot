# Homebot Project Notes

## Architecture Decisions

### ADR-001: Bazel for C++, uv for Python (decoupled at runtime via IPC)

**Status:** Accepted (2026-06-28)

**Context.** homebot is a polyglot codebase. Performance-critical components
(control, state estimation, the serial/IPC bridge) are written in C++ and run on
the Firefly aarch64 board; Python is used for scripting, ML/perception (RKNN),
and visualization. We need a build/dependency story for both languages.

**Decision.**
- **C++ → Bazel.** Bazel gives us hermetic, reproducible cross-compilation from
  an x86 dev machine to the aarch64 board via a pinned LLVM toolchain, removing
  the need to build on the slow board.
- **Python → uv.** uv owns Python dependency management (`pyproject.toml` +
  `uv.lock`). Python is **not** managed through Bazel (no `rules_python` /
  `pip.parse`). uv handles messy native/ML wheels (numpy, opencv, RKNN) far
  better than Bazel does.
- **The two never link.** C++ and Python communicate only over IPC (serial
  frames / ZeroMQ / stdout pipes) — i.e. across process boundaries. Neither
  imports nor links the other at build time, so there is no build-time
  dependency for a single build system to coordinate.

**Consequences.**
- No build-time seam → two independent build systems is clean, not a compromise.
- The IPC wire format (see the message-frame table below) is the **only** shared
  artifact between the languages, and neither build system enforces it. Treat it
  as a hand-maintained, versioned interface.
- **Revisit if** Python ever needs to link C++ at build time — e.g. pybind11
  extension modules, or shared protobuf/gRPC codegen. At that point pull only the
  shared contract into Bazel (`cc_proto_library` + `py_proto_library`) and keep
  the rest of Python on uv.

## Model Files

### YOLOv5s (RK3588 NPU)
- **File**: `object_detection/model/yolo/yolov5s-640-640.rknn` (download with script below)
- **Download Script**: `object_detection/model/yolo/scripts/download_yolo_rknn.sh`
- **Source**: https://github.com/rockchip-linux/rknpu2/tree/main/examples/rknn_yolov5_demo/model/RK3588
- **Description**: Pre-converted YOLOv5s model for Rockchip RK3588 NPU
- **Format**: RKNN (Rockchip Neural Network)
- **Target**: RK3588 (3x NPU cores, 6 TOPS)

**To download:**
```bash
./object_detection/model/yolo/scripts/download_yolo_rknn.sh
```

### YOLOv5s ReLU (RK3588 NPU)
- **File**: `object_detection/model/yolo/yolov5s_relu.rknn` (download with script below)
- **Download Script**: `object_detection/model/yolo/scripts/download_yolov5_relu.sh`
- **Source**: https://github.com/rockchip-linux/rknpu2/tree/main/examples/rknn_yolov5_demo/model/RK3588
- **Description**: YOLOv5s with ReLU activation (alternative to SiLU), sometimes faster on NPU
- **Format**: RKNN (Rockchip Neural Network)
- **Target**: RK3588 (3x NPU cores, 6 TOPS)

**To download:**
```bash
./object_detection/model/yolo/scripts/download_yolov5_relu.sh
```

## Hardware Setup

### Firefly ITX-3588J
- Ubuntu 22.04
- Kernel 6.1
- RK3588 SoC with NPU

### Elegoo Smart Robot Car V4
- Arduino Uno (motor/sensor control)
- ESP32-CAM (WiFi + camera stream)
- Communication: USB Serial + WiFi

## ESP32-CAM Configuration
- **Mode**: WiFi Access Point (AP)
- **SSID**: `ELEGOO-XXXXXXXX` (MAC-based)
- **Password**: `12341234` (customized)
- **IP**: `192.168.4.1`
- **Stream URL**: `http://192.168.4.1:81/stream`
- **Snapshot URL**: `http://192.168.4.1/capture`

## Scripts

### Stream Viewer
- `scripts/stream-simple.py` - Basic ESP32-CAM stream viewer
- `scripts/stream-yolo.py` - YOLO inference on ESP32-CAM stream

## Dependencies

### RKNN
- `rknn_toolkit_lite2-1.6.0-cp310-cp310-linux_aarch64.whl` - Inference runtime for RK3588
- Source: https://github.com/rockchip-linux/rknn-toolkit2


## Model Evaluation

To verify model accuracy quantitatively against a COCO-mini subset (50 images):

1. **Setup Dataset** (first time only):
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && /home/firefly/.local/bin/uv run scripts/setup_eval_dataset.py"
   ```
2. **Run Evaluation**:
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && /home/firefly/.local/bin/uv run scripts/evaluate_model.py"
   ```

### Current Baseline (yolov5s-640-640.rknn)
- **mAP@0.5**: 49.62%
- **Precision**: 61.05%
- **Recall**: 54.97%
- **Latency**: ~45ms (inference), ~73ms (total)

## Code Architecture

### Centralized Engine
Core YOLO logic is centralized in `object_detection/yolo_engine.py`. Always use the `YoloEngine` class for inference to ensure consistency between evaluation and production.

```python
from object_detection.yolo_engine import YoloEngine
engine = YoloEngine("path/to/model.rknn")
boxes, classes, scores = engine.predict(image)
```

## Building

C++ is built with Bazel; Python deps are managed with uv (see ADR-001).

```bash
# C++ — host (x86_64), for local dev/testing
bazel build //src:system_node

# C++ — cross-compile to the Firefly aarch64 board
bazel build --config=arm64 //src:system_node
# -> bazel-bin/src/system_node  (ELF aarch64; scp to the board to run)

# Python — set up / update the virtualenv from uv.lock
uv sync
```

## Development Workflow

To develop and test on the Rockchip (Firefly) board:

1. **Make changes** on your local machine.
2. **Commit and push** the changes to GitHub:
   ```bash
   git add .
   git commit -m "Your message"
   git push
   ```
3. **Pull from the board**:
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && git pull"
   ```
4. **Run the script** on the board:
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && /home/firefly/.local/bin/uv run scripts/test_inference.py"
   ```
