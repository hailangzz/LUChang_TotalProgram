#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>   /**struct fb_var_screeninfo**/
#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>  /** asked by PR_SET_NAME **/
#include <sys/ioctl.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>	   // mkfifo
#include <errno.h>

#include "osd.h"
#include "mpi_sys.h"
#include "lc_jpeg_api.h"
#include "lc_codec_api.h"
#include "sample_comm_vo.h"
#include "lc_dvr_record_log.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_interface_4_vim.h"
#include "lc_glob_type.h"
#include "lc_dvr_glob_def.h"
#include "sample_venc_cfgParser.h"
#include "lc_dvr_semaphore.h"
#include "VimMediaType.h"
#include "lc_dvr_semaphore.h"

#define RECORD_FILE_TOTAL 8
#define VIDEO_CONFIG_BLOCK_LEN  8192
#define VIDEO_CONFIG_DBSAVE_PATH "/data/etc/lcVideoCfgList.dat"

//#define USE_STAND_FILEIO 

typedef struct{
	int handle;
	int cnt;
}FileHandleList;

static VIM_BOOL g_stream_out_flag;
extern VIM_BOOL View_Exit;
LC_VENC_STREAM_INFO mStreamCtr[4]={0};
ExtRwHanlder gVimFileHandl;
FileInfomation mCamRecordInfo[LC_DVR_SUPPORT_CAMERA_NUMBER];

static int mRecordStatus = 0;
static int mAHDDetectSign = 0;
static FileHandleList mFileHandlList[RECORD_FILE_TOTAL];
static int mVideoByteCnt;
static int mSamllByteCnt;
static int mMaxStreamByte;
static char mDynamicMount[64];
static char mMdvrRecordStatus;
static int mVideoCopyRun;
static void* mVideoReadHandle = NULL;
static int mVideoFileCour;
static int mVideoConfig[4];
//--------------------------------------
static int mImageLoopInterval = 0;
static int mImageTimeCnt = 0;
static int mImageLoopSize = 0;
static int mImageLoopChanel = 0;
static int mImageLoopNumber = 0;
//--------------------------------------
extern char *mFifoName[4];

extern void vc_message_send(int eMsg, int val, void* para, void* func);
extern void media_demux_start(char *fileName);
extern void mediaPlay_start(char *fileName);
extern void mediaPlay_stop(void);
extern void mediaPlay_seek(double time);
extern void mediaPlay_pause(int status);
extern void mediaPlay_speed(int speed);
extern void* mediaPlay_information(int *number);
extern void RecordImageSetSize(char* name, int size);
extern void vc_record_video_para_config_update(void);
extern void Record_video_status_info_save(int camId, int index, int size, char* start, char* stop);

void vc_func_after_image_take(int size, int nameInt)
{
	char *name = (char*)nameInt;
	logd_lc("after_image_take->%s(%d)", name, size);
	RecordImageSetSize(name, size);	
}

void vc_loop_image_ctrl(void* imgInfo)
{
	LC_DVR_LOOP_IMAGE_TAKE  *paraIn = (LC_DVR_LOOP_IMAGE_TAKE*)imgInfo;
	mImageLoopNumber = paraIn->number;
	mImageLoopInterval = paraIn->interval*25;
	mImageTimeCnt = mImageLoopInterval;
	mImageLoopSize = paraIn->imageSize;
	mImageLoopChanel = 0;	
	
	for(int i=0; i<4; i++)
	{
		if(paraIn->chnl_en[i])
		{
			mImageLoopChanel |= 1<<i;
		}
	}
	mImageLoopNumber = paraIn->number; /**两次赋值，防止停止时变量冲突**/
}

void vc_loop_image_handle(void)
{
	if(mImageLoopNumber)
	{
		if(mImageTimeCnt-- == 1)
		{
			mImageTimeCnt = mImageLoopInterval;
			
			for(int i=0; i<4; i++)
			{
				if(mImageLoopChanel&(1<<i))
				{
					LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)sys_msg_get_memcache(sizeof(LC_TAKE_PIC_INFO));
					switch(mImageLoopSize)
					{
						case 0:
							picInfo->pic_width = LC_TAKE_THUMB_PIC_SIZE_W;
							picInfo->pic_height = LC_TAKE_THUMB_PIC_SIZE_H;
							break;
						case 1:
							picInfo->pic_width = LC_TAKE_SMALL_PIC_SIZE_W;
							picInfo->pic_height = LC_TAKE_SMALL_PIC_SIZE_H;
							break;
						case 2:
							picInfo->pic_width = LC_TAKE_LARGE_PIC_SIZE_W;
							picInfo->pic_height = LC_TAKE_LARGE_PIC_SIZE_H;
							break;
					}
					
					picInfo->camChnl = i;
					picInfo->frameDelay = 0;
					picInfo->ackFun = vc_func_after_image_take;
					RecordGenImageFile(picInfo->pic_save_path, i);
					vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC, 0, picInfo, NULL);					
				}
			}

			mImageLoopNumber--;
		}
	}
}

