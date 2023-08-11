#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "lc_jpeg_api.h"
#include "lc_glob_type.h"
#include "lc_dvr_record_log.h"
#include "lc_dvr_comm_def.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "lc_dvr_semaphore.h"

#define LC_JPEG_CHANEL_MAX_NUM 6
#define LC_PIC_LIST_COUNT 8
#define ALIGN_16B(width)	((width+15)&(~15))

static JPEG_PIC_CTRL_H g_Jpeg_Info[LC_JPEG_CHANEL_MAX_NUM]={0};
static int g_Jpeg_TakePic;
static int g_pic_list_head = 0;
static int g_pic_list_tail = 0;
static VIDEO_FRAME_INFO_S g_pstFrame;
static int g_srcWidth;
static int g_srcHeight;
static LC_TAKE_PIC_INFO g_pic_list[8];

extern void vc_message_send(int eMsg, int val, void* para, void* func);

JPEG_PIC_CTRL_H *lc_jpeg_handler(VIM_U32 vpssChnl)
{
	if(vpssChnl<LC_JPEG_CHANEL_MAX_NUM)
	{
		return &(g_Jpeg_Info[vpssChnl]);
	}
	else
	{
		return NULL;
	}
}

static VIM_S32 lc_jpeg_malloc(VIM_U32 u32Width, VIM_U32 u32Height, PIXEL_FORMAT_E enPixelFormat,
						  VIDEO_FRAME_INFO_S *pstFrame)
{
    VIM_S32 s32Ret;
	VIM_U32 width = CEILING_2_POWER(u32Width, SAMPLE_SYS_ALIGN_WIDTH);
	VIM_U32 height = CEILING_2_POWER(u32Height, SAMPLE_SYS_ALIGN_WIDTH);
    VIM_U32 r0;
	VIM_U32 r1;
	VIM_U32 r2;
	//logd_lc("SAMPLE_COMM_JPEG_Malloc-->w:%d, h:%d\n", u32Width, u32Height);

    r0 = width * height;
    r1 = r2 = 0;

    switch (enPixelFormat)
    {
    case PIXEL_FORMAT_YUV_PLANAR_420:
        r1 = r2 = r0 / 4;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        r1 = r0 / 2;
        r2 = 0;
        break;
    case PIXEL_FORMAT_YUV_PLANAR_422:
        r1 = r2 = r0 / 2;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
        r1 = r0;
        r2 = 0;
        break;
    case PIXEL_FORMAT_YUV_PLANAR_444:
        r1 = r2 = r0;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_444:
        r1 = 2 * r0;
        r2 = 0;
        break;
    default:
        ///< logd_lc("as yuv400!\n");
        break;
    }

    memset(pstFrame, 0, sizeof(*pstFrame));
	//logd_lc("pstFrame-->%d\n", r0 + r1 + r2);
    s32Ret = VIM_MPI_JPEG_GetVB(pstFrame, r0 + r1 + r2);
    if (VIM_SUCCESS != s32Ret)
    {
        logd_lc("VIM_MPI_JPEG_GetVB failed!\n");
        return s32Ret;
    }

    VIM_U32 viraddr = (VIM_U32)pstFrame->stVFrame.pVirAddr[0];
    VIM_U32 phyaddr = pstFrame->stVFrame.u32PhyAddr[0];

    pstFrame->stVFrame.u32Size[0] = r0;
    pstFrame->stVFrame.u32Size[1] = r1;
    pstFrame->stVFrame.u32Size[2] = r2;

    pstFrame->stVFrame.u32Stride[0] = width;
    pstFrame->stVFrame.enPixelFormat = enPixelFormat;
    pstFrame->stVFrame.u32Width = u32Width;
    pstFrame->stVFrame.u32Height = u32Height;

    if (r1)
    {
        pstFrame->stVFrame.pVirAddr[1] = (void *)(viraddr + r0);
        pstFrame->stVFrame.u32PhyAddr[1] = phyaddr + r0;
    }
    if (r2)
    {
        pstFrame->stVFrame.pVirAddr[2] = (void *)(viraddr + r0 + r1);
        pstFrame->stVFrame.u32PhyAddr[2] = phyaddr + r0 + r1;
    }

    return 0;
}

static VIM_S32 lc_jpeg_free(VIDEO_FRAME_INFO_S *pstFrame)
 {
	 return VIM_MPI_JPEG_ReleaseVB(pstFrame, 0);
 }

 
static VIM_S32 lc_jpeg_start(JPEG_CHN JpgChn, JPEG_CHN_ATTR_S *pstAttr, int level)
{
    VIM_S32 s32Ret;
    /******************************************
     step 1:  Create JPEG Channel
     ******************************************/
    s32Ret = VIM_MPI_JPEG_CreateChn(JpgChn, pstAttr);
    if (VIM_SUCCESS != s32Ret) {
        logd_lc("VIM_MPI_JPEG_CreateChn [%d] faild with %#x!\n", JpgChn, s32Ret);
        return s32Ret;
    }

    /******************************************
     step 2:  Start Recv Venc Pictures
     ******************************************/
    s32Ret = VIM_MPI_JPEG_StartRecvPic(JpgChn);
    if (VIM_SUCCESS != s32Ret) {
        logd_lc("VIM_MPI_JPEG_StartRecvPic faild with%#x!\n", s32Ret);
        return VIM_FAILURE;
    }
	
	VIM_MPI_JPEG_SetChnQLevel( JpgChn, level);
	logd_lc("setChnQLevel --> %d", level);
	
    return VIM_SUCCESS;
}

static VIM_S32 lc_jpeg_stop(JPEG_CHN JpgChn)
{
    VIM_S32 s32Ret;
    /******************************************
     *step 1:  Stop Recv Pictures
     ******************************************/
    s32Ret = VIM_MPI_JPEG_StopRecvPic(JpgChn);
    if (VIM_SUCCESS != s32Ret)
    {
        logd_lc("VIM_MPI_JPEG_StopRecvPic chn[%d] failed with %#x!\n", JpgChn, s32Ret);
        return VIM_FAILURE;
    }
    /******************************************
     *step 2:  Distroy Venc Channel
     ******************************************/
    s32Ret = VIM_MPI_JPEG_DestroyChn(JpgChn);
    if (VIM_SUCCESS != s32Ret)
    {
        logd_lc("VIM_MPI_JPEG_DestroyChn chn[%d] failed with %#x!\n", JpgChn, s32Ret);
        return VIM_FAILURE;
    }
    return VIM_SUCCESS;
}

static void *lc_jpeg_takePic_handler(void *arg)
{
	JPEG_PIC_CTRL_H *chnlInfo = (JPEG_PIC_CTRL_H*)arg;
	VIDEO_FRAME_INFO_S *pstFrame = &(chnlInfo->pstFrame);
	VIM_U32 chn = chnlInfo->JpegConfig.u32JpegChnl;
	VIM_S32 s32Ret;
	s32Ret = VIM_MPI_JPEG_SendFrame(chn, pstFrame);
	if (VIM_SUCCESS != s32Ret) {
		logd_lc("%s failed in line[%d]-->%x\n", __FUNCTION__, __LINE__, s32Ret);
		lc_jpeg_free(pstFrame);
		return NULL;
	}
	
	{
	    VENC_PACK_S pack = {0};
	    JPEG_STREAM_S stStream = {.pstPack[0] = &pack};

	    s32Ret = VIM_MPI_JPEG_GetStream(chn, &stStream, 2000);
	    if (VIM_SUCCESS == s32Ret) {
			/**存储拍照结果**/
			logd_lc("file SaveName:%s", chnlInfo->JpegConfig.fileSaveName);
			//===============================================================================
#if 1			
//			FILE* pFile = fopen((void *)chnlInfo->JpegConfig.fileSaveName, "wb");//		
//			if(pFile!=NULL)
//			{				
//		        fwrite(stStream.pstPack[0]->pu8Addr, stStream.pstPack[0]->u32Len, 1, pFile);
//				fsync(pFile);
//				fclose(pFile);					
//				logd_lc("GetStreamBlock chn[%d] --> %d Success!\n", chn, stStream.pstPack[0]->u32Len);
//			}
//			else
//			{
//				loge_lc("pFile is NULL!\n");
//			}

			direct_file_data_save(chnlInfo->JpegConfig.fileSaveName, stStream.pstPack[0]->pu8Addr, stStream.pstPack[0]->u32Len);
#endif			
			if(chnlInfo->JpegConfig.ackFunc!=NULL)
			{
				LC_ACK_FUNC function = (LC_ACK_FUNC)chnlInfo->JpegConfig.ackFunc;
				function(stStream.pstPack[0]->u32Len, (int)chnlInfo->JpegConfig.fileSaveName);
			}			
	        //===============================================================================
	        VIM_MPI_JPEG_ReleaseStream(chn, &stStream);
	    } else {
	        logd_lc("SAMPLE_COMM_JPEG_GetStreamBlock chn[%d] failed with %#x!\n", chn, s32Ret);
	    }		
	}
	
	lc_jpeg_free(pstFrame);
	vc_message_send(ENU_LC_DVR_MDVR_PIC_NOTIFY, 0, NULL, NULL);
	lc_jpeg_remove_tail_in_queue();

	if(lc_jpeg_get_from_queue()!=NULL)
	{/**是否还有剩余图片需要拍摄**/
		vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC_START, 0, NULL, NULL);
	}

	return NULL;
}

static VIM_S32 lc_jpeg_dma_copy(VIM_VPSS_FRAME_INFO_S *stFrameInfo, VIDEO_FRAME_INFO_S *inputFrame)
{
	VIM_S32 fd, memfd, id;
	struct userdma_user_info info = {0}; 
    struct userdma_lli vmem[MAX_LLI_NUM] = {0};
	VIM_U32 DST;
	VIM_U32 YLen, UVLen;
	VIM_S32 ret;
	int offset = 0;
	
	YLen = stFrameInfo->u32ByteUsedStd[0];
	UVLen = stFrameInfo->u32ByteUsedStd[1];
	DST = inputFrame->stVFrame.u32PhyAddr[0];

	if(inputFrame->stVFrame.u32Size[0]>stFrameInfo->u32ByteUsedStd[0])
	{/**消除VPSS输出图像VEN的输入图像尺寸不匹配造成的粉红彩边**/
		offset = (inputFrame->stVFrame.u32Size[0] - stFrameInfo->u32ByteUsedStd[0]);
		memset(inputFrame->stVFrame.pVirAddr[0], 0x10, offset);
		memset(inputFrame->stVFrame.pVirAddr[0]+inputFrame->stVFrame.u32Size[0]-offset, 0x10, offset);
		
		offset/=2;
		memset(inputFrame->stVFrame.pVirAddr[1], 0x80, offset);
		memset(inputFrame->stVFrame.pVirAddr[1]+inputFrame->stVFrame.u32Size[1]-offset, 0x80, offset);		
	}

	//logd_lc("inSize:%d, %d", stFrameInfo->u32ByteUsedStd[0], stFrameInfo->u32ByteUsedStd[1]);	

	fd = VIM_MPI_SYS_DMAOpen(&id);
	if (fd < 0) {
		loge_lc("[DMA] VIM_MPI_SYS_DMAOpen error\n");
		return -1;
	}
	// logd_lc("[DMA] request dma channel %d\n",id);

	memfd = VIM_PLAT_OpenMem();
	if (memfd < 0) {
		
		VIM_MPI_SYS_DMAClose(fd);
		loge_lc("[DMA] VIM_PLAT_OpenMem err\n");
		return -1;
	}

	info.ch = id;	
	info.listnr = 2;
	info.llis[0].src = stFrameInfo->pPhyAddrStd[0];
	info.llis[0].dst = inputFrame->stVFrame.u32PhyAddr[0]+offset;
	info.llis[0].len = YLen;
	vmem[0].src = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].src, YLen);
	vmem[0].dst = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].dst, YLen);
	vmem[0].len = YLen;

	info.llis[1].src = stFrameInfo->pPhyAddrStd[1];
	info.llis[1].dst = inputFrame->stVFrame.u32PhyAddr[1]+(offset>>1);
	info.llis[1].len = UVLen;
	vmem[1].src = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[1].src, UVLen);
	vmem[1].dst = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[1].dst, UVLen);
	vmem[1].len = UVLen;

	VIM_MPI_SYS_DMACopy(fd, &info);
	ret = VIM_MPI_SYS_DMAPoll(fd, 33000);
	if(ret == 0) {
		logd_lc("[DMA] poll time out!\n");
		goto ret;
	}
