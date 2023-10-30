
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

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

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



typedef struct S_NetIdINFO_S
{
    VIM_U64    u64NId ;
    pthread_t  pthreadset;
    pthread_t  pthreadget;
    VIM_S32 set_thread_exit;
	VIM_S32 get_thread_exit;
    
}S_NetIdINFO_S;

typedef struct {
	int w,h,c,n;
	void* data;
} net_out_blob_;





class Net {
    public:
        std::vector<net_out_blob_> net_out_blob;

   public:
        Net();
        ~Net();
        VIM_S32 ZXW_load_model(const char *modelFile);
        VIM_S32 ZXW_input(unsigned char* data, int width, int height, int channel, const char *inputnode);
        VIM_S32 ZXW_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale);


   private:
       S_NetIdINFO_S  g_stNetId;
       NPU_INFO_S g_stInfo;
       S_MODELPARAM mparam;
       VIM_F32 *pLocAddr_F32[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       VIM_S16 *pLocAddr_S16[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       VIM_U8 *pLocAddr_U8[6] = {VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL};
       int gOutputNodeNumber;
       int scale_flag;
       int gBits;



};

Net::Net()
{
    scale_flag = 0;


}

Net::~Net()
{
    VIM_S32 s32Ret = NPU_MPI_DestroyNetWork(g_stNetId.u64NId);
	if (VIM_SUCCESS != s32Ret)
	{
		printf("NPU_MPI_DestroyNetWork %llu  failed\n",g_stNetId.u64NId);
	}
    for(int i = 0; i < this->gOutputNodeNumber; i++)
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


    NPU_MPI_UnInit(); 
}

VIM_S32 Net::ZXW_load_model(const char *modelFile)
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
    g_stNetId.u64NId = -1;
    s32Ret = NPU_MPI_CreateNetWork(&g_stNetId.u64NId, (VIM_CHAR*)modelFile, 5, u32Encrypt);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("NPU_MPI_CreatNetWork fail! s32Ret = 0x%x\n", s32Ret);
        NPU_MPI_UnInit();
        return -1;
    }
   
	s32Ret = NPU_MPI_GetInfo(g_stNetId.u64NId, &g_stInfo);
    if (VIM_SUCCESS != s32Ret)
	{
		printf("NPU_MPI_GetInfo fail, there is no any input or output! s32Ret = 0x%x\n", s32Ret);
		return -1;
	}

    return VIM_SUCCESS;

}

VIM_S32 Net::ZXW_input(unsigned char* data, int width, int height, int channel, const char *inputnode)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    NPU_TASK_NODE_S stTaskNode;
    VIM_U32 u32Label;
    NPU_INFO_S stInTensor;


    //s32Ret = read_image_file(images_test[images_test_ptr], readimg, mparam.format, mparam.width, mparam.height, &sw, &sx, &sy);
    //if (s32Ret != 0)
    //    return s32Ret;

    s32Ret = NPU_MPI_CreateTask(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_CreateTask err %x!\n", s32Ret);
        return s32Ret;
    }

    //��ȡ�����ĵ�ַ
    s32Ret =  NPU_MPI_GetTaskInTensor(&stTaskNode, (VIM_CHAR*)inputnode, &stInTensor);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetTaskInTensor err %x!\n", s32Ret);
        return s32Ret;
    }

    //�����������
    {
        int size = width*height*channel;
        VIM_U8 *pu8In = stInTensor.stBufInfo[0].pu8Vaddr;
        memcpy(pu8In, data, size);
        //u32Label = images_test_ptr;
        //images_test_ptr++;
    }
    //�ύһ������

    //stTaskNode.u32Private = u32Label;
    stTaskNode.u32Private = 0;
    s32Ret = NPU_MPI_Predict(g_stNetId.u64NId, &stTaskNode);
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


