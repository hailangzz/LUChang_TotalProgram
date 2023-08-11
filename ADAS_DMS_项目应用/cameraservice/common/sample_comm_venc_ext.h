#ifndef __SAMPLE_COMM_VENC_EXT_H__
#define __SAMPLE_COMM_VENC_EXT_H__

#include <pthread.h>
#include <semaphore.h>

typedef struct ring_buffer_t
{
    char *pBuffer;			//buf first addt
    char *pRead;			//buf read addr
    char *pWrite;			//buf write addr
    char *pLast;			//buf end addr
    int lastRead;			//
    int bufferLenght;		//buf length
    int extBufLength;   	//ext buf length
    int dataLenght;   		//valid data length
    pthread_mutex_t mutex;
    pthread_mutex_t rMutex;
    pthread_mutex_t wMutex;
} RING_BUFFER_T;

int ringBufferAlloc(RING_BUFFER_T * pRing, int size);
int ringBufferFree(RING_BUFFER_T * pRing);
char *ringBufferGetWPtr(RING_BUFFER_T * buffer, int *len);
int ringBufferSetWPtr(RING_BUFFER_T * pRing, int len);
char *ringBufferGetRPtr(RING_BUFFER_T * pRing, int *len);
void ringBufferSetRPtr(RING_BUFFER_T * pRing);
int ringBufferGetLenght(RING_BUFFER_T * pRing);
void ringBufferReset(RING_BUFFER_T * pRing);
int ringBufferGetWrAvailSize(RING_BUFFER_T * pRing);
int ringBufferGetRdAvailSize(RING_BUFFER_T * pRing);
int ringBufferGetBufSize(RING_BUFFER_T * pRing);

int InitVencMemLock(void);
int DestroyVencMemLock(void);
int EnterVencMemLock(void);
int LeaveVencMemLock(void);


#endif
