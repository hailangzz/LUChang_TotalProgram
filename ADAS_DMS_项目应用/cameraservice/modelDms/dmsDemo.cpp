#include <iostream>
#include <DMS_interface.h>
#include <ZXWAlgApi.h>
#include <stdlib.h>
#include "settings.h"
#include "audio/librraudio.h"

static zxw_object_result * mYoloDetectResult;
static int dms_debug;
int dms_function_initial(void)
{
	int init_fage=INIT_DMS_MODEL();
	printf("INIT_DMS_MODEL->%d\n", init_fage);
	mYoloDetectResult = (zxw_object_result*)malloc(sizeof(zxw_object_result)*20);
	dms_debug = 0;
	return init_fage;
}

int dms_function_unInitial(void)
{
	int uninit_fage=UNINIT_DMS_MODEL();
	free(mYoloDetectResult);
	printf("UNINIT_DMS_MODEL->%d\n", uninit_fage);
	return uninit_fage;
}

int dms_function_test(void* paraIn, int chl, int *result) 
{
    ImageData data; 	
	DMSDetectResult DMSResult;
	
	data.DMS_ADAS_Flag = chl;
	data.rgb_data = (unsigned char*)paraIn;	
	data.image_width = 384;
	data.image_height = 384;

	DMS_infer(&data, mYoloDetectResult, &DMSResult);
	result[0] = 0;
	
	if(DMSResult.event_eyeclose) 
		result[0] = ADUIO_RES_WRN_DMS_EYECLOSE;
	else if(DMSResult.event_yawn) 	
		result[0] = ADUIO_RES_WRN_DMS_YAWN;
	else if(DMSResult.event_glass) 	
		result[0] = ADUIO_RES_WRN_DMS_GLASS;
	else if(DMSResult.event_smoke) 	
		result[0] = ADUIO_RES_WRN_DMS_SMOKE;
	else if(DMSResult.event_calling)	
		result[0] = ADUIO_RES_WRN_DMS_CALLING;
	else if(DMSResult.event_mask) 	
		result[0] = ADUIO_RES_WRN_DMS_MASK;
	
	printf("event_eyeclose->%x!!\n", DMSResult.event_eyeclose);
	printf("event_yawn->%x!!\n", DMSResult.event_yawn);
	printf("event_glass->%x!!\n", DMSResult.event_glass);
	printf("event_smoke->%x!!\n", DMSResult.event_smoke);
	printf("event_calling->%x!!\n", DMSResult.event_calling);
	printf("event_mask->%x!!\n", DMSResult.event_mask);
	//-----------------------------------
	if(0)
	{
		char saveName[128];
		FILE *fp;
		sprintf(saveName, "/tmp/rawImage%02d.raw", dms_debug);
		fp = fopen(saveName, "wb+");
		fwrite(paraIn, 1, 384*384*3, fp);
		fclose(fp);	
		printf("DMS_infer save->%d\n", dms_debug++);
	}		
	//-----------------------------------
	
    return 0;
}

//int dms_function_test(void* paraIn) 
//{
//    int number = 3000;
//   
//    int init_fage=INIT_DMS_MODEL();
//    
//    //前100帧图像不会报警，用于队列数据填充，100帧之后有事件才会报警
//    for(int i = 1; i < number; i++)
//    {
//      
//      cv::Mat img = cv::imread("./smoke/" + std::to_string(i) + ".jpg");  //必须是红外图像   即3通道的灰度图
//      cv::resize(img, img, cv::Size(384, 384));
//      
//      // 检查图像是否成功读取
//      if (img.empty())
//      {
//          // 图像读取失败
//          std::cout << "can not read this picture: " << "./smoke/" + std::to_string(i) + ".jpg" << std::endl;
//          continue;
//      }
//      else
//      {// 图像读取失败
//          std::cout << "this image is readed!!!: " << "./smoke/" + std::to_string(i) + ".jpg" << std::endl;
//      }

//       // 获取图像的宽度和高度
//      int width = img.cols;
//      int height = img.rows;
//  
//      // 分割图像的颜色通道
//      cv::Mat channels[3];
//      cv::split(img, channels);
//  
//      // 分配内存并复制图像数据到指针中
//      uchar* redChannelData = new uchar[width * height];
//      uchar* greenChannelData = new uchar[width * height];
//      uchar* blueChannelData = new uchar[width * height];
//  
//      memcpy(redChannelData, channels[2].data, width * height);
//      memcpy(greenChannelData, channels[1].data, width * height);
//      memcpy(blueChannelData, channels[0].data, width * height);
//      
//      ImageData2 data;
//      data.r_ChannelData = redChannelData;
//      data.g_ChannelData = greenChannelData;
//      data.b_ChannelData = blueChannelData;
//      data.image_width = 384;  // 384
//      data.image_height =384; // 384
//      
//      DMSDetectResult DMSresult;                    //DMS识别结果
//      zxw_object_result Detectresult[20];      //检测的框
//      
//      int result = DMS_infer2(&data,Detectresult,&DMSresult);
//      if(DMSresult.event_calling == 1){
//          printf("**************************************************************calling\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_calling";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//      if(DMSresult.event_eyeclose == 1){
//          printf("**************************************************************close eye\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_eyeclose";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//      if(DMSresult.event_glass == 1){
//          printf("glass**********************************************************glass\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_glass";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//      if(DMSresult.event_mask == 1){
//          printf("***************************************************************mask\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_mask";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//      if(DMSresult.event_smoke == 1){
//          printf("***************************************************************smoking\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_smoke";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//      if(DMSresult.event_yawn == 1){
//          printf("***************************************************************yawn\n");
//          //currentLine=std::to_string(i) + "_" + image_frame_S + "s   " +"event_yawn";
//          //outputFile << currentLine << std::endl; //文件中写入字符串
//      }
//        
//    } //for循环结束 
//    
//    int uninit_fage=UNINIT_DMS_MODEL();
//  
//    return 0;
//}

