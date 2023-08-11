#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include "lc_glob_type.h"
#include "lc_dvr_mem_write_cache.h"
#include "lc_dvr_record_log.h"
#include "duvonn_storage.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_mmc.h"

#define LC_DVR_CACHE_CAMERA_NUMBER (2*LC_DVR_SUPPORT_CAMERA_NUMBER)
#define LC_MEMWRITE_CACHE_BLOCKA_SIZE	(2*1024)
#define LC_MEMWRITE_CACHE_BLOCKB_SIZE	(512*1024)
#define LC_MEMWRITE_CACHE_BLOCK_NUM		4
#define LC_MEMWRITE_CACHE_BLOCKB_TOTAL	(LC_MEMWRITE_CACHE_BLOCKB_SIZE*LC_MEMWRITE_CACHE_BLOCK_NUM)

#define LC_MEMWRITE_LIST_CNT 32
#define LC_MEMCACHE_DIRECT_WRITE
typedef struct{
	char fileName[128];
	int  storageType;
	int  camId;
	int  firstFill;
	int  fileTotalLen;	
	int  BlockACacheHead;
	int  BlockBCacheHead;
	int  cacheSize;
	int  curIdx;
	int  busy;
	int  fileIndex;	
	int  seek_HT;
	int  seek_HH;
	void *fp;
	char cacheBlockA[LC_MEMWRITE_CACHE_BLOCKA_SIZE];
	char cacheBlockB[32];
}MEM_WRITE_CACHE;

typedef struct{
	int fileIndex;
	int camId;
	void* callBack;
}MEM_FILE_LOCK;

typedef struct{
	int camId;
	void *cacheWritePtr;
	int offset;
	int writeLen;
	int cIndex;
	FILE *fp;
}MEM_WRITEBACK_DATA;

typedef struct{
	int camId;
	int memCacheIdx;
	int fp;
}MEM_CLIST;

extern char *pVideoPath[LC_DVR_SUPPORT_CAMERA_NUMBER];

static MEM_WRITE_CACHE *mMemFileCacheList[LC_DVR_CACHE_CAMERA_NUMBER];
static MEM_CLIST mMemCList[LC_DVR_SUPPORT_CAMERA_NUMBER];
static char* mMemWriteBuf;
static char* mLastFileBuf[LC_DVR_SUPPORT_CAMERA_NUMBER];
static int mMemWriteRun;
static MEM_WRITEBACK_DATA mWriteBackList[LC_MEMWRITE_LIST_CNT];
static int mWriteBackHead;
static int mWriteBackTail;
static int mMemWriteCtrlCnt;
static sem_t semWriteBack;//信号量
static pthread_mutex_t mCacheMutex;
static MEM_FILE_LOCK mLockFunc;
static int mStorageError;
//static int mCamWrkStatus;

extern int RecordGetPerFileSize(int camId);
extern int xxCreateMyFile(char *szFileName, int nFileLength);
extern void vc_message_send(int eMsg, int val, void* para, void* func);

// #define CUR_DEGUB
int lc_dvr_mem_get_cam_cache(void *fp)
{
	int index = -1;
	int fpI = (int)fp;
	
	for(int i=0; i<LC_DVR_SUPPORT_CAMERA_NUMBER; i++)
	{
		if(fpI == mMemCList[i].fp) index = mMemCList[i].memCacheIdx;
	}
	
	return index;
}

int lc_dvr_mem_get_idle_cache(void)
{
	int retval = mMemWriteCtrlCnt++;
	mMemWriteCtrlCnt &= (LC_DVR_CACHE_CAMERA_NUMBER-1);
	return retval;
}

int lc_dvr_mem_register_callback(int camId, int Index, void *func)
{
	logd_lc("register callback->%d, %d, %x", camId, Index, func);
	pthread_mutex_lock(&mCacheMutex);
	mLockFunc.camId = camId;
	mLockFunc.fileIndex = Index;
	mLockFunc.callBack = func;
	pthread_mutex_unlock(&mCacheMutex);
}

void lc_dvr_mem_get_callback_para(int *camId, int *Index)
{
	camId[0] = mLockFunc.camId;
	Index[0] = mLockFunc.fileIndex;
}

