import torch
import torch.nn as nn
#from lib.models.common import Conv, SPP, Bottleneck, BottleneckCSP, Focus, Concat, Detect, SharpenConv  #origin network model struct
from lib.models.common2 import Conv, SPP, Bottleneck, BottleneckCSP, Focus, Concat, Detect, SharpenConv,C3,SPPF,SUBM  #注：lib.models.common2 为导出为C++可用的模型结构
from torch.nn import Upsample
from lib.utils import check_anchor_order
from lib.utils import initialize_weights
import argparse
import onnx
import onnxruntime as ort
import onnxsim

import math
import cv2

# The lane line and the driving area segment branches without share information with each other and without link
YOLOP = [
    # [24, 33, 42],  # Det_out_idx, Da_Segout_idx, LL_Segout_idx
    [24, 33],  # Det_out_idx, LL_Segout_idx
    [-1, Focus, [3, 32, 3]],  # 0
    [-1, Conv, [32, 64, 3, 2]],  # 1
    [-1, BottleneckCSP, [64, 64, 1]],  # 2
    [-1, Conv, [64, 128, 3, 2]],  # 3
    [-1, BottleneckCSP, [128, 128, 3]],  # 4
    [-1, Conv, [128, 256, 3, 2]],  # 5
    [-1, BottleneckCSP, [256, 256, 3]],  # 6
    [-1, Conv, [256, 512, 3, 2]],  # 7
    [-1, SPP, [512, 512, [5, 9, 13]]],  # 8 SPP
    [-1, BottleneckCSP, [512, 512, 1, False]],  # 9
    [-1, Conv, [512, 256, 1, 1]],  # 10
    [-1, Upsample, [None, 2, 'nearest']],  # 11
    [[-1, 6], Concat, [1]],  # 12
    [-1, BottleneckCSP, [512, 256, 1, False]],  # 13
    [-1, Conv, [256, 128, 1, 1]],  # 14
    [-1, Upsample, [None, 2, 'nearest']],  # 15
    [[-1, 4], Concat, [1]],  # 16         #Encoder

    [-1, BottleneckCSP, [256, 128, 1, False]],  # 17
    [-1, Conv, [128, 128, 3, 2]],  # 18
    [[-1, 14], Concat, [1]],  # 19
    [-1, BottleneckCSP, [256, 256, 1, False]],  # 20
    [-1, Conv, [256, 256, 3, 2]],  # 21
    [[-1, 10], Concat, [1]],  # 22
    [-1, BottleneckCSP, [512, 512, 1, False]],  # 23
    [[17, 20, 23], Detect,
     [1, [[3, 9, 5, 11, 4, 20], [7, 18, 6, 39, 12, 31], [19, 50, 38, 81, 68, 157]], [128, 256, 512]]],
    # Detection head 24: from_(features from specific layers), block, nc(num_classes) anchors ch(channels)

    [16, Conv, [256, 128, 3, 1]],  # 25
    [-1, Upsample, [None, 2, 'nearest']],  # 26
    [-1, BottleneckCSP, [128, 64, 1, False]],  # 27
    [-1, Conv, [64, 32, 3, 1]],  # 28
    [-1, Upsample, [None, 2, 'nearest']],  # 29
    [-1, Conv, [32, 16, 3, 1]],  # 30
    [-1, BottleneckCSP, [16, 8, 1, False]],  # 31
    [-1, Upsample, [None, 2, 'nearest']],  # 32
    [-1, Conv, [8, 2, 3, 1]],  # 33 Driving area segmentation head

    # [16, Conv, [256, 128, 3, 1]],  # 34
    # [-1, Upsample, [None, 2, 'nearest']],  # 35
    # [-1, BottleneckCSP, [128, 64, 1, False]],  # 36
    # [-1, Conv, [64, 32, 3, 1]],  # 37
    # [-1, Upsample, [None, 2, 'nearest']],  # 38
    # [-1, Conv, [32, 16, 3, 1]],  # 39
    # [-1, BottleneckCSP, [16, 8, 1, False]],  # 40
    # [-1, Upsample, [None, 2, 'nearest']],  # 41
    # [-1, Conv, [8, 2, 3, 1]]  # 42 Lane line segmentation head
]

