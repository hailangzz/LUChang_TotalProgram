#include <iostream>
#include <ADAS_interface.h>
#include <ZXWAlgApi.h>
#include <stdlib.h>


static zxw_object_result * AdasYoloDetectResult; // 全局静态Adas目标检测结果
static zxw_adas_Paramaters ADAS_param;           //ADAS_参数配置
static int adas_debug;                           // 静态Adas检测图像索引个数
static cv::Mat LaneImg;

int adas_function_initial(void)
{
	int init_fage=INIT_ADAS_MODEL();
	printf("INIT_ADAS_MODEL->%d\n", init_fage);
	
	AdasYoloDetectResult = (zxw_object_result*)malloc(sizeof(zxw_object_result)*20);
	ADAS_param.speed_kmh = 70;                  //70km/h      //车辆速度  
    ADAS_param.lane_threshold = 2.0f;           // 2.0-3.0      //车道偏离阈值
    ADAS_param.hit_threshold = 3.0f;            //车辆碰撞阈值
    ADAS_param.tooclose_threshold = 0.4f;       //车距过近阈值
	adas_debug = 0;	
	
	return init_fage;
}

int adas_function_unInitial(void)
{
	int uninit_fage=UNINIT_ADAS_MODEL();
	free(AdasYoloDetectResult);
	printf("UNINIT_ADAS_MODEL->%d\n", uninit_fage);
	return uninit_fage;
}

int adas_function_test(void* paraIn, int chl, int *result) 
{
    ImageData_ADAS data; 	            // 图像输入
	ADASDetectResult ADASResult;    // adas检测结果
	
	data.DMS_ADAS_Flag = chl;
	data.rgb_data = (unsigned char*)paraIn;	
	data.image_width = 640;			//ADAS输入宽度
	data.image_height = 352;        //ADAS输入高度

	//ADAS_infer(&data, AdasYoloDetectResult, &DMSResult);
	ADAS_infer(&data,&ADAS_param, AdasYoloDetectResult,&LaneImg, &ADASResult);
	result[0] = 0;
	
	if(ADASResult.event_lanechange) 
		result[0] = 0; //"ADAS: event_lanechange"
	else if(ADASResult.event_tooclose) 	
		result[0] = 1; //"ADAS: event_tooclose"
	else if(ADASResult.event_hit) 	
		result[0] = 2; //"ADAS: event_hit"
	
	
	printf("event_lanechange->%x!!\n", ADASResult.event_lanechange);
	printf("event_tooclose->%x!!\n", ADASResult.event_tooclose);
	printf("event_hit->%x!!\n", ADASResult.event_hit);
	
	//-----------------------------------
	if(0) //保存ADAS事件图片····
	{
		char saveName[128];
		FILE *fp;
		sprintf(saveName, "/tmp/adas_rawImage%02d.raw", adas_debug);
		fp = fopen(saveName, "wb+");
		fwrite(paraIn, 1, 640*384*3, fp);
		fclose(fp);	
		printf("ADAS_infer save->%d\n", adas_debug++);
	}		
	//-----------------------------------
	
    return 0;
}


