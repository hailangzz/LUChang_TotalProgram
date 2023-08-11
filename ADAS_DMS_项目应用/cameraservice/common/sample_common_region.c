#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "vim_type.h"
#include "mpi_region.h"
#include "sample_comm_string.h"
#include "vim_comm_region.h"
#include "sample_common_region.h"
#include "loadbmp.h"

#define RGN_RGB888_RED              0x00FF0000
#define RGN_RGB888_BLUE             0x000000FF
#define RGN_RGB888_GREEN			0x0000FF00
#define RGN_RGB888_YELLOW			0x00FFFF00
#define RGN_RGB888_BLACK			0x00000000
#define RGN_RGB888_WRITE			0x00FFFFFF
#define RGN_RGB888_PURPLE			0x009900FF
#define RGN_RGB888_PINK             0x00FFCCFF
#define RGN_RGB888_SKYBLUE          0x0033FFFF

typedef struct vimSAMPLE_RGN_INFO_S
{
    RGN_HANDLE Handle;
    VIM_S32 s32RgnNum;
    VIM_S32 Channel;
    VIM_S32 GrpId;
} SAMPLE_RGN_INFO_S;

SAMPLE_RGN_INFO_S stRgnAttrInfo;
pthread_t g_stVippRgnThread = 0;
VIM_BOOL bExit = VIM_FALSE;
static VIM_S32 loadfont = 1;

#define SAMPLE_RGN_NOT_PASS(err)\
    do {\
        printf("\033[0;31mtest case <%s>not pass at line:%d err:%x\033[0;39m\n",\
                __func__, __LINE__, err);\
    } while (0)

#define SAMPLE_RETURN_PRT(s, ret)\
    do {\
        printf("\n\033[0;31mtest <%s> return is:0x%08x\033[0;39m\n",\
                s, ret);\
    } while (0)

#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s]-%d:", __func__, __LINE__);	\
		printf(fmt);        \
    } while (0)

#define RGB(r, g, b)            ((((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))
#undef RGB_R
#define RGB_R(c)                (((c) & 0xff0000) >> 16)
#undef RGB_G
#define RGB_G(c)                (((c) & 0xff00) >> 8)
#undef RGB_B
#define RGB_B(c)                ((c) & 0xff)
#define RGN_YUV10BIT(y, u, v)   ((v << 22) | (u << 12) | (y << 2))

VIM_VOID DO_VI_GetRGB_YUV(VIM_U8 r, VIM_U8 g, VIM_U8 b, VIM_U8 *py, VIM_U8 *pcb, VIM_U8 *pcr)
{
    /* Y */
    *py = (VIM_U8)(((r * 66 + g * 129 + b * 25) >> 8) + 16);

    /* Cb */
    *pcb = (VIM_U8)((((b * 112 - r * 38) - g * 74) >> 8) + 128);

    /* Cr */
    *pcr = (VIM_U8)((((r * 112 - g * 94) - b * 18) >> 8) + 128);

}

VIM_U32 DO_VI_GetRGB_YUV_10Bit(VIM_U32 u32RGBColor)
{
    VIM_U8 y,u,v;

    /*--------------------------------------------------
    [31:30]	Reserved
    [29:20]	color_v	10'h0	region color v parameter
    [19:10]	color_u	10'h0	region color u parameter
    [9:0]		color_y	10'h0	region color y parameter
    ---------------------------------------------------*/
    DO_VI_GetRGB_YUV(RGB_R(u32RGBColor), RGB_G(u32RGBColor), RGB_B(u32RGBColor), &y, &u, &v);

    return RGN_YUV10BIT(y, u, v);
}

VIM_VOID SAMPLE_RGN_Init(VIM_S32 GrpId)
{
	VIM_MPI_RGN_Init(GrpId);
}

VIM_VOID SAMPLE_RGN_Exit(VIM_S32 GrpId)
{
    VIM_S32 u32FontSize=2;
	VIM_MPI_RGN_Exit(GrpId);
    SAMPLE_RGN_ReleaseFont(u32FontSize);
	loadfont=1;
}

VIM_S32 SAMPLE_RGN_CoverChannelConfig(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32PicWidth, VIM_U32 u32PicHeight)
{
    VIM_S32 s32Ret = 0;
    RGN_CHN_CONFIG_S stChnConfig;

	stChnConfig.enType = COVER_RGN;
	s32Ret =VIM_MPI_RGN_GetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (s32Ret != VIM_SUCCESS)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	stChnConfig.enType = COVER_RGN;
	stChnConfig.ImgSize.u32Width = u32PicWidth;
	stChnConfig.ImgSize.u32Height = u32PicHeight;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	return VIM_SUCCESS;
}

