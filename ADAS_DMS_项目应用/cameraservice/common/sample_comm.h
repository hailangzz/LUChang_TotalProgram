/******************************************************************************
  Hisilicon Hi35xx sample programs head file.

  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-2 Created
******************************************************************************/

#ifndef __SAMPLE_COMM_H__
#define __SAMPLE_COMM_H__

#include "vim_common.h"
#include "vim_comm_sys.h"
#include "vim_comm_vb.h"
#include "vim_comm_venc.h"

#include "vim_defines.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "sample_venc_cfgParser.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/*******************************************************
    macro define
*******************************************************/
#define FILE_NAME_LEN               128

#define ALIGN_BACK(x, a)              ((a) * (((x) / (a))))
#define SAMPLE_SYS_ALIGN_WIDTH      64
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

#define CHECK_RET(express,name)\
    do{\
        VIM_S32 Ret;\
        Ret = express;\
        if (VIM_SUCCESS != Ret)\
        {\
            printf("\033[0;31m%s failed at %s: LINE: %d with %#x!\033[0;39m\n", name, __FUNCTION__, __LINE__, Ret);\
            return Ret;\
        }\
    }while(0)
#define SAMPLE_PIXEL_FORMAT         PIXEL_FORMAT_YUV_SEMIPLANAR_420

#define TW2865_FILE "/dev/tw2865dev"
#define TW2960_FILE "/dev/tw2960dev"
#define TLV320_FILE "/dev/tlv320aic31"


/*** for init global parameter ***/
#define SAMPLE_ENABLE 		    1
#define SAMPLE_DISABLE 		    0
#define SAMPLE_NOUSE 		    -1

#define SAMPLE_AUDIO_AI_DEV 0
#define SAMPLE_AUDIO_AO_DEV 0
#define SAMPLE_AUDIO_PTNUMPERFRM   320

#define VI_MST_NOTPASS_WITH_VALUE_RETURN(s32TempRet) \
    do{\
        NOT_PASS(s32TempRet);\
        VIMST_ExitMpp();\
        return s32TempRet;\
    }while(0)

#define VI_PAUSE()  do {\
        printf("---------------press any key to exit!---------------\n");\
        getchar();\
    } while (0)

/*
#define SAMPLE_PRT(fmt...)   \
    do {\
        printf("[%s]-%d: ", __FUNCTION__, __LINE__);\
        printf(fmt);\
    }while(0)
*/

#define CHECK_NULL_PTR(ptr)\
    do{\
        if(NULL == ptr)\
        {\
            printf("func:%s,line:%d, NULL pointer\n",__FUNCTION__,__LINE__);\
            return VIM_FAILURE;\
        }\
    }while(0)


/*******************************************************
    enum define
*******************************************************/

typedef enum sample_ispcfg_opt_e
{
    CFG_OPT_NONE    = 0,
    CFG_OPT_SAVE    = 1,
    CFG_OPT_LOAD    = 2,
    CFG_OPT_BUTT
} SAMPLE_CFG_OPT_E;


typedef enum
{
    VI_DEV_BT656_D1_1MUX = 0,
    VI_DEV_BT656_D1_4MUX,
    VI_DEV_BT656_960H_1MUX,
    VI_DEV_BT656_960H_4MUX,
    VI_DEV_720P_HD_1MUX,
    VI_DEV_1080P_HD_1MUX,
    VI_DEV_BUTT
} SAMPLE_VI_DEV_TYPE_E;

typedef enum sample_vi_chn_set_e
{
    VI_CHN_SET_NORMAL = 0,      /* mirror, flip close */
    VI_CHN_SET_MIRROR,          /* open MIRROR */
//    VI_CHN_SET_FLIP,            /* open filp */
  //  VI_CHN_SET_FLIP_MIRROR      /* mirror, flip */
} SAMPLE_VI_CHN_SET_E;

typedef enum sample_bind_e
{
    SAMPLE_BIND_VI_VO = 0,      /* VI bind to VO */
    SAMPLE_BIND_VPSS_VO,    /* VPSS bind to VO */
    SAMPLE_BIND_VI_VPSS_VO, /* VI bind to VPSS, VPSS bind to VO */
    SAMPLE_BIND_BUTT
} SAMPLE_BIND_E;

typedef enum
{
    SAMPLE_FRAMERATE_DEFAULT = 0,
    SAMPLE_FRAMERATE_15FPS   = 15,
    SAMPLE_FRAMERATE_20FPS   = 20,
    SAMPLE_FRAMERATE_30FPS   = 30,
    SAMPLE_FRAMERATE_60FPS   = 60,
    SAMPLE_FRAMERATE_120FPS  = 120,
    SAMPLE_FRAMERATE_BUTT,
} SAMPLE_FRAMERATE_E;

typedef enum sample_vo_mode_e
{
    VO_MODE_1MUX = 0,
    VO_MODE_2MUX = 1,
    VO_MODE_BUTT
} SAMPLE_VO_MODE_E;

typedef enum sample_rgn_change_type_e
{
    RGN_CHANGE_TYPE_FGALPHA = 0,
    RGN_CHANGE_TYPE_BGALPHA,
    RGN_CHANGE_TYPE_LAYER
} SAMPLE_RGN_CHANGE_TYPE_EN;

typedef enum sample_sensor_num_e
{
    SAMPLE_SENSOR_SINGLE = 0,
    SAMPLE_SENSOR_DOUBLE   = 1,
    SAMPLE_SENSOR_BUT  
} SAMPLE_SENSOR_NUM_E;

/*******************************************************
    structure define
*******************************************************/
typedef struct sample_vi_param_s
{
    VIM_S32 s32ViDevCnt;         // VI Dev Total Count
    VIM_S32 s32ViDevInterval;    // Vi Dev Interval
    VIM_S32 s32ViChnCnt;         // Vi Chn Total Count
    VIM_S32 s32ViChnInterval;    // VI Chn Interval
} SAMPLE_VI_PARAM_S;

typedef struct sample_video_loss_s
{
    VIM_BOOL bStart;
    pthread_t Pid;
//    VI_TYPE_E enViMode;
} SAMPLE_VIDEO_LOSS_S;


typedef struct sample_vi_frame_info_s
{
    VB_BLK VbBlk;
    VIDEO_FRAME_INFO_S stVideoFrame;
    VIM_U32 u32FrmSize;
} SAMPLE_VI_FRAME_INFO_S;


typedef struct sample_vi_config_s
{
//    VI_TYPE_E    enViMode;
    VIDEO_NORM_E        enNorm;           /*DC: VIDEO_ENCODING_MODE_AUTO */
    ROTATE_E            enRotate;
    SAMPLE_VI_CHN_SET_E enViChnSet;
    WDR_MODE_E          enWDRMode;
    SAMPLE_SENSOR_NUM_E enSnsNum;

    PIXEL_FORMAT_E      enPixFmt;
    PIC_SIZE_E      enPixSize;
    VIM_U8              u8FrmRate;
    SAMPLE_FRAMERATE_E enFrmRate;
	VB_POOL poolid;
    VIM_BOOL b3DNR;
//    VO_INTF_SYNC_E enVoOutInf;
    VIM_U8 enVoOverlay;
    VIM_U8 u8RegionChn;
    VIM_U8 u8VoI2C;
    VIM_U8 u8SnapDev;
	int enRTMode;
} SAMPLE_VI_CONFIG_S;

#define VENC_CHAN_MAX	16
typedef struct 
{
	SAMPLE_ENC_CFG pEncCfg[VENC_CHAN_MAX];
	VIM_U8 dynamic_enable;
	VIM_CHAR venc_cfgfilename[256];
	pthread_t thread_id[VENC_CHAN_MAX];
} SAMPLE_COMM_CONFIG_S;



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* End of #ifndef __SAMPLE_COMMON_H__ */
