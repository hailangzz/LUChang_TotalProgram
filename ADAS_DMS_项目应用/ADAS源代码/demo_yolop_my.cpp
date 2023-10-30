
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
#include<dirent.h>   // /usr/include/dirent.h
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <fstream>

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int class_label;
    float confidence;
} zxw_object_info_p;   //目标检测的结果信息结构体

void qsort_descent_inplace(std::vector<zxw_object_info_p> &objects, int left, int right) {
    int i = left;
    int j = right;
    float p = objects[(left + right) / 2].confidence;

    while (i <= j) {
        while (objects[i].confidence > p) i++;
        while (objects[j].confidence < p) j--;

        if (i <= j) {
            std::swap(objects[i], objects[j]);
            i++;
            j--;
        }
    }

#pragma omp parallel sections
    {
#pragma omp section
        {
            if (left < j) qsort_descent_inplace(objects, left, j);
        }
#pragma omp section
        {
            if (right > i) qsort_descent_inplace(objects, i, right);
        }
    }
}

//多目标检测结果按置信度水平进行排序···
void qsort_descent_inplace(std::vector<zxw_object_info_p> &objects) {
    if (objects.empty()) return;
    qsort_descent_inplace(objects, 0, objects.size() - 1);
}

//计算 两个检测框的IOU值
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1, float ymax1) {
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1));
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1));
    float i = w * h;
    float u = (xmax0 - xmin0) * (ymax0 - ymin0) + (xmax1 - xmin1) * (ymax1 - ymin1) - i;
    return u <= 0.f ? 0.f : (i / u);
}

//对检测框进行NMS极大值抑制筛选
void detect_nms(std::vector<zxw_object_info_p> &objects, std::vector<size_t> &picked, float nms_confidence) {
    picked.clear();
    const size_t n = objects.size();

    for (size_t i = 0; i < n; ++i) {
        const zxw_object_info_p &object_a = objects[i];
        int keep = 1;

        for (size_t j = 0; j < picked.size(); ++j) {
            const zxw_object_info_p &object_b = objects[picked[j]];
            if (object_b.class_label != object_a.class_label)  
                continue;

            int xmin0 = object_a.x;
            int ymin0 = object_a.y;
            int xmax0 = object_a.x + object_a.width;
            int ymax0 = object_a.y + object_a.height;
            int xmin1 = object_b.x;
            int ymin1 = object_b.y;
            int xmax1 = object_b.x + object_b.width;
            int ymax1 = object_b.y + object_b.height;
            float IOU = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (IOU > nms_confidence) keep = 0;
        }
        if (keep) picked.push_back(i);
    }
}

//模型的参数配置结构体信息
typedef struct SMODELPARAM
{
    std::string modelpath;
    int width;
    int height;
    std::string input_node;
    std::vector<std::string> output_node;
    int format;
    int with_nms;
    int nms_load_flag;
    float thresh;
}S_MODELPARAM;


//多进程操作结构体信息
typedef struct S_NetIdINFO_S
{
    VIM_U64    u64NId ;
    pthread_t  pthreadset;
    pthread_t  pthreadget;
    VIM_S32 set_thread_exit;
	VIM_S32 get_thread_exit;
    
}S_NetIdINFO_S;

//网络输出层的输出结果信息
typedef struct {
	int w,h,c,n;
	void* data;
    float scale;
} net_out_blob_;




//网络模型类定义
class Net {
    public:
        std::vector<net_out_blob_> net_out_blob;  //定义：模型输出层的结构体数组对象

   public:
        Net();  //声明构造函数
        ~Net(); //声明析构函数
        VIM_S32 ZXW_load_model(const char *modelFile); //模型加载
        VIM_S32 ZXW_input(unsigned char* data, int width, int height, int channel, const char *inputnode); //数据输入
        VIM_S32 ZXW_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale); //模型推理

   private:
       S_NetIdINFO_S  g_stNetId;  //网络id变量
       NPU_INFO_S g_stInfo;       //NPU信息变量
       S_MODELPARAM mparam;       //模型参数变量
       VIM_F32 *pLocAddr_F32[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};  //输出结果信息32bit
       VIM_S16 *pLocAddr_S16[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};  //输出结果信息16bit
       VIM_U8 *pLocAddr_U8[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};    //输出结果信息8bit
       int gOutputNodeNumber;   //输出结构节点数量
       int scale_flag;          //缩放标志量
       int gBits;               //量化类型

};

Net::Net()   //构造函数定义
{
    scale_flag = 0;   

}

