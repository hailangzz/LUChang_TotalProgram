#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>	   // mkfifo
#include <errno.h>
#include <unistd.h>

#include "lc_dvr_comm_def.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "lc_codec_api.h"
#include "lc_dvr_record_log.h"

#define FIFO_NAME_W0 "/tmp/Lcfifo"
#define FIFO_NAME_W1 "/tmp/Lcfifo1"
#define FIFO_NAME_W2 "/tmp/Lcfifo2"
#define FIFO_NAME_W3 "/tmp/Lcfifo3"

char *mFifoName[4]={
	{FIFO_NAME_W0},
	{FIFO_NAME_W1},
	{FIFO_NAME_W2},
	{FIFO_NAME_W3},
};

static VIM_S32 mFd_pipe[8];
static VIM_S32 mVencStart[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
static VIM_S32 mPipeThreadRun = 0;
static pthread_mutex_t mVENCMutex;
static int mVencinitialSign=0;

extern void vc_message_send(int eMsg, int val, void* para, void* func);

VIM_S32 lc_venc_pipe_creat(int index)	
{ 
	/** 若fifo已存在，则直接使用，否则创建它**/
	mFd_pipe[index] = -1;
	if((mkfifo(mFifoName[index-4],0777) == -1) && errno != EEXIST){
		loge_lc("mkfifo write failure\n");
	}
 
	/**以非阻塞型只写方式打开fifo，扫描方式判断是否成功**/
	mFd_pipe[index] = open(mFifoName[index-4],O_WRONLY | O_NONBLOCK);
	//logd_lc("pipe_creat-->%s\n", mFifoName[index-4]);
	return mFd_pipe[index];
}

VIM_S32 lc_venc_pipe_destroy(int index)
{
	int retval = close(mFd_pipe[index]);	
	logd_lc("lc_venc_pipe_destroy[%d]->%d", index, retval);
	mFd_pipe[index] = -1;
	return 0;
}

void *vc_pipeCreat_handler(void *arg)
{
	VIM_S32 s32Ret;
	int* s32Ptr = (int*)arg;
	int pipeChanel = s32Ptr[0];
	logd_lc("pipe[%d] creat thread begin!", pipeChanel);
	
    while (mPipeThreadRun)
    {	
		s32Ret = lc_venc_pipe_creat(pipeChanel);
		if(s32Ret<0) 
			usleep(250*1000);
		else 
			break;
    }
	mPipeThreadRun = 0;
	logd_lc("pipe[%d] creat ok!", pipeChanel);
	return NULL;
}

// #define LC_STREAM_TEST_SAVE

#ifdef LC_STREAM_TEST_SAVE
FILE* TestStreamFile=NULL;
VIM_U32 StreamSize = 0;
VIM_U32 StreamFileCnt = 0;
VIM_U32 StreamDebugCnt = 0;
#endif

static VIM_S32 lc_venc_pipe_write(void* data, VIM_U32 len, int index)
{	
	int tryTime, retval = 0;
	int byteCnt, remain, curLen = 0;
	char *dPtr = (char*)data;
	if(mFd_pipe[index]<0) return 0;
	
	//retval = fpathconf(mFd_pipe[index], 0);
	//logd_lc("pipe size:%d", retval);
	tryTime = 0;
	for(remain = len; ; )
	{
		byteCnt = remain>(96*1024)?(96*1024):remain;		
		retval = write(mFd_pipe[index], dPtr, byteCnt);
		
//		if(retval<0)
//		{
//			printf("pipe retry(%d)\n", tryTime);
//			retval = 0;
//		}		
//		remain -= retval;

		remain -= byteCnt;
		if(remain == 0) break;
		
		dPtr += byteCnt;
		usleep(15*1000);
	}
#ifdef LC_STREAM_TEST_SAVE	
	if(index == 4)
	{
		if(TestStreamFile==NULL)
		{
			char fileName[128];
			struct tm *tm = NULL;		
			getDateTime(&tm);
			
			sprintf(fileName, "/data/OriStream%02d_%02d_%02d.ts", tm->tm_hour, tm->tm_min, tm->tm_sec);
			TestStreamFile = fopen(fileName, "wb");
			if(TestStreamFile!=NULL)
			{
				logd_lc("StreamFile creat %s success!!\n", fileName);
			}
			StreamFileCnt++;
		}
		
		if(TestStreamFile!=NULL)
		{
			if(StreamSize<32*1024*1024)
			{
				fwrite(data, len, 1, TestStreamFile);
				
				StreamSize += len;
				StreamDebugCnt += len;
				if(StreamDebugCnt>2*1024*1024)
				{
					logd_lc("Pipe file:%d ", StreamSize);
					StreamDebugCnt = 0;
				}
			}
			else 
			{
				fflush(TestStreamFile);
				fclose(TestStreamFile);
				TestStreamFile = NULL;
				StreamSize = 0;
			}
		}
	}
#endif	
	return retval;
}

static VIM_S32 lc_venc_dma_copy(VIM_VPSS_FRAME_INFO_S *stFrameInfo, VIDEO_FRAME_INFO_S *inputFrame)
{
	VIM_S32 fd, memfd, id;
	struct userdma_user_info info = {0}; 
    struct userdma_lli vmem[MAX_LLI_NUM] = {0};
	VIM_U32 DST;
	VIM_U32 YLen, UVLen;
	VIM_S32 ret;		
	VIM_U32 offset = 0;
	
	YLen = stFrameInfo->u32ByteUsedStd[0];
	UVLen = YLen>>1;//stFrameInfo->u32ByteUsedStd[1];
	DST = inputFrame->stVFrame.u32PhyAddr[0];
	
	if(inputFrame->stVFrame.u32Size[0]>stFrameInfo->u32ByteUsedStd[0])
	{/**消除VPSS输出图像VEN的输入图像尺寸不匹配造成的粉红彩边**/
		offset = (inputFrame->stVFrame.u32Size[0] - stFrameInfo->u32ByteUsedStd[0])>>1;
		memset(inputFrame->stVFrame.pVirAddr[0], 0x10, offset);
		memset(inputFrame->stVFrame.pVirAddr[0]+offset+YLen, 0x10, offset);
		memset(inputFrame->stVFrame.pVirAddr[0]+inputFrame->stVFrame.u32Size[0], 0x80, (offset>>1));
		memset(inputFrame->stVFrame.pVirAddr[0]+inputFrame->stVFrame.u32Size[0]+(offset>>1)+(YLen>>1), 0x80, (offset>>1));		
	}

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
	info.listnr = 2; /**yuv420sp格式**/
	info.llis[0].src = stFrameInfo->pPhyAddrStd[0];
	info.llis[0].dst = DST+offset;
	info.llis[0].len = YLen;
	vmem[0].src = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].src, YLen);
	vmem[0].dst = (unsigned int)VIM_PLAT_MapMem(memfd, info.llis[0].dst, YLen);
	vmem[0].len = YLen;

	info.llis[1].src = stFrameInfo->pPhyAddrStd[1];
	info.llis[1].dst = DST+inputFrame->stVFrame.u32Size[0]+(offset>>1);
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

static VIM_S32 lc_venc_stream_save(FILE* fpFile, VENC_STREAM_S* pstStream, int index)
{
	VIM_U32 i;
	VIM_U32 count = 0;
	VIM_U8 *buf =NULL;

	for (i = 0; i < pstStream->u32PackCount; i++)
	{ 
		if(pstStream->pstPack[i]&&pstStream->pstPack[i]->pu8Addr&&pstStream->pstPack[i]->u32Len){
			//logd_lc("patPack[%d]->%d\n", i, pstStream->pstPack[i]->u32Len);	
			lc_venc_pipe_write(pstStream->pstPack[i]->pu8Addr + pstStream->pstPack[i]->u32Offset, pstStream->pstPack[i]->u32Len, index);
			//logd_lc("pipe write-->%d", retVal);
			for(count=0;count<pstStream->pstPack[i]->u32DataNum;count++){
				buf = pstStream->pstPack[i]->pu8Addr+pstStream->pstPack[i]->stPackInfo[count].u32PackOffset;
				if(!((*buf==0x0)&&(*(buf+1)==0x0)&&(*(buf+2)==0x0)&&(*(buf+3)==0x01))){
					logd_lc("i = %d pu8Addr=%p u32Offset=0x%x u32Len[0]=0x%x u32DataNum=%d\n",i,pstStream->pstPack[i]->pu8Addr,
						pstStream->pstPack[i]->u32Offset,pstStream->pstPack[i]->u32Len,pstStream->pstPack[i]->u32DataNum);
					loge_lc("the stream info sometimes is incorrect\n");
				 }
			}
		}
		else if(i==0){
			loge_lc("the stream is abort\n");
		}
	}

	return VIM_SUCCESS;
}

static VIM_S32 lc_venc_start(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN VencChn = venc_cfg_config->VeChn;
	VIM_BOOL bEnableTsvc = 0;
	///<	VENC_ROI_CFG_S pstVencRoiCfg;

	s32Ret = VIM_MPI_VENC_StartRecvPic(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		loge_lc("VIM_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
		return VIM_FAILURE;
	}

	VIM_MPI_VENC_GetTsvc(VencChn, &bEnableTsvc);
	logd_lc("bEnableTsvc=%d\n", bEnableTsvc);

	VENC_SVC_PARAM_S pstSvcParam;

	s32Ret = VIM_MPI_VENC_GetSsvc(VencChn, &pstSvcParam);
	if (s32Ret)
	{
		loge_lc("VIM_MPI_VENC_GetSsvc faild with %#x!\n", s32Ret);
	}

	logd_lc("bEnableSvc=%d  u8SvcMode=%d u8SvcRation=%d\n",
					pstSvcParam.bEnableSvc, pstSvcParam.u8SvcMode, pstSvcParam.u8SvcRation);

	VIM_BOOL bEnable;

	s32Ret = VIM_MPI_VENC_GetEnRoi(VencChn, &bEnable);
	if (s32Ret)
	{
		loge_lc("VIM_MPI_VENC_SetEnRoi faild with %#x!\n", s32Ret);
		return s32Ret;
	}
	logd_lc("VIM_MPI_VENC_GetEnRoi bEnable=%d\n", bEnable);

	if (venc_cfg_config->EnSAO)
	{
		bEnable = venc_cfg_config->EnSAO;
		s32Ret = VIM_MPI_VENC_GetSao(VencChn, &bEnable);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetSao faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}
	logd_lc("VIM_MPI_VENC_GetSao bEnable=%d\n", bEnable);

	logd_lc("exit!\n");

	return VIM_SUCCESS;
}

static VIM_S32 lc_venc_stop(VENC_CHN VencChn)
{
	VIM_S32 s32Ret;

	logd_lc("enter VencChn=%d!\n", VencChn);

	s32Ret = VIM_MPI_VENC_StopRecvPic(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		loge_lc("VIM_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
		return VIM_FAILURE;
	}

	logd_lc("exit VencChn=%d!\n", VencChn);
#ifdef LC_STREAM_TEST_SAVE
	if(TestStreamFile!=NULL)
	{
		fflush(TestStreamFile);
		fclose(TestStreamFile);
		TestStreamFile = NULL;
		StreamSize = 0;
	}
#endif
	return VIM_SUCCESS;
}

VIM_S32 lc_venc_open(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN_ATTR_S stVencChnAttr;
	VENC_CHN VencChn = venc_cfg_config->VeChn;
	pthread_t pipe_creat_thread;

	if(mVencinitialSign==0)
	{
		pthread_mutex_init(&mVENCMutex, NULL);
		mVencinitialSign++;
	}
	
	if(VencChn<4) return -1;
	if(!(mVencStart[VencChn]<0)) return -2;

	logd_lc("%s build data: %s time: %s\n", __func__, __DATE__, __TIME__);
	logd_lc("enter VencChn=%d!\n", VencChn);
	memset(&stVencChnAttr, 0x0, sizeof(VENC_CHN_ATTR_S));

	/******************************************
	 *step 1:  Create Venc Channel
	 ******************************************/
	stVencChnAttr.stVeAttr.enType = venc_cfg_config->enType;
	stVencChnAttr.stGopAttr.enGopMode = venc_cfg_config->enGopMode;
	switch (stVencChnAttr.stVeAttr.enType)
	{
	case PT_H265:
	{
		VENC_ATTR_H265_S stH265Attr;

		stH265Attr.u32MaxPicWidth = venc_cfg_config->u32Width;
		stH265Attr.u32MaxPicHeight = venc_cfg_config->u32Height;
		stH265Attr.u32PicWidth = venc_cfg_config->u32PicWidth;
		stH265Attr.u32PicHeight = venc_cfg_config->u32PicHeight;
		stH265Attr.u32BufSize = venc_cfg_config->u32BufSize;
		stH265Attr.u32Profile = venc_cfg_config->u32Profile;
		stH265Attr.bByFrame = venc_cfg_config->bByFrame;
		memcpy(&stVencChnAttr.stVeAttr.stAttrH265e, &stH265Attr, sizeof(VENC_ATTR_H265_S));

		if (VENC_RC_MODE_H265CBR == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_H265_CBR_S stH265Cbr;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
			stH265Cbr.u32Gop = venc_cfg_config->u32Gop;
			stH265Cbr.u32StatTime = venc_cfg_config->u32StatTime;
			stH265Cbr.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stH265Cbr.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stH265Cbr.u32BitRate = venc_cfg_config->u32BitRate;
			stH265Cbr.u32MaxQp = venc_cfg_config->u32MaxQp;
			stH265Cbr.u32MinQp = venc_cfg_config->u32MinQp;
			stH265Cbr.u32FluctuateLevel = venc_cfg_config->u32FluctuateLevel; /* average bit rate */
			logd_lc("h265 CBR mode stH265Cbr.u32BitRate= %dKbit/s\n", stH265Cbr.u32BitRate);
			logd_lc("h265 Target frame = %d\n", stH265Cbr.fr32TargetFrmRate);
			memcpy(&stVencChnAttr.stRcAttr.stAttrH265Cbr, &stH265Cbr, sizeof(VENC_ATTR_H265_CBR_S));
		}
		else if (VENC_RC_MODE_H265FIXQP == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_H265_FIXQP_S stH265FixQp;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
			stH265FixQp.u32Gop = venc_cfg_config->u32Gop;
			stH265FixQp.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stH265FixQp.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stH265FixQp.u32IQp = venc_cfg_config->u32IQp;
			stH265FixQp.u32PQp = venc_cfg_config->u32PQp;
			stH265FixQp.u32BQp = venc_cfg_config->u32BQp;
			memcpy(&stVencChnAttr.stRcAttr.stAttrH265FixQp, &stH265FixQp, sizeof(VENC_ATTR_H265_FIXQP_S));
			logd_lc("PT_H265 FIXQP mode: I frame: %d, P frame: %d\n\r",
				            stH265FixQp.u32IQp,
				            stH265FixQp.u32PQp);
		}
		else
		{
			loge_lc("h265 error invalid encode enRcMode!\n");
			return -3;
		}
	}

	break;

	case PT_SVAC2:
	{
		VENC_ATTR_SVAC_S stSvacAttr;

		stSvacAttr.u32MaxPicWidth = venc_cfg_config->u32MaxPicWidth;
		stSvacAttr.u32MaxPicHeight = venc_cfg_config->u32MaxPicHeight;
		stSvacAttr.u32PicWidth = venc_cfg_config->u32PicWidth;
		stSvacAttr.u32PicHeight = venc_cfg_config->u32PicHeight;
		stSvacAttr.u32BufSize = venc_cfg_config->u32BufSize;
		stSvacAttr.u32Profile = venc_cfg_config->u32Profile;
		stSvacAttr.bByFrame = venc_cfg_config->bByFrame;
		stSvacAttr.flag_2vdsp = venc_cfg_config->flag_2vdsp;
		memcpy(&stVencChnAttr.stVeAttr.stAttrSvac, &stSvacAttr, sizeof(VENC_ATTR_SVAC_S));

		if (VENC_RC_MODE_SVACCBR == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_SVAC_CBR_S stSvacCbr;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_SVACCBR;
			stSvacCbr.u32Gop = venc_cfg_config->u32Gop;
			stSvacCbr.u32StatTime = venc_cfg_config->u32StatTime;
			stSvacCbr.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stSvacCbr.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stSvacCbr.u32FluctuateLevel = venc_cfg_config->u32FluctuateLevel;
			stSvacCbr.u32MaxQp = venc_cfg_config->u32MaxQp;
			stSvacCbr.u32MinQp = venc_cfg_config->u32MinQp;
			stSvacCbr.specialheader = venc_cfg_config->specialheader;
			stSvacCbr.staicRefPerid = venc_cfg_config->staicRefPerid;
			stSvacCbr.u32BitRate = venc_cfg_config->u32BitRate;
			logd_lc("svac2 CBR mode stSvacCbr.u32BitRate = %dkbit/s\n", stSvacCbr.u32BitRate);
			memcpy(&stVencChnAttr.stRcAttr.stAttrSvacCbr, &stSvacCbr, sizeof(VENC_ATTR_SVAC_CBR_S));
		}
		else if (VENC_RC_MODE_SVACFIXQP == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_SVAC_FIXQP_S stSvacFixQp;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_SVACFIXQP;
			stSvacFixQp.u32Gop = venc_cfg_config->u32Gop;
			stSvacFixQp.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stSvacFixQp.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stSvacFixQp.u32IQp = venc_cfg_config->u32IQp;
			stSvacFixQp.u32PQp = venc_cfg_config->u32PQp;
			stSvacFixQp.u32BQp = venc_cfg_config->u32BQp;
			stSvacFixQp.specialheader = venc_cfg_config->specialheader;
			stSvacFixQp.staicRefPerid = venc_cfg_config->staicRefPerid;
			logd_lc("FIXQP mode: I frame: %d, P frame: %d\n\r", stSvacFixQp.u32IQp, stSvacFixQp.u32PQp);
			memcpy(&stVencChnAttr.stRcAttr.stAttrSvacFixQp, &stSvacFixQp, sizeof(VENC_ATTR_SVAC_FIXQP_S));
		}
		else
		{
			loge_lc("svac2 error invalid encode enRcMode!\n");
			return -4;
		}
	}

	break;

	default:
	{
		loge_lc("error invalid encode type!\n");
		return -5;
	}
	}

	stVencChnAttr.pool_id = VB_INVALID_POOLID;
	if (venc_cfg_config->dst_enFormat == 1)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_PLANAR_420;
	}
	else if (venc_cfg_config->dst_enFormat == 2)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_PLANAR_420_P10;
	}
	else if (venc_cfg_config->dst_enFormat == 3)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	}
	else if (venc_cfg_config->dst_enFormat == 4)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10;
	}
	else
	{
		loge_lc(" invalid format enPixelFormat=%s u32PixelMode=%d\n",
						venc_cfg_config->enPixelFormat, venc_cfg_config->u32PixelMode);
		return -6;
	}
	
	logd_lc("enFormat=%d\n", stVencChnAttr.stVeAttr.enFormat);
	s32Ret = VIM_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		loge_lc("VIM_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
		return -7;
	}

	/******************************************
	 *step 2:  Set Venc ConfPara
	 ******************************************/
	VENC_EXT_PARAM_S vencExtParat;

	memset(&vencExtParat, 0x0, sizeof(VENC_EXT_PARAM_S));
	vencExtParat.u32BFrame = venc_cfg_config->bByFrame;
	vencExtParat.cframe_param.bcframe50Enable = venc_cfg_config->bcframe50Enable;
	vencExtParat.cframe_param.bcframe50LosslessEnable = venc_cfg_config->bcframe50LosslessEnable;
	vencExtParat.cframe_param.cframe50Tx16C = venc_cfg_config->cframe50Tx16C;
	vencExtParat.cframe_param.cframe50Tx16Y = venc_cfg_config->cframe50Tx16Y;
	///<	vencExtParat.Reserved0[0] = 1;
	s32Ret = VIM_MPI_VENC_SetConf(VencChn, &vencExtParat);
	if (s32Ret)
	{
		loge_lc("VIM_MPI_VENC_SetConf faild with %#x!\n", s32Ret);
		return -8;
	}

	VENC_ROTATION_CFG_S RotationInfo;

	RotationInfo.bEnableRotation = venc_cfg_config->bEnableRotation;
	RotationInfo.u32rotAngle = venc_cfg_config->u32rotAngle;
	RotationInfo.enmirDir = venc_cfg_config->enmirDir;
	s32Ret = VIM_MPI_VENC_SetRotation(VencChn, &RotationInfo);
	if (s32Ret)
	{
		loge_lc("VIM_MPI_VENC_SetRotation faild with %#x!\n", s32Ret);
		return -9;
	}

	if (venc_cfg_config->bEnableSvc)
	{
		VENC_SVC_PARAM_S pstSvcParam;

		pstSvcParam.bEnableSvc = venc_cfg_config->bEnableSvc;
		pstSvcParam.u8SvcMode = venc_cfg_config->u8SvcMode;
		pstSvcParam.u8SvcRation = venc_cfg_config->u8SvcRation;
		s32Ret = VIM_MPI_VENC_SetSsvc(VencChn, &pstSvcParam);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetSsvc faild with %#x!\n", s32Ret);
			return -10;
		}
	}

	if (venc_cfg_config->bEnableTsvc)
	{
		s32Ret = VIM_MPI_VENC_SetTsvc(VencChn, venc_cfg_config->bEnableTsvc);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetTsvc faild with %#x!\n", s32Ret);
			return -11;
		}
	}

	if (venc_cfg_config->EnRoi)
	{
		s32Ret = VIM_MPI_VENC_SetEnRoi(VencChn, venc_cfg_config->EnRoi);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetEnRoi faild with %#x!\n", s32Ret);
			return -12;
		}
	}

	if (venc_cfg_config->EnSAO)
	{
		s32Ret = VIM_MPI_VENC_SetSao(VencChn, venc_cfg_config->EnSAO);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetSao faild with %#x!\n", s32Ret);
			return -13;
		}
	}

	if (venc_cfg_config->bEnableLongTerm)
	{
		VENC_LONGTERM_REFPERIOD_CFG_S LongTermInfo;

		LongTermInfo.bEnableLongTerm = venc_cfg_config->bEnableLongTerm;
		LongTermInfo.u32AsLongtermPeriod = venc_cfg_config->u32AsLongtermPeriod;
		LongTermInfo.u32refLongtermPeriod = venc_cfg_config->u32refLongtermPeriod;
		s32Ret = VIM_MPI_VENC_SetLongTerm(VencChn, &LongTermInfo);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetLongTerm faild with %#x!\n", s32Ret);
			return -14;
		}
	}

	if (venc_cfg_config->bEnable)
	{
		VENC_ROI_CFG_S pstVencRoiCfg;

		pstVencRoiCfg.u32Index = venc_cfg_config->u32Index;
		pstVencRoiCfg.bEnable = venc_cfg_config->bEnable;
		pstVencRoiCfg.bAbsQp = venc_cfg_config->bAbsQp;
		pstVencRoiCfg.s32Qp = venc_cfg_config->s32Qp;
		pstVencRoiCfg.s32AvgOp = venc_cfg_config->s32AvgOp;
		pstVencRoiCfg.s32Quality = venc_cfg_config->s32Quality;
		pstVencRoiCfg.bBKEnable = venc_cfg_config->bBKEnable;
		pstVencRoiCfg.stRect.s32X = venc_cfg_config->s32X;
		pstVencRoiCfg.stRect.s32Y = venc_cfg_config->s32Y;
		pstVencRoiCfg.stRect.u32Width = venc_cfg_config->u32Width;
		pstVencRoiCfg.stRect.u32Height = venc_cfg_config->u32Height;
		s32Ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &pstVencRoiCfg);
		if (s32Ret)
		{
			loge_lc("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
		}
	}
	logd_lc("lc_venc_start->bitrate:%d, frame:%d", venc_cfg_config->u32BitRate, venc_cfg_config->fr32TargetFrmRate);
	lc_venc_start(venc_cfg_config);
	mVencStart[VencChn] = VencChn;
	
	{
		mPipeThreadRun = 1;
    s32Ret = pthread_create(&pipe_creat_thread, NULL, (void*)vc_pipeCreat_handler, &mVencStart[VencChn]);
    if (0 == s32Ret)
    {
	        logd_lc("pipe[%d] creat pthread SUCCESS\n", VencChn);
        pthread_detach(pipe_creat_thread);
    }	
	}
	logd_lc(" exit VencChn=%d\n", VencChn);
	return VIM_SUCCESS;
}