VIM_S32 Net::ZXW_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale)
{
#if 0
    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;
    VIM_F32 *pLocAddr[6] = {VIM_NULL};
    NPU_TASK_NODE_S stTaskNode;
    VIM_F32 *pf32Addr;
    NPU_INFO_S stOutTensor;
    VIM_U32 u32LocNum = 0;

    //LOC ����������
    //u32LocNum = mparam.output_node.size();

    //�ȴ����н��
    s32Ret = NPU_MPI_GetRunDone(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetResult err %x!\n\r", s32Ret);
        return s32Ret;
    }
    else
    {
        printf("NPU_MPI_GetRunDone over!\n");
    }

    // NPU_MPI_ToolDump(&stTaskNode, "npu_dump", 6, 0, 300);

    //��ȡ3��sigmoid��������Ϣ
    for(i = 0; i < OutputNodeNumber; i++)
    {
        s32Ret =  NPU_MPI_GetTaskOutTensor(&stTaskNode, (VIM_CHAR*)NodeNames[i].c_str(), &stOutTensor);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("NPU_MPI_GetTaskOutTensor Loc num =%d  err %x!\n", i, s32Ret);
            return s32Ret;
        }
        //�õ�����ĵ�ַ

        pf32Addr = (VIM_F32*)malloc(stOutTensor.stBufInfo[0].u32BufSize * 4);
        if (pf32Addr == VIM_NULL)
        {
            printf("malloc output buffer failed!\n");
            return s32Ret;
        }
        pLocAddr[i] = pf32Addr;
        NPU_MPI_GetFloatData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
                 stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, pf32Addr);
        
        //VIM_F32 *tmp = pf32Addr;
        // std::vector<float> toutdata;
        // for(int j = 0; j < stOutTensor.stBufInfo[0].u32BufSize; j++)
        // {
        //     toutdata.emplace_back(*tmp);
        //     tmp++;
        // }
        // outpoutdata.emplace_back(toutdata);
        net_out_blob_ outb;
        outb.data = pf32Addr;
        outb.c = stOutTensor.stBufInfo[0].u32C;
        outb.h = stOutTensor.stBufInfo[0].u32H;
        outb.w = stOutTensor.stBufInfo[0].u32W;
        net_out_blob.emplace_back(outb);
        
    }

    s32Ret = NPU_MPI_DestroyTask(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_DestroyTask err %x!\n", s32Ret);
        return s32Ret;
    }
    for(i = 0; i < OutputNodeNumber; i++)
    {
        free(pLocAddr[i]);
    }
    return s32Ret;

