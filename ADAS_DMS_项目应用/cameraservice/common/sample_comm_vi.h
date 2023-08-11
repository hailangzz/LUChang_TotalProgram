#ifndef __SIF_ISP_DP_SAMPLE__H_
#define __SIF_ISP_DP_SAMPLE__H_

#include <errno.h>
#include <unistd.h>
#include <semaphore.h>

#include "vim_type.h"
#include "iniparser.h"
#include "dictionary.h"
#include "ini_ctype.h"
#include "mpi_viu.h"

enum v4l2_mbus_pixelcode {
		V4L2_MBUS_FMT_FIXED                                 = 0x0001,
		V4L2_MBUS_FMT_RGB444_2X8_PADHI_BE                   = 0x1001,
		V4L2_MBUS_FMT_RGB444_2X8_PADHI_LE                   = 0x1002,
		V4L2_MBUS_FMT_RGB555_2X8_PADHI_BE                   = 0x1003,
		V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE                   = 0x1004,
		V4L2_MBUS_FMT_BGR565_2X8_BE                         = 0x1005,
		V4L2_MBUS_FMT_BGR565_2X8_LE                         = 0x1006,
		V4L2_MBUS_FMT_RGB565_2X8_BE                         = 0x1007,
		V4L2_MBUS_FMT_RGB565_2X8_LE                         = 0x1008,
		V4L2_MBUS_FMT_Y8_1X8                                = 0x2001,
		V4L2_MBUS_FMT_UYVY8_1_5X8                           = 0x2002,
		V4L2_MBUS_FMT_VYUY8_1_5X8                           = 0x2003,
		V4L2_MBUS_FMT_YUYV8_1_5X8                           = 0x2004,
		V4L2_MBUS_FMT_YVYU8_1_5X8                           = 0x2005,
		V4L2_MBUS_FMT_UYVY8_2X8                             = 0x2006,
		V4L2_MBUS_FMT_VYUY8_2X8                             = 0x2007,
		V4L2_MBUS_FMT_YUYV8_2X8                             = 0x2008,
		V4L2_MBUS_FMT_YVYU8_2X8                             = 0x2009,
		V4L2_MBUS_FMT_Y10_1X10                              = 0x200a,
		V4L2_MBUS_FMT_YUYV10_2X10                           = 0x200b,
		V4L2_MBUS_FMT_YVYU10_2X10                           = 0x200c,
		V4L2_MBUS_FMT_Y12_1X12                              = 0x2013,
		V4L2_MBUS_FMT_UYVY8_1X16                            = 0x200f,
		V4L2_MBUS_FMT_VYUY8_1X16                            = 0x2010,
		V4L2_MBUS_FMT_YUYV8_1X16                            = 0x2011,
		V4L2_MBUS_FMT_YVYU8_1X16                            = 0x2012,
		V4L2_MBUS_FMT_YUYV10_1X20                           = 0x200d,
		V4L2_MBUS_FMT_YVYU10_1X20                           = 0x200e,
		V4L2_MBUS_FMT_UYVY10                                = 0x2020,
		V4L2_MBUS_FMT_YUV420M                               = 0x2021,
		V4L2_MBUS_FMT_YUV420M_20                            = 0x2022,
		V4L2_MBUS_FMT_YUV420P                               = 0x2023,
		V4L2_MBUS_FMT_YUV420P_20                            = 0x2024,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_BAYER8BIT                = 0x2031,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_BAYER10BIT               = 0x2032,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_BAYER12BIT               = 0x2033,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_BAYER14BIT               = 0x2034,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_BAYER16BIT               = 0x2035,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_ToneMapping_RGB16BIT     = 0x2036,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_toYUV422_YUV444          = 0x2037,
		V4L2_MBUS_FMT_ISP_DEBUG_IN_CE_YUV422                = 0x2038,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_DPC_BAYER16BIT          = 0x2041,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_CFA_RGB16BIT            = 0x2042,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_RgbGains2_RGB16BIT      = 0x2043,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_toYUV422_YUV422         = 0x2044,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_BCHS_10BIT_YUV422       = 0x2045,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_INPUT_BAYER16BIT        = 0x2046,
		V4L2_MBUS_FMT_ISP_DEBUG_OUT_BCHS_8BIT_YUV422        = 0x2047,
		V4L2_MBUS_FMT_SIF_OUT_PIXEL_1BYTE                   = 0x2051,
		V4L2_MBUS_FMT_SIF_OUT_PIXEL_2BYTE                   = 0x2052,
		V4L2_MBUS_FMT_RGBP_601                              = 0x2061,
		V4L2_MBUS_FMT_RGBP_10_601                           = 0x2062,
		V4L2_MBUS_FMT_RGBP_709                              = 0x2063,
		V4L2_MBUS_FMT_RGBP_10_709                           = 0x2064,
		V4L2_MBUS_FMT_SBGGR8_1X8                            = 0x3001,
		V4L2_MBUS_FMT_SGBRG8_1X8                            = 0x3013,
		V4L2_MBUS_FMT_SGRBG8_1X8                            = 0x3002,
		V4L2_MBUS_FMT_SRGGB8_1X8                            = 0x3014,
		V4L2_MBUS_FMT_SBGGR10_DPCM8_1X8                     = 0x300b,
		V4L2_MBUS_FMT_SGBRG10_DPCM8_1X8                     = 0x300c,
		V4L2_MBUS_FMT_SGRBG10_DPCM8_1X8                     = 0x3009,
		V4L2_MBUS_FMT_SRGGB10_DPCM8_1X8                     = 0x300d,
		V4L2_MBUS_FMT_SBGGR10_2X8_PADHI_BE                  = 0x3003,
		V4L2_MBUS_FMT_SBGGR10_2X8_PADHI_LE                  = 0x3004,
		V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_BE                  = 0x3005,
		V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE                  = 0x3006,
		V4L2_MBUS_FMT_SBGGR10_1X10                          = 0x3007,
		V4L2_MBUS_FMT_SGBRG10_1X10                          = 0x300e,
		V4L2_MBUS_FMT_SGRBG10_1X10                          = 0x300a,
		V4L2_MBUS_FMT_SRGGB10_1X10                          = 0x300f,
		V4L2_MBUS_FMT_SBGGR12_1X12                          = 0x3008,
		V4L2_MBUS_FMT_SGBRG12_1X12                          = 0x3010,
		V4L2_MBUS_FMT_SGRBG12_1X12                          = 0x3011,
		V4L2_MBUS_FMT_SRGGB12_1X12                          = 0x3012,

