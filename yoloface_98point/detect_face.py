# -*- coding: UTF-8 -*-
import argparse
import time
from pathlib import Path

import cv2
import torch
import torch.backends.cudnn as cudnn
from numpy import random
import copy

from models.experimental import attempt_load
from utils.datasets import letterbox
# from utils.datasets import LoadStreams
from utils.general import check_img_size, non_max_suppression_face, apply_classifier, scale_coords, xyxy2xywh,check_imshow, \
    strip_optimizer, set_logging, increment_path
# from utils.plots import plot_one_box
from utils.torch_utils import select_device, load_classifier, time_synchronized







import numpy as np
np.set_printoptions(threshold = 1e3)#设置打印数量的阈值
torch.set_printoptions(threshold=np.inf)






def load_model(weights, device):
    model = attempt_load(weights, map_location=device)  # load FP32 model
    return model


def scale_coords_landmarks(img1_shape, coords, img0_shape, ratio_pad=None):
    # Rescale coords (xyxy) from img1_shape to img0_shape
    if ratio_pad is None:  # calculate from img0_shape
        gain = min(img1_shape[0] / img0_shape[0], img1_shape[1] / img0_shape[1])  # gain  = old / new
        pad = (img1_shape[1] - img0_shape[1] * gain) / 2, (img1_shape[0] - img0_shape[0] * gain) / 2  # wh padding
    else:
        gain = ratio_pad[0][0]
        pad = ratio_pad[1]

    coords[:, [0, 2, 4, 6]] -= pad[0]  # x padding
    coords[:, [1, 3, 5, 7]] -= pad[1]  # y padding
    coords[:, :196] /= gain
    #clip_coords(coords, img0_shape)
    # coords[:, 0].clamp_(0, img0_shape[1])  # x1
    # coords[:, 1].clamp_(0, img0_shape[0])  # y1
    # coords[:, 2].clamp_(0, img0_shape[1])  # x2
    # coords[:, 3].clamp_(0, img0_shape[0])  # y2
    # coords[:, 4].clamp_(0, img0_shape[1])  # x3
    # coords[:, 5].clamp_(0, img0_shape[0])  # y3
    # coords[:, 6].clamp_(0, img0_shape[1])  # x4
    # coords[:, 7].clamp_(0, img0_shape[0])  # y4
    # # coords[:, 8].clamp_(0, img0_shape[1])  # x5
    # # coords[:, 9].clamp_(0, img0_shape[0])  # y5

    point_number = 98
    for index_i in range(point_number*2):
        if index_i%2==0:
            coords[:, index_i].clamp_(0, img0_shape[1])  # x1
        else:
            coords[:, index_i].clamp_(0, img0_shape[0])  # y1

    return coords

def show_results(img, xywh, conf, landmarks, class_num):
    h,w,c = img.shape
    tl = 1 or round(0.002 * (h + w) / 2) + 1  # line/font thickness
    x1 = int(xywh[0] * w - 0.5 * xywh[2] * w)
    y1 = int(xywh[1] * h - 0.5 * xywh[3] * h)
    x2 = int(xywh[0] * w + 0.5 * xywh[2] * w)
    y2 = int(xywh[1] * h + 0.5 * xywh[3] * h)
    cv2.rectangle(img, (x1,y1), (x2, y2), (0,255,0), thickness=tl+1, lineType=cv2.LINE_AA)

    clors = [(255,0,0),(0,255,0),(0,0,255),(255,255,0)]

    for i in range(4):
        point_x = int(landmarks[2 * i] * w)
        point_y = int(landmarks[2 * i + 1] * h)
        cv2.circle(img, (point_x, point_y), tl+1, clors[i], -1)

    tf = max(tl - 1, 1)  # font thickness
    label = str(int(class_num)) + "  " + str(conf)[:5]
    cv2.putText(img, label, (x1, y1 - 2), 0, tl / 3, [225, 255, 255], thickness=tf, lineType=cv2.LINE_AA)
    return img