#else

    // VIM_S32 s32Ret = VIM_SUCCESS;
    // VIM_U32 i = 0;
    // //VIM_S16 *pLocAddr[6] = {VIM_NULL};
    // NPU_TASK_NODE_S stTaskNode;
    // VIM_S16 *ps16Addr;
    // NPU_INFO_S stOutTensor;
    // VIM_U32 u32LocNum = 0;

    // //LOC ����������
    // //u32LocNum = mparam.output_node.size();

    // //�ȴ����н��
    // s32Ret = NPU_MPI_GetRunDone(g_stNetId.u64NId, &stTaskNode);
    // if (s32Ret != VIM_SUCCESS)
    // {
    //     printf("NPU_MPI_GetResult err %x!\n\r", s32Ret);
    //     return s32Ret;
    // }

    // // NPU_MPI_ToolDump(&stTaskNode, "npu_dump", 6, 0, 300);

    // //��ȡ3��sigmoid��������Ϣ
    // this->gOutputNodeNumber = OutputNodeNumber;
    // for(i = 0; i < OutputNodeNumber; i++)
    // {
    //     s32Ret =  NPU_MPI_GetTaskOutTensor(&stTaskNode, (VIM_CHAR*)NodeNames[i].c_str(), &stOutTensor);
    //     if (s32Ret != VIM_SUCCESS)
    //     {
    //         printf("NPU_MPI_GetTaskOutTensor Loc num =%d  err %x!\n", i, s32Ret);
    //         return s32Ret;
    //     }
    //     //�õ�����ĵ�ַ
    //     int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
    //     if(pLocAddr[i] == VIM_NULL)
    //     {
    //         pLocAddr[i] = (VIM_S16*)malloc(fix_size);
    //         if (pLocAddr[i] == VIM_NULL)
    //         {
    //             printf("malloc output buffer failed!\n");
    //             return s32Ret;
    //         }

    //     }

    //     //pLocAddr[i] = ps16Addr;
    //     VIM_U16 pbTransposed = 0;
    //     VIM_S32 ps32ShiftBits = 0;
    //     float pfScale = 0.0;

    //     NPU_MPI_GetFixedData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
    //         stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, 
    //         pLocAddr[i], &pbTransposed, &ps32ShiftBits,&pfScale); 
        
    //     float fTmpVal;
    //     if (ps32ShiftBits >= 0){
    //         fTmpVal = (float)1.0 / (1 << ps32ShiftBits);
    //     }   
    //     else{
    //         fTmpVal = (float)(1 << (0 - ps32ShiftBits));
    //     }
        
    //     VIM_S16 *tmp = pLocAddr[i];
    //     std::vector<VIM_S16> toutdata;
    //     for(int j = 0; j < stOutTensor.stBufInfo[0].u32BufSize; j++)
    //     {
    //         toutdata.emplace_back(*tmp);
    //         tmp++;
    //     }
    //     outpoutdata.emplace_back(toutdata);

    //     net_out_blob_ outb;
    //     outb.data = pLocAddr[i];
    //     outb.c = stOutTensor.stBufInfo[0].u32C;
    //     outb.h = stOutTensor.stBufInfo[0].u32H;
    //     outb.w = stOutTensor.stBufInfo[0].u32W;
    //     net_out_blob.emplace_back(outb);
        
    // }

    // s32Ret = NPU_MPI_DestroyTask(g_stNetId.u64NId, &stTaskNode);
    // if (s32Ret != VIM_SUCCESS)
    // {
    //     printf("NPU_MPI_DestroyTask err %x!\n", s32Ret);
    //     return s32Ret;
    // }

    // return s32Ret;

    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;
    NPU_TASK_NODE_S stTaskNode;
    NPU_INFO_S stOutTensor;
    this->gOutputNodeNumber = OutputNodeNumber;
    this->gBits = Bits;

    s32Ret = NPU_MPI_GetRunDone(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetResult err %x!\n\r", s32Ret);
        return s32Ret;
    }
    else
    {
        printf("NPU_MPI_GetRunDone ok\n");
    }

    std::vector<net_out_blob_>().swap(net_out_blob);
    for(i = 0; i < OutputNodeNumber; i++)
    {
    	//printf("get outlayer %s\n", params.outout_loc[i]);
        s32Ret =  NPU_MPI_GetTaskOutTensor(&stTaskNode, (VIM_CHAR*)NodeNames[i].c_str(), &stOutTensor);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("NPU_MPI_GetTaskOutTensor Loc num =%d  err %x!\n", i, s32Ret);
            return s32Ret;
        }

		if (Bits == 32)
		{
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
	        printf("fix_size is %d\n",fix_size);
            if(pLocAddr_S16[i] == VIM_NULL)
            {
                printf("*/*/*/***/\n");
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
	        
	        net_out_blob_ nob;         
	        nob.data = pLocAddr_S16[i];
            nob.c = stOutTensor.stBufInfo[0].u32C;
            nob.h = stOutTensor.stBufInfo[0].u32H;
            nob.w = stOutTensor.stBufInfo[0].u32W;
            net_out_blob.emplace_back(nob);
            
            printf("H=%d, W=%d, C=%d\n", stOutTensor.stBufInfo[0].u32H, stOutTensor.stBufInfo[0].u32W, stOutTensor.stBufInfo[0].u32C);
	        
	        if (scale_flag == 0)
	        {
		        if (s32ShiftBits >= 0)
		        {
		            FixScale = (float)1.0 / (1 << s32ShiftBits);
		        }   
		        else
		        {
		            FixScale = (float)(1 << (0 - s32ShiftBits));
		        }
		        
		        scale_flag = 1;
	        }
        }
        else if (Bits == 8)
        {
        	int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
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
	        
	        net_out_blob_ nob;         
	        nob.data = pLocAddr_U8[i];
            nob.c = stOutTensor.stBufInfo[0].u32C;
            nob.h = stOutTensor.stBufInfo[0].u32H;
            nob.w = stOutTensor.stBufInfo[0].u32W;
            net_out_blob.emplace_back(nob);
	        
	        if (scale_flag == 0)
	        {
		        FixScale = pfScale;
		        scale_flag = 1;
	    	}
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

#endif
}

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int class_label;
    float confidence;
} zxw_object_info_p;

