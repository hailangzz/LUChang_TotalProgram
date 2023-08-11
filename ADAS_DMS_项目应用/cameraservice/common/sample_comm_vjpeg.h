#ifndef SAMPLE_COMM_VJPEG_H_
#define SAMPLE_COMM_VJPEG_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#include "mpi_jpeg.h"
#include "vim_comm_jpeg.h"

#ifndef SAMPLE_PRT
#define SAMPLE_PRT  printf
#endif


typedef struct {
    VIM_U32 work;
    VIM_U32 chn;
    VIM_S32 input_mode; // 0: file, 1: VPSS
    char input_file[256];    // input frame filename
    VIM_S32 gap;        // ms between 2 frame
    VIM_U32 repeats;    // repeat times
    JPEG_CHN_ATTR_S stChnAttr;
    VIM_S32 save;
	VIM_S32 chn_vpss;
} JPEG_CHN_CFG_S;

typedef struct {
    VIM_U32 u32ChnNums;
    JPEG_BUF_ATTR_S stBufAttr;
    JPEG_CHN_CFG_S stChnCfg[JPEGE_MAX_CHN_NUM];
} JPEG_CFG_S;

VIM_S32 SAMPLE_COMM_JPEG_ParseCfg(const char *cfg_filename, JPEG_CFG_S *pstJpegCfg);

VIM_S32 SAMPLE_COMM_JPEG_Init(JPEG_BUF_ATTR_S *pstBufAttr);
VIM_VOID SAMPLE_COMM_JPEG_Exit(void);

VIM_S32 SAMPLE_COMM_JPEG_Malloc(VIM_U32 u32Width, VIM_U32 u32Height, PIXEL_FORMAT_E enPixelFormat, VIDEO_FRAME_INFO_S *pstFrame);
VIM_S32 SAMPLE_COMM_JPEG_Free(VIDEO_FRAME_INFO_S *pstFrame);

VIM_VOID SAMPLE_COMM_JPEG_CheckRect(RECT_S *rect, PIXEL_FORMAT_E enPixFmt);
VIM_VOID SAMPLE_COMM_JPEG_AlignRect(RECT_S *rect, VIM_S32 xa, VIM_S32 ya, VIM_S32 wa, VIM_S32 ha);

VIM_S32 SAMPLE_COMM_JPEG_Start(JPEG_CHN JpgChn, JPEG_CHN_ATTR_S *pstAttr);
VIM_S32 SAMPLE_COMM_JPEG_Stop(JPEG_CHN JpgChn);

VIM_S32 SAMPLE_COMM_JPEG_StartGetStream(VIM_S32 s32Cnt);
VIM_S32 SAMPLE_COMM_JPEG_StopGetStream();
VIM_S32 SAMPLE_COMM_JPEG_GetStreamBlock(JPEG_CHN JpgChn, VIM_S32 s32MilliSec, VIM_U32 save);

VIM_S32 SAMPLE_COMM_JPEG_SendFrame(JPEG_CHN JpgChn, VIDEO_FRAME_INFO_S *pstFrame);

#endif /* SAMPLE_COMM_VJPEG_H_ */