		/* JPEG compressed formats - next is 0x4002 */
		V4L2_MBUS_FMT_JPEG_1X8                              = 0x4001,
		V4L2_MBUS_FMT_LLC                                   = 0x100,

	    //hanzhen add for isp bypass
        V4L2_MBUS_FMT_ISP_BYPASS                            = 0X7001,
};


typedef struct __SAMPLE_SIF_ISP_PARA {
    VIM_BOOL                    bSifEnable[VI_DEV_MAX];

	VIM_U32                     InterFace[VI_DEV_MAX];
	VIM_U32                     Format[VI_DEV_MAX];
	VIM_U32                     Bits[VI_DEV_MAX];
    VIM_U32                     FormatCode[VI_DEV_MAX];
	VIM_U32                     BufferDepth[VI_DEV_MAX];
	VIM_VI_OUTPUT_MODE_E        OutputMode[VI_DEV_MAX];
	VIM_BOOL                    bLLCEnable[VI_DEV_MAX];
	VIM_U32                     CropEnable[VI_DEV_MAX];
	VIM_U32                     CropLeft[VI_DEV_MAX];
	VIM_U32                     CropTop[VI_DEV_MAX];
	VIM_U32                     CropWidth[VI_DEV_MAX];
	VIM_U32                     CropHeight[VI_DEV_MAX];
	VIM_U32                     wdrEnable[VI_DEV_MAX];
	VIM_U32                     wdrMode[VI_DEV_MAX];
	VIM_U32                     wdrFrmNum[VI_DEV_MAX];
	VIM_U32                     SensorNum[VI_DEV_MAX];
	VIM_U32                     SensorWidth[VI_DEV_MAX];
	VIM_U32                     SensorHeight[VI_DEV_MAX];
	VIM_U32                     u32Remap0[VI_DEV_MAX];
	VIM_U32                     u32Remap1[VI_DEV_MAX];
	VIM_U32                     u32Remap2[VI_DEV_MAX];
	VIM_U32                     u32Remap3[VI_DEV_MAX];
	VIM_BOOL                    bIspEnable;
	VIM_CHAR                    *IspSrcFile;
	VIM_U32                     IspWidth;
	VIM_U32                     IspHeight;
	VIM_U32                     IspEnBayer;
	VIM_U32                     Ispoformat;
	VIM_U32                     IspPixelWidth;
	VIM_U32                     IspPixelRate;
	VIM_U32                     IsprMode;
	VIM_U32                     IspwMode;
	VIM_U32                     IsprBuffNum;
	VIM_U32                     IspwBuffNum;
	VIM_U32                     IspWdrMode;
	VIM_U32                     IspInputPhyAddr;
	VIM_U32                     IspOutputPhyAddr;
	VIM_U32                     IspFrameDelayms;
	VIM_U32                     sif_isp_bind;
	VIM_U32                     IspStepMode;
	VIM_U32                     DebugMask;

} SAMPLE_SIF_ISP_PARA_T;

