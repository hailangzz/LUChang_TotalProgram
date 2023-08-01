import torch
import numpy as np
from torch.utils.data import Dataset, DataLoader
from torchvision import transforms, utils
import os
import cv2 as cv


class AgeGenderDataset(Dataset):
    def __init__(self, root_dir):
        # Normalize: image => [-1, 1]  （利于更好的训练）
        # ToTensor() => Converts a PIL Image or numpy.ndarray (H x W x C) in the range [0, 255] to a torch.FloatTensor of shape (C x H x W) in the range [0.0, 1.0]
        self.transform = transforms.Compose([transforms.ToTensor(),
                                             transforms.Normalize(mean=[0.5, 0.5, 0.5],
                                                                  std=[0.5, 0.5, 0.5]),
                                             transforms.Resize((64, 64))
                                             ])

        img_files = os.listdir(root_dir)  # 存放的是所有图片的文件名
        self.max_age = 116
        # age: 0 ~116, 0 :male, 1 :female
        self.ages = []
        self.genders = []
        # 注意：self.image存放的是图片的路径
        self.images = []

        for file_name in img_files:
            age_gender_group = file_name.split("_")
            age_ = age_gender_group[0]
            gender_ = age_gender_group[1]
            self.genders.append(np.float32(gender_))
            # 将age缩小到[0, 1]的范围内
            self.ages.append(np.float32(age_) / self.max_age)
            # os.path.join() 将路径和文件名合成为一个路径
            self.images.append(os.path.join(root_dir, file_name))

    def __len__(self):
        return len(self.images)

    def __getitem__(self, idx):
        if torch.is_tensor(idx):
            idx = idx.tolist()
            image_path = self.images[idx]
        else:
            image_path = self.images[idx]

        img = cv.imread(image_path)  # BGR order

        sample = {'image': self.transform(img), 'age': self.ages[idx], 'gender': self.genders[idx]}

        # 返回一个字典形式
        return sample


if __name__ == "__main__":
    ds = AgeGenderDataset(r"F:\AiTotalDatabase\UTKFace")
    for i in range(len(ds)):
        sample = ds[i]
        print(i, sample['image'].size(), sample['age'])
        # 提取一个batch的数据
        if i == 3:
            break

    dataloader = DataLoader(ds, batch_size=4, shuffle=True, num_workers=4)

    # enumerate将可迭代对象组合为索引序列  例如：[(0, 'Tom'), (1, 'Jerry')]
    for i_batch, sample_batched in enumerate(dataloader):
        print(i_batch, sample_batched['image'].size(), sample_batched['gender'])
        break
