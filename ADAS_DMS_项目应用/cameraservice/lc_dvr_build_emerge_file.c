#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include "VimRecordSdk.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_mmc.h"
#include "lc_glob_type.h"
#include "lc_dvr_record_log.h"

#define LC_EMERG_CACHE_NUMBER (LC_DVR_SUPPORT_CAMERA_NUMBER)
#define LC_EMERG_CACHE_BLOCKB_SIZE	(512*1024)
#define LC_EMERG_CACHE_BLOCK_NUM	4
#define LC_EMERG_CACHE_BLOCKB_TOTAL	(LC_EMERG_CACHE_BLOCK_NUM*LC_EMERG_CACHE_BLOCKB_SIZE)
#define LC_EMERGWRITE_LIST_CNT	16
#define LC_EMERGE_DIRECT_WRITE
#define LC_EMERGE_LOOP_BUF_CNT   64
#define LC_EMERGE_MEM_CACHE_SIZE	(LC_EMERGE_LOOP_BUF_CNT*256*1024)

//#define LC_EMERGENCY_CACHE_DEBUG
#ifdef LC_EMERGENCY_CACHE_DEBUG
uint32_t frameSum = 0;
uint32_t frameLen = 0;
uint32_t frameOffset = 0;
//FILE* frameHandle = NULL;
#endif

typedef struct{
	char fileName[128];
	int  camId;
	int  duration;
	int  creatSize;
	int  fileTotalLen;	
	int  BlockBCacheHead;
	int  curIdx;
	void *fp;
	char cacheBlockB[32];
}EMERG_WRITE_CACHE;

typedef struct{
	int camId;
	void *cacheWritePtr;
	int offset;
	int writeLen;
	int cIndex;
	FILE *fp;
}EMERG_WRITEBACK_DATA;

typedef struct{
	void *cacheWritePtr;
	int totalLen;
	int first;
	int recIndex;
	int frameCnt;
	int keyFram[LC_EMERGE_LOOP_BUF_CNT];
}EMERG_CTRL_BLOCK;

static EMERG_WRITE_CACHE *mEmergFileCacheList[LC_EMERG_CACHE_NUMBER];
static char* mEmergWriteBuf = NULL;
static EMERG_WRITEBACK_DATA mEmergBackList[LC_EMERGWRITE_LIST_CNT];
static int mEmergBackHead;
static int mEmergBackTail;
static sem_t semEmergBack;//信号量
static int mEmergeWriteRun;
static pthread_mutex_t mEmergMutex;

static char *mEmergencyMemBuf;
static EMERG_CTRL_BLOCK *mCacheCtrl;
static int mEmergencyRecordStart;
static LC_ACK_FUNC mCloseAckFunc;
static int mEmergencyRecordRun;

extern int RecordGenEmergencyFile(char *name, int duration, int camId);
extern int RecordGetEmergencySize(int sec);
extern void RecordSetFileToDstCam(char* name, int camId);
extern int xxCreateMyFile(char *szFileName, int nFileLength);
extern int RecordFileListFetch(void* fetchInfo);
extern int RecordEmergencyListFetch(void* fetchInfo);
extern void RecordEmergencyFinsh(char *name, int size);

extern void lc_dvr_show_bcd_time(char *log, char* time);
extern void vc_message_send(int eMsg, int val, void* para, void* func);
extern int mFrameCnt;

void *lc_dvr_emerge_write_cache_thread(void* arg)
{
	sem_init(&semEmergBack, 0, 0);
    pthread_mutex_init(&mEmergMutex, NULL);

	while(mEmergeWriteRun==1)
	{	
		sem_wait(&semEmergBack);	
		while(mEmergBackHead<mEmergBackTail)
		{			
			int idx = mEmergBackHead&(LC_EMERGWRITE_LIST_CNT-1);						
			int offset = mEmergBackList[idx].offset;
			int len = mEmergBackList[idx].writeLen;
			char *backPtr = (char*)mEmergBackList[idx].cacheWritePtr;
			int cacheIndex = mEmergBackList[idx].cIndex;	
			int camId = mEmergFileCacheList[cacheIndex]->camId;
			FILE *fp = mEmergFileCacheList[cacheIndex]->fp;
//			struct timeval start, end;	
//			int interval;
//			gettimeofday(&start, NULL);

			if(backPtr!=NULL)
			{				 
				if(fp == NULL)
				{
					char *filePath = mEmergFileCacheList[cacheIndex]->fileName;					
					xxCreateMyFile(filePath, mEmergFileCacheList[cacheIndex]->creatSize);
#ifdef LC_EMERGE_DIRECT_WRITE
					fp = (FILE*)direct_file_open(filePath);
#else
					fp = fopen(filePath, "rb+");
#endif
					mEmergFileCacheList[cacheIndex]->fp = fp;
					// printf("Emerge open[%d](%d) open[%x]->%s\n", cacheIndex, camId, fp, filePath);
				}
				
				if(fp!=NULL)
				{
					int retval; 
					// printf("CamXE[%d]_%x:(%x) %d->%d\n", camId, fp, backPtr, len, offset);
#ifdef LC_EMERGE_DIRECT_WRITE
					retval = direct_file_write(fp, backPtr, len, offset);
#else
					fseeko64(fp, (__off64_t)offset, SEEK_SET); /**不能用fseek，否则3分钟长文件有问题**/
					retval = fwrite(backPtr, 1, len, fp);
#endif	
				}					
			}
			else
			{	
				char *filePath = mEmergFileCacheList[cacheIndex]->fileName;	
				int nullCnt;
				printf("Emerge Close[%d](%d) stop[%x]->%s\n", cacheIndex, camId, fp, filePath);
				if(fp!=NULL)
				{					
#ifdef LC_EMERGE_DIRECT_WRITE
					direct_file_close((int)fp);
#else
					fclose(fp);
#endif						
					RecordEmergencyFinsh(filePath, mEmergFileCacheList[cacheIndex]->fileTotalLen);
					mEmergFileCacheList[cacheIndex]->fp = NULL;	
				}

				for(nullCnt=0; nullCnt<LC_EMERG_CACHE_NUMBER; nullCnt++)
				{
					if(mEmergFileCacheList[nullCnt]->fp!=NULL) break;
				}

				if(nullCnt == LC_EMERG_CACHE_NUMBER)
				{				
mFrameCnt = 1200;
					vc_message_send(ENU_LC_DVR_FINISH_EMERGE_FILE, 0, NULL, NULL);
					mEmergencyRecordRun = 0;
					if(mCloseAckFunc) 
					{
						mCloseAckFunc(0, 0);
						break;
					}					
				}
			}						
			mEmergBackHead++;
//			gettimeofday(&end, NULL);
//			interval = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) + 500;
//			interval /= 1000;
//			printf("xWrite_cache[%d] = %dms\n", camId, interval);			
		}		
	}

	sem_destroy(&semEmergBack);
	logd_lc("All emergency file finish!");
	return NULL;
}

void lc_dvr_emerge_writeBack_opration(int fIndex, void* wbPtr, int len, int offset)
{
	pthread_mutex_lock(&mEmergMutex);		
	int idx = mEmergBackTail&(LC_EMERGWRITE_LIST_CNT-1);
	mEmergBackTail++;		
	mEmergBackList[idx].cacheWritePtr = wbPtr;
	mEmergBackList[idx].writeLen = len;
	mEmergBackList[idx].offset = offset;
	mEmergBackList[idx].cIndex = fIndex;
	sem_post(&semEmergBack);
	pthread_mutex_unlock(&mEmergMutex);
}


