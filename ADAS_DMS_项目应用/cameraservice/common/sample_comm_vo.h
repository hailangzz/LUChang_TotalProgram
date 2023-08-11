#ifndef __SAMPLE_COMMON_VO__H_
#define __SAMPLE_COMMON_VO__H_

#include <errno.h>
#include <unistd.h>

#include "vim_comm_vo.h"


typedef struct _test_options
{
	int layer;
	int layerA_pri;
	int layerB_pri;	
	int mode;
	VIM_U32 layerA_width;
	VIM_U32 layerA_height;
	VIM_U32 layerB_width;
	VIM_U32 layerB_height;	
	VIM_U32 layerA_pixel_format;
	VIM_U32 layerB_pixel_format;	
	int bg_color;
	int m0_val;
	int m1_val;	
	int overlay_mode_1st_pri;
	int overlay_mode_2nd_pri;	
	int layerA_key_color;
	int layerB_key_color;
	int time;
	int alpha_m0_en;
	int alpha_m1_en;
	int overlay_1st_en;
	int overlay_2nd_en;
	int i2c_dev;	
	int multi_block;		
	int fifosize;
	int interval;
	int multi_version;
	int isThrowFrame;
}test_options;
typedef enum
{
	BINDERLIB = 0,
	BINDERDRI,
	NOBINDER,
}voBinderType;

typedef struct _vo_options
{
	VO_OUTPUT_TYPE mode;
	MultiMode multi_block;
	VO_PIXEL_FMT_E codeType;
	voBinderType binderType;
	int layer;
	int layerA_pixel_format;
	int layerB_pixel_format;
	mptxCfg miptCfg;
	int isThrowFrame;
}vo_options;
enum
{
	_480P60_ = 0,
	_576P50_,

	_720P60_,
	_720P50_,
	_720P30_,
	_720P25_,

	_1080P60_,
	_1080P50_,
	_1080P30_,
	_1080P25_,

	_1080i60_,
	_1080i50_,

	_4K30_,

	_800x600P60_,
	_1024x768P60_
};

VIM_S32 VO_Init_Enter(vo_options argc);
VIM_S32 DO_VO_BINDER_AddBuf(MultiChanBind *Vo_MultiChanInfo);
VIM_S32 VIM_MPI_SetFramebufferVar(int mode,int pixel_format,int voDev);

int get_fifo_size(void);

void VO_GetBlockCfg(VIM_U32 mode);
void get_mulblock_cfg(VO_MULMODE VoMulMode, VO_MUL_BLOCK_S *mb_cfg);
void cutYuv(int yuvType, unsigned char *tarYuv, unsigned char *srcYuv, int startW,
            int startH, int cutW, int cutH, int srcW, int srcH) ;
void get_vo_param(vo_options *voParam);
void set_vo_param(vo_options voParam);
VIM_S32 SAMPLE_VO_Cleanup( );
int vo_readParam(vo_options *voParam,char *filePath);


#endif

