#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "loadbmp.h"

VIM_S32 GetBmpInfo(const char *filename,
    OSD_BITMAPFILEHEADER *pBmpFileHeader, OSD_BITMAPINFO *pBmpInfo)
{
    FILE *pFile;

    VIM_U16 bfType;

    if (NULL == filename) {
        printf("OSD_LoadBMP: filename=NULL\n");
        return -1;
    }

    if ((pFile = fopen((char *)filename, "rb")) == NULL) {
        printf("Open file faild:%s!\n", filename);
        return -1;
    }

    (void)fread(&bfType, 1, sizeof(bfType), pFile);
    if (bfType != 0x4d42) {
        printf("not bitmap file\n");
        fclose(pFile);
        return -1;
    }

    (void)fread(pBmpFileHeader, 1, sizeof(OSD_BITMAPFILEHEADER), pFile);
    (void)fread(pBmpInfo, 1, sizeof(OSD_BITMAPINFO), pFile);
    fclose(pFile);

    return 0;
}

static VIM_S32 Full_BitmapPixel(RGN_BITMAP_S *pstBitmap, VIM_U32 x, VIM_U32 y, unsigned int color)
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

	offset = (((y * pstBitmap->u32Width) * 2) / 8) + updByteIndex;
	pDstData = (VIM_CHAR *)(pstBitmap->pData+offset);
	DstData = *pDstData;
	updBitIndex *= 2;
	DstData &= ~(3 << updBitIndex);
	color &= 0x03;
	DstData |= (color << updBitIndex);
	*pDstData = DstData;

	return 0;
}

int LoadBMPSig(const char *filename, RGN_BITMAP_S *pstBitmap, VIM_S32 ColorIdx)
{
    FILE *pFile;
    VIM_U16  i, j, k;
    VIM_U32  w, h;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;
    VIM_U8 *pOrigBMPBuf;
    VIM_U32 stride;
    unsigned char tmp;
    VIM_U8 *pBuf;

    if (NULL == filename) {
        printf("OSD_LoadBMP: filename=NULL\n");
        return -1;
    }

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0) {
        return -1;
    }

    if (bmpInfo.bmiHeader.biCompression != 0) {
        printf("not support compressed bitmap file!\n");
        return -1;
    }

    if (bmpInfo.bmiHeader.biHeight < 0) {
        printf("bmpInfo.bmiHeader.biHeight < 0\n");
        return -1;
    }

    if ((pFile = fopen((char *)filename, "rb")) == NULL) {
        printf("Open file faild:%s!\n", filename);
        return -1;
    }

    w = pstBitmap->u32Width;
    h = pstBitmap->u32Height;

    stride = w /8;

    pOrigBMPBuf = (VIM_U8 *)malloc(h * stride);

    if (NULL == pOrigBMPBuf) {
        printf("not enough memory to malloc!\n");
        fclose(pFile);
        return -1;
    }


    fseek(pFile, bmpFileHeader.bfOffBits, 0);
    if (fread(pOrigBMPBuf, 1, h * stride, pFile) != (h * stride)) {
        printf("fread error!line:%d\n", __LINE__);
        perror("fread:");
        fclose(pFile);
		return -1;
    }

    pBuf = pOrigBMPBuf;
    for (i = 0; i < h; i++) {

        pBuf = pOrigBMPBuf + ((h - 1) - i) * stride;
        for (k = 0; k < w/8; k++) {
            tmp = *pBuf++;
            for (j = 0; j < 8; j++) {
                if (!((tmp >> (7 - j)) & 0x01)) {
                    Full_BitmapPixel(pstBitmap, (j+(8 * k)), i, ColorIdx);
                }
            }
        }

    }

    free(pOrigBMPBuf);
    pOrigBMPBuf = NULL;
    fclose(pFile);

    return 0;
}

char *GetExtName(char *filename)
{
    char *pret = NULL;
    VIM_U32 fnLen;

    if (NULL == filename) {
        printf("filename can't be null!");
        return NULL;
    }

    fnLen = strlen(filename);
    while (fnLen) {
        pret = filename + fnLen;
        if (*pret == '.') {
            return (pret + 1);
        }
        fnLen--;
    }

    return pret;
}

/*
	if use this LoadImageSig(),
	create region equal the width and hight of bitmap,must multiple for 32(* x 32)
*/
int LoadImageSig(const char *filename, RGN_BITMAP_S *pstBitmap, VIM_S32 ColorIdx)
{
    char *ext = GetExtName((char *)filename);

    if (VIM_NULL == ext) {
        printf("GetExtName error!\n");
        return -1;
    }

    if (strcmp(ext, "bmp") == 0) {
        if (0 != LoadBMPSig(filename, pstBitmap, ColorIdx)) {
            printf("OSD_LoadBMP error!\n");
            return -1;
        }
    } else {
        printf("not supported image file!\n");
        return -1;
    }

    return 0;
}

