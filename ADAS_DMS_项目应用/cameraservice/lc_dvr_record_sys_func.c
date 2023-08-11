#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lc_dvr_record_log.h"
#include "lc_dvr_record_sys_func.h"
//==============================================
int dDay=-1, dHour, dMin, dSec;

#define LC_MESSAGE_QUEUE_NUM  128
#define LC_MEM_QUEUE_LENGTH   (256*1024)
#define LC_MEM_CACHE_BLOCK_SIZE 64

char *mMessageMemPool;
char *mMemBufPool;
int mMsgHead; 
int mMsgTail, mMsgTail1, mMsgTail2;
int mMemCachePosition;
static sem_t sem;//ÐÅºÅÁ¿
static pthread_mutex_t mMsgMutex;

int LC_SYS_DEBUG_ENABLE = 0; 

void sys_debug_enable(int state)
{
	LC_SYS_DEBUG_ENABLE = state;
}

void sys_remove_folder(char name[])
{
	
}

void sys_msg_queu_pool_initial(void)
{
	if(mMessageMemPool==NULL)
	{
		int length = LC_ALIGN_2K(sizeof(LC_DVR_RECORD_MESSAGE)*LC_MESSAGE_QUEUE_NUM + (LC_2K_LENGTH-1));
		sem_init(&sem, 0, 0);
		//-----------------------------------------------------------
		pthread_mutex_init(&mMsgMutex, NULL);
		mMessageMemPool = (char*)malloc(length+LC_MEM_QUEUE_LENGTH);
		mMemBufPool = mMessageMemPool+length;
		mMemCachePosition = 0;
		mMsgHead = 0;
		mMsgTail = 0;	
	}
}

void sys_msg_queu_pool_uninitial(void)
{
	free(mMessageMemPool);
	sem_destroy(&sem);	
	mMessageMemPool = NULL;
}

void* sys_msg_get_memcache(int size)
{
	int len = (size+LC_MEM_CACHE_BLOCK_SIZE-1)&(~(LC_MEM_CACHE_BLOCK_SIZE-1));
	void* retMem;
	pthread_mutex_lock(&mMsgMutex);
	if((mMemCachePosition+len)>LC_MEM_QUEUE_LENGTH) mMemCachePosition = 0;
	retMem = (mMemBufPool+mMemCachePosition);
	// logi_lc("memPool[%d]-->%d",mMemCachePosition, len);
	mMemCachePosition += len;
	pthread_mutex_unlock(&mMsgMutex);
	return retMem;
}

int sys_msg_queu_in(LC_DVR_RECORD_MESSAGE *msg)
{
	if(mMessageMemPool == NULL) return -1;
	pthread_mutex_lock(&mMsgMutex);
	int index = mMsgTail&(LC_MESSAGE_QUEUE_NUM-1);
	memcpy(mMessageMemPool+index*sizeof(LC_DVR_RECORD_MESSAGE), msg, sizeof(LC_DVR_RECORD_MESSAGE));
	if(LC_SYS_DEBUG_ENABLE)
	{
		if(!((msg->event==0)||(msg->event==3)||(msg->event==7)))
		{
			if(!((msg->event==15)&&(msg->value==2)))
				logd_lc("%d,%d<%d\n", msg->event, msg->value, index);
		}		
	}	
	mMsgTail++;	
	pthread_mutex_unlock(&mMsgMutex);
	//-----------------------------------------
	sem_post(&sem);		
	return 0;
}

LC_DVR_RECORD_MESSAGE* sys_msg_queu_out(void)
{	
	LC_DVR_RECORD_MESSAGE *msgQueu = (LC_DVR_RECORD_MESSAGE*)mMessageMemPool;
	if(msgQueu==NULL) return NULL;
	sem_wait(&sem);
	pthread_mutex_lock(&mMsgMutex);
	//-----------------------------------------	
	if(mMsgTail == mMsgHead) return NULL;	
	int idx = mMsgHead&(LC_MESSAGE_QUEUE_NUM-1);
	if(LC_SYS_DEBUG_ENABLE)
	{
		if(!((msgQueu[idx].event==0)||(msgQueu[idx].event==3)||(msgQueu[idx].event==7)))
		{
			if(!((msgQueu[idx].event==15)&&(msgQueu[idx].value==2)))
				logd_lc("%d,%d>%d\n", msgQueu[idx].event, msgQueu[idx].value, idx);
		}	
	}
	mMsgHead++;
	pthread_mutex_unlock(&mMsgMutex);
	return &msgQueu[idx];	
}

int  sys_msg_queu_remain(void)
{
	int remain = mMsgTail - mMsgHead;
	if(remain<0) remain+=LC_MESSAGE_QUEUE_NUM;
	return remain;
}

void sys_set_rtc_time(uint64_t *time)
{
	
}

void sys_get_rtc_time(char time_chr[])
{
}

void vc_message_send(int eMsg, int val, void* para, void* func)
{
	LC_DVR_RECORD_MESSAGE message;
	message.ackFunc = NULL;
	message.msg_id  = MSG_BUILD(LC_DVR_SYS_SERID, LC_DVR_VIDEO_SERID);
	message.event = eMsg;
	message.value = val;
	message.msg_ptr = para;
	message.ackFunc = (LC_ACK_FUNC)func;
	sys_msg_queu_in(&message);
}
