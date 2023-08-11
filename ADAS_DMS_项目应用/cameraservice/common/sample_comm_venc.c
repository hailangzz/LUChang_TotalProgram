/******************************************************************************
Some simple Vimicro Vc0768 video encode functions.  Copyright (C), 2018-2028, Vimicro Tech. Co., Ltd.
******************************************************************************
Modification:  2018-2 Created******************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>

#include "sample_venc_cfgParser.h"
#include "mpi_sys.h"
#include "sample_comm_venc.h"
#include "sample_comm_venc_ext.c"
#include "sample_comm_venc_ext.h"

#define chn_num_max 8
#define MAX_COUNT_ROI 15
#define ENABLE_SEGMENT_SAVE
#define ENABLE_TFTP

#define pthread_maxnum chn_num_max
static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara_auto[pthread_maxnum];
static pthread_t gs_VencGetstreamPid[pthread_maxnum];

#define pthread_yuv_maxnum chn_num_max
static SAMPLE_VENC_GETSTREAM_PARA_S gs_YuvPara[pthread_yuv_maxnum];
static pthread_t gs_VencYuvPid[pthread_yuv_maxnum];
static VIM_S8 Venc_init_count;

#define VENC_ALIGN32(_x) (((_x) + 0x1f) & ~0x1f)
#define VENC_ALIGN4(_x) (((_x) + 0x03) & ~0x03)
#define VENC_ALIGN2048(_x) (((_x) + 0x7ff) & ~0x7ff)
#define ENC_FBC50_LUMA_TABLE_SIZE(_w, _h) (VENC_ALIGN2048(VENC_ALIGN32(_w)) * VENC_ALIGN4(_h) / 64)
#define ENC_FBC50_CHROMA_TABLE_SIZE(_w, _h) (VENC_ALIGN2048(VENC_ALIGN32(_w) / 2) * VENC_ALIGN4(_h) / 128)

#define ENC_FBC50_LOSSLESS_LUMA_10BIT_FRAME_SIZE(_w, _h) ((VENC_ALIGN32(_w) + 127) / 128 * VENC_ALIGN4(_h) * 160)
#define ENC_FBC50_LOSSLESS_LUMA_8BIT_FRAME_SIZE(_w, _h) ((VENC_ALIGN32(_w) + 127) / 128 * VENC_ALIGN4(_h) * 128)
#define ENC_FBC50_LOSSLESS_CHROMA_10BIT_FRAME_SIZE(_w, _h) \
	((VENC_ALIGN32(_w) / 2 + 127) / 128 * VENC_ALIGN4(_h) / 2 * 160)
#define ENC_FBC50_LOSSLESS_CHROMA_8BIT_FRAME_SIZE(_w, _h) \
	((VENC_ALIGN32(_w) / 2 + 127) / 128 * VENC_ALIGN4(_h) / 2 * 128)

#define ENC_FBC50_LOSSY_LUMA_FRAME_SIZE(_w, _h, _tx) \
((VENC_ALIGN32(_w) + 127) / 128 * VENC_ALIGN4(_h) * VENC_ALIGN32(_tx))

#define ENC_FBC50_LOSSY_CHROMA_FRAME_SIZE(_w, _h, _tx) \
((VENC_ALIGN32(_w) / 2 + 127) / 128 * VENC_ALIGN4(_h) / 2 * VENC_ALIGN32(_tx))

#define SAMPLE_VENC_UNREFERENCED_PARAMETER(P) \
{                                         \
	(P) = (P);                            \
}

VIM_VOID * SAMPLE_COMM_VENC_Save_Thread(VIM_VOID * p);

/******************************************************************************
 * function : vpss bind venc
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_Vpss_BindVenc(VENC_CHN VeChn, VI_CHN ViChn)
{
	VIM_S32 s32Ret = VIM_SUCCESS;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(VeChn);
	SAMPLE_VENC_UNREFERENCED_PARAMETER(ViChn);

	return s32Ret;
}

/******************************************************************************
 * function : vpss unbind venc
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_Vpss_UnBindVenc(VENC_CHN VeChn, VI_CHN ViChn)
{
	VIM_S32 s32Ret = VIM_SUCCESS;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(VeChn);
	SAMPLE_VENC_UNREFERENCED_PARAMETER(ViChn);

	return s32Ret;
}

/******************************************************************************
 * function : vdec unbind venc
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_Vdec_BindVenc(VDEC_CHN VdChn, VENC_CHN VeChn)
{
	VIM_S32 s32Ret = VIM_SUCCESS;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(VdChn);
	SAMPLE_VENC_UNREFERENCED_PARAMETER(VeChn);

	return s32Ret;
}

/******************************************************************************
 * function : vdec unbind venc
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_Vdec_UnBindVenc(VDEC_CHN VdChn, VENC_CHN VeChn)
{
	VIM_S32 s32Ret = VIM_SUCCESS;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(VdChn);
	SAMPLE_VENC_UNREFERENCED_PARAMETER(VeChn);

	return s32Ret;
}

/******************************************************************************
 * funciton : get file postfix according palyload_type.
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, VIM_CHAR *szFilePostfix)
{
	if (PT_H264 == enPayload)
	{
		strcpy((char *)szFilePostfix, ".h264");
	}
	else if (PT_H265 == enPayload)
	{
		strcpy((char *)szFilePostfix, ".h265");
	}
	else if (PT_JPEG == enPayload)
	{
		strcpy((char *)szFilePostfix, ".jpg");
	}
	else if (PT_MJPEG == enPayload)
	{
		strcpy((char *)szFilePostfix, ".mjp");
	}
	else if (PT_MP4VIDEO == enPayload)
	{
		strcpy((char *)szFilePostfix, ".mp4");
	}
	else if (PT_SVAC == enPayload)
	{
		strcpy((char *)szFilePostfix, ".svac");
	}
	else if (PT_SVAC2 == enPayload)
	{
		strcpy((char *)szFilePostfix, ".svac2");
	}
	else
	{
		SAMPLE_VENC_ERR("payload type err!\n");

		return VIM_FAILURE;
	}
	return VIM_SUCCESS;
}

void byte_swap(VIM_U8 *data, VIM_S32 len)
{
	VIM_U8 temp;
	VIM_S32 i;

	for (i = 0; i < len; i += 2)
	{
		temp = data[i];
		data[i] = data[i + 1];
		data[i + 1] = temp;
	}
}

/******************************************************************************
 * funciton : diff format data conversion
 ******************************************************************************/
void venc_conv_image(VIM_U16 *outbuf,
							VIM_U16 *inbuf,
					 		VIM_S32 pic_width,
					 		VIM_S32 pic_height,
					 		VIM_S32 out_width,
					 		VIM_S32 out_height,
					 		VIM_S32 frm_format)
{
	VIM_S32 x;
	VIM_S32 y;
	VIM_S32 iaddr;
	VIM_S32 oaddr;
	VIM_S32 temp = 0;
	VIM_S32 tbase;

	if (frm_format == YUV_OUTPUT_PLANAR) ///< planar
	{
		///< Y
		iaddr = 0;
		oaddr = 0;
		for (y = 0; y < out_height; y++)
		{
			for (x = 0; x < out_width; x++)
			{
				if (y >= pic_height)
				{
					if (x >= pic_width)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[x + pic_width * (pic_height - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr++;
			}
		}
		///< U
		tbase = pic_width * pic_height;
		iaddr = tbase;
		oaddr = out_width * out_height;
		for (y = 0; y < out_height / 2; y++)
		{
			for (x = 0; x < out_width / 2; x++)
			{
				if (y >= pic_height / 2)
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[tbase + x + (pic_width / 2) * (pic_height / 2 - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr++;
			}
		}
		///< V
		tbase = pic_width * pic_height * 5 / 4;
		iaddr = tbase;
		oaddr = out_width * out_height * 5 / 4;
		for (y = 0; y < out_height / 2; y++)
		{
			for (x = 0; x < out_width / 2; x++)
			{
				if (y >= pic_height / 2)
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[tbase + x + (pic_width / 2) * (pic_height / 2 - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr++;
			}
		}
	}
	else if (frm_format == YUV_OUTPUT_NV12 || frm_format == YUV_OUTPUT_NV21)
	{ ///< NV12, NV21
		///< Y
		iaddr = 0;
		oaddr = 0;
		for (y = 0; y < out_height; y++)
		{
			for (x = 0; x < out_width; x++)
			{
				if (y >= pic_height)
				{
					if (x >= pic_width)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[x + pic_width * (pic_height - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr++;
			}
		}
		///< U
		tbase = (frm_format == YUV_OUTPUT_NV12) ? pic_width * pic_height : pic_width * pic_height * 5 / 4;
		iaddr = tbase;
		oaddr = out_width * out_height;
		for (y = 0; y < out_height / 2; y++)
		{
			for (x = 0; x < out_width / 2; x++)
			{
				if (y >= pic_height / 2)
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[tbase + x + (pic_width / 2) * (pic_height / 2 - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr += 2;
			}
		}
		///< V
		tbase = (frm_format == YUV_OUTPUT_NV12) ? pic_width * pic_height * 5 / 4 : pic_width * pic_height;
		iaddr = tbase;
		oaddr = out_width * out_height + 1;
		for (y = 0; y < out_height / 2; y++)
		{
			for (x = 0; x < out_width / 2; x++)
			{
				if (y >= pic_height / 2)
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[tbase + x + (pic_width / 2) * (pic_height / 2 - 1)];
						outbuf[oaddr] = temp;
					}
				}
				else
				{
					if (x >= pic_width / 2)
						outbuf[oaddr] = temp;
					else
					{
						temp = inbuf[iaddr];
						outbuf[oaddr] = temp;
						iaddr++;
					}
				}
				oaddr += 2;
			}
		}
	}
	else
	{
		SAMPLE_VENC_ERR("Error) Unknown frame format: %d\n", frm_format);
	}
}

/******************************************************************************
 * funciton : convert endian8->endian16(cframe data->ipp cframe output data)
 ******************************************************************************/
VIM_VOID venc_dword_swap(VIM_U8 *data, VIM_S32 len)
{
	VIM_U32 temp;
	VIM_U32 *ptr = (VIM_U32 *)data;
	VIM_S32 i;
	VIM_U8 *ptr_u8 = (VIM_U8 *)data;
	VIM_U8 *ptr4_u8;

	for (i = 0; i < len; i += 8)
	{
		temp = ptr[i / sizeof(VIM_U32)];
		ptr4_u8 = (VIM_U8 *)&temp;
		ptr_u8[i + 0] = ptr_u8[i + 7];
		ptr_u8[i + 1] = ptr_u8[i + 6];
		ptr_u8[i + 2] = ptr_u8[i + 5];
		ptr_u8[i + 3] = ptr_u8[i + 4];
		ptr_u8[i + 4] = ptr4_u8[3];
		ptr_u8[i + 5] = ptr4_u8[2];
		ptr_u8[i + 6] = ptr4_u8[1];
		ptr_u8[i + 7] = ptr4_u8[0];
	}
}

/******************************************************************************
 * funciton : read one frame data of cframe data
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_ReadOneFrame_CframeData(VIM_VOID *p,
																   FILE *fpFile,
																   VIDEO_FRAME_INFO_S *pstFrame)
{
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;
	VIM_U32 lossless = venc_para->bcframe50LosslessEnable;
	VIM_U32 frameSize = 0;
	VIM_U32 tableframeSize = 0;
	VIM_U8 *framebuffer = NULL;
	VIM_S32 s32Ret = VIM_SUCCESS;
	VIM_U32 readsize = 0;

	///< venc_para->bcframeDataType  等于1
	frameSize = pstFrame->stVFrame.u32Size[0];
	tableframeSize = pstFrame->stVFrame.u32Size[1];

	///< add by chenxj for vpss output cframe data test
	if (venc_para->bcframeDataType == 2)
	{
		frameSize = pstFrame->stVFrame.u32Size[0];
		tableframeSize = 0;
		if (lossless == 1)
		{
			frameSize = pstFrame->stVFrame.u32Size[0];
			tableframeSize = pstFrame->stVFrame.u32Size[1];
		}
	}

	framebuffer = pstFrame->stVFrame.pVirAddr[0];
	readsize = frameSize;
	s32Ret = fread(framebuffer, 1, readsize, fpFile);
	if (s32Ret < (VIM_S32)readsize)
	{
		return VIM_FAILURE;
	}

	if (venc_para->bcframeDataType == 1)
	{
		venc_dword_swap(framebuffer, readsize);
	}

	framebuffer = pstFrame->stVFrame.pVirAddr[1];
	readsize = tableframeSize;
	SAMPLE_VENC_PRT(" framebuffer=%p readsize=0x%x At the end of file!!!\n", framebuffer, readsize);

	if ((venc_para->bcframeDataType == 1) && (venc_para->bcframe50LosslessEnable == 0))
	{ ///< when
		fseek(fpFile, pstFrame->stVFrame.u32Size[1], SEEK_CUR);
	}
	else
	{
		s32Ret = fread(framebuffer, 1, readsize, fpFile);
		if (s32Ret < (VIM_S32)readsize)
		{
			return VIM_FAILURE;
		}

		if (venc_para->bcframeDataType == 1)
		{
			venc_dword_swap(framebuffer, readsize);
		}
	}
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : Yuv data Conversion
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_Data_Conversion(VIM_VOID *p, VIDEO_FRAME_INFO_S *pstFrame, VIM_U16 *inbuf)
{
	VIM_S32 i, x, y;
	VIM_U32 tmp32 = 0;
	VIM_S32 width, height;
	VIM_S32 size;
	VIM_S32 oaddr = 0;
	VIM_U8 *temp = (VIM_U8 *)pstFrame->stVFrame.pVirAddr[0];
	VIM_U16 *out_mem = (VIM_U16 *)pstFrame->stVFrame.pVirAddr[0];
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;
	VIM_U32 out_width, out_height, pxl_format = 0;
	VIM_S32 packedFormat = 0;
	VIM_S32 frm_format = 0;
	static char flag;
	VIM_S32 align_width_y = 0;
	VIM_S32 align_width_cr = 0;
	VIM_U32 lumaStride = 0;
	VIM_U32 ChromaStride = 0;

	switch (venc_para->dst_enFormat)
	{
	case 1: ///< PIXEL_FORMAT_YUV_PLANAR_420
		pxl_format = 0;
		frm_format = 0;
		break;
	case 2: ///< PIXEL_FORMAT_YUV_PLANAR_420_P10
		pxl_format = 6;
		frm_format = 0;
		break;
	case 3: ///< PIXEL_FORMAT_YUV_SEMIPLANAR_420
		pxl_format = 0;
		frm_format = 2; ///< sp
		break;
	case 4: ///< PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10
		pxl_format = 6;
		frm_format = 2; ///< sp
		break;
	default:
		SAMPLE_VENC_ERR("%s:%d Not supported format(%d)\n", __FILE__, __LINE__, venc_para->dst_enFormat);
		return VIM_FAILURE;
	}

	align_width_y = VENC_ALIGN(pstFrame->stVFrame.u32Width, 32);
	align_width_cr = align_width_y / 2;
	out_width = pstFrame->stVFrame.u32Width;
	out_height = pstFrame->stVFrame.u32Height;

	lumaStride = ((align_width_y + 2) / 3) * 4;
	ChromaStride = ((align_width_cr + 2) / 3) * 4;
	///<	printf("u32Stride[0]=%d lumaStride=%d ChromaStride=%d align_width_y=%d\r\n",
	///<		pstFrame->stVFrame.u32Stride[0],lumaStride,ChromaStride,align_width_y);
	out_height = ((out_height + 3) / 4) * 4;
	size = (packedFormat) ? out_width * out_height * 2 : out_width * out_height * 3 / 2;
	///<    printf("out_width=%d  out_height=%d pxl_format=%d frm_format=%d\n ",
	///<		out_width,out_height,pxl_format,frm_format);
	///< 8BIT
	if (pxl_format == 0)
	{
		for (i = 0; i < size; i++)
			temp[oaddr++] = inbuf[i] & 0xff;
	}
	///< 16BIT
	else if ((pxl_format == 1)) ///<||(pxl_format == 2))
	{
		for (i = 0; i < size; i++)
		{
			out_mem[oaddr++] = inbuf[i];
		}
	}
	else if (pxl_format == 5)
	{
		for (i = 0; i < size; i++)
		{
			byte_swap((VIM_U8 *)&inbuf[i], 2);
			out_mem[oaddr] = ((inbuf[i] & 0x3ff) << 6);
			byte_swap((VIM_U8 *)&out_mem[oaddr], 2);
			oaddr++;
		}
	}
	///< 32BIT
	else
	{
		///< Y
		width = (packedFormat) ? out_width * 2 : out_width;
		height = out_height;
		///<		printf("width=%d height=%d align_width_y=%d align_width_cr=%d\n",
		///<			width,height,align_width_y,align_width_cr);
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < align_width_y; x = x + 3)
			{
				if (pxl_format == 2)
				{
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = ((inbuf[x + 0 + width * y] & 0x3ff) << 22);
					else if (x + 2 >= width)
						tmp32 = ((inbuf[x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[x + 1 + width * y] & 0x3ff) << 12);
					else
						tmp32 = ((inbuf[x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[x + 1 + width * y] & 0x3ff) << 12) |
								((inbuf[x + 2 + width * y] & 0x3ff) << 2);
				}
				else if (pxl_format == 6)
				{
					if (flag >= 1)
					{
						printf("inbuf[%d]=0x%x\n", x + 0 + width * y, inbuf[x + 0 + width * y]);
						flag--;
					}
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = (inbuf[x + 0 + width * y] & 0x3ff);
					else if (x + 2 >= width)
						tmp32 = (inbuf[x + 0 + width * y] & 0x3ff) |
								((inbuf[x + 1 + width * y] & 0x3ff) << 10);
					else
						tmp32 = (inbuf[x + 0 + width * y] & 0x3ff) |
								((inbuf[x + 1 + width * y] & 0x3ff) << 10) |
								((inbuf[x + 2 + width * y] & 0x3ff) << 20);
				}
				temp[oaddr++] = (tmp32 >> 0) & 0xff;
				temp[oaddr++] = (tmp32 >> 8) & 0xff;
				temp[oaddr++] = (tmp32 >> 16) & 0xff;
				temp[oaddr++] = (tmp32 >> 24) & 0xff;
			}

			if (pstFrame->stVFrame.u32Stride[0] > lumaStride)
			{
				oaddr += pstFrame->stVFrame.u32Stride[0] - lumaStride;
			}
		}
		if (packedFormat) ///< packed
			return 1;

		width = (frm_format == 2 || frm_format == 3) ? out_width : out_width / 2;
		height = out_height / 2;
		///< Cb
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < align_width_cr; x = x + 3)
			{
				///< printf("x=%d y=%d width=%d height=%d,out_width=%d  out_height=%d\n",
				///< x,y,width,height,out_width,out_height);
				if (pxl_format == 2)
				{
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = ((inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff) << 22);
					else if (x + 2 >= width)
						tmp32 = ((inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[out_width * out_height + x + 1 + width * y] & 0x3ff) << 12);
					else
						tmp32 = ((inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[out_width * out_height + x + 1 + width * y] & 0x3ff) << 12) |
								((inbuf[out_width * out_height + x + 2 + width * y] & 0x3ff) << 2);
				}
				else if (pxl_format == 6)
				{
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = (inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff);
					else if (x + 2 >= width)
						tmp32 = (inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff) |
								((inbuf[out_width * out_height + x + 1 + width * y] & 0x3ff) << 10);
					else
						tmp32 = (inbuf[out_width * out_height + x + 0 + width * y] & 0x3ff) |
								((inbuf[out_width * out_height + x + 1 + width * y] & 0x3ff) << 10) |
								((inbuf[out_width * out_height + x + 2 + width * y] & 0x3ff) << 20);
				}
				temp[oaddr++] = (tmp32 >> 0) & 0xff;
				temp[oaddr++] = (tmp32 >> 8) & 0xff;
				temp[oaddr++] = (tmp32 >> 16) & 0xff;
				temp[oaddr++] = (tmp32 >> 24) & 0xff;
			}
			///<			if((pstFrame->stVFrame.u32Stride[0]/2)>ChromaStride){
			oaddr += VENC_ALIGN(ChromaStride, 16) - ChromaStride;
			///<			}
		}
		if (frm_format == 2 || frm_format == 3)
		{ ///< interleave
			SAMPLE_VENC_ERR("%s:%d Not supported interleave format\n", __func__, __LINE__);
			return 1;
		}

		width = out_width / 2;
		height = out_height / 2;
		///< Cr
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < align_width_cr; x = x + 3)
			{
				if (pxl_format == 2)
				{
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = ((inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff) << 22);
					else if (x + 2 >= width)
						tmp32 = ((inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[out_width * out_height * 5 / 4 + x + 1 + width * y] & 0x3ff) << 12);
					else
						tmp32 = ((inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff) << 22) |
								((inbuf[out_width * out_height * 5 / 4 + x + 1 + width * y] & 0x3ff) << 12) |
								((inbuf[out_width * out_height * 5 / 4 + x + 2 + width * y] & 0x3ff) << 2);
				}
				else if (pxl_format == 6)
				{
					if (x >= width)
						tmp32 = 0;
					else if (x + 1 >= width)
						tmp32 = (inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff);
					else if (x + 2 >= width)
						tmp32 = (inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff) |
								((inbuf[out_width * out_height * 5 / 4 + x + 1 + width * y] & 0x3ff) << 10);
					else
						tmp32 = (inbuf[out_width * out_height * 5 / 4 + x + 0 + width * y] & 0x3ff) |
								((inbuf[out_width * out_height * 5 / 4 + x + 1 + width * y] & 0x3ff) << 10) |
								((inbuf[out_width * out_height * 5 / 4 + x + 2 + width * y] & 0x3ff) << 20);
				}
				temp[oaddr++] = (tmp32 >> 0) & 0xff;
				temp[oaddr++] = (tmp32 >> 8) & 0xff;
				temp[oaddr++] = (tmp32 >> 16) & 0xff;
				temp[oaddr++] = (tmp32 >> 24) & 0xff;
			}

			///<			if((pstFrame->stVFrame.u32Stride[0]/2)>ChromaStride){
			oaddr += VENC_ALIGN(ChromaStride, 16) - ChromaStride;
			///<			}
		}
	}
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : read one  normal frame yuv data to souce frame buf
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_FreadOneframeYuv(
	FILE *fpFile,
	VIDEO_FRAME_INFO_S *pstFrame,
	VIM_U32 picWidth,
	VIM_U32 picHeight,
	PIXEL_FORMAT_E format)
{
	VIM_U8 *framebuffer = NULL;
	VIM_S32 ret = VIM_SUCCESS;
	VIM_U32 readsize = 0;
	VIM_U32 bitdepth = 0;
	VIM_U32 srcStrideY = 0;
	VIM_U32 srcStrideC = 0;
	VIM_U32 cbcrInterleave = 0;
	VIM_U32 i = 0;

	switch (format)
	{
	case PIXEL_FORMAT_YUV_PLANAR_420:
		bitdepth = 8;
		cbcrInterleave = 0;
		break;
	case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
		bitdepth = 8;
		cbcrInterleave = 1;
		break;
	case PIXEL_FORMAT_YUV_PLANAR_420_P10:
		bitdepth = 10;
		cbcrInterleave = 0;
		break;
	case PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10:
		bitdepth = 10;
		cbcrInterleave = 1;
		break;
	default:
		SAMPLE_VENC_PRT("%s:%d Not supported format(%d)\n", __FILE__, __LINE__, format);
		break;
	}

	framebuffer = pstFrame->stVFrame.pVirAddr[0];
	//SAMPLE_VENC_PRT("bitdepth=%d cbcrInterleave=%d  picWidth=%d picHeight=%d u32Stride=%d framebuffer=%p\n",
	//bitdepth,cbcrInterleave,picWidth,picHeight,pstFrame->stVFrame.u32Stride[0],framebuffer);
	if (bitdepth == 8)
	{
		readsize = picWidth;
		if (cbcrInterleave == 0)
		{ ///< planar
			srcStrideY = pstFrame->stVFrame.u32Stride[0];
			///<			srcStrideC = VENC_ALIGN(pstFrame->stVFrame.u32Stride[0]/2,16);
			srcStrideC = pstFrame->stVFrame.u32Stride[0] / 2;

			///< Y
			for (i = 0; i < picHeight; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					return VIM_FAILURE;
				}
				framebuffer += srcStrideY;
			}

			///< U
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize / 2, fpFile);
				if (ret < (VIM_S32)readsize / 2)
				{
					SAMPLE_VENC_WARN(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",
									 ret,
									 readsize,
									 framebuffer);
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}

			///< V
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize / 2, fpFile);
				if (ret < (VIM_S32)readsize / 2)
				{
					SAMPLE_VENC_WARN(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",
									 ret,
									 readsize,
									 framebuffer);
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}
		}
		else if (cbcrInterleave == 1)
		{ ///< semiplanar
			srcStrideY = pstFrame->stVFrame.u32Stride[0];
			srcStrideC = pstFrame->stVFrame.u32Stride[0];

			///< Y
			for (i = 0; i < picHeight; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					return VIM_FAILURE;
				}
				framebuffer += srcStrideY;
			}

			///< UV
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}
		}
	}
	else if (bitdepth == 10)
	{
		if (cbcrInterleave == 0)
		{ ///< planar
			srcStrideY = pstFrame->stVFrame.u32Stride[0];
			srcStrideC = pstFrame->stVFrame.u32Stride[0] / 2;
			readsize = VENC_ALIGN(picWidth, 3) / 3 * 4;

			///< Y
			for (i = 0; i < picHeight; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					return VIM_FAILURE;
				}
				framebuffer += srcStrideY;
			}

			///< U
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize / 2, fpFile);
				if (ret < (VIM_S32)readsize / 2)
				{
					SAMPLE_VENC_WARN(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",
									 ret,
									 readsize,
									 framebuffer);
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}

			///< V
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize / 2, fpFile);
				if (ret < (VIM_S32)readsize / 2)
				{
					SAMPLE_VENC_WARN(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",
									 ret,
									 readsize,
									 framebuffer);
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}
		}
		else if (cbcrInterleave == 1)
		{ ///< semiplanar
			srcStrideY = pstFrame->stVFrame.u32Stride[0];
			srcStrideC = pstFrame->stVFrame.u32Stride[0];
			readsize = VENC_ALIGN(picWidth, 3) / 3 * 4;

			///< Y
			for (i = 0; i < picHeight; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					return VIM_FAILURE;
				}
				framebuffer += srcStrideY;
			}

			///< UV
			for (i = 0; i < picHeight / 2; i++)
			{
				ret = fread(framebuffer, 1, readsize, fpFile);
				if (ret < (VIM_S32)readsize)
				{
					SAMPLE_VENC_WARN(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",
									 ret,
									 readsize,
									 framebuffer);
					return VIM_FAILURE;
				}
				framebuffer += srcStrideC;
			}
		}
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : read one  frame yuv(such as ipp output data) data to souce frame buf
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_FreadOneframeYuvDerect(
			VIM_VOID *p,
			FILE *fpFile,
			VIDEO_FRAME_INFO_S *pstFrame,
			VIM_U8 *inbuf)
{
	VIM_S32 s32Ret;
	VIM_S32 height;
	VIM_U8 *framebuffer = NULL;
	VIM_U32 srcStride = 0;
	VIM_U32 readsize = 0;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(p);

	framebuffer = inbuf;
	srcStride = pstFrame->stVFrame.u32Stride[0];
	height = pstFrame->stVFrame.u32Height;

	readsize = srcStride * height * 3 / 2;

	s32Ret = fread(framebuffer, 1, readsize, fpFile);
	if (s32Ret < (VIM_S32)readsize)
	{
		return VIM_FAILURE;
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : read one  normal frame yuv data ,
 * it need to be conversion,such as the 10bit的data,for exaple 1p2b->3p4b
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_FreadOneframeYuvDerect_Normal(VIM_VOID *p,
															FILE *fpFile,
															VIDEO_FRAME_INFO_S *pstFrame,
															VIM_U8 *inbuf)
{
	VIM_S32 s32Ret;
	VIM_S32 height;
	VIM_U8 *framebuffer = NULL;
	VIM_U32 srcStride = 0;
	VIM_U32 readsize = 0;
	VIM_U32 bitdepth = 0;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(p);

	switch (pstFrame->stVFrame.enPixelFormat)
	{
	case PIXEL_FORMAT_YUV_PLANAR_420:
		bitdepth = 8;
		break;
	case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
		bitdepth = 8;
		break;
	case PIXEL_FORMAT_YUV_PLANAR_420_P10:
		bitdepth = 10;
		break;
	case PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10:
		bitdepth = 10;
		break;
	default:
		SAMPLE_VENC_ERR("%s:%d Not supported format(%d)\n",
						__FILE__,
						__LINE__,
						pstFrame->stVFrame.enPixelFormat);
		break;
	}

	framebuffer = inbuf;
	srcStride = pstFrame->stVFrame.u32Width;
	height = pstFrame->stVFrame.u32Height;

	readsize = srcStride * height * 3 >> 1;
	if (bitdepth == 10)
	{
		readsize *= 2;
	}

	s32Ret = fread(framebuffer, 1, readsize, fpFile);
	if (s32Ret < (VIM_S32)readsize)
	{
		//SAMPLE_VENC_PRT(" ret=%d readsize=%d framebuffer=%p At the end of file!!!\n",ret,readsize,framebuffer);
		return VIM_FAILURE;
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : read  frame  data
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_ReadSouceData(VIM_VOID *p,
													FILE *fpFile,
													VIDEO_FRAME_INFO_S *pstFrame,
													VIM_U16 *inbuf)
{
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;
	VIM_S32 s32Ret;
	VIM_S32 width, height;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(inbuf);

	if (venc_para->bcframe50Enable == 1)
	{
		s32Ret = SAMPLE_COMM_VENC_ReadOneFrame_CframeData(p, fpFile, pstFrame);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_WARN(" ret=%d  At the end of file!!!\n", s32Ret);
			rewind(fpFile);
			s32Ret = SAMPLE_COMM_VENC_ReadOneFrame_CframeData(p, fpFile, pstFrame);
			if (s32Ret != VIM_SUCCESS)
			{
				return VIM_FAILURE;
			}
		}
	}
	else if (venc_para->src_enFormat == 0)
	{ ///< read yuv data and send to vcodec derectly , such as ipp data
		s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuvDerect(p, fpFile, pstFrame, pstFrame->stVFrame.pVirAddr[0]);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_WARN(" ret=%d  At the end of file!!!\n", s32Ret);
			rewind(fpFile);
			s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuvDerect(p, fpFile, pstFrame, pstFrame->stVFrame.pVirAddr[0]);
			if (s32Ret != VIM_SUCCESS)
			{
				return VIM_FAILURE;
			}
		}
	}
	else if (venc_para->src_enFormat == venc_para->dst_enFormat)
	{
		width = pstFrame->stVFrame.u32Width;
		height = pstFrame->stVFrame.u32Height;
		s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuv(fpFile,
												   pstFrame,
												   width,
												   height,
												   pstFrame->stVFrame.enPixelFormat);
		if (s32Ret != VIM_SUCCESS)
		{
			///< SAMPLE_VENC_PRT(" ret=%d  At the end of file!!!\n",s32Ret);
			rewind(fpFile);
			s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuv(fpFile,
													   pstFrame,
													   width,
													   height,
													   pstFrame->stVFrame.enPixelFormat);
			if (s32Ret != VIM_SUCCESS)
			{
				return VIM_FAILURE;
			}
		}
	}
	else if (venc_para->src_enFormat != venc_para->dst_enFormat)
	{
		VIM_U8 *tmpbuf = NULL;

		width = pstFrame->stVFrame.u32Width;
		height = pstFrame->stVFrame.u32Height;
		VIM_U32 srcFrameSize = 0;

		srcFrameSize = VENC_ALIGN(width, 16) * height * 3 / 2;
		if (venc_para->src_enFormat == YUV420P_P10_16bit)
		{
			srcFrameSize *= 2 * 2;
		}

		if (((venc_para->dst_enFormat == YUV420P_P10_32bit) ||
			(venc_para->dst_enFormat == YUV420SP_P10_32bit)) &&
			(venc_para->src_enFormat != YUV420P_P10_16bit))
		{
			SAMPLE_VENC_ERR("src_enFormat=%d dst_enFormat=%d not support\n",
							venc_para->src_enFormat, venc_para->dst_enFormat);
			return VIM_FAILURE;
		}

		tmpbuf = malloc(srcFrameSize);
		if (tmpbuf == NULL)
		{
			return VIM_FAILURE;
		}

		s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuvDerect_Normal(p, fpFile, pstFrame, tmpbuf);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_WARN("ret=%d  At the end of file!!!\n", s32Ret);
			rewind(fpFile);
			s32Ret = SAMPLE_COMM_VENC_FreadOneframeYuvDerect_Normal(p, fpFile, pstFrame, tmpbuf);
			if (s32Ret != VIM_SUCCESS)
			{
				free(tmpbuf);
				return VIM_FAILURE;
			}
		}

		s32Ret = SAMPLE_COMM_VENC_Data_Conversion((VIM_VOID *)pstPara, pstFrame, (VIM_U16 *)tmpbuf);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("ret=%d SAMPLE_COMM_VENC_Data_Conversion failed\n", s32Ret);
		}

		if (tmpbuf)
		{
			free(tmpbuf);
		}
	}
	else
	{
		SAMPLE_VENC_ERR("not support src_enFormat: %d dst_enFormat=%d\n",
						venc_para->src_enFormat,
						venc_para->dst_enFormat);
		return VIM_FAILURE;
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : save  stream
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_Save(FILE *fpFile, VENC_STREAM_S *pstStream)
{
	VIM_U32 i;
	static VIM_U8 save_flag = 1;
	VIM_U32 count = 0;
	VIM_U8 *buf = NULL;

	if (save_flag)
	{
		for (i = 0; i < pstStream->u32PackCount; i++)
		{
			if (pstStream->pstPack[i] && pstStream->pstPack[i]->pu8Addr && pstStream->pstPack[i]->u32Len)
			{
				fwrite(pstStream->pstPack[i]->pu8Addr + pstStream->pstPack[i]->u32Offset,
					   pstStream->pstPack[i]->u32Len, 1, fpFile);

				for (count = 0; count < pstStream->pstPack[i]->u32DataNum; count++)
				{
					buf = pstStream->pstPack[i]->pu8Addr + pstStream->pstPack[i]->stPackInfo[count].u32PackOffset;
					if (!((*buf == 0x0) && (*(buf + 1) == 0x0) && (*(buf + 2) == 0x0) && (*(buf + 3) == 0x01)))
					{
						printf("i = %d pu8Addr=%p u32Offset=0x%x u32Len[0]=0x%x u32DataNum=%d\n",
							   i,
							   pstStream->pstPack[i]->pu8Addr,
							   pstStream->pstPack[i]->u32Offset,
							   pstStream->pstPack[i]->u32Len,
							   pstStream->pstPack[i]->u32DataNum);
						SAMPLE_VENC_ERR("the stream info sometimes is incorrect\n");
					}
				}
			}
			else if (i == 0)
			{
				SAMPLE_VENC_ERR("the stream is abort\n");
			}
		}
	}
	save_flag = 1;
	fflush(fpFile);
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : save stream
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_SaveStream(PAYLOAD_TYPE_E enType, FILE *pFd, VENC_STREAM_S *pstStream)
{
	VIM_S32 s32Ret;

	if (PT_H264 == enType)
	{
		s32Ret = SAMPLE_COMM_VENC_Save(pFd, pstStream);
	}
	else if (PT_MJPEG == enType)
	{
		s32Ret = SAMPLE_COMM_VENC_Save(pFd, pstStream);
	}
	else if (PT_H265 == enType)
	{
		s32Ret = SAMPLE_COMM_VENC_Save(pFd, pstStream);
	}
	else if (PT_SVAC == enType)
	{
		s32Ret = SAMPLE_COMM_VENC_Save(pFd, pstStream);
	}
	else if (PT_SVAC2 == enType)
	{
		s32Ret = SAMPLE_COMM_VENC_Save(pFd, pstStream);
	}
	else
	{
		return VIM_FAILURE;
	}
	return s32Ret;
}

/******************************************************************************
 * funciton : Dynamic modify encorder parameters
 ******************************************************************************/
VIM_U8 SAMPLE_COMM_VENC_Modify_Para(void *venc_para, VIM_S32 ch, VIM_U8 chn_num)
{
	VENC_CHN_ATTR_S pstAttr;
	VIM_S32 s32Ret;
	SAMPLE_ENC_CFG *pEncCfg = venc_para;
	VENC_CHN venc_chn = pEncCfg->VeChn;

	static VIM_BOOL bEnableRoi;

	VIM_U8 i = 0;

	SAMPLE_VENC_PRT("enter ch=%c  chn_num=%d\n ", ch, chn_num);

	if (ch == 's' || ch == EOF)
	{
		VIM_S32 ch2 = 0;

		SAMPLE_VENC_PRT("please enter the gop value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter value=%d chn_num=%d\n", ch2, chn_num);

		for (i = 0; i < chn_num; i++)
		{
			venc_chn = (pEncCfg + i)->VeChn;
			VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);
			if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACCBR)
			{
				SAMPLE_VENC_PRT("the original gop=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32Gop);
				SAMPLE_VENC_PRT("modified gop=%d\n", ch2);
				pstAttr.stRcAttr.stAttrSvacCbr.u32Gop = ch2;
			}
			else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACFIXQP)
			{
				SAMPLE_VENC_PRT("the original gop=%d\n", pstAttr.stRcAttr.stAttrSvacFixQp.u32Gop);
				SAMPLE_VENC_PRT("modified gop=%d\n", ch2);
				pstAttr.stRcAttr.stAttrSvacFixQp.u32Gop = ch2;
			}
			else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
			{
				SAMPLE_VENC_PRT("the original gop=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32Gop);
				SAMPLE_VENC_PRT("modified gop=%d\n", ch2);
				pstAttr.stRcAttr.stAttrH265Cbr.u32Gop = ch2;
			}
			else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265FIXQP)
			{
				SAMPLE_VENC_PRT("the original gop=%d\n", pstAttr.stRcAttr.stAttrH265FixQp.u32Gop);
				SAMPLE_VENC_PRT("modified gop=%d\n", ch2);
				pstAttr.stRcAttr.stAttrH265FixQp.u32Gop = ch2;
			}
			s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
			}
		}
		return 0;
	}

	if (ch == 'a' || ch == EOF)
	{
		VIM_S32 ch2 = 0;
		VIM_S32 ch3 = 0;

		SAMPLE_VENC_PRT("please enter the u32PQp value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32PQp=%d\n", ch2);

		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}

		VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);

		if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACFIXQP)
		{
			SAMPLE_VENC_PRT("the original u32PQp=%d\n", pstAttr.stRcAttr.stAttrSvacFixQp.u32PQp);
			pstAttr.stRcAttr.stAttrSvacFixQp.u32PQp = ch2;
			SAMPLE_VENC_PRT("modified u32PQp=%d\n", pstAttr.stRcAttr.stAttrSvacFixQp.u32PQp);
		}
		else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265FIXQP)
		{
			SAMPLE_VENC_PRT("the original u32PQp=%d\n", pstAttr.stRcAttr.stAttrH265FixQp.u32PQp);
			pstAttr.stRcAttr.stAttrH265FixQp.u32PQp = ch2;
			SAMPLE_VENC_PRT("modified u32PQp=%d\n", pstAttr.stRcAttr.stAttrH265FixQp.u32PQp);
		}
		s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
		}
		return 0;
	}

	if (ch == 'b' || ch == EOF)
	{
		VIM_S32 ch2 = 0;
		VIM_S32 ch3 = 0;

		venc_chn = (pEncCfg + i)->VeChn;
		SAMPLE_VENC_PRT("please enter the u32BitRate(kbit/s) value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter value=%d\n", ch2);

		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}
		VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);

		if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACCBR)
		{
			SAMPLE_VENC_PRT("the original u32BitRate=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32BitRate);
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", ch2);
			pstAttr.stRcAttr.stAttrSvacCbr.u32BitRate = ch2;
			SAMPLE_VENC_PRT("enFormat=%d u8EnableConfigBlRate=%d u32BlRate=%d\n",pstAttr.stVeAttr.enFormat,pstAttr.u8EnableConfigBlRate,pstAttr.u32BlRate);
		}
		else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
		{
			SAMPLE_VENC_PRT("the original u32BitRate=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32BitRate);
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", ch2);
			pstAttr.stRcAttr.stAttrH265Cbr.u32BitRate = ch2;
		}
		s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
		}
		return 0;
	}

	if (ch == 'c' || ch == EOF)
	{
		VIM_S32 ch2 = 0;
		VIM_S32 ch3 = 0;

		SAMPLE_VENC_PRT("please enter the u32MinQp value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32MinQp=%d\n", ch2);
		venc_chn = pEncCfg->VeChn;

		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}
		VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);

		if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACCBR)
		{
			SAMPLE_VENC_PRT("the original u32MinQp=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32MinQp);
			pstAttr.stRcAttr.stAttrSvacCbr.u32MinQp = ch2;
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32MinQp);
			s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
			}
		}
		else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
		{
			SAMPLE_VENC_PRT("the original u32MinQp=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32MinQp);
			pstAttr.stRcAttr.stAttrH265Cbr.u32MinQp = ch2;
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32MinQp);
			s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
			}
		}
		return 0;
	}

	if (ch == 'd' || ch == EOF)
	{
		VIM_S32 ch2 = 0;
		VIM_S32 ch3 = 0;

		SAMPLE_VENC_PRT("please enter the u32MaxQp value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32MaxQp=%d\n", ch2);
		venc_chn = pEncCfg->VeChn;

		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}

		VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);
		if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACCBR)
		{
			SAMPLE_VENC_PRT("the original u32MinQp=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32MaxQp);

			pstAttr.stRcAttr.stAttrSvacCbr.u32MaxQp = ch2;
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", pstAttr.stRcAttr.stAttrSvacCbr.u32MaxQp);
			s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
			}
		}
		else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
		{
			SAMPLE_VENC_PRT("the original u32MinQp=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32MaxQp);
			SAMPLE_VENC_PRT("modified u32BitRate=%d\n", pstAttr.stRcAttr.stAttrH265Cbr.u32MaxQp);
			pstAttr.stRcAttr.stAttrH265Cbr.u32MaxQp = ch2;
			s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
			}
		}
		return 0;
	}

	if (ch == 'f' || ch == EOF)
	{
		VENC_ROI_CFG_S pstVencRoiCfg;
		SAMPLE_ENC_CFG *pEncCfg_chn;

		for (i = 0; i < chn_num; i++)
		{
			venc_chn = (pEncCfg + i)->VeChn;
			pEncCfg_chn = (pEncCfg + i);
			pstVencRoiCfg.u32Index = pEncCfg_chn->u32Index;
			pstVencRoiCfg.bEnable = !bEnableRoi;
			pstVencRoiCfg.bAbsQp = pEncCfg_chn->bAbsQp;
			pstVencRoiCfg.s32Qp = pEncCfg_chn->s32Qp;
			pstVencRoiCfg.s32AvgOp = pEncCfg_chn->s32AvgOp;
			pstVencRoiCfg.s32Quality = pEncCfg_chn->s32Quality;
			pstVencRoiCfg.bBKEnable = pEncCfg_chn->bBKEnable;
			pstVencRoiCfg.stRect.s32X = pEncCfg_chn->s32X;
			pstVencRoiCfg.stRect.s32Y = pEncCfg_chn->s32Y;
			pstVencRoiCfg.stRect.u32Width = pEncCfg_chn->u32Width;
			pstVencRoiCfg.stRect.u32Height = pEncCfg_chn->u32Height;
			s32Ret = VIM_MPI_VENC_SetRoiCfg(venc_chn, &pstVencRoiCfg);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
				return s32Ret;
			}
		}

		SAMPLE_VENC_PRT("pstVencRoiCfg.bEnable=%d\n ", pstVencRoiCfg.bEnable);
		if (!bEnableRoi)
		{
			SAMPLE_VENC_PRT("enable the roi bEnableRoi=%d\n ", bEnableRoi);
			bEnableRoi = 1;
		}
		else
		{
			SAMPLE_VENC_PRT("disable  the roi bEnableRoi=%d\n ", bEnableRoi);
			bEnableRoi = 0;
		}
		return 0;
	}

	if (ch == 'h' || ch == EOF)
	{
		for (i = 0; i < chn_num; i++)
		{
			venc_chn = (pEncCfg + i)->VeChn;
			SAMPLE_VENC_PRT("venc_chn:%d force I\n", venc_chn);
			s32Ret = VIM_MPI_VENC_RequestIDR(venc_chn);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_RequestIDR faild with %#x!\n", s32Ret);
				return s32Ret;
			}
		}
		return 0;
	}

	if (ch == 'l' || ch == EOF)
	{
		VIM_S32 ch2 = 0;
		VIM_S32 ch3 = 0;

		SAMPLE_VENC_PRT("please enter the u32IQp value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32IQp=%d\n", ch2);
		venc_chn = pEncCfg->VeChn;
		VIM_MPI_VENC_GetChnAttr(venc_chn, &pstAttr);

		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}

		if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_SVACFIXQP)
		{
			SAMPLE_VENC_PRT("the original u32IQp=%d\n", pstAttr.stRcAttr.stAttrSvacFixQp.u32IQp);
			SAMPLE_VENC_PRT("modified u32IQp=%d\n", ch2);
			pstAttr.stRcAttr.stAttrSvacFixQp.u32IQp = ch2;
		}
		else if (pstAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265FIXQP)
		{
			SAMPLE_VENC_PRT("the original u32IQp=%d\n", pstAttr.stRcAttr.stAttrH265FixQp.u32IQp);
			SAMPLE_VENC_PRT("modified u32IQp=%d\n", ch2);
			pstAttr.stRcAttr.stAttrH265FixQp.u32IQp = ch2;
		}
		s32Ret = VIM_MPI_VENC_SetChnAttr(venc_chn, &pstAttr);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetChnAttr faild with %#x!\n", s32Ret);
		}
		return 0;
	}

	if (ch == 'm' || ch == EOF)
	{
		VIM_S32 ch3 = 0;

		venc_chn = pEncCfg->VeChn;
		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch3);
			SAMPLE_VENC_PRT("enter value=%d\n", ch3);
			venc_chn = ch3;
		}

		VIM_MPI_VENC_StartRecvPic(venc_chn);

		return 0;
	}

	if (ch == 'n' || ch == EOF)
	{
		VIM_S32 ch2 = 0;

		venc_chn = pEncCfg->VeChn;
		if (chn_num > 1)
		{
			SAMPLE_VENC_PRT("please enter the chn num value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter value=%d\n", ch2);
			venc_chn = ch2;
		}

		VIM_MPI_VENC_StopRecvPic(venc_chn);

		return 0;
	}

	if (ch == 'o' || ch == EOF)
	{
		VIM_U32 yuvfmt = 0;

		for (i = 0; i < chn_num; i++)
		{
			venc_chn = (pEncCfg + i)->VeChn;
			if (strcmp(pEncCfg->enPixelFormat, "420p") == 0)
			{
				yuvfmt = 0;
			}
			else if (strcmp(pEncCfg->enPixelFormat, "420sp") == 0)
			{
				yuvfmt = 1;
			}
			else if (strcmp(pEncCfg->enPixelFormat, "uyvy") == 0)
			{
				yuvfmt = 5;
			}
			else
			{
				SAMPLE_VENC_ERR(" invalid format enPixelFormat=%s!\n", pEncCfg->enPixelFormat);
				return VIM_FAILURE;
			}

			SAMPLE_VENC_PRT("grab  the yuv data from venc yuvfmt=%d\n ", yuvfmt);
			///<			SAMPLE_COMM_Venc_GetYuvFrameData(venc_chn,yuvfmt);
		}
	}

	if (ch == 'p' || ch == EOF)
	{
		VENC_ROI_CFG_S pstVencRoiCfg;
		VIM_S32 ch2 = 0;

		SAMPLE_VENC_PRT("please enter the u32Index value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32Index=%d\n", ch2);
		pstVencRoiCfg.u32Index = ch2;

		SAMPLE_VENC_PRT("please enter bEnable value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter bEnable=%d\n", ch2);
		pstVencRoiCfg.bEnable = ch2;

		if (pstVencRoiCfg.bEnable)
		{
			SAMPLE_VENC_PRT("bAbsQp value defualt is zero\n");
			pstVencRoiCfg.bAbsQp = VIM_FALSE;

			SAMPLE_VENC_PRT("please enter s32Qp value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter s32Qp=%d\n", ch2);
			pstVencRoiCfg.s32Qp = ch2;

			SAMPLE_VENC_PRT("please enter s32AvgOp value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter s32AvgOp=%d\n", ch2);
			pstVencRoiCfg.s32AvgOp = ch2;

			SAMPLE_VENC_PRT("please enter s32Quality value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter s32Quality=%d\n", ch2);
			pstVencRoiCfg.s32Quality = ch2;

			SAMPLE_VENC_PRT("please enter bBKEnable value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter bBKEnable=%d\n", ch2);
			pstVencRoiCfg.bBKEnable = ch2;

			SAMPLE_VENC_PRT("please enter s32X value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter s32X=%d\n", ch2);
			pstVencRoiCfg.stRect.s32X = ch2;

			SAMPLE_VENC_PRT("please enter s32Y value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter s32Y=%d\n", ch2);
			pstVencRoiCfg.stRect.s32Y = ch2;

			SAMPLE_VENC_PRT("please enter u32Width value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter u32Width=%d\n", ch2);
			pstVencRoiCfg.stRect.u32Width = ch2;

			SAMPLE_VENC_PRT("please enter u32Height value(dec)\n ");
			scanf("%d", &ch2);
			SAMPLE_VENC_PRT("enter u32Height=%d\n", ch2);
			pstVencRoiCfg.stRect.u32Height = ch2;
		}

		for (i = 0; i < chn_num; i++)
		{
			venc_chn = (pEncCfg + i)->VeChn;
			s32Ret = VIM_MPI_VENC_SetRoiCfg(venc_chn, &pstVencRoiCfg);
			if (s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_SetSvacConf faild with %#x!\n", s32Ret);
				return s32Ret;
			}
		}

		return 0;
	}

	if (ch == 'r' || ch == EOF)
	{
		VENC_ROI_CFG_S pstVencRoiCfg;
		VIM_S32 ch2 = 0;

		SAMPLE_VENC_PRT("please enter the chn value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter chn=%d\n", ch2);
		venc_chn = ch2;

		SAMPLE_VENC_PRT("please enter u32Index value(dec)\n ");
		scanf("%d", &ch2);
		SAMPLE_VENC_PRT("enter u32Index=%d\n", ch2);
		pstVencRoiCfg.u32Index = ch2;

		s32Ret = VIM_MPI_VENC_GetRoiCfg(venc_chn, pstVencRoiCfg.u32Index, &pstVencRoiCfg);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetSvacConf faild with %#x!\n", s32Ret);
			return s32Ret;
		}

		SAMPLE_VENC_PRT("u32Index=%d\n  bEnable=%d\n  bAbsQp=%d\n  s32Qp=%d\n  s32AvgOp=%d\n  s32Quality=%d\n  bBKEnable=%d\n  s32X=%d\n  s32Y=%d\n  u32Width=%d\n  u32Height=%d\n ",
						pstVencRoiCfg.u32Index,
						pstVencRoiCfg.bEnable,
						pstVencRoiCfg.bAbsQp,
						pstVencRoiCfg.s32Qp,
						pstVencRoiCfg.s32AvgOp,
						pstVencRoiCfg.s32Quality,
						pstVencRoiCfg.bBKEnable,
						pstVencRoiCfg.stRect.s32X,
						pstVencRoiCfg.stRect.s32Y,
						pstVencRoiCfg.stRect.u32Width,
						pstVencRoiCfg.stRect.u32Height);

		return 0;
	}

	if (ch == 'u' || ch == EOF)
	{
		VENC_LONGTERM_REFPERIOD_CFG_S LongTermInfo;

		venc_chn = (pEncCfg + i)->VeChn;

		s32Ret = VIM_MPI_VENC_GetLongTerm(venc_chn, &LongTermInfo);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_GetLongTerm faild with %#x!\n", s32Ret);
			return s32Ret;
		}

		SAMPLE_VENC_PRT("bEnableLongTerm=%d u32AsLongtermPeriod=%d  u32refLongtermPeriod=%d\n ",
						LongTermInfo.bEnableLongTerm,
						LongTermInfo.u32AsLongtermPeriod,
						LongTermInfo.u32refLongtermPeriod);

		return 0;
	}

	return 1;
}

