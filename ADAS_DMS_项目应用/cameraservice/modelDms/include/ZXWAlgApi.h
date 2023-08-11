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

//DMS�¼����ͽṹ��
typedef struct {
    int event_eyeclose;                  //����
    int event_yawn;                         //���Ƿ
    int event_glass;                          //��ī��
    int event_smoke;                      //����
    int event_calling;                      //��绰
    int event_mask;                        //����ͷ�ڵ�
} DMSDetectResult;

//ADAS�¼����ͽṹ��
typedef struct {
    int event_lanechange;            //����ƫ��
    int event_tooclose;                  //�������
    int event_hit;                              //������ײ
} ADASDetectResult;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int class_label;
    float confidence;
} zxw_object_result;

//ADAS�������ýṹ��
typedef struct {
    int speed_kmh;                                                //�����ٶ�
    float lane_threshold;                                     //����ƫ����ֵ
    float hit_threshold;                                        //������ײ��ֵ
    float tooclose_threshold;                            //���������ֵ

} zxw_adas_Paramaters;


//����ģ�����ƺ������÷���demo������0Ϊ�ɹ�������-1Ϊʧ��
int GetModelPath(char (*names)[256]);

//NPUӲ����ʼ���� ����0Ϊ�ɹ�������-1Ϊʧ��
int ZXWSystemInit();

//DMS��ʼ��������0Ϊ�ɹ�������-1Ϊʧ��
int ZXW_DMS_Init();

/*
DMS��⺯����ѭ�����ã�����0Ϊ�ɹ�������-1Ϊʧ��
iputimg Ϊ����ͼ��Ϊ����ͼ�񣬼���ͨ�������ƻҶ�ͼ
YUVdataΪyuv���ݣ���ʱ��֧�ָ����룬������֧��
YoloDetectResult Ϊ����Ŀ���
DMSResult ΪDMS�¼����
*/
int ZXW_DMS_Detect(cv::Mat iputimg, uchar *YUVdata, zxw_object_result *YoloDetectResult, DMSDetectResult *DMSResult);

//DMS����ʼ��������0Ϊ�ɹ�������-1Ϊʧ��
int ZXW_DMS_UnInit();

//ADAS��ʼ��������0Ϊ�ɹ�������-1Ϊʧ��
int ZXW_ADAS_Init();

/*
ADAS��⺯����ѭ�����ã�����0Ϊ�ɹ�������-1Ϊʧ��
iputimg Ϊ����ͼƬ��BGR��ʽ
YUVdataΪyuv���ݣ���ʱ��֧�ָ����룬������֧��
param Ϊ���ò������÷���demo
YolopDetectResult Ϊyolop�ĳ��������
LaneImgΪ�ָ���
ADASResultΪADAS�¼����
*/
int ZXW_ADAS_Detect(cv::Mat iputimg, uchar *YUVdata, zxw_adas_Paramaters param, zxw_object_result *YolopDetectResult, cv::Mat *LaneImg, ADASDetectResult *ADASResult);

//ADAS����ʼ��������0Ϊ�ɹ�������-1Ϊʧ��
int ZXW_ADAS_UnInit();

//NPUӲ������ʼ���� ����0Ϊ�ɹ�������-1Ϊʧ��
int ZXWSystemUnInit();


#ifdef __cplusplus
}
#endif

#endif
