// ADAS_interface.h

#ifndef ADAS_INTERFACE_H
#define ADAS_INTERFACE_H
#include <opencv2/opencv.hpp>
#include <ZXWAlgApi.h>

int INIT_ADAS_MODEL();
int UNINIT_ADAS_MODEL();

struct ImageData_ADAS
{
    unsigned char* rgb_data;
    int image_width;  // 640
    int image_height; // 352
    int DMS_ADAS_Flag;// 0！！DMS、1！！ADAS
};

struct ImageData2_ADAS
{
    unsigned char* r_ChannelData;
    unsigned char* g_ChannelData;
    unsigned char* b_ChannelData;
    int image_width;  // 640
    int image_height; // 352
    int DMS_ADAS_Flag;// 0！！DMS、1！！ADAS
    
};



//int ADAS_interface(const cv::Mat& inputImage);
int ADAS_infer2(const ImageData2_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult);

int ADAS_infer(const ImageData_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult);

#endif
