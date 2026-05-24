import cv2
import numpy as np
from rknnlite.api import RKNNLite

rknn = RKNNLite()
rknn.load_rknn('object_detection/model/yolo/yolov5s.rknn')
rknn.init_runtime()

img = cv2.imread('data/snapshot.jpg')
img = cv2.resize(img, (640, 640))
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
outputs = rknn.inference(inputs=[img])
print('Inference successful! Output shapes:', [o.shape for o in outputs])
rknn.release()

