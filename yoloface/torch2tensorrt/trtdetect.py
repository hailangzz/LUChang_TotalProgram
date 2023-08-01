import os
import sys
import cv2
import copy
import torch
import argparse

root_path = os.path.dirname(os.path.abspath(os.path.dirname(__file__)))  # 项目根路径：获取当前路径，再上级路径
sys.path.append(root_path)  # 将项目根路径写入系统路径
from utils.general import check_img_size, non_max_suppression_face, scale_coords, xyxy2xywh
from utils.datasets import letterbox
from torch2tensorrt.yolo_trt_model import YoloTrtModel
from detect_face import scale_coords_landmarks, show_results

cur_path = os.path.abspath(os.path.dirname(__file__))
from utils.torch_utils import time_synchronized


def img_process(img_path, img1=str(0),long_side=640, stride_max=32):
    '''
    图像预处理
    如果输入的是图片路径，则img_path为路径，img1为0
    如果输入的是视频，则img_path不重要，img1是视频原图
    无论输入的视频还是图片路径是什么样子，最终输出的，识别后的都是640*640
    事实证明后面那两个参数没用，因为输入图像大小一定是原图，输出一定是onnx大小
    '''
    if img1!= str(0):   #如果输入的是视频 即传入source是0（默认是none）
        orgimg=img1     #则令orgimg等于视频
    else:               # 如果输入的不是视频
        orgimg = cv2.imread(img_path)
    img0 = copy.deepcopy(orgimg)
    # h0, w0 = orgimg.shape[:2]  # orig hw  直接读取得到的
    # r = long_side / max(h0, w0)  # resize image to img_size
    # if r != 1:  # always resize down, only resize up if training with augmentation
    #     interp = cv2.INTER_AREA if r < 1 else cv2.INTER_LINEAR
    #     img0 = cv2.resize(img0, (int(w0 * r), int(h0 * r)), interpolation=interp)
    #
    # imgsz = check_img_size(long_side, s=stride_max)  # check img_size

    img = letterbox(img0, new_shape=opt.trt_size, auto=False)[0]  # auto True最小矩形 ,缩放成想要的矩形  False固定尺度  传入的img_size是img0，也就是未缩放的图，img是按长边缩放得到的矩形，
    # Convert
    img = img[:, :, ::-1].transpose(2, 0, 1).copy()  # BGR to RGB, to 3x416x416
    img = torch.from_numpy(img)
    img = img.float()  # uint8 to fp16/32
    img /= 255.0  # 0 - 255 to 0.0 - 1.0
    if img.ndimension() == 3:
        img = img.unsqueeze(0)
    return img, orgimg




def img_vis(img, orgimg, pred, device, vis_thres=0.6,save_jpg=True):
    '''
    预测可视化
    vis_thres: 可视化阈值
    这里传入img除了print一下并没有什么作用
    默认图片保存，视频直接imshow不保存
    '''

    print('         img.shape: ', img.shape)
    print('         orgimg.shape: ', orgimg.shape)

    no_vis_nums = 0
    # Process detections
    for i, det in enumerate(pred):  # detections per image
        gn = torch.tensor(orgimg.shape)[[1, 0, 1, 0]].to(device)  # normalization gain whwh
        gn_lks = torch.tensor(orgimg.shape)[[1, 0, 1, 0, 1, 0, 1, 0]].to(device)  # normalization gain landmarks   去掉两个
        if len(det):
            # Rescale boxes from img_size to im0 size
            det[:, :4] = scale_coords(img.shape[2:], det[:, :4], orgimg.shape).round()

            # Print results
            for c in det[:, -1].unique():
                n = (det[:, -1] == c).sum()  # detections per class

            det[:, 5:13] = scale_coords_landmarks(img.shape[2:], det[:, 5:13], orgimg.shape).round()  # 15变13

            for j in range(det.size()[0]):

                if det[j, 4].cpu().numpy() < vis_thres:
                    no_vis_nums += 1
                    continue

                xywh = (xyxy2xywh(det[j, :4].view(1, 4)) / gn).view(-1).tolist()
                conf = det[j, 4].cpu().numpy()
                landmarks = (det[j, 5:13].view(1, 8) / gn_lks).view(-1).tolist()  # 15变13，10变8
                class_num = det[j, 13].cpu().numpy()  # 15变13
                orgimg = show_results(orgimg, xywh, conf, landmarks, class_num)
    if save_jpg == True:
        cv2.imwrite(cur_path + '/result.jpg', orgimg)
        print('result save in ' + cur_path + '/result.jpg')


