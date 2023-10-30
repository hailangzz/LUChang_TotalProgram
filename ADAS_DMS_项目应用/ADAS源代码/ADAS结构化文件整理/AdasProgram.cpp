#include "AdasProgram.h"

void QsortDescentInplace(std::vector<ObjectDetectInfo> &objects, int left, int right) 
{
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

void QsortDescentInplace(std::vector<ObjectDetectInfo> &objects) 
{
    if (objects.empty()) return;
    qsort_descent_inplace(objects, 0, objects.size() - 1);
}

static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1, float ymax1) {
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1));
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1));
    float i = w * h;
    float u = (xmax0 - xmin0) * (ymax0 - ymin0) + (xmax1 - xmin1) * (ymax1 - ymin1) - i;
    return u <= 0.f ? 0.f : (i / u);
}


void DetectNms(std::vector<ObjectDetectInfo> &objects, std::vector<size_t> &picked, float nms_confidence) 
{
    picked.clear();
    const size_t n = objects.size();

    for (size_t i = 0; i < n; ++i) {
        const ObjectDetectInfo &object_a = objects[i];
        int keep = 1;

        for (size_t j = 0; j < picked.size(); ++j) {
            const ObjectDetectInfo &object_b = objects[picked[j]];
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

// sigmoid激活函数
static float Sigmoid(float x)
{
	return 1.0 / (1.0 + exp(-x));
}

// 读取本都图片数据
static int ReadImageFile(cv::Mat imagein, unsigned char* img, int format, int w, int h)
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


int GetAbsoluteFiles(string directory, vector<string>& filesAbsolutePath) 
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

//为原始图片绘制目标框，及粘贴分割图层
void PlotObjectBoxLaneMask(cv::Mat BaseImage, cv::Mat MarkImage, std::vector<ObjectDetectInfo> objects,cv::Mat &segmentedImage) 
{
    
    segmentedImage = BaseImage.clone();  // 创建一个副本以保存结果
    //std::cout << MarkImage << std::endl;
    cv::Vec3b color = cv::Vec3b(0, 0, 255); //标记为红色

    for (int y = 0; y < MarkImage.rows; y++) {
        for (int x = 0; x < MarkImage.cols; x++) {            
            // 获取标记信息像素值
            uchar label = MarkImage.at<uchar>(y, x);
            
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


// yolop网络类成员函数定义

YolopNet::YolopNet()
{
    scale_flag = 0;
	
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
	 
	
}

YolopNet::~YolopNet() 
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

VIM_S32 YolopNet::YolopLoadModel(const char *model_path) 
{
	/******************************************
     step 2: create net work
    ******************************************/
    int u32Encrypt = 0;
    g_stNetId.u64NId = -1;
    s32Ret = NPU_MPI_CreateNetWork(&g_stNetId.u64NId, (VIM_CHAR*)modelFile, 1, u32Encrypt);
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

VIM_S32 YolopNet::YolopInput(unsigned char* data, int width, int height, int channel, const char *inputnode)
{
	VIM_S32 s32Ret = VIM_SUCCESS;
    NPU_TASK_NODE_S stTaskNode;
    VIM_U32 u32Label;
    NPU_INFO_S stInTensor;

	//创建npu网络任务
    s32Ret = NPU_MPI_CreateTask(g_stNetId.u64NId, &stTaskNode);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_CreateTask err %x!\n", s32Ret);
        return s32Ret;
    }

    //获取网络输入节点信息
    s32Ret =  NPU_MPI_GetTaskInTensor(&stTaskNode, (VIM_CHAR*)inputnode, &stInTensor);
    if (s32Ret != VIM_SUCCESS)
    {
        printf("NPU_MPI_GetTaskInTensor err %x!\n", s32Ret);
        return s32Ret;
    }

    //拷贝输入矩阵到tensor中
    {
        int size = width*height*channel;
        VIM_U8 *pu8In = stInTensor.stBufInfo[0].pu8Vaddr;
        memcpy(pu8In, data, size);
        
    }


    //npu前向推理
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


VIM_S32 YolopNet::YolopInference(int OutputNodeNumber, std::vector<std::string> NodeNames, int Bits, float &FixScale)
{

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

    std::vector<NetOutBlobInfo>().swap(net_out_blob);
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
			int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
	        printf("FP32 NPU_MPI_GetFixedOutputSize is %d\n",fix_size);
			
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
	        NetOutBlobInfo nob;         
	        nob.data = pLocAddr_F32[i];
            nob.channel = stOutTensor.stBufInfo[0].u32C;
            nob.height = stOutTensor.stBufInfo[0].u32H;
            nob.width = stOutTensor.stBufInfo[0].u32W;
            net_out_blob.emplace_back(nob);
        }
        else if (Bits == 16)
        {
        	int fix_size = NPU_MPI_GetFixedOutputSize(&stTaskNode, stOutTensor.stBufInfo[0].u32LayerId);
	        //printf("fix_size is %d\n",fix_size);
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
	        
	        NetOutBlobInfo nob;         
	        nob.data = pLocAddr_S16[i];
            nob.channel = stOutTensor.stBufInfo[0].u32C;
            nob.height = stOutTensor.stBufInfo[0].u32H;
            nob.width = stOutTensor.stBufInfo[0].u32W;
            nob.scale = FixScale;
            net_out_blob.emplace_back(nob);
            
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
	        
            printf("s32ShiftBits=%d\n", s32ShiftBits);
            printf("pfScale=%f\n", pfScale);
            if (s32ShiftBits >= 0)
            {
                FixScale = (float)1.0 / (1 << s32ShiftBits);
            }   
            else
            {
                FixScale = (float)(1 << (0 - s32ShiftBits));
            }
            FixScale = pfScale;
	        NetOutBlobInfo nob;         
	        nob.data = pLocAddr_U8[i];
            nob.channel = stOutTensor.stBufInfo[0].u32C;
            nob.height = stOutTensor.stBufInfo[0].u32H;
            nob.width = stOutTensor.stBufInfo[0].u32W;
            nob.scale = FixScale;
            net_out_blob.emplace_back(nob);
	        

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

static void GenerateProposals32(VIM_F32* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale)
{
	
	std::cout<<"FScale:"<<FScale<<std::endl;
    VIM_F32  *pSrcAddr;
    int num_elem = 5 + classnumber;
    //int offnum = num_elem*3 + 4 - ((num_elem*3)%4);
	int offnum = 6;

    float s, tmp;
    float score, max;
    int idx_max = 0;
    //int fixscorethres = int(0.3/FScale); //objconfidence
	//int fixscorethres = objconfidence;
	int fixscorethres = 0.35;
	
    pSrcAddr = output_ptr;

	
    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {			
				
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
                
                if (score <= fixscorethres) {
                    count_num++;
                    continue;
                }
                max = 0;
                for (int n = 0; n < classnumber; n++) {
                    s =  float(pSrcAddr[count_num*offnum + m*num_elem + 5 + n])* 1;
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
				
                float dx = (float(pSrcAddr[count_num*offnum + m*num_elem + 0]) * 1 * 2 - 0.5 + k) * downsample;
                float dy = (float(pSrcAddr[count_num*offnum + m*num_elem + 1]) * 1 * 2 - 0.5 + j) * downsample;
               
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 2]) * 1 * 2;
                float dw = tmp * tmp * anchors[anchorindex][2*m];
                tmp = float(pSrcAddr[count_num*offnum + m*num_elem + 3]) * 1 * 2;
                float dh = tmp * tmp * anchors[anchorindex][2*m+1];       
                float confidence = float(s) * 1 ;
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


                ObjectDetectInfo obj;
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

static void GenerateProposals16(VIM_S16* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale)
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
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];                
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


                ObjectDetectInfo obj;
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

static void GenerateProposals8(VIM_U8* output_ptr, int W, int H, int C, int downsample, int anchorindex, float objconfidence, std::vector<ObjectDetectInfo> &objects, int classnumber, float FScale)
{
    VIM_U8  *pSrcAddr;
    int num_elem = 5 + classnumber;
    int offnum =num_elem*3 + 16 - ((num_elem*3)%16);
    float s, tmp;
    float score, max;
    int idx_max = 0;
    int fixscorethres = int(objconfidence/FScale);
    printf("fixscorethres=%d FScale=%f\n",fixscorethres,FScale);
    pSrcAddr = output_ptr;

    for (int m = 0; m < 3; m++) {
        int count_num = 0;
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < W; k++) {
                score = pSrcAddr[count_num*offnum + m*num_elem + 4];
                
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


                ObjectDetectInfo obj;
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


VIM_S32 YolopNet::YolopGetResult(int Bits, int classnumber, int mode_input_img_w, int mode_input_img_h, float objconfidence, std::vector<ObjectDetectInfo> &objects,cv::Mat &mark_images)
{
	
    VIM_S32 s32Ret = VIM_SUCCESS;
    std::vector<ObjectDetectInfo> proposals;
    for(int i  = 0; i < this->net_out_blob.size(); i++)
    {
        int H = this->net_out_blob[i].height;
        int W = this->net_out_blob[i].width;
        int C = this->net_out_blob[i].channel;
        printf("H=%d, W=%d, C=%d\n", H, W, C);

        VIM_F32 *tmpf;
        VIM_S16 *tmps;
        VIM_U8 *tmpu;
        if(Bits == 32)
        {
			tmpf=(VIM_F32 *)this->net_out_blob[i].data;
			FScale = this->net_out_blob[i].scale;
			printf("FScale_32=%f\n", FScale);
			
			int downsample = mode_input_img_w / W;
            if(downsample == 32)
            {
                GenerateProposals32(tmpf, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                GenerateProposals32(tmpf, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                GenerateProposals32(tmpf, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
            }
			if(downsample == 1)
				{
                printf("FP32 mark info H=%d, W=%d, C=%d\n", H, W, C);
                cv::Mat GreyBack = cv::Mat::zeros(384, 640,CV_8UC1);
				
				int mask_value_index = 0;
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {
                        
						
						if (tmpf[mask_value_index] < 0.5)
						{
							GreyBack.at<uchar>(h,w)=255;
							mark_images.at<uchar>(h,w)=255;
						}
						else
						{
							GreyBack.at<uchar>(h,w)=0;
							mark_images.at<uchar>(h,w)=0;
						}
											
						
                    }   
                }


            }

        }else if (Bits == 16)
        {
			tmps = (VIM_S16 *)this->net_out_blob[i].data;
			FScale = this->net_out_blob[i].scale;
            printf("FScale_16=%f\n", FScale);

            int downsample = mode_input_img_w / W;
            if(downsample == 32)            {
				
                GenerateProposals16(tmps, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                GenerateProposals16(tmps, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                GenerateProposals16(tmps, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
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
                        
                            
						if (tmps[h * W*12+ w*12 + 0]*FScale > 0.5)
						{
							GreyBack.at<uchar>(h,w)=255;
						}
						else
						{
							GreyBack.at<uchar>(h,w)=0;
						}							
                            
                        
                    }   
                }
            
            }
            

        }else if (Bits == 8)
        {
            tmpu = (VIM_U8 *)this->net_out_blob[i].data;
            FScale = this->net_out_blob[i].scale;
            //FScale = 0.007812;                                                                        //强转0.0078125
            
            int downsample = mode_input_img_w / W;
            if(downsample == 32)
            {
                GenerateProposals8(tmpu, W, H, C, downsample, 0, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 16)
            {
                GenerateProposals8(tmpu, W, H, C, downsample, 1, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 8)
            {
                GenerateProposals8(tmpu, W, H, C, downsample, 2, objconfidence, proposals, classnumber, FScale);
            }
            if(downsample == 1)
            {
                
                for(int h = 0; h < H; h++)
                {
                    for(int w = 0; w < W; w++)
                    {                      
                            
						if (tmpu[h * W*16+ w*16 + 1]*FScale > 0.6)
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
    }
    printf("proposals=%d\n", proposals.size());
    QsortDescentInplace(proposals);
    std::vector<size_t> picked;
    DetectNms(proposals, picked,objconfidence);
    printf("picked=%d\n", picked.size());
    //std::vector<ObjectDetectInfo> object;
    objects.resize(picked.size());

    for (size_t i = 0; i < picked.size(); ++i) {
        objects[i] = proposals[picked[i]];
    }

 

    proposals.clear();
    picked.clear();


    return s32Ret;
}