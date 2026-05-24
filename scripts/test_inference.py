"""
Test YOLO inference on a single image

Usage:
  uv run scripts/test_inference.py path/to/image.jpg
  uv run scripts/test_inference.py  # uses data/snapshot.jpg
"""

import sys
import os
import cv2
import numpy as np
import time
from rknnlite.api import RKNNLite

# Configuration
MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'
INPUT_SIZE = (640, 640)
CONF_THRESH = 0.25
NMS_THRESH = 0.45
DEFAULT_IMAGE = 'data/snapshot.jpg'
OUTPUT_IMAGE = 'data/result.jpg'

# COCO class names
CLASSES = ['person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
           'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
           'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
           'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
           'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
           'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
           'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake', 'chair',
           'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop', 'mouse',
           'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink', 'refrigerator',
           'book', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush']

# YOLOv5 anchors
ANCHORS = [
    [[10, 13], [16, 30], [33, 23]],      # P3/8
    [[30, 61], [62, 45], [59, 119]],     # P4/16
    [[116, 90], [156, 198], [373, 326]]  # P5/32
]


def preprocess(img, input_size):
    """Resize and normalize image for YOLO input"""
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, input_size)
    img = img.astype(np.float32) / 255.0
    img = np.expand_dims(img, axis=0)
    return img


def decode_output(output, anchors, stride, conf_thresh=0.25):
    """Decode YOLOv5 feature map to detections"""
    batch, channel, height, width = output.shape
    num_anchors = len(anchors)
    num_classes = 80

    # Reshape: (1, 255, H, W) -> (3, 85, H, W)
    output = output.reshape(num_anchors, 5 + num_classes, height, width)

    # Transpose: (3, 85, H, W) -> (H, W, 3, 85)
    output = np.transpose(output, (2, 3, 0, 1))

    boxes = []
    obj_probs = []
    class_ids = []

    for a in range(num_anchors):
        anchor = anchors[a]
        pred = output[:, :, a, :]
        obj_score = pred[:, :, 4]
        mask = obj_score > conf_thresh

        if not np.any(mask):
            continue

        ys, xs = np.where(mask)

        for y, x in zip(ys, xs):
            bx = (pred[y, x, 0] * 2 - 0.5 + x) * stride
            by = (pred[y, x, 1] * 2 - 0.5 + y) * stride
            bw = ((pred[y, x, 2] * 2) ** 2) * anchor[0]
            bh = ((pred[y, x, 3] * 2) ** 2) * anchor[1]

            x1 = bx - bw / 2
            y1 = by - bh / 2
            x2 = bx + bw / 2
            y2 = by + bh / 2

            conf = pred[y, x, 4]
            class_scores = pred[y, x, 5:]
            class_id = np.argmax(class_scores)
            class_score = class_scores[class_id]
            score = conf * class_score

            if score > conf_thresh:
                boxes.append([x1, y1, x2, y2])
                obj_probs.append(score)
                class_ids.append(class_id)

    detections = []
    for box, score, class_id in zip(boxes, obj_probs, class_ids):
        detections.append([box[0], box[1], box[2], box[3], score, class_id])

    return detections


def postprocess(outputs, orig_shape, input_size, conf_thresh=0.25, nms_thresh=0.45):
    """Process YOLO outputs"""
    strides = [8, 16, 32]
    all_detections = []

    for i, output in enumerate(outputs):
        detections = decode_output(output, ANCHORS[i], strides[i], conf_thresh)
        all_detections.extend(detections)

    if len(all_detections) == 0:
        return []

    # Scale to original image size
    scale_x = orig_shape[1] / input_size[0]
    scale_y = orig_shape[0] / input_size[1]

    boxes = []
    scores = []
    class_ids = []

    for det in all_detections:
        x1, y1, x2, y2, score, class_id = det
        x1 = int(x1 * scale_x)
        y1 = int(y1 * scale_y)
        x2 = int(x2 * scale_x)
        y2 = int(y2 * scale_y)

        x1 = max(0, min(x1, orig_shape[1] - 1))
        y1 = max(0, min(y1, orig_shape[0] - 1))
        x2 = max(0, min(x2, orig_shape[1] - 1))
        y2 = max(0, min(y2, orig_shape[0] - 1))

        boxes.append([x1, y1, x2, y2])
        scores.append(score)
        class_ids.append(int(class_id))

    # Apply NMS
    final_detections = []
    if len(boxes) > 0:
        indices = cv2.dnn.NMSBoxes(boxes, scores, conf_thresh, nms_thresh)
        if len(indices) > 0:
            indices = indices.flatten() if hasattr(indices, 'flatten') else indices
            for i in indices:
                final_detections.append((boxes[i], scores[i], class_ids[i]))

    return final_detections


def draw_detections(frame, detections):
    """Draw bounding boxes and labels on frame"""
    for (box, score, class_id) in detections:
        x1, y1, x2, y2 = box
        color = (0, 255, 0)
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

        label = f"{CLASSES[class_id]}: {score:.2f}"
        label_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)
        label_y = max(y1, label_size[1] + 10)

        cv2.rectangle(frame, (x1, label_y - label_size[1] - 10),
                     (x1 + label_size[0], label_y), color, -1)
        cv2.putText(frame, label, (x1, label_y - 5),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)

    return frame


def main():
    # Get image path
    if len(sys.argv) > 1:
        image_path = sys.argv[1]
    else:
        image_path = DEFAULT_IMAGE

    if not os.path.exists(image_path):
        print(f"ERROR: Image not found: {image_path}")
        sys.exit(1)

    if not os.path.exists(MODEL_PATH):
        print(f"ERROR: Model not found: {MODEL_PATH}")
        sys.exit(1)

    print(f"Loading image: {image_path}")
    img = cv2.imread(image_path)
    if img is None:
        print(f"ERROR: Failed to load image")
        sys.exit(1)

    orig_shape = img.shape[:2]
    print(f"Image shape: {orig_shape}")

    # Preprocess
    print("Preprocessing...")
    input_img = preprocess(img, INPUT_SIZE)

    # Load model
    print(f"Loading model: {MODEL_PATH}")
    rknn = RKNNLite()
    ret = rknn.load_rknn(MODEL_PATH)
    if ret != 0:
        print(f"ERROR: Failed to load model (ret={ret})")
        sys.exit(1)

    ret = rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_AUTO)
    if ret != 0:
        print(f"ERROR: Failed to init runtime (ret={ret})")
        sys.exit(1)

    # Inference
    print("Running inference...")
    start = time.time()
    outputs = rknn.inference(inputs=[input_img])
    inference_time = (time.time() - start) * 1000
    print(f"Inference time: {inference_time:.1f}ms")

    print(f"Output shapes: {[o.shape for o in outputs]}")

    # Postprocess
    print("Postprocessing...")
    detections = postprocess(outputs, orig_shape, INPUT_SIZE, CONF_THRESH, NMS_THRESH)

    print(f"Detections found: {len(detections)}")
    for (box, score, class_id) in detections:
        print(f"  -> {CLASSES[class_id]}: {score:.2f} at {box}")

    # Draw and save
    result = draw_detections(img.copy(), detections)
    cv2.imwrite(OUTPUT_IMAGE, result)
    print(f"Saved result to: {OUTPUT_IMAGE}")

    rknn.release()


if __name__ == '__main__':
    main()