sem_t g_isp_sem;
sem_t g_ispr_sem;

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#undef SAMPLE_PRT
#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d:", __func__, __LINE__);	\
		printf(fmt);									\
    }while (0)

#define ASSERT_RET_RETURN(RET) \
	{\
		if (RET != 0) \
		{\
			SAMPLE_PRT("RUN ERR s32Ret=0x%08x\n", RET); \
			return RET;\
		}\
	}

#define ASSERT_RET_GOTO(RET, LABEL) \
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

#undef SIMPLE_PAUSE
#define SIMPLE_PAUSE(...) ({ \
    int ch1, ch2; \
    printf(__VA_ARGS__); \
    ch1 = getchar(); \
    if (ch1 != '\n' && ch1 != EOF) { \
        while ((ch2 = getchar()) != '\n' && (ch2 != EOF)); \
    } \
    ch1; \
})

#undef ERR_VI
#define ERR_VI(fmt, ...) \
		printf("[ERR] function: %s line:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define CHECK_VI_RET_RETURN(ret) \
    do {\
		VIM_S32 s32RetRet = ret;\
        if (s32RetRet != VIM_SUCCESS)\
        {\
            ERR_VI("main run error %d.\n", s32RetRet); \
            return s32RetRet; \
        } \
    } while (0)

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define	SAMPLE_PRT(fmt...)   							\
	do {												\
		printf("[%s] - %d:", __func__, __LINE__);	\
		printf(fmt);									\
    }while (0)

typedef void *THREAD_HANDLE_S;

#undef THREAD_INVALID_HANDLE
#define THREAD_INVALID_HANDLE   ((THREAD_HANDLE_S) - 1)

typedef int (*FN_THREAD_LOOP)(void *pUserData);

#define ALIGN(x, a)						__ALIGN_MASK(x, ((typeof(x))(a) - 1))
#define __ALIGN_MASK(x, mask)			(((x) + (mask)) &~ (mask))
#define PAGE_SHIFT   					12
#define PAGE_SIZE    					(1 << PAGE_SHIFT)
#define PAGE_ALIGN(addr) 				ALIGN(addr, PAGE_SIZE)

#define MUL_FACT 9
#define ALIGNOF_B 64

#define uint64_t unsigned long long
#define NSEC_PER_SEC 1000000000LL

#define THREAD_STATUS_RUN  1
#define THREAD_STATUS_STOP 0

VIM_S32 SIF_ISP_Init(VIM_CHAR *configPath);
VIM_S32 SIF_ISP_UnInit(void);
VIM_S32 SAMPLE_VI_Init(VIM_CHAR *configPath);
VIM_S32 SAMPLE_VI_UnInit(void);
VIM_S32 SAMPLE_SIFBindISP(VI_DEV ViDev, ISP_DEV IspDev);
VIM_S32 SAMPLE_SIFUnBindISP(VI_DEV ViDev, ISP_DEV IspDev);
VIM_S32 SAMPLE_VI_ISP_WDR_Ctrl(void *param);
VIM_S32 isp_send_trigger(void *param);
VIM_S32 isp_get_trigger(void *param);
VIM_S32 sif_snap_trigger(void *param);
VIM_S32 ispw_mem_init(void *param);
VIM_S32 isp_ldc_reload(void *param);
VIM_S32 isp_alg_mod_ctrl(void *param);

#endif