int lc_emergency_write(int camId, void* dataIn, int len)
{	
	int index = camId;
	int curIdx = mEmergFileCacheList[index]->curIdx;	/**当前偏移位置**/
	int total = len;
	char *writePtr = (char*)dataIn;
	char *buf;

	mEmergFileCacheList[index]->curIdx += total;	/**下一次的偏移位置，同时也作为文件的长度**/
	mEmergFileCacheList[index]->fileTotalLen += total;
	buf = mEmergFileCacheList[index]->cacheBlockB;

	int stepOneByte = LC_EMERG_CACHE_BLOCKB_SIZE - (curIdx&(LC_EMERG_CACHE_BLOCKB_SIZE-1));					

	if(total>=stepOneByte)	/**填写数据后长度超过一个block的长度**/
	{			
		do{
			curIdx &= (LC_EMERG_CACHE_BLOCKB_TOTAL-1);
			memcpy(buf+curIdx, writePtr, stepOneByte); /**分两部分写，先写满一个block，并回写到SD卡**/
			//===================================================================
			/**通过进程顺序回写**/
			int writeBackIndex = mEmergFileCacheList[index]->BlockBCacheHead&(LC_EMERG_CACHE_BLOCKB_TOTAL-1);
			char *backPtr = mEmergFileCacheList[index]->cacheBlockB + writeBackIndex;
			printf("cam[%d]->cLen:%d, step:%d, ofst:%d\n", camId, len, stepOneByte, mEmergFileCacheList[index]->BlockBCacheHead);
			lc_dvr_emerge_writeBack_opration(index, backPtr, LC_EMERG_CACHE_BLOCKB_SIZE, mEmergFileCacheList[index]->BlockBCacheHead);
			//===================================================================
			mEmergFileCacheList[index]->BlockBCacheHead += LC_EMERG_CACHE_BLOCKB_SIZE;
			curIdx += stepOneByte; /**剩下不足的数据写到另外的block**/
			len -= stepOneByte;
			writePtr += stepOneByte;
			stepOneByte = LC_EMERG_CACHE_BLOCKB_SIZE;
		}while(len >= LC_EMERG_CACHE_BLOCKB_SIZE);
	}

	curIdx &= (LC_EMERG_CACHE_BLOCKB_TOTAL-1);
	memcpy(buf+curIdx, writePtr, len);
	return total;
}

int lc_dvr_emerge_close(int camId)
{
	int index = camId;	
	int writeBackIndex, offset, len;
	char *wrtPtr;
	logd_lc("emerge_close begin!");
	mEmergFileCacheList[index]->duration = 0;
	len = mEmergFileCacheList[index]->fileTotalLen - mEmergFileCacheList[index]->BlockBCacheHead;

	if(len>0)
	{
		int reaLen = LC_ALIGN_4K(len);
		offset = mEmergFileCacheList[index]->BlockBCacheHead;
		writeBackIndex = offset&(LC_EMERG_CACHE_BLOCKB_TOTAL-1);
		wrtPtr = mEmergFileCacheList[index]->cacheBlockB+writeBackIndex;
		if(reaLen>len) memset(wrtPtr+len, 0, reaLen-len);
		lc_dvr_emerge_writeBack_opration(index, wrtPtr, reaLen, offset);			
	}

	logd_lc("Emerge File[%d]:%s->TotalLen:%d", index, mEmergFileCacheList[index]->fileName, mEmergFileCacheList[index]->fileTotalLen);/**如果写文件比这一条先执行，打印出来都是NLL**/
	lc_dvr_emerge_writeBack_opration(index, NULL, 0, -1); /**写空表示文件结束**/
			
	return 0;
}

int VencPacketCb_Chl0(int chId, AV_CODEC_FMT fmt, unsigned char* data, int len, long long pts, long long dts, int flag, void* userData)
{		
	EMERG_CTRL_BLOCK *camCtrl =  &mCacheCtrl[chId];
	int offset, remain;
	char *dstPtr;

	if(flag)
	{
		int index;	
		if(mEmergencyRecordStart)
		{
			if(mEmergFileCacheList[chId]->duration)
			{
				int wLen;				
				index = camCtrl->recIndex&(LC_EMERGE_LOOP_BUF_CNT-1);
				offset = camCtrl->keyFram[index]&(LC_EMERGE_MEM_CACHE_SIZE-1);
				
				index++;
				index &= (LC_EMERGE_LOOP_BUF_CNT-1);				
				wLen = camCtrl->keyFram[index]&(LC_EMERGE_MEM_CACHE_SIZE-1);			
				wLen -= offset;
				
				if(wLen<0) wLen += LC_EMERGE_MEM_CACHE_SIZE;

				remain = LC_EMERGE_MEM_CACHE_SIZE - offset;
				dstPtr = camCtrl->cacheWritePtr + offset;

#ifdef LC_EMERGENCY_CACHE_DEBUG				
				{
					uint32_t checkSum = 0;
					if(wLen>remain)
					{
						char *s8Ptr = camCtrl->cacheWritePtr;
						for(int k=0; k<remain; k++) 
						{
							checkSum += dstPtr[k];		
						}
						for(int k=0; k<(wLen-remain); k++) checkSum += s8Ptr[k];		
					}
					else
					{
						for(int k=0; k<wLen; k++) checkSum += dstPtr[k];		
					}	
					printf("sum:%x wxfram(%d)[%d] len:%d->%d\n", checkSum, chId, camCtrl->recIndex+1, wLen, offset);
				}
#endif
				if(wLen>remain)
				{
					// logd_lc("sdw1:%d, sdw2:%d", remain, wLen-remain);
					//if(frameHandle!=NULL) fwrite(dstPtr, 1, remain, frameHandle);
					lc_emergency_write(chId, dstPtr, remain);
					dstPtr = camCtrl->cacheWritePtr;
					//if(frameHandle!=NULL) fwrite(dstPtr, 1, wLen-remain, frameHandle);
					lc_emergency_write(chId, dstPtr, wLen-remain);
				}
				else
				{
					//if(frameHandle!=NULL) fwrite(dstPtr, 1, wLen, frameHandle);
					lc_emergency_write(chId, dstPtr, wLen);
				}	
				
				camCtrl->recIndex++;
				mEmergFileCacheList[chId]->duration--;
				if(mEmergFileCacheList[chId]->duration == 0)
				{
					lc_dvr_emerge_close(chId);
				}
			}
		}
		
		if(camCtrl->first == 0)
		{
			camCtrl->totalLen = 0;
			camCtrl->frameCnt = 0;				
		}
		else
		{
			camCtrl->frameCnt++;
		}
		
		index = camCtrl->frameCnt&(LC_EMERGE_LOOP_BUF_CNT-1);
		camCtrl->keyFram[index] = camCtrl->totalLen;			
		camCtrl->first++;
		
#ifdef LC_EMERGENCY_CACHE_DEBUG	
{
		frameSum = 0;
		frameOffset = camCtrl->totalLen;
		frameLen = 0;
}
#endif
	}		
	
	offset = camCtrl->totalLen&(LC_EMERGE_MEM_CACHE_SIZE-1);
	dstPtr = camCtrl->cacheWritePtr+offset;		
	camCtrl->totalLen += len;

	remain = LC_EMERGE_MEM_CACHE_SIZE - offset;
	
	
#ifdef LC_EMERGENCY_CACHE_DEBUG	
{
	for(int loop=0; loop<len; loop++) frameSum += data[loop];
	frameLen += len;
}
#endif

	if(len>remain)
	{
		memcpy(dstPtr, data, remain);
		dstPtr = camCtrl->cacheWritePtr;
		memcpy(dstPtr, data+remain, len-remain);
	}
	else
	{
		memcpy(dstPtr, data, len);
	}	
	return 0;
}

void lc_dvr_emergency_initial(void)
{
	pthread_t emergeBackThread;
	int err;
	logd_lc("lc_dvr_emergency_initial");
	mEmergencyMemBuf = (char*)malloc(LC_EMERG_CACHE_NUMBER*LC_EMERGE_MEM_CACHE_SIZE);
	mEmergWriteBuf = (char*)malloc((LC_EMERG_CACHE_NUMBER*(4*1024+LC_EMERG_CACHE_BLOCKB_TOTAL)));	
	mCacheCtrl = (EMERG_CTRL_BLOCK*)malloc(LC_EMERG_CACHE_NUMBER*sizeof(EMERG_CTRL_BLOCK));
	logd_lc("mEmergencyMemBuf1->%x", mEmergencyMemBuf);
	
	mEmergBackHead = 0;
	mEmergBackTail = 0;
	mEmergencyRecordStart = 0;
	mEmergencyRecordRun = 0;
	for(int i=0; i<LC_EMERG_CACHE_NUMBER; i++)
	{
		mEmergFileCacheList[i] = (EMERG_WRITE_CACHE*)(mEmergWriteBuf+i*(4*1024+LC_EMERG_CACHE_BLOCKB_TOTAL));		
		memset(mEmergFileCacheList[i], 0, sizeof(EMERG_WRITE_CACHE));
		memset(&mCacheCtrl[i], 0, sizeof(EMERG_CTRL_BLOCK));
		mCacheCtrl[i].cacheWritePtr = mEmergencyMemBuf + i*LC_EMERGE_MEM_CACHE_SIZE;
	}
		
	VimVencChAddConsumer(0, VencPacketCb_Chl0, NULL);
	VimVencChAddConsumer(1, VencPacketCb_Chl0, NULL);	
	VimVencChAddConsumer(2, VencPacketCb_Chl0, NULL);
	VimVencChAddConsumer(3, VencPacketCb_Chl0, NULL);	

	mEmergeWriteRun = 1;	
	mCloseAckFunc = NULL;
    err = pthread_create(&emergeBackThread, NULL, lc_dvr_emerge_write_cache_thread, NULL);
    if (0 != err)
    {
        printf("can't create emergeBackThread: %s\n", strerror(err));
    }
    pthread_detach(emergeBackThread);	
}

