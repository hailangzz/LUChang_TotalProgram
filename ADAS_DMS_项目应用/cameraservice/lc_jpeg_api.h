#ifndef _LC_JPEG_API_H_
#define _LC_JPEG_API_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include "mpi_sys.h"
#include "mpi_vpss.h"
#include "mpi_jpeg.h"

#define SAMPLE_SYS_ALIGN_WIDTH 32

typedef struct{	
	VIM_BOOL JpegTakePic;		/** 拍照标志位，为TRUE时从流中截取数据并拷贝 **/
	JPEG_CHN u32JpegChnl;		/** 拍照的通道 **/
	VIM_U32  u32CameraChnl; 	/** 摄像头通道 **/	
	VIM_U32  u32PicWidth;		/** 照片的尺寸宽度 **/	
	VIM_U32  u32PicHeight;		/** 照片的尺寸高度 **/
	PIXEL_FORMAT_E enPixFmt;	/** 视频流的输入格式 **/
	VIM_S8 fileSaveName[128]; 	/** 图片存储的路径 **/
	void* ackFunc;
}JPEG_PIC_ATTR;

typedef struct{
	VIM_U32  u32UsedSign;
	VIM_U32  vpss_chn;			/** 绑定的视频流通道 **/
	VIM_U32  cam_chn;
	VIM_U32  frameDelay;
	VIM_U32  do_TakePic;
	VIDEO_FRAME_INFO_S pstFrame;	
	JPEG_PIC_ATTR JpegConfig;
}JPEG_PIC_CTRL_H;

VIM_S32 lc_jpeg_frame_send(VIM_VPSS_FRAME_INFO_S *stFrameInfo, VIM_U32 vpssChnl);
VIM_S32 lc_jpeg_do_takePic(char *path, VIM_U32 camChnl, VIM_U32 vpssChnl, void* para);
VIM_S32 lc_jpeg_chanel_start(JPEG_PIC_ATTR *jpegCfg, VIM_U32 vpssChnl, int level);
VIM_S32 lc_jpeg_chanel_stop(VIM_U32 vpssChnl);
JPEG_PIC_CTRL_H *lc_jpeg_handler(VIM_U32 vpssChnl);
VIM_VOID lc_jpeg_insert_to_queue(void* picTask);
VIM_VOID *lc_jpeg_get_from_queue(void);
VIM_VOID lc_jpeg_remove_tail_in_queue(void);
VIM_VOID lc_scaler_picture_take(int w, int h, VIM_VPSS_FRAME_INFO_S *inFrameInfo);
void lc_rgb_convert(void* srcR, void* srcG, void *srcB, void *color, int srcw, int srch);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif

