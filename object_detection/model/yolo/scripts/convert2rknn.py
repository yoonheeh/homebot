from ultralytics import YOLO

# Load the YOLO26 model
model = YOLO("yolo26n.pt")

# Export the model to RKNN format
model.export(format="rknn", name="rk3588")  # creates '/yolo26n_rknn_model'