void *lc_dvr_mem_write_cache_thread(void* arg)
{
	while(mMemWriteRun==1)
	{	
		sem_wait(&semWriteBack);
		while(mWriteBackHead<mWriteBackTail)
		{
			struct timeval start, end;	
			int interval;
			gettimeofday(&start, NULL);
			
			int idx = mWriteBackHead&(LC_MEMWRITE_LIST_CNT-1);						
			int offset = mWriteBackList[idx].offset;
			int len = mWriteBackList[idx].writeLen;
			char *backPtr = (char*)mWriteBackList[idx].cacheWritePtr;
			int cacheIndex = mWriteBackList[idx].cIndex;			
			int curVideoIdx = mMemFileCacheList[cacheIndex]->fileIndex;
			int camId = mMemFileCacheList[cacheIndex]->camId;
			FILE *fp = mMemFileCacheList[cacheIndex]->fp;
			int writeResult;			
			
			if(mMemFileCacheList[cacheIndex]->storageType)
			{
				if(backPtr!=NULL)
				{
					int byteFill = len&(LC_MEMWRITE_CACHE_BLOCKA_SIZE-1);					
#ifdef CUR_DEGUB				
					logd_lc("(%d)duvonn[%d.%d](%d)(%x, %d) -->%x",camId, cacheIndex, mMemFileCacheList[cacheIndex]->busy, curVideoIdx, offset, len, backPtr);
#endif					
					if(byteFill)
					{
						for(int cnt=byteFill; cnt<(LC_MEMWRITE_CACHE_BLOCKA_SIZE); cnt++)
						{
							backPtr[len++] = 0;
						}
					}
					writeResult = duvonn_video_write(camId, curVideoIdx, backPtr, len, offset);
					if(writeResult==0)
					{
						mStorageError &= ~1;
					}
					else
					{
						if((mStorageError&0x01)==0)
						{	/**写卡异常告警**/
							vc_message_send(ENU_LC_DVR_STORAGE_ERROR, mStorageError, NULL, NULL);
						}
						mStorageError |= 1;
					}

					pthread_mutex_lock(&mCacheMutex);
					mMemFileCacheList[cacheIndex]->busy--;
					pthread_mutex_unlock(&mCacheMutex);					
				}
				else
				{
					pthread_mutex_lock(&mCacheMutex);
					LC_ACK_FUNC callFunc = (LC_ACK_FUNC)mLockFunc.callBack;
					if(callFunc!=NULL)
					{
						if((camId==mLockFunc.camId)&&(curVideoIdx==mLockFunc.fileIndex))
						{
							callFunc(camId, curVideoIdx);
							mLockFunc.callBack = NULL;
							mLockFunc.camId = -1;
							mLockFunc.fileIndex = -1;
						}
					}
Record_get_video_relateto_file(mMemFileCacheList[cacheIndex]->fileName);
					memset(mMemFileCacheList[cacheIndex], 0, sizeof(MEM_WRITE_CACHE));
//					mCamWrkStatus &= ~(1<<camId);
//					if(mCamWrkStatus==0)
//					{
//						vc_message_send(ENU_LC_DVR_NOTIFY_RECORD_STATUS, 0, NULL, NULL);
//					}
					pthread_mutex_unlock(&mCacheMutex);							
				}				
			}
			else
			{
				if(backPtr!=NULL)
				{				 
#ifdef CUR_DEGUB				
					logd_lc("(%d)cache[%d.%d](%x)(%x, %d) -->%x", idx, cacheIndex, mMemFileCacheList[cacheIndex]->busy, fp, offset, len, backPtr);
#endif
					if(fp == NULL)
					{
						char *filePath = mMemFileCacheList[cacheIndex]->fileName;
						xxCreateMyFile(filePath, RecordGetPerFileSize(camId));
#ifdef LC_MEMCACHE_DIRECT_WRITE
						fp = (FILE*)direct_file_open(filePath);
#else
						fp = fopen(filePath, "rb+");
#endif
						mMemFileCacheList[cacheIndex]->fp = fp;
//						mCamWrkStatus |= (1<<camId);
						// printf("Video open[%d](%d) open[%x]->%s\n", cacheIndex, camId, fp, filePath);
					}
					
					if(fp!=NULL)
					{
						int retval;
#ifdef LC_MEMCACHE_DIRECT_WRITE
						retval = direct_file_write(fp, backPtr, len, offset);
#else
						fseeko64(fp, (__off64_t)offset, SEEK_SET); /**不能用fseek，否则3分钟长文件有问题**/
						retval = fwrite(backPtr, 1, len, fp);
#endif	
						if(retval==len)
						{
							mStorageError &= ~0x02;
						}
						else
						{
							if((mStorageError&0x02)==0)
							{	/**写卡异常告警**/
								vc_message_send(ENU_LC_DVR_STORAGE_ERROR, mStorageError, NULL, NULL);
							}
							mStorageError|=0x02;
							
							/**存在写卡不成功的可能。**/
							// logd_lc("retval = %d", retval);
							logd_lc("cache Error[%d.%d]-->%x",cacheIndex, mMemFileCacheList[cacheIndex]->busy);							
						}

						pthread_mutex_lock(&mCacheMutex);
						mMemFileCacheList[cacheIndex]->busy--;
						pthread_mutex_unlock(&mCacheMutex);
					}					
				}
				else
				{	
					char *filePath = mMemFileCacheList[cacheIndex]->fileName;				
					printf("Video Close[%d](%d) stop[%x]->%s\n", cacheIndex, camId, fp, filePath);
					if(fp!=NULL)
					{					
#ifdef LC_MEMCACHE_DIRECT_WRITE
						direct_file_close((int)fp);
#else
						fclose(fp);
#endif
						{
							pthread_mutex_lock(&mCacheMutex);
							LC_ACK_FUNC callFunc = (LC_ACK_FUNC)mLockFunc.callBack;
							if(callFunc!=NULL)
							{
								if((camId==mLockFunc.camId)&&(curVideoIdx==mLockFunc.fileIndex))
								{
									callFunc(camId, curVideoIdx);
									mLockFunc.callBack = NULL;
									mLockFunc.camId = -1;
									mLockFunc.fileIndex = -1;
								}
							}
							memset(mMemFileCacheList[cacheIndex], 0, sizeof(MEM_WRITE_CACHE));
							pthread_mutex_unlock(&mCacheMutex);							
						}
//						mCamWrkStatus &= ~(1<<camId);
//						if(mCamWrkStatus==0)
//						{
//							vc_message_send(ENU_LC_DVR_NOTIFY_RECORD_STATUS, 0, NULL, NULL);
//						}
					}
				}
			}						
			mWriteBackHead++;
#ifdef CUR_DEGUB				
			gettimeofday(&end, NULL);
			interval = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) + 500;
			interval /= 1000;
			printf("write_cache[%d] = %dms\n", camId, interval);
#endif			
		}		
	}
	mMemWriteRun = 2;
	return NULL;
}

