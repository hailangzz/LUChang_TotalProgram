// DMS_interface.h

#ifndef DMS_INTERFACE_H
#define DMS_INTERFACE_H
#include <opencv2/opencv.hpp>
#include <ZXWAlgApi.h>

int INIT_DMS_MODEL();
int UNINIT_DMS_MODEL();

struct ImageData
{
    unsigned char* rgb_data;
    int image_width;  // 384
    int image_height; // 384
    int DMS_ADAS_Flag;// 0！！DMS、1！！ADAS
};

struct ImageData2
{
    unsigned char* r_ChannelData;
    unsigned char* g_ChannelData;
    unsigned char* b_ChannelData;
    int image_width;  // 384
    int image_height; // 384
    int DMS_ADAS_Flag;// 0！！DMS、1！！ADAS
    
};



//int DMS_interface(const cv::Mat& inputImage);
int DMS_infer2(const ImageData2* data, zxw_object_result *YoloDetectResult,DMSDetectResult *DMSResult);

int DMS_infer(const ImageData* data, zxw_object_result *YoloDetectResult,DMSDetectResult *DMSResult);

#endif