VIM_S32 SAMPLE_RGN_OSDChannelConfig(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32PicWidth, VIM_U32 u32PicHeight)
{
    VIM_S32 s32Ret = 0;
    RGN_CHN_CONFIG_S stChnConfig;

	/* osd region need config output image size*/
	stChnConfig.enType = OSD_RGN;
	s32Ret = VIM_MPI_RGN_GetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelConfig", s32Ret);
        SAMPLE_RGN_NOT_PASS(s32Ret);
        return s32Ret;
    }

	stChnConfig.enType = OSD_RGN;
	stChnConfig.ImgSize.u32Width = u32PicWidth;
	stChnConfig.ImgSize.u32Height = u32PicHeight;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
        SAMPLE_RGN_NOT_PASS(s32Ret);
        return s32Ret;
    }

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_RGN_SetCoverDefaultAttr(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 s32Ret;
	COVER_ATTR_S stCvrAttr;
	RGN_CHN_ATTR_S stChnAttr;

	stCvrAttr.u32Color = DO_VI_GetRGB_YUV_10Bit(0x0000FF);//0x1b8ef0a0 --blue
	stCvrAttr.enPixelFmt = PIXEL_FMT_RAW_1BPP;
	stCvrAttr.stRgnOps.bEnable = VIM_TRUE;
	s32Ret = VIM_MPI_RGN_SetCoverDisplayAttr(GrpId, &stCvrAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetCoverDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
	stChnAttr.enType = COVER_RGN;
	s32Ret =VIM_MPI_RGN_GetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (s32Ret != VIM_SUCCESS)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	stChnAttr.uChnAttr.stCrvVi.stRgnOps.bEnable = VIM_TRUE;
	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	return VIM_SUCCESS;
}


VIM_S32 SAMPLE_RGN_SetOsdChnInvertAttr(VIM_S32 GrpId, VIM_S32 ChnId,
                    VIM_U32 u32InvertValue, VIM_U32 u32Width, VIM_U32 u32Height)
{
	VIM_S32 s32Ret;
	RGN_CHN_ATTR_S stChnAttr;

	stChnAttr.enType = OSD_RGN;
	s32Ret = VIM_MPI_RGN_GetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = u32InvertValue & 0x1f;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.stInvColArea.u32Height = u32Height;//64;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.stInvColArea.u32Width = u32Width;//304;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.stInvColPos.s32X = 0;//640;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.stInvColPos.s32Y = 0;//472;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32LowClrMappedVal = 2;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32UpperClrMappedVal = 1;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32NormalClrMappedVal = 3;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32ThreshLow = 0x40;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32ThreshUpper = 0xc0;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.enChgMod = BOTHDO_LUM_THRESH;

	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_RGN_SetOsdDefaultAttr(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 s32Ret;
	RGN_CHN_ATTR_S stChnAttr;

	stChnAttr.enType = OSD_RGN;
    s32Ret = VIM_MPI_RGN_GetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
        return s32Ret;
	}

	stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
	stChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0;
	stChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
	stChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(0xFF0000);//rgb:  r-0xFF0000(0x3bc5a144)
	stChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(0x00FF00);//rgb:  g-0x00FF00(0x08836240)
	stChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(0x0000FF);//rgb:  b-0x0000FF(0x1b8ef0a0)
	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
        return s32Ret;
	}

	return VIM_SUCCESS;
}

VIM_S32 SAMPLE_RGN_DestroyRegion(VIM_S32 GrpId, VIM_S32 Handle)
{
	return VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
}

VIM_S32 SAMPLE_RGN_ShowStringOsd(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
    VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y, VIM_CHAR *Str, VIM_S32 State)
{
	VIM_S32 s32Ret = 0;
	VIM_S32  u32FontSize = 2;
	VIM_CHAR sTestString[128] = " ";
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;

    stRgnAttr.enType = OSD_RGN;
	stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stRgnAttr.stSize.u32Width = u32Width;
	stRgnAttr.stSize.u32Height = u32Height;
	stRgnAttr.stPoint.s32X = s32X;
	stRgnAttr.stPoint.s32Y = s32Y;
	stRgnAttr.bindSate = State;

	s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//叠加区域到指定通道
	s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
	if (s32Ret != VIM_SUCCESS)
	{
		VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
		//SAMPLE_RGN_DestroyRegion(GrpId,Handle)
		return s32Ret;
	}

	//更新区域位图
	s32Ret = VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//字符转位图接口测试
	stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
	stBitmap.u32Width = stRgnAttr.stSize.u32Width;
	stBitmap.u32Height = stRgnAttr.stSize.u32Height;
	stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
	if (stBitmap.pData == NULL) {
		SAMPLE_PRT("malloc memory failed.\n");
		s32Ret = VIM_ERR_RGN_NOMEM;
		return s32Ret;
	}

	memset(stBitmap.pData, 0x00, (stBitmap.u32Width * stBitmap.u32Height * 2 / 8));
	memcpy(sTestString, Str, strlen(Str));

	printf("%s loadfont = %d\n", __func__, loadfont);
	if (loadfont == 1) {
        SAMPLE_RGN_ReleaseFont(u32FontSize);
        SAMPLE_RGN_LoadFont(u32FontSize);
        --loadfont;
        printf("loadfont complete loadfont = %d\n", loadfont);
	}

	SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 3);

	s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
	if (stBitmap.pData != NULL) {
		free(stBitmap.pData);
	}

	//显示region
	s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_ShowRectangle(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
                        VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y)
{
	VIM_S32 s32Ret = 0;
	RGN_ATTR_S stRgnAttr;

    stRgnAttr.enType = OSD_EXT_RECTANGLE_RGN;
	stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stRgnAttr.stSize.u32Width = u32Width;
	stRgnAttr.stSize.u32Height = u32Height;
	stRgnAttr.stPoint.s32X = s32X;
	stRgnAttr.stPoint.s32Y = s32Y;
	stRgnAttr.enRctLine = 1;
	stRgnAttr.u32ClrMappedVal = 3;

	s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//叠加区域到指定通道
	s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
	if (s32Ret != VIM_SUCCESS)
	{
		VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
		return s32Ret;
	}

	//显示region
	s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_ShowMasaic(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
            VIM_S32 u32Width, VIM_S32 u32Height,VIM_S32 s32X,VIM_S32 s32Y,VIM_S32 s32ClrMapval)
{
	VIM_S32 s32Ret = 0;
	RGN_ATTR_S stRgnAttr;

    stRgnAttr.enType = OSD_EXT_MOSAIC_RGN;
	stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stRgnAttr.stSize.u32Width = u32Width;
	stRgnAttr.stSize.u32Height = u32Height;
	stRgnAttr.stPoint.s32X = s32X;
	stRgnAttr.stPoint.s32Y = s32Y;
	stRgnAttr.enRctLine = 1;
	stRgnAttr.u32ClrMappedVal = s32ClrMapval;

	s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//叠加区域到指定通道
	s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId,Handle);
	if (s32Ret != VIM_SUCCESS)
	{
		VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
		return s32Ret;
	}

	//显示region
	s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}


VIM_S32 SAMPLE_RGN_ChgStringOsd(VIM_S32 GrpId, VIM_S32 Handle, VIM_CHAR *Str)
{
	VIM_S32 s32Ret = 0;
	VIM_S32  u32FontSize = 2;
	VIM_CHAR sTestString[128] = " ";
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;

	VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
	stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
	stBitmap.u32Width = stRgnAttr.stSize.u32Width;
	stBitmap.u32Height = stRgnAttr.stSize.u32Height;
	stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
	if (stBitmap.pData == NULL) {
		SAMPLE_PRT("malloc memory failed.\n");
		s32Ret = VIM_ERR_RGN_NOMEM;
		return s32Ret;
	}

	memset(stBitmap.pData, 0x00, (stBitmap.u32Width * stBitmap.u32Height * 2 / 8));
	memcpy(sTestString, Str, strlen(Str));

	if (loadfont == 1) {
			SAMPLE_RGN_ReleaseFont(u32FontSize);
			SAMPLE_RGN_LoadFont(u32FontSize);
			--loadfont;
    }

	SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 3);
	s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId,Handle, &stBitmap);
	if (stBitmap.pData != NULL) {
        free(stBitmap.pData);
    }

	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_AddInvertArea(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 key,
                    VIM_S32 u32Width, VIM_S32 u32Height,VIM_S32 s32X,VIM_S32 s32Y)
{
	VIM_S32 s32Ret = 0;
	RGN_INVRTAREA_ATTR_S stInvrtAttr;

	stInvrtAttr.stInvAreaPos.s32X = s32X;
	stInvrtAttr.stInvAreaPos.s32Y = s32Y;
	stInvrtAttr.stInvAreaSize.u32Width = u32Width;
	stInvrtAttr.stInvAreaSize.u32Height = u32Height;
	stInvrtAttr.alive = 1;

	s32Ret = VIM_MPI_RGN_AddInvertArea(GrpId, ChnId, key, &stInvrtAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("VIM_MPI_RGN_AddInvertArea, s32Ret=0x%08x\n", s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_DelInvertArea(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 key)
{
	VIM_S32 s32Ret = 0;

	s32Ret = VIM_MPI_RGN_DelInvertArea(GrpId, ChnId, key);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("VIM_MPI_RGN_DelInvertArea, s32Ret=0x%08x\n", s32Ret);
    }
	return s32Ret;
}

VIM_S32 SAMPLE_RGN_AddCoverToVipp(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 Handle;
	VIM_S32 s32Ret = 0;
	RGN_CHN_ATTR_S stChnAttr;
	RGN_ATTR_S stRgnAttr;

   	 //配置通道参数image size
    s32Ret = SAMPLE_RGN_CoverChannelConfig(GrpId, ChnId, 640, 384);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_CoverChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//设置显示属性和通道使能
	s32Ret = SAMPLE_RGN_SetCoverDefaultAttr(GrpId, ChnId);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_SetCoverDefaultAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	stChnAttr.enType = COVER_RGN;
	s32Ret =VIM_MPI_RGN_GetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//创建3个cover region，并显示
	for (Handle = 0; Handle < 3; Handle++)
	{
		memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));
		stRgnAttr.enType = COVER_RGN;
		stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_1BPP;
		stRgnAttr.stSize.u32Width = 32;
		stRgnAttr.stSize.u32Height = 32;
		s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		//也可调用创建region接口时直接设置x,y
		stRgnAttr.stPoint.s32X = Handle * 40 + 96;
		stRgnAttr.stPoint.s32Y = Handle * 40;
        s32Ret = VIM_MPI_RGN_SetRegionAttr(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetRegionAttr", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		//叠加区域到指定通道
		s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
		if (s32Ret != VIM_SUCCESS)
		{
			//销毁region
			VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
			return s32Ret;
		}

		s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }
	}

	return 0;
}