VIM_S32 lc_venc_close(VENC_CHN VencChn)
{
	VIM_S32 s32Ret;
	VENC_CHN_ATTR_S pstAttr;
	if(mVencinitialSign!=0)
	{
		mVencinitialSign--;
		if(mVencinitialSign==0)
		{
			pthread_mutex_destroy(&mVENCMutex);
		}
	}

	if(mVencStart[VencChn]<0) return VIM_FAILURE;	
	
	usleep(100*1000);

	logd_lc("VencChn:%d enter!\n", VencChn);
	lc_venc_stop(VencChn);

	s32Ret = VIM_MPI_VENC_GetChnAttr(VencChn, &pstAttr);
	if (s32Ret)
	{
		loge_lc("\nVIM_MPI_VENC_GetChnAttr failed, s32Ret = 0x%x\n", s32Ret);
		return s32Ret;
	}

	/******************************************
	 *step 1:  Distroy Venc Channel
	 ******************************************/
	s32Ret = VIM_MPI_VENC_DestroyChn(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		loge_lc("VIM_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n",
						VencChn, s32Ret);
		return VIM_FAILURE;
	}
	
	lc_venc_pipe_destroy(VencChn);
	mVencStart[VencChn] = -1;
	logd_lc("VencChn:%d exit!\n", VencChn);
	
	return VIM_SUCCESS;
}

VIM_S32 lc_venc_sendFrame_getStream(VIM_U32 VencChn, VIM_VPSS_FRAME_INFO_S *stFrameInfo)/**选择编码通道**/
{
	VIM_S32 ret = 0;
	VENC_STREAM_S stStream;
	VENC_CHN_STAT_S stStat;
	VIM_U32 i = 0;
	VIDEO_FRAME_INFO_S inputFrame;
	VIM_U32 cur_count;
	int retval = 0;
	
	if(mVencStart[VencChn]<0) return -1;
	
	pthread_mutex_lock(&mVENCMutex);
	//logd_lc("%s enter\n",__FUNCTION__);
	ret = VIM_MPI_VENC_GetEmptyFramebuf(VencChn,&inputFrame,0); /** 要跟VIM_MPI_VENC_SendFrame成对出现 **/
	if(ret != VIM_SUCCESS){
		if(ret == VIM_ERR_VENC_BUF_FULL){
			retval = -1;
			goto EXIT_VENC_SEND_FRAME;
		}else{
			loge_lc("VIM_MPI_VENC_GetEmptyFramebuf some error chn=%d ret=0x%x\n",VencChn,ret);
			retval = -2;
			goto EXIT_VENC_SEND_FRAME;
		}
	}	
	/**========================装填图像数据========================**/
	{
		struct timeval t_start,t_end;
		gettimeofday(&t_start, NULL);
		lc_venc_dma_copy(stFrameInfo, &inputFrame);		
		gettimeofday(&t_end, NULL);
		// logd_lc("cpTime: %ld us\n", t_end.tv_usec - t_start.tv_usec);
//		{/**测试数据复制是否成功**/
//			int yLen = stFrameInfo->u32ByteUsedStd[0];
//			int uvLen = stFrameInfo->u32ByteUsedStd[1];
//			VIM_U32 *cmp_src, *cmp_dst;
//			VIM_U32 cmpLen = uvLen/sizeof(VIM_U32);
//			VIM_S32 different = -1;
//			cmp_src = (VIM_U32*)inputFrame.stVFrame.pVirAddr[0];
//			cmp_src += yLen/sizeof(VIM_U32); 
//			cmp_dst = (VIM_U32*)stFrameInfo->pVirAddrStd[1];
//			//cmp_src[1024] = 0x55aa;
//			for(int i=0; i<cmpLen; i++)
//			{
//				if(cmp_src[i]!=cmp_dst[i])
//				{
//					different = i;
//					break;
//				}
//			}
//			logd_lc("UV Different position -->%d\n", different);
//		}
	}
	
	//================================================================	
	ret = VIM_MPI_VENC_SendFrame(VencChn, &inputFrame, 0);
	if(ret < 0) {
		loge_lc("VIM_MPI_VENC_SendFrame vechn[%d] failed with %#x!\n", VencChn, ret);
		retval = -3;
		goto EXIT_VENC_SEND_FRAME;
	}

	{
		/*******************************************************
		step 2.1 : query how many packs in one-frame stream.
		SAMPLE_VENC_PRT(" chn_num=%d  sendframe_num[%d]=%d chn_count=%d\n", i, i,sendframe_num[i],chn_count);
		*******************************************************/
		memset(&stStream, 0, sizeof(stStream));
		ret = VIM_MPI_VENC_Query(VencChn, &stStat);
		if (VIM_SUCCESS != ret){
			loge_lc("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", i, ret);
			retval = -4;
			goto EXIT_VENC_SEND_FRAME;
		}

		/*******************************************************
		step 2.2 : malloc corresponding number of pack nodes.
		*******************************************************/
		stStream.pstPack[0] = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
		if (NULL == stStream.pstPack[0]){
			loge_lc("malloc stream pack failed!\n");
			retval = -5;
			goto EXIT_VENC_SEND_FRAME;
		}

		for(cur_count=1;cur_count<stStat.u32CurPacks;cur_count++){
			stStream.pstPack[cur_count] = (VENC_PACK_S*)((char*)stStream.pstPack[cur_count-1]+sizeof(VENC_PACK_S));
		}

		/*******************************************************
		step 2.3 : call mpi to get one-frame stream
		*******************************************************/
		//stStream.u32PackCount = stStat.u32CurPacks;
		ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, 3000);
		if (VIM_SUCCESS != ret){
			free(stStream.pstPack[0]);
			stStream.pstPack[0] = NULL;
			loge_lc("chn:%d VIM_MPI_VENC_GetStream failed with %#x!\n",VencChn, ret);
			retval = -6;
			goto EXIT_VENC_SEND_FRAME;
		}

		/*******************************************************
		step 2.4 : save frame to file
		*******************************************************/	
		{
			/**在这里处理编码后的码流**/
			lc_venc_stream_save(NULL, &stStream, VencChn);
		}
		
		/*******************************************************
		step 2.5 : release stream
		*******************************************************/
		ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
		if (VIM_SUCCESS != ret){
			free(stStream.pstPack[0]);
			stStream.pstPack[0] = NULL;
			loge_lc("chn%d release stream failed!\n",VencChn);
			retval = -7;
			goto EXIT_VENC_SEND_FRAME;
		}

		/*******************************************************
		step 2.6 : free pack nodes
		*******************************************************/
		free(stStream.pstPack[0]);
		stStream.pstPack[0] = NULL;
	}
