#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "sample_comm.h"
#include "npu_alg.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>   // /usr/include/dirent.h
#include<string.h>
#include<iostream>
#include <vector>
using namespace std;

#define QUEUE_DEEP          3

#define RGB 0
#define BGR 1

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
S_MODELPARAM mparam;

VIM_U64    g_u64NId = -1;
NPU_INFO_S g_stInfo;
VIM_U32 g_u32TotalNum = 0;
VIM_U32 gRunPicNum = 1;

vector<string> images_test;
unsigned char* readimg = NULL;//data format on npu input:rrrgggbbb or bbbgggrrr
int images_test_ptr = 0;
cv::Mat imagein;
cv::Mat image;
int sw, sx, sy;

std::vector<cv::Mat> v_saveimg;
std::vector<std::string> v_imgsavename;

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

// 输入图像的长宽比填充函数
void getBorderImage(cv::Mat image_org, cv::Mat& image, int* sw, int* sx, int* sy, int w, int h)
{
    int need_rows = image_org.cols * h / w;
    if (need_rows < image_org.rows)
    {
        int need_cols = image_org.rows * w / h;
        int half_w = int((need_cols - image_org.cols) / 2);
        cv::copyMakeBorder(image_org, image, 0, 0, half_w, half_w, cv::BORDER_CONSTANT);

        *sw = image_org.rows;
        *sx = half_w;
        *sy = 0;
    }
    else
    {
        int half_h = int((need_rows - image_org.rows) / 2);
        cv::copyMakeBorder(image_org, image, half_h, half_h, 0, 0, cv::BORDER_CONSTANT);

        *sw = image_org.cols;
        *sx = 0;
        *sy = half_h;
    }
}

