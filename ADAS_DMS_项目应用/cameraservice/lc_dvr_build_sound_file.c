#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>	   // mkfifo
#include <errno.h>

#include "VimRecordSdk.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_mmc.h"
#include "lc_glob_type.h"
#include "lc_dvr_record_log.h"
#include "lc_dvr_build_sound_file.h"

#define FIFO_AUDIO_W0 "/tmp/LcAudioPipe"
//==================================================================
typedef struct{
	char fileName[128];
	int  duration;
	int  creatSize;
	int  fileTotalLen;	
	int  BlockBCacheHead;
	int  index;
	int  curIdx;
	void *fp;
	char firstBlock[2048];
	char cacheBlockB[32];
}SOUND_WRITE_CACHE;

typedef struct{
	void *cacheWritePtr;
	int offset;
	int writeLen;
	int cIndex;
	FILE *fp;
}SOUND_WRITEBACK_DATA;

typedef struct WAVE_HEADER{  
	char		 fccID[4];		  
	unsigned   long    dwSize;			  
	char		 fccType[4];	
}WAVE_HEADER;  

typedef struct WAVE_FMT{  
	char		 fccID[4];		  
	unsigned   long 	  dwSize;			 
	unsigned   short	 wFormatTag;	
	unsigned   short	 wChannels;  
	unsigned   long 	  dwSamplesPerSec;	
	unsigned   long 	  dwAvgBytesPerSec;  
	unsigned   short	 wBlockAlign;  
	unsigned   short	 uiBitsPerSample;  
}WAVE_FMT;	

typedef struct WAVE_DATA{  
	char	   fccID[4];		  
	unsigned long dwSize;			   
}WAVE_DATA; 

#define LC_SOUND_CACHE_NUMBER (1)
#define LC_SOUND_CACHE_BLOCKB_SIZE	(512*1024)
#define LC_SOUND_CACHE_BLOCK_NUM	2
#define LC_SOUND_CACHE_BLOCKB_TOTAL	(LC_SOUND_CACHE_BLOCK_NUM*LC_SOUND_CACHE_BLOCKB_SIZE)
#define LC_SOUND_LIST_CNT	4
#define LC_SOUND_FILE_CNT   2
#define LC_SOUND_DIRECT_WRITE
#define LC_WAVE_HAED_LEN 		(sizeof(WAVE_HEADER)+sizeof(WAVE_FMT)+sizeof(WAVE_DATA))


static SOUND_WRITE_CACHE *mSoundFileCacheList[LC_SOUND_FILE_CNT];
static SOUND_WRITEBACK_DATA mSoundBackList[LC_SOUND_LIST_CNT];

static int mSoundBackHead;
static int mSoundBackTail;
static sem_t semSoundBack;//信号量
static pthread_mutex_t mSoundMutex;
static int mSoundThreadRun;

static int mFd_audio;
static int mAudioPipeRun;
static int mSoundDebugCnt = 0;
//========================================================
static char *mSoundMemCache = NULL;
static int mSoundRecordSign;
static int mSoundWriteRun;
static int mSoundRecordSize;
static int mSoundRecordContinue;
static int mListIndex;
static int mSoundDuration;
//==================================================================
extern int RecordGenSoundFile(char *name);
extern void RecordSoundFinsh(int size);
//------------------------------------------------------------------
int pcm16le_to_wave_head(int channels,int sample_rate, int len, char* blockHead)
{  
    WAVE_HEADER   *pcmHEADER;  
    WAVE_FMT   *pcmFMT;  
    WAVE_DATA   *pcmDATA;
	int bits = 16; 
	int offset = sizeof(WAVE_HEADER)+sizeof(WAVE_FMT)+sizeof(WAVE_DATA);
	
	pcmHEADER = (WAVE_HEADER*)blockHead;
	pcmFMT = (WAVE_FMT*)(blockHead+sizeof(WAVE_HEADER));
	pcmDATA = (WAVE_DATA*)(blockHead+sizeof(WAVE_HEADER)+sizeof(WAVE_FMT));
	//WAVE_HEADER
    memcpy(pcmHEADER->fccID,"RIFF",strlen("RIFF"));                    
    memcpy(pcmHEADER->fccType,"WAVE",strlen("WAVE"));
	
	//WAVE_FMT
    pcmFMT->dwSamplesPerSec=sample_rate;  
    pcmFMT->dwAvgBytesPerSec=pcmFMT->dwSamplesPerSec*sizeof(short);  
    pcmFMT->uiBitsPerSample=bits;
    memcpy(pcmFMT->fccID,"fmt ",strlen("fmt "));  
    pcmFMT->dwSize=16;
    pcmFMT->wBlockAlign=2;
    pcmFMT->wChannels=channels;
    pcmFMT->wFormatTag=1;

    memcpy(pcmDATA->fccID,"data",strlen("data"));   	
 	pcmDATA->dwSize = len;
    pcmHEADER->dwSize=36+pcmDATA->dwSize;
 
    return offset;
}

static int lc_audio_pipe_creat(void)	
{ 
	/** 若fifo已存在，则直接使用，否则创建它**/
	mFd_audio = -1;
	if((mkfifo(FIFO_AUDIO_W0,0777) == -1) && errno != EEXIST){
		loge_lc("mkfifo write failure\n");
	}
 
	/**以非阻塞型只写方式打开fifo，扫描方式判断是否成功**/
	mFd_audio = open(FIFO_AUDIO_W0,O_WRONLY | O_NONBLOCK);
	return mFd_audio;
}

int lc_audio_pipe_quit(void)
{
	mAudioPipeRun = 0;
	if(mFd_audio>=0)
	{
		int closeHandle = mFd_audio;
		mFd_audio = -1;
		usleep(500*1000);
		int retval = close(closeHandle);	
		logd_lc("lc_venc_pipe_destroy->%d", retval);
	}
	
	return 0;
}

static void *vc_audio_pipe_handler(void *arg)
{
	int s32Ret;
	logd_lc("audio creat sound thread begin!");
	
    while (mAudioPipeRun)
    {	
		s32Ret = lc_audio_pipe_creat();
		if(s32Ret<0) 
			usleep(250*1000);
		else 
			break;
    }
	mAudioPipeRun = 0;
	logd_lc("sound pipe creat ok!");
	return NULL;
}

int lc_audio_pipe_write(void* data, int len)
{	
	int tryTime, retval = 0;
	int byteCnt, remain, curLen = 0;
	char *dPtr = (char*)data;
	if(mFd_audio<0) return 0;
	
	tryTime = 0;
	for(remain = len; ; )
	{
		byteCnt = remain>(96*1024)?(96*1024):remain;		
		retval = write(mFd_audio, dPtr, byteCnt);
		remain -= byteCnt;
		if(remain == 0) break;
		
		dPtr += byteCnt;
		usleep(5*1000);
	}
	return retval;
}

void lc_audio_export_start(void)
{
	int s32Ret;
	pthread_t pipe_thread;
	mAudioPipeRun = 1;
	s32Ret = pthread_create(&pipe_thread, NULL, (void*)vc_audio_pipe_handler, NULL);
	if (0 == s32Ret)
	{
		logd_lc("Audio pipe creat pthread SUCCESS\n");
		pthread_detach(pipe_thread);
	}	
}

void *lc_dvr_sound_write_cache_thread(void* arg)
{
	sem_init(&semSoundBack, 0, 0);
    pthread_mutex_init(&mSoundMutex, NULL);
	logd_lc("sound_write_cache_thread begin!");
	
	while(mSoundThreadRun==1)
	{	
		sem_wait(&semSoundBack);	
		while(mSoundBackHead<mSoundBackTail)
		{			
			int idx = mSoundBackHead&(LC_SOUND_LIST_CNT-1);						
			int offset = mSoundBackList[idx].offset;
			int len = mSoundBackList[idx].writeLen;
			char *backPtr = (char*)mSoundBackList[idx].cacheWritePtr;
			int cacheIndex = mSoundBackList[idx].cIndex;
			FILE *fp = mSoundFileCacheList[cacheIndex]->fp;			

			if(backPtr!=NULL)
			{				 
				if(fp == NULL)
				{
					char *filePath = mSoundFileCacheList[cacheIndex]->fileName;					
					xxCreateMyFile(filePath, mSoundFileCacheList[cacheIndex]->creatSize);
#ifdef LC_SOUND_DIRECT_WRITE
					fp = (FILE*)direct_file_open(filePath);
#else
					fp = fopen(filePath, "rb+");
#endif
					mSoundFileCacheList[cacheIndex]->fp = fp;
					printf("Sound open[%d] open[%x]->%s\n", cacheIndex, fp, filePath);
				}
				
				if(fp!=NULL)
				{
					int retval;
					// printf("Sound_%x:(%x) %d->%d\n", fp, backPtr, len, offset);
#ifdef LC_SOUND_DIRECT_WRITE
					retval = direct_file_write(fp, backPtr, len, offset);
#else
					fseeko64(fp, (__off64_t)offset, SEEK_SET); /**不能用fseek，否则3分钟长文件有问题**/
					retval = fwrite(backPtr, 1, len, fp);
#endif	
				}					
			}
			else
			{	
				char *filePath = mSoundFileCacheList[cacheIndex]->fileName;	
				int nullCnt;
				printf("Sound Close(%d) stop[%x]->%s\n", cacheIndex, fp, filePath);
				if(fp!=NULL)
				{					
#ifdef LC_SOUND_DIRECT_WRITE
					direct_file_close((int)fp);
#else
					fclose(fp);
#endif						
					
					mSoundFileCacheList[cacheIndex]->fp = NULL;	
					if(mSoundRecordContinue)
					{			
						mListIndex++;
						lc_dvr_sound_record_start(mSoundDuration, 1);
					}					
				}
			}
			
			mSoundBackHead++;
//			gettimeofday(&end, NULL);
//			interval = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) + 500;
//			interval /= 1000;
//			printf("xWrite_cache[%d] = %dms\n", camId, interval);			
		}		
	}

	sem_destroy(&semSoundBack);
	logd_lc("All emergency file finish!");
	return NULL;
}

