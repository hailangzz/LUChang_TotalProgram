import os

import numpy as np
import torch
import torch.backends.cudnn as cudnn
import torch.distributed as dist
import torch.optim as optim
from torch.utils.data import DataLoader

from nets.arcface import Arcface
from nets.arcface_training import get_lr_scheduler, set_optimizer_lr
from utils.callback import LossHistory
from utils.dataloader import FacenetDataset, LFWDataset, dataset_collate
from utils.utils import get_num_classes, show_config
from utils.utils_fit import fit_one_epoch

if __name__ == "__main__":
    #-------------------------------#
    #   �Ƿ�ʹ��Cuda
    #   û��GPU�������ó�False
    #-------------------------------#
    Cuda            = True
    #---------------------------------------------------------------------#
    #   distributed     ����ָ���Ƿ�ʹ�õ����࿨�ֲ�ʽ����
    #                   �ն�ָ���֧��Ubuntu��CUDA_VISIBLE_DEVICES������Ubuntu��ָ���Կ���
    #                   Windowsϵͳ��Ĭ��ʹ��DPģʽ���������Կ�����֧��DDP��
    #   DPģʽ��
    #       ����            distributed = False
    #       ���ն�������    CUDA_VISIBLE_DEVICES=0,1 python train.py
    #   DDPģʽ��
    #       ����            distributed = True
    #       ���ն�������    CUDA_VISIBLE_DEVICES=0,1 python -m torch.distributed.launch --nproc_per_node=2 train.py
    #---------------------------------------------------------------------#
    distributed     = False
    #---------------------------------------------------------------------#
    #   sync_bn     �Ƿ�ʹ��sync_bn��DDPģʽ�࿨����
    #---------------------------------------------------------------------#
    sync_bn         = False
    #---------------------------------------------------------------------#
    #   fp16        �Ƿ�ʹ�û�Ͼ���ѵ��
    #               �ɼ���Լһ����Դ桢��Ҫpytorch1.7.1����
    #---------------------------------------------------------------------#
    fp16            = False
    #--------------------------------------------------------#
    #   ָ���Ŀ¼�µ�cls_train.txt����ȡ����·�����ǩ
    #--------------------------------------------------------#
    annotation_path = "cls_train.txt"
    #--------------------------------------------------------#
    #   ����ͼ���С
    #--------------------------------------------------------#
    input_shape     = [112, 112, 3]
    #--------------------------------------------------------#
    #   ����������ȡ�����ѡ��
    #   mobilefacenet
    #   mobilenetv1
    #   iresnet18
    #   iresnet34
    #   iresnet50
    #   iresnet100
    #   iresnet200
    #
    #   ����mobilenetv1�⣬������backbone���ɴ�0��ʼѵ����
    #   ��������mobilenetv1û�вв�ߣ������ٶ�������˽��飺
    #   ���ʹ��mobilenetv1Ϊ����,  ������pretrain = True
    #   ���ʹ����������Ϊ���ɣ�    ������pretrain = False
    #--------------------------------------------------------#
    backbone        = "mobilefacenet"
    #----------------------------------------------------------------------------------------------------------------------------#
    #   ���ѵ�������д����ж�ѵ���Ĳ��������Խ�model_path���ó�logs�ļ����µ�Ȩֵ�ļ������Ѿ�ѵ����һ���ֵ�Ȩֵ�ٴ����롣
    #   ͬʱ�޸��·���ѵ���Ĳ���������֤ģ��epoch�������ԡ�
    #   
    #   ��model_path = ''��ʱ�򲻼�������ģ�͵�Ȩֵ��
    #
    #   �˴�ʹ�õ�������ģ�͵�Ȩ�أ��������train.py���м��صģ�pretrain��Ӱ��˴���Ȩֵ���ء�
    #   �����Ҫ��ģ�ʹ����ɵ�Ԥѵ��Ȩֵ��ʼѵ����������model_path = ''��pretrain = True����ʱ���������ɡ�
    #   �����Ҫ��ģ�ʹ�0��ʼѵ����������model_path = ''��pretrain = Fasle����ʱ��0��ʼѵ����
    #----------------------------------------------------------------------------------------------------------------------------#  
    model_path      = ""
    #----------------------------------------------------------------------------------------------------------------------------#
    #   �Ƿ�ʹ�����������Ԥѵ��Ȩ�أ��˴�ʹ�õ������ɵ�Ȩ�أ��������ģ�͹�����ʱ����м��صġ�
    #   ���������model_path�������ɵ�Ȩֵ������أ�pretrained��ֵ�����塣
    #   ���������model_path��pretrained = True����ʱ���������ɿ�ʼѵ����
    #   ���������model_path��pretrained = False����ʱ��0��ʼѵ����
    #   ����mobilenetv1�⣬������backbone��δ�ṩԤѵ��Ȩ�ء�
    #----------------------------------------------------------------------------------------------------------------------------#
    pretrained      = False

    #----------------------------------------------------------------------------------------------------------------------------#
    #   �Դ治�������ݼ���С�޹أ���ʾ�Դ治�����Сbatch_size��
    #   �ܵ�BatchNorm��Ӱ�죬����Ϊ1��
    #
    #   �ڴ��ṩ���ɲ������ý��飬��λѵ���߸����Լ������������������
    #   ��һ����Ԥѵ��Ȩ�ؿ�ʼѵ����
    #       Adam��
    #           Init_Epoch = 0��Epoch = 100��optimizer_type = 'adam'��Init_lr = 1e-3��weight_decay = 0��
    #       SGD��
    #           Init_Epoch = 0��Epoch = 100��optimizer_type = 'sgd'��Init_lr = 1e-2��weight_decay = 5e-4��
    #       ���У�UnFreeze_Epoch������100-300֮�������
    #   ������batch_size�����ã�
    #       ���Կ��ܹ����ܵķ�Χ�ڣ��Դ�Ϊ�á��Դ治�������ݼ���С�޹أ���ʾ�Դ治�㣨OOM����CUDA out of memory�����Сbatch_size��
    #       �ܵ�BatchNorm��Ӱ�죬batch_size��СΪ2������Ϊ1��
    #       ���������Freeze_batch_size����ΪUnfreeze_batch_size��1-2�������������õĲ�������Ϊ��ϵ��ѧϰ�ʵ��Զ�������
    #----------------------------------------------------------------------------------------------------------------------------#
    #------------------------------------------------------#
    #   ѵ������
    #   Init_Epoch      ģ�͵�ǰ��ʼ��ѵ������
    #   Epoch           ģ���ܹ�ѵ����epoch
    #   batch_size      ÿ�������ͼƬ����
    #------------------------------------------------------#
    Init_Epoch      = 0
    Epoch           = 10
    # batch_size      = 64
    batch_size = 2
    #------------------------------------------------------------------#
    #   ����ѵ��������ѧϰ�ʡ��Ż�����ѧϰ���½��й�
    #------------------------------------------------------------------#
    #------------------------------------------------------------------#
    #   Init_lr         ģ�͵����ѧϰ��
    #   Min_lr          ģ�͵���Сѧϰ�ʣ�Ĭ��Ϊ���ѧϰ�ʵ�0.01
    #------------------------------------------------------------------#
    Init_lr             = 1e-2
    Min_lr              = Init_lr * 0.01
    #------------------------------------------------------------------#
    #   optimizer_type  ʹ�õ����Ż������࣬��ѡ����adam��sgd
    #                   ��ʹ��Adam�Ż���ʱ��������  Init_lr=1e-3
    #                   ��ʹ��SGD�Ż���ʱ��������   Init_lr=1e-2
    #   momentum        �Ż����ڲ�ʹ�õ���momentum����
    #   weight_decay    Ȩֵ˥�����ɷ�ֹ�����
    #                   adam�ᵼ��weight_decay����ʹ��adamʱ��������Ϊ0��
    #------------------------------------------------------------------#
    optimizer_type      = "sgd"
    momentum            = 0.9
    weight_decay        = 5e-4
    #------------------------------------------------------------------#
    #   lr_decay_type   ʹ�õ���ѧϰ���½���ʽ����ѡ����step��cos
    #------------------------------------------------------------------#
    lr_decay_type       = "cos"
    #------------------------------------------------------------------#
    #   save_period     ���ٸ�epoch����һ��Ȩֵ��Ĭ��ÿ������������
    #------------------------------------------------------------------#
    save_period         = 1
    #------------------------------------------------------------------#
    #   save_dir        Ȩֵ����־�ļ�������ļ���
    #------------------------------------------------------------------#
    save_dir            = 'logs'
    #------------------------------------------------------------------#
    #   ���������Ƿ�ʹ�ö��̶߳�ȡ����
    #   �������ӿ����ݶ�ȡ�ٶȣ����ǻ�ռ�ø����ڴ�
    #   �ڴ��С�ĵ��Կ�������Ϊ2����0  
    #------------------------------------------------------------------#
    num_workers     = 4
    #------------------------------------------------------------------#
    #   �Ƿ���LFW����
    #------------------------------------------------------------------#
    lfw_eval_flag   = True
    #------------------------------------------------------------------#
    #   LFW�������ݼ����ļ�·���Ͷ�Ӧ��txt�ļ�
    #------------------------------------------------------------------#
    lfw_dir_path    = "lfw"
    lfw_pairs_path  = "model_data/lfw_pair.txt"

    #------------------------------------------------------#
    #   �����õ����Կ�
    #------------------------------------------------------#
    ngpus_per_node  = torch.cuda.device_count()
    if distributed:
        dist.init_process_group(backend="nccl")
        local_rank  = int(os.environ["LOCAL_RANK"])
        rank        = int(os.environ["RANK"])
        device      = torch.device("cuda", local_rank)
        if local_rank == 0:
            print(f"[{os.getpid()}] (rank = {rank}, local_rank = {local_rank}) training...")
            print("Gpu Device Count : ", ngpus_per_node)
    else:
        device          = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        local_rank      = 0
        rank            = 0

    num_classes = get_num_classes(annotation_path)
    #---------------------------------#
    #   ����ģ�Ͳ�����Ԥѵ��Ȩ��
    #---------------------------------#
    model = Arcface(num_classes=num_classes, backbone=backbone, pretrained=pretrained)

    if model_path != '':
        #------------------------------------------------------#
        #   Ȩֵ�ļ��뿴README���ٶ���������
        #------------------------------------------------------#
        if local_rank == 0:
            print('Load weights {}.'.format(model_path))
        
        #------------------------------------------------------#
        #   ����Ԥѵ��Ȩ�ص�Key��ģ�͵�Key���м���
        #------------------------------------------------------#
        model_dict      = model.state_dict()
        pretrained_dict = torch.load(model_path, map_location = device)
        load_key, no_load_key, temp_dict = [], [], {}
        for k, v in pretrained_dict.items():
            if k in model_dict.keys() and np.shape(model_dict[k]) == np.shape(v):
                temp_dict[k] = v
                load_key.append(k)
            else:
                no_load_key.append(k)
        model_dict.update(temp_dict)
        model.load_state_dict(model_dict)
        #------------------------------------------------------#
        #   ��ʾû��ƥ���ϵ�Key
        #------------------------------------------------------#
        if local_rank == 0:
            print("\nSuccessful Load Key:", str(load_key)[:500], "����\nSuccessful Load Key Num:", len(load_key))
            print("\nFail To Load Key:", str(no_load_key)[:500], "����\nFail To Load Key num:", len(no_load_key))
            print("\n\033[1;33;44m��ܰ��ʾ��head����û����������������Backbone����û�������Ǵ���ġ�\033[0m")

    #----------------------#
    #   ��¼Loss
    #----------------------#
    if local_rank == 0:
        loss_history = LossHistory(save_dir, model, input_shape=input_shape)
    else:
        loss_history = None
        
    #------------------------------------------------------------------#
    #   torch 1.2��֧��amp������ʹ��torch 1.7.1��������ȷʹ��fp16
    #   ���torch1.2������ʾ"could not be resolve"
    #------------------------------------------------------------------#
    if fp16:
        from torch.cuda.amp import GradScaler as GradScaler
        scaler = GradScaler()
    else:
        scaler = None

    model_train     = model.train()
    #----------------------------#
    #   �࿨ͬ��Bn
    #----------------------------#
    if sync_bn and ngpus_per_node > 1 and distributed:
        model_train = torch.nn.SyncBatchNorm.convert_sync_batchnorm(model_train)
    elif sync_bn:
        print("Sync_bn is not support in one gpu or not distributed.")

    if Cuda:
        if distributed:
            #----------------------------#
            #   �࿨ƽ������
            #----------------------------#
            model_train = model_train.cuda(local_rank)
            model_train = torch.nn.parallel.DistributedDataParallel(model_train, device_ids=[local_rank], find_unused_parameters=True)
        else:
            model_train = torch.nn.DataParallel(model)
            cudnn.benchmark = True
            model_train = model_train.cuda()

    #---------------------------------#
    #   LFW����
    #---------------------------------#
    LFW_loader = torch.utils.data.DataLoader(
        LFWDataset(dir=lfw_dir_path, pairs_path=lfw_pairs_path, image_size=input_shape), batch_size=32, shuffle=False) if lfw_eval_flag else None

    #-------------------------------------------------------#
    #   0.01������֤��0.99����ѵ��
    #-------------------------------------------------------#
    # val_split = 0.01
    val_split = 0.01
    with open(annotation_path,"r") as f:
        lines = f.readlines()
    np.random.seed(10101)
    np.random.shuffle(lines)
    np.random.seed(None)
    num_val = int(len(lines)*val_split)
    num_train = len(lines) - num_val
    
    show_config(
        num_classes = num_classes, backbone = backbone, model_path = model_path, input_shape = input_shape, \
        Init_Epoch = Init_Epoch, Epoch = Epoch, batch_size = batch_size, \
        Init_lr = Init_lr, Min_lr = Min_lr, optimizer_type = optimizer_type, momentum = momentum, lr_decay_type = lr_decay_type, \
        save_period = save_period, save_dir = save_dir, num_workers = num_workers, num_train = num_train, num_val = num_val
    )

    if True:
        #-------------------------------------------------------------------#
        #   �жϵ�ǰbatch_size������Ӧ����ѧϰ��
        #-------------------------------------------------------------------#
        nbs             = 64
        lr_limit_max    = 1e-3 if optimizer_type == 'adam' else 1e-1
        lr_limit_min    = 3e-4 if optimizer_type == 'adam' else 5e-4
        Init_lr_fit     = min(max(batch_size / nbs * Init_lr, lr_limit_min), lr_limit_max)
        Min_lr_fit      = min(max(batch_size / nbs * Min_lr, lr_limit_min * 1e-2), lr_limit_max * 1e-2)

        #---------------------------------------#
        #   ����optimizer_typeѡ���Ż���
        #---------------------------------------#
        optimizer = {
            'adam'  : optim.Adam(model.parameters(), Init_lr_fit, betas = (momentum, 0.999), weight_decay = weight_decay),
            'sgd'   : optim.SGD(model.parameters(), Init_lr_fit, momentum=momentum, nesterov=True, weight_decay = weight_decay)
        }[optimizer_type]

        #---------------------------------------#
        #   ���ѧϰ���½��Ĺ�ʽ
        #---------------------------------------#
        lr_scheduler_func = get_lr_scheduler(lr_decay_type, Init_lr_fit, Min_lr_fit, Epoch)
        
        #---------------------------------------#
        #   �ж�ÿһ�������ĳ���
        #---------------------------------------#
        epoch_step      = num_train // batch_size
        epoch_step_val  = num_val // batch_size
        
        if epoch_step == 0 or epoch_step_val == 0:
            raise ValueError("���ݼ���С���޷���������ѵ�������������ݼ���")

        #---------------------------------------#
        #   �������ݼ���������
        #---------------------------------------#
        train_dataset   = FacenetDataset(input_shape, lines[:num_train], random = True)
        val_dataset     = FacenetDataset(input_shape, lines[num_train:], random = False)
        
        if distributed:
            train_sampler   = torch.utils.data.distributed.DistributedSampler(train_dataset, shuffle=True,)
            val_sampler     = torch.utils.data.distributed.DistributedSampler(val_dataset, shuffle=False,)
            batch_size      = batch_size // ngpus_per_node
            shuffle         = False
        else:
            train_sampler   = None
            val_sampler     = None
            shuffle         = True

        gen             = DataLoader(train_dataset, shuffle=shuffle, batch_size=batch_size, num_workers=num_workers, pin_memory=True,
                                drop_last=True, collate_fn=dataset_collate, sampler=train_sampler)
        gen_val         = DataLoader(val_dataset, shuffle=shuffle, batch_size=batch_size, num_workers=num_workers, pin_memory=True,
                                drop_last=True, collate_fn=dataset_collate, sampler=val_sampler)

        for epoch in range(Init_Epoch, Epoch):
            if distributed:
                train_sampler.set_epoch(epoch)
                
            set_optimizer_lr(optimizer, lr_scheduler_func, epoch)
            
            fit_one_epoch(model_train, model, loss_history, optimizer, epoch, epoch_step, epoch_step_val, gen, gen_val, Epoch, Cuda, LFW_loader, lfw_eval_flag, fp16, scaler, save_period, save_dir, local_rank)

        if local_rank == 0:
            loss_history.writer.close()
