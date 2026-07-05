# data/

Runtime data directory for calibration, snapshots, evaluation datasets, and inference outputs.

## Contents

- `snapshot.jpg` — sample camera frame, often used as the default input for `scripts/test_inference.py`.
- `robot_calibration.txt` (created by `pico_interface/calibrate_encoders`) — stores the robot’s physical odometry parameters:
  - `wheel_radius` — nominal wheel radius in meters.
  - `wheel_base` — track width in meters.
  - `ticks_per_rev` — encoder counts per wheel revolution.
  - `scale_factor` — linear odometry correction computed during calibration.
- `evaluation/` (created by `scripts/setup_eval_dataset.py`) — COCO-mini evaluation dataset used by `scripts/evaluate_model.py`.
- `result.jpg` (created by `scripts/test_inference.py`) — annotated YOLO inference output.

## How it is used

- `pico_interface/main.cpp` reads `data/robot_calibration.txt` by default to configure the EKF wheel geometry.
- `pico_interface/calibrate_encoders.cpp` writes the calibration file.
- `scripts/test_inference.py` reads `data/snapshot.jpg` as the default input image.
- `scripts/evaluate_model.py` reads images and ground-truth JSON from `data/evaluation/`.

## Notes

- This directory is not a source-code package; files here are generated, downloaded, or produced during development.
- Do not commit large model files or datasets here. Use `.gitignore` to exclude generated outputs and downloaded evaluation data.