if __name__ == '__main__':
    # ============参数================
    parser = argparse.ArgumentParser()   #所有路径根目录都是yoloface训练
    parser.add_argument('--weights', type=str, default= 'weights/2000+3000.trt', help='weights path')  # from yolov5/models/
    parser.add_argument('--trt_size', nargs='+', type=int, default=[640, 640], help='must same as onnx,imput 2 numbers')  # 在letterbox函数调用 必须与onnx模型匹配
    parser.add_argument('--source', type=str, default='data/images/1.jpeg', help='file/dir/URL/glob, 0 for webcam') #判断信息源 如果为0，1，2，200则认为是视频流
    parser.add_argument('--cls', type=int, default=17, help='class numbers')  #在初始化引擎时调用 必须与onnx模型匹配
    parser.add_argument('--conf_thres', type=float, default=0.25, help='confidence threshold')  #在非极大值抑制时调用
    parser.add_argument('--iou_thres', type=float, default=0.45, help='NMS IoU threshold') #在非极大值抑制时调用
    opt = parser.parse_args()
    if len(opt.trt_size)!=2:
        print("error:--trt_size must imput 2 numbers")
        exit()


    # ============参数处理=============
    device = "cuda:0"
    fp16_mode = True  # True则FP16推理
    onnx_model_path = root_path + "/" + opt.weights  # ONNX模型路径
    # print(opt.img_size)

    # ============初始化TensorRT引擎================
    t0 = time_synchronized()  # 计算时间
    yolo_trt_model = YoloTrtModel(device, onnx_model_path, fp16_mode, opt.cls,opt.trt_size[0],opt.trt_size[1])
    t = time_synchronized()  # 计算时间
    print("初始化引擎：", t - t0)

    # ============判断视频还是图片================
    if (opt.source == str(0) or opt.source == str(1) or opt.source == str(2) or opt.source == str(200)):
        capture = cv2.VideoCapture(int(opt.source))
        while (True):
            t1 = time_synchronized()
            ret, frame = capture.read()  # ret为返回值，frame为视频的每一帧

            img, orgimg = img_process(opt.source,frame)  #视频预处理
            t2 = time_synchronized()#视频预处理时间

            pred = yolo_trt_model(img.cpu().numpy())  # tensorrt推理
            t3 = time_synchronized() # tensorrt推理时间

            pred = yolo_trt_model.after_process(pred, device)  # torch后处理
            t4 = time_synchronized()# torch后处理时间

            pred = non_max_suppression_face(pred, conf_thres=opt.conf_thres, iou_thres=opt.iou_thres) # 非极大值抑制
            t5 = time_synchronized() # 非极大值抑制时间

            img_vis(img, orgimg, pred, device,save_jpg=False) #可视化
            cv2.imshow("video", orgimg)
            c = cv2.waitKey(1)
            if c == 27:  # 按了esc候可以退出
                break
            t6 = time_synchronized()  # 可视化时间

            print("视频流读取：", t2 - t1)
            print("tensorrt推理：", t3 - t2)
            print("torch后处理：", t4 - t3)
            print("非极大值抑制：", t5 - t4)
            print("可视化：", t6 - t5)
            print("总耗时", t6 - t1)

    else:
        img_path = root_path + "/" + opt.source  # 图片路径
        t1 = time_synchronized()

        img, orgimg = img_process(img_path)  # 图像预处理
        t2 = time_synchronized()  # 图像预处理时间

        pred = yolo_trt_model(img.cpu().numpy())  # tensorrt推理
        t3 = time_synchronized()  # tensorrt推理时间

        pred = yolo_trt_model.after_process(pred, device)  # torch后处理
        t4 = time_synchronized()  # torch后处理时间

        pred = non_max_suppression_face(pred, conf_thres=opt.conf_thres, iou_thres=opt.iou_thres) # 非极大值抑制
        t5 = time_synchronized() # 非极大值抑制时间

        img_vis(img, orgimg, pred, device)
        t6 = time_synchronized() # 可视化时间

        print("图像预处理：", t2 - t1)
        print("tensorrt推理：", t3 - t2)
        print("torch后处理：", t4 - t3)
        print("非极大值抑制：", t5 - t4)
        print("图像可视化：", t6 - t5)