int lc_dvr_mem_cam_busy_status(void)
{
	return (mWriteBackHead!=mWriteBackTail)?1:0;
}

void lc_dvr_mem_write_cache_initial(void)
{
	pthread_t fielWriteBackThread;
	int err;
	logd_lc("lc_dvr_mem_write_cache_initial");
#ifndef USE_STAND_FILEIO
	logd_lc("write cache in used!");
#endif
	CreateSemaphore();
	direct_file_initial();
	mMemWriteBuf = (char*)malloc((LC_DVR_CACHE_CAMERA_NUMBER*(8*1024+LC_MEMWRITE_CACHE_BLOCKB_TOTAL)));
	logd_lc("mMemWriteBuf memPool-> %x", mMemWriteBuf);
	
	mMemWriteRun = 1;
	mWriteBackHead = 0;
	mWriteBackTail = 0;
	mStorageError = 0;
//	mCamWrkStatus = 0;
	memset(&mLockFunc, 0, sizeof(MEM_FILE_LOCK));
	sem_init(&semWriteBack, 0, 0);
    pthread_mutex_init(&mCacheMutex, NULL);

	for(int i=0; i<LC_DVR_CACHE_CAMERA_NUMBER; i++)
	{
		mMemFileCacheList[i] = (MEM_WRITE_CACHE*)(mMemWriteBuf+i*(8*1024+LC_MEMWRITE_CACHE_BLOCKB_TOTAL));
		memset(mMemFileCacheList[i], 0, sizeof(MEM_WRITE_CACHE));
		mMemFileCacheList[i]->cacheSize = LC_MEMWRITE_CACHE_BLOCKB_SIZE;
	}

	for(int i=0; i<LC_DVR_SUPPORT_CAMERA_NUMBER; i++)
	{
		memset(&mMemCList[i], 0, sizeof(MEM_CLIST));
		mMemCList[i].camId = -1;
	}
	
    err = pthread_create(&fielWriteBackThread, NULL, lc_dvr_mem_write_cache_thread, NULL);
    if (0 != err)
    {
        printf("can't create write back thread: %s\n", strerror(err));
    }
    pthread_detach(fielWriteBackThread);
	
}

void lc_dvr_mem_write_cache_release(void)
{
	free(mMemWriteBuf);
	direct_file_unInitial();
	mMemWriteRun = 0;
	sem_post(&semWriteBack);
	do{
		usleep(100*1000);
	}while(mMemWriteRun==0);
	sem_destroy(&semWriteBack);
}

void lc_dvr_mem_writeBack_opration(int fIndex, void* wbPtr, int len, int offset)
{
	pthread_mutex_lock(&mCacheMutex);	
	int idx = mWriteBackTail&(LC_MEMWRITE_LIST_CNT-1);
	mWriteBackTail++;		
	mWriteBackList[idx].cacheWritePtr = wbPtr;
	mWriteBackList[idx].writeLen = len;
	mWriteBackList[idx].offset = offset;
	mWriteBackList[idx].cIndex = fIndex;
	mMemFileCacheList[fIndex]->busy++;
//#ifdef CUR_DEGUB	
//	logd_lc("fIndex(%x)[%d] write->%d!",mMemFileCacheList[fIndex]->fp, fIndex, idx);
//#endif
	sem_post(&semWriteBack);
	pthread_mutex_unlock(&mCacheMutex);		
}

void* lc_dvr_mem_write_cache_open(char* filePath, char *modes)
{
	int i, camId=-1;
	int cacheIndex;
	char *sPtr;

	for(i=0; i<LC_DVR_SUPPORT_CAMERA_NUMBER; i++)
	{
		if(strstr(filePath, pVideoPath[i])!=NULL)
		{
			camId = i;
			break;
		}
	}
	if(camId<0) return NULL;
	pthread_mutex_lock(&mCacheMutex);
	for(int i=0; i<LC_DVR_CACHE_CAMERA_NUMBER; i++)
	{
		cacheIndex = lc_dvr_mem_get_idle_cache();
		if(mMemFileCacheList[cacheIndex]->fileName[0] == 0) break; //保证该块为空闲状态，通过遍历的方式找空闲的块
	}
	pthread_mutex_unlock(&mCacheMutex);

	logd_lc("cache index:%d", cacheIndex);
	
	memset(mMemFileCacheList[cacheIndex], 0, sizeof(MEM_WRITE_CACHE));
	mMemFileCacheList[cacheIndex]->cacheSize = LC_MEMWRITE_CACHE_BLOCKB_SIZE;	
	for(i=strlen(filePath); i>0; i--)
	{
		if(filePath[i]=='-')
		{
			sPtr = filePath+i-3;
		}
	}
	mMemFileCacheList[cacheIndex]->fileIndex = atoi(sPtr);	/**通过文件名记录文件序号**/
	strcpy(mMemFileCacheList[cacheIndex]->fileName, filePath);
	mMemFileCacheList[cacheIndex]->camId = camId;
	mMemFileCacheList[cacheIndex]->seek_HT = 0x7fffffff,
	mMemFileCacheList[cacheIndex]->seek_HH = -1;
logd_lc("fileIndex = %d", mMemFileCacheList[cacheIndex]->fileIndex);
	if(is_duvonn_device())
	{
		switch(camId)
		{
			case LC_DUVONN_CHANEL_0:
			case LC_DUVONN_CHANEL_1:
				mMemFileCacheList[cacheIndex]->storageType = 1;
				break;
		}
	}
	mMemCList[camId].camId = camId;
	mMemCList[camId].fp = (rand()&0xffffff00)+camId;
	mMemCList[camId].memCacheIdx = cacheIndex;
	logd_lc("Cam[%d]cache open[%d](%x):%s",camId, cacheIndex, mMemCList[camId].fp, mMemFileCacheList[cacheIndex]->fileName);

	return (void*)mMemCList[camId].fp;
}

