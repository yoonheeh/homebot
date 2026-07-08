"""
Setup evaluation dataset - Download COCO val2017 subset
"""

import os
import sys
import json
import requests
import zipfile
from tqdm import tqdm

# Configuration
DATA_DIR = "data/evaluation"
IMAGES_DIR = os.path.join(DATA_DIR, "images")
ANNOTATIONS_DIR = os.path.join(DATA_DIR, "ground_truth")

# 10 images from COCO val2017 for a quick test
# In a real scenario, you'd use more, but let's keep it lean for the demo
COCO_VAL_URL = "http://images.cocodataset.org/zips/val2017.zip"
COCO_ANN_URL = "http://images.cocodataset.org/annotations/annotations_trainval2017.zip"

# COCO 80 classes to match our model
COCO_CLASSES = [
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    27,
    28,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    67,
    70,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
]

# Map COCO category IDs (1-90) to our model's class indices (0-79)
COCO_ID_MAP = {coco_id: idx for idx, coco_id in enumerate(COCO_CLASSES)}


def download_file(url, dest_path):
    print(f"Downloading {url}...")
    response = requests.get(url, stream=True)
    total_size = int(response.headers.get("content-length", 0))

    with (
        open(dest_path, "wb") as f,
        tqdm(total=total_size, unit="iB", unit_scale=True) as pbar,
    ):
        for data in response.iter_content(1024):
            f.write(data)
            pbar.update(len(data))


def setup():
    os.makedirs(IMAGES_DIR, exist_ok=True)
    os.makedirs(ANNOTATIONS_DIR, exist_ok=True)

    ann_zip = os.path.join(DATA_DIR, "annotations.zip")
    if not os.path.exists(ann_zip):
        download_file(COCO_ANN_URL, ann_zip)

    print("Extracting annotations...")
    with zipfile.ZipFile(ann_zip, "r") as zip_ref:
        # We only need instances_val2017.json
        zip_ref.extract("annotations/instances_val2017.json", DATA_DIR)

    ann_path = os.path.join(DATA_DIR, "annotations/instances_val2017.json")
    with open(ann_path, "r") as f:
        coco_data = json.load(f)

    # Select first 50 images from val2017
    target_images = coco_data["images"][:50]
    target_image_ids = {img["id"]: img for img in target_images}

    print(f"Preparing simplified ground truth for {len(target_images)} images...")

    # Group annotations by image_id
    simplified_gt = {}
    for ann in coco_data["annotations"]:
        img_id = ann["image_id"]
        if img_id in target_image_ids:
            if img_id not in simplified_gt:
                simplified_gt[img_id] = {
                    "filename": target_image_ids[img_id]["file_name"],
                    "width": target_image_ids[img_id]["width"],
                    "height": target_image_ids[img_id]["height"],
                    "objects": [],
                }

            # Map COCO category_id to our 0-79 index
            cat_id = ann["category_id"]
            if cat_id in COCO_ID_MAP:
                class_idx = COCO_ID_MAP[cat_id]
                # COCO bbox is [x, y, width, height]
                simplified_gt[img_id]["objects"].append(
                    {"bbox": ann["bbox"], "class": class_idx}
                )

    # Save simplified JSON
    gt_file = os.path.join(ANNOTATIONS_DIR, "val2017_mini.json")
    with open(gt_file, "w") as f:
        json.dump(simplified_gt, f, indent=2)

    print(f"Ground truth saved to {gt_file}")

    # Download images
    print("Downloading images (subset)...")
    # Note: Downloading individual images is slower but saves bandwidth
    # If this fails, consider downloading the whole val2017.zip (1GB+)
    for img_id, info in tqdm(simplified_gt.items()):
        img_filename = info["filename"]
        img_url = f"http://images.cocodataset.org/val2017/{img_filename}"
        img_dest = os.path.join(IMAGES_DIR, img_filename)

        if not os.path.exists(img_dest):
            resp = requests.get(img_url)
            with open(img_dest, "wb") as f:
                f.write(resp.content)

    print("\nSetup complete!")
    print(f"Images: {IMAGES_DIR}")
    print(f"Ground Truth: {gt_file}")


if __name__ == "__main__":
    setup()