VIM_VOID vc_Gsensor_event_record(void)
{
	VimStartEventRecord(0, 5);
	VimStartEventRecord(1, 5);
}

VIM_VOID *vc_record_para_get(void)
{
	logd_lc("RecConfig->t:%d, b:%d, m:%d, p:%d", mVideoConfig[VC_IDX_DURATION], mVideoConfig[VC_IDX_BITRATE], 
							mVideoConfig[VC_IDX_MUTE], mVideoConfig[VC_IDX_PHOTOSZ]);
	return mVideoConfig;
}


VIM_VOID vc_get_info_from_name(char *name, int handle)
{
	int len = strlen(name);
	int index, camId;
	int s1, s2=0;
	char *start, *src;
	char camChar;
	for(int i=len-1; i>=0; i--)
	{
		if(name[i]=='/')
		{
			s1 = i+1;
			break;
		}
		else if(name[i]=='_')
		{
			if(s2==0) s2=i-1;
		}
	}
	
	index = atoi(name+s1);
	camChar = name[s2];
	
	switch(camChar)
	{
		case CMA_DMS:
			camId = 0;
			break;
		case CMA_ADAS:
			camId = 1;
			break;
		case CMA_BRAKE:
			camId = 2;
			break;
		case CMA_REAR:
			camId = 3;
			break;		
	}
	
	mCamRecordInfo[camId].handle = handle;
	mCamRecordInfo[camId].index = index;
	start = mCamRecordInfo[camId].startTime;
	src = name+s2+4;

	//memset(mCamRecordInfo[camId].fileDir, 0, 128);
	//memcpy(mCamRecordInfo[camId].fileDir, name, s1);

	for(int i=0; i<6; i++)
	{
		start[i] = src[i*2+0]-'0';
		start[i] <<= 4;
		start[i] += src[i*2+1]-'0';
	}	

	
	//logd_lc("bcd time:%02x-%02x-%02x %02x:%02x:%02x", start[0],start[1],start[2],start[3],start[4],start[5]);	
}

void vc_save_info_to_file(int handle, int fileSize)
{
	for(int i=0; i<4; i++)
	{
		if(handle == mCamRecordInfo[i].handle)
		{
			time_t timer;
			struct tm *tm = NULL;
			char *stop = mCamRecordInfo[i].stopTime;
			int year, month; 
			timer = time(NULL);
			tm = localtime(&timer); 		
			mCamRecordInfo[i].size = fileSize;
			year = tm->tm_year-100;
			stop[0] = year/10;
			stop[0] <<= 4;
			stop[0] += year%10;
			
			month = tm->tm_mon + 1;
			stop[1] = month/10;
			stop[1] <<= 4;
			stop[1] += month%10;
	
			stop[2] = tm->tm_mday/10;
			stop[2] <<= 4;
			stop[2] += tm->tm_mday%10;
	
			stop[3] = tm->tm_hour/10;
			stop[3] <<= 4;
			stop[3] += tm->tm_hour%10;
	
			stop[4] = tm->tm_min/10;
			stop[4] <<= 4;
			stop[4] += tm->tm_min%10;
	
			stop[5] = tm->tm_sec/10;
			stop[5] <<= 4;
			stop[5] += tm->tm_sec%10;

			Record_video_status_info_save(i, mCamRecordInfo[i].index, fileSize, mCamRecordInfo[i].startTime, mCamRecordInfo[i].stopTime);
		}
	}
}

VIM_VOID vc_set_mdvr_record_status(int stauts)
{
	mMdvrRecordStatus = stauts;
}

int vc_get_mdvr_record_status(void)
{
	return mMdvrRecordStatus;
}

extern void lc_dvr_mem_debug(void* membuf, int len);
VIM_VOID vc_start_mediaFile_play(char* fileName)
{
	//mediaPlay_start(fileName);
	media_demux_start(fileName);	
}

VIM_VOID vc_start_mediaFile_stop(void)
{
	mediaPlay_stop();
}

VIM_VOID vc_media_play_seek(double time)
{
	mediaPlay_seek(time);
}

VIM_VOID vc_media_play_pause(int status)
{
	mediaPlay_pause(status);
}

VIM_VOID vc_media_play_speed(int speed)
{
	mediaPlay_speed(speed);
}

VIM_VOID *vc_media_play_notify_info(int *number)
{
	return mediaPlay_information(number);
}

VIM_VOID vc_set_record_status(int val)
{
	logd_lc("vc_set_record_status->%d", val);
	mRecordStatus = val;
}