/******************************************************************************
 * funciton : VENC OPEN
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_INIT(SAMPLE_ENC_CFG *venc_cfg_config)
{

	VIM_S32 s32Ret;
	VENC_ATTR_CONFIG venc_attr;

	SAMPLE_VENC_PRT("%s build data: %s	 time: %s\n", __func__, __DATE__, __TIME__);

	if(Venc_init_count == 0){
		InitVencMemLock();
	}

	EnterVencMemLock();
	Venc_init_count++;
	SAMPLE_VENC_PRT("%s VIM_MPI_VENC_Init start Venc_init_count=%d\n", __func__, Venc_init_count);
	if (Venc_init_count == 1)
	{
		SAMPLE_VENC_PRT("%s VIM_MPI_VENC_Init start\n", __func__);
		memset(&venc_attr, 0x0, sizeof(venc_attr));
		venc_attr.u8MaxChnNum = venc_cfg_config->u8MaxChnNum;
		venc_attr.u8FlagCodeAndDecorder = venc_cfg_config->u8FlagCodeAndDecorder;
		venc_attr.u32MaxPicWidth = venc_cfg_config->u32MaxPicWidth;
		venc_attr.u32MaxPicHeight = venc_cfg_config->u32MaxPicHeight;
		venc_attr.u32MaxPixelMode = venc_cfg_config->u32MaxPixelMode;
		venc_attr.u32MaxTargetFrmRate = venc_cfg_config->u32MaxTargetFrmRate;
		if (strlen(venc_cfg_config->VencFwPath))
		{
			venc_attr.VencFwPath = venc_cfg_config->VencFwPath;
		}
		///< venc_attr.VencFwPath="/mnt/test/";
		s32Ret = VIM_MPI_VENC_Init(&venc_attr);
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_Init failed 0x%x\n", s32Ret);
		}
		SAMPLE_VENC_PRT("%s VIM_MPI_VENC_Init exit\n", __func__);
	}

	LeaveVencMemLock();

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : VENC OPEN
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_OPEN(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN_ATTR_S stVencChnAttr;
	VENC_CHN VencChn = venc_cfg_config->VeChn;

	SAMPLE_VENC_PRT("%s build data: %s time: %s\n", __func__, __DATE__, __TIME__);
	SAMPLE_VENC_PRT("enter VencChn=%d!\n", VencChn);
	memset(&stVencChnAttr, 0x0, sizeof(VENC_CHN_ATTR_S));

	/******************************************
	 *step 1:  Create Venc Channel
	 ******************************************/
	stVencChnAttr.stVeAttr.enType = venc_cfg_config->enType;
	stVencChnAttr.stGopAttr.enGopMode = venc_cfg_config->enGopMode;
	switch (stVencChnAttr.stVeAttr.enType)
	{
	case PT_H265:
	{
		VENC_ATTR_H265_S stH265Attr;

		stH265Attr.u32MaxPicWidth = venc_cfg_config->u32Width;
		stH265Attr.u32MaxPicHeight = venc_cfg_config->u32Height;
		stH265Attr.u32PicWidth = venc_cfg_config->u32PicWidth;
		stH265Attr.u32PicHeight = venc_cfg_config->u32PicHeight;
		stH265Attr.u32BufSize = venc_cfg_config->u32BufSize;
		stH265Attr.u32Profile = venc_cfg_config->u32Profile;
		stH265Attr.bByFrame = venc_cfg_config->bByFrame;
		memcpy(&stVencChnAttr.stVeAttr.stAttrH265e, &stH265Attr, sizeof(VENC_ATTR_H265_S));

		if (VENC_RC_MODE_H265CBR == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_H265_CBR_S stH265Cbr;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
			stH265Cbr.u32Gop = venc_cfg_config->u32Gop;
			stH265Cbr.u32StatTime = venc_cfg_config->u32StatTime;
			stH265Cbr.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stH265Cbr.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stH265Cbr.u32BitRate = venc_cfg_config->u32BitRate;
			stH265Cbr.u32MaxQp = venc_cfg_config->u32MaxQp;
			stH265Cbr.u32MinQp = venc_cfg_config->u32MinQp;
			stH265Cbr.u32FluctuateLevel = venc_cfg_config->u32FluctuateLevel; /* average bit rate */
			SAMPLE_VENC_PRT("h265 CBR mode stH265Cbr.u32BitRate= %dKbit/s\n", stH265Cbr.u32BitRate);
			memcpy(&stVencChnAttr.stRcAttr.stAttrH265Cbr, &stH265Cbr, sizeof(VENC_ATTR_H265_CBR_S));
		}
		else if (VENC_RC_MODE_H265FIXQP == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_H265_FIXQP_S stH265FixQp;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
			stH265FixQp.u32Gop = venc_cfg_config->u32Gop;
			stH265FixQp.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stH265FixQp.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stH265FixQp.u32IQp = venc_cfg_config->u32IQp;
			stH265FixQp.u32PQp = venc_cfg_config->u32PQp;
			stH265FixQp.u32BQp = venc_cfg_config->u32BQp;
			memcpy(&stVencChnAttr.stRcAttr.stAttrH265FixQp, &stH265FixQp, sizeof(VENC_ATTR_H265_FIXQP_S));
			SAMPLE_VENC_PRT("PT_H265 FIXQP mode: I frame: %d, P frame: %d\n\r",
				            stH265FixQp.u32IQp,
				            stH265FixQp.u32PQp);
		}
		else
		{
			SAMPLE_VENC_ERR("h265 error invalid encode enRcMode!\n");
			return VIM_FAILURE;
		}
	}

	break;

	case PT_SVAC2:
	{
		VENC_ATTR_SVAC_S stSvacAttr;

		stSvacAttr.u32MaxPicWidth = venc_cfg_config->u32MaxPicWidth;
		stSvacAttr.u32MaxPicHeight = venc_cfg_config->u32MaxPicHeight;
		stSvacAttr.u32PicWidth = venc_cfg_config->u32PicWidth;
		stSvacAttr.u32PicHeight = venc_cfg_config->u32PicHeight;
		stSvacAttr.u32BufSize = venc_cfg_config->u32BufSize;
		stSvacAttr.u32Profile = venc_cfg_config->u32Profile;
		stSvacAttr.bByFrame = venc_cfg_config->bByFrame;
		stSvacAttr.flag_2vdsp = venc_cfg_config->flag_2vdsp;
		memcpy(&stVencChnAttr.stVeAttr.stAttrSvac, &stSvacAttr, sizeof(VENC_ATTR_SVAC_S));

		if (VENC_RC_MODE_SVACCBR == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_SVAC_CBR_S stSvacCbr;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_SVACCBR;
			stSvacCbr.u32Gop = venc_cfg_config->u32Gop;
			stSvacCbr.u32StatTime = venc_cfg_config->u32StatTime;
			stSvacCbr.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stSvacCbr.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stSvacCbr.u32FluctuateLevel = venc_cfg_config->u32FluctuateLevel;
			stSvacCbr.u32MaxQp = venc_cfg_config->u32MaxQp;
			stSvacCbr.u32MinQp = venc_cfg_config->u32MinQp;
			stSvacCbr.specialheader = venc_cfg_config->specialheader;
			stSvacCbr.staicRefPerid = venc_cfg_config->staicRefPerid;
			stSvacCbr.u32BitRate = venc_cfg_config->u32BitRate;
			SAMPLE_VENC_PRT("svac2 CBR mode stSvacCbr.u32BitRate = %dkbit/s\n", stSvacCbr.u32BitRate);
			memcpy(&stVencChnAttr.stRcAttr.stAttrSvacCbr, &stSvacCbr, sizeof(VENC_ATTR_SVAC_CBR_S));
		}
		else if (VENC_RC_MODE_SVACFIXQP == venc_cfg_config->enRcMode)
		{
			VENC_ATTR_SVAC_FIXQP_S stSvacFixQp;

			stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_SVACFIXQP;
			stSvacFixQp.u32Gop = venc_cfg_config->u32Gop;
			stSvacFixQp.u32SrcFrmRate = venc_cfg_config->u32SrcFrmRate;
			stSvacFixQp.fr32TargetFrmRate = venc_cfg_config->fr32TargetFrmRate;
			stSvacFixQp.u32IQp = venc_cfg_config->u32IQp;
			stSvacFixQp.u32PQp = venc_cfg_config->u32PQp;
			stSvacFixQp.u32BQp = venc_cfg_config->u32BQp;
			stSvacFixQp.specialheader = venc_cfg_config->specialheader;
			stSvacFixQp.staicRefPerid = venc_cfg_config->staicRefPerid;
			SAMPLE_VENC_PRT("FIXQP mode: I frame: %d, P frame: %d\n\r", stSvacFixQp.u32IQp, stSvacFixQp.u32PQp);
			memcpy(&stVencChnAttr.stRcAttr.stAttrSvacFixQp, &stSvacFixQp, sizeof(VENC_ATTR_SVAC_FIXQP_S));
		}
		else
		{
			SAMPLE_VENC_ERR("svac2 error invalid encode enRcMode!\n");
			return VIM_FAILURE;
		}
	}

	break;

	default:
	{
		SAMPLE_VENC_ERR("error invalid encode type!\n");
		return VIM_ERR_VENC_NOT_SUPPORT;
	}
	}

	stVencChnAttr.pool_id = VB_INVALID_POOLID;
	if (venc_cfg_config->dst_enFormat == 1)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_PLANAR_420;
	}
	else if (venc_cfg_config->dst_enFormat == 2)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_PLANAR_420_P10;
	}
	else if (venc_cfg_config->dst_enFormat == 3)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	}
	else if (venc_cfg_config->dst_enFormat == 4)
	{
		stVencChnAttr.stVeAttr.enFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10;
	}
	else
	{
		SAMPLE_VENC_ERR(" invalid format enPixelFormat=%s u32PixelMode=%d\n",
						venc_cfg_config->enPixelFormat, venc_cfg_config->u32PixelMode);
		return VIM_FAILURE;
	}

	SAMPLE_VENC_PRT("enFormat=%d u8EnableConfigBlRate=%d u32BlRate=%d\n", stVencChnAttr.stVeAttr.enFormat,stVencChnAttr.u8EnableConfigBlRate,stVencChnAttr.u32BlRate);
	s32Ret = VIM_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
		return s32Ret;
	}

	/******************************************
	 *step 2:  Set Venc ConfPara
	 ******************************************/
	VENC_EXT_PARAM_S vencExtParat;

	memset(&vencExtParat, 0x0, sizeof(VENC_EXT_PARAM_S));
	vencExtParat.u32BFrame = venc_cfg_config->bByFrame;
	vencExtParat.cframe_param.bcframe50Enable = venc_cfg_config->bcframe50Enable;
	vencExtParat.cframe_param.bcframe50LosslessEnable = venc_cfg_config->bcframe50LosslessEnable;
	vencExtParat.cframe_param.cframe50Tx16C = venc_cfg_config->cframe50Tx16C;
	vencExtParat.cframe_param.cframe50Tx16Y = venc_cfg_config->cframe50Tx16Y;
	///<	vencExtParat.Reserved0[0] = 1;
	s32Ret = VIM_MPI_VENC_SetConf(VencChn, &vencExtParat);
	if (s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_SetConf faild with %#x!\n", s32Ret);
		return s32Ret;
	}

	VENC_ROTATION_CFG_S RotationInfo;

	RotationInfo.bEnableRotation = venc_cfg_config->bEnableRotation;
	RotationInfo.u32rotAngle = venc_cfg_config->u32rotAngle;
	RotationInfo.enmirDir = venc_cfg_config->enmirDir;
	s32Ret = VIM_MPI_VENC_SetRotation(VencChn, &RotationInfo);
	if (s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRotation faild with %#x!\n", s32Ret);
		return s32Ret;
	}

	if (venc_cfg_config->bEnableSvc)
	{
		VENC_SVC_PARAM_S pstSvcParam;

		pstSvcParam.bEnableSvc = venc_cfg_config->bEnableSvc;
		pstSvcParam.u8SvcMode = venc_cfg_config->u8SvcMode;
		pstSvcParam.u8SvcRation = venc_cfg_config->u8SvcRation;
		s32Ret = VIM_MPI_VENC_SetSsvc(VencChn, &pstSvcParam);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetSsvc faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	if (venc_cfg_config->bEnableTsvc)
	{
		s32Ret = VIM_MPI_VENC_SetTsvc(VencChn, venc_cfg_config->bEnableTsvc);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetTsvc faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	if (venc_cfg_config->EnRoi)
	{
		s32Ret = VIM_MPI_VENC_SetEnRoi(VencChn, venc_cfg_config->EnRoi);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetEnRoi faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	if (venc_cfg_config->EnSAO)
	{
		s32Ret = VIM_MPI_VENC_SetSao(VencChn, venc_cfg_config->EnSAO);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetSao faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	if (venc_cfg_config->bEnableLongTerm)
	{
		VENC_LONGTERM_REFPERIOD_CFG_S LongTermInfo;

		LongTermInfo.bEnableLongTerm = venc_cfg_config->bEnableLongTerm;
		LongTermInfo.u32AsLongtermPeriod = venc_cfg_config->u32AsLongtermPeriod;
		LongTermInfo.u32refLongtermPeriod = venc_cfg_config->u32refLongtermPeriod;
		s32Ret = VIM_MPI_VENC_SetLongTerm(VencChn, &LongTermInfo);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetLongTerm faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	if (venc_cfg_config->bEnable)
	{
		VENC_ROI_CFG_S pstVencRoiCfg;

		pstVencRoiCfg.u32Index = venc_cfg_config->u32Index;
		pstVencRoiCfg.bEnable = venc_cfg_config->bEnable;
		pstVencRoiCfg.bAbsQp = venc_cfg_config->bAbsQp;
		pstVencRoiCfg.s32Qp = venc_cfg_config->s32Qp;
		pstVencRoiCfg.s32AvgOp = venc_cfg_config->s32AvgOp;
		pstVencRoiCfg.s32Quality = venc_cfg_config->s32Quality;
		pstVencRoiCfg.bBKEnable = venc_cfg_config->bBKEnable;
		pstVencRoiCfg.stRect.s32X = venc_cfg_config->s32X;
		pstVencRoiCfg.stRect.s32Y = venc_cfg_config->s32Y;
		pstVencRoiCfg.stRect.u32Width = venc_cfg_config->u32Width;
		pstVencRoiCfg.stRect.u32Height = venc_cfg_config->u32Height;
		s32Ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &pstVencRoiCfg);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
		}
	}
	SAMPLE_VENC_PRT(" exit VencChn=%d\n", VencChn);
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : VENC START
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_START(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN VencChn = venc_cfg_config->VeChn;
	VIM_BOOL bEnableTsvc = 0;
	///<	VENC_ROI_CFG_S pstVencRoiCfg;

	SAMPLE_VENC_PRT("enter!\n");

	s32Ret = VIM_MPI_VENC_StartRecvPic(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
		return VIM_FAILURE;
	}

	///<	if(venc_cfg_config->bEnable){
	///<		VENC_ROI_CFG_S pstVencRoiCfg;
	///<		pstVencRoiCfg.u32Index = venc_cfg_config->u32Index;
	///<		pstVencRoiCfg.bEnable = venc_cfg_config->bEnable;
	///<		pstVencRoiCfg.bAbsQp = venc_cfg_config->bAbsQp;
	///<		pstVencRoiCfg.s32Qp = venc_cfg_config->s32Qp;
	///<		pstVencRoiCfg.s32AvgOp = venc_cfg_config->s32AvgOp;
	///<		pstVencRoiCfg.s32Quality = venc_cfg_config->s32Quality;
	///<		pstVencRoiCfg.bBKEnable = venc_cfg_config->bBKEnable;
	///<		pstVencRoiCfg.stRect.s32X = venc_cfg_config->s32X;
	///<		pstVencRoiCfg.stRect.s32Y = venc_cfg_config->s32Y;
	///<		pstVencRoiCfg.stRect.u32Width = venc_cfg_config->u32Width;
	///<		pstVencRoiCfg.stRect.u32Height = venc_cfg_config->u32Height;
	///<		s32Ret = VIM_MPI_VENC_SetRoiCfg(VencChn,&pstVencRoiCfg);
	///<		if (s32Ret) {
	///<			SAMPLE_VENC_PRT("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
	///<		}
	///<	}

	VIM_MPI_VENC_GetTsvc(VencChn, &bEnableTsvc);
	SAMPLE_VENC_PRT("bEnableTsvc=%d\n", bEnableTsvc);

	VENC_SVC_PARAM_S pstSvcParam;

	s32Ret = VIM_MPI_VENC_GetSsvc(VencChn, &pstSvcParam);
	if (s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_GetSsvc faild with %#x!\n", s32Ret);
	}

	SAMPLE_VENC_PRT("bEnableSvc=%d  u8SvcMode=%d u8SvcRation=%d\n",
					pstSvcParam.bEnableSvc, pstSvcParam.u8SvcMode, pstSvcParam.u8SvcRation);

	VIM_BOOL bEnable;

	s32Ret = VIM_MPI_VENC_GetEnRoi(VencChn, &bEnable);
	if (s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_SetEnRoi faild with %#x!\n", s32Ret);
		return s32Ret;
	}
	SAMPLE_VENC_PRT("VIM_MPI_VENC_GetEnRoi bEnable=%d\n", bEnable);

	if (venc_cfg_config->EnSAO)
	{
		bEnable = venc_cfg_config->EnSAO;
		s32Ret = VIM_MPI_VENC_GetSao(VencChn, &bEnable);
		if (s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_SetSao faild with %#x!\n", s32Ret);
			return s32Ret;
		}
	}
	SAMPLE_VENC_PRT("VIM_MPI_VENC_GetSao bEnable=%d\n", bEnable);

	SAMPLE_VENC_PRT("exit!\n");

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : VENC STOP
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_STOP(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN VencChn = venc_cfg_config->VeChn;

	SAMPLE_VENC_PRT("enter VencChn=%d!\n", VencChn);

	s32Ret = VIM_MPI_VENC_StopRecvPic(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
		return VIM_FAILURE;
	}

	SAMPLE_VENC_PRT("exit VencChn=%d!\n", VencChn);

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : VENC CLOSE
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_CLOSE(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret;
	VENC_CHN VencChn = venc_cfg_config->VeChn;
	VENC_CHN_ATTR_S pstAttr;

	SAMPLE_VENC_PRT("VencChn:%d enter!\n", VencChn);

	s32Ret = VIM_MPI_VENC_GetChnAttr(VencChn, &pstAttr);
	if (s32Ret)
	{
		SAMPLE_VENC_ERR("\nVIM_MPI_VENC_GetChnAttr failed, s32Ret = 0x%x\n", s32Ret);
		return s32Ret;
	}

	/******************************************
	 *step 1:  Distroy Venc Channel
	 ******************************************/
	s32Ret = VIM_MPI_VENC_DestroyChn(VencChn);
	if (VIM_SUCCESS != s32Ret)
	{
		SAMPLE_VENC_ERR("VIM_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n",
						VencChn, s32Ret);
		return VIM_FAILURE;
	}

	SAMPLE_VENC_PRT("VencChn:%d exit!\n", VencChn);
	return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMM_VENC_EXIT(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VIM_S32 s32Ret = VIM_SUCCESS;

	SAMPLE_VENC_UNREFERENCED_PARAMETER(venc_cfg_config);
	
	EnterVencMemLock();

	SAMPLE_VENC_PRT("%s  start EIXT Venc_init_count=%d\n", __func__, Venc_init_count);
	if (Venc_init_count == 1)
	{
		s32Ret = VIM_MPI_VENC_Exit();
		if (VIM_SUCCESS != s32Ret)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_Exit failed 0x%x\n", s32Ret);
		}
	}
	Venc_init_count--;
	SAMPLE_VENC_PRT("%s  end EXIT Venc_init_count=%d\n", __func__, Venc_init_count);
	LeaveVencMemLock();

	return s32Ret;
}

/******************************************************************************
 * funciton : get stream from each channels and save them ,for the select method
 ******************************************************************************/
VIM_VOID *SAMPLE_COMM_VENC_GetVencStreamProc(VIM_VOID *p)
{
	PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
	VIM_S32 VencFd[VENC_MAX_CHN_NUM];
	VIM_CHAR aszFileName[VENC_MAX_CHN_NUM][128];
	FILE *pFile[VENC_MAX_CHN_NUM] = {NULL};
	VENC_CHN VencChn = 0;
	VIM_U32 stream_count = 0;
	VIM_U32 w = 0;
	VIM_U32 h = 0;
	VIM_U32 cur_count = 0;
	VIM_S32 maxfd = 0;
	VIM_S32 s32ChnTotal = 0;
	SAMPLE_ENC_CFG *pEncCfg = NULL;
	VIM_S32 i = 0;
	static VIM_U32 timeout_num;
	VENC_CHN_ATTR_S stVencChnAttr;
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
	struct timeval TimeoutVal;
	fd_set read_fds;
	char szFilePostfix[10];
	VENC_CHN_STAT_S stStat;
	VENC_STREAM_S stStream;
	VIM_S32 s32Ret;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	s32ChnTotal = pstPara->s32Cnt;
	pEncCfg = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;

	if(NULL == pEncCfg)
		return NULL;
	/******************************************
	 *step 1:  check & prepare save-file & venc-fd
	 ******************************************/
	if (s32ChnTotal >= VENC_MAX_CHN_NUM)
	{
		SAMPLE_VENC_PRT("input count invaild\n");
		return NULL;
	}

	SAMPLE_VENC_PRT("s32ChnTotal = 0x%x!\n", s32ChnTotal);
	for (i = 0; i < s32ChnTotal; i++)
	{
		/* decide the stream file name, and open file to save stream */
		///< VencChn = gs_Vencchn[i];
		VencChn = (pEncCfg + i)->VeChn;
		s32Ret = VIM_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
			return NULL;
		}
		enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

		s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n",
							stVencChnAttr.stVeAttr.enType, s32Ret);
			return NULL;
		}
		SAMPLE_VENC_PRT("enable_nosavefile = 0x%x!\n", (pEncCfg + i)->enable_nosavefile);

		w = stVencChnAttr.stVeAttr.stAttrSvac.u32PicWidth;
		h = stVencChnAttr.stVeAttr.stAttrSvac.u32PicHeight;
		snprintf(aszFileName[i], 128, "/mnt/nfs/stream%d_chn%d_%d_%d_%dbit%s", pstPara->s32Index,
				 VencChn, w, h, (pEncCfg + i)->u32PixelMode, szFilePostfix);
		SAMPLE_VENC_PRT("(pEncCfg+i)->streamname=%s!\n", (pEncCfg + i)->streamname);
		strcpy(aszFileName[i], (pEncCfg + i)->streamname);
		snprintf(aszFileName[i], 128, "%s_index%d%s", aszFileName[i], pstPara->s32Index, szFilePostfix);
		SAMPLE_VENC_PRT("filename=%s!\n", aszFileName[i]);
		if ((pEncCfg + i)->enable_nosavefile == 0)
		{
			pFile[i] = fopen(aszFileName[i], "wb");
			if (!pFile[i])
			{
				SAMPLE_VENC_PRT("open file[%s] failed!\n", aszFileName[i]);
				return NULL;
			}
		}

		VencFd[VencChn] = VIM_MPI_VENC_GetFd(VencChn);
		if (VencFd[VencChn] < 0)
		{
			SAMPLE_VENC_PRT("VIM_MPI_VENC_GetFd failed with %#x!\n", VencFd[VencChn]);
			return NULL;
		}
		if (maxfd <= VencFd[VencChn])
		{
			maxfd = VencFd[VencChn];
		}
	}

	/******************************************
	 *step 2:  Start to get streams of each channel.
	 ******************************************/
	while (VIM_TRUE == pstPara->bThreadStart)
	{
		FD_ZERO(&read_fds);
		for (i = 0; i < s32ChnTotal; i++)
		{
			///< VencChn = gs_Vencchn[i];
			VencChn = (pEncCfg + i)->VeChn;
			FD_SET(VencFd[VencChn], &read_fds);
		}

		TimeoutVal.tv_sec = 2;
		TimeoutVal.tv_usec = 0;
		s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
		if (s32Ret < 0)
		{
			SAMPLE_VENC_ERR("select failed!\n");
			break;
		}
		else if (s32Ret == 0)
		{
			SAMPLE_VENC_PRT("VencChn=%d get venc stream time out, exit thread\n", VencChn);
			timeout_num++;
			if (timeout_num > 2)
			{
				///< SAMPLE_VENC_PRT("VencChn:%d get venc stream time out, hold on\n",VencChn);
				///< while(1);
			}
			continue;
		}
		else
		{
			for (i = 0; i < s32ChnTotal; i++)
			{
				///< VencChn = gs_Vencchn[i];
				VencChn = (pEncCfg + i)->VeChn;
				if (FD_ISSET(VencFd[VencChn], &read_fds))
				{
					/*******************************************************
					 *step 2.1 : query how many packs in one-frame stream.
					 *******************************************************/
					memset(&stStream, 0, sizeof(stStream));
					s32Ret = VIM_MPI_VENC_Query(VencChn, &stStat);
					if (VIM_SUCCESS != s32Ret)
					{
						SAMPLE_VENC_ERR("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", VencChn, s32Ret);
						///< pstPara->bThreadStart = VIM_FALSE;
						///< break;
					}

					if (0 == stStat.u32CurPacks)
					{
						SAMPLE_VENC_ERR("NOTE: Current  frame is NULL!\n");
						continue;
					}
					/*******************************************************
					 *step 2.3 : malloc corresponding number of pack nodes.
					 *******************************************************/
					stStream.pstPack[0] = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
					if (NULL == stStream.pstPack[0])
					{
						SAMPLE_VENC_ERR("malloc stream pack failed!\n");
						break;
					}

					memset(stStream.pstPack[0], 0x0, sizeof(VENC_PACK_S) * stStat.u32CurPacks);

					for (cur_count = 1; cur_count < stStat.u32CurPacks; cur_count++)
					{
						stStream.pstPack[cur_count] = stStream.pstPack[cur_count - 1] + 1;
					}

					///< SAMPLE_VENC_PRT("chn[%d]\n", VencChn);

					/*******************************************************
					 *step 2.4 : call mpi to get one-frame stream
					 *******************************************************/
					stStream.u32PackCount = stStat.u32CurPacks;
					s32Ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, -1);
					if (VIM_SUCCESS != s32Ret)
					{
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						SAMPLE_VENC_ERR("VIM_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
						break;
					}
					stream_count++;

					///<					rate_count += stStream.pstPack[0]->u32Len+stStream.pstPack[1]->u32Len;
					///<					if(!(stream_count%25)){
					///<						printf("rate_count:%d\n", rate_count*8/1024);
					///<						rate_count = 0;
					///<					}
					///<					printf("%d reserved:%d\n", stream_count,stStream.reserved[2]);
					///<					if(!(stream_count%8)){
					///<						printf("%d\n", stream_count);
					///<						SAMPLE_COMM_Venc_GetYuvFrameData(i,0);
					///<					}

					/*******************************************************
					 *step 2.5 : save frame to file
					 *******************************************************/
					if ((pEncCfg + i)->enable_nosavefile == 0)
					{
						if (pFile[i] == NULL)
						{
							pFile[i] = fopen(aszFileName[i], "wb");
							if (!pFile[i])
							{
								SAMPLE_VENC_ERR("open file[%s] failed!\n", aszFileName[i]);
								return NULL;
							}
						}

						s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i], &stStream);
						if (VIM_SUCCESS != s32Ret)
						{
							free(stStream.pstPack[0]);
							stStream.pstPack[0] = NULL;
							SAMPLE_VENC_ERR("save stream failed!\n");
							break;
						}
					}
					/*******************************************************
					 *step 2.6 : release stream
					 *******************************************************/
					s32Ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
					if (VIM_SUCCESS != s32Ret)
					{
						SAMPLE_VENC_ERR("Release stream failed!\n");
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						break;
					}
					/*******************************************************
					 *step 2.7 : free pack nodes
					 *******************************************************/
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
				}
				///<				else{
				///<					SAMPLE_VENC_PRT("the chn;%d has no stream!\n",VencChn);
				///<				}
			}
		}
	}
	/*******************************************************
	 * step 3 : close save-file
	 *******************************************************/
	for (i = 0; i < s32ChnTotal; i++)
	{
		if (((pEncCfg + i)->enable_nosavefile == 0) && pFile[i])
		{
			fclose(pFile[i]);
		}
	}

	SAMPLE_VENC_PRT("SAMPLE_COMM_VENC_GetStream_auto exit\n");
	return NULL;
}

