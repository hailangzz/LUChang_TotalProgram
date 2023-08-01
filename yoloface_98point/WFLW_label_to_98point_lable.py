import os.path
import sys
import torch
import torch.utils.data as data
import cv2
import numpy as np


class WiderFaceDetection(data.Dataset):
    def __init__(self, data_path, preproc=None):
        self.preproc = preproc
        self.imgs_path = []
        self.words = []
        txt_path = os.path.join(data_path, "list_98pt_rect_attr_train.txt")
        f = open(txt_path, 'r')
        lines = f.readlines()
        isFirst = True
        labels = []
        for line in lines[:100]:
            line = line.strip().split()
            label = line[0:200]  # type(line)：<class 'list'>, len(line)：201
            label = [float(x) for x in label]
            imgname = line[-1]  # 图片名，如：'jd_train_9.jpg'
            self.imgs_path.append(os.path.join(data_path, "WFLW_images", imgname))  # 图像路径
            # self.imgs_path.append(os.path.join(data_path,imgname))  # 图像路径
            labels.append(label)

        # self.words.append(labels)
        self.words = labels

    def __len__(self):
        return len(self.imgs_path)

    def __getitem__(self, index):
        img = cv2.imread(self.imgs_path[index])
        height, width, _ = img.shape

        labels = self.words[index]
        # annotations = np.zeros((0, 15))
        annotations = np.zeros((0, 201))
        if len(labels) == 0:
            return annotations
        annotation = np.zeros((1, 201))     # 1行201列
        # bbox，原label.txt中的后4位是bbox相关坐标信息？

        # labels[-4] = (labels[-4] + labels[-2]) / 2 # 中心点坐标x
        # labels[-3] = (labels[-3] + labels[-1]) / 2 # 中心点坐标y
        # labels[-2] = (labels[-2] - labels[-4]) / 2 # w
        # labels[-1] = (labels[-1] - labels[-3]) / 2 # h

        annotation[0, 0] = labels[-4]  # x1       # annotation[0, 0]：第0行，第0列，即第一个数据
        annotation[0, 1] = labels[-3]  # y1
        annotation[0, 2] = labels[-2]  # x2
        annotation[0, 3] = labels[-1]  # y2
        print("===============")
        # landmarks，1~196之间的是landmarks相关信息

        # for idx, label in enumerate(labels):
        for nx in range(196):
            annotation[0, 4+nx] = labels[nx]

        if annotation[0, 4] < 0:
            annotation[0, 200] = -1
        else:
            annotation[0, 200] = 1

        annotations = np.append(annotations, annotation, axis=0)
        target = np.array(annotations)
        if self.preproc is not None:
            img, target = self.preproc(img, target)

        return torch.from_numpy(img), target


def detection_collate(batch):
    """Custom collate fn for dealing with batches of images that have a different
    number of associated object annotations (bounding boxes).

    Arguments:
        batch: (tuple) A tuple of tensor images and lists of annotations

    Return:
        A tuple containing:
            1) (tensor) batch of images stacked on their 0 dim
            2) (list of tensors) annotations for a given image are stacked on 0 dim
    """
    targets = []
    imgs = []
    for _, sample in enumerate(batch):
        for _, tup in enumerate(sample):
            if torch.is_tensor(tup):
                imgs.append(tup)
            elif isinstance(tup, type(np.empty(0))):
                annos = torch.from_numpy(tup).float()
                targets.append(annos)

    return torch.stack(imgs, 0), targets


if __name__ == '__main__':

    original_path = r'F:\AiTotalDatabase\人脸关键点数据集\WFLW_Face\WFLW_images'
    save_path = r'F:\AiTotalDatabase\人脸关键点数据集\WFLW_Face\WFLW_yolo\train'

    aa = WiderFaceDetection(original_path)
    # print(aa)
    # print(type(aa))
    # tmp = torch.utils.data.DataLoader(aa, batch_size=1)
    # for i, (input, target) in enumerate(tmp):
    #     print("当前批样例的input为：{}".format(input.size()))
    #     print("当前样例的target为：{}".format(target))

    for i in range(len(aa.imgs_path)):
        print(i, aa.imgs_path[i])
        # img = cv2.imread(aa.imgs_path[i])
        img = cv2.imdecode(np.fromfile(aa.imgs_path[i], dtype=np.uint8), -1)
        # 保存image与label
        base_img = os.path.basename(aa.imgs_path[i])
        base_txt = os.path.basename(aa.imgs_path[i])[:-4] + ".txt"
        save_img_path = os.path.join(save_path,'images', base_img)
        save_txt_path = os.path.join(save_path,'labels', base_txt)

        with open(save_txt_path, "w") as f:
            height, width, _ = img.shape
            # print("height为：{} || width为：{}".format(height, width))
            labels = aa.words[i]
            annotations = np.zeros((0, 200))
            if len(labels) == 0:
                continue

            # for idx, label in enumerate(labels):
            annotation = np.zeros((1, 200))

            # 注意：lables[-4]，labels[-3]为原标签中的bbox左上角坐标；labels[-2]，labels[-1]为原标签中的bbox的右下角坐标
            labels[-4] = max(0, labels[-4])
            labels[-3] = max(0, labels[-3])
            labels[-2] = labels[-2] - labels[-4]
            labels[-1] = labels[-1] - labels[-3]
            labels[-2] = min(width - 1, labels[-2])
            labels[-1] = min(height - 1, labels[-1])

            annotation[0, 0] = (labels[-4] + labels[-2] / 2) / width    # cx
            annotation[0, 1] = (labels[-3] + labels[-1] / 2) / height    # cy
            annotation[0, 2] = labels[-2] / width    # w
            annotation[0, 3] = labels[-1] / height    # h
            # annotation[0, 0] = (labels[0] + labels[2] / 2) / width  # cx
            # annotation[0, 1] = (labels[1] + labels[3] / 2) / height  # cy
            # annotation[0, 2] = labels[2] / width  # w
            # annotation[0, 3] = labels[3] / height  # h

            for nx in range(196):
                if nx % 2 ==0:
                    annotation[0, 4 + nx] = labels[nx] / width
                else:
                    annotation[0, 4 + nx] = labels[nx] / height
            str_label = "0"
            for i in range(len(annotation[0])):
                # str_label = str_label + " " + str(annotation[0][i])
                str_label = str_label + " " + str(annotation[0][i])
            # str_label = str_label.replace('[', '').replace(']', '')
            # str_label = str_label.replace(',', '') + '\n'
            f.write(str_label)
        # cv2.imwrite(save_img_path, img)
        cv2.imencode('.jpg', img)[1].tofile(save_img_path)

