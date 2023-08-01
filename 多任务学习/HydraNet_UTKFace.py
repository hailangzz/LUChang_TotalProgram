import torch
import numpy as np
from torch.utils.data import Dataset, DataLoader
from torchvision import transforms, utils
import os
import cv2 as cv

import torch
from torch.utils.data import DataLoader



from collections import OrderedDict
from PIL import Image
from torchvision import models
import torch
import torch.nn as nn
import tqdm

BATCH_SIZE = 4
n_epochs=100
device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

class UTKFace(Dataset):
    def __init__(self, image_paths):
        self.transform = transforms.Compose([transforms.Resize((32, 32)), transforms.ToTensor(),
                                             transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])])
        self.image_paths = os.listdir(image_paths)
        # print(self.image_paths)
        self.images = []
        self.ages = []
        self.genders = []
        self.races = []

        for path in self.image_paths:
            # filename = path[8:].split("_")
            filename = path.split("_")
            if len(filename) == 4:
                self.images.append(os.path.join(image_paths,path))
                self.ages.append(int(filename[0]))
                self.genders.append(int(filename[1]))
                self.races.append(int(filename[2]))

    def __len__(self):
        return len(self.images)

    def __getitem__(self, index):
        img = Image.open(self.images[index]).convert('RGB')
        img = self.transform(img)

        age = self.ages[index]
        gender = self.genders[index]
        eth = self.races[index]

        sample = {'image': img, 'age': age, 'gender': gender, 'ethnicity': eth}

        return sample


# train_dataloader = DataLoader(UTKFace(r"F:\AiTotalDatabase\UTKFace\train"), shuffle=True, batch_size=BATCH_SIZE)
# val_dataloader = DataLoader(UTKFace(r"F:\AiTotalDatabase\UTKFace\val"), shuffle=False, batch_size=BATCH_SIZE)



class HydraNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.net = models.resnet18(pretrained=True)
        self.n_features = self.net.fc.in_features
        self.net.fc = nn.Identity()

        self.net.fc1 = nn.Sequential(OrderedDict(
            [('linear', nn.Linear(self.n_features, self.n_features)),
             ('relu1', nn.ReLU()),
             ('final', nn.Linear(self.n_features, 1))]))

        self.net.fc2 = nn.Sequential(OrderedDict(
            [('linear', nn.Linear(self.n_features, self.n_features)),
             ('relu1', nn.ReLU()),
             ('final', nn.Linear(self.n_features, 1))]))

        self.net.fc3 = nn.Sequential(OrderedDict(
            [('linear', nn.Linear(self.n_features, self.n_features)),
             ('relu1', nn.ReLU()),
             ('final', nn.Linear(self.n_features, 5))]))

    def forward(self, x):
        age_head = self.net.fc1(self.net(x))
        gender_head = self.net.fc2(self.net(x))
        ethnicity_head = self.net.fc3(self.net(x))
        return age_head, gender_head, ethnicity_head


model = HydraNet().to(device=device)
print(model)
ethnicity_loss = nn.CrossEntropyLoss()
gender_loss = nn.BCELoss()
age_loss = nn.L1Loss()
sig = nn.Sigmoid()

optimizer = torch.optim.SGD(model.parameters(), lr=1e-4, momentum=0.09)
train_dataloader = DataLoader(UTKFace(r"F:\AiTotalDatabase\UTKFace\train"), shuffle=True, batch_size=BATCH_SIZE)

for epoch in range(n_epochs):
    model.train()
    total_training_loss = 0

    load_batch_num=0
    for i, data in enumerate(train_dataloader):
        inputs = data["image"].to(device=device)

        age_label = data["age"].to(device=device)
        gender_label = data["gender"].to(device=device)
        eth_label = data["ethnicity"].to(device=device)

        optimizer.zero_grad()
        age_output, gender_output, eth_output = model(inputs)

        loss_1 = ethnicity_loss(eth_output, eth_label)
        loss_2 = gender_loss(sig(gender_output), gender_label.unsqueeze(1).float())
        loss_3 = age_loss(age_output, age_label.unsqueeze(1).float())

        loss = loss_1 + loss_2 + loss_3
        loss.backward()
        optimizer.step()

        total_training_loss += loss
        load_batch_num+=1
        print(total_training_loss/load_batch_num)