ret:
	VIM_PLAT_UnmapMem((void*)(vmem[0].src), YLen);
	VIM_PLAT_UnmapMem((void*)(vmem[0].dst), YLen);
	VIM_PLAT_UnmapMem((void*)(vmem[1].src), UVLen);
	VIM_PLAT_UnmapMem((void*)(vmem[1].dst), UVLen);

	//VIM_PLAT_CloseMem(memfd);
	close(memfd);
	VIM_MPI_SYS_DMAClose(fd);
	
	return VIM_SUCCESS;
}

void lc_yuv_scaler(void* src_y, void* src_uv, void *dst_y, void *dst_uv, int srcw, int srch, int offset)
{
	int sum;
	int w = srcw, h = srch;
	int col_bytes = ALIGN_16B(srcw)/16;
	uint8_t *line1 = (uint8_t*)src_y;
	uint8_t *line3 = line1+2*w; 	
	uint8_t *srcuv1 = (uint8_t*)src_uv; 
	
	uint8_t *dstY1 =  (uint8_t*)dst_y;
	uint8_t *dstY2 =  dstY1 + (w/2);
	uint8_t *dstuv1 =  (uint8_t*)dst_uv;
	
	for(int row=0; row<(offset/2); row++)
	{
		memset(dstY1, 0x10, w/2); dstY1+=w;
		memset(dstY2, 0x10, w/2); dstY2+=w;
		memset(dstuv1, 0x80, w/2); dstuv1+=w/2;
	}
	
	dstY1 +=  (w*h/4);
	dstY2 =  dstY1 + (w/2);
	dstuv1 +=  (w*h/8);

	for(int row=0; row<(offset/2); row++)
	{
		memset(dstY1, 0x10, w/2); dstY1+=w;
		memset(dstY2, 0x10, w/2); dstY2+=w;
		memset(dstuv1, 0x80, w/2); dstuv1+=w/2;
	}
	
	dstY1 =  (uint8_t*)dst_y + ((offset*w)/2);
	dstY2 =  dstY1 + (w/2);
	dstuv1 =  (uint8_t*)dst_uv + ((offset*w)/4);	
	
	for(int row=0; row<h; row+=4)
	{
#if 1
		asm volatile
		(		
			"mov	r6, 	%[col_bytes]	\t\n"
			"1:\t\n"
		
			"pld			[%[line1],#64]				\n\t"
			"pld			[%[line3],#64]				\n\t"
			"pld			[%[srcuv1],#64]				\n\t"
			"vld1.u8		{d0,d1},  [%[line1]]!		\n\t"	
			"vld1.u8		{d2,d3},  [%[line3]]!		\n\t"
			"vld1.u8		{d4,d5},  [%[srcuv1]]! 		\n\t"

			"vmovn.u16		d6, q0		\n\t"
			"vmovn.u16		d7, q1		\n\t"
			"vmovn.u32		d8, q2		\n\t"
			
			"vst1.u8		{d6},	[%[dstY1]]! 	\n\t"	
			"vst1.u8		{d7},	[%[dstY2]]! 	\n\t"	
			"vst1.u8		{d8},	[%[dstuv1]]! 	\n\t"
				
			"subs r6, r6, #1\t\n"
			"bne 1b\t\n"					// JUMP TO 1:\t\n
			"2:\t\n"

			:[line1] "+r" (line1),		// %0
			 [line3] "+r" (line3),		// %1 
			 [srcuv1] "+r" (srcuv1),	// %2
			 [dstY1] "+r" (dstY1),		// %3
			 [dstY2] "+r" (dstY2),		// %4
			 [dstuv1] "+r" (dstuv1),	// %5
			 [col_bytes] "+r" (col_bytes) // %6
			:
			: "r6", "cc","memory", 
				"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7","d8" 			
		);
#else
		for(int col=0; col<w/2; col++)
		{
			dstY1[0] = line1[0];
			line1+=2;
			dstY1++;
			
			dstY2[0] = line3[0];
			line3+=2;
			dstY2++;
			
			if((col&0x01)==0)
			{
				dstuv1[0] = srcuv1[0];
			}
			else
			{
				dstuv1[0] = srcuv1[1];
				srcuv1+=4;		
			}
			dstuv1++;
		}
#endif
		line1 += 3*w;
		line3 += 3*w;
		srcuv1 += w;
		dstY1 += w/2;
		dstY2 += w/2;
	}
}

void lc_rgb_convert(void* srcR, void* srcG, void *srcB, void *color, int srcw, int srch)
{
	int col_bytes = ALIGN_16B(srcw*srch)/8;
	uint8_t *line1 = (uint8_t*)srcB;
	uint8_t *line2 = (uint8_t*)srcG;
	uint8_t *line3 = (uint8_t*)srcR;	
	uint8_t *dstColor =  (uint8_t*)color;

	asm volatile
	(		
		"mov	r6, 	%[col_bytes]	\t\n"
		"1:\t\n"
	
		"pld			[%[line1],#64]				\n\t"
		"pld			[%[line2],#64]				\n\t"
		"pld			[%[line3],#64]				\n\t"
		
		"vld1.u8		{d0},  [%[line1]]!		\n\t"	
		"vld1.u8		{d1},  [%[line2]]!		\n\t"
		"vld1.u8		{d2},  [%[line3]]! 		\n\t"
		
		"vst3.u8		{d0, d1, d2},	[%[dstColor]]! 	\n\t"
		"subs r6, r6, #1\t\n"
		"bne 1b\t\n"					// JUMP TO 1:\t\n
		"2:\t\n"

		:[line1] "+r" (line1),		// %0
		 [line2] "+r" (line2),		// %1 
		 [line3] "+r" (line3),		// %2
		 [dstColor] "+r" (dstColor),  // %3
		 [col_bytes] "+r" (col_bytes) // %6
		:
		: "r6", "cc","memory", 
			"d0", "d1", "d2", "d3"
	);		

}


static VIM_VOID lc_picture_scaler(int w, int h, VIDEO_FRAME_INFO_S *inFrameInfo)
{
	int yLen = inFrameInfo->stVFrame.u32Size[0];	
	int uvLen = inFrameInfo->stVFrame.u32Size[1];
	int spendTime;
	JPEG_PIC_CTRL_H *jpegCtrl = lc_jpeg_handler(LC_THUMB_ENCODE_CHANEL);
	
    VIM_U32 width = jpegCtrl->JpegConfig.u32PicWidth;
    VIM_U32 height = jpegCtrl->JpegConfig.u32PicHeight;
    PIXEL_FORMAT_E enPixFmt = jpegCtrl->JpegConfig.enPixFmt;
	VIDEO_FRAME_INFO_S *pstFrame = &jpegCtrl->pstFrame;

	jpegCtrl->do_TakePic = 0;
	lc_jpeg_malloc(width, height, enPixFmt, pstFrame);
	struct timeval t_start,t_end;
	gettimeofday(&t_start, NULL);
	{
		char *src_y = inFrameInfo->stVFrame.pVirAddr[0];
		char *src_uv = inFrameInfo->stVFrame.pVirAddr[1];
		
		char *dst_y = pstFrame->stVFrame.pVirAddr[0];
		char *dst_uv = pstFrame->stVFrame.pVirAddr[1];
		int offset = (pstFrame->stVFrame.u32Size[0]-yLen/4)/2;

		lc_yuv_scaler(src_y, src_uv, dst_y, dst_uv, w, h, 12);
	}
	
	lc_jpeg_free(inFrameInfo);
	gettimeofday(&t_end, NULL);
	spendTime = t_end.tv_usec - t_start.tv_usec;
	if(spendTime<0) spendTime += 1000000;
	spendTime /= 1000;
	
	logd_lc("Scaler_Time: %d ms!!\n", spendTime); /**大概20毫秒**/
	lc_jpeg_takePic_handler(jpegCtrl);
}


static VIM_VOID lc_scaler_feed_thread(void *para)
{
	lc_picture_scaler(g_srcWidth, g_srcHeight, &g_pstFrame);	
}