YOLOP_MY = [
# [24, 33, 42],   #Det_out_idx, Da_Segout_idx, LL_Segout_idx
[24, 39],   #Det_out_idx, Da_Segout_idx, LL_Segout_idx
[ -1, Focus, [3, 32, 3]],   #0                            [-1, 32, 128, 128]
[ -1, Conv, [32, 64, 3, 2]],    #1                        [-1, 64, 64, 64]
[ -1, C3, [64, 48, 1]],  #2                               [-1, 48, 64, 64]
[ -1, Conv, [48, 96, 3, 2]],   #3                         [-1, 96, 32, 32]
[ -1, C3, [96, 96, 1]],    #4                             [-1, 96, 32, 32]
[ -1, Conv, [96, 192, 3, 2]],  #5                         [-1, 192, 16, 16]
[ -1, C3, [192, 192, 2]],    #6                           [-1, 192, 16, 16]
[ -1, Conv, [192, 384, 3, 2]],  #7                        [-1, 384, 8, 8]
# [ -1, SPP, [512, 512, [5, 9, 13]]],     #8
[ -1, SPPF, [384, 384]],     #8                           [-1, 384, 8, 8]
[ -1, C3, [384, 384, 1, False]],     #9                   [-1, 384, 8, 8]
[ -1, Conv,[384, 192, 1, 1]],   #10                       [-1, 192, 8, 8]
[ -1, Upsample, [None, 2, 'nearest']],  #11               [-1, 192, 16, 16]
[ [-1, 6], Concat, [1]],    #12                           [-1, 384, 16, 16]
[ -1, C3, [384, 192, 1, False]], #13                      [-1, 192, 16, 16]
[ -1, Conv, [192, 96, 1, 1]],  #14                        [-1, 96, 16, 16]
[ -1, Upsample, [None, 2, 'nearest']],  #15               [-1, 96, 32, 32]
[ [-1,4], Concat, [1]],     #16         #Encoder          [-1, 192, 32, 32]

[ -1, C3, [192, 96, 1, False]],     #17                   [-1, 96, 32, 32]
[ -1, Conv, [96, 96, 3, 2]],      #18                     [-1, 96, 16, 16]
[ [-1, 14], Concat, [1]],       #19                       [-1, 192, 16, 16]
[ -1, C3, [192, 192, 1, False]],     #20                  [-1, 192, 16, 16]
[ -1, Conv, [192, 192, 3, 2]],      #21                   [-1, 192, 8, 8]
[ [-1, 10], Concat, [1]],   #22                           [-1, 384, 8, 8]
[ -1, C3, [384, 384, 1, False]],     #23                  [-1, 384, 8, 8]
[ [17, 20, 23], Detect,  [1, [[3,9,5,11,4,20], [7,18,6,39,12,31], [19,50,38,81,68,157]], [96, 192, 384]]], #Detection head 24  检测类别数:1 自定义 anchors:[[3,9,5,11,4,20], [7,18,6,39,12,31], [19,50,38,81,68,157]] 特征通道数：[128, 256, 512]
[[13,20], SUBM, [0.8]],    #25  # [-1, 192, 16, 16]
[ -1, Conv, [192, 96, 3, 1]],   #26 #[-1, 96, 16, 16]
[ -1, Upsample, [None, 2, 'nearest']],  #27 #[-1, 96, 32, 32]
[ [15,-1], Concat, [1]], # 28 # [192,32, 32]
[ -1, C3, [192, 96, 1, False]],  #29 #[-1, 96, 32, 32]
[[17,-1], SUBM, [0.8]],    #30  # [-1, 96, 32, 32]
[ -1, Conv, [96, 48, 3, 1]],   #31 #[-1, 48, 32, 32]
[ -1, Upsample, [None, 2, 'nearest']],  #32 #[-1, 48, 64, 64]
[[2,32], SUBM, [0.8]],    #33  # [-1, 48, 64, 64]
[ -1, C3, [48, 32, 1, False]],  #34 #[-1, 24, 64, 64]
[ -1, Conv, [32, 16, 3, 1]],   #35 #[-1, 32, 64, 64]
[ -1, Upsample, [None, 2, 'nearest']],  #36 #[-1, 16, 128, 128]

[ -1, C3, [16, 8, 1, False]],  #37 #[-1, 8, 128, 128]
[ -1, Upsample, [None, 2, 'nearest']],  #38 #[-1, 8, 256, 256]
[ -1, Conv, [8, 2, 3, 1]],   #39 #[-1, 2, 256, 256]
]

YOLOP_MY2 = [
# [24, 33, 42],   #Det_out_idx, Da_Segout_idx, LL_Segout_idx
[24, 41],   #Det_out_idx, Da_Segout_idx, LL_Segout_idx
[ -1, Focus, [3, 32, 3]],   #0                            [-1, 32, 128, 128]
[ -1, Conv, [32, 64, 3, 2]],    #1                        [-1, 64, 64, 64]
[ -1, C3, [64, 48, 1]],  #2                               [-1, 48, 64, 64]
[ -1, Conv, [48, 96, 3, 2]],   #3                         [-1, 96, 32, 32]
[ -1, C3, [96, 96, 1]],    #4                             [-1, 96, 32, 32]
[ -1, Conv, [96, 192, 3, 2]],  #5                         [-1, 192, 16, 16]
[ -1, C3, [192, 192, 2]],    #6                           [-1, 192, 16, 16]
[ -1, Conv, [192, 384, 3, 2]],  #7                        [-1, 384, 8, 8]
# [ -1, SPP, [512, 512, [5, 9, 13]]],     #8
[ -1, SPPF, [384, 384]],     #8                           [-1, 384, 8, 8]
[ -1, C3, [384, 384, 1, False]],     #9                   [-1, 384, 8, 8]
[ -1, Conv,[384, 192, 1, 1]],   #10                       [-1, 192, 8, 8]
[ -1, Upsample, [None, 2, 'nearest']],  #11               [-1, 192, 16, 16]
[ [-1, 6], Concat, [1]],    #12                           [-1, 384, 16, 16]
[ -1, C3, [384, 192, 1, False]], #13                      [-1, 192, 16, 16]
[ -1, Conv, [192, 96, 1, 1]],  #14                        [-1, 96, 16, 16]
[ -1, Upsample, [None, 2, 'nearest']],  #15               [-1, 96, 32, 32]
[ [-1,4], Concat, [1]],     #16         #Encoder          [-1, 192, 32, 32]

[ -1, C3, [192, 96, 1, False]],     #17                   [-1, 96, 32, 32]
[ -1, Conv, [96, 96, 3, 2]],      #18                     [-1, 96, 16, 16]
[ [-1, 14], Concat, [1]],       #19                       [-1, 192, 16, 16]
[ -1, C3, [192, 192, 1, False]],     #20                  [-1, 192, 16, 16]
[ -1, Conv, [192, 192, 3, 2]],      #21                   [-1, 192, 8, 8]
[ [-1, 10], Concat, [1]],   #22                           [-1, 384, 8, 8]
[ -1, C3, [384, 384, 1, False]],     #23                  [-1, 384, 8, 8]
[ [17, 20, 23], Detect,  [1, [[3,9,5,11,4,20], [7,18,6,39,12,31], [19,50,38,81,68,157]], [96, 192, 384]]], #Detection head 24  检测类别数:1 自定义 anchors:[[3,9,5,11,4,20], [7,18,6,39,12,31], [19,50,38,81,68,157]] 特征通道数：[128, 256, 512]

[[4,15], SUBM, [0.8]],    #25  # [-1, 96, 32, 32]
[ -1, Conv, [96, 48, 3, 1]],   #26 #[-1, 96, 32, 32]
[ -1, C3, [48, 48, 1, False]],  #27 #[-1, 96, 32, 32]

[[6,13], SUBM, [0.8]],    #28  # [-1, 192, 16, 16]
[ -1, Conv, [192, 96, 3, 1]],   #29 #[-1, 96, 16, 16]
[ -1, C3, [96, 48, 1, False]],  #30 #[-1, 96, 16, 16]
[ -1, Upsample, [None, 2, 'nearest']],  #31 #[-1, 96, 32, 32]
[ [27,-1], Concat, [1]], # 32 # [96,32, 32]

[ -1, C3, [96, 48, 1, False]],  #33 #[-1, 48, 32, 32]
[ -1, Upsample, [None, 2, 'nearest']],  #34 #[-1, 48, 64, 64]
[[2,-1], SUBM, [0.8]],    #35  # [-1, 48, 64, 64]


[ -1, C3, [48, 32, 1, False]],  #36 #[-1, 32, 64, 64]
[ -1, Conv, [32, 16, 3, 1]],   #37 #[-1, 16, 64, 64]
[ -1, Upsample, [None, 2, 'nearest']],  #38 #[-1, 16, 128, 128]

[ -1, C3, [16, 8, 1, False]],  #39 #[-1, 8, 128, 128]
[ -1, Upsample, [None, 2, 'nearest']],  #40 #[-1, 8, 256, 256]
[ -1, Conv, [8, 2, 3, 1]],   #41 #[-1, 2, 256, 256]
]


