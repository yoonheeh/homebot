"""
Test YOLO inference on a single image - Exact Rockchip implementation

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


def preprocess(img, input_size):
    """Resize and prepare image for YOLO input - RKNN models usually expect uint8"""
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, input_size)
    # Most RKNN models have normalization baked in, so they expect uint8 [0, 255]
    # If the model expects float32 [0, 1], uncomment the following lines:
    # img = img.astype(np.float32) / 255.0
    img = np.expand_dims(img, axis=0)
    return img


def xywh2xyxy(x):
    """Convert [x, y, w, h] to [x1, y1, x2, y2]"""
    y = np.copy(x)
    y[:, 0] = x[:, 0] - x[:, 2] / 2  # top left x
    y[:, 1] = x[:, 1] - x[:, 3] / 2  # top left y
    y[:, 2] = x[:, 0] + x[:, 2] / 2  # bottom right x
    y[:, 3] = x[:, 1] + x[:, 3] / 2  # bottom right y
    return y


def process(input_data, mask, anchors):
    """Exact copy from Rockchip test.py"""
    anchors = [anchors[i] for i in mask]
    grid_h, grid_w = map(int, input_data.shape[0:2])

    box_confidence = input_data[..., 4]
    box_confidence = np.expand_dims(box_confidence, axis=-1)

    box_class_probs = input_data[..., 5:]

    # Debug: print max confidence for this layer
    print(f"  Layer {grid_h}x{grid_w} max confidence: {np.max(box_confidence):.4f}")

    box_xy = input_data[..., :2]*2 - 0.5

    col = np.tile(np.arange(0, grid_w), grid_w).reshape(-1, grid_w)
    row = np.tile(np.arange(0, grid_h).reshape(-1, 1), grid_h)
    col = col.reshape(grid_h, grid_w, 1, 1).repeat(3, axis=-2)
    row = row.reshape(grid_h, grid_w, 1, 1).repeat(3, axis=-2)
    grid = np.concatenate((col, row), axis=-1)
    box_xy += grid
    box_xy *= int(INPUT_SIZE[0]/grid_h)

    box_wh = pow(input_data[..., 2:4]*2, 2)
    box_wh = box_wh * anchors

    box = np.concatenate((box_xy, box_wh), axis=-1)

    return box, box_confidence, box_class_probs


def filter_boxes(boxes, box_confidences, box_class_probs, conf_thresh=0.25):
    """Exact copy from Rockchip test.py"""
    boxes = boxes.reshape(-1, 4)
    box_confidences = box_confidences.reshape(-1)
    box_class_probs = box_class_probs.reshape(-1, box_class_probs.shape[-1])

    _box_pos = np.where(box_confidences >= conf_thresh)
    boxes = boxes[_box_pos]
    box_confidences = box_confidences[_box_pos]
    box_class_probs = box_class_probs[_box_pos]

    class_max_score = np.max(box_class_probs, axis=-1)
    classes = np.argmax(box_class_probs, axis=-1)
    _class_pos = np.where(class_max_score >= conf_thresh)

    boxes = boxes[_class_pos]
    classes = classes[_class_pos]
    scores = (class_max_score * box_confidences)[_class_pos]

    return boxes, classes, scores


def nms_boxes(boxes, scores, nms_thresh=0.45):
    """Exact copy from Rockchip test.py"""
    x = boxes[:, 0]
    y = boxes[:, 1]
    w = boxes[:, 2] - boxes[:, 0]
    h = boxes[:, 3] - boxes[:, 1]

    areas = w * h
    order = scores.argsort()[::-1]

    keep = []
    while order.size > 0:
        i = order[0]
        keep.append(i)

        xx1 = np.maximum(x[i], x[order[1:]])
        yy1 = np.maximum(y[i], y[order[1:]])
        xx2 = np.minimum(x[i] + w[i], x[order[1:]] + w[order[1:]])
        yy2 = np.minimum(y[i] + h[i], y[order[1:]] + h[order[1:]])

        w1 = np.maximum(0.0, xx2 - xx1 + 0.00001)
        h1 = np.maximum(0.0, yy2 - yy1 + 0.00001)
        inter = w1 * h1

        ovr = inter / (areas[i] + areas[order[1:]] - inter)
        inds = np.where(ovr <= nms_thresh)[0]
        order = order[inds + 1]
    keep = np.array(keep)
    return keep


def yolov5_post_process(input_data):
    """Exact copy from Rockchip test.py"""
    masks = [[0, 1, 2], [3, 4, 5], [6, 7, 8]]
    anchors = [[10, 13], [16, 30], [33, 23], [30, 61], [62, 45],
               [59, 119], [116, 90], [156, 198], [373, 326]]

    boxes, classes, scores = [], [], []
    for input, mask in zip(input_data, masks):
        b, c, s = process(input, mask, anchors)
        b, c, s = filter_boxes(b, c, s)
        boxes.append(b)
        classes.append(c)
        scores.append(s)

    boxes = np.concatenate(boxes)
    boxes = xywh2xyxy(boxes)
    classes = np.concatenate(classes)
    scores = np.concatenate(scores)

    nboxes, nclasses, nscores = [], [], []
    for c in set(classes):
        inds = np.where(classes == c)
        b = boxes[inds]
        c = classes[inds]
        s = scores[inds]

        keep = nms_boxes(b, s)

        nboxes.append(b[keep])
        nclasses.append(c[keep])
        nscores.append(s[keep])

    if not nclasses and not nscores:
        return None, None, None

    boxes = np.concatenate(nboxes)
    classes = np.concatenate(nclasses)
    scores = np.concatenate(nscores)

    return boxes, classes, scores


def draw(image, boxes, scores, classes):
    """Draw the boxes on the image - from Rockchip test.py"""
    print("\n{:^12} {:^12}  {}".format('class', 'score', 'xmin, ymin, xmax, ymax'))
    print('-' * 50)
    for box, score, cl in zip(boxes, scores, classes):
        top, left, right, bottom = box
        top = int(top)
        left = int(left)
        right = int(right)
        bottom = int(bottom)

        cv2.rectangle(image, (top, left), (right, bottom), (255, 0, 0), 2)
        cv2.putText(image, '{0} {1:.2f}'.format(CLASSES[cl], score),
                    (top, left - 6),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6, (0, 0, 255), 2)

        print("{:^12} {:^12.3f} [{:>4}, {:>4}, {:>4}, {:>4}]".format(CLASSES[cl], score, top, left, right, bottom))


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

    print(f"Raw output shapes: {[o.shape for o in outputs]}")

    # Reshape like Rockchip does
    print("Reshaping outputs...")
    input0_data = outputs[0]
    input1_data = outputs[1]
    input2_data = outputs[2]

    input0_data = input0_data.reshape([3, -1]+list(input0_data.shape[-2:]))
    input1_data = input1_data.reshape([3, -1]+list(input1_data.shape[-2:]))
    input2_data = input2_data.reshape([3, -1]+list(input2_data.shape[-2:]))

    input_data = list()
    input_data.append(np.transpose(input0_data, (2, 3, 0, 1)))
    input_data.append(np.transpose(input1_data, (2, 3, 0, 1)))
    input_data.append(np.transpose(input2_data, (2, 3, 0, 1)))

    print(f"Reshaped: {[d.shape for d in input_data]}")

    # Postprocess
    print("Postprocessing...")
    boxes, classes, scores = yolov5_post_process(input_data)

    if boxes is None:
        print("No detections found!")
    else:
        print(f"Detections found: {len(boxes)}")
        # Draw and save
        draw(img, boxes, scores, classes)
        cv2.imwrite(OUTPUT_IMAGE, img)
        print(f"\nSaved result to: {OUTPUT_IMAGE}")

    rknn.release()


if __name__ == '__main__':
    main()
