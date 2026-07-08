"""
Test YOLO inference using centralized YoloEngine
"""

import sys
import os
import cv2
import numpy as np
import time

# Add project root to path so we can import object_detection
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from object_detection.yolo_engine import YoloEngine

# Configuration
MODEL_PATH = "object_detection/model/yolo/yolov5s-640-640.rknn"
DEFAULT_IMAGE = "data/snapshot.jpg"
OUTPUT_IMAGE = "data/result.jpg"


def draw(image, boxes, scores, classes, class_names):
    """Draw the boxes on the image"""
    print("\n{:^12} {:^12}  {}".format("class", "score", "xmin, ymin, xmax, ymax"))
    print("-" * 50)
    for box, score, cl in zip(boxes, scores, classes):
        xmin, ymin, xmax, ymax = box
        xmin = int(xmin)
        ymin = int(ymin)
        xmax = int(xmax)
        ymax = int(ymax)

        cv2.rectangle(image, (xmin, ymin), (xmax, ymax), (255, 0, 0), 2)
        cv2.putText(
            image,
            "{0} {1:.2f}".format(class_names[cl], score),
            (xmin, ymin - 6),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            (0, 0, 255),
            2,
        )

        print(
            "{:^12} {:^12.3f} [{:>4}, {:>4}, {:>4}, {:>4}]".format(
                class_names[cl], score, xmin, ymin, xmax, ymax
            )
        )


def main():
    # Get image path
    image_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IMAGE

    if not os.path.exists(image_path):
        print(f"ERROR: Image not found: {image_path}")
        sys.exit(1)

    print(f"Loading image: {image_path}")
    img = cv2.imread(image_path)
    if img is None:
        print(f"ERROR: Failed to load image")
        sys.exit(1)

    orig_shape = img.shape[:2]
    print(f"Image shape: {orig_shape}")

    # Initialize Engine
    print(f"Initializing YoloEngine with model: {MODEL_PATH}")
    engine = YoloEngine(MODEL_PATH)

    # Inference
    print("Running inference...")
    start = time.time()
    boxes, classes, scores = engine.predict(img)
    inference_time = (time.time() - start) * 1000
    print(f"Total processing time: {inference_time:.1f}ms")

    if boxes is None:
        print("No detections found!")
    else:
        print(f"Detections found: {len(boxes)}")

        # Draw and save
        draw(img, boxes, scores, classes, engine.CLASSES)
        cv2.imwrite(OUTPUT_IMAGE, img)
        print(f"\nSaved result to: {OUTPUT_IMAGE}")

    engine.release()


if __name__ == "__main__":
    main()
