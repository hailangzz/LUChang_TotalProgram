#ifndef _VC0768_LOG_PRT_H_
#define _VC0768_LOG_PRT_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include <stdio.h>

#ifndef SAMPLE_PRT
#define SAMPLE_PRT(fmt...)                                      \
         do {                                                   \
             printf("[%s] - %d: ", __FUNCTION__, __LINE__);     \
             printf(fmt);                                       \
            }while(0)
#endif


#define SAMPLE_ERROR(fmt...)    \
    do {                        \
        printf( "[ERROR]-" );   \
        SAMPLE_PRT( fmt );      \
    }while( 0 )


#define SAMPLE_VPSS_NULL_PTR(ptr) \
    do {\
        if (NULL == (ptr))\
        {\
            printf("PTR('%s') is NULL.\n", # ptr); \
            return -1; \
        } \
    } while (0)



#define ASSERT_RET_PRT(RET) \
    {\
        if (0 != RET) \
        {\
            SAMPLE_PRT("s32Ret=0x%08x\n", RET); \
        }\
    }
    
    
#define ASSERT_RET_RETURN(RET) \
    do {\
        if (0 != RET) \
        {\
            SAMPLE_PRT("ret = %x\n", RET); \
            return RET;\
        }\
    }while(0)
    
#define ASSERT_RET_GOTO(RET,LABEL) \
        {\
            if (RET != 0) \
            {\
                SAMPLE_PRT("RUN ERR and GOTO %s s32Ret=0x%08x\n", #LABEL, RET); \
                goto LABEL;\
            }\
        }

#define CHECK_RET(express)\
        do {\
            VIM_S32 s32Ret;\
            s32Ret = express;\
            if (VIM_SUCCESS != s32Ret)\
            {\
                printf("Failed at %s: LINE: %d with %#x!\n", __FUNCTION__, __LINE__, s32Ret);\
            }\
        }while(0)


#define CHECK_CHN_RET(express,Chn,name)\
    do{\
        VIM_S32 Ret;\
        Ret = express;\
        if (VIM_SUCCESS != Ret)\
        {\
            printf("\033[0;31m%s chn %d failed at %s: LINE: %d with %#x!\033[0;39m\n", name, Chn, __FUNCTION__, __LINE__, Ret);\
            fflush(stdout);\
            return Ret;\
        }\
    }while(0)
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif
