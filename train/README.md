# Training

## Data Collection
According to [lerobot training guide](https://huggingface.co/docs/lerobot/il_robots), it is recommended to collect at least 50 epsidoes of the same action. 
Data collection requires running two background processes:
Note: make sure you are on the same local network
1. Camera streaming
```
# deploy with bazel
bazel run --config=arm64 //vision/stream:deploy_stream

# run the binary
./$BIN_DIR/stream
```

2. Robot controlling
```
# deploy with bazel
bazel run --config=arm64 //pico_interface:deploy_control

# run the binary
./$BIN_DIR/control
```
3. (optional) Visualize camera stream
```
# deploy streaming server with bazel
bazel run --config=arm64 //vision/stream:deploy_flask_server

# run flask server
uv run $BIN_DIR/server.py

# visualize from browser
$BOARD_IP:5000
```

Each episode will be created separately, and will create a new dir as below:
```
├── episode_20260712_201928
│   ├── frame_00000.jpg
    ... more images ...
    ├── frame_01106.jpg
    └── metadata.csv
```

## Data Converter
### Prerequisite
1. Move the data from the edge device to your host machine
2. Note that you need python 3.12 or above to run make v3.0 LeRobot dataset
3. Need to setup HuggingFace account and produce a write-access token to upload dataset to HF.

### Converting to LeRobotDataset
To leverage LeRobot ecosystem, we can convert the collected data into `LeRobotDataset`. You can simply run the below script to run v3.0 dataset. 
```
uv run train/data_converter/lerobot_dataset_converter.py --raw_dir path/to/your/data
```

### Uploading to HuggingFace
You can upload the data to HuggingFace hub, so that you can use free notebooks like Colab and Kaggle to retrieve data easily from.
```
uv run train/data_converter/upload_to_huggingface.py --dataset_dir local/my_navigation_bot --huggingface_hub $YOUR_HF_USERNAME/$REPO
```

### Checking Integrity of the Training Pipeline
After recording some episodes (5 in my case), make sure to run the training pipeline, observe that the loss is decreasing in a meaningful way (note that with such small sample size, it will overfit and memorize the routes). 
TODO: add notebook


