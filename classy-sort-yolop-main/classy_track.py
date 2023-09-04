
# python interpreter searchs these subdirectories for modules

import os
os.environ['KMP_DUPLICATE_LIB_OK']='True'

import sys
sys.path.insert(0, './YOLOP') # 添加环境变量
sys.path.insert(0, './sort')

import argparse

import platform
import shutil
import time
from pathlib import Path
import cv2
import torch
import torch.backends.cudnn as cudnn

#yolov5 YOLOP
from YOLOP.utils.datasets import LoadImages, LoadStreams
from YOLOP.utils.general import check_img_size, non_max_suppression, scale_coords
from YOLOP.utils.torch_utils import select_device, time_synchronized

#SORT
import skimage
from sort import *
import onnxruntime as ort
import numpy as np



torch.set_printoptions(precision=3)

palette = (2 ** 11 - 1, 2 ** 15 - 1, 2 ** 20 - 1)


def bbox_rel(*xyxy):
    """" Calculates the relative bounding box from absolute pixel values. """
    bbox_left = min([xyxy[0].item(), xyxy[2].item()])
    bbox_top = min([xyxy[1].item(), xyxy[3].item()])
    bbox_w = abs(xyxy[0].item() - xyxy[2].item())
    bbox_h = abs(xyxy[1].item() - xyxy[3].item())
    x_c = (bbox_left + bbox_w / 2)
    y_c = (bbox_top + bbox_h / 2)
    w = bbox_w
    h = bbox_h
    return x_c, y_c, w, h


def compute_color_for_labels(label):
    """
    Simple function that adds fixed color depending on the class
    """
    color = [int((p * (label ** 2 - label + 1)) % 255) for p in palette]
    return tuple(color)


def draw_boxes(img, bbox, identities=None, categories=None, names=None, offset=(0, 0)):
    for i, box in enumerate(bbox):
        x1, y1, x2, y2 = [int(i) for i in box]
        x1 += offset[0]
        x2 += offset[0]
        y1 += offset[1]
        y2 += offset[1]
        # box text and bar
        cat = int(categories[i]) if categories is not None else 0

        id = int(identities[i]) if identities is not None else 0

        color = compute_color_for_labels(id)

        label = f'{names[cat]} | {id}'
        t_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_PLAIN, 2, 2)[0]
        cv2.rectangle(img, (x1, y1), (x2, y2), color, 3)
        cv2.rectangle(
            img, (x1, y1), (x1 + t_size[0] + 3, y1 + t_size[1] + 4), color, -1)
        cv2.putText(img, label, (x1, y1 +
                                 t_size[1] + 4), cv2.FONT_HERSHEY_PLAIN, 2, [255, 255, 255], 2)

    return img


def resize_unscale(img, new_shape=(640, 640), color=114):
    shape = img.shape[:2]  # current shape [height, width]
    if isinstance(new_shape, int):
        new_shape = (new_shape, new_shape)

    canvas = np.zeros((new_shape[0], new_shape[1], 3))
    canvas.fill(color)
    # Scale ratio (new / old) new_shape(h,w)
    r = min(new_shape[0] / shape[0], new_shape[1] / shape[1])

    # Compute padding
    new_unpad = int(round(shape[1] * r)), int(round(shape[0] * r))  # w,h
    new_unpad_w = new_unpad[0]
    new_unpad_h = new_unpad[1]
    pad_w, pad_h = new_shape[1] - new_unpad_w, new_shape[0] - new_unpad_h  # wh padding

    dw = pad_w // 2  # divide padding into 2 sides
    dh = pad_h // 2

    if shape[::-1] != new_unpad:  # resize
        img = cv2.resize(img, new_unpad, interpolation=cv2.INTER_AREA)

    canvas[dh:dh + new_unpad_h, dw:dw + new_unpad_w, :] = img

    return canvas, r, dw, dh, new_unpad_w, new_unpad_h  # (dw,dh)

def draw_result_image(img_rgb,boxes,da_seg_out, ll_seg_out):
    # resize & normalize
    height, width, _ = img_rgb.shape
    canvas, r, dw, dh, new_unpad_w, new_unpad_h = resize_unscale(img_rgb, (640, 640))

    save_det_path = f"./YOLOP/pictures/detect_onnx.jpg"
    save_da_path = f"./YOLOP/pictures/da_onnx.jpg"
    save_ll_path = f"./YOLOP/pictures/ll_onnx.jpg"
    save_merge_path = f"./YOLOP/pictures/output_onnx.jpg"

    print("boxes:", boxes, boxes.shape)

    if boxes.shape[0] == 0:
        print("no bounding boxes detected.")
        return

    # scale coords to original size.
    boxes[:, 0] -= dw
    boxes[:, 1] -= dh
    boxes[:, 2] -= dw
    boxes[:, 3] -= dh
    boxes[:, :4] /= r

    print(f"detect {boxes.shape[0]} bounding boxes.")
    print(f"detect {da_seg_out.shape} da_seg_out.")
    print(f"detect {ll_seg_out.shape} ll_seg_out.")

    img_det = img_rgb[:, :, ::-1].copy()
    for i in range(boxes.shape[0]):
        x1, y1, x2, y2, conf, label = boxes[i]
        x1, y1, x2, y2, label = int(x1), int(y1), int(x2), int(y2), int(label)
        img_det = cv2.rectangle(img_det, (x1, y1), (x2, y2), (0, 255, 0), 2, 2)

    cv2.imwrite(save_det_path, img_det)

    # select da & ll segment area.
    da_seg_out = da_seg_out[:, :, dh:dh + new_unpad_h, dw:dw + new_unpad_w]
    ll_seg_out = ll_seg_out[:, :, dh:dh + new_unpad_h, dw:dw + new_unpad_w]

    da_seg_mask = np.argmax(da_seg_out, axis=1)[0]  # (?,?) (0|1)
    ll_seg_mask = np.argmax(ll_seg_out, axis=1)[0]  # (?,?) (0|1)
    print("da_seg_mask.shape", da_seg_mask.shape)
    print("ll_seg_mask.shape", ll_seg_mask.shape)

    color_area = np.zeros((new_unpad_h, new_unpad_w, 3), dtype=np.uint8)
    color_area[da_seg_mask == 1] = [0, 255, 0]
    color_area[ll_seg_mask == 1] = [255, 0, 0]
    color_seg = color_area

    # convert to BGR
    color_seg = color_seg[..., ::-1]
    color_mask = np.mean(color_seg, 2)
    img_merge = canvas[dh:dh + new_unpad_h, dw:dw + new_unpad_w, :]
    img_merge = img_merge[:, :, ::-1]

    # merge: resize to original size
    img_merge[color_mask != 0] = \
        img_merge[color_mask != 0] * 0.5 + color_seg[color_mask != 0] * 0.5
    img_merge = img_merge.astype(np.uint8)
    img_merge = cv2.resize(img_merge, (width, height),
                           interpolation=cv2.INTER_LINEAR)
    for i in range(boxes.shape[0]):
        x1, y1, x2, y2, conf, label = boxes[i]
        x1, y1, x2, y2, label = int(x1), int(y1), int(x2), int(y2), int(label)
        img_merge = cv2.rectangle(img_merge, (x1, y1), (x2, y2), (0, 255, 0), 2, 2)

    # da: resize to original size
    da_seg_mask = da_seg_mask * 255
    da_seg_mask = da_seg_mask.astype(np.uint8)
    da_seg_mask = cv2.resize(da_seg_mask, (width, height),
                             interpolation=cv2.INTER_LINEAR)

    # ll: resize to original size
    ll_seg_mask = ll_seg_mask * 255
    ll_seg_mask = ll_seg_mask.astype(np.uint8)
    ll_seg_mask = cv2.resize(ll_seg_mask, (width, height),
                             interpolation=cv2.INTER_LINEAR)

    cv2.imwrite(save_merge_path, img_merge)
    cv2.imwrite(save_da_path, da_seg_mask)
    cv2.imwrite(save_ll_path, ll_seg_mask)

    print("detect done.")