def detect_one(model, image_path, device):
    # Load model
    img_size = 640
    conf_thres = 0.3
    iou_thres = 0.5

    if image_path == str(0):
        print("hello")
        # view_img = check_imshow()
        # cudnn.benchmark = True  # set True to speed up constant image size inference
        # source = str(image_path)
        # imgsz = check_img_size(imgsize, s=stride)
        # dataset = LoadStreams(source, img_size=imgsz, stride=stride, auto=pt)
        # bs = len(dataset)  # batch_size



    else:

        orgimg = cv2.imread(image_path)  # BGR
        img0 = copy.deepcopy(orgimg)
        assert orgimg is not None, 'Image Not Found ' + image_path
        h0, w0 = orgimg.shape[:2]  # orig hw
        r = img_size / max(h0, w0)  # resize image to img_size
        if r != 1:  # always resize down, only resize up if training with augmentation
            interp = cv2.INTER_AREA if r < 1  else cv2.INTER_LINEAR
            img0 = cv2.resize(img0, (int(w0 * r), int(h0 * r)), interpolation=interp)

        imgsz = check_img_size(img_size, s=model.stride.max())  # check img_size

        img = letterbox(img0, new_shape=imgsz)[0]  #yoloface

        # Convert
        img = img[:, :, ::-1].transpose(2, 0, 1).copy()  # BGR to RGB, to 3x416x416

        # Run inference
        t0 = time.time()

        img = torch.from_numpy(img).to(device)
        img = img.float()  # uint8 to fp16/32
        img /= 255.0  # 0 - 255 to 0.0 - 1.0

        if img.ndimension() == 3:
            img = img.unsqueeze(0)

        # Inference
        t1 = time_synchronized()

        pred = model(img)[0]

        # Apply NMS
        pred = non_max_suppression_face(pred, conf_thres, iou_thres)

        print('img.shape: ', img.shape)
        print('orgimg.shape: ', orgimg.shape)

        # Process detections
        for i, det in enumerate(pred):  # detections per image
            gn = torch.tensor(orgimg.shape)[[1, 0, 1, 0]].to(device)  # normalization gain whwh
            gn_lks = torch.tensor(orgimg.shape)[[1, 0, 1, 0, 1, 0, 1, 0, ]].to(device)  # normalization gain landmarks
            if len(det):  #xyxy4,置信度1，各个关键点位置8，类别1）
                # Rescale boxes from img_size to im0 size
                det[:, :4] = scale_coords(img.shape[2:], det[:, :4], orgimg.shape).round()

                # Print results
                for c in det[:, -1].unique():
                    n = (det[:, -1] == c).sum()  # detections per class

                det[:, 5:201] = scale_coords_landmarks(img.shape[2:], det[:, 5:201], orgimg.shape).round()

                for j in range(det.size()[0]):
                    xywh = (xyxy2xywh(det[j, :4].view(1, 4)) / gn).view(-1).tolist()
                    conf = det[j, 4].cpu().numpy()
                    landmarks = (det[j, 5:201].view(1, 196) / gn_lks).view(-1).tolist()
                    class_num = det[j, 201].cpu().numpy()
                    orgimg = show_results(orgimg, xywh, conf, landmarks, class_num)

        cv2.imwrite('result.jpg', orgimg)




if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--weights', nargs='+', type=str, default='runs/train/exp30/weights/best.pt', help='model.pt path(s)')
    parser.add_argument('--image', type=str, default='data/yolov5_robot/Pic_2021_11_12_230209_31.jpeg', help='source')  # file/folder, 0 for webcam
    parser.add_argument('--img-size', type=int, default=640, help='inference size (pixels)')
    # parser.add_argument('--source', type=str, default='data/images', help='file/dir/URL/glob, 0 for webcam')
    opt = parser.parse_args()
    print(opt)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = load_model(opt.weights, device)
    detect_one(model, opt.image, device)