int lc_dvr_mem_write_cache_write(void* stream, void* input, int len)
{	
	int index = lc_dvr_mem_get_cam_cache(stream); /**通过文件句柄映射到对应摄像头通道**/

	if(index>=0)
	{
		int curIdx = mMemFileCacheList[index]->curIdx;  /**当前偏移位置**/
		int cnt, total = len;
		int backCurIdx = curIdx;
		int backTotal = mMemFileCacheList[index]->fileTotalLen;
		char *writePtr = (char*)input;
		char *buf;

//		if(mMemFileCacheList[index]->busy>(LC_MEMWRITE_CACHE_BLOCK_NUM>>1))
//		{
//			printf("xxxxx write[%d] fast! xxxxx\n", mMemFileCacheList[index]->camId); /**如果两个缓冲块都被占用，则直接丢弃该数据**/
//			usleep(20*1000);
//			//return total;
//		}

		mMemFileCacheList[index]->curIdx += total;	/**下一次的偏移位置，同时也作为文件的长度**/
		if(mMemFileCacheList[index]->curIdx>mMemFileCacheList[index]->fileTotalLen)
		{
			mMemFileCacheList[index]->fileTotalLen = mMemFileCacheList[index]->curIdx;
		}

		if(curIdx<LC_MEMWRITE_CACHE_BLOCKA_SIZE)
		{
			if(mMemFileCacheList[index]->firstFill!=0)	/**已经被填满的4K缓存，在文件seek回文件前面时用**/
			{
				buf = mMemFileCacheList[index]->cacheBlockA; /**实际测试，seek到前面时不会超过4k**/
				memcpy(buf+curIdx, writePtr, len);
				return len;
			}		
		}

		buf = mMemFileCacheList[index]->cacheBlockB;

		int stepOneByte = LC_MEMWRITE_CACHE_BLOCKB_SIZE - (curIdx&(LC_MEMWRITE_CACHE_BLOCKB_SIZE-1));					

		if(total>=stepOneByte)	/**填写数据后长度超过一个block的长度**/
		{			
			curIdx &= (LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
			memcpy(buf+curIdx, writePtr, stepOneByte); /**分两部分写，先写满一个block，并回写到SD卡**/
			//===================================================================
			/**通过进程顺序回写**/
			int writeBackIndex = mMemFileCacheList[index]->BlockBCacheHead&(LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
			char *backPtr = mMemFileCacheList[index]->cacheBlockB + writeBackIndex;

			if(mMemFileCacheList[index]->firstFill==0)
			{			
				mMemFileCacheList[index]->firstFill = 1;	/**把文件最开始的4K内容备份到blockA中**/
				memcpy(mMemFileCacheList[index]->cacheBlockA, backPtr, LC_MEMWRITE_CACHE_BLOCKA_SIZE);
			}
			
			// logd_lc("stream:%x, index:%d, camId:%d", stream, index, mMemFileCacheList[index]->camId);
			lc_dvr_mem_writeBack_opration(index, backPtr, LC_MEMWRITE_CACHE_BLOCKB_SIZE, mMemFileCacheList[index]->BlockBCacheHead);
			//===================================================================
			mMemFileCacheList[index]->BlockBCacheHead += LC_MEMWRITE_CACHE_BLOCKB_SIZE;
			curIdx += stepOneByte; /**剩下不足的数据写到另外的block**/
			len -= stepOneByte;
			writePtr += stepOneByte;
		}

		curIdx &= (LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
		if(len < LC_MEMWRITE_CACHE_BLOCKB_SIZE)
		{
			memcpy(buf+curIdx, writePtr, len);
		}
		else
		{
			logd_lc("xxxxx write over len! xxxxx");
		}
		return total;
	}
	else
	{
		return -1;
	}
}


int lc_dvr_mem_write_cache_seek(void* stream, uint32_t offset)
{
	int index = lc_dvr_mem_get_cam_cache(stream);	
	if(index>=0)
	{
		pthread_mutex_lock(&mCacheMutex);
		mMemFileCacheList[index]->curIdx = offset;
		pthread_mutex_unlock(&mCacheMutex);
		if(offset>1024*1024)
		{
			if((int)offset<mMemFileCacheList[index]->seek_HT) mMemFileCacheList[index]->seek_HT = offset;
			if((int)offset>mMemFileCacheList[index]->seek_HH) mMemFileCacheList[index]->seek_HH = offset;
		}

	//	printf("cidx:%x, len:%x\n", offset, mMemFileCacheList[camId]->fileTotalLen);
		return 0;
	}
	else
	{
		return -1;
	}
}

int lc_dvr_mem_write_cache_tell(void* stream)
{
	int index = lc_dvr_mem_get_cam_cache(stream);
	if(index>=0)
	{
		pthread_mutex_lock(&mCacheMutex);	
		mMemFileCacheList[index]->curIdx = mMemFileCacheList[index]->fileTotalLen;
		pthread_mutex_unlock(&mCacheMutex);	
		return mMemFileCacheList[index]->fileTotalLen;
	}
	else
	{
		return -1;
	}
}

int lc_dvr_mem_write_cache_close(void* stream, int* size)
{
	int index = lc_dvr_mem_get_cam_cache(stream);	
	if(index>=0)
	{
		int writeBackIndex, offset, len;
		char *wrtPtr;
		int camId = mMemFileCacheList[index]->camId;
		logd_lc("cache_close begin!");
		/**seek to 0, 然后把头部4K内容写回去**/
		// printf("CacheHead->%x, curIdx:%x\n", mMemFileCacheList[index]->BlockBCacheHead, mMemFileCacheList[index]->curIdx); 	

		wrtPtr = mMemFileCacheList[index]->cacheBlockA;
		lc_dvr_mem_writeBack_opration(index, wrtPtr, LC_MEMWRITE_CACHE_BLOCKA_SIZE, 0);

		if(mMemFileCacheList[index]->fileTotalLen>LC_MEMWRITE_CACHE_BLOCKB_SIZE)
		{
			len = LC_MEMWRITE_CACHE_BLOCKB_SIZE;	/**把最尾部的前一个block回写一次，这部分会被修改**/
			offset = mMemFileCacheList[index]->BlockBCacheHead-len;			
			
			writeBackIndex = offset&(LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
			wrtPtr = mMemFileCacheList[index]->cacheBlockB+writeBackIndex;
			lc_dvr_mem_writeBack_opration(index, wrtPtr, len, offset);

			// logd_lc("offset:%x, seekHT:%x", offset, mMemFileCacheList[index]->seek_HT);
			if(offset > mMemFileCacheList[index]->seek_HT)
			{
				offset -= len;
				writeBackIndex = offset&(LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
				wrtPtr = mMemFileCacheList[index]->cacheBlockB+writeBackIndex;
				lc_dvr_mem_writeBack_opration(index, wrtPtr, len, offset);
			}
		}
		
		/**seek to BlockBCacheHead, 然后把结尾的内容写进文件**/
		len = mMemFileCacheList[index]->fileTotalLen - mMemFileCacheList[index]->BlockBCacheHead;

		if(len>0)
		{
			offset = mMemFileCacheList[index]->BlockBCacheHead;
			writeBackIndex = offset&(LC_MEMWRITE_CACHE_BLOCKB_TOTAL-1);
			wrtPtr = mMemFileCacheList[index]->cacheBlockB+writeBackIndex;
			lc_dvr_mem_writeBack_opration(index, wrtPtr, len, offset);			
		}
		
		if(mMemFileCacheList[index]->storageType)
		{
			int *tailBuf = (int*)wrtPtr;
			int fIndex = mMemFileCacheList[index]->fileIndex;
			memset(tailBuf, 0xff, 4096);
			tailBuf[0] = mMemFileCacheList[index]->fileTotalLen;
			lc_dvr_mem_writeBack_opration(index, tailBuf, 4096, duvonn_get_video_tail(camId, fIndex)-4096); /**记录文件的长度信息**/
		}		
		
		if(size!=NULL)
		{
			size[0] = mMemFileCacheList[index]->fileTotalLen;
		}
		logd_lc("File[%d]:%s->TotalLen:%d", index, mMemFileCacheList[index]->fileName, mMemFileCacheList[index]->fileTotalLen);/**如果写文件比这一条先执行，打印出来都是NLL**/
		lc_dvr_mem_writeBack_opration(index, NULL, 0, -1); /**写空表示文件结束**/
				
		return 0;
	}
	else
	{
		return -1;
	}
}

int lc_dvr_mem_video_file_read(int camId, int index, char* dir, char *fileNmae)
{
	char *memReadBuf = (char*)malloc(512*1024);
	void *fragmentPtr;
	int *s32Ptr = (int*)memReadBuf;
	int len, remain;
	char savePath[128];
	int byteNum, retval=0;
	int offset;
	unsigned long long as;
	
	offset = duvonn_get_video_tail(camId, index)-4096;
	duvonn_video_read(camId, index, memReadBuf, 4096, offset);
	len = s32Ptr[0];	

	if(len>254*1024*1024) return -3;
	
	as = availSize(dir)*1024*1024;

	if(as > len)
	{	
		sprintf(savePath, "%s/%s", dir, fileNmae); //(char*)duvonn_get_video_name(fragmentPtr)
		logd_lc("[%d_%d]VideoSave(%d):%s", camId, index, len, savePath);
		FILE* fp = fopen(savePath, "wb");
		offset = 0;
		
		for(remain=len; remain>0;)
		{
			int cpLen = remain>512*1024?512*1024:remain;
			int readLen = LC_ALIGN_4K(cpLen);
			usleep(1000);
			duvonn_video_read(camId, index, memReadBuf, readLen, offset);
			offset+=readLen;
			byteNum = fwrite(memReadBuf, 1, cpLen, fp);
			if(byteNum != cpLen)
			{
				logd_lc("duvonn copy err!");
				retval = -1;
				break;
			}
			if(vc_video_file_copy_status() == 0)
			{
				retval = -3;
				break;
			}
			remain -= cpLen;
		}
		fclose(fp);
		logd_lc("duvonn copy finish!");
	}

	free(memReadBuf);	
	return retval;
}

//int lc_dvr_mem_video_file_read(int camId, int index, char* dir, char *fileNmae)
//{
//	char *memReadBuf = (char*)malloc(512*1024);
//	void *fragmentPtr;
//	int *s32Ptr = (int*)memReadBuf;
//	int len, remain;
//	char savePath[128];
//	int byteNum, retval=0;
//	unsigned long long as;
//	
//	fragmentPtr = duvonn_video_open(camId, index);	
//	duvonn_ctrl_seek(fragmentPtr, duvonn_get_video_tail(camId, index)-4096, 0);
//	duvonn_ctrl_read(memReadBuf, 4096, fragmentPtr);
//	len = s32Ptr[0];
//	duvonn_ctrl_seek(fragmentPtr, 0, 0);	
//	as = availSize(dir)*1024*1024;

//	if(as > len)
//	{	
//		sprintf(savePath, "%s/%s", dir, fileNmae); //(char*)duvonn_get_video_name(fragmentPtr)
//		logd_lc("[%d_%d]VideoSave(%d):%s", camId, index, len, savePath);
//		FILE* fp = fopen(savePath, "wb");
//		for(remain=len; remain>0;)
//		{
//			int cpLen = remain>512*1024?512*1024:remain;
//			int readLen = LC_ALIGN_4K(cpLen);

//			duvonn_ctrl_read(memReadBuf, readLen, fragmentPtr);
//			byteNum = fwrite(memReadBuf, 1, cpLen, fp);
//			if(byteNum != cpLen)
//			{
//				logd_lc("duvonn copy err!");
//				retval = -1;
//				break;
//			}
//			remain -= cpLen;
//		}
//		fclose(fp);
//	}
//	
//	duvonn_ctrl_close(fragmentPtr);
//	free(memReadBuf);	
//	return retval;
//}


void lc_dvr_mem_write_cache_test(void)
{
	void *fp;
	char *testBuf = (char*)malloc(128*1024);

	srand(176);
	memset(testBuf, 0, 128*1024);
	lc_dvr_mem_write_cache_initial();	
	fp = lc_dvr_mem_write_cache_open("./mnt/usb/usb1/record/frontVideo0/rear1_120811.mp4", "wb+");

	for(int i=0; i<1024; i++)
	{
		int writeByte = rand()&0x1ffff;
		lc_dvr_mem_write_cache_write(fp, testBuf, writeByte);
	}

	lc_dvr_mem_write_cache_seek(fp, 40);
	for(int i=0; i<32; i++)
	{
		lc_dvr_mem_write_cache_write(fp, testBuf, 32);
	}

	lc_dvr_mem_write_cache_tell(fp);
	for(int i=0; i<32; i++)
	{
		lc_dvr_mem_write_cache_write(fp, testBuf, 32);
	}

	lc_dvr_mem_write_cache_close(fp, NULL);
	free(testBuf);
}
