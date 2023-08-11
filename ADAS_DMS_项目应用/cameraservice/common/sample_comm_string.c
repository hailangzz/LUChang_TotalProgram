/******************************************************************************
  A simple program of vimcro's vc0718p string osd display.
  Copyright (C), 2015-2025, vimicro Tech. Co., Ltd.
 ******************************************************************************
	Modification:  2018-7 Created by Amanda (dengnaili)
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_region.h"
#include "vim_comm_sys.h"
///<#include "include/vim_comm_region.h"

typedef struct vimOSD_FONT_INFO_S
{
	VIM_U8 *pFontData;
	VIM_S32 s32FontDataSize;
	VIM_S32 u32FontSize;
} OSD_FONT_INFO;

static OSD_FONT_INFO s_OSDFontInfo[6] = {
	{0, 0, 0}, /*ascii8x16.vim*/
	{0, 0, 1}, /*ascii12x24.vim*/
	{0, 0, 2}, /*ascii16x32.vim*/
	{0, 0, 0}, /*gbk16x16.vim*/
	{0, 0, 1}, /*gbk24x24.vim*/
	{0, 0, 2}  /*gbk32x32.vim*/
};

/********************************************************************************************************
 * 0: 8*16, 16*16
 * 1: 12*24, 24*24
 * 2: 16*32, 32*3
 ********************************************************************************************************/
static int gASCY = 16;
static int gASCX = 8;
static int gHZX = 16;

static void Get_ASC_Buffer(VIM_U32 offset, VIM_U8 *buf)
{
	VIM_U32 addr;

	switch (gASCX)
	{
	case 8: ///< 8x16
		addr = offset * 16 * 1 + (VIM_U32)s_OSDFontInfo[0].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 16);
		break;
	case 16: ///< 16x32
		addr = offset * 32 * 2 + (VIM_U32)s_OSDFontInfo[2].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 64);
		break;
	case 24: ///< 24x48
			 ///< addr=offset*48*3+s_OSDFontInfo[3].pFontData;
			 ///< memcpy(buf, addr, 144);
		break;
	case 32: ///< 32x64
			 ///< addr=offset*64*4+s_OSDFontInfo[4].pFontData;
			 ///< memcpy(buf, addr, 256);
		break;
	default: ///< 12x24
		addr = offset * 24 * 2 + (VIM_U32)s_OSDFontInfo[1].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 48);
		break;
	}
}

static void Get_GBK_Buffer(VIM_U32 offset, unsigned char *buf)
{
	VIM_U32 addr;
	///< 24x24
	switch (gHZX)
	{
	case 16: ///< 16x16
		addr = offset * 16 * 2 + (VIM_U32)s_OSDFontInfo[3].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 32);
		break;
	case 32: ///< 32x32
		addr = offset * 32 * 4 + (VIM_U32)s_OSDFontInfo[5].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 128);
		break;
	default: ///< 24x24
		addr = offset * 24 * 3 + (VIM_U32)s_OSDFontInfo[4].pFontData;
		memcpy(buf, (VIM_U8 *)addr, 72);
		break;
	}
}

static VIM_S32 Update_BitmapPixel(RGN_BITMAP_S *pstBitmap,
										 VIM_U32 x,
										 VIM_U32 y,
										 unsigned int color)
{
	VIM_U32 updByteIndex;
	VIM_U32 updBitIndex;
	VIM_U32 offset;
	VIM_CHAR DstData;
	VIM_CHAR *pDstData;

	if (x >= pstBitmap->u32Width)
		return VIM_SUCCESS;
	if (y >= pstBitmap->u32Height)
		return VIM_SUCCESS;

	updByteIndex = x / 4;
	updBitIndex = x % 4;

	offset = y * pstBitmap->u32Width * 2 / 8 + updByteIndex;
	pDstData = (VIM_CHAR *)(pstBitmap->pData + offset);
	DstData = *pDstData;
	updBitIndex *= 2;
	DstData &= ~(3 << updBitIndex);
	color &= 0x03;
	DstData |= (color << updBitIndex);
	*pDstData = DstData;

	return 0;
}

static void SAMPLE_RGN_FillASCII_Ext(RGN_BITMAP_S *pstBitmap,
                                              int x,
                                              int y,
                                              unsigned char *BUF,
                                              unsigned int color)
{
	unsigned char tmp;
	int i;
	int j;

	if (gASCX == 16)
	{
		for (i = 0; i < 32; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
		}
	}
	else if (gASCX == 12)
	{
		for (i = 0; i < 24; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 4; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
		}
	}
	else if (gASCX == 24)
	{
		for (i = 0; i < 48; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 16, y + i, color);
			}
		}
	}
	else if (gASCX == 32)
	{
		for (i = 0; i < 64; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 16, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 24, y + i, color);
			}
		}
	}
	else if (gASCX == 8)
	{
		for (i = 0; i < 16; i++)
		{
			tmp = BUF[i];
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
		}
	}
}

static void SAMPLE_RGN_FillGBK_Ext(RGN_BITMAP_S *pstBitmap,
                                            int x,
                                            int y,
                                            unsigned char *BUF,
                                            unsigned int color)
{
	unsigned char tmp;
	unsigned char i;
	unsigned char j;

	if (gHZX == 32)
	{
		for (i = 0; i < 32; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 16, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 24, y + i, color);
			}
		}
	}
	else if (gHZX == 24)
	{
		for (i = 0; i < 24; i++)
		{
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
			tmp = *BUF++;
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 16, y + i, color);
			}
		}
	}
	else if (gHZX == 16)
	{
		for (i = 0; i < 16; i++)
		{
			tmp = BUF[i * 2];
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = BUF[i * 2 + 1];
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
		}
	}
	else if (gHZX == 12)
	{
		for (i = 0; i < 16; i++)
		{
			tmp = BUF[i * 2];
			for (j = 0; j < 8; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j, y + i, color);
			}
			tmp = BUF[i * 2 + 1];
			for (j = 0; j < 4; j++)
			{
				if ((tmp >> (7 - j)) & 0x01)
					Update_BitmapPixel(pstBitmap, x + j + 8, y + i, color);
			}
		}
	}
}

void SAMPLE_RGN_SelectFont(VIM_S32 u32FontSize)
{
	switch (u32FontSize)
	{
	case 0:
		gASCX = 8;
		gASCY = 16;
		gHZX = 16;
		break;
	case 2:
		gASCX = 16;
		gASCY = 32;
		gHZX = 32;
		break;
	default:
		gASCX = 12;
		gASCY = 24;
		gHZX = 24;
		break;
	}
}

VIM_S32 SAMPLE_RGN_LoadFont(VIM_S32 u32FontSize)
{
	FILE *pFile;
	VIM_CHAR szFileNameAscii[128] = "/system/app/font/";
	VIM_CHAR szFileNameGbk[128] = "/system/app/font/";
	VIM_S32 s32Ret = VIM_SUCCESS;
	VIM_S32 s32DataLen = 0;
	int basePathLen = strlen(szFileNameAscii); 

	if (u32FontSize > 2)
		u32FontSize = 1;
	printf("OSD FontSize:%d\n", u32FontSize);
	switch (u32FontSize)
	{
	case 0:
		sprintf(&szFileNameAscii[basePathLen], "ascii8x16.vim.bin");
		sprintf(&szFileNameGbk[basePathLen], "gbk16x16.vim.bin");
		gASCX = 8;
		gASCY = 16;
		gHZX = 16;
		break;
	case 2:
		sprintf(&szFileNameAscii[basePathLen], "ascii16x32.vim.bin");
		sprintf(&szFileNameGbk[basePathLen], "gbk32x32.vim.bin");
		gASCX = 16;
		gASCY = 32;
		gHZX = 32;
		break;
	default:
		sprintf(&szFileNameAscii[basePathLen], "ascii12x24.vim.bin");
		sprintf(&szFileNameGbk[basePathLen], "gbk24x24.vim.bin");
		gASCX = 12;
		gASCY = 24;
		gHZX = 24;
		break;
	}
	///< printf("Open file :%s!\n", szFileNameAscii);
	pFile = fopen((char *)szFileNameAscii, "rb");
	if (pFile == NULL)
	{
		printf("Open file faild:%s!\n", szFileNameAscii);
		return -1;
	}
	fseek(pFile, 0L, SEEK_END);
	s32DataLen = ftell(pFile);
	///< printf(" file length=%d!\n", s32DataLen);
	s_OSDFontInfo[u32FontSize].s32FontDataSize = s32DataLen;
	s_OSDFontInfo[u32FontSize].pFontData = (VIM_U8 *)malloc(s32DataLen);
	if (s_OSDFontInfo[u32FontSize].pFontData == NULL)
	{
		fclose(pFile);
		s32Ret = VIM_FAILURE;
		return s32Ret;
	}
	fseek(pFile, 0L, SEEK_SET);
	s_OSDFontInfo[u32FontSize].s32FontDataSize = fread(s_OSDFontInfo[u32FontSize].pFontData,
		                                               1,
		                                               s32DataLen,
		                                               pFile);

	if (s_OSDFontInfo[u32FontSize].s32FontDataSize != s32DataLen)
	{
		printf("load file faild!\n");
		free(s_OSDFontInfo[u32FontSize].pFontData);
		fclose(pFile);
		s32Ret = VIM_FAILURE;
		return s32Ret;
	}
	fclose(pFile);
	///< printf("Open file :%s!\n", szFileNameGbk);
	pFile = fopen((char *)szFileNameGbk, "rb");
	if (pFile == NULL)
	{
		printf("Open file faild:%s!\n", szFileNameGbk);
		free(s_OSDFontInfo[u32FontSize].pFontData);
		return -1;
	}
	fseek(pFile, 0L, SEEK_END);
	s32DataLen = ftell(pFile);
	s_OSDFontInfo[u32FontSize + 3].s32FontDataSize = s32DataLen;
	s_OSDFontInfo[u32FontSize + 3].pFontData = (VIM_U8 *)malloc(s32DataLen);
	if (s_OSDFontInfo[u32FontSize + 3].pFontData == NULL)
	{
		fclose(pFile);
		free(s_OSDFontInfo[u32FontSize].pFontData);
		s32Ret = VIM_FAILURE;
		return s32Ret;
	}
	fseek(pFile, 0L, SEEK_SET);
	s_OSDFontInfo[u32FontSize + 3].s32FontDataSize = fread(s_OSDFontInfo[u32FontSize + 3].pFontData,
		                                                   1,
		                                                   s32DataLen,
		                                                   pFile);

	if (s_OSDFontInfo[u32FontSize + 3].s32FontDataSize != s32DataLen)
	{
		printf("load file faild!\n");
		free(s_OSDFontInfo[u32FontSize + 3].pFontData);
		free(s_OSDFontInfo[u32FontSize].pFontData);
		fclose(pFile);
		s32Ret = VIM_FAILURE;
		return s32Ret;
	}
	fclose(pFile);

	return s32Ret;
}

