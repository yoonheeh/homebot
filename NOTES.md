# Homebot Project Notes

## Model Files

### YOLOv5s (RK3588 NPU)
- **File**: `object_detection/model/yolo/yolov5s-640-640.rknn` (download with script below)
- **Download Script**: `object_detection/model/yolo/scripts/download_yolo_rknn.sh`
- **Source**: https://github.com/rockchip-linux/rknpu2/tree/main/examples/rknn_yolov5_demo/model/RK3588
- **Description**: Pre-converted YOLOv5s model for Rockchip RK3588 NPU
- **Format**: RKNN (Rockchip Neural Network)
- **Target**: RK3588 (3x NPU cores, 6 TOPS)

**To download:**
```bash
./object_detection/model/yolo/scripts/download_yolo_rknn.sh
```

### YOLOv5s ReLU (RK3588 NPU)
- **File**: `object_detection/model/yolo/yolov5s_relu.rknn` (download with script below)
- **Download Script**: `object_detection/model/yolo/scripts/download_yolov5_relu.sh`
- **Source**: https://github.com/rockchip-linux/rknpu2/tree/main/examples/rknn_yolov5_demo/model/RK3588
- **Description**: YOLOv5s with ReLU activation (alternative to SiLU), sometimes faster on NPU
- **Format**: RKNN (Rockchip Neural Network)
- **Target**: RK3588 (3x NPU cores, 6 TOPS)

**To download:**
```bash
./object_detection/model/yolo/scripts/download_yolov5_relu.sh
```

## Hardware Setup

### Firefly ITX-3588J
- Ubuntu 22.04
- Kernel 6.1
- RK3588 SoC with NPU

### Elegoo Smart Robot Car V4
- Arduino Uno (motor/sensor control)
- ESP32-CAM (WiFi + camera stream)
- Communication: USB Serial + WiFi

## ESP32-CAM Configuration
- **Mode**: WiFi Access Point (AP)
- **SSID**: `ELEGOO-XXXXXXXX` (MAC-based)
- **Password**: `12341234` (customized)
- **IP**: `192.168.4.1`
- **Stream URL**: `http://192.168.4.1:81/stream`
- **Snapshot URL**: `http://192.168.4.1/capture`

## Scripts

### Stream Viewer
- `scripts/stream-simple.py` - Basic ESP32-CAM stream viewer
- `scripts/stream-yolo.py` - YOLO inference on ESP32-CAM stream

## Dependencies

### RKNN
- `rknn_toolkit_lite2-1.6.0-cp310-cp310-linux_aarch64.whl` - Inference runtime for RK3588
- Source: https://github.com/rockchip-linux/rknn-toolkit2


## Development Workflow

To develop and test on the Rockchip (Firefly) board:

1. **Make changes** on your local machine.
2. **Commit and push** the changes to GitHub:
   ```bash
   git add .
   git commit -m "Your message"
   git push
   ```
3. **Pull from the board**:
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && git pull"
   ```
4. **Run the script** on the board:
   ```bash
   ssh firefly@88.88.88.165 "cd homebot && /home/firefly/.local/bin/uv run scripts/test_inference.py"
   ```