EXIT_VENC_SEND_FRAME:	
	pthread_mutex_unlock(&mVENCMutex);
	return retval;
}

VIM_VOID lc_venc_para_from_file(SAMPLE_ENC_CFG *encCfg, VIM_S32 chanl)
{
	char cfgfilename[] = "/system/app/cfg/venc_h265_stream_out_4chn.cfg";
	FILE* pFile=NULL;
	VIM_S32 ChnCfgOffset = 0;
	VIM_S32 ChnCfgOffset_end = 0;	
	logd_lc("Parse(%s) chanel %d\n", cfgfilename, chanl);
 	pFile = fopen((void *)cfgfilename, "r");
    if (pFile == NULL) {
		loge_lc("Config file no exist!\n");
    }		
	GetCfgOffset(pFile,"ChnStartFlag",chanl,&ChnCfgOffset);
	GetCfgOffset(pFile,"ChnEndFlag",chanl,&ChnCfgOffset_end);
	logd_lc( "ChnCfgOffset = 0x%x ChnCfgOffset_end=0x%x\n", ChnCfgOffset,ChnCfgOffset_end);
	parseEncCfgFile(encCfg,(char *)cfgfilename,(VIM_U32)ChnCfgOffset,(VIM_U32)ChnCfgOffset_end);
	printParsePara(encCfg);
}

//static VIM_S32 lc_venc_getFilePostFix(PAYLOAD_TYPE_E enPayload, VIM_CHAR * szFilePostfix)
//{
//    if (PT_H264 == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".h264");
//    }
//    else if (PT_H265 == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".h265");
//    }
//    else if (PT_JPEG == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".jpg");
//    }
//    else if (PT_MJPEG == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".mjp");
//    }
//    else if (PT_MP4VIDEO == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".mp4");
//    }
//	else if (PT_SVAC == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".svac");
//    }
//	else if (PT_SVAC2 == enPayload)
//    {
//        strcpy((char *)szFilePostfix, ".svac2");
//    }
//    else
//    {
//        loge_lc("payload type err!\n");
//        return VIM_FAILURE;
//    }
//    return VIM_SUCCESS;
//}