class detect_yolop{

    public:
        Net *pNet;
 
    
        detect_yolop();
        ~detect_yolop();
        VIM_S32 detect_yolop_load_model(const char *modelFile);
        VIM_S32 detect_yolop_input(unsigned char* data, int width, int height, int channel, const char *inputnode);
        VIM_S32 detect_yolop_input(cv::Mat img, int width, int height, int channel, const char *inputnode);
        VIM_S32 detect_yolop_inference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits);
        VIM_S32 detect_yolop_get_result(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<zxw_object_info_p> &objects);
    
    private:
        std::vector<std::vector<float>> outpoutdata;
        unsigned char* gdata;
        float FScale;

};

const std::vector<std::vector<float>> anchors = {
    {1.7591,  4.0912, 2.6935,  6.1736, 4.0721, 11.3594}, {0.9321,  2.3855, 1.4359,  3.3805, 2.1728,  5.1573}, {0.5067,  1.4337, 0.8076,  2.1085,  1.1406,  3.1967}};

static void generate_proposals16(VIM_S16* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<zxw_object_info_p> &objects, int classnumber, float FScale)
{
    VIM_S16  *pSrcAddr;
    int num_elem = 5 + classnumber;
    int offnum = num_elem*3 + 8 - ((num_elem*3)%8);

    float s, tmp;
    float score, max;
    int idx_max = 0;
    int fixscorethres = int(0.3/FScale);

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
				
				std::cout<<"x0:"<<x0<<"x1:"<<x1<<"y0:"<<y0<<"y1:"<<y1<<std::endl;

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

    float s, tmp;
    float score, max;
    int idx_max = 0;
    int fixscorethres = int(0.3/FScale);

    pSrcAddr = output_ptr;

    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
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

static void generate_proposals(int8_t* output_ptr, float scale, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<zxw_object_info_p> &objects, int classnumber)
{
    int channelC = classnumber + 5;
    for (int w = 0; w < W; w++) 
    {
        for (int h = 0; h<H; h++) 
        {
            for(int a = 0; a < 3; a++)
            {
            //     int stride = w*H*C + h*C + channelC*a;
            //     //int val_i8 = output_ptr[stride + 4];
            //     //float val_f32 = val_i8 * scale;
            //     float fgrdconfidence = Table[output_ptr[stride + 4]+128]; //sigmoid(val_f32);
            //     int k;
            //     float maxval = 0.0;
            //     int index;
            //     float classconfidence;
            //     for(k = 5; k < channelC; k ++)
            //     {
            //         //val_i8 = output_ptr[stride + k];
            //         //val_f32 = val_i8 * scale;
            //         classconfidence = Table[output_ptr[stride + k]+128]; //sigmoid(val_f32);
            //         if(classconfidence > maxval)
            //         {
            //             maxval = classconfidence;
            //             index = k-5;
            //         }

            //     }
            //     float allconfidence = fgrdconfidence*maxval;
            //     if(allconfidence > objconfidence)
            //     {
            //         //printf("*****************\n");
                    
            //         // float dx1 = output_ptr[stride + 0] * scale;
            //         // float dy1 = output_ptr[stride + 1] * scale;
            //         // float dw1 = output_ptr[stride + 2] * scale;
            //         // float dh1 = output_ptr[stride + 3] * scale;

            //         // float dx = sigmoid(dx1);
            //         // float dy = sigmoid(dy1);
            //         // float dw = sigmoid(dw1);
            //         // float dh = sigmoid(dh1);

            //         float dx = Table[output_ptr[stride + 0]+128];
            //         float dy = Table[output_ptr[stride + 1]+128];
            //         float dw = Table[output_ptr[stride + 2]+128];
            //         float dh = Table[output_ptr[stride + 3]+128];


            //         dx = (dx * 2.0f - 0.5f + (float)w) * downsample;
            //         dy = (dy * 2.0f - 0.5f + (float)h) * downsample;
            //         dw = (dw * 2.0f) * (dw * 2.0f) * anchors[anchorindex][2*a];
            //         dh = (dh * 2.0f) * (dh * 2.0f) * anchors[anchorindex][2*a+1];

            //         float x0 = dx - (dw - 1.0f) * 0.5f;
            //         float x1 = dx + (dw - 1.0f) * 0.5f;
            //         float y0 = dy - (dh - 1.0f) * 0.5f;
            //         float y1 = dy + (dh - 1.0f) * 0.5f;

            //         int x = x0;
            //         if(x < 0) x = 0;
            //         int y = y0;
            //         if(y < 0) y = 0;
            //         int width = x1 - x0;
            //         if(width < 0) width = 0;
            //         int height = y1 - y0;
            //         if(height < 0) height = 0;


            //         zxw_object_info obj;
            //         obj.x = x;
            //         obj.y = y;
            //         obj.width = width;
            //         obj.height = height;
            //         obj.class_label = index;
            //         obj.confidence = allconfidence;
            //         objects.push_back(obj);

            //     }

            }

        }
    }

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
        delete pNet;
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


VIM_S32 detect_yolop::detect_yolop_get_result(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<zxw_object_info_p> &objects)
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
            tmpf = (VIM_F32 *)pNet->net_out_blob[i].data;

        }else if (Bits == 16)
        {
            tmps = (VIM_S16 *)pNet->net_out_blob[i].data;
            printf("FScale=%f\n", FScale);

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
                printf("H=%d, W=%d, C=%d\n", H, W, C);
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {
						
						continue;
						//if(Bits == 16)
                        //    {
						//		printf("16 bit model 0: %f\n", tmpf[h * W*16+ w*16 + 0]*FScale);
                        //       printf("16 bit model 1: %f\n", tmpf[h * W*16+ w*16 + 1]*FScale);

                        //    }
						/*
                        for(int c = 0; c < C; c++)
                        {
                            if(Bits == 32)
                            {
                                printf("%f\n", tmpf[c]);

                            }else if (Bits == 16)
                            {
                                //printf("%f\n", tmps[c]*FScale);
                            }else if (Bits == 8)
                            {
                                printf("%f\n", tmpu[c]*FScale);
                            }
                        }
						*/
                    }
                        
                }
            }

            
        }else if (Bits == 8)
        {
            tmpu = (VIM_U8 *)pNet->net_out_blob[i].data;
            printf("FScale=%f\n", FScale);

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


            
        }    
    }

    return s32Ret;
}


using namespace std;





int main()
{
    detect_yolop *pyolop = new detect_yolop();
    pyolop->detect_yolop_load_model("model/yolop-det-ll-640-640.npumodel");
    cv::Mat test_img = cv::imread("./yolop.jpg");
    cv::resize(test_img, test_img, cv::Size(640, 640));
    pyolop->detect_yolop_input(test_img, 640, 640, 3, "input0");
    std::vector<std::string> NodeNames;
    NodeNames.push_back("Sigmoid160");
    NodeNames.push_back("Sigmoid162");
    NodeNames.push_back("Sigmoid164");
    NodeNames.push_back("Sigmoid202");
    
    pyolop->detect_yolop_inference(4, NodeNames, 16);
    std::vector<zxw_object_info_p> objs;
    pyolop->detect_yolop_get_result(16, 1, 640, 640, 0.3, objs);
    printf("finish inference\n");

    pyolop->~detect_yolop();
    delete pyolop;
    pyolop = nullptr;

    return 0;
}