class MCnet(nn.Module):
    def __init__(self, block_cfg):
        super(MCnet, self).__init__()
        layers, save = [], []
        self.nc = 1  # traffic or not
        self.detector_index = -1  #记录模型检测输出网络结构层索引信息
        self.det_out_idx = block_cfg[0][0]
        self.seg_out_idx = block_cfg[0][1:]
        self.num_anchors = 3
        self.num_outchannel = 5 + self.nc  # dx,dy,dw,dh,obj_conf+cls_conf
        # Build model
        for i, (from_, block, args) in enumerate(block_cfg[1:]):  # block_cfg 为网络结构数组信息，from_：上一层网络索引, block：当前网络名称, args：当前网络参数
            block = eval(block) if isinstance(block, str) else block  # eval strings
            if block is Detect:
                self.detector_index = i
            block_ = block(*args)
            block_.index, block_.from_ = i, from_
            layers.append(block_)
            save.extend(x % i for x in ([from_] if isinstance(from_, int) else from_) if x != -1)  # append to savelist  # isinstance() 判断对象是否为已知类型
        assert self.detector_index == block_cfg[0][0]

        self.model, self.save = nn.Sequential(*layers), sorted(save)
        self.names = [str(i) for i in range(self.nc)]

        # set stride、anchor for detector
        Detector = self.model[self.detector_index]  # detector
        if isinstance(Detector, Detect):
            s = 128  # 2x min stride
            # for x in self.forward(torch.zeros(1, 3, s, s)):
            #     print (x.shape)
            with torch.no_grad():
                model_out = self.forward(torch.zeros(1, 3, s, s))
                detects, _= model_out
                Detector.stride = torch.tensor([s / x.shape[-2] for x in detects])  # forward
            # print("stride"+str(Detector.stride ))
            Detector.anchors /= Detector.stride.view(-1, 1, 1)  # Set the anchors for the corresponding scale
            check_anchor_order(Detector)
            self.stride = Detector.stride
            # self._initialize_biases()
        initialize_weights(self)

    def forward(self, x):
        cache = []
        out = []
        #det_out = None   # 由于目标检测输出的后处理部分，进行了剔除，因此检测输出实际上是3个输出未concat的结果，所以将输出变为一个列表格式
        det_out = []

        for i, block in enumerate(self.model):
            if block.from_ != -1:
                x = cache[block.from_] if isinstance(block.from_, int) \
                    else [x if j == -1 else cache[j] for j in
                          block.from_]  # calculate concat detect
            x = block(x)
            if i in self.seg_out_idx:  # save driving area segment result
                # m = nn.Sigmoid()
                # out.append(m(x))
                out.append(torch.sigmoid(x)) #正确的区域 、车道线预测输出
                # out.append(torch.relu(x)) # 我的测试区域、车道线预测输出
            if i == self.detector_index:

                # det_out = x
                if self.training:
                    det_out = x
                else:
                    # det_out = x[0]  # (torch.cat(z, 1), input_feat) if test
                    for det_out_list_index in range(len(x)): # 由于目标检测输出的后处理部分，进行了剔除，因此检测输出实际上是3个输出未concat的结果，所以将输出变为一个列表格式
                        det_out.append(x[det_out_list_index])
            cache.append(x if block.index in self.save else None)
        print('the self.detector_index:',self.detector_index)
        print('len(det_out):',len(det_out))
        return det_out, out[0]  # det, da, ll
        # return det_out, out[0], out[1]  # det, da, ll
        # (1,na*ny*nx*nl,no=2+2+1+nc=xy+wh+obj_conf+cls_prob), (1,2,h,w) (1,2,h,w)


    def _initialize_biases(self, cf=None):  # initialize biases into Detect(), cf is class frequency
        # https://arxiv.org/abs/1708.02002 section 3.3
        # cf = torch.bincount(torch.tensor(np.concatenate(dataset.labels, 0)[:, 0]).long(), minlength=nc) + 1.
        # m = self.model[-1]  # Detect() module
        m = self.model[self.detector_index]  # Detect() module
        for mi, s in zip(m.m, m.stride):  # from
            b = mi.bias.view(m.na, -1)  # conv.bias(255) to (3,85)
            b[:, 4] += math.log(8 / (640 / s) ** 2)  # obj (8 objects per 640 image)
            b[:, 5:] += math.log(0.6 / (m.nc - 0.99)) if cf is None else torch.log(cf / cf.sum())  # cls
            mi.bias = torch.nn.Parameter(b.view(-1), requires_grad=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--height', type=int, default=640)  # height
    parser.add_argument('--width', type=int, default=640)  # width
    args = parser.parse_args()

    do_simplify = True

    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    # model = MCnet(YOLOP)
    model = MCnet(YOLOP_MY2)
    # checkpoint = torch.load('./weights/End-to-end.pth', map_location=device)
    checkpoint = torch.load('./weights/final_state.pth', map_location=device)
    # model.load_state_dict(checkpoint['state_dict'])
    model.load_state_dict(checkpoint)
    model.eval()

    height = args.height
    width = args.width
    print("Load ./weights/End-to-end.pth done!")
    onnx_path = f'./weights/yolop-{height}-{width}.onnx'
    inputs = torch.randn(1, 3, height, width)

    print(f"Converting to {onnx_path}")
    torch.onnx.export(model, inputs, onnx_path,
                      verbose=False, opset_version=12, input_names=['images'],
                      # output_names=['det_out80','det_out40','det_out20', 'drive_area_seg', 'lane_line_seg']
                      output_names=['det_out80', 'det_out40', 'det_out20',  'lane_line_seg']
                      )
    print('convert', onnx_path, 'to onnx finish!!!')
    # Checks
    model_onnx = onnx.load(onnx_path)  # load onnx model
    onnx.checker.check_model(model_onnx)  # check onnx model
    print(onnx.helper.printable_graph(model_onnx.graph))  # print

    if do_simplify:
        print(f'simplifying with onnx-simplifier {onnxsim.__version__}...')
        model_onnx, check = onnxsim.simplify(model_onnx, check_n=3)
        assert check, 'assert check failed'
        onnx.save(model_onnx, onnx_path)

    x = inputs.cpu().numpy()
    try:
        sess = ort.InferenceSession(onnx_path)

        for ii in sess.get_inputs():
            print("Input: ", ii)
        for oo in sess.get_outputs():
            print("Output: ", oo)

        print('read onnx using onnxruntime sucess')
    except Exception as e:
        print('read failed')
        raise e

    """
    PYTHONPATH=. python3 ./export_onnx.py --height 640 --width 640
    PYTHONPATH=. python3 ./export_onnx.py --height 1280 --width 1280
    PYTHONPATH=. python3 ./export_onnx.py --height 320 --width 320
    """
