#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vo.h"
#include "sample_comm_vo.h"
#include "mpi_hdmi.h"
#include "de_hdmi.h"
#include "dictionary.h"
#include "iniparser.h"

int g_DevVo = VO_DEV_SD;

#define SAMPLE_PRT(fmt...)                       \
	do                                           \
	{                                            \
		printf("[%s]-%d: ", __func__, __LINE__); \
		printf(fmt);                             \
	} while (0)

#define SAMPLE_ERROR(fmt...) \
	do                       \
	{                        \
		printf("[ERROR]-");  \
		SAMPLE_PRT(fmt);     \
	} while (0)

#define SAMPLE_ENTER() SAMPLE_PRT("ENTER\n")
#define SAMPLE_LEAVE() SAMPLE_PRT("LEAVE\n\n")

typedef struct chnl_cfg
{
	///< int nSlice;
	int chnl_id;
	///< int nblock;
	int chnl_dst_x;
	int chnl_dst_y; ///<=slice y
	int chnl_src_x;
	int chnl_src_y;
	int chnl_w;
	int chnl_h; ///<=slice h
	int frame_rate;
} st_chnl_cfg;

typedef struct slice_cfg
{
	int chnl_cnt;
	int slice_y;
	int slice_h;
	st_chnl_cfg chnl_list[8];
} st_slice_cfg;

typedef struct block_cfg
{
	int slice_cnt;
	int fifo_size;
	st_slice_cfg slice_list[8];
} st_block_cfg;

st_block_cfg g_block_cfg;

int interval[MAX_BLOCK] = {0};
VIM_U32 y_addr[8] = {0};
VIM_U32 uv_addr[8] = {0};
static vo_options voParams = {0};

void get_vo_param(vo_options *voParam)
{
	memcpy(voParam, &voParams, sizeof(voParams));
}

void set_vo_param(vo_options voParam)
{
	memcpy(&voParams, &voParam, sizeof(vo_options));
}

int vo_readParam(vo_options *voParam, char *filePath)
{
	dictionary *ini = NULL;

	ini = iniparser_load(filePath);
	if (ini == NULL)
	{
		printf("--.ini file ERR %s--\n", filePath);
		return -1;
	}
	printf("--Get Params From: %s--\n", filePath);

	/**************************VO_PARAM******************************/
	voParam->layer = iniparser_getint(ini, "VO:u32Layer", 0);
	voParam->mode = iniparser_getint(ini, "VO:u32Mode", 0);
	voParam->layerA_pixel_format = iniparser_getint(ini, "VO:u32LayerACodeType", 0);
	voParam->layerB_pixel_format = iniparser_getint(ini, "VO:u32LayerBCodeType", 0);
	voParam->multi_block = iniparser_getint(ini, "VO:u32MultiMode", 0);
	voParam->binderType = iniparser_getint(ini, "VO:u32BinderType", 0);
	voParam->isThrowFrame = iniparser_getint(ini, "VO:u32IsThrowFrame", 0);
	
	/**************************MPTX_PARAM******************************/
	voParam->miptCfg.screen_info.h_act = iniparser_getint(ini, "MPTX:u32h_act", 0);
	voParam->miptCfg.screen_info.v_act = iniparser_getint(ini, "MPTX:u32v_act", 0);
	voParam->miptCfg.screen_info.hfp = iniparser_getint(ini, "MPTX:u32hfp", 0);
	voParam->miptCfg.screen_info.hbp = iniparser_getint(ini, "MPTX:u32hbp", 0);
	voParam->miptCfg.screen_info.hsw = iniparser_getint(ini, "MPTX:u32hsw", 0);
	voParam->miptCfg.screen_info.vfp = iniparser_getint(ini, "MPTX:u32vfp", 0);
	voParam->miptCfg.screen_info.vbp = iniparser_getint(ini, "MPTX:u32vbp", 0);
	voParam->miptCfg.screen_info.vsw = iniparser_getint(ini, "MPTX:u32vsw", 0);
	voParam->miptCfg.screen_info.datarate = iniparser_getint(ini, "MPTX:u32datarate", 0);
	voParam->miptCfg.screen_info.den_pol = iniparser_getint(ini, "MPTX:u32den_pol", 0);
	voParam->miptCfg.screen_info.vsync_pol = iniparser_getint(ini, "MPTX:u32vsync_pol", 0);
	voParam->miptCfg.screen_info.hsync_pol = iniparser_getint(ini, "MPTX:u32hsync_pol", 0);
	voParam->miptCfg.screen_info.mipi_mode = iniparser_getint(ini, "MPTX:u32mipi_mode", 0);
	
#if 0
	printf("\t voParam.Layer -> %d\n", voParam->layer);
	printf("\t voParam.mode -> %d\n", voParam->mode);
	printf("\t voParam.layerA_pixel_format -> %d\n", voParam->layerA_pixel_format);
	printf("\t voParam.layerB_pixel_format -> %d\n", voParam->layerB_pixel_format);
	printf("\t voParam.multi_block -> %d\n", voParam->multi_block);
	printf("\t voParam->miptCfg.h_act -> %d\n", voParam->miptCfg.screen_info.h_act);
	printf("\t voParam->miptCfg.v_act -> %d\n", voParam->miptCfg.screen_info.v_act);
	printf("\t voParam->miptCfg.hfp -> %d\n", voParam->miptCfg.screen_info.hfp);
	printf("\t voParam->miptCfg.hbp -> %d\n", voParam->miptCfg.screen_info.hbp);
	printf("\t voParam->miptCfg.hsw -> %d\n", voParam->miptCfg.screen_info.hsw);
	printf("\t voParam->miptCfg.vfp -> %d\n", voParam->miptCfg.screen_info.vfp);
	printf("\t voParam->miptCfg.vbp -> %d\n", voParam->miptCfg.screen_info.vbp);
	printf("\t voParam->miptCfg.vsw -> %d\n", voParam->miptCfg.screen_info.vsw);
	printf("\t voParam->miptCfg.datarate -> %d\n", voParam->miptCfg.screen_info.datarate);
	printf("\t voParam->miptCfg.den_pol -> %d\n", voParam->miptCfg.screen_info.den_pol);
	printf("\t voParam->miptCfg.vsync_pol -> %d\n", voParam->miptCfg.screen_info.vsync_pol);
	printf("\t voParam->miptCfg.hsync_pol -> %d\n", voParam->miptCfg.screen_info.hsync_pol);
	printf("\t voParam->miptCfg.mipi_mode -> %d\n", voParam->miptCfg.screen_info.mipi_mode);
	printf("\t ----------------------\n");
	printf("case config:\n");
#endif

	iniparser_freedict(ini);
	ini = NULL;

	return 0;
}