void lc_dvr_sound_writeBack_opration(int fIndex, void* wbPtr, int len, int offset)
{
	pthread_mutex_lock(&mSoundMutex);		
	int idx = mSoundBackTail&(LC_SOUND_LIST_CNT-1);
	mSoundBackTail++;		
	mSoundBackList[idx].cacheWritePtr = wbPtr;
	mSoundBackList[idx].writeLen = len;
	mSoundBackList[idx].offset = offset;
	mSoundBackList[idx].cIndex = fIndex;
	sem_post(&semSoundBack);
	pthread_mutex_unlock(&mSoundMutex);
}


int lc_sound_write(int index, void* dataIn, int len)
{	
	int curIdx = mSoundFileCacheList[index]->curIdx;	/**当前偏移位置**/
	int total = len;
	char *writePtr = (char*)dataIn;
	char *buf;

	mSoundFileCacheList[index]->curIdx += total;	/**下一次的偏移位置，同时也作为文件的长度**/
	mSoundFileCacheList[index]->fileTotalLen += total;
	buf = mSoundFileCacheList[index]->cacheBlockB;

	int stepOneByte = LC_SOUND_CACHE_BLOCKB_SIZE - (curIdx&(LC_SOUND_CACHE_BLOCKB_SIZE-1));
//	logd_lc("total:%d One:%d", total, stepOneByte);
	if(total>=stepOneByte)	/**填写数据后长度超过一个block的长度**/
	{			
		do{
			curIdx &= (LC_SOUND_CACHE_BLOCKB_TOTAL-1);
			memcpy(buf+curIdx, writePtr, stepOneByte); /**分两部分写，先写满一个block，并回写到SD卡**/
			//===================================================================
			/**通过进程顺序回写**/
			int writeBackIndex = mSoundFileCacheList[index]->BlockBCacheHead&(LC_SOUND_CACHE_BLOCKB_TOTAL-1);
			char *backPtr = mSoundFileCacheList[index]->cacheBlockB + writeBackIndex;	
			if(mSoundFileCacheList[index]->BlockBCacheHead == 0)
			{
				memcpy(mSoundFileCacheList[index]->firstBlock, backPtr, 2048);
			}
			lc_dvr_sound_writeBack_opration(index, backPtr, LC_SOUND_CACHE_BLOCKB_SIZE, mSoundFileCacheList[index]->BlockBCacheHead);
			//===================================================================
			mSoundFileCacheList[index]->BlockBCacheHead += LC_SOUND_CACHE_BLOCKB_SIZE;
			curIdx += stepOneByte; /**剩下不足的数据写到另外的block**/
			len -= stepOneByte;
			writePtr += stepOneByte;
		}while(len >= LC_SOUND_CACHE_BLOCKB_SIZE);
	}

	curIdx &= (LC_SOUND_CACHE_BLOCKB_TOTAL-1);
	memcpy(buf+curIdx, writePtr, len);
	return total;
}