VIM_VOID *SAMPLE_RGN_VippStringRgnChgPoint(void *pParam)
{
	VIM_S32 s32Ret = VIM_SUCCESS;
	VIM_S32 s32RgnNum;
	VIM_S32 Grpid;
	RGN_HANDLE Handle;
	RGN_HANDLE startHandle;
	RGN_ATTR_S stRgnAttr;
	SAMPLE_RGN_INFO_S *pstRgnAttrInfo = NULL;

	pstRgnAttrInfo = (SAMPLE_RGN_INFO_S *)pParam;
	startHandle = pstRgnAttrInfo->Handle;
	s32RgnNum = pstRgnAttrInfo->s32RgnNum;
	Grpid = pstRgnAttrInfo->GrpId;

	while (VIM_FALSE == bExit)
	{
        for (Handle = startHandle; Handle < (startHandle + s32RgnNum); Handle++)
        {
            if (bExit)
				break;

			/* 1. get region's attr*/
			sleep(1);

            s32Ret = VIM_MPI_RGN_GetRegionAttr(Grpid, Handle, &stRgnAttr);
			if (s32Ret != VIM_SUCCESS)
			{
				SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
				SAMPLE_RGN_NOT_PASS(s32Ret);
			}

			if (Handle % 2) {
				stRgnAttr.stPoint.s32X -= 50;
				stRgnAttr.stPoint.s32Y -= 50;
				if (stRgnAttr.stPoint.s32X <= 0)
					stRgnAttr.stPoint.s32X = 600;
				if (stRgnAttr.stPoint.s32Y <= 0)
					stRgnAttr.stPoint.s32Y = 300;
			} else {
				stRgnAttr.stPoint.s32X += 50;
				stRgnAttr.stPoint.s32Y += 50;
				if (stRgnAttr.stPoint.s32X >= 640)
					stRgnAttr.stPoint.s32X = 0;
				if (stRgnAttr.stPoint.s32Y >= 384)
					stRgnAttr.stPoint.s32Y = 0;
			}

			s32Ret = VIM_MPI_RGN_SetRegionAttr(Grpid, Handle, &stRgnAttr);
		}
	}

	return (VIM_VOID *)s32Ret;
}