int read_image_file(std::string imgfilename, unsigned char* img, int format, int w, int h, int* sw, int* sx, int* sy)
{
    imagein = cv::imread(imgfilename);
    int ch_size = w * h;
    unsigned char* r = img;
    unsigned char* g = img + ch_size;
    unsigned char* b = img + ch_size*2;

    if (imagein.cols == 0 || imagein.rows == 0)
    {
        printf("image [%s] read failed\n", imgfilename.c_str());
        return -1;
    }
    
    int len = imgfilename.length();
    char* src = (char*)(imgfilename.c_str());
    int pos = -1;
    for (int i=len-1; i>0; i--)
    {
        if (src[i] == '\\' || src[i] =='/')
        {
            pos = i+1;
            break;
        }
    }

    char cfname[64];
    memcpy(cfname, src+pos, len-pos+1);
    std::string str_cfname = cfname;
    std::string savename = "./result/"+str_cfname;
    //printf("savename=%s\n", savename.c_str());

    cv::Mat model_img(h, w, CV_8UC3);
    if (imagein.cols == w && imagein.rows == h)
    {
        model_img = imagein;
        image = imagein;
        *sx = 0;
        *sy = 0;
    }
    else
    {
        getBorderImage(imagein, image, sw, sx, sy, w, h);
        cv::resize(image, model_img, cv::Size(w, h));
    }
    //printf("img [in w:%d h:%d][new w:%d h:%d sx:%d sy:%d][resize w:%d h:%d]\n", imagein.cols, imagein.rows, image.cols, image.rows, sx, sy, model_img.cols, model_img.rows);

    cv::Mat vputimg = imagein.clone();
    v_saveimg.push_back(vputimg);
    v_imgsavename.push_back(savename);

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

void SAMPLE_resultProcess(VIM_F32 *pLocAddr[], VIM_S32* len, VIM_U32 u32Lable)
{
#if 1
	cv::Mat saveimg1 = v_saveimg[u32Lable];  //输入的原始图像的矩阵数据
    std::string imgsavename1 = v_imgsavename[u32Lable];  //输入图像的名称
	
    VIM_F32* result = pLocAddr[0];  //预测结果数据
    VIM_S32 x, y;                
    VIM_S32 imgw = saveimg1.cols;   //输入的原始图像的宽
    VIM_S32 imgh = saveimg1.rows;   //输入的原始图像的高
    
    FILE* fop = fopen("./result/result.txt", "a+");
    if (fop)
    {
        fprintf(fop, "No %d: %s", u32Lable+1, imgsavename1.c_str());
    } 

    for (VIM_S32 i=0; i<len[0]/2; i++)  //循环关键点坐标的数据
    {
        x = result[i*2] * imgw - sx;   //获取横坐标信息，将预测的像素坐标减去边框填充的坐像素值，获得实际预测的坐标信息
        y = result[i*2+1] * imgh - sy; //获取纵坐标信息
        //printf("[%d]%d,%d\n", i, x, y);
        cv::circle(saveimg1, cv::Point(x,y), 1, cv::Scalar(0,0,255), 1); //在原图上绘制坐标点信息
        if (fop)
        {
            fprintf(fop, ",%d,%d", x,y);
        } 
    }

    if (fop)
    {
        fprintf(fop, "\n");
        fclose(fop);
    }

    cv::imwrite(imgsavename1, saveimg1);
    saveimg1.release();
#endif

    return;
}


VIM_S32 SAMPLE_MODEL_Predict(VIM_U64 u64NId)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    NPU_TASK_NODE_S stTaskNode;
    VIM_U32 u32Label;
    NPU_INFO_S stInTensor;

    s32Ret = read_image_file(images_test[images_test_ptr], readimg, mparam.format, mparam.width, mparam.height, &sw, &sx, &sy);
    if (s32Ret != 0)
        return s32Ret;

    s32Ret = NPU_MPI_CreateTask(u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_CreateTask err %x!\n", s32Ret);
        return s32Ret;
    }

    //获取输入层的地址
    s32Ret =  NPU_MPI_GetTaskInTensor(&stTaskNode, (VIM_CHAR*)mparam.input_node.c_str(), &stInTensor);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetTaskInTensor err %x!\n", s32Ret);
        return s32Ret;
    }

    //填充输入数据
    {
        int size = mparam.width*mparam.height*3;
        VIM_U8 *pu8In = stInTensor.stBufInfo[0].pu8Vaddr;
        memcpy(pu8In, readimg, size);
        u32Label = images_test_ptr;
        images_test_ptr++;
    }
    //提交一个任务

    stTaskNode.u32Private = u32Label;
    s32Ret = NPU_MPI_Predict(u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_Predict err %x!\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}


VIM_S32 SAMPLE_MODEL_GetResult(VIM_U64 u64NId)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;
    VIM_F32 *pLocAddr[6] = {VIM_NULL};
    NPU_TASK_NODE_S stTaskNode;
    VIM_F32 *pf32Addr;
    NPU_INFO_S stOutTensor;
    VIM_U32 u32LocNum = 0;

    //LOC 输出层的名字
    u32LocNum = mparam.output_node.size();

    //等待运行结果
    s32Ret = NPU_MPI_GetRunDone(u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetResult err %x!\n\r", s32Ret);
        return s32Ret;
    }

    // NPU_MPI_ToolDump(&stTaskNode, "npu_dump", 6, 0, 300);

    //获取3个sigmoid层的输出信息
    for(i = 0; i < u32LocNum; i++)
    {
        s32Ret =  NPU_MPI_GetTaskOutTensor(&stTaskNode, (VIM_CHAR*)mparam.output_node[i].c_str(), &stOutTensor);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("NPU_MPI_GetTaskOutTensor Loc num =%d  err %x!\n", i, s32Ret);
            return s32Ret;
        }
        //得到浮点的地址

        pf32Addr = (VIM_F32*)malloc(stOutTensor.stBufInfo[0].u32BufSize * 4);
        if (pf32Addr == VIM_NULL)
        {
            printf("malloc output buffer failed!\n");
            return s32Ret;
        }
        pLocAddr[i] = pf32Addr;
        NPU_MPI_GetFloatData(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId,
                 stOutTensor.stBufInfo[0].enDir, stOutTensor.stBufInfo[0].u32BufId, pf32Addr);
        /*printf("xxx i = %d ,loc name =%s , featurenum =%d, w =%d, h=%d \n",
                           i, (char*)mparam.output_node[i].c_str(), stOutTensor.stBufInfo[0].u32FeatureNum, stOutTensor.stBufInfo[0].u32Col, stOutTensor.stBufInfo[0].u32Row);*/

    }

    int len = 98 * 2;
    SAMPLE_resultProcess(pLocAddr, &len, stTaskNode.u32Private);

    s32Ret = NPU_MPI_DestroyTask(u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_DestroyTask err %x!\n", s32Ret);
        return s32Ret;
    }
    for(i = 0; i < u32LocNum; i++)
    {
        free(pLocAddr[i]);
    }
    return s32Ret;
}

