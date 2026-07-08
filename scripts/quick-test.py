import cv2
import numpy as np
from rknnlite.api import RKNNLite

MODEL_PATH = "object_detection/model/yolo/yolov5s-640-640.rknn"

rknn = RKNNLite()
print(f"Loading model: {MODEL_PATH}")
ret = rknn.load_rknn(MODEL_PATH)
if ret != 0:
    print(f"Failed to load model: {ret}")
    exit(1)

ret = rknn.init_runtime()
if ret != 0:
    print(f"Failed to init runtime: {ret}")
    exit(1)

img_path = "data/snapshot.jpg"
print(f"Loading image: {img_path}")
img = cv2.imread(img_path)
if img is None:
    print(f"Failed to load image: {img_path}")
    exit(1)

img = cv2.resize(img, (640, 640))
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
img = np.expand_dims(img, axis=0)  # Add batch dimension

print("Running inference...")
outputs = rknn.inference(inputs=[img])
print("Inference successful! Output shapes:", [o.shape for o in outputs])
rknn.release()
