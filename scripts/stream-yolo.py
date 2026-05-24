"""
YOLO streaming script for ESP32-CAM + RK3588 NPU

Prerequisites:
  uv pip install opencv-python numpy
  uv pip install ./rknn_toolkit_lite2-1.6.0-cp310-cp310-linux_aarch64.whl
"""

import cv2
import numpy as np
import threading
import queue
import time
from rknnlite.api import RKNNLite

# Configuration
STREAM_URL = 'http://192.168.4.1:81/stream'
MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'
INPUT_SIZE = (640, 640)
CONF_THRESH = 0.25
NMS_THRESH = 0.45

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


def sigmoid(x):
    return 1 / (1 + np.exp(-x))


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

    # Reshape: (1, 255, H, W) -> (1, 3, 85, H, W) -> (1, 3, H, W, 85)
    output = output.reshape(batch, num_anchors, 5 + num_classes, height, width)
    output = output.transpose(0, 1, 3, 4, 2)

    # Create grid
    xv, yv = np.meshgrid(np.arange(width), np.arange(height))
    grid = np.stack((xv, yv), axis=2).reshape(1, 1, height, width, 2)

    detections = []

    for a in range(num_anchors):
        anchor = anchors[a]

        # Extract predictions for this anchor
        pred = output[0, a, :, :, :]  # (H, W, 85)

        # Sigmoid for objectness and class scores
        pred[..., 4:] = sigmoid(pred[..., 4:])

        # Find predictions above threshold
        objectness = pred[..., 4]
        mask = objectness > conf_thresh

        if not np.any(mask):
            continue

        # Get positions where mask is True
        ys, xs = np.where(mask)

        for y, x in zip(ys, xs):
            # Decode box
            bx = (sigmoid(pred[y, x, 0]) * 2 - 0.5 + x) * stride
            by = (sigmoid(pred[y, x, 1]) * 2 - 0.5 + y) * stride
            bw = (sigmoid(pred[y, x, 2]) * 2) ** 2 * anchor[0]
            bh = (sigmoid(pred[y, x, 3]) * 2) ** 2 * anchor[1]

            # Convert to corners
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
                detections.append([x1, y1, x2, y2, score, class_id])

    return detections


def postprocess(outputs, orig_shape, input_size, conf_thresh=0.25, nms_thresh=0.45, debug=True):
    """Process YOLO outputs - Rockchip format with 3 feature maps"""

    strides = [8, 16, 32]

    all_detections = []

    if debug:
        print(f"[DEBUG] Number of outputs: {len(outputs)}")
        for i, out in enumerate(outputs):
            print(f"[DEBUG] Output[{i}] shape: {out.shape}")

    # Decode each feature map
    for i, output in enumerate(outputs):
        detections = decode_output(output, ANCHORS[i], strides[i], conf_thresh)
        all_detections.extend(detections)

    if debug:
        print(f"[DEBUG] Total detections before NMS: {len(all_detections)}")

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

        # Clamp to image bounds
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

    if debug:
        print(f"[DEBUG] Detections after NMS: {len(final_detections)}")
        for (box, score, class_id) in final_detections:
            print(f"[DEBUG]   -> {CLASSES[class_id]}: {score:.2f} at {box}")

    return final_detections


def draw_detections(frame, detections):
    """Draw bounding boxes and labels on frame"""
    for (box, score, class_id) in detections:
        x1, y1, x2, y2 = box

        # Draw box
        color = (0, 255, 0)
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

        # Draw label
        label = f"{CLASSES[class_id]}: {score:.2f}"
        label_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)
        label_y = max(y1, label_size[1] + 10)

        cv2.rectangle(frame, (x1, label_y - label_size[1] - 10),
                     (x1 + label_size[0], label_y), color, -1)
        cv2.putText(frame, label, (x1, label_y - 5),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)

    return frame


def print_detection_summary(detections):
    for (box, score, class_id) in detections:
        label = f"{CLASSES[class_id]}: {score:.2f}"
        print(label)


