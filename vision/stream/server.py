import zmq
import cv2
import numpy as np
from flask import Flask, Response

app = Flask(__name__)

FRAME_WIDTH = 640
FRAME_HEIGHT = 400


def stream_rgb():
    """Generator for the raw BGR stream"""
    context = zmq.Context()
    subscriber = context.socket(zmq.SUB)
    subscriber.setsockopt(zmq.LINGER, 0)
    subscriber.connect("ipc:///tmp/oakd_rgb_stream.ipc")
    subscriber.setsockopt_string(zmq.SUBSCRIBE, "")
    subscriber.setsockopt(zmq.CONFLATE, 1)

    width = 640
    height = 400

    try:
        while True:
            frame_bytes = subscriber.recv()
            if len(frame_bytes) != int(width * height * 1.5):
                print(f"Skipping bad frame: {len(frame_bytes)} bytes")
                continue

            flat_array = np.frombuffer(frame_bytes, dtype=np.uint8)

            # To decode NV12, OpenCV requires the matrix height to be exactly H * 1.5
            nv12_matrix = flat_array.reshape((int(height * 1.5), width))

            # Hardware accelerated YUV to BGR conversion
            bgr_frame = cv2.cvtColor(nv12_matrix, cv2.COLOR_YUV2BGR_NV12)

            success, encoded_image = cv2.imencode(
                ".jpg", bgr_frame, [cv2.IMWRITE_JPEG_QUALITY, 70]
            )
            if not success:
                continue

            yield (
                b"--frame\r\n"
                b"Content-Type: image/jpeg\r\n\r\n" + encoded_image.tobytes() + b"\r\n"
            )
    finally:
        subscriber.close()
        context.term()


def stream_depth():
    """Generator for the raw 16-bit Depth stream"""
    context = zmq.Context()
    subscriber = context.socket(zmq.SUB)
    subscriber.setsockopt(zmq.LINGER, 0)
    subscriber.connect("ipc:///tmp/oakd_depth_stream.ipc")
    subscriber.setsockopt_string(zmq.SUBSCRIBE, "")
    subscriber.setsockopt(zmq.CONFLATE, 1)

    width = 640
    height = 480

    try:
        while True:
            frame_bytes = subscriber.recv()

            # Failsafe: only process if the byte count perfectly matches 16-bit 640x480
            expected_bytes = width * height * 2
            if len(frame_bytes) != expected_bytes:
                print(
                    f"[Depth] Skipping bad frame: {len(frame_bytes)} bytes (expected {expected_bytes})"
                )
                continue

            # Load as 16-bit integers
            flat_array = np.frombuffer(frame_bytes, dtype=np.uint16)

            # Reshape using the math we deduced
            depth_frame = flat_array.reshape((height, width))

            # Normalize for human eyes (16-bit -> 8-bit) and apply thermal colors
            normalized = cv2.normalize(
                depth_frame, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U
            )
            colorized = cv2.applyColorMap(normalized, cv2.COLORMAP_JET)

            # Set missing depth values (0) to black
            colorized[depth_frame == 0] = [0, 0, 0]

            success, encoded_image = cv2.imencode(
                ".jpg", colorized, [cv2.IMWRITE_JPEG_QUALITY, 70]
            )
            if not success:
                continue

            yield (
                b"--frame\r\n"
                b"Content-Type: image/jpeg\r\n\r\n" + encoded_image.tobytes() + b"\r\n"
            )
    finally:
        subscriber.close()
        context.term()


# --- Flask Routes ---


@app.route("/rgb")
def rgb_feed():
    return Response(stream_rgb(), mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/depth")
def depth_feed():
    return Response(
        stream_depth(), mimetype="multipart/x-mixed-replace; boundary=frame"
    )


@app.route("/")
def dashboard():
    """A simple HTML dashboard to view both streams side-by-side."""
    return """
    <html>
        <head>
            <title>Homebot Vision</title>
            <style>
                body { background-color: #121212; color: white; text-align: center; font-family: sans-serif; }
                .container { display: flex; justify-content: center; gap: 20px; margin-top: 20px; }
                img { border: 2px solid #555; border-radius: 8px; }
            </style>
        </head>
        <body>
            <h2>Homebot Vision Dashboard</h2>
            <div class="container">
                <div>
                    <h3>RGB Camera</h3>
                    <img src="/rgb" width="640" height="400">
                </div>
                <div>
                    <h3>Depth Map</h3>
                    <img src="/depth" width="640" height="400">
                </div>
            </div>
        </body>
    </html>
    """


if __name__ == "__main__":
    print("Starting Vision Dashboard at http://0.0.0.0:5000/")
    app.run(host="0.0.0.0", port=5000, threaded=True)