VIM_S32 vc_get_record_status(void)
{
	return mRecordStatus;
}

VIM_VOID vc_set_AHDDetect_status(int val)
{
	mAHDDetectSign = val;
}

VIM_S32 vc_get_AHDDetect_status(void)
{
	return mAHDDetectSign;
}

VIM_VOID vc_chn_record_start(void)
{
#ifdef OSD_ENABLE
	osd_reset_license(RecordGetLicense());
#endif
	logd_lc("vc_chn_record_start");
    vc_set_AHDDetect_status(1);
}

VIM_S32 vc_chn_record_stop(void)
{
    VIM_S32 s32Ret = -1;
	vc_set_AHDDetect_status(0);
	vc_message_send(ENU_LC_DVR_NOTIFY_RECORD_STATUS, 0, NULL, NULL);
	
    for(int recCh = 0; recCh < 4; recCh++)
    {    	
		s32Ret = VimRecordChStop(recCh);
        if(s32Ret != 0)
        {
            logd_lc("error:VimRecordChStop ch:%d ret:%d\n", recCh, s32Ret);
        }
    }
	vc_set_record_status(0);
	return s32Ret;
}

void *vimOpen(const char *fileName, const char *modes)
{	
	void* fp;
	printf("vimOpen->%s, %s\n", fileName, modes);
	if(strstr(modes, "wb+")!=NULL)
	{
#ifdef USE_STAND_FILEIO
		fp = (void*)fopen(fileName, "rb+");
#else
		fp = (void*)lc_dvr_mem_write_cache_open(fileName, "rb+");
#endif
		mVideoByteCnt = 0;
		mSamllByteCnt = 0;
		mMaxStreamByte = 0;	
		vc_get_info_from_name(fileName, fp);	/**记录文件的序号和文件名**/
		logd_lc("video write...");
	}
	else
	{
		fp = (void*)fopen(fileName, modes);
		if(fp==NULL)
		{
			if(is_duvonn_device())
			{
				int i, len = strlen(fileName);	/**检查该文件是否存在灾备存储器中**/
				char realName[128];			
				
				for(i=(len-1); i>0; i--)
				{
					if(fileName[i]=='/')
					{
						i++;
						break;
					}
				}
				sprintf(realName, "/mnt/intsd/%s", fileName+i);
				printf("realName:%s\n", realName);
				fp = (void*)fopen(realName, modes);
			}
		}
		mVideoReadHandle = fp;
		mVideoFileCour = 0;
	}
	return fp;
}

size_t vimRead(void *param, size_t size, size_t n, void *stream)
{
	size_t readByte = 0;
	if(stream == mVideoReadHandle)
	{	
		readByte = fread(param, size, n, stream);
		mVideoFileCour += readByte;
		// printf("n:%d, r:%d\n", n, readByte);		
	}
	else
	{
#ifdef USE_STAND_FILEIO
		readByte = fread(param, size, n, stream);
#else
		readByte = 0;//lc_dvr_mem_write_cache_read(stream, param, n);
#endif	
	}
	return readByte;
}

size_t vimWrite(const void *ptr, size_t size, size_t n, void *stream)
{	
	size_t writeByte = 0;
	if(stream == mVideoReadHandle)
	{

	}
	else
	{
#ifdef USE_STAND_FILEIO
		writeByte = fwrite(ptr, size, n, stream);
#else	
		writeByte = lc_dvr_mem_write_cache_write(stream, ptr, n);
#endif
		//printf("w:%d, %d\n", size, n);
		mVideoByteCnt += n;
	}
	return writeByte;
}

int vimSeek(void *stream, __off64_t off, int whence)
{	
	int seekVal;

	if(stream == mVideoReadHandle)
	{
		seekVal = fseeko64(stream, off, whence);
		if(whence==0)
		{
			mVideoFileCour = off;
		}
		// printf("seek:%lld, %d\n", off, whence);
	}
	else
	{
#ifdef USE_STAND_FILEIO
		seekVal = fseek(stream, off, whence);	
#else
		uint32_t seekTo = (uint32_t)(off&0xffffffff);
		seekVal = lc_dvr_mem_write_cache_seek(stream, seekTo);
#endif
	}
	return seekVal;
}

__off64_t vimTell(void *stream)
{
	__off64_t byte = 0;
	
	if(stream == mVideoReadHandle)
	{		
		byte = ftello64(stream);
		mVideoFileCour = byte;
		// printf("tell: %lld\n", byte);
	}
	else
	{
#ifdef USE_STAND_FILEIO
		byte = ftello64(stream);
#else	
		byte = lc_dvr_mem_write_cache_tell(stream);
#endif
	}
	//printf("->T:%lld\n", byte);
	return byte;
}

int vimClose(void *stream)
{	
	int closeVal;
	
	if(stream == mVideoReadHandle)
	{
		closeVal = fclose(stream);
		mVideoReadHandle = NULL;
		logd_lc("vimClose-->%d", mVideoFileCour);		
	}
	else
	{
		int fileSize = 0;
#ifdef USE_STAND_FILEIO
		closeVal = fclose(stream);
#else	
		closeVal = lc_dvr_mem_write_cache_close(stream, &fileSize);
#endif
		logd_lc("vimClose-->%x", stream);
		vc_save_info_to_file((int)stream, fileSize);
	}
	return closeVal;
}

VIM_VOID *vc_vim_file_handle(void)
{
	gVimFileHandl.open = vimOpen;
	gVimFileHandl.read = vimRead;
	gVimFileHandl.write = vimWrite;
	gVimFileHandl.seek = vimSeek;
	gVimFileHandl.tell = vimTell;
	gVimFileHandl.close = vimClose;
	memset(&mFileHandlList, 0, sizeof(FileHandleList)*8);
	
//	gVimFileHandl.open = fopen;
//	gVimFileHandl.read = fread;
//	gVimFileHandl.write = fwrite;
//	gVimFileHandl.seek = fseek;
//	gVimFileHandl.tell = ftell;
//	gVimFileHandl.close = fclose;
	logd_lc("vc_vim_file_handle!!");
	return &gVimFileHandl;
}

int vc_vim_copy_video_file_to_dist(int chanel, int index, char* dstDir, char* name)
{
	int retval = 0;
	switch(chanel)
	{
		case LC_DUVONN_CHANEL_0:
		case LC_DUVONN_CHANEL_1: 
			if(is_duvonn_device())
			{
				retval = lc_dvr_mem_video_file_read(chanel, index, dstDir, name);
				break;
			}			
		default:	
			retval = Record_video_file_copy(chanel, index, dstDir, name);
			break;
	}	
	return retval;
}

#define LC_STREAM_LOCAL_SAVE

static int STREAM_BYTE_CNT=0;
void *vc_stream_handler(void *arg)
{
	VIM_S32 pipe_read; //lc_venc_get_pipe();
	VIM_U8 *streamBuf = (VIM_U8*)malloc(1024*1024);
	VIM_S32 retval;    
	int quit = 0;
	int *s32Ptr =(int*)arg;
	int pipeChnl = s32Ptr[0];
    struct timeval tv;
	fd_set rfds;
#ifdef LC_STREAM_LOCAL_SAVE
	FILE* StreamFile=NULL;
	VIM_U32 StreamSize = 0;
	
	StreamFile = fopen("/tmp/stream2.ts", "wb");
	if(StreamFile!=NULL)
	{
		loge_lc("StreamFile creat success!!\n");
	}
#endif
	g_stream_out_flag = VIM_TRUE;

    tv.tv_sec = 0;
    tv.tv_usec = 60*1000;
	
	if((mkfifo(mFifoName[pipeChnl],0777) == -1) && errno != EEXIST){
		logd_lc("mkfifo failure\n");
	}else{
		logd_lc("mkfifo succeed\n");
	}	
	
	pipe_read = open(mFifoName[pipeChnl], O_RDONLY); /**???????????????????*/
	logd_lc("vc_stream_handler begin!");
    while (VIM_TRUE == g_stream_out_flag)
    {
       FD_ZERO(&rfds);
       FD_SET(pipe_read,&rfds);
       retval = select(pipe_read+1,&rfds,NULL,NULL,&tv);
       if(retval ==-1)
       {
	        logd_lc("select() err!!\n");
       }
       else if(retval)
       {
			VIM_S32 byteRead = read(pipe_read, streamBuf, 1024*512);	
//			STREAM_BYTE_CNT+=byteRead;
//			if(STREAM_BYTE_CNT>128*1024)
//			{
//				STREAM_BYTE_CNT = 0;
//				logd_lc("stream out continue!"); 
//			}
#ifdef LC_STREAM_LOCAL_SAVE		   
			if(StreamSize<2*1024*1024)
			{
				fwrite(streamBuf, byteRead, 1, StreamFile);
				fflush(StreamFile);
				StreamSize += byteRead;
				logd_lc("file:%d ", StreamSize);
			}
			else if(quit==0)
			{
				vc_message_send(ENU_LC_DVR_STREAM_CLOSE, pipeChnl, NULL, NULL);
				quit = 1;
			}
#endif			
			//logd_lc(" [%d]pRead:%d\n", mFrameCnt, byteRead);
       }
    }
	
#ifdef LC_STREAM_LOCAL_SAVE	
	fflush(StreamFile);
	fclose(StreamFile);
	logd_lc("File total size:%d\n", StreamSize);
#endif
	free(streamBuf);
	close(pipe_read);
	logd_lc("stream out stop!");	
	
	return NULL;
}

VIM_S32 vc_jpeg_enc_initial(int width, int height, int chanel, int level)
{
	JPEG_PIC_ATTR jpegCfg;
	//logd_lc("jpeg_enc->%d , %d", width, height);
	jpegCfg.u32JpegChnl = chanel;	/** JPEG??????? **/
	jpegCfg.u32CameraChnl = 2;	/** ????????	**/
	jpegCfg.u32PicWidth = width;
	jpegCfg.u32PicHeight = height; /** ????16???????????*/
	jpegCfg.enPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420;	
	jpegCfg.JpegTakePic = VIM_TRUE;	/**VIM_TRUE ????????????**/
	lc_jpeg_chanel_start(&jpegCfg, chanel, level);	
	
	return 0;
}

VIM_S32 vc_jpeg_enc_uninitial(int chanel)
{
	lc_jpeg_chanel_stop(chanel);	
	return 0;
}

VIM_VOID vc_jpeg_take_picture(int chanel, char* fullpath)
{
	LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)sys_msg_get_memcache(sizeof(LC_TAKE_PIC_INFO));

//	picInfo->pic_width = LC_TAKE_STREAM_PIC_SIZE_W;
//	picInfo->pic_height = LC_TAKE_STREAM_PIC_SIZE_H;

	picInfo->pic_width = LC_TAKE_THUMB_PIC_SIZE_W;
	picInfo->pic_height = LC_TAKE_THUMB_PIC_SIZE_H;
	
	picInfo->camChnl = chanel;
	picInfo->frameDelay = 0;
	picInfo->ackFun = NULL;
	
	strcpy(picInfo->pic_save_path, fullpath);
	RecordGenPicNameFromVideo(picInfo->pic_save_path);
	vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC, 0, picInfo, NULL);
}

VIM_VOID vc_stream_out_start(int camChnl, int on_off, int index)
{
	mStreamCtr[index].camChnl = camChnl;
	mStreamCtr[index].on_off = on_off;
}

VIM_VOID vc_stream_out_stop(int index)
{
	mStreamCtr[index].on_off = 0; 
	usleep(50*1000);
	g_stream_out_flag = VIM_FALSE;
	usleep(50*1000);
}

VIM_VOID vc_stream_out_chnlSwitch(int camChnl, int index)
{
	mStreamCtr[index].camChnl = camChnl;
}

VIM_S32 vc_stream_venc_initial(void *venc_cfg, int index)
{
	VIM_S32 s32Ret;
	SAMPLE_ENC_CFG *venc_cfg_config = (SAMPLE_ENC_CFG*)venc_cfg;
	venc_cfg_config->VeChn += index;
	logd_lc("%s index:%d, VeChn:%d enter\n",__FUNCTION__, index, venc_cfg_config->VeChn);
	mStreamCtr[index].codeChnl = venc_cfg_config->VeChn;
	s32Ret = lc_venc_open(venc_cfg_config);
	if (VIM_SUCCESS != s32Ret)
	{
		logd_lc("SAMPLE_COMM_VENC_OPEN [%d] faild with %d! ===\n",venc_cfg_config->VeChn, s32Ret);
		return VIM_FAILURE;
	}

	return VIM_SUCCESS;
}

VIM_S32 vc_stream_venc_unInitial(int index)
{
	VIM_S32 s32Ret;
	VENC_CHN VencChn = mStreamCtr[index].codeChnl;
	
	// SAMPLE_COMM_VENC_StopGetStream(venc_cfg_config->VeChn,venc_cfg_config);	
	s32Ret = lc_venc_close(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		logd_lc("SAMPLE_COMM_VENC_CLOSE [%d] faild with %#x! ===\n",VencChn, s32Ret);
	}

	return VIM_SUCCESS;
}

VIM_S32 vc_stream_venc_config(int resolution, int index)
{	
	SAMPLE_ENC_CFG pEncCfg;	
	
	if(mStreamCtr[index].venc_used != 0) return -1;
	
	memset(&pEncCfg,0x00,sizeof(SAMPLE_ENC_CFG));
	mStreamCtr[index].resolution = resolution;
	logd_lc("resolution is: %d", resolution);
	lc_venc_para_from_file(&pEncCfg, resolution);

	if(VIM_SUCCESS == vc_stream_venc_initial(&pEncCfg, index))
	{
		logd_lc("VIM_VENC_OPEN Chanel %d VIM_SUCCESS!\n", pEncCfg.VeChn);
		mStreamCtr[index].venc_used = 1;
	}
	else
	{
		return -1;
	}

	return pEncCfg.VeChn;
}