def infer_yolop(weight="yolop-640-640.onnx",img_path="./inference/images/7dd9ef45-f197db95.jpg"):

    ort.set_default_logger_severity(4)
    onnx_path = f"./YOLOP/weights/{weight}"
    ort_session = ort.InferenceSession(onnx_path)
    print(f"Load {onnx_path} done!")

    outputs_info = ort_session.get_outputs()
    inputs_info = ort_session.get_inputs()

    for ii in inputs_info:
        print("Input: ", ii)
    for oo in outputs_info:
        print("Output: ", oo)

    save_det_path = f"./YOLOP/pictures/detect_onnx.jpg"
    save_da_path = f"./YOLOP/pictures/da_onnx.jpg"
    save_ll_path = f"./YOLOP/pictures/ll_onnx.jpg"
    save_merge_path = f"./YOLOP/pictures/output_onnx.jpg"


    img_bgr = cv2.imread(img_path)
    height, width, _ = img_bgr.shape

    # convert to RGB
    img_rgb = img_bgr[:, :, ::-1].copy()

    # resize & normalize
    canvas, r, dw, dh, new_unpad_w, new_unpad_h = resize_unscale(img_rgb, (640, 640))

    img = canvas.copy().astype(np.float32)  # (3,640,640) RGB
    img /= 255.0
    img[:, :, 0] -= 0.485
    img[:, :, 1] -= 0.456
    img[:, :, 2] -= 0.406
    img[:, :, 0] /= 0.229
    img[:, :, 1] /= 0.224
    img[:, :, 2] /= 0.225

    img = img.transpose(2, 0, 1)

    img = np.expand_dims(img, 0)  # (1, 3,640,640)

    print("img shape :",img.shape)
    # inference: (1,n,6) (1,2,640,640) (1,2,640,640)
    det_out, da_seg_out, ll_seg_out = ort_session.run(
        ['det_out', 'drive_area_seg', 'lane_line_seg'],
        input_feed={"images": img}
    )

    det_out = torch.from_numpy(det_out).float()
    boxes = non_max_suppression(det_out)[0]  # [n,6] [x1,y1,x2,y2,conf,cls]
    boxes = boxes.cpu().numpy().astype(np.float32)
    print("boxes:",  boxes.shape)

    return img_rgb,boxes

    draw_result_image(img_rgb, boxes,da_seg_out, ll_seg_out) # 绘制结果图像


def detect(opt, img_rgb,boxes,*args):
    out, source, weights, view_img, save_txt, imgsz, save_img, sort_max_age, sort_min_hits, sort_iou_thresh = \
        opt.output, opt.img, opt.weight, opt.view_img, opt.save_txt, opt.img_size, opt.save_img, opt.sort_max_age, opt.sort_min_hits, opt.sort_iou_thresh

    names = ['car']

    webcam = source == '0' or source.startswith(
        'rtsp') or source.startswith('http') or source.endswith('.txt')
    # Initialize SORT
    sort_tracker = Sort(max_age=sort_max_age,
                        min_hits=sort_min_hits,
                        iou_threshold=sort_iou_thresh)  # {plug into parser}

    print(boxes,img_rgb.shape)
    # Process detections
    img1_shape = [640,640]
    for i, det in enumerate([boxes]):  # for each detection in this frame # im0s为原始图像，其尺寸为 720* 1280
        print("imgsz is :",imgsz)
        det = torch.tensor(det)


        p, s, im0 = r"./YOLOP/inference/images/9aa94005-ff1d4c9a.jpg", '', img_rgb

        # Rescale boxes from img_size (temporarily downscaled size) to im0 (native) size
        det[:, :4] = scale_coords(img1_shape,det[:,:4],img_rgb.shape)  # im0 为原始图像尺寸···
        print(det)

        dets_to_sort = np.empty((0, 6))
        # Pass detections to SORT
        # NOTE: We send in detected object class too
        for x1, y1, x2, y2, conf, detclass in det.cpu().detach().numpy():
            dets_to_sort = np.vstack((dets_to_sort, np.array([x1, y1, x2, y2, conf, detclass])))
        print('\n')
        print('Input into SORT:\n', dets_to_sort, '\n')

        # Run SORT
        tracked_dets = sort_tracker.update(dets_to_sort)
        print('Output from SORT:\n', tracked_dets, '\n')
        # draw boxes for visualization
        if len(tracked_dets) > 0:
            bbox_xyxy = tracked_dets[:, :4]
            identities = tracked_dets[:, 8]
            categories = tracked_dets[:, 4]
            draw_boxes(img_rgb, bbox_xyxy, identities, categories, names)

            # Write detections to file. NOTE: Not MOT-compliant format.
            if save_txt and len(tracked_dets) != 0:
                for j, tracked_dets in enumerate(tracked_dets):
                    bbox_x1 = tracked_dets[0]
                    bbox_y1 = tracked_dets[1]
                    bbox_x2 = tracked_dets[2]
                    bbox_y2 = tracked_dets[3]
                    category = tracked_dets[4]
                    u_overdot = tracked_dets[5]
                    v_overdot = tracked_dets[6]
                    s_overdot = tracked_dets[7]
                    identity = tracked_dets[8]

                    print("txt_path:",txt_path)
                    with open(txt_path, 'a') as f:
                        f.write(
                            f'{frame_idx},{bbox_x1},{bbox_y1},{bbox_x2},{bbox_y2},{category},{u_overdot},{v_overdot},{s_overdot},{identity}\n')

            # Stream images results(opencv)
            if not view_img:
                cv2.imshow(p, img_rgb)
                if cv2.waitKey(1) == ord('q'):  # q to quit
                    raise StopIteration
            # Save video results
            save_path = r"./YOLOP/pictures/detect_trater.jpg"
            if not save_img:
                print('saving img!')
                # if dataset.mode == 'images':
                cv2.imwrite(save_path, img_rgb)
                # else:
                #     print('saving video!')
                #     if vid_path != save_path:  # new video
                #         vid_path = save_path
                #         if isinstance(vid_writer, cv2.VideoWriter):
                #             vid_writer.release()  # release previous video writer
                #
                #         fps = vid_cap.get(cv2.CAP_PROP_FPS)
                #         w = int(vid_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                #         h = int(vid_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                #         vid_writer = cv2.VideoWriter(
                #             save_path, cv2.VideoWriter_fourcc(*opt.fourcc), fps, (w, h))
                #     vid_writer.write(im0)

