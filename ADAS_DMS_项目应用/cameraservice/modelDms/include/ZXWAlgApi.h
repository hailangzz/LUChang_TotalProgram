#ifndef _ZXWALGAPI_H_
#define _ZXWALGAPI_H_

#include <stdio.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>

#include <math.h>
#include <memory.h>

#include <dirent.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>



#ifdef __cplusplus
extern "C" {
#endif

//DMS事件类型结构体
typedef struct {
    int event_eyeclose;                  //闭眼
    int event_yawn;                         //打哈欠
    int event_glass;                          //戴墨镜
    int event_smoke;                      //抽烟
    int event_calling;                      //打电话
    int event_mask;                        //摄像头遮挡
} DMSDetectResult;

//ADAS事件类型结构体
typedef struct {
    int event_lanechange;            //车道偏离
    int event_tooclose;                  //车距过近
    int event_hit;                              //车辆碰撞
} ADASDetectResult;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int class_label;
    float confidence;
} zxw_object_result;

//ADAS参数配置结构体
typedef struct {
    int speed_kmh;                                                //车辆速度
    float lane_threshold;                                     //车道偏离阈值
    float hit_threshold;                                        //车辆碰撞阈值
    float tooclose_threshold;                            //车距过近阈值

} zxw_adas_Paramaters;


//传入模型名称函数，用法见demo，返回0为成功，返回-1为失败
int GetModelPath(char (*names)[256]);

//NPU硬件初始化， 返回0为成功，返回-1为失败
int ZXWSystemInit();

//DMS初始化，返回0为成功，返回-1为失败
int ZXW_DMS_Init();

/*
DMS检测函数，循环调用，返回0为成功，返回-1为失败
iputimg 为传入图像，为红外图像，即三通道，看似灰度图
YUVdata为yuv数据，暂时不支持该输入，后续会支持
YoloDetectResult 为检测的目标框
DMSResult 为DMS事件结果
*/
int ZXW_DMS_Detect(cv::Mat iputimg, uchar *YUVdata, zxw_object_result *YoloDetectResult, DMSDetectResult *DMSResult);

//DMS反初始化，返回0为成功，返回-1为失败
int ZXW_DMS_UnInit();

//ADAS初始化，返回0为成功，返回-1为失败
int ZXW_ADAS_Init();

/*
ADAS检测函数，循环调用，返回0为成功，返回-1为失败
iputimg 为输入图片，BGR格式
YUVdata为yuv数据，暂时不支持该输入，后续会支持
param 为配置参数，用法见demo
YolopDetectResult 为yolop的车辆检测结果
LaneImg为分割结果
ADASResult为ADAS事件结果
*/
int ZXW_ADAS_Detect(cv::Mat iputimg, uchar *YUVdata, zxw_adas_Paramaters param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult);

//ADAS反初始化，返回0为成功，返回-1为失败
int ZXW_ADAS_UnInit();

//NPU硬件反初始化， 返回0为成功，返回-1为失败
int ZXWSystemUnInit();


#ifdef __cplusplus
}
#endif

#endif