Net::~Net()  //析构函数定义
{
    VIM_S32 s32Ret = NPU_MPI_DestroyNetWork(g_stNetId.u64NId);  //销毁网络
	if (VIM_SUCCESS != s32Ret)
	{
		printf("NPU_MPI_DestroyNetWork %llu  failed\n",g_stNetId.u64NId);
	}
    for(int i = 0; i < this->gOutputNodeNumber; i++)            //销毁内存分配信息
    {
        if(this->gBits == 32)
        {
            free(pLocAddr_F32[i]);  

        }else if (this->gBits == 16)
        {
            free(pLocAddr_S16[i]);
        }else if (this->gBits == 8)
        {
            free(pLocAddr_U8[i]);
        }  
    }
    scale_flag = 0;


    NPU_MPI_UnInit();   //销毁NPU结构系统
}

VIM_S32 Net::ZXW_load_model(const char *modelFile)   //定义模型加载函数
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    /******************************************
     step 1: init mpi system
    ******************************************/
    s32Ret = NPU_MPI_Init();
    if (VIM_SUCCESS != s32Ret)
    {
         printf("NPU_MPI_Init fail! s32Ret = 0x%x\n", s32Ret);
         return -1;
    }

    /******************************************
     step 2: create net work
    ******************************************/
    int u32Encrypt = 0;
    g_stNetId.u64NId = -1;    //初始化网络ID（类私有成员变量）信息
    s32Ret = NPU_MPI_CreateNetWork(&g_stNetId.u64NId, (VIM_CHAR*)modelFile, 1, u32Encrypt);  //创建网络神经网络结构
    if (VIM_SUCCESS != s32Ret)
    {
        printf("NPU_MPI_CreatNetWork fail! s32Ret = 0x%x\n", s32Ret);
        NPU_MPI_UnInit();
        return -1;
    }
   
	s32Ret = NPU_MPI_GetInfo(g_stNetId.u64NId, &g_stInfo);  //网络创建成功后，获取NPU信息，并返回g_stInfo的引用。
    if (VIM_SUCCESS != s32Ret)
	{
		printf("NPU_MPI_GetInfo fail, there is no any input or output! s32Ret = 0x%x\n", s32Ret);
		return -1;
	}

    return VIM_SUCCESS;  //网络创建结束

}   

VIM_S32 Net::ZXW_input(unsigned char* data, int width, int height, int channel, const char *inputnode) //定义模型输入函数
{
    VIM_S32 s32Ret = VIM_SUCCESS;  
    NPU_TASK_NODE_S stTaskNode;  //声明NPU任务变量
    VIM_U32 u32Label;            //
    NPU_INFO_S stInTensor;       //声明NPU输入信息变量


    //s32Ret = read_image_file(images_test[images_test_ptr], readimg, mparam.format, mparam.width, mparam.height, &sw, &sx, &sy);
    //if (s32Ret != 0)
    //    return s32Ret;

    s32Ret = NPU_MPI_CreateTask(g_stNetId.u64NId, &stTaskNode); //创建NPU任务，返回NPU任务节点信息的引用
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_CreateTask err %x!\n", s32Ret);
        return s32Ret;
    }

    //��ȡ�����ĵ�ַ
    s32Ret =  NPU_MPI_GetTaskInTensor(&stTaskNode, (VIM_CHAR*)inputnode, &stInTensor); //根据创建的NPU任务节点信息，获取任务的输入节点tensor并返回其引用
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetTaskInTensor err %x!\n", s32Ret);
        return s32Ret;
    }

    //将输入图像data的数据，拷贝到NPU任务的输入节点Tensor结构中，等待推理
    {
        int size = width*height*channel;
        VIM_U8 *pu8In = stInTensor.stBufInfo[0].pu8Vaddr;
        memcpy(pu8In, data, size);
        //u32Label = images_test_ptr;
        //images_test_ptr++;
    }
    //�ύһ������

    //stTaskNode.u32Private = u32Label;
    stTaskNode.u32Private = 0;   //初始化当前NPU任务变量的私有ID信息
    s32Ret = NPU_MPI_Predict(g_stNetId.u64NId, &stTaskNode); //对此网络、此任务。完成推理，并返回此任务的节点变量引用
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_Predict err %x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        printf("NPU_MPI_Predict over!\n");
    }

    return VIM_SUCCESS;
}

