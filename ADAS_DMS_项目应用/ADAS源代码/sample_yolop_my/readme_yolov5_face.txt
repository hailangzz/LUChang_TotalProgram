1. 简介
    使用应用SDK接口写的sample_yolov5_face代码，采用双线程方式和select方式，实现人脸检测功能。
    本示例中使用到的model为Yolov5s网络转换好的npumodel。

2. 使用说明
(1)  进入VC0768_SDK/source/chip/vc0768/external/mpp/npu_samples/sample_yolov5_face/ 目录，
      执行sh mk_arm.sh命令进行交叉编译，编译后得到嵌入式可执行文件：sample_yolov5。
     *此步骤在linux系统中进行。
(2)  将可执行文件sample_yolov5和当前文件夹下的images/和model/文件夹拷贝到板子上的同一个目录里。
(3)  进入板子里对应的目录，执行 ./sample_yolov5 -help 查看使用说明，num=<int>代表要运行的图片数量，
      用户可以根据自己需要进行更改，默认num=1，如果输入的数字大于文件中已有的数据量时按照实际数据量执行。
      执行 ./sample_yolov5 num=1，得到的运行结果保存在板子上当前目录下的result/ 文件夹下。
(4)  用户可以将板子上当前目录下的result/文件夹和VC0768_SDK/source/chip/vc0768/external/mpp/npu_samples/sample_yolov5_face/ 
      目录下的resource/文件夹、yolov5-face.html文件拷贝到windows系统下的某个目录，
      双击yolov5-face.html，则可看到网页版展示效果。推荐使用Microsoft Edge浏览器。