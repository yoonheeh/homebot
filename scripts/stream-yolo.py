"""
YOLO streaming script for ESP32-CAM + RK3588 NPU
Uses centralized YoloEngine for consistent inference.

Prerequisites:
  uv pip install opencv-python numpy
  uv pip install ./rknn_toolkit_lite2-1.6.0-cp310-cp310-linux_aarch64.whl
"""

import cv2
import numpy as np
import threading
import queue
import time
import os
import sys

# Add project root to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from object_detection.yolo_engine import YoloEngine

# Configuration
STREAM_URL = 'http://192.168.4.1:81/stream'
MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'
CONF_THRESH = 0.25
NMS_THRESH = 0.45

def draw_detections(frame, detections, class_names):
    """Draw bounding boxes and labels on frame"""
    boxes, classes, scores = detections
    if boxes is None:
        return frame

    for box, score, class_id in zip(boxes, scores, classes):
        x1, y1, x2, y2 = map(int, box)

        # Draw box
        color = (0, 255, 0)
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

        # Draw label
        label = f"{class_names[class_id]}: {score:.2f}"
        label_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)
        label_y = max(y1, label_size[1] + 10)

        cv2.rectangle(frame, (x1, label_y - label_size[1] - 10),
                     (x1 + label_size[0], label_y), color, -1)
        cv2.putText(frame, label, (x1, label_y - 5),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)

    return frame


def print_detection_summary(detections, class_names):
    boxes, classes, scores = detections
    if boxes is None:
        return
    for _, score, class_id in zip(boxes, scores, classes):
        label = f"{class_names[class_id]}: {score:.2f}"
        print(label)


class YOLOStreamer:
    def __init__(self, stream_url, model_path, conf_thresh=0.25, nms_thresh=0.45):
        self.stream_url = stream_url
        self.model_path = model_path
        self.conf_thresh = conf_thresh
        self.nms_thresh = nms_thresh
        self.cap = None
        self.engine = None
        self.frame_queue = queue.Queue(maxsize=2)
        self.result_queue = queue.Queue(maxsize=1)
        self.running = False
        self.fps = 0
        self.draw_bounding_box = True # Default to True for streaming

    def init_camera(self):
        """Initialize video capture from ESP32-CAM"""
        self.cap = cv2.VideoCapture(self.stream_url)
        if not self.cap.isOpened():
            raise RuntimeError(f"Failed to open stream: {self.stream_url}")

        # Set buffer size to 1 to minimize latency
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        print(f"Connected to stream: {self.stream_url}")

    def init_engine(self):
        """Initialize YoloEngine"""
        print(f"Initializing YoloEngine with model: {self.model_path}")
        self.engine = YoloEngine(
            self.model_path, 
            conf_thresh=self.conf_thresh, 
            nms_thresh=self.nms_thresh
        )

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

            # Inference using centralized engine
            boxes, classes, scores = self.engine.predict(frame)

            # Update result queue (keep latest)
            if self.result_queue.full():
                try:
                    self.result_queue.get_nowait()
                except queue.Empty:
                    pass
            self.result_queue.put((frame.copy(), (boxes, classes, scores)))

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
        self.init_engine()

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
                    display = draw_detections(frame, detections, self.engine.CLASSES)

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
                    print_detection_summary(detections, self.engine.CLASSES)

        except KeyboardInterrupt:
            print("Interrupted by user")

        finally:
            self.running = False
            capture_t.join(timeout=2)
            infer_t.join(timeout=2)

            if self.cap:
                self.cap.release()
            if self.engine:
                self.engine.release()

            cv2.destroyAllWindows()
            print("Cleanup complete")


if __name__ == '__main__':
    # Verify model path exists
    if not os.path.exists(MODEL_PATH):
        print(f"Model not found: {MODEL_PATH}")
        print("Please update MODEL_PATH to point to your .rknn model")
        exit(1)

    streamer = YOLOStreamer(STREAM_URL, MODEL_PATH, conf_thresh=CONF_THRESH, nms_thresh=NMS_THRESH)
    streamer.run()
