#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/fb.h>   /**struct fb_var_screeninfo**/
#include <unistd.h>
#include <sys/prctl.h>  /** asked by PR_SET_NAME **/
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>	   // mkfifo
#include <errno.h>

#include "lc_glob_type.h"
#include "lc_dvr_record_log.h"
#include "vc0768_log_prt.h"
//#include "lc_codec_api.h"
#include "lc_jpeg_api.h"
#include "mpi_sys.h"
#include "mpi_vpss.h"


typedef void (*VPSS_PRO_FUNC)(int vpssChl, void* data);

static int gThreadVpssRun = 0;
static int gFuncPara;
static int gVpssImageSave;
VPSS_PRO_FUNC mVpssProcFunc;

VIM_S32 lc_dvr_vpss_copy(VIM_VPSS_FRAME_INFO_S *stFrameInfo, void *dstFrame)
{
	VIM_U32 YLen = stFrameInfo->u32ByteUsedStd[0];
	char *srcMem = (char*)stFrameInfo->pVirAddrStd[0];
	memcpy(dstFrame, srcMem, YLen);
	return YLen;
}

static VIM_S32 lc_dvr_vpss_dma_copy(VIM_VPSS_FRAME_INFO_S *stFrameInfo, VIM_VPSS_FRAME_INFO_S *dstFrame)
{
	VIM_S32 fd, memfd, id;
	struct userdma_user_info info = {0}; 
    struct userdma_lli vmem[MAX_LLI_NUM] = {0};
	VIM_U32 DST;
	VIM_U32 YLen, UVLen;
	VIM_S32 ret;	
	
	YLen = stFrameInfo->u32ByteUsedStd[0];
	UVLen = stFrameInfo->u32ByteUsedStd[1];
	DST = dstFrame->pPhyAddrStd[0];


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
	info.llis[0].dst = DST;
	info.llis[0].len = YLen;
	vmem[0].src = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].src, YLen);
	vmem[0].dst = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].dst, YLen);
	vmem[0].len = YLen;

	info.llis[1].src = stFrameInfo->pPhyAddrStd[1];
	info.llis[1].dst = DST+YLen;
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

static VIM_S32 lc_dvr_vpss_destroy(VIM_U32 u32GrpID, VIM_U32 u32ChnId)
{
	VIM_S32 s32Ret = 0;
	VIM_VPSS_GROUP_ATTR_S vpssGrpAttr = {0};
	VIM_MPI_VPSS_GetGrpAttr(u32GrpID, &vpssGrpAttr);

	s32Ret = VIM_MPI_VPSS_StopChn(u32GrpID, u32ChnId);
	ASSERT_RET_RETURN(s32Ret);
	
	s32Ret = VIM_MPI_VPSS_DisableChn(u32GrpID, u32ChnId);
	ASSERT_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_VPSS_DestroyGrp(u32GrpID);
	ASSERT_RET_RETURN(s32Ret);

//	s32Ret = VIM_MPI_VPSS_DisableDev();
//	ASSERT_RET_RETURN(s32Ret);
	logd_lc("lc_dvr_vpss_destroy!");

	return s32Ret;
}

static void *lc_dvr_vpss_stream_thread(void *arg)
{
    int ret = -1;
    int cndIdx = 0;
    int releaseIdx = 0;
	int wrkIdx;

    VIM_VPSS_FRAME_INFO_S stFrameInfoEX[4] = {0};

	//logd_lc("lc_dvr_vpss_stream_thread start!");
    while (gThreadVpssRun)
    {
        /**浠嶸PSS绗?缁勭0涓�氶亾鑾峰彇DCVI閫氶亾鍥惧儚鏁版嵁**/
		wrkIdx = cndIdx&0x03;
        ret = VIM_MPI_VPSS_GetFrame(LC_DVR_JPEG_GROUP_ID, LC_DVR_JPEG_CHANEL_ID, &stFrameInfoEX[wrkIdx], 3000);

		gThreadVpssRun--;
		
		if (stFrameInfoEX[wrkIdx].pPhyAddrStd[0]==0) ret = -2;
		
        if(0 != ret)
        {
            logd_lc("Asist_VPSS_GetFrame err %x\n", ret);
            usleep(1);
            break;
        }
        else
        {
//			if(gVpssImageSave)
//			{			
//				char saveName[128];
//				FILE *fp;
//				int RLen = stFrameInfoEX[wrkIdx].u32ByteUsedStd[0];
//				int GLen = stFrameInfoEX[wrkIdx].u32ByteUsedStd[1];	
//				int BLen = stFrameInfoEX[wrkIdx].u32ByteUsedStd[2];
//				sprintf(saveName, "/mnt/sdcard/rawImage%02d.raw", gVpssImageSave);
//				fp = fopen(saveName, "wb+");
//				fwrite(stFrameInfoEX[wrkIdx].pVirAddrStd[0], 1, RLen, fp);
//				fwrite(stFrameInfoEX[wrkIdx].pVirAddrStd[1], 1, GLen, fp);
//				fwrite(stFrameInfoEX[wrkIdx].pVirAddrStd[2], 1, BLen, fp);
//				fclose(fp);	
//				logd_lc("u32SrcVidChn->%d", stFrameInfoEX[wrkIdx].u32SrcVidChn);
//				logd_lc("R:%d, G:%d, B:%d", RLen, GLen, BLen);		
//				gVpssImageSave--;
//			}
		
            cndIdx++;
        }

		if(mVpssProcFunc!=NULL)
		{	
			mVpssProcFunc(gFuncPara, &stFrameInfoEX[wrkIdx]);
		}
		
        if(cndIdx > 1)
        {
        	wrkIdx = releaseIdx&0x03;
			VIM_MPI_VPSS_GetFrame_Release(LC_DVR_JPEG_GROUP_ID, LC_DVR_JPEG_CHANEL_ID, &stFrameInfoEX[wrkIdx]);
			releaseIdx++;
        }
    }

	//logd_lc("Stream Exit-->cndIdx:%d, releaseIdx:%d!\n", cndIdx, releaseIdx);
	for(;releaseIdx!=cndIdx;)
	{
    	wrkIdx = releaseIdx&0x03;
        VIM_MPI_VPSS_GetFrame_Release(LC_DVR_JPEG_GROUP_ID, LC_DVR_JPEG_CHANEL_ID, &stFrameInfoEX[wrkIdx]);	
		releaseIdx++;
	}	
    return NULL;
}