if __name__ == '__main__':


    parser = argparse.ArgumentParser()
    parser.add_argument('--weight', type=str, default="yolop-640-640_origin.onnx")
    parser.add_argument('--img', type=str, default="./YOLOP/inference/images/9aa94005-ff1d4c9a.jpg")
    # parser.add_argument('--img', type=str, default="./test.jpg")

    parser.add_argument('--output', type=str, default='inference/output',
                        help='output folder')  # output folder
    parser.add_argument('--img-size', type=int, default=1080,
                        help='inference size (pixels)')
    parser.add_argument('--conf-thres', type=float,
                        default=0.3, help='object confidence threshold')
    parser.add_argument('--iou-thres', type=float,
                        default=0.4, help='IOU threshold for NMS')
    parser.add_argument('--fourcc', type=str, default='mp4v',
                        help='output video codec (verify ffmpeg support)')
    parser.add_argument('--device', default='',
                        help='cuda device, i.e. 0 or 0,1,2,3 or cpu')
    parser.add_argument('--view-img', action='store_true',
                        help='display results')
    parser.add_argument('--save-img', action='store_true',
                        help='save video file to output folder (disable for speed)')
    parser.add_argument('--save-txt', action='store_true',
                        help='save results to *.txt')
    parser.add_argument('--classes', nargs='+', type=int,
                        default=[i for i in range(80)], help='filter by class')  # 80 classes in COCO dataset
    parser.add_argument('--agnostic-nms', action='store_true',
                        help='class-agnostic NMS')
    parser.add_argument('--augment', action='store_true',
                        help='augmented inference')

    # SORT params
    parser.add_argument('--sort-max-age', type=int, default=5,
                        help='keep track of object even if object is occluded or not detected in n frames')
    parser.add_argument('--sort-min-hits', type=int, default=2,
                        help='start tracking only after n number of objects detected')
    parser.add_argument('--sort-iou-thresh', type=float, default=0.2,
                        help='intersection-over-union threshold between two frames for association')
    args = parser.parse_args()
    print(args)

    img_rgb,boxes, = infer_yolop(weight=args.weight, img_path=args.img)
    with torch.no_grad():
        detect(args,img_rgb,boxes)



    # with torch.no_grad():
    #     detect(args)
