#include "ZXWAlgApi.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <ADAS_interface.h>

//模型名称
char ModelList[10][256] = {"/system/app/model/yolov5s_dms16_384.npumodel","/system/app/model/eye16.npumodel", "/system/app/model/yolop-det-ll-352-640.npumodel"};

static int frame_image_number=0;
static int frame_index=0;

double GetCurrentTime(void) 
{
    struct timeval tm_tval;
    int ret = gettimeofday(&tm_tval, NULL);
    if (ret == -1) {
        perror("get time bad\n");
    }
    double usec = tm_tval.tv_usec;

    return double(tm_tval.tv_sec) + usec / 1000000;
}

int INIT_ADAS_MODEL()
  {
    
    std::cout << "into INIT_ADAS_MODEL program!"<< std::endl;
    GetModelPath(ModelList);
    ZXWSystemInit();
    ZXW_ADAS_Init();
    
    std::cout << "init ADAS Model finish!!" << std::endl;
    return 1;
  }

int UNINIT_ADAS_MODEL()
  {
  
    ZXW_ADAS_UnInit();
    ZXWSystemUnInit();
    std::cout << "Uninit ADAS Model!!" << std::endl;
    return 0;
  }

//#define DMS 1  //当编译DMS模型时
#define DMS 0  //当编译ADAS模型时


int ADAS_infer2(const ImageData2_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult)
{
    ADAS_param->speed_kmh = 70;                  //70km/h        车辆实时速度  用于计算车距过近 
    ADAS_param->lane_threshold = 2.0;         // 2.0-3.0 车道偏离  越小越敏感  可本地视频测试
    ADAS_param->hit_threshold = 3.0;            //车辆碰撞  越大越敏感  不建议本地视频测试 需要实际路上测试
    ADAS_param->tooclose_threshold = 0.4;    //车距过近  越大越敏感   可本地视频测试
    
    int width=data->image_width;  // 图像宽度
    int height=data->image_height; // 图像高度
    
    // 获取程序开始时间点
    auto start = std::chrono::high_resolution_clock::now();
    
    // 创建一个 cv::Mat 对象，将指针数据转换为图像
    static cv::Mat image(height, width, CV_8UC3); // 创建一个 3 通道的图像对象
    // 将数据复制到图像中
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // 计算当前像素在内存中的索引
            int index = y * width + x;
    
            // 从指针数据中获取 R、G、B 通道的像素值
            unsigned char  r = data->r_ChannelData[index];
            unsigned char  g = data->g_ChannelData[index];
            unsigned char  b = data->b_ChannelData[index];
    
            // 设置图像的像素值
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
        }
    }
    // 获取程序结束时间点
    auto end = std::chrono::high_resolution_clock::now();
    // 计算程序运行时间
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // 输出运行时间
    std::cout << "rgb指针转Mat花费时间: " << duration << " 毫秒" << std::endl;
    
    u_char *virt_base_addr;                        //暂时不支持yuv输入， 后续支持

    
    
    //std::ofstream outputFile("DMS_Result.txt"); // 创建一个名为example.txt的文本文件

        
    ZXW_ADAS_Detect(image, virt_base_addr,*ADAS_param, YolopDetectResult, LaneImg,ADASResult);

    

    return 0;

}


int number = 0;