VIM_VOID lc_scaler_picture_take(int w, int h, VIM_VPSS_FRAME_INFO_S *inFrameInfo)
{
	int s32Ret;
	pthread_t jpg_thread;
	lc_jpeg_malloc(w, h, PIXEL_FORMAT_YUV_SEMIPLANAR_420, &g_pstFrame);		
	lc_jpeg_dma_copy(inFrameInfo, &g_pstFrame);
	g_srcWidth = w;
	g_srcHeight = h;
	s32Ret = pthread_create(&jpg_thread,NULL,(void*)lc_scaler_feed_thread,(void*)&g_pstFrame);
	if(0 == s32Ret )
	{
		// printf("jpg_feed pthread SUCCESS\n");
		pthread_detach(jpg_thread);
	}	
}

VIM_S32 lc_jpeg_frame_send(VIM_VPSS_FRAME_INFO_S *stFrameInfo, VIM_U32 vpssChnl)
{
	JPEG_PIC_CTRL_H *jpegCtrl = &g_Jpeg_Info[vpssChnl];
	if(jpegCtrl->do_TakePic)
	{		
	    VIM_S32 s32Ret;		
	    VIM_U32 width = jpegCtrl->JpegConfig.u32PicWidth;
	    VIM_U32 height = jpegCtrl->JpegConfig.u32PicHeight;
	    PIXEL_FORMAT_E enPixFmt = jpegCtrl->JpegConfig.enPixFmt;
		VIDEO_FRAME_INFO_S *pstFrame = &jpegCtrl->pstFrame;
		pthread_t picThread;

		jpegCtrl->do_TakePic = 0;		
		lc_jpeg_malloc(width, height, enPixFmt, pstFrame);		
		lc_jpeg_dma_copy(stFrameInfo, pstFrame);		
	//	memcpy(pstFrame->stVFrame.pVirAddr[0], stFrameInfo->pVirAddrStd[0], pstFrame->stVFrame.u32Size[0]);
	//	memcpy(pstFrame->stVFrame.pVirAddr[1], stFrameInfo->pVirAddrStd[1], pstFrame->stVFrame.u32Size[1]);
	//	memcpy(pstFrame->stVFrame.pVirAddr[2], stFrameInfo->pVirAddrStd[2], pstFrame->stVFrame.u32Size[2]);	

		s32Ret = pthread_create(&picThread, NULL, (void*)lc_jpeg_takePic_handler, jpegCtrl);
	    if (0 == s32Ret)
	    {
	    	//logd_lc("Creat jpeg thread!\n");
	        pthread_detach(picThread);// 执行完自动释放资源
	    }		
	}
	return 0;
}

VIM_S32 lc_jpeg_chanel_stop(VIM_U32 vpssChnl)
{
	VIM_U32 usedNum = 0;
	VIM_S32 s32Ret = -1;
	
	if(vpssChnl < LC_JPEG_CHANEL_MAX_NUM)
	{
		JPEG_PIC_CTRL_H *jpegCtrl = &g_Jpeg_Info[vpssChnl];
		
		if(jpegCtrl->u32UsedSign==0) 
			return -1;
		
		s32Ret = lc_jpeg_stop(jpegCtrl->JpegConfig.u32JpegChnl);
		memset(jpegCtrl, 0, sizeof(JPEG_PIC_CTRL_H));

		for(int i=0; i<LC_JPEG_CHANEL_MAX_NUM; i++)
		{
			if(g_Jpeg_Info[i].u32UsedSign!=0) usedNum++;
		}

		if(usedNum == 0)
		{
	    	s32Ret = VIM_MPI_JPEG_Exit();
		}
	}
    return s32Ret;
}