VIM_S32 SAMPLE_RGN_ReleaseFont(VIM_S32 u32FontSize)
{
	if (s_OSDFontInfo[u32FontSize].pFontData != NULL)
	{
		free(s_OSDFontInfo[u32FontSize].pFontData);
		s_OSDFontInfo[u32FontSize].pFontData = NULL;
	}
	if (s_OSDFontInfo[u32FontSize + 3].pFontData != NULL)
	{
		free(s_OSDFontInfo[u32FontSize + 3].pFontData);
		s_OSDFontInfo[u32FontSize + 3].pFontData = NULL;
	}

	return VIM_SUCCESS;
}

void SAMPLE_RGN_FillStringToBitmapBuffer(RGN_BITMAP_S *pstBitmap,
                                                       char *sString,
                                                       VIM_S32 ColorIdx)
{
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int i = 0;
	unsigned int hzoff = 0;
	unsigned char tmp1 = 0;
	unsigned char tmp2 = 0;
	unsigned char zkbuf[64 * 4];

	for(i = 0; i < sizeof(zkbuf); i++)
	{
		zkbuf[i] = 0;
	}

	if (sString == 0)
		return;
	x = 0;
	y = 0;

	while (*sString && x <= pstBitmap->u32Width && y <= pstBitmap->u32Height)
	{
		tmp1 = *sString;
		if (tmp1 >= 0x81)
		{
		    ///< GBK
			tmp2 = *(sString + 1);
			if (tmp2 == '\0')
				return;
			if (((tmp1 >= 0x81) && (tmp1 <= 0xfe)) && ((tmp2 >= 0x41) && (tmp2 <= 0xfe)))
			{
				///< printf("-%02x%02x", tmp1,tmp2);
				if (tmp2 < 0x7f)
				{
					tmp2 -= 0x40;
				}
				else
				{
					tmp2 -= 0x41;
				}
				hzoff = tmp1 - 0x80 - 1;
				hzoff = hzoff * 190 + tmp2;
				Get_GBK_Buffer(hzoff, zkbuf);
			}
			SAMPLE_RGN_FillGBK_Ext(pstBitmap, x, y, zkbuf, ColorIdx);
			x += gHZX;
			sString += 2;
		}
		else
		{
		    ///< ascii
		    ///< printf("-%02x", tmp1);
			if (tmp1 == 0x0d || tmp1 == 0x0a)
			{
				sString++;
				if ((*sString) == 0x0d || (*sString) == 0x0a)
					sString++;
				y += (gASCY+8);
				x = 0;
			}
			else
			{
				if (tmp1 > 0x0d)
				{
					Get_ASC_Buffer(tmp1, zkbuf);
					SAMPLE_RGN_FillASCII_Ext(pstBitmap, x, y, zkbuf, ColorIdx);
					x += gASCX;
				}
				sString += 1;
			}
		}
	}
}

VIM_S32 draw_internal_stringTobitmap(RGN_BITMAP_S *pstBitmap,
                                                char *sString,
                                                VIM_S32 ColorIdx,
                                                VIM_S32 u32FontSize)
{
	VIM_S32 Ret = 0;

	SAMPLE_RGN_ReleaseFont(u32FontSize);
	Ret = SAMPLE_RGN_LoadFont(u32FontSize);
	if (Ret != VIM_SUCCESS)
	{
		printf("draw_string_to_bitmap -> SAMPLE_RGN_LoadFont fail!\n");
		return Ret;
	}
	SAMPLE_RGN_FillStringToBitmapBuffer(pstBitmap, sString, ColorIdx);
	Ret = SAMPLE_RGN_ReleaseFont(u32FontSize);
	if (Ret != VIM_SUCCESS)
	{
		printf("draw_string_to_bitmap -> SAMPLE_RGN_ReleaseFont fail!\n");
		return Ret;
	}

	return Ret;
}