static int test_vi_opt_parser(vo_options paramCfg,
							  test_options *opt,
							  mptxCfg *miptCfg)
{
	int ret = 0;

	SAMPLE_ENTER();

	opt->layer = paramCfg.layer;
	opt->layerA_pri = 0;
	opt->layerB_pri = 1;
	opt->mode = paramCfg.mode;
	opt->layerA_width = 0;
	opt->layerA_height = 0;
	opt->layerB_width = 0;
	opt->layerB_height = 0;
	opt->layerA_pixel_format = paramCfg.layerA_pixel_format;
	opt->layerB_pixel_format = paramCfg.layerB_pixel_format;
	opt->bg_color = 0xffffff;
	opt->alpha_m0_en = 0;
	opt->alpha_m1_en = 0;
	opt->m0_val = 0x0;
	opt->m1_val = 0x0;
	opt->overlay_mode_1st_pri = 0;
	opt->overlay_mode_2nd_pri = 0;
	opt->layerA_key_color = 0x0;
	opt->layerB_key_color = 0x0;
	opt->overlay_1st_en = 0;
	opt->overlay_2nd_en = 0;
	opt->i2c_dev = 0;
	opt->multi_block = paramCfg.multi_block;
	opt->fifosize = 1;
	opt->interval = 24;
	opt->time = 100000;
	opt->multi_version = 1;
	opt->isThrowFrame = paramCfg.isThrowFrame;
	memcpy(miptCfg, &paramCfg.miptCfg, sizeof(mptxCfg));
#if 0
	printf("opt->layer 0x%x\n", opt->layer);
	printf("opt->layerA_pri 0x%x\n", opt->layerA_pri);
	printf("opt->layerB_pri 0x%x\n", opt->layerB_pri);
	printf("opt->mode 0x%x\n", opt->mode);
	printf("opt->layerA_width 0x%x\n", opt->layerA_width);
	printf("opt->layerA_height 0x%x\n", opt->layerA_height);
	printf("opt->layerB_width 0x%x\n", opt->layerB_width);
	printf("opt->layerB_height 0x%x\n", opt->layerB_height);
	printf("opt->layerA_pixel_format 0x%x\n", opt->layerA_pixel_format);
	printf("opt->layerB_pixel_format 0x%x\n", opt->layerB_pixel_format);
	printf("opt->bg_color 0x%x\n", opt->bg_color);
	printf("opt->alpha_m0_en 0x%x\n", opt->alpha_m0_en);
	printf("opt->alpha_m1_en 0x%x\n", opt->alpha_m1_en);
	printf("opt->m0_val 0x%x\n", opt->m0_val);
	printf("opt->m1_val 0x%x\n", opt->m1_val);
	printf("opt->overlay_1st_en 0x%x\n", opt->overlay_1st_en);
	printf("opt->overlay_2nd_en 0x%x\n", opt->overlay_2nd_en);
	printf("opt->overlay_mode_1st_pri 0x%x\n", opt->overlay_mode_1st_pri);
	printf("opt->overlay_mode_2nd_pri 0x%x\n", opt->overlay_mode_2nd_pri);
	printf("opt->layerA_key_color 0x%x\n", opt->layerA_key_color);
	printf("opt->layerB_key_color 0x%x\n", opt->layerB_key_color);
	printf("opt->i2c_dev %d\n", opt->i2c_dev);
	printf("opt->multi_block %d\n", opt->multi_block);
	printf("opt->fifosize %d\n", opt->fifosize);
	printf("opt->interval %d\n", opt->interval);
	printf("opt->time %d\n", opt->time);
	printf("opt->multi_version %d\n", opt->multi_version);
	printf("opt->isThrowFrame %d\n", opt->isThrowFrame);
#endif
	if (2 == opt->mode || 3 == opt->mode || 4 == opt->mode || 5 == opt->mode ||
		6 == opt->mode || 7 == opt->mode || 8 == opt->mode || 9 == opt->mode ||
		10 == opt->mode)
	{
		g_DevVo = VO_DEV_HD;
	}
	else
	{
		g_DevVo = VO_DEV_SD;
		interval[0] = opt->interval;
	}

	SAMPLE_LEAVE();

	return ret;
}

