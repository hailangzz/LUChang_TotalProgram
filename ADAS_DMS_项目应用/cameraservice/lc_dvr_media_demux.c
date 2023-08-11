#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <linux/falloc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "lc_dvr_comm_def.h"
#include "lc_glob_type.h"
#include "lc_dvr_glob_def.h"
#include "VimRecordSdk.h"
#include "VimMediaType.h"
#include "lc_dvr_record_log.h"

// #define LC_DEMUX_FILE_SAVE
#define LC_DEMUX_DEBUG_SHOW

static MPSHANDLE mDemuxHandle;
static int mDemuxFrameCnt;
static int mDemuxSpeed;
static int mDemuxTotalFrame;
static int mPlayExit;
static uint32_t mDemuxKeyTime;
static char mDemuxFilePath[256] = {0};
static pthread_mutex_t mDemuxMutex;
static LC_ACK_FUNC mDataProcessFunc;
static long mTimeStampPre;
static int mptsPre;
static int loopTime = 0;
extern int mFrameCnt;
extern ExtRwHanlder gVimFileHandl;
#ifdef LC_DEMUX_FILE_SAVE	
FILE* mDemuxFp;
#endif

void mediaPlay_demux_timer(int sign)
{
	int retval, endSign;	
	int skipFrame, CurSpeed;
	MpsPacket packInfo;	

	pthread_mutex_lock(&mDemuxMutex);		
	CurSpeed = mDemuxSpeed;
	endSign = 0;
	skipFrame = 0;

	{
		retval = VimMpsDemuxerReadPacket(mDemuxHandle, &packInfo);			
		if(retval==0) 
		{			
			endSign = 1;
			goto DEMUX_QUIT;
		}				
		
		if(packInfo.type == AV_TYPE_VIDEO)
		{			
			mDemuxFrameCnt++;			
			if(packInfo.flags)
			{
				uint32_t prePts = mDemuxKeyTime;
				mDemuxFrameCnt = 0;
				mDemuxTotalFrame++;
				mDemuxKeyTime = (uint32_t)(packInfo.pts);
#ifdef LC_DEMUX_DEBUG_SHOW
				logd_lc("mDemuxKeyTime = %d", mDemuxKeyTime);
#endif
				if(mDemuxKeyTime == prePts) 
				{
					endSign = 1;
					goto DEMUX_QUIT;
				}
			}
			
			switch(CurSpeed)
			{
				case 2:
					if(mDemuxFrameCnt==12)
					{
						skipFrame = 1;
					}						
					break;
				case 4:
					if(mDemuxFrameCnt==6)
					{
						skipFrame = 1;
					}
					break;
				case 8:
					if(mDemuxFrameCnt==3)
					{
						skipFrame = 1;
					}
					break;
				case 16:
					if(mDemuxTotalFrame&0x01)
					{
						if(mDemuxFrameCnt==1)
						{
							skipFrame = 1;
						}					
					}
					else
					{
						skipFrame = 1;
					}
					break;
				case 25:	/**关键帧播放**/
					skipFrame = 1;
					break;
				case -1:	/**关键帧回退**/
					skipFrame = -1;
					break;
			}			
		}		

		if(packInfo.type == AV_TYPE_VIDEO)
		{
			uint8_t *u8Ptr = packInfo.data;
#ifdef LC_DEMUX_DEBUG_SHOW			
			// printf("F%04d.%d->", mDemuxFrameCnt, packInfo.flags);			
			// printf("l:%02d t:%lld\n", packInfo.len/1024, packInfo.pts);
			
			// printf("naul->%02x %02x %02x %02x\n", u8Ptr[2], u8Ptr[3], u8Ptr[4], u8Ptr[5]);
#endif
			//处理视频数据
			//=======================================
#ifdef LC_DEMUX_FILE_SAVE
			if(mDemuxFp!=NULL)
			{
				fwrite(packInfo.data, 1, packInfo.len, mDemuxFp);
			}
#endif
			//=======================================	
			if(mDataProcessFunc!=NULL) mDataProcessFunc(AV_TYPE_VIDEO,(int)(&packInfo));
			mptsPre = packInfo.pts;
		}
		else if(packInfo.type == AV_TYPE_AUDIO)
		{
			if(CurSpeed == 1)
			{
				//printf("TYPE_AUDIO(%d) \n", packInfo.flags);
				//---------------------------------------------------------
				//处理音频数据流，快速播放时不推送音频，正常播放才推送音频
				//---------------------------------------------------------
				if(mDataProcessFunc!=NULL) mDataProcessFunc(AV_TYPE_AUDIO,(int)(&packInfo));
			}
		}
		VimMpsDemuxerReleasePacket(mDemuxHandle, &packInfo);		

		if(skipFrame!=0)
		{
			double foundKeyFrame = (mDemuxKeyTime)/1000.0;

			if(skipFrame>0)
				foundKeyFrame += 1.0-0.04;
			else
				foundKeyFrame -= 1.0+0.04;
			logd_lc("seek to %f", foundKeyFrame);
			retval = VimMpsDemuxerSeek(mDemuxHandle, foundKeyFrame);
			if(retval !=0)
			{
				logd_lc("seek error -> %x", retval);
				endSign = 1;
			}			
		}
	}
DEMUX_QUIT:
	if((endSign)||(mPlayExit))
	{
		VimMpsDemuxerClose(mDemuxHandle);
		mFrameCnt = 250;	/**循环测试计数器**/
#ifdef LC_DEMUX_FILE_SAVE	
		if(mDemuxFp!=NULL)
		{
			fclose(mDemuxFp);
			mDemuxFp = NULL;
		}
#endif		
	}
	else
	{
		long curStamp;
		struct timeval start;
		
		gettimeofday(&start, NULL);
		curStamp = start.tv_usec - mTimeStampPre;	/**计划使用时间补偿，似乎有问题**/
		mTimeStampPre = start.tv_usec;
		if(curStamp<0) curStamp += 1000000;
		curStamp = 80*1000-curStamp;

//		if(curStamp>0)
//			media_demux_time_set(curStamp);
//		else
//			media_demux_time_set(1*1000);

//		logd_lc("tv_usec:%d, dif:%d", start.tv_usec, curStamp);		
		media_demux_time_set(40*1000);
	}	

	pthread_mutex_unlock(&mDemuxMutex);
}