VIM_S32 SAMPLE_RGN_AddOsdStringToVipp(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 s32Ret = 0, Handle;
	VIM_S32  u32FontSize = 0;
	VIM_CHAR sTestString[128] = " ";
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;
	RGN_CHN_ATTR_S stChnAttr;

	//配置通道参数（image size）
	s32Ret = SAMPLE_RGN_OSDChannelConfig(GrpId, ChnId, 640, 384);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_OSDChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//设置通道显示属性
	s32Ret = SAMPLE_RGN_SetOsdDefaultAttr(GrpId, ChnId);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_SetOsdDefaultAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	s32Ret = SAMPLE_RGN_SetOsdChnInvertAttr(GrpId,ChnId, 0x5, 640, 384);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_SetOsdChnInvertAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	s32Ret = SAMPLE_RGN_AddInvertArea(GrpId, ChnId, 0, 104, 32, 128, 320);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_AddInvertArea", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	stChnAttr.enType = OSD_RGN;
	s32Ret = VIM_MPI_RGN_GetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	printf("Info: u32Alpha=%d\n", stChnAttr.uChnAttr.stOsdVi.u32Alpha);
	printf("Info: bEnable=%d\n", stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable);
	printf("Info: u32ThreshLow=%d\n", stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32ThreshLow);
	printf("Info: u32ThreshUpper=%d\n", stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32ThreshUpper);
	printf("Info: enChgMod=%d\n", stChnAttr.uChnAttr.stOsdVi.stInvertColor.enChgMod);
	printf("Info: u32InvClrControlVal=%d\n", stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal);
	printf("Info: enChgMod=%d\n", stChnAttr.uChnAttr.stOsdVi.stInvertColor.enChgMod);

	//创建region
	for (Handle = 3; Handle < 5; Handle++)
	{
		stRgnAttr.enType = OSD_RGN;
		stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
		stRgnAttr.stSize.u32Width = 104;
		stRgnAttr.stSize.u32Height = 32;
		stRgnAttr.stPoint.s32X = (Handle - 3) * 128 + 128;
		stRgnAttr.stPoint.s32Y = 320;

		s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		//叠加区域到指定通道
		s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
		if (s32Ret != VIM_SUCCESS)
		{
			VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
			return s32Ret;
		}

		//更新区域位图
		s32Ret = VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		//字符转位图接口测试
		stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
		stBitmap.u32Width = stRgnAttr.stSize.u32Width;
		stBitmap.u32Height = stRgnAttr.stSize.u32Height;
		stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		if (stBitmap.pData == NULL) {
			SAMPLE_PRT("malloc memory failed.\n");
			s32Ret = VIM_ERR_RGN_NOMEM;
			return s32Ret;
		}

		memset(stBitmap.pData, 0x00, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		sprintf(&sTestString[0], "CHANNEL%d:FSL", ChnId);

		//字符串转位图数据接口设置
		//参数1：stBitmap位图结构体指针
		//参数2：字符串数组指针
		//参数3：字符颜色映射值（对应OSD的三种颜色),使用反色时
		//		最好设置为u32NormalClrMappedVal
		//参数4：希望的字体大小（0：ascii8x16.vim.bin，gbk16x16.vim.bin
		//2：ascii16x32.vim.bin gbk32x32.vim.bin  其他：ascii12x24.vim.bin gbk24x24.vim.bin）
		s32Ret = draw_internal_stringTobitmap(&stBitmap, &sTestString[0], 2, u32FontSize);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("draw_internal_stringTobitmap", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
		if (stBitmap.pData != NULL) {
			free(stBitmap.pData);
		}

		//显示region
		s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }
	}

	//开启线程不断改变region的位置
	sleep(7);
	stRgnAttrInfo.Handle = 3;
	stRgnAttrInfo.s32RgnNum = 2;

	stRgnAttrInfo.Channel = ChnId;
	stRgnAttrInfo.GrpId = GrpId;

	pthread_create(&g_stVippRgnThread, NULL, SAMPLE_RGN_VippStringRgnChgPoint, (VIM_VOID *)&stRgnAttrInfo);
	//等待一段时间后修改位图，如果需要不断修改可开启一个线程进行操作
	sleep(6);
	for (Handle = 3; Handle < 5; Handle++)
	{
		VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
		stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
		stBitmap.u32Width = stRgnAttr.stSize.u32Width;
		stBitmap.u32Height = stRgnAttr.stSize.u32Height;
		stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		if (stBitmap.pData == NULL) {
			SAMPLE_PRT("malloc memory failed.\n");
			s32Ret = VIM_ERR_RGN_NOMEM;
			return s32Ret;
		}

		memset(stBitmap.pData, 0x00, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);

		sprintf(&sTestString[0], "chnid%d:%d HD", ChnId, Handle);
		s32Ret = draw_internal_stringTobitmap(&stBitmap, &sTestString[0], 1, u32FontSize);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("draw_internal_stringTobitmap", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetRegionBitMap", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		if (stBitmap.pData != NULL) {
			free(stBitmap.pData);
        }
	}

	//等待25秒后，销毁线程回收资源
	sleep(25);
	bExit = VIM_TRUE;
	if (g_stVippRgnThread)
	{
        pthread_join(g_stVippRgnThread, 0);
        g_stVippRgnThread = 0;
	}
	bExit = VIM_FALSE;
	printf("%s to end!\n", __func__);

	sleep(2);

	//销毁region
	for (Handle = 3; Handle < 5; Handle++)
	{
		s32Ret = SAMPLE_RGN_DestroyRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("SAMPLE_RGN_DestroyRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }
	}

	return 0;
}

VIM_VOID *SAMPLE_RGN_ChgTimeStringOsd(void *arg)
{
	VIM_S32 s32Ret = 0;
	VIM_S32 Gid;
	VIM_S32 Hid;
	SAMPLE_RGN_INFO_S *pstRgnAttrInfo = NULL;
    struct  tm *lt;
	struct timeval tv;
	struct timezone tz;
	struct timeval start,end;
	VIM_CHAR sTestString[128] = " ";

	pstRgnAttrInfo = (SAMPLE_RGN_INFO_S *)arg;
	Gid = pstRgnAttrInfo->GrpId;
	Hid = pstRgnAttrInfo->Handle;

    while (VIM_FALSE == bExit) {
		gettimeofday(&tv, &tz);
        lt =  localtime(&tv.tv_sec);
		sprintf(&sTestString[0], "%d/%d/%d %d:%d:%d.%ld\n",
            lt->tm_year + 1900, lt->tm_mon, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, tv.tv_usec / 1000);
		gettimeofday(&start, NULL);
		s32Ret = SAMPLE_RGN_ChgStringOsd(Gid, Hid, sTestString);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("SAMPLE_RGN_ChgStringOsd", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return (void *)s32Ret;
        }
		gettimeofday(&end, NULL);
	}

	return NULL;
}

VIM_S32 SAMPLE_RGN_AddOsdToDec2Vo(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32Width, VIM_U32 u32Height)
{
	VIM_S32 s32Ret = 0;
	VIM_CHAR sTestString[128] = " ";
    struct  tm *lt;
	struct timeval tv;
	struct timezone tz;

	//配置通道参数（image size）
	s32Ret = SAMPLE_RGN_OSDChannelConfig(GrpId, ChnId, u32Width, u32Height);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_OSDChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//设置通道显示属性
	s32Ret = SAMPLE_RGN_SetOsdDefaultAttr(GrpId, ChnId);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_SetOsdDefaultAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	s32Ret = SAMPLE_RGN_SetOsdChnInvertAttr(GrpId, ChnId, 0x0, u32Width, u32Height);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_SetOsdChnInvertAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//add frame number osd,logo osd,time osd
	sprintf(&sTestString[0], "CHANNEL:%d", ChnId);
	s32Ret = SAMPLE_RGN_ShowStringOsd(GrpId, ChnId, 3,
                    376, 64, 32, 64, sTestString, RGN_BIND_FRAMENOEN);//RGN_BIND_FRAMENOEN
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_ShowStringOsd", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	sprintf(&sTestString[0], "vimicro_AI");
	s32Ret = SAMPLE_RGN_ShowStringOsd(GrpId, ChnId, 4,
                            312, 64, u32Width - 312, 64, sTestString, 0);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_ShowStringOsd", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

    gettimeofday(&tv, &tz);
    lt =  localtime(&tv.tv_sec);

	sprintf(&sTestString[0], "%d/%d/%d %d:%d:%d.%ld\n",
        lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, tv.tv_usec/1000);

	s32Ret = SAMPLE_RGN_ShowStringOsd(GrpId, ChnId, 5,
            304, 64, (u32Width - 304) / 32 * 32, u32Height - 64, sTestString, RGN_BIND_TIMEEN);//RGN_BIND_TIMEEN
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("SAMPLE_RGN_ShowStringOsd", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	printf("SAMPLE_RGN_AddOsdToDectoVo to end!\n");

	return 0;
}

VIM_S32 SAMPLE_RGN_ShowOsdLogo(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
                    VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y)
{
	VIM_S32 s32Ret = 0;
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;

    stRgnAttr.enType = OSD_RGN;
	stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stRgnAttr.stSize.u32Width = u32Width;
	stRgnAttr.stSize.u32Height = u32Height;
	stRgnAttr.stPoint.s32X = s32X;
	stRgnAttr.stPoint.s32Y = s32Y;
	stRgnAttr.bindSate = 0;

	s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
    if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//叠加区域到指定通道
	s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
	if (s32Ret != VIM_SUCCESS)
	{
		VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
		return s32Ret;
	}

	//更新区域位图
	s32Ret = VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//字符转位图接口测试
	stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
	stBitmap.u32Width = stRgnAttr.stSize.u32Width;
	stBitmap.u32Height = stRgnAttr.stSize.u32Height;
	stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
	if (stBitmap.pData == NULL) {
		SAMPLE_PRT("malloc memory failed.\n");
		s32Ret = VIM_ERR_RGN_NOMEM;
		return s32Ret;
	}

	memset(stBitmap.pData, 0x00, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);

	s32Ret = LoadImageSig("logo.bmp", &stBitmap, 3);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("LoadImageSig can transforme SingleBitmap fail!\n");
		return s32Ret;
    }

	printf("Info: LoadImageSig can transforme SingleBitmap out:success\n");
	s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
	if(stBitmap.pData != NULL) {
		free(stBitmap.pData);
	}

	//显示region
	s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_AddLogoToVipp(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
                        VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y)
{
	VIM_S32 s32Ret = 0;
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;

	//open this will can direct operation this function show logo.bmp
	RGN_CHN_CONFIG_S stChnConfig;
	RGN_CHN_ATTR_S stChnAttr;

	//配置通道参数（image size）
	stChnConfig.enType = OSD_RGN;
	stChnConfig.ImgSize.u32Width = 640;
	stChnConfig.ImgSize.u32Height = 384;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//设置通道显示属性
	memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
	stChnAttr.enType = OSD_RGN;
	stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
	stChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0;
	stChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
	stChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(0xFF0000);
	stChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(0x00FF00);
	stChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(0x0000FF);

	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

    stRgnAttr.enType = OSD_RGN;
	stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stRgnAttr.stSize.u32Width = u32Width;
	stRgnAttr.stSize.u32Height = u32Height;
	stRgnAttr.stPoint.s32X = s32X;
	stRgnAttr.stPoint.s32Y = s32Y;
	stRgnAttr.bindSate = 0;
	s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//叠加区域到指定通道
	s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
	if (s32Ret != VIM_SUCCESS)
	{
		VIM_MPI_RGN_DestroyRegion(GrpId,Handle);
		return s32Ret;
	}

	//更新区域位图
	s32Ret = VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
    }

	//字符转位图接口测试
	stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
	stBitmap.u32Width = stRgnAttr.stSize.u32Width;
	stBitmap.u32Height = stRgnAttr.stSize.u32Height;
	stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
	if (stBitmap.pData == NULL) {
		SAMPLE_PRT("malloc memory failed.\n");
		s32Ret = VIM_ERR_RGN_NOMEM;
		return s32Ret;
	}

	memset(stBitmap.pData, 0x00, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);

	//if use this LoadImageSig(), the width and hight of bitmap must multiple for 32(* x 32) !!!!!!!
	s32Ret = LoadImageSig("logo.bmp", &stBitmap, 3);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("LoadImageSig can transforme SingleBitmap fail!\n");
		return s32Ret;
    }

	printf("Info: LoadImageSig can transforme SingleBitmap out:success\n");
	s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
	if (stBitmap.pData != NULL) {
		free(stBitmap.pData);
	}

	//显示region
	s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
    }

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_AddOsdToVipp(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 s32Ret = 0, Handle;
	RGN_ATTR_S stRgnAttr;
	RGN_BITMAP_S stBitmap;
	RGN_CHN_CONFIG_S stChnConfig;
	RGN_CHN_ATTR_S stChnAttr;

	//配置通道参数（image size）
	stChnConfig.enType = OSD_RGN;
	stChnConfig.ImgSize.u32Width = 640;
	stChnConfig.ImgSize.u32Height = 384;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//设置通道显示属性
	memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
	stChnAttr.enType = OSD_RGN;
	stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
	stChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0;
	stChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
	stChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(0xFF0000);
	stChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(0x00FF00);
	stChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(0x0000FF);
	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//创建region
	for (Handle = 5; Handle < 7; Handle++)
	{
		stRgnAttr.enType = OSD_RGN;
		stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
		stRgnAttr.stSize.u32Width = 104;
		stRgnAttr.stSize.u32Height = 32;
		stRgnAttr.stPoint.s32X = (Handle - 5) * 128 + 128;
		stRgnAttr.stPoint.s32Y = 96;
		s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}

		//叠加区域到指定通道
		s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
		if (s32Ret != VIM_SUCCESS)
		{
			VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
			return s32Ret;
		}

		//更新区域位图
		s32Ret = VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_GetRegionAttr", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}

		stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
		stBitmap.u32Width = stRgnAttr.stSize.u32Width;
		stBitmap.u32Height = stRgnAttr.stSize.u32Height;
		stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		if (stBitmap.pData == NULL) {
			SAMPLE_PRT("malloc memory failed.\n");
			s32Ret = VIM_ERR_RGN_NOMEM;
			return s32Ret;
		}

		memset(stBitmap.pData, 0xFF, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
		if (stBitmap.pData != NULL) {
			free(stBitmap.pData);
		}

		//显示region
		s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}
	}

	sleep(15);

	for (Handle = 5; Handle < 7; Handle++)
	{
		VIM_MPI_RGN_GetRegionAttr(GrpId, Handle, &stRgnAttr);
		stBitmap.enPixelFormat = stRgnAttr.enPixelFmt;
		stBitmap.u32Width = stRgnAttr.stSize.u32Width;
		stBitmap.u32Height = stRgnAttr.stSize.u32Height;
		stBitmap.pData = (VIM_VOID *)malloc(stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		if (stBitmap.pData == NULL) {
			SAMPLE_PRT("malloc memory failed.\n");
			s32Ret = VIM_ERR_RGN_NOMEM;
			return s32Ret;
		}

		memset(stBitmap.pData, 0xa5, stBitmap.u32Width * stBitmap.u32Height * 2 / 8);
		s32Ret = VIM_MPI_RGN_SetRegionBitMap(GrpId, Handle, &stBitmap);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetRegionBitMap", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
        }

		if (stBitmap.pData != NULL) {
            free(stBitmap.pData);
        }
	}

	return 0;
}