VIM_S32 vc_stream_venc_destroy(int index)
{
	if(mStreamCtr[index].venc_used == 0) return -1;
	mStreamCtr[index].venc_used = 0;
	return vc_stream_venc_unInitial(index);
}

LC_VENC_STREAM_INFO* vc_stream_venc_resolution_get(int index)
{
	return &mStreamCtr[index];
}

static VIM_S32 vc_venc_set_framebufferVar(VIM_VOID)
{
    VIM_S32 ret = 0;

    VO_DEV voDev = VO_DEV_HD;
    VO_LAYER voLayer = Layer_B;
    VIM_U32 nChnlId = 0;
    VIM_U32 nFifoId = 0;
    VIM_S32 screen_fb = 0;
    ret = VIM_MPI_VO_OpenFramebufferGetFd(voDev, voLayer, nChnlId, nFifoId, &screen_fb);
    if (ret)
    {
        logd_lc("VIM_MPI_VO_OpenFramebufferGetFd failed. ret = %d.", ret);
        return ret;
    }

    struct fb_var_screeninfo fb_var;
    ret = ioctl(screen_fb, FBIOGET_VSCREENINFO, &fb_var);
    if (ret)
    {
        logd_lc("ioctl(FBIOGET_VSCREENINFO) failed. ret = %d.", ret);
        return VIM_ERR_VO_VIDEO_NOT_ENABLE; 
    }

    VIM_U32 width = 1920;
    VIM_U32 height = 1080;
    fb_var.bits_per_pixel = 32;
    fb_var.xres = width;
    fb_var.yres = height;
    fb_var.width = width;
    fb_var.height = height;
    fb_var.xres_virtual = width;
    fb_var.yres_virtual = height;
    /**以下为像素格式ARGB8888的红绿蓝位域信息**/
    fb_var.transp.length = 8;
    fb_var.transp.offset = 24;
    fb_var.transp.msb_right = 0;
    fb_var.red.length = 8;
    fb_var.red.offset = 16;
    fb_var.red.msb_right = 0;
    fb_var.green.length = 8;
    fb_var.green.offset = 8;
    fb_var.green.msb_right = 0;
    fb_var.blue.length = 8;
    fb_var.blue.offset = 0;
    fb_var.blue.msb_right = 0;

    ret = ioctl(screen_fb, FBIOPUT_VSCREENINFO, &fb_var);
    if (ret)
    {
        logd_lc("ioctl(FBIOPUT_VSCREENINFO) failed. ret = %d.", ret);
        return VIM_ERR_VO_VIDEO_NOT_ENABLE;     
    }

    ret = ioctl(screen_fb, FBIOGET_VSCREENINFO, &fb_var);
    if (ret)
    {
        logd_lc("ioctl(FBIOGET_VSCREENINFO) failed. ret = %d.", ret);
        return VIM_ERR_VO_VIDEO_NOT_ENABLE;     
    }

    VIM_MPI_VO_CloseFramebufferByFd(voDev, screen_fb);

    return ret;
}

char* vc_dynamic_mount_path(void)
{
	return mDynamicMount;
}

void vc_video_cpy_notify_progress(int cur, int total)
{
	LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
	int fd = lc_socket_app_fd_get(LC_DVR_RECO_SERID);
	int *u32Ptr = (int*)msgAck->msg_buf;
	
	lc_socket_app_fd_set(LC_DVR_GUI_SERID, fd);						
	memset(msgAck, 0, sizeof(LC_DVR_RECORD_MESSAGE));
	msgAck->event = ENU_LC_DVR_VIDEO_CPY_PROGRESS; /**进度通报**/
	u32Ptr[0] = cur;
	u32Ptr[1] = total;
	lc_socket_client_send(fd, msgAck, sizeof(LC_DVR_RECORD_MESSAGE));
}

int  vc_video_file_copy_status(void)
{
	return mVideoCopyRun;
}

void  vc_video_file_copy_break(void)
{
	mVideoCopyRun = 0;
}


