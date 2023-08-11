#ifndef __SIF_ISP_VO_SAMPLE__H_
#define __SIF_ISP_VO_SAMPLE__H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include <errno.h>
#include <unistd.h>

#include "vim_type.h"

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
		
#define CHECK_VI_RET_RETURN(ret) \
    do {\
		VIM_S32 s32RetRet = ret;\
        if (s32RetRet != VIM_SUCCESS)\
        {\
            ERR_VI("main run error %d.\n", s32RetRet); \
            return s32RetRet; \
        } \
    } while (0)	
	
void* mdvr_thread_handle(void *arg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif

