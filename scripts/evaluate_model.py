"""
Evaluate YOLO model on COCO mini dataset - Calculate mAP
"""

import os
import sys
import json
import cv2
import numpy as np
import time
from tqdm import tqdm

# Add project root to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from object_detection.yolo_engine import YoloEngine

# Configuration
MODEL_PATH = 'object_detection/model/yolo/yolov5s-640-640.rknn'
DATA_DIR = 'data/evaluation'
IMAGES_DIR = os.path.join(DATA_DIR, 'images')
GT_FILE = os.path.join(DATA_DIR, 'ground_truth', 'val2017_mini.json')
IOU_THRESHOLD = 0.5

def calculate_iou(box1, box2):
    """
    Calculate Intersection over Union (IoU) between two boxes
    box1: [x1, y1, x2, y2]
    box2: [x1, y1, x2, y2]
    """
    # Intersection coordinates
    x_left = max(box1[0], box2[0])
    y_top = max(box1[1], box2[1])
    x_right = min(box1[2], box2[2])
    y_bottom = min(box1[3], box2[3])

    if x_right < x_left or y_bottom < y_top:
        return 0.0

    intersection_area = (x_right - x_left) * (y_bottom - y_top)
    
    # Union area
    box1_area = (box1[2] - box1[0]) * (box1[3] - box1[1])
    box2_area = (box2[2] - box2[0]) * (box2[3] - box2[1])
    
    iou = intersection_area / float(box1_area + box2_area - intersection_area)
    return iou

def evaluate():
    if not os.path.exists(GT_FILE):
        print(f"ERROR: Ground truth file not found: {GT_FILE}")
        print("Run scripts/setup_eval_dataset.py first.")
        return

    with open(GT_FILE, 'r') as f:
        ground_truth = json.load(f)

    print(f"Initializing YoloEngine with model: {MODEL_PATH}")
    engine = YoloEngine(MODEL_PATH, conf_thresh=0.001)

    all_results = []
    total_gt_objects = 0
    
    print(f"Evaluating on {len(ground_truth)} images...")
    
    for img_id, info in tqdm(ground_truth.items()):
        img_path = os.path.join(IMAGES_DIR, info['filename'])
        img = cv2.imread(img_path)
        if img is None:
            continue

        orig_h, orig_w = img.shape[:2]
        
        # Inference
        pred_boxes, pred_classes, pred_scores = engine.predict(img)
        
        # Prepare GT boxes (convert [x, y, w, h] to [x1, y1, x2, y2])
        gt_objs = []
        for obj in info['objects']:
            x, y, w, h = obj['bbox']
            gt_objs.append({
                'bbox': [x, y, x + w, y + h],
                'class': obj['class'],
                'matched': False
            })
            total_gt_objects += 1

        if pred_boxes is None:
            continue

        # Match predictions to ground truth
        # Sort predictions by score descending
        indices = np.argsort(pred_scores)[::-1]
        for idx in indices:
            p_box = pred_boxes[idx]
            p_class = pred_classes[idx]
            p_score = pred_scores[idx]
            
            best_iou = 0
            best_gt_idx = -1
            
            for g_idx, g_obj in enumerate(gt_objs):
                if g_obj['class'] == p_class and not g_obj['matched']:
                    iou = calculate_iou(p_box, g_obj['bbox'])
                    if iou > best_iou:
                        best_iou = iou
                        best_gt_idx = g_idx
            
            if best_iou >= IOU_THRESHOLD:
                gt_objs[best_gt_idx]['matched'] = True
                all_results.append({'score': p_score, 'tp': 1, 'fp': 0})
            else:
                all_results.append({'score': p_score, 'tp': 0, 'fp': 1})

    # Calculate Precision and Recall
    # Sort results by score
    all_results.sort(key=lambda x: x['score'], reverse=True)
    
    tp_sum = 0
    fp_sum = 0
    precisions = []
    recalls = []
    
    for res in all_results:
        tp_sum += res['tp']
        fp_sum += res['fp']
        precisions.append(tp_sum / (tp_sum + fp_sum))
        recalls.append(tp_sum / total_gt_objects)

    # Simple AP calculation (Area under PR curve)
    # We use the 11-point interpolation or VOC-style integration
    ap = 0
    for t in np.arange(0, 1.1, 0.1):
        p_at_t = [p for p, r in zip(precisions, recalls) if r >= t]
        if p_at_t:
            ap += max(p_at_t)
    ap /= 11.0

    print("\n" + "="*40)
    print("Evaluation Results (mAP@0.5)")
    print("="*40)
    print(f"Total Images:      {len(ground_truth)}")
    print(f"Total GT Objects:  {total_gt_objects}")
    print(f"Total Predictions: {len(all_results)}")
    print(f"True Positives:    {tp_sum}")
    print(f"False Positives:   {fp_sum}")
    print("-" * 40)
    print(f"mAP@0.5:           {ap:.4f} ({ap*100:.2f}%)")
    print(f"Precision:         {tp_sum / (tp_sum + fp_sum):.4f}")
    print(f"Recall:            {tp_sum / total_gt_objects:.4f}")
    print("="*40)

    engine.release()

if __name__ == '__main__':
    evaluate()