//获取模型推理结果
VIM_S32 Net::ZXW_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale)
{

    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;
    NPU_TASK_NODE_S stTaskNode;  //声明NPU任务变量
    NPU_INFO_S stOutTensor;      //声明NPU输出信息变量
    this->gOutputNodeNumber = OutputNodeNumber; //初始化私有成员变量
    this->gBits = Bits;          //初始化私有成员变量

    s32Ret = NPU_MPI_GetRunDone(g_stNetId.u64NId, &stTaskNode); //等待网路推理完成，返回NPU任务节点输出变量引用
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetResult err %x!\n\r", s32Ret);
        return s32Ret;
    }
    else
    {
        printf("NPU_MPI_GetRunDone ok\n");
    }

    std::vector<net_out_blob_>().swap(net_out_blob);  //将net_out_blob的信息进行高效信息交换
    for(i = 0; i < OutputNodeNumber; i++) //循环输出节点数量
    {
    	//printf("get outlayer %s\n", params.outout_loc[i]);
        s32Ret =  NPU_MPI_GetTaskOutTensor(&stTaskNode, (VIM_CHAR*)NodeNames[i].c_str(), &stOutTensor); //获取NPU任务节点的对象、使用当前输出节点名称、最终返回NPU节点输出层的Tensor信息
        if (s32Ret != VIM_SUCCESS)
        {
            printf("NPU_MPI_GetTaskOutTensor Loc num =%d  err %x!\n", i, s32Ret);
            return s32Ret;
        }

		if (Bits == 32)
		{
			int node_out_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
			printf("s32 node_out_size=%d\n", node_out_size);
			
            if(pLocAddr_F32[i] == VIM_NULL)
            {
                pLocAddr_F32[i] = (VIM_F32*)malloc(stOutTensor.stBufInfo[0].u32BufSize * 4);
                if (pLocAddr_F32[i] == VIM_NULL)
                {
                    printf("malloc output buffer failed!\n");
                    return s32Ret;
                }

            }
	        
	        NPU_MPI_GetFloatData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
	                 stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, pLocAddr_F32[i]);
	        net_out_blob_ nob;         
	        nob.data = pLocAddr_F32[i];
            nob.c = stOutTensor.stBufInfo[0].u32C;
            nob.h = stOutTensor.stBufInfo[0].u32H;
            nob.w = stOutTensor.stBufInfo[0].u32W;
            net_out_blob.emplace_back(nob);
        }
        else if (Bits == 16)
        {
        	int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
	        std::cout<<"347 lines id: fix_size uint16 malloc size:"<<fix_size<<std::endl;  //模型结果申请的内存空间，大小信息 ：fix_size
            if(pLocAddr_S16[i] == VIM_NULL)
            {
                //printf("*/*/*/***/\n");
                pLocAddr_S16[i] = (VIM_S16*)malloc(fix_size);
                if (pLocAddr_S16[i] == VIM_NULL)
                {
                    printf("malloc ps16Addr buffer failed!\n");
                    return s32Ret;
                }

            }

	        VIM_U16 bTransposed = 0;
	        VIM_S32 s32ShiftBits = 0;
	        float pfScale = 0.0;
	        
	        NPU_MPI_GetFixedData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
	            stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, 
	            pLocAddr_S16[i], &bTransposed, &s32ShiftBits,&pfScale); 

            if (s32ShiftBits >= 0)
            {
                FixScale = (float)1.0 / (1 << s32ShiftBits);
            }   
            else
            {
                FixScale = (float)(1 << (0 - s32ShiftBits));
            }
	        
	        net_out_blob_ nob;         
	        nob.data = pLocAddr_S16[i];
            nob.c = stOutTensor.stBufInfo[0].u32C;
            nob.h = stOutTensor.stBufInfo[0].u32H;
            nob.w = stOutTensor.stBufInfo[0].u32W;
            nob.scale = FixScale;
            net_out_blob.emplace_back(nob);
            
        }
        else if (Bits == 8)
        {
        	int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
			std::cout<<"388 lines id: fix_size uint8 malloc size:"<<fix_size<<std::endl;  //模型结果申请的内存空间，大小信息 ：fix_size
            if(pLocAddr_U8[i] == VIM_NULL)
            {
                pLocAddr_U8[i] = (VIM_U8*)malloc(int(fix_size));
                if (pLocAddr_U8[i] == VIM_NULL)
                {
                    printf("malloc u8Addr buffer failed!\n");
                    return s32Ret;
                }

            }
	        VIM_U16 bTransposed = 0;
	        VIM_S32 s32ShiftBits = 0;
	        float pfScale = 0.0;
	        
	        NPU_MPI_GetFixedData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
	            stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, 
	            pLocAddr_U8[i], &bTransposed, &s32ShiftBits,&pfScale);
	        
            //printf("s32ShiftBits=%d\n", s32ShiftBits);
            //printf("pfScale=%f\n", pfScale);
            if (s32ShiftBits >= 0)
            {
                FixScale = (float)1.0 / (1 << s32ShiftBits);
            }   
            else
            {
                FixScale = (float)(1 << (0 - s32ShiftBits));
            }
            FixScale = pfScale;
	        net_out_blob_ nob;         
	        nob.data = pLocAddr_U8[i];
            nob.c = stOutTensor.stBufInfo[0].u32C;
            nob.h = stOutTensor.stBufInfo[0].u32H;
            nob.w = stOutTensor.stBufInfo[0].u32W;
            nob.scale = FixScale;
            net_out_blob.emplace_back(nob);
	        
	        // if (scale_flag == 0)
	        // {
		    //FixScale = pfScale;
		    //     scale_flag = 1;
	    	// }
        }
        else
        {
        	printf("fix mode not surport\n");
        	return -1;
        }
    }

    s32Ret = NPU_MPI_DestroyTask(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_DestroyTask err %x!\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;


}