class YOLOStreamer:
    def __init__(self, stream_url, model_path):
        self.stream_url = stream_url
        self.model_path = model_path
        self.cap = None
        self.rknn = None
        self.frame_queue = queue.Queue(maxsize=2)
        self.result_queue = queue.Queue(maxsize=1)
        self.running = False
        self.fps = 0
        self.draw_bounding_box = False

    def init_camera(self):
        """Initialize video capture from ESP32-CAM"""
        self.cap = cv2.VideoCapture(self.stream_url)
        if not self.cap.isOpened():
            raise RuntimeError(f"Failed to open stream: {self.stream_url}")

        # Set buffer size to 1 to minimize latency
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        print(f"Connected to stream: {self.stream_url}")

    def init_model(self):
        """Initialize RKNN model"""
        self.rknn = RKNNLite()
        ret = self.rknn.load_rknn(self.model_path)
        if ret != 0:
            raise RuntimeError(f"Failed to load RKNN model: {ret}")

        # Use all NPU cores for maximum performance
        ret = self.rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_AUTO)
        if ret != 0:
            raise RuntimeError(f"Failed to init RKNN runtime: {ret}")

        print(f"Loaded model: {self.model_path}")

    def capture_thread(self):
        """Continuously capture frames from camera"""
        print("Capture thread started")
        while self.running:
            ret, frame = self.cap.read()
            if not ret:
                print("Failed to read frame, retrying...")
                time.sleep(0.1)
                continue

            # Drop old frame if queue is full (keep latest)
            if self.frame_queue.full():
                try:
                    self.frame_queue.get_nowait()
                except queue.Empty:
                    pass

            self.frame_queue.put(frame)

    def inference_thread(self):
        """Run YOLO inference on captured frames"""
        print("Inference thread started")
        frame_count = 0
        start_time = time.time()

        while self.running:
            try:
                frame = self.frame_queue.get(timeout=1.0)
            except queue.Empty:
                continue

            orig_shape = frame.shape[:2]

            # Preprocess
            input_img = preprocess(frame, INPUT_SIZE)

            # Inference on NPU
            outputs = self.rknn.inference(inputs=[input_img])

            # Postprocess
            detections = postprocess(outputs, orig_shape, INPUT_SIZE,
                                    CONF_THRESH, NMS_THRESH)

            # Update result queue (keep latest)
            if self.result_queue.full():
                try:
                    self.result_queue.get_nowait()
                except queue.Empty:
                    pass
            self.result_queue.put((frame.copy(), detections))

            # Calculate FPS
            frame_count += 1
            elapsed = time.time() - start_time
            if elapsed >= 1.0:
                self.fps = frame_count / elapsed
                frame_count = 0
                start_time = time.time()

    def run(self):
        """Main run loop"""
        self.running = True

        # Initialize
        self.init_camera()
        self.init_model()

        # Start threads
        capture_t = threading.Thread(target=self.capture_thread)
        infer_t = threading.Thread(target=self.inference_thread)

        capture_t.start()
        infer_t.start()

        print("Streaming started. Press 'q' to quit, 's' to save snapshot")

        try:
            while self.running:
                # Get latest result
                try:
                    frame, detections = self.result_queue.get(timeout=0.1)
                except queue.Empty:
                    continue

                if self.draw_bounding_box:
                    # Draw detections
                    display = draw_detections(frame, detections)

                    # Draw FPS
                    cv2.putText(display, f"FPS: {self.fps:.1f}", (10, 30),
                               cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

                    # Show frame
                    cv2.imshow('YOLO + ESP32-CAM', display)

                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q'):
                        print("Quitting...")
                        break
                    elif key == ord('s'):
                        timestamp = time.strftime("%Y%m%d_%H%M%S")
                        filename = f"snapshot_{timestamp}.jpg"
                        cv2.imwrite(filename, display)
                        print(f"Saved: {filename}")
                else:
                    # print instead of drawing detection
                    print_detection_summary(detections)

        except KeyboardInterrupt:
            print("Interrupted by user")

        finally:
            self.running = False
            capture_t.join(timeout=2)
            infer_t.join(timeout=2)

            if self.cap:
                self.cap.release()
            if self.rknn:
                self.rknn.release()

            if self.draw_bounding_box:
                cv2.destroyAllWindows()
            print("Cleanup complete")


if __name__ == '__main__':
    # Verify model path exists
    import os
    if not os.path.exists(MODEL_PATH):
        print(f"Model not found: {MODEL_PATH}")
        print("Please update MODEL_PATH to point to your .rknn model")
        exit(1)

    streamer = YOLOStreamer(STREAM_URL, MODEL_PATH)
    streamer.run()
