/******************************************************************************
  Vimicro Vcxx sample programs head file.

  Copyright (C), 2018-2030, Vimicro Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2018-2 Created
******************************************************************************/

#ifndef __SAMPLE_COMM_VENC_H__
#define __SAMPLE_COMM_VENC_H__


#include <semaphore.h>

#include "vim_common.h"
#include "vim_comm_sys.h"
#include "vim_comm_vb.h"
#include "vim_comm_venc.h"
#include "vim_defines.h"

#include "mpi_sys.h"
#include "mpi_vb.h"
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
#define SAMPLE_VENC_PRT(fmt...)   							\
				do {												\
						printf("[%s]-%d: ", __FUNCTION__, __LINE__);	\
						printf(fmt);									\
					}while(0)

#define	SAMPLE_VENC_WARN(fmt...)   								\
				do {													\
						printf("\033[36m/[sample WARN] \033[0m:[%s] - %d: ", __FUNCTION__, __LINE__);	\
						printf(fmt);										\
					   }while(0)


#define	SAMPLE_VENC_ERR(fmt...)   								\
				do {													\
					printf("\033[36m[sample ERROR] \033[0m:[%s] - %d: ", __FUNCTION__, __LINE__);	\
					printf(fmt);										\
				   }while(0)
		



#define VENC_ALIGN(_data, _align)     (((_data)+(_align-1))&~(_align-1))

/*** for init global parameter ***/


/*******************************************************
    enum define
*******************************************************/
/**
 * @brief	This is an enumeration type for representing chroma formats of the frame buffer and pixel formats in packed mode.
 */ 
 typedef enum {
	FORMAT_ERR			 = -1,
	FORMAT_420			 = 0 ,	  /* 8bit */
	FORMAT_422				 ,	  /* 8bit */
	FORMAT_224				 ,	  /* 8bit */
	FORMAT_444				 ,	  /* 8bit */
	FORMAT_400				 ,	  /* 8bit */

								  /* Little Endian Perspective	   */
								  /*	 | addr 0  | addr 1  |	   */
	FORMAT_420_P10_16BIT_MSB = 5, /* lsb | 00xxxxx |xxxxxxxx | msb */
	FORMAT_420_P10_16BIT_LSB ,	  /* lsb | xxxxxxx |xxxxxx00 | msb */
	FORMAT_420_P10_32BIT_MSB ,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
	FORMAT_420_P10_32BIT_LSB ,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

								  /* 4:2:2 packed format */
								  /* Little Endian Perspective	   */
								  /*	 | addr 0  | addr 1  |	   */
	FORMAT_422_P10_16BIT_MSB ,	  /* lsb | 00xxxxx |xxxxxxxx | msb */
	FORMAT_422_P10_16BIT_LSB ,	  /* lsb | xxxxxxx |xxxxxx00 | msb */
	FORMAT_422_P10_32BIT_MSB ,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
	FORMAT_422_P10_32BIT_LSB ,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */
	
	FORMAT_YUYV 			 ,	  /**< 8bit packed format : Y0U0Y1V0 Y2U1Y3V1 ... */
	FORMAT_YUYV_P10_16BIT_MSB,	  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
	FORMAT_YUYV_P10_16BIT_LSB,	  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
	FORMAT_YUYV_P10_32BIT_MSB,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
	FORMAT_YUYV_P10_32BIT_LSB,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
	
	FORMAT_YVYU 			 ,	  /**< 8bit packed format : Y0V0Y1U0 Y2V1Y3U1 ... */
	FORMAT_YVYU_P10_16BIT_MSB,	  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
	FORMAT_YVYU_P10_16BIT_LSB,	  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
	FORMAT_YVYU_P10_32BIT_MSB,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
	FORMAT_YVYU_P10_32BIT_LSB,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
	
	FORMAT_UYVY 			 ,	  /**< 8bit packed format : U0Y0V0Y1 U1Y2V1Y3 ... */
	FORMAT_UYVY_P10_16BIT_MSB,	  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
	FORMAT_UYVY_P10_16BIT_LSB,	  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
	FORMAT_UYVY_P10_32BIT_MSB,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
	FORMAT_UYVY_P10_32BIT_LSB,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
	
	FORMAT_VYUY 			 ,	  /**< 8bit packed format : V0Y0U0Y1 V1Y2U1Y3 ... */
	FORMAT_VYUY_P10_16BIT_MSB,	  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
	FORMAT_VYUY_P10_16BIT_LSB,	  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
	FORMAT_VYUY_P10_32BIT_MSB,	  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */
	FORMAT_VYUY_P10_32BIT_LSB,	  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */

	FORMAT_MAX,
} Venc_FrameBufferFormat;

 typedef enum {
	YUV_FORMAT_ERR			 = 0,
	YUV420P,
	YUV420P_P10_32bit,
	YUV420SP,
	YUV420SP_P10_32bit,
	YUV420P_P10_16bit,
	YUV_FORMAT_MAX,
} Venc_SrcBufferFormat;

enum {
    YUV_OUTPUT_PLANAR           = 0,
    YUV_OUTPUT_NV12             = 2,
    YUV_OUTPUT_NV21             = 3,
    YUV_OUTPUT_PACKED           = 4,
};


typedef enum sample_rc_e
{
    SAMPLE_RC_CBR = 0,
    SAMPLE_RC_VBR,
    SAMPLE_RC_AVBR,
    SAMPLE_RC_FIXQP
} SAMPLE_RC_E;


/*******************************************************
    structure define
*******************************************************/

typedef struct tagSYSTIME {
	int  year;
	int  month;
	int  day;
	int  wday;
	int  hour;
	int  minute;
	int  second;
	int  msec;
	int  isdst;
} VENC_SYSTIME;

typedef struct sample_venc_getstream_s
{
    VIM_BOOL bThreadStart;
    VIM_S32  s32Cnt;
    VIM_S32  s32Index;
	VIM_VOID *pEncCfg;
} SAMPLE_VENC_GETSTREAM_PARA_S;


typedef struct sample_venc_sendstream_s
{
	FILE* pFile;
	VIM_VOID *pEncCfg;
	VIM_CHAR *szFilePostfix;	
	sem_t sem;
} SAMPLE_VENC_SENDSTREAM_PARA_S;


typedef struct sample_venc_para
{
    VENC_CHN chn;
    PAYLOAD_TYPE_E enType;
    VIDEO_NORM_E enNorm;
    PIC_SIZE_E enSize;
    SAMPLE_RC_E enRcMode;
	VIM_U32  rate;
	VIM_U32  u32Profile;
	VIM_U32  u32Gop;
    VENC_GOP_ATTR_S *pstGopAttr;	
	VENC_EXT_PARAM_S svacEnt;
	VIM_U32 qp_value;
	VIM_U32 edsp_enable;
	VENC_SVC_PARAM_S pstSvcParam;
	VENC_ROI_CFG_S pstVencRoiCfg;
	VENC_EXTINFO_S	extinfo;
	VIM_U32 chn_count;
	VIM_U32 specialheader;
	VIM_U32 enable_nosavefile;
}SAMPLE_VENC_PARA;


typedef struct video_info{
	VIM_S8 mark[3];
	VIM_U8 len;
	VIM_U8 mode;
	VIM_U8 devId;
	VIM_U8 chnId;
	VIM_U8 chnCtrl;
	VIM_U16 fsn;
	VIM_U16 picW;
	VIM_U16 picH;
	VIM_U16 byteW;
	VIM_U16 byteH;
	VIM_U16 reserv;
	VIM_U32 rts;
	VIM_U32 dts;
	VIM_U32 pts;
	VIM_U16 crc;
} FRAME_PRAVATE_INFO, *PFRAME_PRAVATE_INFO;

VIM_S32 SAMPLE_COMM_Vpss_BindVenc(VENC_CHN VeChn, VI_CHN ViChn);
VIM_S32 SAMPLE_COMM_Vpss_UnBindVenc(VENC_CHN VeChn ,VI_CHN ViChn);
VIM_S32 SAMPLE_COMM_Vdec_BindVenc(VDEC_CHN VdChn, VENC_CHN VeChn);
VIM_S32 SAMPLE_COMM_Vdec_UnBindVenc(VDEC_CHN VdChn, VENC_CHN VeChn);

VIM_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, VIM_CHAR *szFilePostfix);
VIM_S32 SAMPLE_COMM_VENC_MemConfig(VIM_VOID);
VIM_U8 SAMPLE_COMM_VENC_Modify_Para(void *venc_para,VIM_S32 ch,VIM_U8 chn_num);

VIM_S32 SAMPLE_COMM_VENC_INIT(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 SAMPLE_COMM_VENC_OPEN(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 SAMPLE_COMM_VENC_START(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 SAMPLE_COMM_VENC_STOP(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 SAMPLE_COMM_VENC_CLOSE(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 SAMPLE_COMM_VENC_EXIT(SAMPLE_ENC_CFG *venc_cfg_config);

VIM_S32 SAMPLE_COMM_VENC_StartSendYuv(VIM_U32 pthread_id,VIM_S32 s32Cnt,VIM_S32 s32Index,SAMPLE_ENC_CFG *pEncCfg);
VIM_S32 SAMPLE_COMM_VENC_StopSendYuv(VIM_U32 pthread_id);

VIM_VOID* SAMPLE_COMM_VENC_SendFrame_Getstream_poll(VIM_VOID* p);
VIM_VOID* SAMPLE_COMM_VENC_SendFrame_Getstream_Wait(VIM_VOID* p);

VIM_S32 SAMPLE_COMM_VENC_StartGetStream(VIM_U32 pthread_id,VIM_S32 s32Cnt,VIM_S32 s32Index,SAMPLE_ENC_CFG *pEncCfg);
VIM_S32 SAMPLE_COMM_VENC_StopGetStream(VIM_U32 pthread_id,SAMPLE_ENC_CFG *pEncCfg);

VIM_S32 SAMPLE_COMM_VENC_BindVi(VENC_CHN VeChn, VI_CHN ViChn);
VIM_S32 SAMPLE_COMM_VENC_UnBindVi(VENC_CHN VencChn,VI_CHN ViChn);

VIM_S32 SAMPLE_COMM_VENC_PARACFG(VIM_CHAR *cfgfilename,SAMPLE_ENC_CFG *pEncCfg,VIM_U8*cfgchnnum);

VIM_S32 SAMPLE_COMM_VENC_ParseRoiCfgFile(SAMPLE_ENC_CFG *pEncCfg,VENC_ROI_CFG_S *pstVencRoiCfg,VIM_S32 num,VIM_U32 *roi_num);
VIM_S32 SAMPLE_SET_VENV_BufInfo(SAMPLE_ENC_CFG *venc_cfg_config);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* End of #ifndef __SAMPLE_COMMON_VENC_H__ */