class detect_yolop{

    public:
        Net *pNet;
 
    
        detect_yolop();
        ~detect_yolop();
        VIM_S32 detect_yolop_load_model(const char *modelFile);
        VIM_S32 detect_yolop_input(unsigned char* data, int width, int height, int channel, const char *inputnode);
        VIM_S32 detect_yolop_input(cv::Mat img, int width, int height, int channel, const char *inputnode);
        VIM_S32 detect_yolop_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits);
        VIM_S32 detect_yolop_get_result(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<zxw_object_info_p> &objects,cv::Mat &mark_images);
    
    private:
        std::vector<std::vector<float>> outpoutdata;
        unsigned char* gdata;
        float FScale;

};


const std::vector<std::vector<float>> anchors = {
    {19,50,38,81,68,157}, {7,18,6,39,12,31}, {3,9,5,11,4,20}};
//const std::vector<std::vector<float>> anchors = {
//    {0.7909*32.0, 3.1555*32.0,  1.3701*32.0, 2.8368*32.0, 1.8908*32.0, 5.1912*32.0}, {0.6198*16.0, 1.5220*16.0, 0.9669*16.0, 2.3219*16.0, 1.5251*16.0, 3.2392*16.0}, {0.3043*8.0, 0.8347*8.0, 0.4964*8.0, 1.3752*8.0,  0.7498*8.0, 2.0096*8.0}};


static void generate_proposals32(VIM_F32* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<zxw_object_info_p> &objects, int classnumber, float FScale)
{
    VIM_F32  *pSrcAddr;
    int num_elem = 5 + classnumber;
    int offnum = num_elem*3 + 8 - ((num_elem*3)%8);
	//int offnum = 48;

    float s, tmp;
    float score, max;
    int idx_max = 0;
    //int fixscorethres = int(0.3/FScale); //objconfidence
	int fixscorethres = int(objconfidence/FScale);
	
    pSrcAddr = output_ptr;

    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {
                //printf("before p\n");
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
                //printf("score is %f\n,",score);
                //std::cout << pSrcAddr[count_num*offnum + m*num_elem + 4] << "::" << score << ":" << fixscorethres << std::endl;
                if (score <= fixscorethres) {
                    count_num++;
                    continue;
                }
                max = 0;
                for (int n = 0; n < classnumber; n++) {
                    s =  float(pSrcAddr[count_num*offnum + m*num_elem + 5 + n])* FScale;
                    if (s > max) {
                        max = s;
                        idx_max = n;
                    }
                }
                s = max * score; 
                if (s <= fixscorethres){
                    count_num++;
                    continue;
                }
                float dx = (float(pSrcAddr[count_num*offnum + m*num_elem + 0]) * FScale * 2 - 0.5 + k) * downsample;
                float dy = (float(pSrcAddr[count_num*offnum + m*num_elem + 1]) * FScale * 2 - 0.5 + j) * downsample;
                //printf("dx is %f\n,",dx);
                // tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 2]) * FScale * 2;
                // float x2 = tmp * tmp * anchors[anchorindex][2*m];
                // tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 3]) * FScale * 2;
                // float y2 = tmp * tmp * anchors[anchorindex][2*m+1]; 
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 2]) * FScale * 2;
                float dw = tmp * tmp * anchors[anchorindex][2*m];
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 3]) * FScale * 2;
                float dh = tmp * tmp * anchors[anchorindex][2*m+1];       
                float confidence = float(s) * FScale ;
                int classType = idx_max;
                count_num++;

                float x0 = dx - (dw - 1.0f) * 0.5f;
                float x1 = dx + (dw - 1.0f) * 0.5f;
                float y0 = dy - (dh - 1.0f) * 0.5f;
                float y1 = dy + (dh - 1.0f) * 0.5f;

                int x = x0;
                if(x < 0) x = 0;
                int y = y0;
                if(y < 0) y = 0;
                int width = x1 - x0;
                if(width < 0) width = 0;
                int height = y1 - y0;
                if(height < 0) height = 0;


                zxw_object_info_p obj;
                obj.x = x;
                obj.y = y;
                obj.width = width;
                obj.height = height;
                obj.class_label = classType;
                obj.confidence = confidence;
                objects.push_back(obj);
            }
        }
    }

}



static void generate_proposals16(VIM_S16* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<zxw_object_info_p> &objects, int classnumber, float FScale)
{
    VIM_S16  *pSrcAddr;
    int num_elem = 5 + classnumber;
    int offnum = num_elem*3 + 8 - ((num_elem*3)%8);
	//int offnum = 48;

    float s, tmp;
    float score, max;
    int idx_max = 0;
    //int fixscorethres = int(0.3/FScale); //objconfidence
	int fixscorethres = int(objconfidence/FScale);
	
    pSrcAddr = output_ptr;

    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {
                //printf("before p\n");
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
                //printf("score is %f\n,",score);
                //std::cout << pSrcAddr[count_num*offnum + m*num_elem + 4] << "::" << score << ":" << fixscorethres << std::endl;
                if (score <= fixscorethres) {
                    count_num++;
                    continue;
                }
                max = 0;
                for (int n = 0; n < classnumber; n++) {
                    s =  float(pSrcAddr[count_num*offnum + m*num_elem + 5 + n])* FScale;
                    if (s > max) {
                        max = s;
                        idx_max = n;
                    }
                }
                s = max * score; 
                if (s <= fixscorethres){
                    count_num++;
                    continue;
                }
                float dx = (float(pSrcAddr[count_num*offnum + m*num_elem + 0]) * FScale * 2 - 0.5 + k) * downsample;
                float dy = (float(pSrcAddr[count_num*offnum + m*num_elem + 1]) * FScale * 2 - 0.5 + j) * downsample;

                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 2]) * FScale * 2;
                float dw = tmp * tmp * anchors[anchorindex][2*m];
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 3]) * FScale * 2;
                float dh = tmp * tmp * anchors[anchorindex][2*m+1];       
                float confidence = float(s) * FScale ;
                int classType = idx_max;
                count_num++;

                float x0 = dx - (dw - 1.0f) * 0.5f;
                float x1 = dx + (dw - 1.0f) * 0.5f;
                float y0 = dy - (dh - 1.0f) * 0.5f;
                float y1 = dy + (dh - 1.0f) * 0.5f;

                int x = x0;
                if(x < 0) x = 0;
                int y = y0;
                if(y < 0) y = 0;
                int width = x1 - x0;
                if(width < 0) width = 0;
                int height = y1 - y0;
                if(height < 0) height = 0;


                zxw_object_info_p obj;
                obj.x = x;
                obj.y = y;
                obj.width = width;
                obj.height = height;
                obj.class_label = classType;
                obj.confidence = confidence;
                objects.push_back(obj);
            }
        }
    }

}

static void generate_proposals8(VIM_U8* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<zxw_object_info_p> &objects, int classnumber, float FScale)
{
    VIM_U8  *pSrcAddr;
    int num_elem = 5 + classnumber;
    int offnum =num_elem*3 + 16 - ((num_elem*3)%16);
    //int offnum = 64;
    float s, tmp;
    float score, max;
    int idx_max = 0;
    int fixscorethres = int(objconfidence/FScale);
    printf("fixscorethres=%d downsample=%d FScale=%f\n",fixscorethres,downsample,FScale);
    pSrcAddr = output_ptr;

    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
                // printf("888888888888888888888888888888888888888888888\n");
                // for(int kkk = 0; kkk< 32;kkk++)
                // {
                //     printf("0=%d\n", pSrcAddr[count_num*offnum + m*num_elem + kkk]);
                // }


                //printf("score is %f,",score);
                if (score <= fixscorethres) {
                    count_num++;
                    continue;
                }
                max = 0;
                for (int n = 0; n < classnumber; n++) {
                    s =  float(pSrcAddr[count_num*offnum + m*num_elem + 5 + n])* FScale;
                    if (s > max) {
                        max = s;
                        idx_max = n;
                    }
                }
                s = max * score; 
                if (s <= fixscorethres){
                    count_num++;
                    continue;
                }
                float dx = (float(pSrcAddr[count_num*offnum + m*num_elem + 0]) * FScale * 2 - 0.5 + k) * downsample;
                float dy = (float(pSrcAddr[count_num*offnum + m*num_elem + 1]) * FScale * 2 - 0.5 + j) * downsample;

                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 2]) * FScale * 2;  //计算不同ancher_index = 0时的【0,1,2,3,4,5】；ancher_index = 1时的【0,1,2,3,4,5】等；，
                float dw = tmp * tmp * anchors[anchorindex][2*m];
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 3]) * FScale * 2;
                float dh = tmp * tmp * anchors[anchorindex][2*m+1];       
                float confidence = float(s) * FScale ;
                int classType = idx_max;
                count_num++;

                float x0 = dx - (dw - 1.0f) * 0.5f;
                float x1 = dx + (dw - 1.0f) * 0.5f;
                float y0 = dy - (dh - 1.0f) * 0.5f;
                float y1 = dy + (dh - 1.0f) * 0.5f;

                int x = x0;
                if(x < 0) x = 0;
                int y = y0;
                if(y < 0) y = 0;
                int width = x1 - x0;
                if(width < 0) width = 0;
                int height = y1 - y0;
                if(height < 0) height = 0;


                zxw_object_info_p obj;
                obj.x = x;
                obj.y = y;
                obj.width = width;
                obj.height = height;
                obj.class_label = classType;
                obj.confidence = confidence;
                objects.push_back(obj);
            }
        }
    }

}