int lc_dvr_emergency_close_all(void)
{
	int quitPath = 0;
	for(int i=0; i<LC_EMERG_CACHE_NUMBER; i++)
	{		
		if(mEmergFileCacheList[i]->fp!=NULL)
		{
			lc_dvr_emerge_close(i);
			quitPath = 1;
		}
	}
	return quitPath;
}

void lc_dvr_emergency_destroy(int key, int para)
{
	logd_lc("lc_dvr_emergency_destroy");	
	free(mEmergencyMemBuf);
	free(mCacheCtrl);
	free(mEmergWriteBuf);
	mEmergeWriteRun = 0;
	mCloseAckFunc = NULL;
}

void lc_dvr_emergency_unInitial(void)
{	
	VimVencChDelConsumer(0, VencPacketCb_Chl0);
	VimVencChDelConsumer(1, VencPacketCb_Chl0);	
	VimVencChDelConsumer(2, VencPacketCb_Chl0);
	VimVencChDelConsumer(3, VencPacketCb_Chl0);	

	if(lc_dvr_emergency_close_all())
	{		
		mCloseAckFunc = lc_dvr_emergency_destroy;
	}
	else
	{
		lc_dvr_emergency_destroy(0, 0);
	}
}

void lc_dvr_emergency_file_build(int fileDuration)
{
	int duration;
	int warnType = LC_NORMAL_EMERGNECY_EVENT;
	fileDuration += 59; /**强制以分钟为单位**/
	fileDuration /= 60;
	fileDuration *= 60;

	logd_lc("lc_dvr_emergency_file_build!");
	if(mEmergencyRecordRun == 0)
	{		
		for(int i=0; i<LC_EMERG_CACHE_NUMBER; i++)
		{
			EMERG_CTRL_BLOCK *camCtrl =  &mCacheCtrl[i];
			mEmergFileCacheList[i] = (EMERG_WRITE_CACHE*)(mEmergWriteBuf+i*(4*1024+LC_EMERG_CACHE_BLOCKB_TOTAL));
			memset(mEmergFileCacheList[i], 0, sizeof(EMERG_WRITE_CACHE));
			if(fileDuration>0)
			{
				if(camCtrl->frameCnt>60)
					camCtrl->recIndex = camCtrl->frameCnt-60;
				else 
					camCtrl->recIndex = 0;
				duration = fileDuration;
				warnType = LC_SPECIAL_EMERGNECY_EVENT;
			}
			else
			{
				camCtrl->recIndex = camCtrl->frameCnt;
				duration = 120;
			}

			int creatSize = RecordGetEmergencySize(duration)+(1024*0124-1);
			creatSize += (2*1024*1024);
			creatSize &= ~(1024*0124-1);
			
			mEmergFileCacheList[i]->camId = i;	
			mEmergFileCacheList[i]->duration = duration;
			mEmergFileCacheList[i]->curIdx = 0;
			mEmergFileCacheList[i]->fileTotalLen = 0;
			mEmergFileCacheList[i]->BlockBCacheHead = 0;
			mEmergFileCacheList[i]->creatSize = creatSize;

			RecordGenEmergencyFile(mEmergFileCacheList[i]->fileName, duration, i);
			RecordSetCurEmergerncyType(warnType);
			vc_jpeg_take_picture(i, mEmergFileCacheList[i]->fileName);			
		}

		mEmergencyRecordStart = 1;
		mEmergencyRecordRun = 1;
		logd_lc("emgency duration:%d", duration);
	}
	else
	{
		if(fileDuration>0)
		{
			duration = fileDuration;
		}
		else
		{
			duration = 120;
		}	
		
		for(int i=0; i<LC_EMERG_CACHE_NUMBER; i++)
		{
			mEmergFileCacheList[i]->duration += duration;
		}
	} 
}

