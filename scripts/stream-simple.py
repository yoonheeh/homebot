# /// script
# dependencies = [
#   "opencv-python",
# ]
# ///

"""Simple ESP32-CAM streaming viewer - no ML, just display"""

import cv2
import time

STREAM_URL = "http://192.168.4.1:81/stream"


def main():
    print(f"Connecting to ESP32-CAM at {STREAM_URL}...")

    cap = cv2.VideoCapture(STREAM_URL)
    if not cap.isOpened():
        print("Failed to open stream!")
        print("Make sure:")
        print("  1. Firefly is connected to ESP32's WiFi (ELEGOO-XXXXXXXX)")
        print("  2. ESP32-CAM is powered on")
        return

    # Minimize buffering for lower latency
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    print("Connected! Streaming...")
    print("  'q' - quit")
    print("  's' - save snapshot")

    frame_count = 0
    start_time = time.time()
    fps = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Frame read failed, retrying...")
            time.sleep(0.1)
            continue

        # Calculate FPS
        frame_count += 1
        elapsed = time.time() - start_time
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            start_time = time.time()

        # Draw FPS on frame
        cv2.putText(
            frame,
            f"FPS: {fps:.1f}",
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            1,
            (0, 255, 0),
            2,
        )

        cv2.imshow("ESP32-CAM Stream", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        elif key == ord("s"):
            timestamp = time.strftime("%Y%m%d_%H%M%S")
            filename = f"snapshot_{timestamp}.jpg"
            cv2.imwrite(filename, frame)
            print(f"Saved: {filename}")

    cap.release()
    cv2.destroyAllWindows()
    print("Stream closed")


if __name__ == "__main__":
    main()