/******************************************************************************
 * funciton : sendframe and get stream from each channels and save stream ,for the poll method
 ******************************************************************************/
VIM_VOID *SAMPLE_COMM_VENC_SendFrame_Getstream_poll(VIM_VOID *p)
{
	VIM_S32 ret = 0;
	VIM_S32 VencFd[12];
	VIM_S32 maxfd = 0;
	struct timeval TimeoutVal;
	fd_set read_fds = {0};
	VENC_STREAM_S stStream;
	VENC_CHN_STAT_S stStat;
	FILE *fp[chn_num_max] = {NULL};
	FILE *fp_in[chn_num_max] = {NULL};
	VIM_S8 filename[256];
	VIM_S8 filename_yuv[256];
	VIM_U32 writeflag[chn_num_max] = {0};
	VIM_U32 i = 0;
	VIDEO_FRAME_INFO_S inputFrame[4];
	VIM_U32 chnmask = 0;
	VIM_S32 readsize[chn_num_max] = {0};
	VENC_CHN_ATTR_S pstAttr;
	VIM_U32 w = 0;
	VIM_U32 h = 0;
	VIM_U8 *file = NULL;
	VIM_S8 szFilePostfix[10];
	VIM_U32 sendframe_num[chn_num_max] = {0};
	VIM_U32 cur_count = 0;
	VIM_U32 s32ChnTotal = 0;
	VIM_S8 EncoderSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_S8 GetbufSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_U32 *framebuffer[chn_num_max];
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = NULL;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;

	s32ChnTotal = pstPara->s32Cnt;

	VIM_U32 chn_count = s32ChnTotal;
	VIM_U32 VencChn = venc_para->VeChn;

	SAMPLE_VENC_PRT("venc_para->chn=%d,venc_para->chn_count=%d\r\n", venc_para->VeChn, chn_count);

	for (i = 0; i < chn_count; i++)
	{
		VencChn = (venc_para + i)->VeChn;
		VIM_MPI_VENC_GetChnAttr(VencChn, &pstAttr);
		w = pstAttr.stVeAttr.stAttrSvac.u32PicWidth;
		h = pstAttr.stVeAttr.stAttrSvac.u32PicHeight;

		switch (0)
		{
		case 0: ///< yuv420p
		case 1: ///< yuv420sp
			readsize[i] = w * h * 8 * 3 / 16;
			break;

		case 4: ///< yuv400
			readsize[i] = w * h * 8 / 8;
			break;

		default:
			readsize[i] = w * h * 8 * 3 / 16;
			break;
		}

		if (venc_para->u32PixelMode == 10)
		{
			readsize[i] = (readsize[i] * 5) >> 2;
		}

		file = (VIM_U8 *)venc_para->souce_filename;
		sprintf((char *)filename_yuv, "%s", file);
		SAMPLE_VENC_PRT("the input file name %s\r\n", filename_yuv);
		fp_in[i] = fopen((const char *)filename_yuv, "rb+");
		if (fp_in[i] == NULL)
		{
			SAMPLE_VENC_PRT(" SAMPLE_VENC_ReadFrame can't open device fp_in file.\n");
			goto exit;
		}
		else
		{
			SAMPLE_VENC_PRT(" open the input file successful \r\n");
		}

		ret = SAMPLE_COMM_VENC_GetFilePostfix((venc_para + i)->enType, (char *)szFilePostfix);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", (venc_para + i)->enType, ret);
			goto exit;
		}

		sprintf((char *)filename, "%s%s", (venc_para + i)->streamname, szFilePostfix);
		fp[i] = fopen((const char *)filename, "wb+");
		if (fp[i] == NULL)
		{
			SAMPLE_VENC_ERR("cann't open the output file filename=%s.\n", filename);
			goto exit;
		}

		chnmask = (1 << (VencChn + 1)) - 1;

		VencFd[VencChn] = VIM_MPI_VENC_GetFd(VencChn);
		if (VencFd[VencChn] < 0)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_GetFd failed with %#x!\n", VencFd[VencChn]);
			return NULL;
		}
		if (maxfd <= VencFd[VencChn])
		{
			maxfd = VencFd[VencChn];
		}

		SAMPLE_VENC_PRT("VIM_MPI_VENC_GetFd  chn%d:%d\n", VencChn, VencFd[VencChn]);
	}

	while (VIM_TRUE == pstPara->bThreadStart)
	{
		FD_ZERO(&read_fds);
		for (i = 0; i < s32ChnTotal; i++)
		{
			VencChn = (venc_para + i)->VeChn;
			FD_SET(VencFd[VencChn], &read_fds);
		}

		for (i = 0; i < chn_count; i++)
		{
			VencChn = (venc_para + i)->VeChn;
			if ((chnmask & (1 << i)) && (sendframe_num[i] < ((VIM_U32)(venc_para + i)->souce_framenum)))
			{
				if (fp_in[i] && ((EncoderSuccessFlag[i] == VIM_SUCCESS)))
				{
					ret = VIM_MPI_VENC_GetEmptyFramebuf(VencChn, &inputFrame[i], 0);
					if (ret != VIM_SUCCESS)
					{
						continue;
					}
					GetbufSuccessFlag[i] = ret;
					framebuffer[i] = inputFrame[i].stVFrame.pVirAddr[0];
					SAMPLE_VENC_PRT(" w=%d h=%d chnmask=%d \r\n", w, h, chnmask);
					ret = SAMPLE_COMM_VENC_FreadOneframeYuv(fp_in[i],
															&inputFrame[i],
															inputFrame[i].stVFrame.u32Width,
															inputFrame[i].stVFrame.u32Height,
															inputFrame[i].stVFrame.enPixelFormat);
					if (ret != VIM_SUCCESS)
					{
						SAMPLE_VENC_WARN("ret=%d readsize=%d sendframe_num[i]=%d framebuffer[i]=%p At the end of file!!!\n",
										 ret, readsize[i], sendframe_num[i], framebuffer[i]);
						///<						chnmask &=(~(1<<i)) ;
						///<						SAMPLE_VENC_PRT(" chnmask=%d i =%d \r\n",chnmask,i);
						///< return NULL;
						///< ret = 0;
						rewind(fp_in[i]);
						ret = SAMPLE_COMM_VENC_FreadOneframeYuv(fp_in[i],
																&inputFrame[i],
																inputFrame[i].stVFrame.u32Width,
																inputFrame[i].stVFrame.u32Height,
																inputFrame[i].stVFrame.enPixelFormat);
						SAMPLE_VENC_WARN(" ret=%d!\n", ret);
					}
				}
				else
				{
					//SAMPLE_VENC_PRT("the fp_in[%d] is Null or one frame buf not be used\n",i);
				}

				SAMPLE_VENC_PRT(" chn_num=%d  sendframe_num[%d]=%d\n", i, i, sendframe_num[i]);
				if ((chnmask & (1 << i)) && (GetbufSuccessFlag[i] == VIM_SUCCESS))
				{
					ret = VIM_MPI_VENC_SendFrame(VencChn, &inputFrame[i], 0);
					if (ret < 0)
					{
						SAMPLE_VENC_ERR("VIM_MPI_VENC_SendFrame vechn[%d] failed with %#x!\n", i, ret);
						EncoderSuccessFlag[i] = VIM_FAILURE;
					}
					else
					{
						sendframe_num[i]++;
						EncoderSuccessFlag[i] = VIM_SUCCESS;
					}
				}
			}
		}

		TimeoutVal.tv_sec = 3;
		TimeoutVal.tv_usec = 0;
		ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
		if (ret < 0)
		{
			SAMPLE_VENC_ERR("select failed!\n");
			break;
		}
		else if (ret == 0)
		{
			SAMPLE_VENC_ERR("get venc stream timeout, exit thread\n");
			///< continue;
		}
		else
		{
			for (i = 0; i < s32ChnTotal; i++)
			{
				VencChn = (venc_para + i)->VeChn;
				if (FD_ISSET(VencFd[i], &read_fds))
				{
					/*******************************************************
					 *step 2.1 : query how many packs in one-frame stream.
					 *******************************************************/
					memset(&stStream, 0, sizeof(stStream));
					ret = VIM_MPI_VENC_Query(VencChn, &stStat);
					if (VIM_SUCCESS != ret)
					{
						SAMPLE_VENC_ERR("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", i, ret);
						break;
					}

					SAMPLE_VENC_PRT("[chn%d] Query info:\n", i);
					///< SAMPLE_VENC_PRT("\tbRegistered:%d\n", stStat.bRegistered);
					///< SAMPLE_VENC_PRT("\tu32LeftPics:%d\n", stStat.u32LeftPics);
					SAMPLE_VENC_PRT("\tu32LeftStreamFrames:%d\n", stStat.u32LeftStreamFrames);
					SAMPLE_VENC_PRT("\tu32CurPacks:%d\n", stStat.u32CurPacks);

					/*******************************************************
					 *step 2.2 : malloc corresponding number of pack nodes.
					 *******************************************************/
					stStream.pstPack[0] = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
					if (NULL == stStream.pstPack[0])
					{
						SAMPLE_VENC_ERR("malloc stream pack failed!\n");
						break;
					}

					for (cur_count = 1; cur_count < stStat.u32CurPacks; cur_count++)
					{
						stStream.pstPack[cur_count] =
							(VENC_PACK_S *)((char *)stStream.pstPack[cur_count - 1] + sizeof(VENC_PACK_S));
					}

					/*******************************************************
					 *step 2.3 : call mpi to get one-frame stream
					 *******************************************************/
					///< stStream.u32PackCount = stStat.u32CurPacks;
					ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, -1);
					if (VIM_SUCCESS != ret)
					{
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						SAMPLE_VENC_ERR("VIM_MPI_VENC_GetStream failed with %#x!\n", ret);
						break;
					}

					/*******************************************************
					 *step 2.4 : save frame to file
					 *******************************************************/
					ret = SAMPLE_COMM_VENC_SaveStream(venc_para->enType, fp[i], &stStream);
					if (VIM_SUCCESS != ret)
					{
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						SAMPLE_VENC_ERR("save stream failed!\n");
						break;
					}

					/*******************************************************
					 *step 2.5 : release stream
					 *******************************************************/
					ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
					if (VIM_SUCCESS != ret)
					{
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						SAMPLE_VENC_ERR("release stream failed!\n");
						break;
					}
					/*******************************************************
					 *step 2.6 : free pack nodes
					 *******************************************************/
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;

					SAMPLE_VENC_PRT("chn:%d Read [%d/%d]!!!\n",
									VencChn,
									writeflag[i],
									venc_para->souce_framenum);
					writeflag[i]++;
					if (writeflag[i] >= (VIM_U32)venc_para->souce_framenum)
					{
						SAMPLE_VENC_PRT("Read all svac stream, exit!!!\n");
						goto exit;
					}
				}
			}
		}

		SAMPLE_VENC_PRT("chnmask = %d\n", chnmask);
		if (chnmask == 0)
		{
			SAMPLE_VENC_PRT("exit the encode\n");
			break;
		}
	}