void* vc_video_file_copy_thread(void* arg)
{
	char *s8Ptr = (char*)arg;
	char dir[128];
	LC_RECORD_LIST_FETCH fileAck;
	int maxNum = RecordFileMaxNumber();
	int IdxToCopy[4], dirPositon[4];
	int IdxCal[4];
	char* fileName;
	int status, totalFileNum=0, curFileNum=0;
	int i=0, skipCur = 0; /**如果真在录像中，则当前真在录的文件被忽略**/
	mVideoCopyRun = 1;
	strcpy(dir, s8Ptr);
	logd_lc("GB19056_VIDEO_COPY->(%d)%s", maxNum, dir);
	if(vc_get_mdvr_record_status()) /**如果处于录像状态，需要先停止当前录像**/
	{
		skipCur = 1;
		vc_record_stop();			
		vc_record_start();
		for(int wait=0; wait<10; wait++)
		{
			logd_lc("record status:%x", vc_get_record_status());
			if(vc_get_record_status()==0x0f) wait=100;
			sleep(1);
		}
	}
	
	for(int chnl=0; chnl<4; chnl++)
	{
		fileAck.camChnl = chnl;
		fileAck.offset = -1;
		fileAck.fileNumber = 1;		
		RecordFileListFetch(&fileAck);
		IdxToCopy[chnl] = fileAck.offset+1;
		IdxCal[chnl] = fileAck.offset+1;
		dirPositon[chnl] = strlen(fileAck.fileDir);
	}
	
	for(i=0; i<maxNum-skipCur; i++)
	{
		for(int chnl=0; chnl<4; chnl++)
		{
			IdxCal[chnl]++;
			if(IdxCal[chnl]>=maxNum) IdxCal[chnl]-=maxNum;			
			fileName = RecordFileFetchByIndex(chnl, IdxCal[chnl]);
			
			if(fileName[0]!=0)
			{
				fileName += dirPositon[chnl];//+4;
				if(strstr(fileName, RecordGetLicense())!=NULL)
				{/**车牌一致，开始拷贝视频文件**/
					logd_lc("(%d)precopy[%d]->%s", chnl, IdxCal[chnl], fileName);
					totalFileNum++;
				}			
			}
		}
	}
	
	for(i=0; i<maxNum-skipCur; i++)
	{
		for(int chnl=0; chnl<4; chnl++)
		{
			IdxToCopy[chnl]++;
			if(IdxToCopy[chnl]>=maxNum) IdxToCopy[chnl]-=maxNum;
			
			fileName = RecordFileFetchByIndex(chnl, IdxToCopy[chnl]);
			
			if(fileName[0]!=0)
			{
				fileName += dirPositon[chnl];//+4;
				if(strstr(fileName, RecordGetLicense())!=NULL)
				{/**车牌一致，开始拷贝视频文件**/
					logd_lc("(%d)copy[%d]->%s", chnl, IdxToCopy[chnl], fileName);
					vc_video_cpy_notify_progress(curFileNum, totalFileNum);
					status = vc_vim_copy_video_file_to_dist(chnl, IdxToCopy[chnl], dir, fileName);

					curFileNum++;
					if(status!=0)
					{						
						logd_lc("status:%d", status);
						if(status!=-3)
						{							
							i = maxNum-skipCur;
						}
						else
						{
							i = 2048;
						}
						break;
					}	
				}				
			}
		}
	}

	if(i==maxNum-skipCur)
	{
		vc_video_cpy_notify_progress(0, 0);
	}
	
	if(curFileNum>0)
	{
		logd_lc("IO sync begin!");
		sync();		
		logd_lc("video file copy finish!!");
	}
	
	mVideoCopyRun = 0;
	// vc_video_cpy_notify_progress(0, 0);
}

void vc_record_restart(void *para)
{
    for(int recCh = 0; recCh < 4; recCh++)
    {    	
		int s32Ret = VimRecordChStop(recCh);
        if(s32Ret != 0)
        {
            logd_lc("error:VimRecordChStop ch:%d ret:%d\n", recCh, s32Ret);
        }		
    }

	vc_record_video_para_config_update();
	vc_record_start();

	logd_lc("vc_record_restart finish!");
}

int vc_record_video_para_get(int index)
{
	return mVideoConfig[index];
}

VIM_VOID vc_record_para_initial(void)
{
	LC_DVR_REC_DB_SAVE *dbCur = (LC_DVR_REC_DB_SAVE*)malloc(VIDEO_CONFIG_BLOCK_LEN);
	int length = lc_dvr_dbget_content(VIDEO_CONFIG_DBSAVE_PATH, dbCur, VIDEO_CONFIG_BLOCK_LEN);
	if(length == 0)
	{
		memset(dbCur, 0, VIDEO_CONFIG_BLOCK_LEN);
		mVideoConfig[VC_IDX_DURATION] = LC_DVR_DURATION;
		mVideoConfig[VC_IDX_BITRATE] = LC_DVR_BITRATE;
		mVideoConfig[VC_IDX_MUTE] = AV_FMT_AAC;
		mVideoConfig[VC_IDX_PHOTOSZ] = 0; //默认1920x1080
		
		dbCur->duration = mVideoConfig[VC_IDX_DURATION];
		dbCur->birate = mVideoConfig[VC_IDX_BITRATE];
		dbCur->mute = mVideoConfig[VC_IDX_MUTE];
		dbCur->photosize = mVideoConfig[VC_IDX_PHOTOSZ];
		logd_lc("initial record parameter!");
		lc_dvr_dbsave_content(VIDEO_CONFIG_DBSAVE_PATH, dbCur, sizeof(LC_DVR_REC_DB_SAVE), VIDEO_CONFIG_BLOCK_LEN);
	}
	else
	{
		mVideoConfig[VC_IDX_DURATION] = dbCur->duration;
		mVideoConfig[VC_IDX_BITRATE] = dbCur->birate;
		mVideoConfig[VC_IDX_MUTE] = dbCur->mute;
		mVideoConfig[VC_IDX_PHOTOSZ] = dbCur->photosize;
		logd_lc("dbRecord->t:%d, b:%d, m:%d, p:%d",mVideoConfig[VC_IDX_DURATION], mVideoConfig[VC_IDX_BITRATE], 
						mVideoConfig[VC_IDX_MUTE], mVideoConfig[VC_IDX_PHOTOSZ]);
	}
	free(dbCur);
}

