#ifndef ADAS_PROGRAM_H
#define ADAS_PROGRAM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "npu_comm.h"
#include "npu_errno.h"
#include "npu_mpi.h"
#include "vim_type.h"

#include<string.h>
#include<iostream>
#include <vector>
#include<dirent.h>   
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

#include <fstream>

// 检测框对象数组置信度排序
void QsortDescentInplace(std::vector<ObjectDetectInfo> &objects, int left, int right);
void QsortDescentInplace(std::vector<ObjectDetectInfo> &objects);

//目标框IOU计算
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1, float ymax1);

//目标框非极大值抑制函数
void DetectNms(std::vector<ObjectDetectInfo> &objects, std::vector<size_t> &picked, float nms_confidence)

// sigmoid激活函数
static float Sigmoid(float x);

// 读取本都图片数据
static int ReadImageFile(cv::Mat imagein, unsigned char* img, int format, int w, int h);

// 获取给定路径下，文件路径数组
int GetAbsoluteFiles(string directory, vector<string>& filesAbsolutePath); //参数1[in]要变量的目录  参数2[out]存储文件名

// 在输入图片上绘制车辆检测框、车道线等信息
void PlotObjectBoxLaneMask(cv::Mat BaseImage, cv::Mat MarkImage, std::vector<ObjectDetectInfo> objects,cv::Mat &segmentedImage);

// FP32模型输出后处理函数
static void GenerateProposals32(VIM_F32* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale);
// FP16模型输出后处理函数
static void GenerateProposals16(VIM_S16* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale);
// Uint8模型输出后处理函数
static void GenerateProposals8(VIM_U8* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale);

//车辆目标结构体
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int class_label;
    float confidence;
} ObjectDetectInfo;

//多线程结构体
typedef struct S_NetIdINFO_S
{
    VIM_U64    u64NId ;
    pthread_t  pthreadset;
    pthread_t  pthreadget;
    VIM_S32 set_thread_exit;
	VIM_S32 get_thread_exit;
    
}S_NetIdINFO_S;

//网络输出层信息结构体
typedef struct {
	int width,height,channel,number;
	void* data;
    float scale;
} NetOutBlobInfo;


//Yolop网络类
class YolopNet {
    public:
        std::vector<NetOutBlobInfo> net_out_blob;
		

   public:
        YolopNet();
        ~YolopNet();
        VIM_S32 YolopLoadModel(const char *model_path);
        VIM_S32 YolopInput(unsigned char* data, int width, int height, int channel, const char *inputnode);		
		VIM_S32 YolopInput(cv::Mat img, int width, int height, int channel, const char *inputnode);
        VIM_S32 YolopInference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale);
		VIM_S32 YolopGetResult(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<ObjectDetectInfo> &objects,cv::Mat &mark_images);

   private:
       S_NetIdINFO_S  g_stNetId;
       NPU_INFO_S g_stInfo;
       VIM_F32 *pLocAddr_F32[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       VIM_S16 *pLocAddr_S16[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       VIM_U8 *pLocAddr_U8[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       int gOutputNodeNumber;
       int scale_flag;
       int gBits;

};


// 目标检测框anchors定义
const std::vector<std::vector<float>> anchors = {
	{19,50,38,81,68,157}, 
	{7,18,6,39,12,31}, 
	{3,9,5,11,4,20}
	};

//模型参数信息结构体
struct S_MODELPARAM {
    std::string modelpath = "./yolop-384-640_8bit.npumodel"; // modelpath  yolop模型路径
    int width = 640;              // width 图像宽度
    int height = 384;             // height 图像高度
    std::string input_node = "input0"; // input_node 输入节点名称
    std::vector<std::string> output_node = {
		"Sigmoid140",
		"Sigmoid142",
		"Sigmoid144",
		"Sigmoid178"
		}; // output_node 网络输出节点名称数组
    int format = 0;             // format 模型量化类型
    int with_nms = 0;           // with_nms 是否启用NMS
    int nms_load_flag = 0;      // nms_load_flag 初始化为 0
    float thresh = 0.0f;        // thresh 初始化为 0.0
};


#endif //ADAS_PROGRAM_H