void *lc_dvr_assist_vpss_frame_send(void *frame, int group, int funPara)
{
	if(gThreadVpssRun)
	{
		VIM_S32 s32Ret;
	 
		VIM_VPSS_FRAME_INFO_S* sFrameInfo = (VIM_VPSS_FRAME_INFO_S*)frame;
		VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};
						
		s32Ret = VIM_MPI_VPSS_SendFrame_Prepare(group, &stFrameInfo); /*一个GROUP只有一个输入源，可以输出到最多6个通道*/
		if (s32Ret != 0)
		{
			printf("SendFrame_Prepare Err --> %x!!\n", s32Ret);
		}

		lc_dvr_vpss_dma_copy(sFrameInfo, &stFrameInfo); /**1080拷贝耗时2~3毫秒**/
		stFrameInfo.u32SrcVidChn = funPara;
		
	//	for(int plane=0; plane<VI_FRAME_STD_PLANE_MAX; plane++)
	//	{/**设置成外部mem方式始终不成功，之前用input0是可以的**/
	//		stFrameInfo.pVirAddrStd[plane] = sFrameInfo->pVirAddrStd[plane];
	//		stFrameInfo.pPhyAddrStd[plane] = sFrameInfo->pPhyAddrStd[plane];
	//		stFrameInfo.u32ByteUsedStd[plane] = sFrameInfo->u32ByteUsedStd[plane];
	//		stFrameInfo.u32ByteUsedCfr[plane] = 0;
	//	}
		gFuncPara = funPara;
		s32Ret = VIM_MPI_VPSS_SendFrame(group, &stFrameInfo, 3000);
		if (s32Ret != 0)
		{
			printf("SendFrame Err-->%x!!\n", s32Ret);
		}	
	}
    return NULL;
}
// inW inH ： input image size
// otW otH :  output image size
VIM_S32 lc_dvr_assist_vpss_creat(VIM_U32 inW, VIM_U32 inH, VIM_U32 otW, VIM_U32 otH, void* ackFunc)
{
	VIM_VPSS_GROUP_ATTR_S vpssGrpAttr = {0};
	VIM_CHN_CFG_ATTR_S stChnAttr = {0};
	VIM_S32 s32Ret = -1;
	VIM_U32 u32GrpID = LC_DVR_JPEG_GROUP_ID;
	VIM_U32 u32ChnId = LC_DVR_JPEG_CHANEL_ID;
	
	logd_lc("Go lc_dvr_vpss_jpeg_creat start!");

	mVpssProcFunc = (VPSS_PRO_FUNC)ackFunc;

	vpssGrpAttr.u32GrpId = LC_DVR_JPEG_GROUP_ID;
	vpssGrpAttr.u32GrpStatus = E_VPSS_GROUP_STATIC;
	
	if(u32ChnId == 4)
		vpssGrpAttr.u32GrpDataSrc = E_VPSS_INPUT_1;	/**INPUT_1只能接通道4**/
	else if(u32ChnId == 5)	
		vpssGrpAttr.u32GrpDataSrc = E_VPSS_INPUT_2;  /**INPUT_2只能接通道5**/
	else
		vpssGrpAttr.u32GrpDataSrc = E_VPSS_INPUT_0;
	
	vpssGrpAttr.enInputType = E_INPUT_TYPE_MEM;
	vpssGrpAttr.u32ViIsVpssBuf = VIM_TRUE; //VIM_FALSE; /**这里只能用内部缓存的方式，用外部缓存没有图像输出**/

	vpssGrpAttr.stGrpInput.u16Width = inW;
	vpssGrpAttr.stGrpInput.u16Height = inH;
	vpssGrpAttr.stGrpInput.bMirror = VIM_FALSE;
	vpssGrpAttr.stGrpInput.enCrop = VIM_FALSE;
	vpssGrpAttr.stGrpInput.u16CropLeft = 0;
	vpssGrpAttr.stGrpInput.u16CropTop = 0;
	vpssGrpAttr.stGrpInput.u16CropWidth = 0;
	vpssGrpAttr.stGrpInput.u16CropHeight = 0;
	vpssGrpAttr.stGrpInput.enPixfmt= PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	vpssGrpAttr.stGrpInput.u16BufferNum = 4;
	vpssGrpAttr.stGrpInput.u32WidthStride = 0;
	vpssGrpAttr.stGrpInput.u32HeightStride = 0;

	/**单画面1920x1080全屏显示的通道**/
	stChnAttr.u16Width = otW;
	stChnAttr.u16Height = otH;
	stChnAttr.enPixfmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420; // PIXEL_FORMAT_YUV_SEMIPLANAR_420;	// PIXEL_FORMAT_RGB_PLANAR_601
	stChnAttr.enYcbcr = 0;									/**E_INPUT_YCBCR_DISABLE**/
	stChnAttr.enCframe = VIM_FALSE;

	stChnAttr.u32Numerator = 30;
	stChnAttr.u32Denominator = 30;
	stChnAttr.u16BufferNum = 12;
	stChnAttr.u32VoIsVpssBuf = 1;
	stChnAttr.enCrop = VIM_FALSE;
	stChnAttr.u16CropLeft = 0;
	stChnAttr.u16CropTop = 0;
	stChnAttr.u16CropWidth = 0;
	stChnAttr.u16CropHeight = 0;
	//logd_lc("Type -> PIXEL_FORMAT_RGB_PLANAR_601");
	logd_lc("u32GrpID: %d w:%d h:%d.\n",u32GrpID,vpssGrpAttr.stGrpInput.u16Width, vpssGrpAttr.stGrpInput.u16Height);

	s32Ret = VIM_MPI_VPSS_EnableDev();
	ASSERT_RET_RETURN(s32Ret);
	
	s32Ret = VIM_MPI_VPSS_CreatGrp(&vpssGrpAttr);
	ASSERT_RET_RETURN(s32Ret);
	
	s32Ret = VIM_MPI_VPSS_SetGrpInAttr(vpssGrpAttr.u32GrpId, &vpssGrpAttr);
	ASSERT_RET_RETURN(s32Ret);


	if(vpssGrpAttr.u32ViIsVpssBuf == VIM_FALSE)
	{
		VIM_VPSS_GROUP_ATTR_S vpssGrpAttr1	= {0};
		VIM_VPSS_GROUP_ATTR_S vpssGrpAttr2	= {0};
		VIM_STREAM_DMA_BUFFER pstUserBuff;
		
		VIM_MPI_VPSS_GetGrpAttr(u32GrpID, &vpssGrpAttr1);
		vpssGrpAttr1.stGrpInput.u16BufferNum = 4;
		VIM_MPI_VPSS_SetGrpInAttr(u32GrpID, &vpssGrpAttr1);
		VIM_MPI_VPSS_GetGrpAttr(u32GrpID, &vpssGrpAttr2);
		
		logd_lc("stGrpInput.u16BufferNum-->%d", vpssGrpAttr2.stGrpInput.u16BufferNum);
		for(int i = 0; i<vpssGrpAttr2.stGrpInput.u16BufferNum; i++)
		{
			memset( &pstUserBuff, 0, sizeof(VIM_STREAM_DMA_BUFFER));
			/**当u32ViIsVpssBuf为FALSE时，一定要配置用户buffer，否则在VIM_MPI_VPSS_SendFrame的时候会提示通道没打开**/
			s32Ret = VIM_MPI_VPSS_SetGrpInUserBufs(u32GrpID, i, &pstUserBuff);
		}
	}

	logd_lc("vpss config u32GrpID:%d chn:%d.. \n", u32GrpID, u32ChnId);

	s32Ret = VIM_MPI_VPSS_SetChnOutAttr(u32GrpID, u32ChnId, &stChnAttr);				
	ASSERT_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_VPSS_EnableChn(u32GrpID, u32ChnId);			
	ASSERT_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_VPSS_StartChn(u32GrpID, u32ChnId);
	ASSERT_RET_RETURN(s32Ret);
	logd_lc("StartChn-->G: %d, C: %d\n", u32GrpID, u32ChnId);
	gVpssImageSave = 1;
	logd_lc("lc_dvr_vpss_jpeg_creat finish!");
	return s32Ret;
}