VIM_VOID * SAMPLE_MODEL_PredictProc(VIM_VOID *p)
{
#if defined(ARM_LINUX)
    fd_set fsetw;
    VIM_S32 s32Fd;
    struct timeval stTimeoutVal;
    s32Fd = NPU_MPI_GetNetFd(g_u64NId, NPU_FD_OP_W);
    VIM_U32 iCounter = 0;
    while(iCounter < gRunPicNum)
    {
        FD_ZERO(&fsetw);
        FD_SET(s32Fd, &fsetw);
        stTimeoutVal.tv_sec  = 60;
        stTimeoutVal.tv_usec = 0;
        VIM_S32 s32Ret = select(s32Fd+1, NULL, &fsetw, NULL, &stTimeoutVal);
        if (s32Ret <= 0)
        {
            printf("SAMPLE_MODEL_Predict select timeout!  s32Ret %d\n",s32Ret);
            break;
        }
        s32Ret = SAMPLE_MODEL_Predict(g_u64NId);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("SAMPLE_MODEL_Predict err %x!\n", s32Ret);
            break;
        }

        if(++iCounter==g_u32TotalNum)
        {
            printf("Predict over!\n");
            break;
        }
    }
#endif
    return NULL;
}

VIM_VOID * SAMPLE_MODEL_GetResultProc(VIM_VOID *p)
{
#if defined(ARM_LINUX)
    fd_set fsetr;
    VIM_S32 s32Fd;
    struct timeval stTimeoutVal;
    s32Fd = NPU_MPI_GetNetFd(g_u64NId, NPU_FD_OP_R);
    VIM_U32 iCounter = 0;
    while(iCounter < gRunPicNum)
    {
        FD_ZERO(&fsetr);
        FD_SET(s32Fd, &fsetr);
        stTimeoutVal.tv_sec  = 60;
        stTimeoutVal.tv_usec = 0;

        VIM_S32 s32Ret = select(s32Fd+1, &fsetr, NULL, NULL, &stTimeoutVal);
        if (s32Ret <= 0)
        {
            printf("SAMPLE_MODEL_GetResultProc select timeout! s32Ret %d\n",s32Ret);
            break;
        }
        s32Ret = SAMPLE_MODEL_GetResult(g_u64NId);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("SAMPLE_MODEL_GetResult err %x!\n", s32Ret);
            break;
        }
        if(++iCounter==g_u32TotalNum)
        {
            printf("GetResult over!\n");
            break;
        }
    }
#endif
    return NULL;
}


