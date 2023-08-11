#ifndef __MAIN__H_
#define __MAIN__H_

#include <errno.h>
#include <unistd.h>

#include "vim_type.h"

#include "mpi_vpss.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef enum REGIN_TEST_EFFECT_E {
	E_SHIFT_NONE,
	E_SHIFT_LEFT,
	E_SHIFT_RIGHT,
	E_SHIFT_TOP,
	E_SHIFT_BOTTOM,
	E_SHIFT_BUTT,
}E_REGIN_TEST_EFFECT;

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#undef SAMPLE_PRT
#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d: ", __func__, __LINE__);	\
		printf(fmt);									\
    } while (0)

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
		printf("[ERR] function: %s line:%d" fmt, __func__, __LINE__, ##__VA_ARGS__)

#define CHECK_VPSS_RET_RETURN(ret) \
    do {\
		VIM_S32 s32RetRet = ret;\
        if (s32RetRet != VIM_SUCCESS)\
        {\
            ERR_VI("main run error %x.\n", s32RetRet); \
            return s32RetRet; \
        } \
    } while (0)

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#undef SAMPLE_PRT
#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d: ", __func__, __LINE__);	\
		printf(fmt);									\
    } while (0)

#define SAMPLE_VPSS_NULL_PTR(ptr) \
    do {\
        if (NULL == (ptr))\
        {\
            printf("PTR('%s') is NULL.\n", # ptr); \
            return -1; \
        } \
    } while (0)

typedef void *THREAD_HANDLE_S;

#define THREAD_INVALID_HANDLE   ((THREAD_HANDLE_S)-1)

typedef int (*FN_THREAD_LOOP)(void *pUserData);

typedef enum {
    TEST_THREAD_INITIALIZED         = 1 << 1, // Recorder has been initialized.
    TEST_THREAD_RUNNING             = 1 << 4, // Recording is in progress.
}TEST_THREAD_STATE_E;

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


typedef struct _TEST_RESULT
{
	int sore;
}TEST_RESULT_S;

typedef struct _TEST_THREAD_CTX_S
{
    pthread_t       		thread;
    pthread_mutex_t 		mutex;
    VIM_BOOL         		bRun;
    VIM_U32        			u32PrintNum;
	TEST_THREAD_STATE_E    	enState;
    char 					*pThreadName;
	TEST_RESULT_S 			*pstResult;

    FN_THREAD_LOOP 			entryFunc;

    void      				*pUserData;
    const char              *s8FileName;
	VIM_U8 					u8Dynamic;
}TEST_THREAD_CTX_S;


typedef struct __SAMPLE_MAN_INFO{
	VIM_U8      bSaveSingleFile;
	VIM_U8      bSaveFile;
	VIM_U8      u8DebugMask;
	const char  *strSaveFileFolder;
	const char  *videosrcFile;
	char        *strIniName;
	VIM_U32     u32OsdOpen;
	VIM_U32     u32OsdColor;
	VIM_S32     bEnableInputFrameNo;
	VIM_U64     s32StartInputFrameNo[MAX_VPSS_VIDEO_POINT];
	VIM_U32     u32FpsDelayMs;
	VIM_U32     u32ChnOutMask[MAX_VPSS_GROUP];
	VIM_U32     u32GrpMask;
	VIM_U32     u32DynamicPlay;
	VIM_U32     u32SrcMod;
	VIM_U32     u32DstMod[MAX_VPSS_GROUP][6];
}SAMPLE_MAN_INFO_S;


VIM_S32 SAMPLE_COMMON_VPSS_StartGroup(VIM_U32 u32Group, VIM_VPSS_GROUP_ATTR_S *pStGrpAttr);

VIM_S32 SAMPLE_COMMON_VPSS_StopGroup(VIM_U32 u32Group);

VIM_S32 SAMPLE_COMMON_VPSS_StartChn(VIM_U32 u32Group, VIM_U32 u32ChnId, VIM_CHN_CFG_ATTR_S *pStChnAttr);

VIM_S32 SAMPLE_COMMON_VPSS_StopChn(VIM_U32 u32Group, VIM_U32 u32ChnId);

VIM_S32 SAMPLE_COMMON_VPSS_Send(VIM_U32 u32Group, VIM_U32 u32PrintNum, const char *s8FileName, VIM_U8 u8Dynamic);

VIM_S32 SAMPLE_COMMON_VPSS_Capture(VIM_U32 u32Group, VIM_U32 u32ChnId,
                            VIM_U32 u32CaptureNum, const char *s8SavePush, VIM_U8 u8Dynamic);

VIM_S32 SAMPLE_COMMON_VPSS_SetInUserBuff(VIM_U32 u32Group, VIM_STREAM_DMA_BUFFER *pstUserBuff,  VIM_U32 u32BuffOfset);

VIM_S32 SAMPLE_COMMON_VPSS_SetOutUserBuff(VIM_U32 u32Group, VIM_U32 u32ChnId,
                            VIM_STREAM_DMA_BUFFER *pstUserBuff,  VIM_U32 u32BuffOfset);

VIM_S32 SAMPLE_VPSS_SetOutUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_CHN_CFG_ATTR_S *stChnAttr, VIM_U32 u32ChnId);

VIM_S32 SAMPLE_VPSS_SetInUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_VPSS_GROUP_ATTR_S *vpssGrpAttr);

VIM_S32 SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(VIM_U8 u8Plane,
									VIM_IMG_FORMAT_E  enInputFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U16 u16Width,
									VIM_U16 u16Height);

VIM_S32 SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneCframeSizeMain(
									VIM_U8  u8Plane,
									VIM_IMG_FORMAT_E  enFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U8  u8BitDepth,
									VIM_U8  u8BitDepthu,
									VIM_BOOL enLoss,
									VIM_U16 u16Width,
									VIM_U16 u16Height);

VIM_S32 SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(
                                    PIXEL_FORMAT_E enPixfmt,
                                    VIM_IMG_FORMAT_E *enFormat,
                                    VIM_IMG_BITS_E *enBits);

VIM_S32 SAMPLE_COMMON_VPSS_VpssFmtToPixFmt(
                                    VIM_IMG_FORMAT_E enFormat,
                                    VIM_IMG_BITS_E enBits,
                                    PIXEL_FORMAT_E *enPixfmt);

VIM_S32 SAMPLE_VPSS_FreedOutContinuousBuff(
                                    VIM_CHN_CFG_ATTR_S *stChnAttr,
                                    VIM_U32 u32ChnId,
                                    VIM_U32 u32Index);
VIM_S32 SAMPLE_COMMON_VPSS_Capture_ContinuousFrm(
									VIM_U32 u32Group, 
									VIM_U32 u32ChnId,
									VIM_U32 u32CaptureNum, 
									VIM_S8 *s8SavePush, 
									VIM_U8 u8Dynamic);

typedef struct __VI_CHN_REALINFO{
	double dFPSPerSecond;
	VIM_U64 u64DealFrames;
	VIM_U64 u64DealBytes;
}VI_CHN_REALINFO_S;

typedef struct __USER_GROUP_LINK_TOOLS_GROUP{
	VIM_U64 user_group;
	VIM_U64 tools_group;
}USER_DATA_LINK_TOOLS_DATA;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



#endif

