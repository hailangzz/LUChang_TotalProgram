import numpy as np
import argparse
import cv2
print(cv2.__version__)
# 组建参数parse
# parser = argparse.ArgumentParser(
#     description='Script to run MobileNet-SSD object detection network ')
# parser.add_argument("--video", help="path to video file. If empty, camera's stream will be used")
# parser.add_argument("--model", type=str, default="MobileNetSSD_deploy",
#                                help="path to trained model")
# parser.add_argument("--thr", default=0.2, type=float, help="confidence threshold to filter out weak detections")
# args = parser.parse_args()
#
# # 类别标签变量.
# classNames = { 0: 'background',
#     1: 'aeroplane', 2: 'bicycle', 3: 'bird', 4: 'boat',
#     5: 'bottle', 6: 'bus', 7: 'car', 8: 'cat', 9: 'chair',
#     10: 'cow', 11: 'diningtable', 12: 'dog', 13: 'horse',
#     14: 'motorbike', 15: 'person', 16: 'pottedplant',
#     17: 'sheep', 18: 'sofa', 19: 'train', 20: 'tvmonitor' }
#
# input_size = (300, 300)
#
# # 加载模型
# net = cv2.dnn.Net_readFromModelOptimizer(args.model+".xml", args.model+".bin")
# # 设置推理引擎后端
# net.setPreferableBackend(cv2.dnn.DNN_BACKEND_INFERENCE_ENGINE)