void media_demux_time_set(int interval)
{
	struct itimerval tick;	
	signal(SIGALRM, mediaPlay_demux_timer);
	memset(&tick, 0, sizeof(tick));
	
	//第一次调用时间
	tick.it_value.tv_sec = 0;
	tick.it_value.tv_usec = interval;
	
	//第一次调用后，每次调用间隔时间
	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
	{
		printf("Set timer failed!\n");
	}
}

void media_demux_set_speed(int speed)
{
	pthread_mutex_lock(&mDemuxMutex);
	mDemuxSpeed = speed;
	pthread_mutex_unlock(&mDemuxMutex);
}

int media_demux_start(char *fileName)
{
	mDemuxHandle = -1;
	logd_lc("palyFile Name:%s", fileName);
	if(access(fileName, F_OK) == 0)
	{
		MediaInfo mediaInfo;
		int err;
		pthread_mutex_init(&mDemuxMutex, NULL);
		strcpy(mDemuxFilePath, fileName);
		mDataProcessFunc = NULL;
		mPlayExit = 0;
		err = VimMpsDemuxerOpenFile((const char*)fileName, &mDemuxHandle, &mediaInfo);
		if(err == 0)
//		{
//			MpsPacket packInfo;	
//			struct timeval start, end;
//			int retval, interval;
//			int framCnt = 0, totalSize = 0;
//			char dummy[256];
//			int first = 0;
//			gettimeofday(&start, NULL);
//			mDemuxFp = fopen("/mnt/sdcard/demuxStream.ts", "wb+");
//			memset(dummy, 0xff, 256);
//			
//			for(;;)
//			{
//				retval = VimMpsDemuxerReadPacket(mDemuxHandle, &packInfo);
//				if(packInfo.type == AV_TYPE_VIDEO)
//				{					
//					if(packInfo.flags) 
//					{
//						if(first)
//						{
//							fwrite(dummy, 1, 256, mDemuxFp);
//							printf("fill!!\n");
//						}
//						printf("key:%d\n", totalSize);
//						totalSize = 0;
//						first++;
//					}
//					framCnt ++;
//					totalSize += packInfo.len;
//				}
//				
//				fwrite(packInfo.data, 1, packInfo.len, mDemuxFp);
//				
//				VimMpsDemuxerReleasePacket(mDemuxHandle, &packInfo);
//				if(retval==0)
//				{
//					break;
//				}
//			}
//			gettimeofday(&end, NULL);
//			interval =((end.tv_sec - start.tv_sec)*1000) + ((end.tv_usec - start.tv_usec)/1000);
//			printf("demux fram:%d len:%d time:%dms\n", framCnt, totalSize, interval);
//			
//			VimMpsDemuxerClose(mDemuxHandle);
//			fclose(mDemuxFp);
//		}

		{
			struct timeval start;
			logd_lc("lc_dvr_demux_start->%d!!", loopTime++);
			mDemuxKeyTime = 0xffffffff;
			mDemuxFrameCnt = 0;
			mDemuxSpeed = 8; /**播放速率**/
			mDemuxTotalFrame = 0;
			mptsPre = 0;
			gettimeofday(&start, NULL);
			mTimeStampPre = start.tv_usec;
			media_demux_time_set(40*1000);
		}
	}
	else
	{
		logd_lc("video file no exist!");
	}
#ifdef LC_DEMUX_FILE_SAVE	
	mDemuxFp = fopen("/tmp/demuxStream.ts", "wb+");
#endif
	return (int)mDemuxHandle;
}

void media_demux_stop(int fp)
{
	if(fp == (int)mDemuxHandle)
	{
		mPlayExit = 1;
	}
}

void mddia_data_pro_register(void *funcion)
{
	mDataProcessFunc = (LC_ACK_FUNC)funcion;
}

