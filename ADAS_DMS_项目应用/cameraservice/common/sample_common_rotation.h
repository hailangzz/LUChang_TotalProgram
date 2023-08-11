#ifndef __ROTATION_COMMON__H_
#define __ROTATION_COMMON__H_

#include <errno.h>
#include <unistd.h>

#include "vim_type.h"

#include "mpi_rotation.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d: ", __FUNCTION__, __LINE__);	\
		printf(fmt);									\
	   }while(0)

#define ASSERT_RET_RETURN(RET) \
	{\
		if (RET != 0) \
		{\
			SAMPLE_PRT("RUN ERR s32Ret=0x%08x\n", RET); \
			return RET;\
		}\
	}

#define ASSERT_RET_GOTO(RET,LABEL) \
	{\
		if (RET != 0) \
		{\
			SAMPLE_PRT("RUN ERR and GOTO %s s32Ret=0x%08x\n", #LABEL, RET); \
			goto LABEL;\
		}\
	}

#define ASSERT_RET_PRT(RET) \
	{\
		if (RET != 0) \
		{\
			SAMPLE_PRT("s32Ret=0x%08x\n", RET); \
		}\
	}
	
#define SIMPLE_PAUSE(...) ({ \
    int ch1, ch2; \
    printf(__VA_ARGS__); \
    ch1 = getchar(); \
    if (ch1 != '\n' && ch1 != EOF) { \
        while((ch2 = getchar()) != '\n' && (ch2 != EOF)); \
    } \
    ch1; \
})

#define ERR_VI(fmt, ...) \
		printf("[ERR] ""function: %s line:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
		
#define CHECK_ROTATION_RET_RETURN(ret) \
    do {\
		VIM_S32 s32RetRet = ret;\
        if (s32RetRet != VIM_SUCCESS)\
        {\
            ERR_VI("main run error %x.\n", s32RetRet); \
            return s32RetRet; \
        } \
    } while (0)	
		
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d: ", __FUNCTION__, __LINE__);	\
		printf(fmt);									\
	   }while(0)
		   
typedef void* THREAD_HANDLE_S;

#define THREAD_INVALID_HANDLE   ((THREAD_HANDLE_S)-1)

typedef int (*FN_THREAD_LOOP)(void *pUserData);

typedef enum {
    ROT_TEST_THREAD_INITIALIZED           = 1 << 1, // Recorder has been initialized.
    ROT_TEST_THREAD_RUNNING             = 1 << 4, // Recording is in progress.
}ROT_TEST_THREAD_STATE_E;

typedef struct _ROT_TEST_RESULT
{
	int sore;
}ROT_TEST_RESULT_S;

typedef struct _ROT_TEST_THREAD_CTX_S
{
    pthread_t       		thread;
    pthread_mutex_t 		mutex;
    VIM_BOOL         		bRun;
    VIM_U32        			u32PrintNum;
	ROT_TEST_THREAD_STATE_E    	enState;
    char 					*pThreadName;
	ROT_TEST_RESULT_S 			*pstResult;

    FN_THREAD_LOOP 			entryFunc;
    
    void      				*pUserData;
    VIM_S8*    				s8FileName;
	VIM_U8 					u8Dynamic;
}ROT_TEST_THREAD_CTX_S;



typedef enum __VIM_IMG_FORMAT_E{
	E_IMG_FORMAT_422SP,
	E_IMG_FORMAT_420SP,
	E_IMG_FORMAT_422P,
	E_IMG_FORMAT_420P,
	E_IMG_FORMAT_601P,
	E_IMG_FORMAT_709P,
	E_IMG_FORMAT_422I,
	E_IMG_FORMAT_BUTT,
}VIM_IMG_FORMAT_E;

typedef enum __VIM_INPUT_BITS_E{
	E_IMG_BITS_8,
	E_IMG_BITS_10,
	E_IMG_BITS_BUTT,
}VIM_IMG_BITS_E;


typedef struct __ROT_SAMPLE_MAN_INFO{
	VIM_U8 bSaveFile;
	VIM_U8 u8DebugMask;
	VIM_S8 *strSaveFileFolder;
	VIM_S8 *videosrcFile;
	VIM_S8 *strIniName;
	VIM_U32 u32HeapSize;
	VIM_U32 u32HeapBase;
	VIM_U32 u32DynamicPlay;
	VIM_U32 u32FpsDelayMs;
	VIM_U32 u32ChnMax;
	VIM_U32 u32Vpssbinder[2];
	VIM_U32 u32VoShow[2];
}ROT_SAMPLE_MAN_INFO_S;

VIM_S32 SAMPLE_COMMON_ROTATION_StartChn(VIM_U32 u32ChnId, VIM_ROFORMAT_S* pStChnAttr);

VIM_S32 SAMPLE_COMMON_ROTATION_StopChn(VIM_U32 u32ChnId);

VIM_S32 SAMPLE_COMMON_ROTATION_Send(VIM_U32 u32ChnID, VIM_U32 u32PrintNum, VIM_S8* s8FileName);

VIM_S32 SAMPLE_COMMON_ROTATION_Capture(VIM_U32 u32ChnId, VIM_U32 u32CaptureNum, VIM_S8* s8SavePush);

VIM_S32 SAMPLE_COMMON_ROTATION_IPCCapture( VIM_U32 u32ChnId, VIM_U32 u32CaptureNum, VIM_S8* s8SavePush);

VIM_S32 SAMPLE_COMMON_ROTATION_StopCapture(void);

VIM_S32 SAMPLE_COMMON_ROTATION_SetInUserBuff(VIM_U32 u32ChnId, VIM_STREAM_DMA_BUFFER *pstUserBuff,  VIM_U32 u32BuffOfset);

VIM_S32 SAMPLE_COMMON_ROTATION_SetOutUserBuff(VIM_U32 u32ChnId, VIM_STREAM_DMA_BUFFER *pstUserBuff,  VIM_U32 u32BuffOfset);

VIM_S32 SAMPLE_ROTATION_SetOutUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_ROFORMAT_S *RotAttr,VIM_U32 u32Chn);

VIM_S32 SAMPLE_ROTATION_SetInUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_ROFORMAT_S *RotAttr,VIM_U32 u32Chn);

VIM_S32 SAMPLE_COMMON_ROTATION_CalcFrameSinglePlaneSize(VIM_U8 u8Plane, 
									VIM_IMG_FORMAT_E  enInputFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U16 u16Width,
									VIM_U16 u16Height);

VIM_S32 SAMPLE_COMMON_ROTATION_CalcFrameSinglePlaneCframeSizeMain(
									VIM_U8  u8Plane,
									VIM_IMG_FORMAT_E  enFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U8  u8BitDepth,
									VIM_U8  u8BitDepthu,
									VIM_BOOL enLoss,
									VIM_U16 u16Width,
									VIM_U16 u16Height);



typedef struct __ROT_VI_CHN_REALINFO{
	double dFPSPerSecond;
	VIM_U64 u64DealFrames;
	VIM_U64 u64DealBytes;
}ROT_VI_CHN_REALINFO_S;

typedef struct __ROT_USER_GROUP_LINK_TOOLS_GROUP{
	VIM_U64 user_group;
	VIM_U64 tools_group;
}ROT_USER_DATA_LINK_TOOLS_DATA;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif

