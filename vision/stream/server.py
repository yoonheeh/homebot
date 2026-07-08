import zmq
from flask import Flask, Response

app = Flask(__name__)

def generate_frames():
    # 1. Initialize ZeroMQ Context and Subscriber Socket
    context = zmq.Context()
    subscriber = context.socket(zmq.SUB)
    
    # Connect to the local C++ publisher (use localhost since both are on Firefly)
    subscriber.connect("tcp://127.0.0.1:5556")
    
    # Subscribe to all messages (empty string means no topic filtering)
    subscriber.setsockopt_string(zmq.SUBSCRIBE, "")
    
    # Set a conflate option so we only process the latest frame if Python lags
    subscriber.setsockopt(zmq.CONFLATE, 1)

    print("ZeroMQ Subscriber connected to C++ publisher.")

    try:
        while True:
            # Receive raw JPEG bytes from the C++ process
            frame_bytes = subscriber.recv()

            # Yield the frame using standard MJPEG boundary format
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
                   
    except Exception as e:
        print(f"Streaming error: {e}")
    finally:
        subscriber.close()
        context.term()

@app.route('/')
def video_feed():
    # Return the dynamic boundary stream response
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    # Run server on port 5000, visible to your entire local network
    app.run(host='0.0.0.0', port=5000, threaded=True)
