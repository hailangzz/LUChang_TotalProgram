#include "ZXWAlgApi.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <ADAS_interface.h>

//ģ������
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

//#define DMS 1  //������DMSģ��ʱ
#define DMS 0  //������ADASģ��ʱ


int ADAS_infer2(const ImageData2_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult)
{
    ADAS_param->speed_kmh = 70;                  //70km/h        ����ʵʱ�ٶ�  ���ڼ��㳵����� 
    ADAS_param->lane_threshold = 2.0;         // 2.0-3.0 ����ƫ��  ԽСԽ����  �ɱ�����Ƶ����
    ADAS_param->hit_threshold = 3.0;            //������ײ  Խ��Խ����  �����鱾����Ƶ���� ��Ҫʵ��·�ϲ���
    ADAS_param->tooclose_threshold = 0.4;    //�������  Խ��Խ����   �ɱ�����Ƶ����
    
    int width=data->image_width;  // ͼ����
    int height=data->image_height; // ͼ��߶�
    
    // ��ȡ����ʼʱ���
    auto start = std::chrono::high_resolution_clock::now();
    
    // ����һ�� cv::Mat ���󣬽�ָ������ת��Ϊͼ��
    static cv::Mat image(height, width, CV_8UC3); // ����һ�� 3 ͨ����ͼ�����
    // �����ݸ��Ƶ�ͼ����
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // ���㵱ǰ�������ڴ��е�����
            int index = y * width + x;
    
            // ��ָ�������л�ȡ R��G��B ͨ��������ֵ
            unsigned char  r = data->r_ChannelData[index];
            unsigned char  g = data->g_ChannelData[index];
            unsigned char  b = data->b_ChannelData[index];
    
            // ����ͼ�������ֵ
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
        }
    }
    // ��ȡ�������ʱ���
    auto end = std::chrono::high_resolution_clock::now();
    // �����������ʱ��
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // �������ʱ��
    std::cout << "rgbָ��תMat����ʱ��: " << duration << " ����" << std::endl;
    
    u_char *virt_base_addr;                        //��ʱ��֧��yuv���룬 ����֧��

    
    
    //std::ofstream outputFile("DMS_Result.txt"); // ����һ����Ϊexample.txt���ı��ļ�

        
    ZXW_ADAS_Detect(image, virt_base_addr,*ADAS_param, YolopDetectResult, LaneImg,ADASResult);

    

    return 0;

}


int number = 0;

int ADAS_infer(const ImageData_ADAS* data, zxw_adas_Paramaters *ADAS_param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult)
{
    
    ADAS_param->speed_kmh = 70;                  //70km/h        ����ʵʱ�ٶ�  ���ڼ��㳵����� 
    ADAS_param->lane_threshold = 2.0;         // 2.0-3.0 ����ƫ��  ԽСԽ����  �ɱ�����Ƶ����
    ADAS_param->hit_threshold = 3.0;            //������ײ  Խ��Խ����  �����鱾����Ƶ���� ��Ҫʵ��·�ϲ���
    ADAS_param->tooclose_threshold = 0.4;    //�������  Խ��Խ����   �ɱ�����Ƶ����
    
    if (frame_index%4==0) //���֡��������12�����Ԥ�������������
      {
      try
          {
      
      //std::cout << "the main program into DMS_infer!!!"<< std::endl;
      //std::cout << std::to_string(data->image_width)<< std::endl;
      int width=data->image_width;  // ͼ����
      int height=data->image_height; // ͼ��߶�
      
      // ��ȡ����ʼʱ���
      // auto start = std::chrono::high_resolution_clock::now();
      
      //cv::Mat image(height, width, CV_8UC3, data->rgb_data);
      cv::Mat image(384, 384, CV_8UC3, data->rgb_data);
      
      // ���ͼ��ߴ���Ϣ
      std::cout << "ADAS Image width: " << image.cols << " pixels" << std::endl;
      std::cout << "ADAS Image height: " << image.rows << " pixels" << std::endl;

      //�ߴ�ת��
      cv::Mat resizedImage;
      cv::resize(image, resizedImage, cv::Size(640, 352));
      
      //
      // ����ͼ��
      cv::imwrite("/tmp/"+std::to_string(number) + ".jpg", image);
  
      std::cout << "adas ͼ���ѱ�������/tmp/"+std::to_string(number) + ".jpg"<< std::endl;
      number++;
      //std::cout << "the program will enter ZXW_DMS_Detect() !"<< std::endl;
      u_char *virt_base_addr;                        //��ʱ��֧��yuv���룬 ����֧��
      ZXW_ADAS_Detect(resizedImage, virt_base_addr,*ADAS_param, YolopDetectResult, LaneImg,ADASResult);
      
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
        
      //����ָ�ͼƬ
      //cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_lane_line" +".jpg", cv::InputArray(LaneImg));
      cv::imwrite("/tmp/"+std::to_string(frame_image_number) + "_lane_line" +".jpg", *LaneImg);
      
      //std::cout << "ZXW_DMS_Detect() is finished !"<< std::endl;
      /*
      
      // ��ȡ�������ʱ���
      // auto end = std::chrono::high_resolution_clock::now();
      // �����������ʱ��
      // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
      // �������ʱ��
      // std::cout << "ͼƬתrgbָ�뻨��ʱ��: " << duration << " ����" << std::endl;
      
      u_char *virt_base_addr;                        //��ʱ��֧��yuv���룬 ����֧��
  
      
      
      //std::ofstream outputFile("DMS_Result.txt"); // ����һ����Ϊexample.txt���ı��ļ�
  
      //DMSDetectResult DMSresult;                    //DMSʶ����
      //zxw_object_result Detectresult[20];      //���Ŀ�
      //memset(Detectresult, 0, sizeof(zxw_object_result)*20);
      // ��ȡ����ʼʱ���
      auto start = std::chrono::high_resolution_clock::now();
      ZXW_DMS_Detect(image, virt_base_addr, YoloDetectResult, dms_detect_result);
      // ��ȡ�������ʱ���
      auto end = std::chrono::high_resolution_clock::now();
  
      // �����������ʱ��
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
      // �������ʱ��
      std::cout << "ZXW_DMS_Detect����ʱ��: " << duration << " ����" << std::endl;
      
      */
          }catch (const std::exception& e){
              // ���񲢴����쳣
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