exit:
	for (i = 0; i < chn_count; i++)
	{
		if (fp[i])
		{
			fclose(fp[i]);
		}

		if (fp_in[i])
		{
			fclose(fp_in[i]);
		}
	}

	venc_para->dowork_flag = 0;
	return 0;
}

/******************************************************************************
 * funciton : sendframe and get stream from each channels,then save stream ,for the wait method.
 ******************************************************************************/
VIM_VOID *SAMPLE_COMM_VENC_SendFrame_Getstream_Wait(VIM_VOID *p)
{
	VIM_S32 ret = 0;
	VENC_STREAM_S stStream;
	VENC_CHN_STAT_S stStat;
	FILE *fp[chn_num_max] = {NULL};
	FILE *fp_in[chn_num_max] = {NULL};
	FILE *fp_inBL[chn_num_max] = {NULL};
	VIM_S8 filename_yuv[256];
	VIM_S8 filename[256];
	VIM_U32 writeflag[chn_num_max] = {0};
	VIM_U32 i = 0, x = 0;
	VIDEO_FRAME_INFO_S inputFrame[chn_num_max];
	VIM_U32 chnmask = 0;
	VIM_S32 readsize[chn_num_max] = {0};
	VENC_CHN_ATTR_S pstAttr;
	VIM_U32 w = 0;
	VIM_U32 h = 0;
	VIM_U32 sendframe_num[chn_num_max] = {0};
	VIM_U32 cur_count = 0;
	VIM_S32 s32ChnTotal = 0;
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = NULL;
	VIM_S8 EncoderSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_S8 GetbufSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_U8 *file = NULL;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;

	s32ChnTotal = pstPara->s32Cnt;
	VIM_S8 szFilePostfix[10];
	VIM_S32 isSVAC_SVC_EL[chn_num_max] = {1};
	FILE *fp_temp = NULL;
	VENC_ROI_CFG_S stVencRoiCfg[MAX_COUNT_ROI];
	VIM_U32 RoiNum = 0;
	VIM_U32 ReadYuvNum[2] = {5, 5};

	SAMPLE_VENC_PRT("%s enter\n", __func__);

	VIM_U32 chn_count = s32ChnTotal;
	VIM_U32 VencChn;

	if(NULL == venc_para)
		return NULL;

	SAMPLE_VENC_PRT("venc_para->chn=%d,venc_para->chn_count=%d\r\n", venc_para->VeChn, chn_count);

	VIM_S8 threadname[16];

	memset(threadname, 0x0, 16);
	if (venc_para)
	{
		sprintf((char *)threadname, "S-Gstream_chn%d", venc_para->VeChn);
		prctl(PR_SET_NAME, threadname);
	}

	for (i = 0; i < chn_count; i++)
	{
		VencChn = (venc_para + i)->VeChn;
		VIM_MPI_VENC_GetChnAttr(VencChn, &pstAttr);
		w = pstAttr.stVeAttr.stAttrSvac.u32PicWidth;
		h = pstAttr.stVeAttr.stAttrSvac.u32PicHeight;

		switch (0)
		{
		case 0: ///< yuv420p
		case 1: ///< yuv420sp
			readsize[i] = w * h * 8 * 3 / 16;
			break;

		case 4: ///< yuv400
			readsize[i] = w * h * 8 / 8;
			break;

		default:
			readsize[i] = w * h * 8 * 3 / 16;
			break;
		}

		if ((venc_para + i)->u32PixelMode == 10)
		{
			readsize[i] = (readsize[i] * 5) >> 2;
		}

		file = (VIM_U8 *)(venc_para + i)->souce_filename;
		sprintf((char *)filename_yuv, "%s", file);
		SAMPLE_VENC_PRT("the input file name %s\r\n", filename_yuv);
		fp_in[i] = fopen((const char *)filename_yuv, "rb+");
		if (fp_in[i] == NULL)
		{
			SAMPLE_VENC_ERR(" SAMPLE_VENC_ReadFrame can't open file:%s.\n", filename_yuv);
			goto exit;
		}
		else
		{
			SAMPLE_VENC_PRT(" open the input file successful \r\n");
		}

		if ((venc_para + i)->bEnableSvc)
		{
			file = (VIM_U8 *)(venc_para + i)->souce_filenameBL;
			sprintf((char *)filename_yuv, "%s", file);
			SAMPLE_VENC_PRT("the input BL file name %s\r\n", filename_yuv);
			fp_inBL[i] = fopen((const char *)filename_yuv, "rb+");
			if (fp_inBL[i] == NULL)
			{
				SAMPLE_VENC_ERR(" SAMPLE_VENC_ReadFrame can't open device fp_in file.\n");
				goto exit;
			}
			else
			{
				SAMPLE_VENC_PRT(" open the input file successful \r\n");
			}
			isSVAC_SVC_EL[i] = 0;
			(venc_para + i)->souce_framenum *= 2;
		}

		ret = SAMPLE_COMM_VENC_GetFilePostfix((venc_para + i)->enType, (char *)szFilePostfix);
		if (ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n",
							(venc_para + i)->enType, ret);
			goto exit;
		}

		SAMPLE_VENC_PRT("enable_nosavefile = 0x%x!\n", (venc_para + i)->enable_nosavefile);
		if ((venc_para + i)->enable_nosavefile == 0)
		{
			sprintf((char *)filename, "%s%s", (venc_para + i)->streamname, szFilePostfix);
			fp[i] = fopen((const char *)filename, "wb+");
			if (fp[i] == NULL)
			{
				SAMPLE_VENC_ERR("cann't open the output file filename=%s.\n", filename);
				goto exit;
			}
		}

		chnmask = (1 << (VencChn + 1)) - 1;
	}

	while (VIM_TRUE == pstPara->bThreadStart)
	{
		for (i = 0; i < chn_count; i++)
		{
			fp_temp = fp_in[i];
			if (isSVAC_SVC_EL[i] == 0)
			{
				fp_temp = fp_inBL[i];
			}

			if ((chnmask & (1 << i)) && (sendframe_num[i] < ((VIM_U32)(venc_para + i)->souce_framenum)))
			{
				VencChn = (venc_para + i)->VeChn;
				if (fp_temp && ((EncoderSuccessFlag[i] == VIM_SUCCESS)))
				{
					ret = VIM_MPI_VENC_GetEmptyFramebuf(VencChn, &inputFrame[i], (isSVAC_SVC_EL[i] + 1));
					if (ret != VIM_SUCCESS)
					{
						if (ret == VIM_ERR_VENC_BUF_FULL)
						{
							continue;
						}
						else
						{
							SAMPLE_VENC_ERR("VIM_MPI_VENC_GetEmptyFramebuf some error chn=%d ret=0x%x\n",
											VencChn, ret);
							pstPara->bThreadStart = VIM_FALSE;
							break;
						}
					}

					GetbufSuccessFlag[i] = ret;
					if (ReadYuvNum[isSVAC_SVC_EL[i]] > 0)
					{
						ret = SAMPLE_COMM_VENC_ReadSouceData(pstPara, fp_temp, &inputFrame[i], NULL);
						if (ret != VIM_SUCCESS)
						{
							rewind(fp_temp);
							ret = SAMPLE_COMM_VENC_ReadSouceData(pstPara, fp_temp, &inputFrame[i], NULL);
							if (ret != VIM_SUCCESS)
							{
								SAMPLE_VENC_ERR("ReadSouceData error ret=0x%x\n", ret);
								pstPara->bThreadStart = VIM_FALSE;
								break;
							}
						}
						if ((venc_para + i)->test_async_mode == 1)
						{
							ReadYuvNum[isSVAC_SVC_EL[i]]--;
						}
					}
				}
				else
				{
					SAMPLE_VENC_ERR("chn:%d the fp_in[%d] is Null or one frame buf not be used\n",
									VencChn, i);
				}

				if ((chnmask & (1 << i)) && (GetbufSuccessFlag[i] == VIM_SUCCESS))
				{
					SAMPLE_COMM_VENC_ParseRoiCfgFile((venc_para + i), stVencRoiCfg, writeflag[i], &RoiNum);
					for (x = 0; x < RoiNum; x++)
					{
						ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &stVencRoiCfg[x]);
						if (ret)
						{
							SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", ret);
						}
					}

					ret = VIM_MPI_VENC_SendFrame(VencChn, &inputFrame[i], (isSVAC_SVC_EL[i] + 1));
					if (ret < 0)
					{
						SAMPLE_VENC_ERR("VIM_MPI_VENC_SendFrame vechn[%d] failed with %#x!\n",
										VencChn, ret);
						EncoderSuccessFlag[i] = VIM_FAILURE;
					}
					else
					{
						sendframe_num[i]++;
						EncoderSuccessFlag[i] = VIM_SUCCESS;

						if ((venc_para + i)->bEnableSvc)
						{
							isSVAC_SVC_EL[i] = isSVAC_SVC_EL[i] ^ 1;
						}
					}
				}

				//SAMPLE_VENC_PRT(" chn_num=%d  sendframe_num[%d]=%d\n", i, i,sendframe_num[i]);
			}
		}

		for (i = 0; i < chn_count; i++)
		{
			if (sendframe_num[i] > 0)
			{
				VencChn = (venc_para + i)->VeChn;
				/*******************************************************
				 *step 2.1 : query how many packs in one-frame stream.
				 *******************************************************/
				memset(&stStream, 0, sizeof(stStream));
				ret = VIM_MPI_VENC_Query(VencChn, &stStat);
				if (VIM_SUCCESS != ret)
				{
					SAMPLE_VENC_ERR("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", i, ret);
					break;
				}

				///< SAMPLE_VENC_PRT("[chn%d] Query info:u32LeftStreamFrames:%d u32CurPacks:%d\n",
				///< i,stStat.u32LeftStreamFrames,stStat.u32CurPacks);
				///< SAMPLE_VENC_PRT("\tbRegistered:%d\n", stStat.bRegistered);
				///< SAMPLE_VENC_PRT("\tu32LeftPics:%d\n", stStat.u32LeftPics);
				///< SAMPLE_VENC_PRT("\tu32LeftStreamFrames:%d\n", stStat.u32LeftStreamFrames);
				///< SAMPLE_VENC_PRT("\tu32CurPacks:%d\n", stStat.u32CurPacks);

				/*******************************************************
				 *step 2.2 : malloc corresponding number of pack nodes.
				 *******************************************************/
				stStream.pstPack[0] = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
				if (NULL == stStream.pstPack[0])
				{
					SAMPLE_VENC_ERR("malloc stream pack failed!\n");
					break;
				}

				for (cur_count = 1; cur_count < stStat.u32CurPacks; cur_count++)
				{
					stStream.pstPack[cur_count] =
						(VENC_PACK_S *)((char *)stStream.pstPack[cur_count - 1] + sizeof(VENC_PACK_S));
				}

				/*******************************************************
				 *step 2.3 : call mpi to get one-frame stream
				 *******************************************************/
				///< stStream.u32PackCount = stStat.u32CurPacks;
				ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, 3000);
				if (VIM_SUCCESS != ret)
				{
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
					SAMPLE_VENC_ERR("chn:%d VIM_MPI_VENC_GetStream failed with %#x!\n",
									VencChn, ret);
					break;
				}
				//SAMPLE_VENC_PRT("VIM_MPI_VENC_GetStream reserved[2]=%d ,reserved[3]=%d u32Seq=%d\n",
				//stStream.reserved[2],stStream.reserved[3],stStream.u32Seq);

				/*******************************************************
				 *step 2.4 : save frame to file
				 *******************************************************/
				if ((venc_para + i)->enable_nosavefile == 0)
				{
					ret = SAMPLE_COMM_VENC_SaveStream(venc_para->enType, fp[i], &stStream);
					if (VIM_SUCCESS != ret)
					{
						free(stStream.pstPack[0]);
						stStream.pstPack[0] = NULL;
						SAMPLE_VENC_ERR("save stream failed!\n");
						break;
					}
				}

				/*******************************************************
				 *step 2.5 : release stream
				 *******************************************************/
				ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
				if (VIM_SUCCESS != ret)
				{
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
					SAMPLE_VENC_ERR("chn%d release stream failed!\n", VencChn);
					break;
				}
				/*******************************************************
				 *step 2.6 : free pack nodes
				 *******************************************************/
				free(stStream.pstPack[0]);
				stStream.pstPack[0] = NULL;
				writeflag[i]++;
				if ((venc_para + i)->bEnableSvc)
				{
				}
				else
				{
				}

				if (writeflag[i] >= ((VIM_U32)(venc_para + i)->souce_framenum))
				{
					if ((venc_para + i)->bEnableSvc)
					{
						SAMPLE_VENC_PRT("VencChn:%d Read all  stream  %d frames  exit!!!\n",
							            VencChn, ((VIM_U32)(venc_para + i)->souce_framenum / 2));
					}
					else
					{
						SAMPLE_VENC_PRT("VencChn:%d Read all  stream  %d frames  exit!!!\n",
							            VencChn, ((VIM_U32)(venc_para + i)->souce_framenum));
					}
					goto exit;
				}
			}
		}

		if (chnmask == 0)
		{
			SAMPLE_VENC_PRT("exit the encode chnmask=%d\n", chnmask);
			break;
		}
	}

exit:
	for (i = 0; i < chn_count; i++)
	{
		if ((venc_para + i)->enable_nosavefile == 0)
		{
			if (fp[i])
			{
				fclose(fp[i]);
				fp[i] = NULL;
			}
		}
		if (fp_in[i])
		{
			fclose(fp_in[i]);
			fp_in[i] = NULL;
		}

		if (fp_inBL[i])
		{
			fclose(fp_inBL[i]);
			fp_inBL[i] = NULL;
		}
	}
	venc_para->dowork_flag = 0;

	SAMPLE_VENC_PRT("%s exit\n", __func__);
	return NULL;
}

/******************************************************************************
 * funciton : get stream from each channels,then save them
 ******************************************************************************/
VIM_VOID *SAMPLE_COMM_VENC_GetVencStreamProc_wait(VIM_VOID *p)
{
	VIM_CHAR aszFileName[VENC_MAX_CHN_NUM][128];
	VENC_ROI_CFG_S stVencRoiCfg[MAX_COUNT_ROI];
	PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
	static FILE *pFile[VENC_MAX_CHN_NUM] = {NULL};
	VIM_S32 s32Ret = VIM_SUCCESS;
	VENC_CHN VencChn = 0;
	VIM_S32 stream_count = 0;
	VIM_S32 stream_count_sum = 0;
	VIM_U32 w = 0;
	VIM_U32 h = 0;
	VIM_U32 cur_count = 0;
	VIM_S32 RoiNum = 0;
	VIM_S32 cnt_index = 0;
	VIM_S32 timeout = 0;
	VIM_S32 select_timeout = 6000;
	VIM_S32 i = 0;
	VIM_S32 x = 0;
	pthread_t gs_VencSendstreamPid;
	SAMPLE_VENC_SENDSTREAM_PARA_S stsendstream_s;
	VIM_S32 s32ChnTotal;
	VENC_CHN_ATTR_S stVencChnAttr;
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
	VIM_CHAR szFilePostfix[10];
	VENC_CHN_STAT_S stStat;
	VENC_STREAM_S stStream;
	SAMPLE_ENC_CFG *pEncCfg;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	s32ChnTotal = pstPara->s32Cnt;
	pEncCfg = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;
	cnt_index = pstPara->s32Index;
	VIM_S8 threadname[16];

	memset(threadname, 0x0, 16);
	if (pEncCfg)
	{
		sprintf((char *)threadname, "GetStream%d", pEncCfg->VeChn);
		prctl(PR_SET_NAME, threadname);
	}
	/**************************************************
	 *step 1:  check & prepare save-file & venc-fd
	 **************************************************/
	if (s32ChnTotal >= VENC_MAX_CHN_NUM)
	{
		SAMPLE_VENC_ERR("input count invaild\n");
		return NULL;
	}

	SAMPLE_VENC_PRT("%s enter s32ChnTotal = %d!\n", __func__, s32ChnTotal);
	for (i = 0; i < s32ChnTotal; i++)
	{
		/* decide the stream file name, and open file to save stream */
		VencChn = (pEncCfg + i)->VeChn;
		s32Ret = VIM_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("VIM_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
			return NULL;
		}
		enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

		s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
		if (s32Ret != VIM_SUCCESS)
		{
			SAMPLE_VENC_ERR("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n",
							stVencChnAttr.stVeAttr.enType, s32Ret);
			return NULL;
		}
		SAMPLE_VENC_PRT("enable_nosavefile = 0x%x!\n", (pEncCfg + i)->enable_nosavefile);

		w = stVencChnAttr.stVeAttr.stAttrSvac.u32PicWidth;
		h = stVencChnAttr.stVeAttr.stAttrSvac.u32PicHeight;
		snprintf(aszFileName[i], 128, "./stream%d_chn%d_%d_%d_%dbit%s", pstPara->s32Index,
				 VencChn, w, h, (pEncCfg + i)->u32PixelMode, szFilePostfix);

		sprintf((char *)aszFileName[i], "%s_%d%s", (pEncCfg + i)->streamname, cnt_index, szFilePostfix);
		SAMPLE_VENC_PRT("aszFileName=%s!\n", aszFileName[i]);
		if ((pEncCfg + i)->enable_nosavefile == 0)
		{
			if (pFile[VencChn])
			{
				fclose(pFile[VencChn]);
				pFile[VencChn] = NULL;
			}

			pFile[VencChn] = fopen(aszFileName[i], "wb");
			if (!pFile[VencChn])
			{
				SAMPLE_VENC_ERR("open file[%s] failed!\n", aszFileName[i]);
				return NULL;
			}

#ifdef ENABLE_TFTP
			if ((pEncCfg + i)->reserved[0] == 1)
			{
				SAMPLE_VENC_PRT("start SAMPLE_COMM_VENC_Save_Thread\n");
				stsendstream_s.pEncCfg = pEncCfg;
				stsendstream_s.pFile = pFile[VencChn];
				stsendstream_s.szFilePostfix = szFilePostfix;
				Sem_Init(&stsendstream_s.sem, 0, 0);
				pthread_create(&gs_VencSendstreamPid,
					           0,
					           SAMPLE_COMM_VENC_Save_Thread,
					           (VIM_VOID *)&stsendstream_s);
			}
#endif
		}
	}

	/**************************************************
	 *step 2:  Start to get streams of each channel.
	 **************************************************/
	while (VIM_TRUE == pstPara->bThreadStart)
	{
		for (i = 0; i < s32ChnTotal; i++)
		{
			VencChn = (pEncCfg + i)->VeChn;
			/*******************************************************
			 *step 2.1 : query how many packs in one-frame stream.
			 *******************************************************/
			memset(&stStream, 0, sizeof(stStream));
			s32Ret = VIM_MPI_VENC_Query(VencChn, &stStat);
			if (VIM_SUCCESS != s32Ret)
			{
				SAMPLE_VENC_ERR("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", VencChn, s32Ret);
				///< pstPara->bThreadStart = VIM_FALSE;
				///< break;
			}

			if (0 == stStat.u32CurPacks)
			{
				SAMPLE_VENC_ERR("NOTE: Current  frame is NULL!\n");
				continue;
			}
			/*******************************************************
			 *step 2.3 : malloc corresponding number of pack nodes.
			 *******************************************************/
			stStream.pstPack[0] = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
			if (NULL == stStream.pstPack[0])
			{
				SAMPLE_VENC_ERR("malloc stream pack failed!\n");
				break;
			}

			memset(stStream.pstPack[0], 0x0, sizeof(VENC_PACK_S) * stStat.u32CurPacks);

			for (cur_count = 1; cur_count < stStat.u32CurPacks; cur_count++)
			{
				stStream.pstPack[cur_count] = stStream.pstPack[cur_count - 1] + 1;
			}

			SAMPLE_COMM_VENC_ParseRoiCfgFile((pEncCfg + i), stVencRoiCfg, stream_count, (VIM_U32 *)&RoiNum);
			for (x = 0; x < RoiNum; x++)
			{
				s32Ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &stVencRoiCfg[x]);
				if (s32Ret)
				{
					SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
				}
			}
			/******************************************************
			 *step 2.4 : call mpi to get one-frame stream
			 ******************************************************/
			stStream.u32PackCount = stStat.u32CurPacks;
			s32Ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, select_timeout);
			if (VIM_SUCCESS != s32Ret)
			{
				free(stStream.pstPack[0]);
				stStream.pstPack[0] = NULL;
				SAMPLE_VENC_ERR("VencChn:%d VIM_MPI_VENC_GetStream failed with %#x!\n",
					            VencChn, s32Ret);
				timeout++;
				///< VIM_MPI_VENC_ResetChn(VencChn,VPU_RESET_SAFETY);
				if (timeout > 10)
				{
					///< SAMPLE_VENC_ERR("timrout and reboot!\n");
					///< system("reboot");
				}
				break;
			}
#if 0
			FRAME_PRAVATE_INFO data_ctrl;
			FRAME_PRAVATE_INFO* p = &data_ctrl;
			memcpy(&data_ctrl,(VIM_VOID *)stStream.pstPack[0]->private_data,sizeof(FRAME_PRAVATE_INFO));
			if(VencChn == 0)
			{
				printf("\n Chn %d,ctrldata MARK:%c%c%c, LEN:%d, Mode:%02x, DID:%02x, CID:%02x, ChnCtl:%02x, FrameID:%04x, Width:%04x, Height:%04x, BWidth:%04x, BHeight:%04x, Reserved:%04x, RTS:%08x, DTS:%08x, PTS:%08x, CheckSum:%02x\n",
				VencChn,
				p->mark[0],p->mark[1],p->mark[2],
				p->len,
				p->mode,
				p->devId,
				p->chnId,
				p->chnCtrl,
				p->fsn,
				p->picW,
				p->picH,
				p->byteW,
				p->byteH,
				p->reserv,
				p->rts,
				p->dts,
				p->pts,
				p->crc);
			}
#endif
			select_timeout = 3000;
			stream_count++;
			stream_count_sum++;
#ifdef ENABLE_SEGMENT_SAVE
			if ((pEncCfg + i)->filesize_num == 0)
			{
				(pEncCfg + i)->filesize_num = 1800;
			}
			if (((pEncCfg + i)->enable_nosavefile == 0) &&
				(stream_count >= (pEncCfg + i)->filesize_num) &&
				(stStream.reserved[2] == 0))
			{
				if (pFile[VencChn])
				{
					fclose(pFile[VencChn]);
					pFile[VencChn] = NULL;
				}

#ifdef ENABLE_TFTP
				if ((pEncCfg + i)->reserved[0] == 1)
				{
					Sem_Post(&stsendstream_s.sem);
				}
#endif
				cnt_index++;
				sprintf((char *)aszFileName[i],
					    "%s_%d%s",
					    (pEncCfg + i)->streamname,
					    cnt_index,
					    szFilePostfix);

				SAMPLE_VENC_PRT("aszFileName=%s stream_count=%d\n", aszFileName[i], stream_count);
				pFile[VencChn] = fopen(aszFileName[i], "wb");
				if (!pFile[VencChn])
				{
					SAMPLE_VENC_ERR("open file[%s] failed!\n", aszFileName[i]);
					return NULL;
				}

#ifdef ENABLE_TFTP
				if (cnt_index == (pEncCfg + i)->reserved[1])
				{
					cnt_index = 0;
				}
#endif
				stream_count = 0;
			}

#endif

			/*******************************************************
			 *step 2.5 : save frame to file
			 *******************************************************/
			if ((pEncCfg + i)->enable_nosavefile == 0)
			{
				if (pFile[VencChn] == NULL)
				{
					pFile[VencChn] = fopen(aszFileName[i], "wb");
					if (!pFile[VencChn])
					{
						SAMPLE_VENC_ERR("open file[%s] failed!\n", aszFileName[i]);
						return NULL;
					}
				}
				s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[VencChn], &stStream);
				if (VIM_SUCCESS != s32Ret)
				{
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
					SAMPLE_VENC_ERR("save stream failed!\n");
					break;
				}
			}
			/*******************************************************
			 *step 2.6 : release stream
			 *******************************************************/
			s32Ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
			if (VIM_SUCCESS != s32Ret)
			{
				SAMPLE_VENC_ERR("Release stream failed!\n");
				free(stStream.pstPack[0]);
				stStream.pstPack[0] = NULL;
				break;
			}
			/*******************************************************
			 *step 2.7 : free pack nodes
			 *******************************************************/
			free(stStream.pstPack[0]);
			stStream.pstPack[0] = NULL;
		}
	}
	/*******************************************************
	 * step 3 : close save-file
	 *******************************************************/
	for (i = 0; i < s32ChnTotal; i++)
	{
		if ((pEncCfg + i)->enable_nosavefile == 0)
		{
			VencChn = (pEncCfg + i)->VeChn;
			if (pFile[VencChn])
			{
				fclose(pFile[VencChn]);
				pFile[VencChn] = NULL;
			}
		}
	}
	pEncCfg->dowork_flag = 0;