VIM_S32 SAMPLE_RGN_AddRectangleToVipp(VIM_S32 GrpId, VIM_S32  ChnId)
{
	VIM_S32 s32Ret = 0, Handle;
	RGN_ATTR_S stRgnAttr;
	RGN_CHN_CONFIG_S stChnConfig;
	RGN_CHN_ATTR_S stChnAttr;

	//配置通道参数（image size），和osd一致，是osd的拓展
	stChnConfig.enType = OSD_EXT_RECTANGLE_RGN;
	stChnConfig.ImgSize.u32Width = 640;
	stChnConfig.ImgSize.u32Height = 384;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//设置通道显示属性
	memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
	stChnAttr.enType = OSD_EXT_RECTANGLE_RGN;
	stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
	stChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0;
	stChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
	stChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(0xFF0000);
	stChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(0x00FF00);
	stChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(0x0000FF);
	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//创建region
	for (Handle = 7; Handle < 9; Handle++)
	{
		stRgnAttr.enType = OSD_EXT_RECTANGLE_RGN;
		stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
		stRgnAttr.stSize.u32Width = 104;
		stRgnAttr.stSize.u32Height = 32;
		stRgnAttr.stPoint.s32X = (Handle - 7) * 128 + 128;
		stRgnAttr.stPoint.s32Y = 160;
		stRgnAttr.enRctLine = 0;
		stRgnAttr.u32ClrMappedVal = 1;
		s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}

		//叠加区域到指定通道
		s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
		if (s32Ret != VIM_SUCCESS)
		{
			VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
			return s32Ret;
		}

		//显示region
		s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}
	}

	return 0;
}

