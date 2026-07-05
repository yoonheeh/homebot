# scripts/

Python utilities and Bazel deployment helpers for Homebot.

## Python scripts

- `stream-simple.py` — basic OpenCV viewer for the ESP32-CAM WiFi stream (`http://192.168.4.1:81/stream`). Supports FPS overlay and snapshot capture.
- `stream-yolo.py` — threaded ESP32-CAM stream viewer that runs YOLO inference on the Firefly NPU using `object_detection.yolo_engine.YoloEngine`. Draws bounding boxes and labels.
- `test_inference.py` — runs a single YOLO inference on `data/snapshot.jpg` (or a provided image) and saves the annotated result to `data/result.jpg`.
- `evaluate_model.py` — evaluates a YOLO model against the COCO-mini evaluation dataset in `data/evaluation/` and reports mAP@0.5, precision, and recall.
- `setup_eval_dataset.py` — downloads/prepares the COCO-mini evaluation dataset under `data/evaluation/`.
- `check_model.py` — model sanity/check helper.
- `debug_postprocess.py` — debugging aid for YOLO post-processing.
- `quick-test.py` — quick smoke test.

## Deployment helpers

- `ship_to_board.sh` — builds `pico_interface` binaries for aarch64 and scp’s them to the robot board.
  - Usage: `scripts/ship_to_board.sh user@host [dest_dir]`
  - Also callable via Bazel: `bazel run --config=arm64 //scripts:ship_to_board -- user@host`
- `remote_run.bzl` — Bazel starlark rule used by `pico_interface` to cross-compile, copy, and remotely execute a binary over SSH.

## Bazel build file

- `BUILD.bazel` — defines `//scripts:ship_to_board`, a `sh_binary` that depends on the four `pico_interface` executables so Bazel builds them before deployment.

## Dependencies

Most Python scripts need the `uv` environment (`uv sync`). They depend on packages listed in `pyproject.toml` plus the committed `rknn_toolkit_lite2` wheel at repo root.

## Common commands

```bash
uv run python scripts/stream-simple.py
uv run python scripts/test_inference.py data/snapshot.jpg
uv run python scripts/evaluate_model.py
uv run python scripts/setup_eval_dataset.py

# Deploy host binaries to the Firefly board
bazel run --config=arm64 //scripts:ship_to_board -- firefly@88.88.88.165
```