VIM_S32 lc_jpeg_chanel_start(JPEG_PIC_ATTR *jpegCfg, VIM_U32 vpssChnl, int level)
{
	JPEG_BUF_ATTR_S jpegAttr;
	JPEG_CHN_ATTR_S pstAttr;
	JPEG_CHN JpgChn = jpegCfg->u32JpegChnl;
	VIM_S32 s32Ret=-1, usedNum = 0;	

	if(vpssChnl < LC_JPEG_CHANEL_MAX_NUM)
	{
		JPEG_PIC_CTRL_H *jpegCtrl = &g_Jpeg_Info[vpssChnl];
		
		for(int i=0; i<LC_JPEG_CHANEL_MAX_NUM; i++)
		{
			if(g_Jpeg_Info[i].u32UsedSign!=0) usedNum++;
		}

		if(usedNum==0) 
		{		
			jpegAttr.u32UseExtBufs = 0;
			jpegAttr.u32NumOutBufs = 3; // YUV Max is 3Chanel
			jpegAttr.u32SizeOutBufs = 1920*1088;
			s32Ret = VIM_MPI_JPEG_Init(&jpegAttr); /**如果没有通道被打开，则需要初始化JPEG模块**/
		}

		if(jpegCtrl->u32UsedSign)
		{/** 已经被占用 **/
			return -2;
		}

		memcpy(&(jpegCtrl->JpegConfig), jpegCfg, sizeof(JPEG_PIC_ATTR));
		jpegCtrl->u32UsedSign = 1;
		jpegCtrl->vpss_chn = vpssChnl;

		memset(&pstAttr, 0, sizeof(JPEG_CHN_ATTR_S));
	    pstAttr.decode = 0;	// 0: jpeg encode, 1: jpeg decode
	    pstAttr.u32QLevel = 4;  // 1~6 default 4
	    
	    //pstAttr.u32HorScale = 0;	/**仅解码用**/
	    //pstAttr.u32VerScale = 0;  /**仅解码用**/
	    //pstAttr.u32MaxPicWidth = 0;   /**暂未使用**/
	    //pstAttr.u32MaxPicHeight = 0;  /**暂未使用**/
	    
	    pstAttr.u32PicWidth	= jpegCtrl->JpegConfig.u32PicWidth;
	    pstAttr.u32PicHeight = jpegCtrl->JpegConfig.u32PicHeight;
	    pstAttr.enPixFmt = jpegCtrl->JpegConfig.enPixFmt;
	    pstAttr.enRotate = 0;		// 0, 90, 180, 270
	    pstAttr.enMirror = 0;		// 0: no mirror, 1: vertical, 2: horizontal, 3: both

		pstAttr.stCropRect.s32X = 0;
		pstAttr.stCropRect.s32Y = 0;
		pstAttr.stCropRect.u32Width = jpegCtrl->JpegConfig.u32PicWidth;
		pstAttr.stCropRect.u32Height = jpegCtrl->JpegConfig.u32PicHeight;
		
	    pstAttr.u32FrameGap = 0;  // binder use, drop frame	
		s32Ret = lc_jpeg_start(JpgChn, &pstAttr, level);		
	}
	g_Jpeg_TakePic = 0;
	return s32Ret;
}

VIM_S32 lc_jpeg_do_takePic(char *path, VIM_U32 camChnl, VIM_U32 vpssChnl, void* para)
{
	JPEG_PIC_CTRL_H *jpegCtrl = &g_Jpeg_Info[vpssChnl]; /**vpssChnl有不同的分辨率， camChnl表示不同的摄像头通道**/
	if(jpegCtrl->u32UsedSign)
	{
		 logd_lc("do_takePic->%s", path);	
		strcpy((char*)jpegCtrl->JpegConfig.fileSaveName, path);	
		jpegCtrl->cam_chn = camChnl;		
		jpegCtrl->do_TakePic = 1; /**这里作为帧延时序号，当值为1时执行拍照**/
		jpegCtrl->JpegConfig.ackFunc = para;
	}		
	return 0;
}

VIM_VOID lc_jpeg_insert_to_queue(void* picTask)
{
	memcpy(&g_pic_list[g_pic_list_head], picTask, sizeof(LC_TAKE_PIC_INFO));
	//printf("lc_jpeg_insert_to_queue-->%d\n", g_pic_list_head);
	g_pic_list_head++;
	if(g_pic_list_head>=LC_PIC_LIST_COUNT) g_pic_list_head = 0;
}

VIM_VOID* lc_jpeg_get_from_queue(void)
{
	LC_TAKE_PIC_INFO *picTake = NULL;
	// printf("g_pic_list-->t:%d, h:%d\n", g_pic_list_tail, g_pic_list_head);
	if(g_pic_list_tail!=g_pic_list_head)
	{
		picTake = &g_pic_list[g_pic_list_tail];
	}

	return picTake;
}

VIM_VOID lc_jpeg_remove_tail_in_queue(void)
{
	//printf("lc_jpeg_remove_tail_in_queue-->%d\n", g_pic_list_tail);
	g_pic_list_tail++;
	if(g_pic_list_tail>=LC_PIC_LIST_COUNT) g_pic_list_tail = 0;	
}