int ADAS_infer(const ImageData_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult)
{
    
    ADAS_param->speed_kmh = 70;                  //70km/h        车辆实时速度  用于计算车距过近 
    ADAS_param->lane_threshold = 2.0;         // 2.0-3.0 车道偏离  越小越敏感  可本地视频测试
    ADAS_param->hit_threshold = 3.0;            //车辆碰撞  越大越敏感  不建议本地视频测试 需要实际路上测试
    ADAS_param->tooclose_threshold = 0.4;    //车距过近  越大越敏感   可本地视频测试
    
    if (frame_index%4==0) //如果帧数能整除12则进入预测操作····
      {
      try
          {
      
      //std::cout << "the main program into DMS_infer!!!"<< std::endl;
      //std::cout << std::to_string(data->image_width)<< std::endl;
      int width=data->image_width;  // 图像宽度
      int height=data->image_height; // 图像高度
      
      // 获取程序开始时间点
      // auto start = std::chrono::high_resolution_clock::now();
      
      cv::Mat image(height, width, CV_8UC3, data->rgb_data);
      //cv::Mat image(384, 384, CV_8UC3, data->rgb_data);
      
      // 输出图像尺寸信息
      std::cout << "ADAS Image width: " << image.cols << " pixels" << std::endl;
      std::cout << "ADAS Image height: " << image.rows << " pixels" << std::endl;

      //尺寸转换
      //cv::Mat resizedImage;
      //cv::resize(image, resizedImage, cv::Size(640, 352));
      
      //
      // 保存图像
      cv::imwrite("/tmp/"+std::to_string(number) + ".jpg", image);
  
      std::cout << "adas 图像已保存至：/tmp/"+std::to_string(number) + ".jpg"<< std::endl;
      number++;
      //std::cout << "the program will enter ZXW_DMS_Detect() !"<< std::endl;
      u_char *virt_base_addr;                        //暂时不支持yuv输入， 后续支持
      ZXW_ADAS_Detect(image, virt_base_addr,*ADAS_param, YolopDetectResult, LaneImg,ADASResult);
      
      if(ADASResult->event_lanechange == 1){
            printf("**********************************************event_lanechange\n");
            cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_ADAS_event_lanechange" +".jpg", image);
        }
        if(ADASResult->event_tooclose == 1){
            printf("**********************************************event_tooclose\n");
            cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_ADAS_event_tooclose" +".jpg", image);
        }
        if(ADASResult->event_hit == 1){
            printf("**********************************************event_hit\n");
            cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_ADAS_event_hit" +".jpg", image);
        }
        
      //保存分割图片
      //cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_lane_line" +".jpg", cv::InputArray(LaneImg));
      cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_lane_line" +".jpg", *LaneImg);
      
      //std::cout << "ZXW_DMS_Detect() is finished !"<< std::endl;
      /*
      
      // 获取程序结束时间点
      // auto end = std::chrono::high_resolution_clock::now();
      // 计算程序运行时间
      // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      // 输出运行时间
      // std::cout << "图片转rgb指针花费时间: " << duration << " 毫秒" << std::endl;
      
      u_char *virt_base_addr;                        //暂时不支持yuv输入， 后续支持
  
      
      
      //std::ofstream outputFile("DMS_Result.txt"); // 创建一个名为example.txt的文本文件
  
      //DMSDetectResult DMSresult;                    //DMS识别结果
      //zxw_object_result Detectresult[20];      //检测的框
      //memset(Detectresult, 0, sizeof(zxw_object_result)*20);
      // 获取程序开始时间点
      auto start = std::chrono::high_resolution_clock::now();
      ZXW_DMS_Detect(image, virt_base_addr, YoloDetectResult, dms_detect_result);
      // 获取程序结束时间点
      auto end = std::chrono::high_resolution_clock::now();
  
      // 计算程序运行时间
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
      // 输出运行时间
      std::cout << "ZXW_DMS_Detect运行时间: " << duration << " 毫秒" << std::endl;
      
      */
          }catch (const std::exception& e){
              // 捕获并处理异常
              std::cerr << "Exception caught: " << e.what() << std::endl;
          }
      }
    else
      {
      ADASResult->event_lanechange=0;
      ADASResult->event_tooclose=0;
      ADASResult->event_hit = 0;
       
      
      /*
      std::cout << "dms_detect_result->event_calling: " << dms_detect_result->event_calling << std::endl;
      std::cout << "dms_detect_result->event_eyeclose: " << dms_detect_result->event_eyeclose << std::endl;
      std::cout << "dms_detect_result->event_glass: " << dms_detect_result->event_glass << std::endl;
      std::cout << "dms_detect_result->event_mask: " << dms_detect_result->event_mask << std::endl;
      std::cout << "dms_detect_result->event_smoke: " << dms_detect_result->event_smoke << std::endl;
      std::cout << "dms_detect_result->event_yawn: " << dms_detect_result->event_yawn << std::endl;
      */
      }
    
    frame_index+=1;
    
    return 0;
}