VIM_VOID vc_record_para_set(void *config)
{
	LC_DVR_REC_DB_SAVE *dbCur = (LC_DVR_REC_DB_SAVE*)sys_msg_get_memcache(512);
	LC_DVR_RECORD_PARAMETER *cfgPara = (LC_DVR_RECORD_PARAMETER*)config;
	int recordReset = 0;
	int preMuteStatus;
	int length = lc_dvr_dbget_content(VIDEO_CONFIG_DBSAVE_PATH, dbCur, VIDEO_CONFIG_BLOCK_LEN);
	logd_lc("vc_record_para_set");
	logd_lc("pre->t:%d, b:%d, m:%d, p:%d",mVideoConfig[VC_IDX_DURATION], mVideoConfig[VC_IDX_BITRATE], 
					mVideoConfig[VC_IDX_MUTE], mVideoConfig[VC_IDX_PHOTOSZ]);
	logd_lc("cur->t:%d, b:%d, m:%d, p:%d",cfgPara->duration, cfgPara->bitrate, 
					cfgPara->mute, cfgPara->resolution);	
	
	if(length > 0)
	{	
		if((cfgPara->duration>0)&&(cfgPara->duration<=600))
		{
			if(mVideoConfig[VC_IDX_DURATION] != cfgPara->duration)
			{
				mVideoConfig[VC_IDX_DURATION] 	= cfgPara->duration;
				recordReset = 1;
			}
		}
		
		if((cfgPara->bitrate>=1024)&&(cfgPara->bitrate<=1536))
		{
			if(mVideoConfig[VC_IDX_BITRATE] != cfgPara->bitrate)
			{
				mVideoConfig[VC_IDX_BITRATE] = cfgPara->bitrate;
				recordReset = 1;
			}		
		}	

		preMuteStatus = mVideoConfig[VC_IDX_MUTE];
		if(cfgPara->mute==0)
		{			
			mVideoConfig[VC_IDX_MUTE] = AV_FMT_NONE;
		}	
		else
		{
			mVideoConfig[VC_IDX_MUTE] = AV_FMT_AAC;
		}

		if(mVideoConfig[VC_IDX_MUTE]!=preMuteStatus)
		{
			recordReset = 1;
		}

		if((cfgPara->resolution>=0)&&(cfgPara->resolution<2))
		{
			if(mVideoConfig[VC_IDX_PHOTOSZ] != cfgPara->resolution)
			{
				mVideoConfig[VC_IDX_PHOTOSZ] = cfgPara->resolution;
				recordReset = 1;
			}
		}

		dbCur->duration = mVideoConfig[VC_IDX_DURATION];
		dbCur->birate = mVideoConfig[VC_IDX_BITRATE];
		dbCur->mute = mVideoConfig[VC_IDX_MUTE];
		dbCur->photosize = mVideoConfig[VC_IDX_PHOTOSZ];
		lc_dvr_dbsave_content(VIDEO_CONFIG_DBSAVE_PATH, dbCur, sizeof(LC_DVR_REC_DB_SAVE), VIDEO_CONFIG_BLOCK_LEN);
		logd_lc("[%d]vc_get_record_status()->%d", recordReset, vc_get_record_status());
		if(vc_get_record_status()&&recordReset)
		{
			pthread_t rset_thread;
			
			int s32Ret = pthread_create(&rset_thread, NULL, (void*)vc_record_restart, (void*)NULL);
			if(0 == s32Ret )
			{
				printf("vc_record_restart pthread SUCCESS\n");
				pthread_detach(rset_thread);
			}			
		}
	}
}

void test_video_file(char* fileName)
{
	char *memBuf = (char*)sys_msg_get_memcache(2048);
	void *stream = vimOpen(fileName, "rb");
	memset(memBuf, 0, 256);
	vimRead(memBuf, 1, 256, stream);
	printf("read:\n%s\n", memBuf);
	//lc_dvr_mem_debug(memBuf, 2048);
	vimClose(stream);
}


