"""
Check RKNN model input/output shapes

Usage:
  uv run scripts/check_model.py path/to/model.rknn
  uv run scripts/check_model.py  # uses default MODEL_PATH
"""

import sys
import numpy as np
from rknnlite.api import RKNNLite

# Default model path (relative to homebot root)
DEFAULT_MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'


def check_model(model_path):
    """Load model and print input/output shapes"""
    print(f"Loading model: {model_path}")
    print("-" * 50)

    rknn = RKNNLite()

    # Load model
    ret = rknn.load_rknn(model_path)
    if ret != 0:
        print(f"ERROR: Failed to load model (ret={ret})")
        return 1

    # Initialize runtime
    ret = rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_AUTO)
    if ret != 0:
        print(f"ERROR: Failed to init runtime (ret={ret})")
        return 1

    # RKNNLite doesn't have get_input_shape/get_output_shape
    # We need to run inference to determine shapes
    print("\nRunning test inference to determine shapes...")

    # Try common input shapes for YOLO models
    test_shapes = [
        (1, 640, 640, 3),   # NHWC - most common
        (1, 3, 640, 640),   # NCHW
    ]

    outputs = None
    used_shape = None

    for shape in test_shapes:
        try:
            # RKNN models usually expect uint8
            dummy_input = np.zeros(shape, dtype=np.uint8)
            outputs = rknn.inference(inputs=[dummy_input])
            used_shape = shape
            print(f"  Input shape: {list(shape)} (NHWC format, uint8)")
            break
        except Exception as e:
            continue

    if outputs is None:
        print("ERROR: Could not determine input shape. Model may require different input dimensions.")
        return 1

    print(f"\nOutput Shapes:")
    total_params = 0
    for i, out in enumerate(outputs):
        shape = list(out.shape)
        size = out.size
        total_params += size
        print(f"  Output[{i}]: {shape} -> {size:,} elements")

    print(f"\nTotal output elements: {total_params:,}")

    rknn.release()
    print("\nModel check complete!")
    return 0


if __name__ == '__main__':
    import os

    # Get model path from args or use default
    if len(sys.argv) > 1:
        model_path = sys.argv[1]
    else:
        model_path = DEFAULT_MODEL_PATH

    # Verify file exists
    if not os.path.exists(model_path):
        print(f"ERROR: Model file not found: {model_path}")
        print(f"Usage: {sys.argv[0]} [path/to/model.rknn]")
        sys.exit(1)

    sys.exit(check_model(model_path))
