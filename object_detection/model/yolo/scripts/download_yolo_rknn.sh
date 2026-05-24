#!/bin/bash

# Download YOLOv5s RKNN model for RK3588 from Rockchip official repository
# Source: https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_yolov5_demo/model/RK3588

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODEL_DIR="$(dirname "$SCRIPT_DIR")"
MODEL_NAME="yolov5s-640-640.rknn"
MODEL_URL="https://github.com/rockchip-linux/rknpu2/raw/master/examples/rknn_yolov5_demo/model/RK3588/yolov5s-640-640.rknn"

echo "Downloading YOLOv5s RKNN model for RK3588..."
echo "Source: $MODEL_URL"
echo "Destination: $MODEL_DIR/$MODEL_NAME"
echo ""

if [ -f "$MODEL_DIR/$MODEL_NAME" ]; then
    echo "Model already exists: $MODEL_DIR/$MODEL_NAME"
    read -p "Overwrite? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Download cancelled."
        exit 0
    fi
fi

cd "$MODEL_DIR"

if command -v wget &> /dev/null; then
    wget "$MODEL_URL" -O "$MODEL_NAME"
elif command -v curl &> /dev/null; then
    curl -L "$MODEL_URL" -o "$MODEL_NAME"
else
    echo "Error: Neither wget nor curl is installed."
    exit 1
fi

echo ""
echo "Download complete!"
echo "Model saved to: $MODEL_DIR/$MODEL_NAME"
echo ""
echo "Verify with: ls -lh $MODEL_DIR/$MODEL_NAME"