static VIM_S32 VO_Init_LayerA(test_options *options)
{
	VIM_S32 ret = 0;
	SIZE_S stCapSize = {0};
	VO_VIDEO_LAYER_ATTR_S stLayerAttr = {0};

	SAMPLE_ENTER();

	if (options->layerA_width == 0 && options->layerA_height == 0)
	{
		switch (options->mode)
		{
		case 0:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 576;
			break;
		case 1:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 480;
			break;
		case 2:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 3:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 576;
			break;
		case 4:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 5:
			stCapSize.u32Width = 3840;
			stCapSize.u32Height = 2160;
			break;
		case 6:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 7:
			stCapSize.u32Width = 3840;
			stCapSize.u32Height = 2160;
			break;
		case 8:
			stCapSize.u32Width = 1280;
			stCapSize.u32Height = 720;
			break;
		case 9:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 10:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		default:
			break;
		}
	}
	else
	{
		stCapSize.u32Width = options->layerA_width;
		stCapSize.u32Height = options->layerA_height;
	}

	if (options->mode == 0 && (stCapSize.u32Width > 720 || stCapSize.u32Height > 576))
	{
		stCapSize.u32Width = 720;
		stCapSize.u32Height = 576;
	}
	else if (options->mode == 1 && (stCapSize.u32Width > 720 || stCapSize.u32Height > 480))
	{
		stCapSize.u32Width = 720;
		stCapSize.u32Height = 480;
	}

	stLayerAttr.stImageSize.u32Width = stCapSize.u32Width;
	stLayerAttr.stImageSize.u32Height = stCapSize.u32Height;
	stLayerAttr.stDispRect.s32X = 0;
	stLayerAttr.stDispRect.s32Y = 0;
	stLayerAttr.stDispRect.u32Width = stCapSize.u32Width;
	stLayerAttr.stDispRect.u32Height = stCapSize.u32Height;
	stLayerAttr.u32DispFrmRt = 60;

	switch (options->layerA_pixel_format)
	{
	case 0: ///< 422sp
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_SP_UV;
		break;
	case 1: ///< 420sp
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV420_SP_UV;
		break;
	case 2:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_SP_VU;
		break;
	case 3:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV420_SP_VU;
		break;
	case 4:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_UYVY;
		break;
	case 5:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_VYUY;
		break;
	case 6:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_YUYV;
		break;
	case 7:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_YVYU;
		break;
	default:
		stLayerAttr.enPixFormat = PIXEL_FMT_YUV422_SP_UV;
		break;
	}

	ret = VIM_MPI_VO_SetVideoLayerAttr(g_DevVo, 0, &stLayerAttr);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set video layer attributes failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	if (2 == options->mode || 3 == options->mode || 4 == options->mode || 5 == options->mode ||
		6 == options->mode || 7 == options->mode || 8 == options->mode || 9 == options->mode ||
		10 == options->mode)
	{
		VIM_MPI_VO_SetMultiMode(options->multi_block, MULTI_CONTROL_AUTO, NULL, NULL);
	}

	ret = VIM_MPI_VO_EnableVideoLayer(g_DevVo, 0, options->multi_block);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Enable video layer failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_SetVideoLayerPriority(g_DevVo, 0, options->layerA_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set video layer priority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	SAMPLE_LEAVE();

	return ret;
}
VIM_S32 SAMPLE_VO_SetFramebufferVar(int mode, int pixel_format, int voDev)
{
	VIM_S32 screen_fb = 0;
	struct fb_var_screeninfo fb_var;
	VIM_U32 width = 0;
	VIM_U32 height = 0;
	VIM_S32 ret = 0;

	ret = VIM_MPI_VO_OpenFramebufferGetFd(voDev, 2, 0, 0, &screen_fb);
	if (ret)
	{
		SAMPLE_ERROR("VIM_MPI_VO_OpenFramebufferGetFd failed. ret = %d.", ret);
		return ret;
	}

	if (0 == screen_fb)
	{
		return ret;
	}

	ret = ioctl(screen_fb, FBIOGET_VSCREENINFO, &fb_var);
	if (ret)
	{
		SAMPLE_ERROR("ioctl(FBIOGET_VSCREENINFO) failed. ret = %d.", ret);
		return VIM_ERR_VO_VIDEO_NOT_ENABLE;
	}

	switch (mode)
	{
	case 0:
		width = 720;
		height = 576;
		break;
	case 1:
		width = 720;
		height = 480;
		break;
	case 2:
		width = 1920;
		height = 1080;
		break;
	case 3:
		width = 720;
		height = 576;
		break;
	case 4:
		width = 1920;
		height = 1080;
		break;
	case 5:
		width = 3840;
		height = 2160;
		break;
	default:
		break;
	}

	switch (pixel_format)
	{
	case 0:
		fb_var.bits_per_pixel = 16;

		fb_var.xres = width;
		fb_var.yres = height;
		fb_var.width = width;
		fb_var.height = height;
		fb_var.xres_virtual = width;
		fb_var.yres_virtual = height; ///< 3

		fb_var.red.length = 5;
		fb_var.red.offset = 11;
		fb_var.red.msb_right = 0;

		fb_var.green.length = 6;
		fb_var.green.offset = 5;
		fb_var.green.msb_right = 0;

		fb_var.blue.length = 5;
		fb_var.blue.offset = 0;
		fb_var.blue.msb_right = 0;
		break;
	case 1:
		fb_var.bits_per_pixel = 32;

		fb_var.xres = width;
		fb_var.yres = height * 2;
		fb_var.width = width;
		fb_var.height = height * 2;
		fb_var.xres_virtual = width;
		fb_var.yres_virtual = height * 2; ///< 3

		fb_var.red.length = 8;
		fb_var.red.offset = 16;
		fb_var.red.msb_right = 0;

		fb_var.green.length = 8;
		fb_var.green.offset = 8;
		fb_var.green.msb_right = 0;

		fb_var.blue.length = 8;
		fb_var.blue.offset = 0;
		fb_var.blue.msb_right = 0;
		break;
	case 2:
		fb_var.bits_per_pixel = 16;

		fb_var.xres = width;
		fb_var.yres = height;
		fb_var.width = width;
		fb_var.height = height;
		fb_var.xres_virtual = width;
		fb_var.yres_virtual = height; ///< 3

		fb_var.transp.length = 1;
		fb_var.transp.offset = 15;
		fb_var.transp.msb_right = 0;

		fb_var.red.length = 5;
		fb_var.red.offset = 10;
		fb_var.red.msb_right = 0;

		fb_var.green.length = 5;
		fb_var.green.offset = 10;
		fb_var.green.msb_right = 0;

		fb_var.blue.length = 5;
		fb_var.blue.offset = 0;
		fb_var.blue.msb_right = 0;
		break;
	case 3:
		fb_var.bits_per_pixel = 32;

		fb_var.xres = width;
		fb_var.yres = height * 2;
		fb_var.width = width;
		fb_var.height = height * 2;
		fb_var.xres_virtual = width;
		fb_var.yres_virtual = height * 2; ///< 3

		fb_var.transp.length = 8;
		fb_var.transp.offset = 24;
		fb_var.transp.msb_right = 0;

		fb_var.red.length = 8;
		fb_var.red.offset = 16;
		fb_var.red.msb_right = 0;

		fb_var.green.length = 8;
		fb_var.green.offset = 8;
		fb_var.green.msb_right = 0;

		fb_var.blue.length = 8;
		fb_var.blue.offset = 0;
		fb_var.blue.msb_right = 0;
		break;
	case 4:
		fb_var.bits_per_pixel = 32;

		fb_var.xres = width;
		fb_var.yres = height * 2;
		fb_var.width = width;
		fb_var.height = height * 2;
		fb_var.xres_virtual = width;
		fb_var.yres_virtual = height * 2; ///< 3

		fb_var.transp.length = 8;
		fb_var.transp.offset = 0;
		fb_var.transp.msb_right = 0;

		fb_var.red.length = 8;
		fb_var.red.offset = 24;
		fb_var.red.msb_right = 0;

		fb_var.green.length = 8;
		fb_var.green.offset = 16;
		fb_var.green.msb_right = 0;

		fb_var.blue.length = 8;
		fb_var.blue.offset = 8;
		fb_var.blue.msb_right = 0;
		break;
	default:
		///< strncpy(format, "RGB565", strlen("RGB565"));
		break;
	}

	ret = ioctl(screen_fb, FBIOPUT_VSCREENINFO, &fb_var);
	if (ret)
	{
		SAMPLE_ERROR("ioctl(FBIOPUT_VSCREENINFO) failed. ret = %d.", ret);
		return VIM_ERR_VO_VIDEO_NOT_ENABLE;
	}

	ret = ioctl(screen_fb, FBIOGET_VSCREENINFO, &fb_var);
	if (ret)
	{
		SAMPLE_ERROR("ioctl(FBIOGET_VSCREENINFO) failed. ret = %d.", ret);
		return VIM_ERR_VO_VIDEO_NOT_ENABLE;
	}

	VIM_MPI_VO_CloseFramebufferByFd(voDev, screen_fb);

	return ret;
}

static VIM_S32 VO_Init_LayerB(test_options *options)
{
	VIM_S32 ret = 0;
	SIZE_S stCapSize = {0};
	VO_VIDEO_LAYER_ATTR_S stLayerAttr = {0};
	VO_ALPHA_S pstDevAlpha = {0};

	SAMPLE_ENTER();

	if (options->layerB_width == 0 && options->layerB_height == 0)
	{
		switch (options->mode)
		{
		case 0:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 576;
			break;
		case 1:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 480;
			break;
		case 2:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 3:
			stCapSize.u32Width = 720;
			stCapSize.u32Height = 576;
			break;
		case 4:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 5:
			stCapSize.u32Width = 3840;
			stCapSize.u32Height = 2160;
			break;
		case 6:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 7:
			stCapSize.u32Width = 3840;
			stCapSize.u32Height = 2160;
			break;
		case 8:
			stCapSize.u32Width = 1280;
			stCapSize.u32Height = 720;
			break;
		case 9:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		case 10:
			stCapSize.u32Width = 1920;
			stCapSize.u32Height = 1080;
			break;
		default:
			break;
		}
	}
	else
	{
		stCapSize.u32Width = options->layerB_width;
		stCapSize.u32Height = options->layerB_height;
	}

	if (options->mode == 0 && (stCapSize.u32Width > 720 || stCapSize.u32Height > 576))
	{
		stCapSize.u32Width = 720;
		stCapSize.u32Height = 576;
	}
	else if (options->mode == 1 && (stCapSize.u32Width > 720 || stCapSize.u32Height > 480))
	{
		stCapSize.u32Width = 720;
		stCapSize.u32Height = 480;
	}

	stLayerAttr.stImageSize.u32Width = stCapSize.u32Width;
	stLayerAttr.stImageSize.u32Height = stCapSize.u32Height;

	stLayerAttr.stDispRect.s32X = 0;
	stLayerAttr.stDispRect.s32Y = 0;
	stLayerAttr.stDispRect.u32Width = stCapSize.u32Width;
	stLayerAttr.stDispRect.u32Height = stCapSize.u32Height;
	stLayerAttr.u32DispFrmRt = 60;

	switch (options->layerB_pixel_format)
	{
	case 0:
		stLayerAttr.enPixFormat = PIXEL_FMT_RGB565;
		break;
	case 1:
		stLayerAttr.enPixFormat = PIXEL_FMT_RGB888_UNPACKED;
		break;
	case 2:
		stLayerAttr.enPixFormat = PIXEL_FMT_ARGB1555;
		break;
	case 3:
		stLayerAttr.enPixFormat = PIXEL_FMT_ARGB8888;
		break;
	case 4:
		stLayerAttr.enPixFormat = PIXEL_FMT_RGBA8888;
		break;
	default:
		stLayerAttr.enPixFormat = PIXEL_FMT_RGB565;
		break;
	}

	ret = VIM_MPI_VO_SetVideoLayerAttr(g_DevVo, 2, &stLayerAttr);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set video layer attributes failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_EnableVideoLayer(g_DevVo, 2, Multi_None);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Enable video layer failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_SetVideoLayerPriority(g_DevVo, 2, options->layerB_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set video layer priority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	if (options->alpha_m0_en)
	{
		pstDevAlpha.bAlphaEnable = options->alpha_m0_en;
		pstDevAlpha.u32AlphaMode = 0;
		pstDevAlpha.u32GlobalAlpha = options->m0_val;
	}

	if (options->alpha_m1_en)
	{
		pstDevAlpha.bAlphaEnable = options->alpha_m1_en;
		pstDevAlpha.u32AlphaMode = 1;
		pstDevAlpha.u32GlobalAlpha = options->m1_val;
	}

	ret = VIM_MPI_VO_SetAlpha(g_DevVo, 2, &pstDevAlpha);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set Alpha failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}
	ret = SAMPLE_VO_SetFramebufferVar(options->mode, options->layerB_pixel_format, g_DevVo);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set FramebufferVar failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	SAMPLE_LEAVE();

	return ret;
}

static VIM_S32 VO_cfg_1stpri_layer_overlay(test_options *options)
{
	VIM_S32 ret = 0;
	VIM_U32 layerA_pri = 0;
	VIM_U32 layerB_pri = 0;
	VIM_U32 layer_1st = 0;
	VO_COLORKEY_S pstDevColorKey;

	SAMPLE_ENTER();

	pstDevColorKey.bKeyEnable = 1;
	pstDevColorKey.u32OverlayMode = options->overlay_mode_1st_pri;

	ret = VIM_MPI_VO_GetVideoLayerPriority(g_DevVo, 0, &layerA_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("VIM_MPI_VO_GetVideoLayerAPriority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_GetVideoLayerPriority(g_DevVo, 2, &layerB_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("VIM_MPI_VO_GetVideoLayerBPriority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	if (layerA_pri < layerB_pri)
	{
		layer_1st = 0;
		pstDevColorKey.u32Key = options->layerA_key_color;
	}
	else if (layerA_pri > layerB_pri)
	{
		layer_1st = 2;
		pstDevColorKey.u32Key = options->layerB_key_color;
	}
	else
	{
		SAMPLE_ERROR("layer priority config error!\n");
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_SetColorKey(g_DevVo, layer_1st, &pstDevColorKey);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set ColorKey failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	SAMPLE_LEAVE();

	return ret;
}
VIM_S32 SAMPLE_VO_Cleanup(void)
{
	int ret = 0;

	ret = VIM_MPI_VO_CleanUpBinder();

	return ret;
}

static VIM_S32 VO_cfg_2ndpri_layer_overlay(test_options *options)
{
	VIM_S32 ret = 0;
	VIM_U32 layerA_pri = 0;
	VIM_U32 layerB_pri = 0;
	VIM_U32 layer_2nd = 0;
	VO_COLORKEY_S pstDevColorKey;

	SAMPLE_ENTER();

	pstDevColorKey.bKeyEnable = 1;
	pstDevColorKey.u32OverlayMode = options->overlay_mode_2nd_pri;

	ret = VIM_MPI_VO_GetVideoLayerPriority(g_DevVo, 0, &layerA_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("VIM_MPI_VO_GetVideoLayerAPriority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_GetVideoLayerPriority(g_DevVo, 2, &layerB_pri);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("VIM_MPI_VO_GetVideoLayerBPriority failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	if (layerA_pri < layerB_pri)
	{
		layer_2nd = 2;
		pstDevColorKey.u32Key = options->layerA_key_color;
	}
	else if (layerA_pri > layerB_pri)
	{
		layer_2nd = 0;
		pstDevColorKey.u32Key = options->layerB_key_color;
	}
	else
	{
		SAMPLE_ERROR("layer priority config error!\n");
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_SetColorKey(g_DevVo, layer_2nd, &pstDevColorKey);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("Set ColorKey failed with errno %#x!\n", ret);
		return VIM_FAILURE;
	}

	SAMPLE_LEAVE();

	return ret;
}

VIM_S32 VO_StartDev(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, VIM_S32 nBgColor)
{
	VIM_S32 ret = 0;
	VO_PUB_ATTR_S stPubAttr;

	memset(&stPubAttr, 0, sizeof(stPubAttr));

	stPubAttr.stYuvCfg.en_ycbcr = 0;
	stPubAttr.stYuvCfg.yuv_clip = 0;
	stPubAttr.stYuvCfg.yc_swap = 0;
	stPubAttr.stYuvCfg.uv_seq = 0;

	switch (enIntfSync)
	{
	case VO_OUTPUT_PAL: ///< PAL
		stPubAttr.enIntfType = VO_INTF_CVBS;
		stPubAttr.enIntfSync = VO_OUTPUT_PAL;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 720;
		stPubAttr.stSyncInfo.u16Hbb = 264;
		stPubAttr.stSyncInfo.u16Hfb = 24;

		stPubAttr.stSyncInfo.u16Vact = 576;
		stPubAttr.stSyncInfo.u16Vbb = 22;
		stPubAttr.stSyncInfo.u16Vfb = 2;

		stPubAttr.stSyncInfo.u16Bvbb = 23;
		stPubAttr.stSyncInfo.u16Bvfb = 2;

		stPubAttr.stSyncInfo.u16Hpw = 1;
		stPubAttr.stSyncInfo.u16Vpw = 1;
		stPubAttr.stSyncInfo.u16Vpw2 = 1;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;
		break;

	case VO_OUTPUT_640P60: ///< VGA tmply
		stPubAttr.enIntfType = VO_INTF_CVBS;
		stPubAttr.enIntfSync = VO_OUTPUT_640P60;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 720;
		stPubAttr.stSyncInfo.u16Hbb = 264;
		stPubAttr.stSyncInfo.u16Hfb = 24;

		stPubAttr.stSyncInfo.u16Vact = 576;
		stPubAttr.stSyncInfo.u16Vbb = 22;
		stPubAttr.stSyncInfo.u16Vfb = 2;

		stPubAttr.stSyncInfo.u16Bvbb = 23;
		stPubAttr.stSyncInfo.u16Bvfb = 2;

		stPubAttr.stSyncInfo.u16Hpw = 1;
		stPubAttr.stSyncInfo.u16Vpw = 1;
		stPubAttr.stSyncInfo.u16Vpw2 = 1;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;
		break;

	case VO_OUTPUT_NTSC: ///< NTSC
		stPubAttr.enIntfType = VO_INTF_CVBS;
		stPubAttr.enIntfSync = VO_OUTPUT_NTSC;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 720;
		stPubAttr.stSyncInfo.u16Hbb = 244;
		stPubAttr.stSyncInfo.u16Hfb = 32;

		stPubAttr.stSyncInfo.u16Vact = 480;
		stPubAttr.stSyncInfo.u16Vbb = 19;
		stPubAttr.stSyncInfo.u16Vfb = 3;

		stPubAttr.stSyncInfo.u16Bvbb = 20;
		stPubAttr.stSyncInfo.u16Bvfb = 3;

		stPubAttr.stSyncInfo.u16Hpw = 1;
		stPubAttr.stSyncInfo.u16Vpw = 1;
		stPubAttr.stSyncInfo.u16Vpw2 = 1;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;
		break;
	case VO_OUTPUT_1080P30: ///< 1080P hdmi
		stPubAttr.enIntfType = VO_INTF_BT1120;
		stPubAttr.enIntfSync = VO_OUTPUT_1080P30;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1920;
		stPubAttr.stSyncInfo.u16Hbb = 192; ///<
		stPubAttr.stSyncInfo.u16Hfb = 88;  ///< 0

		stPubAttr.stSyncInfo.u16Vact = 1080;
		stPubAttr.stSyncInfo.u16Vbb = 41;
		stPubAttr.stSyncInfo.u16Vfb = 4;

		stPubAttr.stSyncInfo.u16Bvbb = 41;
		stPubAttr.stSyncInfo.u16Bvfb = 4;

		stPubAttr.stSyncInfo.u16Hpw = 44; ///<
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 1;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 1;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;

	case VO_OUTPUT_576P50: ///< 576P hdmi
		stPubAttr.enIntfType = VO_INTF_BT1120;
		stPubAttr.enIntfSync = VO_OUTPUT_576P50;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 720;
		stPubAttr.stSyncInfo.u16Hbb = 264;
		stPubAttr.stSyncInfo.u16Hfb = 24;

		stPubAttr.stSyncInfo.u16Vact = 576;
		stPubAttr.stSyncInfo.u16Vbb = 22;
		stPubAttr.stSyncInfo.u16Vfb = 2;

		stPubAttr.stSyncInfo.u16Bvbb = 23;
		stPubAttr.stSyncInfo.u16Bvfb = 2;

		stPubAttr.stSyncInfo.u16Hpw   = 64;
		stPubAttr.stSyncInfo.u16Vpw   = 5;
		stPubAttr.stSyncInfo.u16Vpw2  = 1;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 1;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;

	case CSI_OUT_1080P30: ///< csi 1080@30
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = CSI_OUT_1080P30;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_CSI;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1920;
		stPubAttr.stSyncInfo.u16Hbb = 192;
		stPubAttr.stSyncInfo.u16Hfb = 88;

		stPubAttr.stSyncInfo.u16Vact = 1080;
		stPubAttr.stSyncInfo.u16Vbb = 41;
		stPubAttr.stSyncInfo.u16Vfb = 4;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 44;
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case CSI_OUT_4K30: ///< csi 4K@30
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = CSI_OUT_4K30;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_CSI;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 3840;
		stPubAttr.stSyncInfo.u16Hbb = 384;
		stPubAttr.stSyncInfo.u16Hfb = 176;

		stPubAttr.stSyncInfo.u16Vact = 2160;
		stPubAttr.stSyncInfo.u16Vbb = 82;
		stPubAttr.stSyncInfo.u16Vfb = 8;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 88;
		stPubAttr.stSyncInfo.u16Vpw = 10;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case CSI_OUT_1080P60: ///< csi 1080@30
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = CSI_OUT_1080P60;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_CSI;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1920;
		stPubAttr.stSyncInfo.u16Hbb = 192;
		stPubAttr.stSyncInfo.u16Hfb = 88;

		stPubAttr.stSyncInfo.u16Vact = 1080;
		stPubAttr.stSyncInfo.u16Vbb = 41;
		stPubAttr.stSyncInfo.u16Vfb = 4;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 44;
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case DSI_OUT_1080P30: ///< dsi 1080@30
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = DSI_OUT_1080P30;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_DSI_RGB888;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1920;
		stPubAttr.stSyncInfo.u16Hbb = 192;
		stPubAttr.stSyncInfo.u16Hfb = 88;

		stPubAttr.stSyncInfo.u16Vact = 1080;
		stPubAttr.stSyncInfo.u16Vbb = 41;
		stPubAttr.stSyncInfo.u16Vfb = 4;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 44;
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case DSI_OUT_1080P60: ///< dsi 1080@60
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = DSI_OUT_1080P60;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_DSI_RGB888;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1920;
		stPubAttr.stSyncInfo.u16Hbb = 192;
		stPubAttr.stSyncInfo.u16Hfb = 88;

		stPubAttr.stSyncInfo.u16Vact = 1080;
		stPubAttr.stSyncInfo.u16Vbb = 41;
		stPubAttr.stSyncInfo.u16Vfb = 4;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 44;
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case DSI_OUT_4K30: ///< csi 4K@30
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = DSI_OUT_4K30;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_DSI_RGB888;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 3840;
		stPubAttr.stSyncInfo.u16Hbb = 384;
		stPubAttr.stSyncInfo.u16Hfb = 176;

		stPubAttr.stSyncInfo.u16Vact = 2160;
		stPubAttr.stSyncInfo.u16Vbb = 82;
		stPubAttr.stSyncInfo.u16Vfb = 8;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 88;
		stPubAttr.stSyncInfo.u16Vpw = 10;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;
	case DSI_OUT_720P60: ///< dsi 720p@60
		stPubAttr.enIntfType = VO_INTF_MIPI;
		stPubAttr.enIntfSync = DSI_OUT_720P60;
		stPubAttr.stSyncInfo.u16MipiMode = MIPI_MODE_DSI_RGB888;

		stPubAttr.stSyncInfo.bSynm = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIop = (VIM_BOOL)1;
		stPubAttr.stSyncInfo.u8Intfb = 8;

		stPubAttr.stSyncInfo.u16Hact = 1280;
		stPubAttr.stSyncInfo.u16Hbb = 260;
		stPubAttr.stSyncInfo.u16Hfb = 110;

		stPubAttr.stSyncInfo.u16Vact = 720;
		stPubAttr.stSyncInfo.u16Vbb = 25;
		stPubAttr.stSyncInfo.u16Vfb = 5;

		stPubAttr.stSyncInfo.u16Bvbb = 0;
		stPubAttr.stSyncInfo.u16Bvfb = 0;

		stPubAttr.stSyncInfo.u16Hpw = 40;
		stPubAttr.stSyncInfo.u16Vpw = 5;
		stPubAttr.stSyncInfo.u16Vpw2 = 0;

		stPubAttr.stSyncInfo.bIdv = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIhs = (VIM_BOOL)0;
		stPubAttr.stSyncInfo.bIvs = (VIM_BOOL)0;

		stPubAttr.enBt1120 = 0;
		stPubAttr.stYuvCfg.en_ycbcr = 0;
		stPubAttr.stYuvCfg.yc_swap = 0;
		break;

	default:
		SAMPLE_PRT("Invalid Interface(%d)\n", stPubAttr.enIntfSync);
		return VIM_FAILURE;
	}

	stPubAttr.u32BgColor = nBgColor;

	ret = VIM_MPI_VO_SetPubAttr(VoDev, &stPubAttr);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_PRT("VIM_MPI_VO_SetPubAttr() failed. (%d)\n", ret);
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VO_Enable(VoDev, enIntfSync);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_PRT("Enable vo dev %d failed with errno %#x!\n", VoDev, ret);
		return VIM_FAILURE;
	}

	return ret;
}

static VIM_S32 VO_Setup(test_options *options)
{
	VIM_S32 ret = 0;
	VIM_S32 nSyncMode = {0};

	SAMPLE_ENTER();

	switch (options->mode)
	{
	case 0:
		nSyncMode = VO_OUTPUT_PAL;
		break;
	case 1:
		nSyncMode = VO_OUTPUT_NTSC;
		break;
	case 2:
		nSyncMode = VO_OUTPUT_1080P30;
		break;
	case 3:
		nSyncMode = VO_OUTPUT_576P50;
		break;
	case 4:
		nSyncMode = CSI_OUT_1080P30;
		break;
	case 5:
		nSyncMode = CSI_OUT_4K30;
		break;
	case 6:
		nSyncMode = DSI_OUT_1080P30;
		break;
	case 7:
		nSyncMode = DSI_OUT_4K30;
		break;
	case 8:
		nSyncMode = DSI_OUT_720P60;
		break;
	case 9:
		nSyncMode = DSI_OUT_1080P60;
		break;
	case 10:
		nSyncMode = CSI_OUT_1080P60;
		break;
	default:
		break;
	}

	ret = VO_StartDev(g_DevVo, nSyncMode, options->bg_color);
	if (ret != VIM_SUCCESS)
	{
		SAMPLE_ERROR("SAMPLE_VO_Init_Dev() failed. (%d)\n", ret);
		return VIM_FAILURE;
	}

	if (0 == options->layer)
	{
		ret = VO_Init_LayerA(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("SAMPLE_VO_Init_Dev() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}

		ret = VIM_MPI_VO_DisableVideoLayer(g_DevVo, 2);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("VIM_MPI_VO_DisableVideoLayer() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}
	}

	if (1 == options->layer)
	{
		ret = VO_Init_LayerB(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("SAMPLE_VO_Init_Dev() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}

		ret = VIM_MPI_VO_DisableVideoLayer(g_DevVo, 0);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("VIM_MPI_VO_DisableVideoLayer() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}
	}

	if (2 == options->layer)
	{
		ret = VO_Init_LayerA(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("SAMPLE_VO_Init_Dev() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}

		ret = VO_Init_LayerB(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("SAMPLE_VO_Init_Dev() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}
	}

	if (options->overlay_1st_en)
	{
		ret = VO_cfg_1stpri_layer_overlay(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("VO_cfg_1stpri_layer_overlay() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}
	}

	if (options->overlay_2nd_en)
	{
		ret = VO_cfg_2ndpri_layer_overlay(options);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_ERROR("VO_cfg_2ndpri_layer_overlay() failed. (%d)\n", ret);
			return VIM_FAILURE;
		}
	}

	if (2 == options->mode)
	{
		ret = de_hdmi_init(_1080P30_);
		if (0 != ret)
		{
			printf("DE HDMI INIT ERR PLEASE CHECK IIC-2\n");
		}
	}
	else if (3 == options->mode)
	{
		ret = de_hdmi_init(_576P50_);
		if (0 != ret)
		{
			printf("DE HDMI INIT ERR PLEASE CHECK IIC-2\n");
		}
	}
	else if (4 == options->mode) ///< 
	{
		ret = de_hdmi_init(_720P60_);
		if (0 != ret)
		{
			printf("DE HDMI INIT ERR PLEASE CHECK IIC-2\n");
		}
	}
	VIM_MPI_VO_BinderIsThrowFrame(options->isThrowFrame);

	VIM_MPI_VO_SetIRQ(g_DevVo, 1);

	SAMPLE_LEAVE();

	return ret;
}

void cutYuv(int yuvType, unsigned char *tarYuv, unsigned char *srcYuv, int startW,
			int startH, int cutW, int cutH, int srcW, int srcH)
{
	int i;
	int j = 0;
	int k = 0;
	unsigned char *tmpY = 0;
	unsigned char *tmpUV = 0;

	printf("cut: startw %d, starth %d srcW%d,srcH %d  cutW%d  cutH%d\n", startW, startH, srcW, srcH, cutW, cutH);
	switch (yuvType)
	{
	case 0:
	case 2:
		tmpY = (unsigned char *)malloc(cutW * cutH);
		tmpUV = (unsigned char *)malloc(cutW * cutH / 1);

		for (i = startH; i < cutH + startH; i++)
		{
			memcpy(tmpY + j * cutW, srcYuv + startW + i * srcW, cutW);
			j++;
		}
		for (i = (startH); i < (cutH + startH) / 1; i++)
		{
			memcpy(tmpUV + k * cutW, srcYuv + startW + srcW * srcH + i * srcW, cutW);
			k++;
		}

		memcpy(tarYuv, tmpY, cutW * cutH);
		memcpy(tarYuv + cutW * cutH, tmpUV, cutW * cutH / 1);
		free(tmpY);
		free(tmpUV);
		break;
	case 1:
	case 3:
		tmpY = (unsigned char *)malloc(cutW * cutH);
		tmpUV = (unsigned char *)malloc(cutW * cutH / 2);

		for (i = startH; i < cutH + startH; i++)
		{
			memcpy(tmpY + j * cutW, srcYuv + startW + i * srcW, cutW);
			j++;
		}
		for (i = (startH / 2); i < (cutH + startH) / 2; i++)
		{
			memcpy(tmpUV + k * cutW, srcYuv + startW + srcW * srcH + i * srcW, cutW);
			k++;
		}

		memcpy(tarYuv, tmpY, cutW * cutH);
		memcpy(tarYuv + cutW * cutH, tmpUV, cutW * cutH / 2);
		free(tmpY);
		free(tmpUV);
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		tmpY = (unsigned char *)malloc(cutW * cutH * 2);

		for (i = startH; i < (cutH + startH); i++)
		{
			memcpy(tmpY + j * cutW * 2, srcYuv + startW + i * srcW * 2, cutW * 2);
			j++;
		}
		memcpy(tarYuv, tmpY, (cutW * cutH) * 2);
		free(tmpY);
		break;
	default:
		SAMPLE_ERROR("unsupport yuvType (%d)\n", yuvType);
		break;
	}
}

char test_pic[MAX_SLICE_CNT][MAX_SLICE_CNT][MAX_FIFO][50] = {0};

int get_chnl_cnt(void)
{
	int chnl_cnt = 0;
	int i;

	for (i = 0; i < g_block_cfg.slice_cnt; i++)
	{
		chnl_cnt += g_block_cfg.slice_list[i].chnl_cnt;
	}

	return chnl_cnt;
}

int get_fifo_size(void)
{
	return g_block_cfg.fifo_size;
}

void get_mulblock_cfg(VO_MULMODE VoMulMode, VO_MUL_BLOCK_S *mb_cfg)
{
	int i;
	int j;
	int k = 0;

	mb_cfg->mul_mode = VoMulMode;
	mb_cfg->slice_number = g_block_cfg.slice_cnt;
	for (i = 0; i < MAX_SLICE_CNT; i++)
	{
		mb_cfg->slice_cnt[i] = g_block_cfg.slice_list[i].chnl_cnt;
	}

	for (i = 0; i < MAX_TOP_Y; i++)
	{
		mb_cfg->top_y[i] = g_block_cfg.slice_list[i + 1].slice_y;
	}

	for (i = 0; i < g_block_cfg.slice_cnt; i++)
	{
		for (j = 0; j < g_block_cfg.slice_list[i].chnl_cnt; j++)
		{
			mb_cfg->image_width[k] = g_block_cfg.slice_list[i].chnl_list[j].chnl_w;
			k++;
		}
	}
}

VIM_S32 VO_Init_Enter(vo_options argc)
{
	VIM_S32 ret = 0;
	test_options options;
	mptxCfg miptCfg;

	SAMPLE_ENTER();

	memset(&options, 0, sizeof(test_options));

	ret = test_vi_opt_parser(argc, &options, &miptCfg);
	if (ret)
	{
		SAMPLE_ERROR("Failed to parse command line options. (ret = %d)\n", ret);
		return ret;
	}

	VIM_MPI_VO_SetIsUseVirAddr(0); ///< 不使用虚拟地址
	ret = VO_Setup(&options);
	if (ret)
	{
		SAMPLE_ERROR("failed, ret = %d\n", ret);
	}
	if(NOBINDER == argc.binderType)
	{
		VIM_MPI_VO_IsUseBinder(0, argc.multi_block, argc.binderType);
	}
	else
	{
		VIM_MPI_VO_IsUseBinder(1, argc.multi_block, argc.binderType);
	}
	
	if (5 == argc.mode || 6 == argc.mode || 7 == argc.mode || 9 == argc.mode || 10 == argc.mode)
	{
		VIM_VO_MPTX_Start(&miptCfg);
	}
	SAMPLE_PRT("%s OK\n", __func__);

	return ret;
}
