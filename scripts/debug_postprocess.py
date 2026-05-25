"""
Debug YOLO postprocessing - compare different approaches
"""

import sys
import os
import cv2
import numpy as np
from rknnlite.api import RKNNLite

MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'
INPUT_SIZE = (640, 640)
CONF_THRESH = 0.25

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
    """Resize and normalize image for YOLO input"""
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, input_size)
    img = img.astype(np.float32) / 255.0
    img = np.expand_dims(img, axis=0)
    return img


def sigmoid(x):
    return 1 / (1 + np.exp(-x))


# Rockchip's exact approach from test.py
def process_rockchip(input, mask, anchors):
    """Exact copy from Rockchip test.py"""
    anchors = [anchors[i] for i in mask]
    grid_h, grid_w = map(int, input.shape[0:2])

    box_confidence = input[..., 4]
    box_confidence = np.expand_dims(box_confidence, axis=-1)

    box_class_probs = input[..., 5:]

    box_xy = input[..., :2]*2 - 0.5

    col = np.tile(np.arange(0, grid_w), grid_w).reshape(-1, grid_w)
    row = np.tile(np.arange(0, grid_h).reshape(-1, 1), grid_h)
    col = col.reshape(grid_h, grid_w, 1, 1).repeat(3, axis=-2)
    row = row.reshape(grid_h, grid_w, 1, 1).repeat(3, axis=-2)
    grid = np.concatenate((col, row), axis=-1)
    box_xy += grid
    box_xy *= int(640/grid_h)

    box_wh = pow(input[..., 2:4]*2, 2)
    box_wh = box_wh * anchors

    box = np.concatenate((box_xy, box_wh), axis=-1)

    return box, box_confidence, box_class_probs


def debug_output(output, name):
    """Print debug info about an output tensor"""
    print(f"\n{name}:")
    print(f"  Shape: {output.shape}")
    print(f"  Min: {output.min():.4f}, Max: {output.max():.4f}")
    print(f"  Mean: {output.mean():.4f}")

    # Check if values look like they need sigmoid
    # (raw YOLO outputs are typically -5 to +5, sigmoid outputs are 0-1)
    if output.min() < 0 or output.max() > 1:
        print(f"  -> Values outside [0,1], may need sigmoid")
    else:
        print(f"  -> Values in [0,1], likely already sigmoid-activated")

    # Sample values - handle different dimensions
    if len(output.shape) == 3:
        print(f"  Sample [0,:5,:5]: {output[0,:5,:5]}")
    elif len(output.shape) == 4:
        print(f"  Sample [0,0,0,:]: {output[0,0,0,:]}")


def main():
    image_path = sys.argv[1] if len(sys.argv) > 1 else 'data/snapshot.jpg'

    if not os.path.exists(image_path):
        print(f"ERROR: Image not found: {image_path}")
        sys.exit(1)

    # Load image
    img = cv2.imread(image_path)
    input_img = preprocess(img, INPUT_SIZE)

    # Load model
    rknn = RKNNLite()
    rknn.load_rknn(MODEL_PATH)
    rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_AUTO)

    # Run inference
    print("Running inference...")
    outputs = rknn.inference(inputs=[input_img])

    print(f"\nNumber of outputs: {len(outputs)}")

    for i, out in enumerate(outputs):
        debug_output(out[0], f"Output[{i}]")  # Remove batch dim
        
        # Check max confidence
        # Reshape to [3, 85, H, W]
        h, w = out.shape[-2:]
        reshaped = out.reshape(3, 85, h, w)
        conf_channel = reshaped[:, 4, :, :]
        print(f"  Max confidence in Output[{i}]: {conf_channel.max():.4f}")

    # Now let's try reshaping like Rockchip does
    print("\n" + "="*50)
    print("Rockchip reshape/transpose test:")
    print("="*50)

    for i, out in enumerate(outputs):
        print(f"\nOutput[{i}]:")
        print(f"  Original: {out.shape}")

        # Rockchip reshape: [3, -1] + shape[-2:]
        reshaped = out.reshape([3, -1] + list(out.shape[-2:]))
        print(f"  After reshape [3,-1,H,W]: {reshaped.shape}")

        # Rockchip transpose: (2, 3, 0, 1)
        transposed = np.transpose(reshaped, (2, 3, 0, 1))
        print(f"  After transpose (2,3,0,1): {transposed.shape}")

        debug_output(transposed, f"  Final")

    rknn.release()


if __name__ == '__main__':
    main()
