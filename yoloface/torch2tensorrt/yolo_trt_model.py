import os
import sys
import onnx
import pycuda.autoinit
import pycuda.driver as cuda
import tensorrt as trt
import torch
import time
import torchvision
import numpy as np
import os
import sys
from torch2tensorrt.tensorrt_com import Do_Inference,Init_TensorRT,ONNX_to_TensorRT

class YoloTrtModel():
    '''
    ONNX->TensorRT并推理
    '''
    def __init__(self,device_id="cuda:0",onnx_model_path=None,fp16_mode=False,cls=17,x=640,y=640):
        '''
        device_id: "cuda:0"
        onnx_model_path: 加载onnx模型的路径
        output_size: # 输出尺寸 eg:(1,-1) 
        fp16_mode: True则FP16推理
        cls:类别数
        x，y：横向纵向图片大小
        '''

        trt_engine_path = onnx_model_path.replace('.onnx','.trt')

        # 初始化TensorRT, 加载trt引擎文件
        self.model_params=Init_TensorRT(trt_engine_path)
        self.cls=cls
        # 输出特征
        self.stride8_shape=(1,3,x//8,y//8,13+cls)
        self.stride16_shape=(1,3,x//16,y//16,13+cls)
        self.stride32_shape=(1,3,x//32,y//32,13+cls)

    def __call__(self,img_np_nchw):
        '''
        TensorRT推理
        img_np_nchw: 输入图像 [1,3,640,640]
        '''
        context,inputs, outputs, bindings, stream = self.model_params
        
        # 加载输入数据到buffer
        inputs[0].host = img_np_nchw.reshape(-1) #输入形状转为一维，作为输入
        # inputs[1].host = ... for multiple input  对于多输入情况

        trt_outputs = Do_Inference(context, bindings=bindings, inputs=inputs, outputs=outputs, stream=stream)  # numpy data
        stride_8 = trt_outputs[0].reshape(*self.stride8_shape) # 输出形状由一维转为指定形状
        stride_16 = trt_outputs[1].reshape(*self.stride16_shape) # 输出形状由一维转为指定形状
        stride_32 = trt_outputs[2].reshape(*self.stride32_shape) # 输出形状由一维转为指定形状
        return [stride_8,stride_16,stride_32]
    def after_process(self,pred,device):
        '''
        Pytorch后处理
        pred: tensorrt输出
        device: "cuda:0"
        '''

        # 降8、16、32倍
        stride= torch.tensor([8.,16.,32.]).to(device)
        x=[torch.from_numpy(pred[0]).to(device),torch.from_numpy(pred[1]).to(device),torch.from_numpy(pred[2]).to(device)]
        # =====提取自models/yolo.py=====

        nc=self.cls
        no=self.cls+13 # 4坐标+1置信度+8关键点坐标+17类别
        # nc=17  #17类，如果修改模型需要对这里进行更改
        nl=3
     
        grid=[torch.zeros(1).to(device)] * nl 

        anchor_grid=torch.tensor([[[[[[  4.,   5.]]],  #这个可以改
            [[[  8.,  10.]]],
            [[[ 13.,  16.]]]]],
            [[[[[ 23.,  29.]]],
            [[[ 43.,  55.]]],
            [[[ 73., 105.]]]]],
            [[[[[146., 217.]]],
            [[[231., 300.]]],
            [[[335., 433.]]]]]]).to(device)
       
        
        z = [] 
        for i in range(len(x)):   #x是一个列表，相当于遍历x里的元素
        
            bs,ny, nx = x[i].shape[0],x[i].shape[2] ,x[i].shape[3] 
            if grid[i].shape[2:4] != x[i].shape[2:4]:
                grid[i] = self._make_grid(nx, ny).to(x[i].device)
            y = torch.full_like(x[i], 0)
            class_range = list(range(5)) + list(range(13, 13 + nc))
            y[..., class_range] = x[i][..., class_range].sigmoid()  # 这里是只对关键点以外的值进行sigmoid
            y[..., 5:13] = x[i][..., 5:13]
            #y = x[i].sigmoid()

            y[..., 0:2] = (y[..., 0:2] * 2. - 0.5 + grid[i].to(x[i].device)) * stride[i]  # xy
            y[..., 2:4] = (y[..., 2:4] * 2) ** 2 * anchor_grid[i]  # wh

            #y[..., 5:15] = y[..., 5:15] * 8 - 4
            y[..., 5:7]   = y[..., 5:7] *   anchor_grid[i] + grid[i].to(x[i].device) * stride[i] # landmark x1 y1
            y[..., 7:9]   = y[..., 7:9] *   anchor_grid[i] + grid[i].to(x[i].device) * stride[i]# landmark x2 y2
            y[..., 9:11]  = y[..., 9:11] *  anchor_grid[i] + grid[i].to(x[i].device) * stride[i]# landmark x3 y3
            y[..., 11:13] = y[..., 11:13] * anchor_grid[i] + grid[i].to(x[i].device) * stride[i]# landmark x4 y4
            # y[..., 13:15] = y[..., 13:15] * anchor_grid[i] + grid[i].to(x[i].device) * stride[i]# landmark x5 y5

            #y[..., 5:7] = (y[..., 5:7] * 2 -1) * anchor_grid[i]  # landmark x1 y1
            #y[..., 7:9] = (y[..., 7:9] * 2 -1) * anchor_grid[i]  # landmark x2 y2
            #y[..., 9:11] = (y[..., 9:11] * 2 -1) * anchor_grid[i]  # landmark x3 y3
            #y[..., 11:13] = (y[..., 11:13] * 2 -1) * anchor_grid[i]  # landmark x4 y4
            #y[..., 13:15] = (y[..., 13:15] * 2 -1) * anchor_grid[i]  # landmark x5 y5

            z.append(y.view(bs, -1, no))
        return torch.cat(z, 1)

    def _make_grid(self,nx=20, ny=20):
        yv, xv = torch.meshgrid([torch.arange(ny), torch.arange(nx)])
        return torch.stack((xv, yv), 2).view((1, 1, ny, nx, 2)).float()