VIM_S32 SAMPLE_RGN_AddMasaicToVipp(VIM_S32 GrpId, VIM_S32 ChnId)
{
	VIM_S32 s32Ret = 0, Handle;
	RGN_ATTR_S stRgnAttr;
	RGN_CHN_CONFIG_S stChnConfig;
	RGN_CHN_ATTR_S stChnAttr;

	//配置通道参数（image size）,和osd一致，是osd的拓展
	stChnConfig.enType = OSD_EXT_MOSAIC_RGN;
	stChnConfig.ImgSize.u32Width = 640;
	stChnConfig.ImgSize.u32Height = 384;
	s32Ret = VIM_MPI_RGN_SetChannelConfig(GrpId, ChnId, &stChnConfig);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelConfig", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//设置通道显示属性
	memset(&stChnAttr,0, sizeof(RGN_CHN_ATTR_S));
	stChnAttr.enType = OSD_EXT_MOSAIC_RGN;
	stChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
	stChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
	stChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0;
	stChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
	stChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(0xFF0000);
	stChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(0x00FF00);
	stChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(0x0000FF);
	s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(GrpId, ChnId, &stChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_RETURN_PRT("VIM_MPI_RGN_SetChannelDisplayAttr", s32Ret);
		SAMPLE_RGN_NOT_PASS(s32Ret);
		return s32Ret;
	}

	//创建region
	for (Handle = 9; Handle < 10; Handle++)
	{
		stRgnAttr.enType = OSD_EXT_MOSAIC_RGN;
		stRgnAttr.enPixelFmt = PIXEL_FMT_RAW_2BPP;
		stRgnAttr.stSize.u32Width = 104;
		stRgnAttr.stSize.u32Height = 32;
		stRgnAttr.stPoint.s32X = (Handle - 7) * 128 + 128;
		stRgnAttr.stPoint.s32Y = 240;
		stRgnAttr.enBlkSize = 0;
		stRgnAttr.u32ClrMappedVal = 3;
		s32Ret = VIM_MPI_RGN_CreateRegion(GrpId, Handle, &stRgnAttr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_CreateRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}

		//叠加区域到指定通道
		s32Ret = VIM_MPI_RGN_AttachToChn(GrpId, ChnId, Handle);
		if (s32Ret != VIM_SUCCESS)
		{
			VIM_MPI_RGN_DestroyRegion(GrpId, Handle);
			return s32Ret;
		}

		//显示region
		s32Ret = VIM_MPI_RGN_ShowRegion(GrpId, Handle);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_RETURN_PRT("VIM_MPI_RGN_ShowRegion", s32Ret);
			SAMPLE_RGN_NOT_PASS(s32Ret);
			return s32Ret;
		}
	}

	return 0;
}