static float sigmoid(float x)
{
	return 1.0 / (1.0 + exp(-x));
}



static int read_image_file(cv::Mat imagein, unsigned char* img, int format, int w, int h)
{
    int ch_size = w * h;
    unsigned char* r = img;
    unsigned char* g = img + ch_size;
    unsigned char* b = img + ch_size*2;


    cv::Mat model_img(h, w, CV_8UC3);
    if (imagein.cols == w && imagein.rows == h)
    {
        model_img = imagein;
    }
    else
    {
        printf("model_img not equal imagein size\n");
        return -1;

    }
   

    int offset = 0;
    if (format == 0)
    {
        for (int i=0; i<h; i++)
        {
            for (int j=0; j<w; j++)
            {
                offset = i * w + j;
                b[offset] = model_img.at<cv::Vec3b>(i, j)[0];
                g[offset] = model_img.at<cv::Vec3b>(i, j)[1];
                r[offset] = model_img.at<cv::Vec3b>(i, j)[2];
            }
        }
    }
    else
    {
        for (int i=0; i<h; i++)
        {
            for (int j=0; j<w; j++)
            {
                offset = i * w + j;
                b[offset] = model_img.at<cv::Vec3b>(i, j)[2];
                g[offset] = model_img.at<cv::Vec3b>(i, j)[1];
                r[offset] = model_img.at<cv::Vec3b>(i, j)[0];
            }
        }
    }

    return 0;
}




detect_yolop::detect_yolop()
{
    pNet = new Net();
}

detect_yolop::~detect_yolop()
{
    if (pNet != nullptr) 
    {
        pNet->~Net();
        //delete pNet;
        //pNet = nullptr;
    }
}

VIM_S32 detect_yolop::detect_yolop_load_model(const char *modelFile)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    s32Ret = pNet->ZXW_load_model(modelFile);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("pNet->ZXW_load_model! s32Ret = 0x%x\n", s32Ret);
        return -1;
    }

    return s32Ret;
}


VIM_S32 detect_yolop::detect_yolop_input(cv::Mat img, int width, int height, int channel, const char *inputnode)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    
    gdata = (unsigned char*)malloc(width*height*channel);

    s32Ret = read_image_file(img, gdata, 0, width, height);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("read_image_file error! s32Ret = 0x%x\n", s32Ret);
        return -1;
    }

    s32Ret = pNet->ZXW_input(gdata, width, height, channel, inputnode);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("pNet->ZXW_input! s32Ret = 0x%x\n", s32Ret);
        return -1;
    }

    free(gdata);

    return s32Ret;
}


VIM_S32 detect_yolop::detect_yolop_input(unsigned char* data, int width, int height, int channel, const char *inputnode)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    s32Ret = pNet->ZXW_input(data, width, height, channel, inputnode);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("pNet->ZXW_input! s32Ret = 0x%x\n", s32Ret);
        return -1;
    }

    return s32Ret;
}


VIM_S32 detect_yolop::detect_yolop_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    s32Ret = pNet->ZXW_inference(OutputNodeNumber, NodeNames, Bits, FScale);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("pNet->ZXW_inference! s32Ret = 0x%x\n", s32Ret);
        return -1;
    }


    return s32Ret;
}


VIM_S32 detect_yolop::detect_yolop_get_result(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<zxw_object_info_p> &objects,cv::Mat &mark_images)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    std::vector<zxw_object_info_p> proposals;
    for(int i  = 0; i < pNet->net_out_blob.size(); i++)
    {
        int H = pNet->net_out_blob[i].h;
        int W = pNet->net_out_blob[i].w;
        int C = pNet->net_out_blob[i].c;
        printf("H=%d, W=%d, C=%d\n", H, W, C);

        VIM_F32 *tmpf;
        VIM_S16 *tmps;
        VIM_U8 *tmpu;
        if(Bits == 32)
        {
			tmpf=(VIM_F32 *)pNet->net_out_blob[i].data;
			FScale = pNet->net_out_blob[i].scale;
			printf("FScale_32=%f\n", FScale);
			
			int downsample = mode_input_img_w / W;
            if(downsample == 32)
            {
                generate_proposals32(tmpf, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                generate_proposals32(tmpf, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                generate_proposals32(tmpf, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
            }
			if(downsample == 1)
				{
                //printf("H=%d, W=%d, C=%d\n", H, W, C);
                cv::Mat GreyBack = cv::Mat::zeros(384, 640,CV_8UC1);
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {
                        for(int c = 0; c < C; c++)
                        {
                            if(Bits == 32)
                            {
								if (tmps[h * W*8+ w*8 + 1]*FScale > 0.5)
                                {
                                    GreyBack.at<uchar>(h,w)=255;
                                }
                                else
                                {
                                    GreyBack.at<uchar>(h,w)=0;
                                }
								
                            }else if (Bits == 16)
                            {
								
								
                            }else if (Bits == 8)
                            {
                                
                                
                            }
                        }
                    }   
                }
               cv::imwrite("GreyBack.jpg", GreyBack);
            }

        }
		else if (Bits == 16)
        {
			tmps = (VIM_S16 *)pNet->net_out_blob[i].data;
			FScale = pNet->net_out_blob[i].scale;
            printf("FScale_16=%f\n", FScale);

            int downsample = mode_input_img_w / W;
            if(downsample == 32)
            {
				
                generate_proposals16(tmps, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                generate_proposals16(tmps, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                generate_proposals16(tmps, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 1)
				{
				int pixel_value_index=0;
                //printf("H=%d, W=%d, C=%d\n", H, W, C);
                cv::Mat GreyBack = cv::Mat::zeros(384, 640,CV_8UC1);
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {
                        for(int c = 0; c < C; c++)
                        {
                            if(Bits == 32)
                            {
                            }else if (Bits == 16)
                            {
								//printf("lane mark pixel value: =%d\n", tmps[pixel_value_index]);
								//pixel_value_index+=1;
								//if (tmps[h * W*8+ w*8 + 1]*FScale > 0.5)
								if (tmps[h * W*12+ w*12 + 0]*FScale > 0.5)
                                {
                                    GreyBack.at<uchar>(h,w)=255;
                                }
                                else
                                {
                                    GreyBack.at<uchar>(h,w)=0;
                                }
								
                            }else if (Bits == 8)
                            {
                                
                                
                            }
                        }
                    }   
                }
               cv::imwrite("GreyBack.jpg", GreyBack);
            }
            

        }
		else if (Bits == 8)
        {
            tmpu = (VIM_U8 *)pNet->net_out_blob[i].data;
            FScale = pNet->net_out_blob[i].scale;
			//printf("FScale_8=%f\n", FScale);
            //FScale = 0.007812;                                                                        //强转0.0078125
            
            int downsample = mode_input_img_w / W;
            if(downsample == 32)
            {
                generate_proposals8(tmpu, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                generate_proposals8(tmpu, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                generate_proposals8(tmpu, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 1)
            {
				//printf("downsample=%d FScale=%f\n", downsample,FScale);
				
				//int pixel_value_index=0;
                //printf("image segmentation H=%d, W=%d, C=%d\n", H, W, C);
                //cv::Mat GreyBack = cv::Mat::zeros(384, 640,CV_8UC1);
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {
                        for(int c = 0; c < C; c++)
                        {
                            if(Bits == 32)
                            {
                            }else if (Bits == 16)
                            {
                            }else if (Bits == 8)
                            {
								//printf("lane mark pixel value: =%d\n", tmpu[pixel_value_index]);
								//pixel_value_index+=1;
								//printf("lane mark pixel value: =%d\n", tmpu[h * W*16+ w*16 + 1]*FScale)
                                if (FScale*tmpu[h * W*16+ w*16 + 1] > 0.5)
							    //if (FScale * tmpu[h * W+ w + 1] > 0.5)
                                {
                                    mark_images.at<uchar>(h,w)=255;
                                }
                                else
                                {
                                    mark_images.at<uchar>(h,w)=0;
                                }
                            }
                        }
                    }   
                }
               //cv::imwrite("GreyBack.jpg", GreyBack);
            }
            
        }    
    }
    printf("proposals=%d\n", proposals.size());
    qsort_descent_inplace(proposals);
    std::vector<size_t> picked;
    detect_nms(proposals, picked,objconfidence);
    printf("picked=%d\n", picked.size());
    //std::vector<zxw_object_info_p> object;
    objects.resize(picked.size());

    for (size_t i = 0; i < picked.size(); ++i) {
        objects[i] = proposals[picked[i]];
    }

 

    proposals.clear();
    picked.clear();


    return s32Ret;
}


using namespace std;

int getAbsoluteFiles(string directory, vector<string>& filesAbsolutePath) //参数1[in]要变量的目录  参数2[out]存储文件名
{
    DIR* dir = opendir(directory.c_str()); //打开目录   DIR-->类似目录句柄的东西
    if ( dir == NULL )
    {
        cout<<directory<<" is not a directory or not exist!"<<endl;
        return -1;
    }

    struct dirent* d_ent = NULL;       //dirent-->会存储文件的各种属性
    char dot[3] = ".";                //linux每个下面都有一个 .  和 ..  要把这两个都去掉
    char dotdot[6] = "..";

    while ( (d_ent = readdir(dir)) != NULL )    //一行一行的读目录下的东西,这个东西的属性放到dirent的变量中
    {
        if ( (strcmp(d_ent->d_name, dot) != 0)
              && (strcmp(d_ent->d_name, dotdot) != 0) )   //忽略 . 和 ..
        {
            if ( d_ent->d_type == DT_DIR ) //d_type可以看到当前的东西的类型,DT_DIR代表当前都到的是目录,在usr/include/dirent.h中定义的
            {
                continue;
            }
            else   //如果不是目录
            {
                string absolutePath = directory + string("/") + string(d_ent->d_name);  //构建绝对路径
                if( directory[directory.length()-1] == '/')  //如果传入的目录最后是/--> 例如a/b/  那么后面直接链接文件名
                {
                    absolutePath = directory + string(d_ent->d_name); // /a/b/1.txt
                }
                filesAbsolutePath.push_back(absolutePath);
            }
        }
    }

    closedir(dir);
    return 0;
}


void paste_lane_mask(cv::Mat BaseImage, cv::Mat MarkImage, std::vector<zxw_object_info_p> objects,cv::Mat &segmentedImage) {
    //为原始图片绘制目标框，及粘贴分割图层

    segmentedImage = BaseImage.clone();  // 创建一个副本以保存结果
    //std::cout << MarkImage << std::endl;
    cv::Vec3b color = cv::Vec3b(0, 0, 255); //标记为红色

    for (int y = 0; y < MarkImage.rows; y++) {
        for (int x = 0; x < MarkImage.cols; x++) {            
            // 获取标记信息像素值
            uchar label = MarkImage.at<uchar>(y, x);
            // 根据标记信息决定颜色，这里示例使用随机颜色
            if (label >= 190) {
                // 在分割结果图像上绘制标记信息               
                segmentedImage.at<cv::Vec3b>(y, x) = color;
            }
        }
    }

    //绘制目标检测框体

    for (int k = 0; k < objects.size(); k++)
    {
        if (objects[k].width > 0)
        {
            cv::Rect_<float> rect;
            rect.x = objects[k].x;
            rect.y = objects[k].y;
            rect.width = objects[k].width;
            rect.height = objects[k].height;
            //label = objs[k].confidence;
            cv::rectangle(segmentedImage, rect, cv::Scalar(128, 255, 0));  //绘制目标检测框

        }
    }
}



int main()
{
    detect_yolop *pyolop = new detect_yolop();
    pyolop->detect_yolop_load_model("./yolop-384-640_8bit.npumodel");
	//pyolop->detect_yolop_load_model("./yolop-384-640_16bit.npumodel");
    
    std::vector<std::string> NodeNames;
    NodeNames.push_back("Sigmoid140");
    NodeNames.push_back("Sigmoid142");
    NodeNames.push_back("Sigmoid144");
    NodeNames.push_back("Sigmoid178");
	
	std::string ImageFolderPath = "./images";
	//获取所有待检测图片的路径信息
    std::vector<std::string> jpgFiles;
	getAbsoluteFiles(ImageFolderPath,jpgFiles);
	
	
	for (const std::string& jpgFilePath : jpgFiles) {
		
		std::cout << jpgFilePath << std::endl;
		cv::Mat test_img = cv::imread(jpgFilePath);
		cv::resize(test_img, test_img, cv::Size(640, 384));
		pyolop->detect_yolop_input(test_img, 640, 384, 3, "input0");
		printf("finish detect_yolop_input() \n");
        pyolop->detect_yolop_inference(4, NodeNames, 8);
		printf("finish detect_yolop_inference() \n");
		std::vector<zxw_object_info_p> objs;  //检测到的车辆框
		cv::Mat mark_images = cv::Mat::zeros(384, 640,CV_8UC1);                  //分割好的车道mark图片
		printf("begin line detect_yolop_get_result() \n");
		pyolop->detect_yolop_get_result(8, 1, 640, 384, 0.3, objs,mark_images);
		printf("finish line detect_yolop_get_result() \n");
		cv::Mat segmentedImage;                  //最终的结果图片
		paste_lane_mask(test_img, mark_images, objs,segmentedImage);
		
		size_t found = jpgFilePath.rfind('//');
		
        if (found != std::string::npos) {
            // 找到反斜杠符号，获取最后一段
            std::string image_name = jpgFilePath.substr(found + 1);			
			std::string save_image_name = "./result_adas/"+image_name;
			std::cout<<save_image_name<<std::endl;
            cv::imwrite("./result_adas/"+image_name, segmentedImage);
        }
		
		objs.clear();
		
    }
	
	jpgFiles.clear();
	printf("finish inference\n");
    pyolop->~detect_yolop();

    return 0;
}
