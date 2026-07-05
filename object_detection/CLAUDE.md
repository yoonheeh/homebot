# object_detection/

Rockchip RK3588 NPU object-detection pipeline. This is the centralized home for YOLO inference; both evaluation scripts and production streaming use `YoloEngine` so numbers stay consistent.

## Files

- `yolo_engine.py` — main inference class.
  - Loads an `.rknn` model via `rknnlite.api.RKNNLite`.
  - Runs letterbox resize, NPU inference, YOLOv5-style post-processing (anchor decoding, confidence filtering, class selection, per-class NMS).
  - Returns bounding boxes, class IDs, and scores mapped back to original image coordinates.
  - Includes the 80-class COCO label list.
- `model/yolo/scripts/download_yolo_rknn.sh` — helper to download a pre-converted YOLOv5s RKNN model.
- `model/yolo/scripts/download_yolov5_relu.sh` — helper to download the ReLU-activated variant.

## Model files

Models are not committed. Download them with the scripts above; they land at paths like:

- `object_detection/model/yolo/yolov5s-640-640.rknn`
- `object_detection/model/yolo/yolov5s_relu.rknn`

## Usage

```python
from object_detection.yolo_engine import YoloEngine

engine = YoloEngine("object_detection/model/yolo/yolov5s-640-640.rknn")
boxes, classes, scores = engine.predict(image)
engine.release()
```

## Relationship to other parts

- Imported by `scripts/test_inference.py`, `scripts/stream-yolo.py`, and `scripts/evaluate_model.py`.
- Depends on `rknn_toolkit_lite2` (installed from the committed wheel at repo root) plus `numpy` and `opencv-python`.
- Evaluation numbers are recorded in `NOTES.md`.
