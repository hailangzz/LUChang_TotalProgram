#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "sample_comm_venc_ext.h"

pthread_mutex_t VENC_MEM_LOCK;


#define buf_free(p)                         \
    do {                                       \
        if(p) {                                \
            free(p);                           \
            p = NULL;                          \
        }                                      \
    } while(0)


#define CheckInvalidParam(trueIsInvalid, ...) \
        do { \
            if ((trueIsInvalid)) { \
                printf("[error]===== invalid param! <%s> =====", # trueIsInvalid); \
                return __VA_ARGS__;} \
        } while(0)


int ringBufferAlloc(RING_BUFFER_T * pRing,int size)
{
    CheckInvalidParam(pRing == NULL, -1);
    CheckInvalidParam(size < 1024, -1);
    pRing->bufferLenght = size;
    pRing->extBufLength = ((size>>2) > (500*1024)) ? (500*1024) : (size>>2);
    pRing->pBuffer = (char*)malloc(pRing->bufferLenght + pRing->extBufLength + 16);
    CheckInvalidParam(pRing->pBuffer == NULL, -2);
    pthread_mutex_init(&(pRing->mutex),NULL);
    pRing->pRead = pRing->pBuffer;
    pRing->pWrite = pRing->pBuffer;
    pRing->pLast = pRing->pBuffer + size;
    pRing->lastRead = 0;
    pRing->dataLenght = 0;
    return 0;
}

int ringBufferFree(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL, -1);
    pthread_mutex_lock(&(pRing->mutex));
    buf_free(pRing->pBuffer);
    pRing->pRead = NULL;
    pRing->pWrite = NULL;
    pRing->bufferLenght = 0;
    pRing->lastRead = 0;
    pRing->dataLenght = 0;
    pthread_mutex_destroy(&(pRing->mutex));
    return 0;
}

char *ringBufferGetWPtr(RING_BUFFER_T *pRing ,int *len)
{
    CheckInvalidParam(pRing == NULL, NULL);
    pthread_mutex_lock(&(pRing->mutex));
    if(pRing->dataLenght < pRing->bufferLenght) {
        int validLen;
        if(pRing->pWrite >= pRing->pRead){
            validLen = pRing->bufferLenght - (pRing->pWrite - pRing->pBuffer);
        }
        else{
            validLen = pRing->pRead - pRing->pWrite;
        }
        *len = (*len < validLen) ? *len : validLen;
        pthread_mutex_unlock(&(pRing->mutex));
        return pRing->pWrite;
    } else {
        *len = 0;
        pthread_mutex_unlock(&(pRing->mutex));
        return NULL;
    }
}

int ringBufferSetWPtr(RING_BUFFER_T *pRing ,int len)
{
    CheckInvalidParam(pRing == NULL, -1);
    pthread_mutex_lock(&(pRing->mutex));
    if((pRing->pWrite + len) >= pRing->pLast){
        pRing->pWrite = pRing->pBuffer;
    }
    else{
        pRing->pWrite += len;
    }
    pRing->dataLenght += len;
    pthread_mutex_unlock(&(pRing->mutex));
    return 0;
}
char *ringBufferGetRPtr(RING_BUFFER_T *pRing,int *len)
{
    CheckInvalidParam(pRing == NULL, NULL);
    pthread_mutex_lock(&(pRing->mutex));
    if(pRing->dataLenght > 0) {
        if(pRing->pRead >= pRing->pWrite) {
            if (*len < (pRing->pLast - pRing->pRead)) {
            } else {
                const int dataLen = (pRing->pLast - pRing->pRead) + (pRing->pWrite - pRing->pBuffer);
                const int maxReadLen = (pRing->pLast - pRing->pRead) + pRing->extBufLength;
                int readLen = (maxReadLen <= dataLen) ? maxReadLen : dataLen;
                readLen = (readLen <= *len) ? readLen : *len;
                *len = readLen;
                const int moveLen = readLen - (pRing->pLast - pRing->pRead);
                memcpy(pRing->pLast, pRing->pBuffer, moveLen);
            }
        } else {
            const int dataLen = pRing->pWrite - pRing->pRead;
            *len = (*len < dataLen) ? *len : dataLen;
        }
        pRing->lastRead = *len;
        char *pReadBuf = pRing->pRead;
        pthread_mutex_unlock(&(pRing->mutex));
        return pReadBuf;
    } else {
        *len = 0;
        pthread_mutex_unlock(&(pRing->mutex));
        return NULL;
    }
}

void ringBufferSetRPtr(RING_BUFFER_T *pRing)
{
    CheckInvalidParam(pRing == NULL);
    pthread_mutex_lock(&(pRing->mutex));
    if((pRing->pRead + pRing->lastRead) >= pRing->pLast) {
        pRing->pRead = pRing->pBuffer + ((pRing->pRead + pRing->lastRead) - pRing->pLast);
    } else {
        pRing->pRead += pRing->lastRead;
    }
    pRing->dataLenght -= pRing->lastRead;
    pRing->lastRead = 0;
    pthread_mutex_unlock(&(pRing->mutex));
}

void ringBufferReset(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL);
    pthread_mutex_lock(&(pRing->mutex));
    pRing->pRead = pRing->pBuffer;
    pRing->pWrite = pRing->pBuffer;
    pRing->dataLenght=0;
    pRing->lastRead = 0;
    pthread_mutex_unlock(&(pRing->mutex));
}

int ringBufferGetLenght(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL, 0);
    return pRing->dataLenght;
}

int ringBufferGetBufSize(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL, 0);
    return pRing->bufferLenght;
}

int ringBufferGetWrAvailSize(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL, 0);
    return pRing->bufferLenght - pRing->dataLenght;
}

int ringBufferGetRdAvailSize(RING_BUFFER_T * pRing)
{
    CheckInvalidParam(pRing == NULL, 0);
    return pRing->dataLenght;
}


VIM_S32 Sem_Init(sem_t *sem, int pshared, unsigned int value)
{
	sem_init(sem,pshared,value);
	return VIM_SUCCESS;
}


VIM_S32 Sem_Destory(sem_t *sem)
{
	sem_destroy(sem);
	return VIM_SUCCESS;
}


VIM_S32 Sem_Wait(sem_t *sem)
{
	sem_wait(sem);

	return VIM_SUCCESS;
}


VIM_S32 Sem_Post(sem_t *sem)
{
	sem_post(sem);
	return VIM_SUCCESS;
}


int InitVencMemLock(void)
{
	if(pthread_mutex_init(&VENC_MEM_LOCK, NULL) != 0)
	{
		printf("venc sample pthread_mutex_init  failed!\n");
		return -1;
	}
    return 0;
}


int DestroyVencMemLock(void)
{  
	pthread_mutex_destroy(&VENC_MEM_LOCK);
    return 0;
}


int EnterVencMemLock(void)
{  
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	pthread_mutex_lock(&VENC_MEM_LOCK);
    return 0;
}

int LeaveVencMemLock(void)
{
	pthread_mutex_unlock(&VENC_MEM_LOCK);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    return 0;
}