#ifdef ENABLE_TFTP
	if ((pEncCfg + i)->reserved[0] == 1)
	{
		pthread_join(gs_VencSendstreamPid, 0);
	}
#endif
	SAMPLE_VENC_PRT("%s exit\n", __func__);
	return NULL;
}

/******************************************************************************
 * funciton : send frame data to encorder
 ******************************************************************************/
VIM_VOID *SAMPLE_COMM_VENC_SendFrameYuv(VIM_VOID *p)
{
	VIM_S32 ret = 0;
	FILE *fp[chn_num_max] = {NULL};
	FILE *fp_in[chn_num_max] = {NULL};
	FILE *fp_inBL[chn_num_max] = {NULL};
	VIM_S8 filename_yuv[256];
	VIM_U32 writeflag[chn_num_max] = {0};
	VIM_U32 i = 0, x = 0;
	VIDEO_FRAME_INFO_S inputFrame[chn_num_max];
	VIM_U32 chnmask = 0;
	VIM_S32 readsize[chn_num_max] = {0};
	VENC_CHN_ATTR_S pstAttr;
	VIM_U32 w = 0;
	VIM_U32 h = 0;
	VIM_U32 sendframe_num[chn_num_max] = {0};
	VIM_S32 s32ChnTotal = 0;
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = NULL;
	VIM_S8 EncoderSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_S8 GetbufSuccessFlag[chn_num_max] = {VIM_SUCCESS};
	VIM_U8 *file = NULL;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	SAMPLE_ENC_CFG *venc_para = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;

	s32ChnTotal = pstPara->s32Cnt;
	VIM_S32 isSVAC_SVC_EL[chn_num_max] = {1};
	FILE *fp_temp = NULL;
	VENC_ROI_CFG_S stVencRoiCfg[MAX_COUNT_ROI];
	VIM_U32 RoiNum = 0;
	VIM_U32 ReadYuvNum[2] = {5, 5};
	VIM_U32 chn_count = s32ChnTotal;
	VIM_U32 VencChn;

	SAMPLE_VENC_PRT("%s enter\n", __func__);
	SAMPLE_VENC_PRT("venc_para->chn=%d,venc_para->chn_count=%d\r\n", venc_para->VeChn, chn_count);

	VIM_S8 threadname[16];

	memset(threadname, 0x0, 16);
	if (venc_para)
	{
		sprintf((char *)threadname, "SendFrameYuv%d", venc_para->VeChn);
		prctl(PR_SET_NAME, threadname);
	}

	for (i = 0; i < chn_count; i++)
	{
		VencChn = (venc_para + i)->VeChn;
		VIM_MPI_VENC_GetChnAttr(VencChn, &pstAttr);
		w = pstAttr.stVeAttr.stAttrSvac.u32PicWidth;
		h = pstAttr.stVeAttr.stAttrSvac.u32PicHeight;

		switch (0)
		{
		case 0: ///< yuv420p
		case 1: ///< yuv420sp
			readsize[i] = w * h * 8 * 3 / 16;
			break;

		case 4: ///< yuv400
			readsize[i] = w * h * 8 / 8;
			break;

		default:
			readsize[i] = w * h * 8 * 3 / 16;
			break;
		}

		if ((venc_para + i)->u32PixelMode == 10)
		{
			readsize[i] = (readsize[i] * 5) >> 2;
		}

		file = (VIM_U8 *)(venc_para + i)->souce_filename;
		sprintf((char *)filename_yuv, "%s", file);
		SAMPLE_VENC_PRT("the input file name %s\r\n", filename_yuv);
		fp_in[i] = fopen((const char *)filename_yuv, "rb+");
		if (fp_in[i] == NULL)
		{
			SAMPLE_VENC_ERR(" SAMPLE_VENC_ReadFrame can't open file:%s.\n", filename_yuv);
			goto exit;
		}
		else
		{
			SAMPLE_VENC_PRT(" open the input file successful \r\n");
		}

		if ((venc_para + i)->bEnableSvc)
		{
			file = (VIM_U8 *)(venc_para + i)->souce_filenameBL;
			sprintf((char *)filename_yuv, "%s", file);
			SAMPLE_VENC_PRT("the input BL file name %s\r\n", filename_yuv);
			fp_inBL[i] = fopen((const char *)filename_yuv, "rb+");
			if (fp_inBL[i] == NULL)
			{
				SAMPLE_VENC_ERR(" SAMPLE_VENC_ReadFrame can't open device fp_in file.\n");
				goto exit;
			}
			else
			{
				SAMPLE_VENC_PRT(" open the input file successful \r\n");
			}
			isSVAC_SVC_EL[i] = 0;
			(venc_para + i)->souce_framenum *= 2;
		}
		chnmask = (1 << (VencChn + 1)) - 1;
	}

	while (VIM_TRUE == pstPara->bThreadStart)
	{
		for (i = 0; i < chn_count; i++)
		{
			fp_temp = fp_in[i];
			if (isSVAC_SVC_EL[i] == 0)
			{
				fp_temp = fp_inBL[i];
			}

			if ((chnmask & (1 << i)) && (sendframe_num[i] < ((VIM_U32)(venc_para + i)->souce_framenum)))
			{
				if (fp_temp && ((EncoderSuccessFlag[i] == VIM_SUCCESS)))
				{
					VencChn = (venc_para + i)->VeChn;
					ret = VIM_MPI_VENC_GetEmptyFramebuf(VencChn, &inputFrame[i], (isSVAC_SVC_EL[i] + 1));
					if (ret != VIM_SUCCESS)
					{
						if (ret == VIM_ERR_VENC_BUF_FULL)
						{
							usleep(1000);
							continue;
						}
						else
						{
							SAMPLE_VENC_ERR("VIM_MPI_VENC_GetEmptyFramebuf some error i=%d ret=0x%x\n", i, ret);
							pstPara->bThreadStart = VIM_FALSE;
							break;
						}
					}

					GetbufSuccessFlag[i] = ret;

					if (ReadYuvNum[isSVAC_SVC_EL[i]] > 0)
					{
						ret = SAMPLE_COMM_VENC_ReadSouceData(pstPara,
															 fp_temp,
															 &inputFrame[i],
															 NULL);
						if (ret != VIM_SUCCESS)
						{
							rewind(fp_temp);
							SAMPLE_COMM_VENC_ReadSouceData(pstPara,
														   fp_temp,
														   &inputFrame[i],
														   NULL);
						}
						if ((venc_para + i)->test_async_mode == 2)
						{
							ReadYuvNum[isSVAC_SVC_EL[i]]--;
						}
					}
				}
				else
				{
					SAMPLE_VENC_ERR("the fp_in[%d] is Null or one frame buf not be used\n", i);
				}

				if ((chnmask & (1 << i)) && (GetbufSuccessFlag[i] == VIM_SUCCESS))
				{
					ret = VIM_MPI_VENC_SendFrame(VencChn, &inputFrame[i], (isSVAC_SVC_EL[i] + 1));
					if (ret < 0)
					{
						SAMPLE_VENC_ERR("VIM_MPI_VENC_SendFrame vechn[%d] failed with %#x!\n", i, ret);
						EncoderSuccessFlag[i] = VIM_FAILURE;
					}
					else
					{
						sendframe_num[i]++;
						EncoderSuccessFlag[i] = VIM_SUCCESS;

						if ((venc_para + i)->bEnableSvc)
						{
							isSVAC_SVC_EL[i] = isSVAC_SVC_EL[i] ^ 1;
						}

						SAMPLE_COMM_VENC_ParseRoiCfgFile((venc_para + i),
														 stVencRoiCfg,
														 writeflag[i] - 1,
														 &RoiNum);
						for (x = 0; x < RoiNum; x++)
						{
							ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &stVencRoiCfg[x]);
							if (ret)
							{
								SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", ret);
							}
						}
					}
				}

				//SAMPLE_VENC_PRT(" chn_num=%d	sendframe_num[%d]=%d\n", i, i,sendframe_num[i]);
			}
		}

		if (chnmask == 0)
		{
			SAMPLE_VENC_PRT("exit the encode chnmask=%d\n", chnmask);
			break;
		}
	}

