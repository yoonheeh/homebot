import json
import zmq
import time
import sys
import numpy as np

MAX_JITTER_MS = 15.0  # Max acceptable standard deviation in frame delivery


def convert_to_np(val):
    npdict = {
        "uint8": np.uint8,
        "uint16": np.uint16,
    }
    converted = ""
    try:
        converted = npdict[val]
    except KeyError:
        print(f"{val} not valid data type")
    return converted


def connect_socket(endpoint):
    context = zmq.Context.instance()
    sock = context.socket(zmq.SUB)
    sock.setsockopt(zmq.LINGER, 0)
    sock.setsockopt(zmq.CONFLATE, 1)
    sock.setsockopt_string(zmq.SUBSCRIBE, "")
    sock.RCVTIMEO = 2000
    sock.connect(endpoint)
    return sock


def run_diagnostics(socket_name, stream_name, config):
    print(f"\n[Auto-Test] Analyzing {stream_name} stream...")
    sock = connect_socket(socket_name)

    try:
        sock.recv()
    except:
        pass

    arrival_times = []
    frames_received = 0
    all_zeros_detected = False
    payload_mismatch = False
    actual_bytes = 0

    expected_bytes = config["width"] * config["height"] * config["channels"]
    sample_frames = 3 * config["fps"]
    dtype = convert_to_np(config["dtype"])

    while frames_received < sample_frames:
        try:
            data = sock.recv()
            arrival_times.append(time.time())

            if len(data) != expected_bytes:
                payload_mismatch = True
                actual_bytes = len(data)
                break

            if frames_received == 0:
                arr = np.frombuffer(data, dtype=dtype)
                if not np.any(arr):
                    all_zeros_detected = True
                    break

            frames_received += 1

        except zmq.error.Again:
            print(f"❌ FAIL: {stream_name} hardware stall. ZMQ timed out.")
            return False

    if payload_mismatch:
        print(
            f"❌ FAIL: Payload mismatch! Expected {expected_bytes}, got {actual_bytes}"
        )
        return False

    if all_zeros_detected:
        print(f"❌ FAIL: Sensor buffer is dead (all zeros). Camera requires reboot.")
        return False

    # 3. Jitter / Frame Pacing Check
    # Calculate time between consecutive frames in milliseconds
    intervals = np.diff(arrival_times) * 1000
    avg_interval = np.mean(intervals)
    jitter = np.std(intervals)
    actual_fps = 1000.0 / avg_interval

    target_fps = config["fps"]

    print(f"  -> Payload:  {expected_bytes} bytes [PASS]")
    print(f"  -> Data:     Non-zero buffer [PASS]")
    print(f"  -> FPS:      {actual_fps:.1f} (Target: {target_fps})")
    print(f"  -> Pacing:   {avg_interval:.1f}ms avg interval, {jitter:.1f}ms jitter")

    if abs(actual_fps - target_fps) > 2.0:
        print(f"❌ FAIL: {stream_name} FPS unstable.")
        return False

    if jitter > MAX_JITTER_MS:
        print(
            f"❌ FAIL: {stream_name} frame pacing is too erratic (Jitter: {jitter:.1f}ms)."
        )
        return False

    print(f"✅ PASS: {stream_name} stream is healthy.")
    return True


if __name__ == "__main__":
    print("Starting Advanced Vision Diagnostics...")
    print("Note: Make sure to start streaming")

    # Load config
    with open("vision_config.json", "r") as f:
        config = json.load(f)

    # Sockets
    rgb_sock = config["pipeline"]["rgb_ipc"]
    depth_sock = config["pipeline"]["depth_ipc"]

    success = True

    rgb_config = config["rgb"]
    depth_config = config["depth"]

    if not run_diagnostics(rgb_sock, "RGB", rgb_config):
        success = False

    if not run_diagnostics(depth_sock, "DEPTH", depth_config):
        success = False

    if success:
        print("\n✅ All streams passed. Vision subsystem is ML-ready.")
        sys.exit(0)
    else:
        print("\n❌ Vision subsystem failed diagnostics.")
        sys.exit(1)