int lc_dvr_sound_close(int index)
{
	int writeBackIndex, offset, len;
	char *wrtPtr;
	logd_lc("sound_close begin!");
	mSoundFileCacheList[index]->duration = 0;
	len = mSoundFileCacheList[index]->fileTotalLen - mSoundFileCacheList[index]->BlockBCacheHead;

	if(len>0)
	{
		int reaLen = LC_ALIGN_4K(len);
		int soundSize = (mSoundFileCacheList[index]->fileTotalLen - LC_WAVE_HAED_LEN);
		offset = mSoundFileCacheList[index]->BlockBCacheHead;
		writeBackIndex = offset&(LC_SOUND_CACHE_BLOCKB_TOTAL-1);
		wrtPtr = mSoundFileCacheList[index]->cacheBlockB+writeBackIndex;
		if(reaLen>len) memset(wrtPtr+len, 0, reaLen-len);
		lc_dvr_sound_writeBack_opration(index, wrtPtr, reaLen, offset);	
		//logd_lc("total len:%d soundSize:%d", mSoundFileCacheList[index]->fileTotalLen, soundSize);
		wrtPtr = mSoundFileCacheList[index]->firstBlock;
		pcm16le_to_wave_head(1, 16000, soundSize, wrtPtr);
		lc_dvr_sound_writeBack_opration(index, wrtPtr, 2048, 0);
	}

	logd_lc("Sound File[%d]:%s->TotalLen:%d", index, mSoundFileCacheList[index]->fileName, mSoundFileCacheList[index]->fileTotalLen);/**如果写文件比这一条先执行，打印出来都是NLL**/
	lc_dvr_sound_writeBack_opration(index, NULL, 0, -1); /**写空表示文件结束**/
			
	return 0;
}
//------------------------------------------------------------------
void lc_dvr_sound_data_insert(void* data, int len)
{
	if(mSoundRecordSign)
	{
		int index = (mListIndex&(LC_SOUND_FILE_CNT-1));	
		lc_sound_write(index, data, len);
		mSoundRecordSize -= len;
		if(mSoundRecordSize <= 0)
		{
			printf("lc_dvr_sound_close\n");			
			lc_dvr_sound_close(index);
			RecordSoundFinsh(mSoundFileCacheList[index]->fileTotalLen);
			mSoundRecordSign = 0;
		}
	}	
	mSoundDebugCnt++;

	if(mFd_audio>=0)
	{
		lc_audio_pipe_write(data, len);
	}
}

void lc_dvr_sound_record_stop(void)
{
	mSoundRecordSize = 0;
	mSoundRecordContinue = 0;
}

void lc_dvr_sound_record_start(int duration, int conti)
{
	logd_lc("sound_record_start(%d, %d)!->%d", duration, conti, mListIndex);
	char headInf[64];
	int len, cIndex = (mListIndex&(LC_SOUND_FILE_CNT-1));
	
	if(duration>0)
	{
		if(mSoundRecordSign == 0)
		{
			memset(mSoundFileCacheList[cIndex], 0, sizeof(SOUND_WRITE_CACHE));
			RecordGenSoundFile(mSoundFileCacheList[cIndex]->fileName);
			mSoundFileCacheList[cIndex]->duration = duration;	
			mSoundDuration = duration;
			mSoundRecordSign = 1;
			mSoundRecordContinue = conti;
			mSoundRecordSize = duration*16000*2;

			memset(headInf, 0, 64);
			mSoundFileCacheList[cIndex]->creatSize = (LC_WAVE_HAED_LEN+mSoundRecordSize+(64*1024-1)&(~(64*1024-1)));
			lc_sound_write(cIndex, headInf, LC_WAVE_HAED_LEN);			
		}
	}
	else
	{
		lc_dvr_sound_record_stop();
	}
}

void lc_dvr_sound_record_initial(void)
{	
	int err;
	pthread_t soundBackThread;
	
	logd_lc("sound_record_initial!");
	if(mSoundMemCache == NULL) 
	{
		mSoundMemCache = (char*)malloc(LC_SOUND_FILE_CNT*(4*1024+LC_SOUND_CACHE_BLOCKB_TOTAL));
	}
	
	mSoundBackHead = 0;
	mSoundBackTail = 0;
	mListIndex = 0;

	for(int i=0; i<2; i++)
	{
		mSoundFileCacheList[i] = (SOUND_WRITE_CACHE*)(mSoundMemCache+i*(4*1024+LC_SOUND_CACHE_BLOCKB_TOTAL));		
		memset(mSoundFileCacheList[i], 0, sizeof(SOUND_WRITE_CACHE));
	}

	mSoundRecordSign = 0;
	mSoundRecordSize = 0;
	mSoundRecordContinue = 0;
	mSoundWriteRun = 1;
	mSoundThreadRun = 1;

    err = pthread_create(&soundBackThread, NULL, lc_dvr_sound_write_cache_thread, NULL);
    if (0 != err)
    {
        printf("can't create emergeBackThread: %s\n", strerror(err));
    }
    pthread_detach(soundBackThread);
}

void lc_dvr_sound_record_unInitial(void)
{
	if(mSoundMemCache != NULL) free(mSoundMemCache);
}

void lc_dvr_sound_file_query(void* para)
{
	LC_DVR_SOUND_RECORD_QUERY *queryInfo = (LC_DVR_SOUND_RECORD_QUERY*)para;
}