exit:
	for (i = 0; i < chn_count; i++)
	{
		if (fp[i])
		{
			fclose(fp[i]);
		}

		if (fp_in[i])
		{
			fclose(fp_in[i]);
		}

		if (fp_inBL[i])
		{
			fclose(fp_inBL[i]);
		}
	}
	venc_para->dowork_flag = 0;

	SAMPLE_VENC_PRT("%s exit\n", __func__);
	return NULL;
}

/******************************************************************************
 * funciton : start get venc stream process thread
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_StartGetStream(VIM_U32 pthread_id,
													  VIM_S32 s32Cnt,
													  VIM_S32 s32Index,
													  SAMPLE_ENC_CFG *pEncCfg)
{
	VIM_S32 s32Ret = VIM_FAILURE;

	if (pthread_id < pthread_maxnum)
	{
		gs_stPara_auto[pthread_id].bThreadStart = VIM_TRUE;
		gs_stPara_auto[pthread_id].s32Cnt = s32Cnt;
		gs_stPara_auto[pthread_id].s32Index = s32Index;
		gs_stPara_auto[pthread_id].pEncCfg = pEncCfg;
		if (pEncCfg->test_async_mode == 2)
		{
			pthread_create(&gs_VencYuvPid[pthread_id],
						   0,
						   SAMPLE_COMM_VENC_SendFrameYuv,
						   (VIM_VOID *)&gs_stPara_auto[pthread_id]);
			pthread_create(&gs_VencGetstreamPid[pthread_id],
						   0,
						   SAMPLE_COMM_VENC_GetVencStreamProc_wait,
						   (VIM_VOID *)&gs_stPara_auto[pthread_id]);
		}
		else
		{
			if (pEncCfg->venc_bind == 0)
			{
				pthread_create(&gs_VencGetstreamPid[pthread_id],
					           0,
					           SAMPLE_COMM_VENC_SendFrame_Getstream_Wait,
					           (VIM_VOID *)&gs_stPara_auto[pthread_id]);
			}
			else if (pEncCfg->venc_bind != 0)
			{
				pthread_create(&gs_VencGetstreamPid[pthread_id],
				               0,
				               SAMPLE_COMM_VENC_GetVencStreamProc_wait,
				               (VIM_VOID *)&gs_stPara_auto[pthread_id]);
			}
		}
		return s32Ret;
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : stop get venc stream process.
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_StopGetStream(VIM_U32 pthread_id, SAMPLE_ENC_CFG *pEncCfg)
{
	if ((VIM_TRUE == gs_stPara_auto[pthread_id].bThreadStart) && (pthread_id < pthread_maxnum))
	{
		gs_stPara_auto[pthread_id].bThreadStart = VIM_FALSE;
		pthread_cancel(gs_VencGetstreamPid[pthread_id]);
		pthread_join(gs_VencGetstreamPid[pthread_id], 0);
		///< if(performance_test == 2){
		if (pEncCfg->test_async_mode == 2)
		{
			pthread_cancel(gs_VencYuvPid[pthread_id]);
			pthread_join(gs_VencYuvPid[pthread_id], 0);
		}
	}
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : start send souce frame data  process thread
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_StartSendYuv(VIM_U32 pthread_id,
												   VIM_S32 s32Cnt,
												   VIM_S32 s32Index,
												   SAMPLE_ENC_CFG *pEncCfg)
{
	if (pthread_id < pthread_yuv_maxnum)
	{
		gs_YuvPara[pthread_id].bThreadStart = VIM_TRUE;
		gs_YuvPara[pthread_id].s32Cnt = s32Cnt;
		gs_YuvPara[pthread_id].s32Index = s32Index;
		gs_YuvPara[pthread_id].pEncCfg = pEncCfg;
		return pthread_create(&gs_VencYuvPid[pthread_id],
							  0,
							  SAMPLE_COMM_VENC_SendFrameYuv,
							  (VIM_VOID *)&gs_YuvPara[pthread_id]);
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : stop send souce frame data  process thread
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_StopSendYuv(VIM_U32 pthread_id)
{
	if ((VIM_TRUE == gs_YuvPara[pthread_id].bThreadStart) && (pthread_id < pthread_yuv_maxnum))
	{
		gs_YuvPara[pthread_id].bThreadStart = VIM_FALSE;
		pthread_cancel(gs_VencYuvPid[pthread_id]);
		pthread_join(gs_VencYuvPid[pthread_id], 0);
	}
	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : parse the configure file
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_PARACFG(VIM_CHAR *cfgfilename, SAMPLE_ENC_CFG *pEncCfg, VIM_U8 *cfgchnnum)
{
	FILE *pFile = NULL;
	VIM_S32 VALUE = 0;
	VIM_U8 chnnum = 0;
	VIM_U8 i = 0;
	VIM_S32 ChnCfgOffset = 0;
	VIM_S32 ChnCfgOffset_end = 0;

	if (cfgfilename)
	{
		pFile = fopen((void *)cfgfilename, "r");
		if (pFile == NULL)
		{
			return VIM_FAILURE;
		}
	}
	else
	{
		SAMPLE_VENC_ERR("failed, the input cfg file is NULL\n");
		return VIM_FAILURE;
	}

	if (WAVE_GetValue(pFile, "chnnum", &VALUE, 0) == 0)
	{
		SAMPLE_VENC_PRT("chnnum = %d\n", VALUE);
	}
	else
	{
		chnnum = VALUE;
		SAMPLE_VENC_PRT(" chnnum = %d\n", chnnum);
	}

	memset(pEncCfg, 0x0, sizeof(SAMPLE_ENC_CFG) * chnnum);

	for (i = 0; i < chnnum; i++)
	{
		GetCfgOffset(pFile, "ChnStartFlag", (i + 1), &ChnCfgOffset);
		GetCfgOffset(pFile, "ChnEndFlag", (i + 1), &ChnCfgOffset_end);
		SAMPLE_VENC_PRT("ChnCfgOffset = 0x%x ChnCfgOffset_end=0x%x\n", ChnCfgOffset, ChnCfgOffset_end);
		parseEncCfgFile(&pEncCfg[i], (char *)cfgfilename, (VIM_U32)ChnCfgOffset, (VIM_U32)ChnCfgOffset_end);
		printParsePara(&pEncCfg[i]);
	}
	*cfgchnnum = chnnum;

	if (pFile)
	{
		fclose(pFile);
	}

	return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : parse the roi parameters
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_ParseRoiCtuModeParam(VIM_CHAR *lineStr, VENC_ROI_CFG_S *roiRegionInfo)
{
	VIM_S32 numParsed;

	memset(roiRegionInfo, 0, sizeof(VENC_ROI_CFG_S));

	numParsed = sscanf(lineStr, "%d %d %d %d %d %d %d",
					   &roiRegionInfo->u32Index, (VIM_S32 *)&roiRegionInfo->bEnable,
					   &roiRegionInfo->stRect.s32X, &roiRegionInfo->stRect.s32Y,
					   &roiRegionInfo->stRect.u32Width, &roiRegionInfo->stRect.u32Height,
					   &roiRegionInfo->s32Qp);

	SAMPLE_VENC_PRT("u32Index=%d bEnable=%d s32X=%d s32Y=%d u32Width=%d u32Height=%d s32Qp=%d\n",
					roiRegionInfo->u32Index,
					roiRegionInfo->bEnable,
					roiRegionInfo->stRect.s32X,
					roiRegionInfo->stRect.s32Y,
					roiRegionInfo->stRect.u32Width,
					roiRegionInfo->stRect.u32Height,
					roiRegionInfo->s32Qp);

	if (numParsed != 7)
	{
		return 0;
	}

	return 1;
}

/******************************************************************************
 * funciton : parse the roi  configure file
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_VENC_ParseRoiCfgFile(SAMPLE_ENC_CFG *pEncCfg,
													  VENC_ROI_CFG_S *pstVencRoiCfg,
													  VIM_S32 num,
													  VIM_U32 *roi_num)
{
	VIM_S32 roiNum = 0;
	VIM_S32 i = 0;
	FILE *roi_file = NULL;
	VIM_S32 Offset = 0;
	VIM_S32 value = 0;
	VIM_S32 roicfgNum = 0;
	VIM_S32 s32AvgOp = 0;

	memset(pstVencRoiCfg, 0, sizeof(VENC_ROI_CFG_S) * MAX_COUNT_ROI);
	*roi_num = 0;

	roi_file = fopen((const char *)pEncCfg->roi_file_name, "rb+");
	if (roi_file == NULL)
	{
		///<	  	SAMPLE_VENC_PRT(" roi_file_name can't open file:%s\n",pEncCfg->roi_file_name);
		return VIM_FAILURE;
	}
	else
	{
		///< SAMPLE_VENC_PRT(" open the roi file successful \r\n");
	}

	if (WAVE_GetValue(roi_file, "roicfgNum", &value, 0) == 0)
	{
		SAMPLE_VENC_PRT("roicfgNum = %d\n", value);
	}
	else
	{
		roicfgNum = value;
		///<		SAMPLE_VENC_PRT( " roicfgNum = %d\n", roicfgNum);
	}

	VIM_CHAR lineStr[256] = {
		0,
	};
	VIM_S32 val = 0;

	for (i = 0; i < roicfgNum; i++)
	{
		GetCfgOffset(roi_file, "Start", (i + 1), &Offset);
		fseek(roi_file, Offset, 0);
		fgets(lineStr, 256, roi_file);
		sscanf(lineStr, "%d\n", &val);
		///< SAMPLE_VENC_PRT( " val = %d\n", val);
		if (val > num)
		{
			break;
		}

		if (val == num)
		{
			fgets(lineStr, 256, roi_file);
			sscanf(lineStr, "%d\n", &roiNum); ///< number of roi regions
			SAMPLE_VENC_PRT(" roiNum = %d\n", roiNum);
			if (roiNum > MAX_COUNT_ROI)
			{
				SAMPLE_VENC_PRT(" roiNum:%d\n", roiNum);
				return VIM_FAILURE;
			}

			fgets(lineStr, 256, roi_file);
			sscanf(lineStr, "%d\n", &s32AvgOp); ///< s32AvgOp value
			SAMPLE_VENC_PRT(" s32AvgOp = %d\n", s32AvgOp);

			for (i = 0; i < roiNum; i++)
			{
				fgets(lineStr, 256, roi_file);
				if (SAMPLE_COMM_VENC_ParseRoiCtuModeParam(lineStr, &pstVencRoiCfg[i]) == 0)
				{
					SAMPLE_VENC_ERR("CFG file error : %s value is not available.\n",
						pEncCfg->roi_file_name);
				}
				pstVencRoiCfg[i].s32AvgOp = s32AvgOp;
			}
			*roi_num = roiNum;
			break;
		}
	}

	if (roi_file)
	{
		fclose(roi_file);
	}
	return VIM_SUCCESS;
}

#ifdef ENABLE_TFTP
VIM_VOID *SAMPLE_COMM_VENC_Save_Thread(VIM_VOID *p)
{
	SAMPLE_ENC_CFG *pEncCfg;
	VIM_S32 cnt_index = 0;
	VIM_CHAR aszFileName[384];
	SAMPLE_VENC_SENDSTREAM_PARA_S *pstPara = (SAMPLE_VENC_SENDSTREAM_PARA_S *)p;
	VIM_CHAR tftpcmdbuf[512] = {0};

	pEncCfg = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;

	while (1)
	{
		Sem_Wait(&pstPara->sem);
		sprintf((char *)aszFileName, "%s_%d%s", pEncCfg->streamname, cnt_index, pstPara->szFilePostfix);
		///< SAMPLE_VENC_PRT("aszFileName=%s!\n",aszFileName);

		if (pEncCfg->reserved_str[0] != 0)
		{
			sprintf(tftpcmdbuf, "tftp -pl %s -r %s -b 512  %s",
				    aszFileName, aszFileName, pEncCfg->reserved_str);
			printf("tftpcmdbuf %s\n", tftpcmdbuf);
			system("sync");
			system(tftpcmdbuf);
		}

		cnt_index++;

		if (cnt_index == pEncCfg->reserved[1])
		{
			cnt_index = 0;
		}
	}

	return NULL;
}
#endif
VIM_S32 SAMPLE_SET_VENV_BufInfo(SAMPLE_ENC_CFG *venc_cfg_config)
{
	VENC_BufInfo_CFG_S VencBufCfg = {0};
	VencBufCfg.u8IsUseNonVencFrameBuf = venc_cfg_config->u8IsUseNonVencFrameBuf;
	VencBufCfg.u8SelectMode = venc_cfg_config->u8SelectMode;
	VencBufCfg.u8SelectModeEnable = venc_cfg_config->u8SelectModeEnable;
	VencBufCfg.u8SelectModeChn = venc_cfg_config->VeChn;
	VIM_S32 s32Ret = VIM_MPI_VENC_SetBufInfo(venc_cfg_config->VeChn, &VencBufCfg);
	if (VIM_SUCCESS != s32Ret) {
		SAMPLE_VENC_PRT("VIM_MPI_VENC_SetBufInfo [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
		return VIM_FAILURE;
	}
	return VIM_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