VIM_VOID lc_dvr_assist_vpss_run(int times)
{
	pthread_t vpss_jpeg_thread;

	gThreadVpssRun = times;
    if (pthread_create(&vpss_jpeg_thread, NULL, (void*)lc_dvr_vpss_stream_thread, NULL) == 0)
    {
        //logd_lc("~vpss_jpeg_thread SUCCESS\n");
        pthread_detach(vpss_jpeg_thread);
    }	
}

VIM_S32 lc_dvr_assist_vpss_config(VIM_U32 width, VIM_U32 height)
{
    VIM_S32 s32Ret = -1;
    VIM_U32 u32GrpID = LC_DVR_JPEG_GROUP_ID;
    VIM_U32 u32ChnId = LC_DVR_JPEG_CHANEL_ID;
	VIM_CHN_CFG_ATTR_S stChnAttr = {0};

	s32Ret = VIM_MPI_VPSS_GetChnOutAttr(u32GrpID, u32ChnId, &stChnAttr);
	ASSERT_RET_RETURN(s32Ret);
	
	s32Ret = VIM_MPI_VPSS_StopChn(u32GrpID, u32ChnId);		
	ASSERT_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_VPSS_DisableChn(u32GrpID, u32ChnId);
	ASSERT_RET_RETURN(s32Ret);	
	
    stChnAttr.u16Width = width;
    stChnAttr.u16Height = height;
	
	s32Ret = VIM_MPI_VPSS_EnableChn(u32GrpID, u32ChnId);
	ASSERT_RET_RETURN(s32Ret);		
	
	s32Ret = VIM_MPI_VPSS_StartChn(u32GrpID, u32ChnId);		
	ASSERT_RET_RETURN(s32Ret);

	return 0;
}

VIM_S32 lc_dvr_assist_vpss_destroy(VIM_VOID)
{
	return lc_dvr_vpss_destroy(LC_DVR_JPEG_GROUP_ID, LC_DVR_JPEG_CHANEL_ID);
}