/******************************************************************************
* function : model sample run func
******************************************************************************/
VIM_VOID SAMPLE_MODEL_Run()
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    /******************************************
     step 1: init mpi system
    ******************************************/
    s32Ret = NPU_MPI_Init();
    if (VIM_SUCCESS != s32Ret)
    {
        printf("NPU_MPI_Init fail! s32Ret = 0x%x\n", s32Ret);
        return;
    }

    /******************************************
     step 2: create net work
    ******************************************/
    int u32Encrypt = 0;
    s32Ret = NPU_MPI_CreateNetWork(&g_u64NId, (VIM_CHAR*)mparam.modelpath.c_str(), QUEUE_DEEP,u32Encrypt);
    if (VIM_SUCCESS != s32Ret)
    {
        printf("NPU_MPI_CreatNetWork fail! s32Ret = 0x%x\n", s32Ret);
        NPU_MPI_UnInit();
        return;
    }

    
	s32Ret = NPU_MPI_GetInfo(g_u64NId, &g_stInfo);
    if (VIM_SUCCESS != s32Ret)
	{
		printf("NPU_MPI_GetInfo fail, there is no any input or output! s32Ret = 0x%x\n", s32Ret);
		return ;
	}

    /******************************************
     step 3: predict and get result
    ******************************************/
#if defined(ARM_LINUX)
    pthread_t pthreadset,pthreadget;
    s32Ret = pthread_create(&pthreadset,NULL,SAMPLE_MODEL_PredictProc,NULL);
    s32Ret = pthread_create(&pthreadget,NULL,SAMPLE_MODEL_GetResultProc,NULL);

    pthread_join(pthreadset,NULL);
    pthread_join(pthreadget,NULL);
#else
    while (s32Cnt < gRunPicNum)
    {
    	printf("No%d\n",s32Cnt);
        s32Ret = SAMPLE_MODEL_Predict(g_u64NId);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("SAMPLE_MODEL_Predict err %x!\n", s32Ret);
            break;
        }

        s32Ret = SAMPLE_MODEL_GetResult(g_u64NId);
        if (s32Ret != VIM_SUCCESS)
        {
            printf("SAMPLE_MODEL_GetResult err %x!\n", s32Ret);
            break;
        }
   
        s32Cnt++;
    }
#endif


    /******************************************
     step 5: FREE memory,destroy net,uninit
    ******************************************/

    NPU_MPI_DestroyNetWork(g_u64NId); 
    NPU_MPI_UnInit();    
    return;
}

void SAMPLE_MODEL_Usage(char *sPrgNm)
{
    printf( "%s {<options>}\n",sPrgNm);
    printf( "help                   :display this help\n");
    printf( "num=<int>              :is unnessesary, if enter a value for picnum, run picnum times, default run all pic\n");
    return;
}

void init_model_param(S_MODELPARAM& param)
{
    param.modelpath = "model/landmark_98_deploy.npumodel";
    param.width = 112;
    param.height = 112;
    param.input_node = "data";
    param.output_node.clear();
    param.output_node.push_back("fc1");

    param.format = BGR;
    param.with_nms = 0;

 	readimg = (unsigned char*)malloc(param.width*param.height*3);
}
/******************************************************************************
* function    : main()
* Description : YOLO sample
******************************************************************************/
int main(int argc, char* argv[])
{
    init_model_param(mparam);

    if (SAMPLE_COMM_CheckCmdLineFlag(argc, (const char **)argv, "help"))
   	{
        SAMPLE_MODEL_Usage(argv[0]);
        return -1;
    }

    if (SAMPLE_COMM_CheckCmdLineFlag(argc, (const char **)argv, "num"))
   	{
        gRunPicNum = SAMPLE_COMM_GetCmdLineArgumentInt(argc, (const char **)argv, "num");
    }

    {
        getAbsoluteFiles("./images/",  images_test);
        g_u32TotalNum = images_test.size();

        if (g_u32TotalNum == 0)
        {
            printf("there is no jpg file in ./images folder\n");
            return -1;
        }
    }

    system("rm ./result -rf");
    system("mkdir result");

    SAMPLE_MODEL_Run();

    if (readimg)
        free(readimg);
    return 0;
}

