#ifndef     __LOAD_BMP_H__
#define     __LOAD_BMP_H__

#include "vim_type.h"
#include "vim_comm_region.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


/* the color format OSD supported */
typedef enum vimOSD_COLOR_FMT_E {
    OSD_COLOR_FMT_RGB444    = 0,
    OSD_COLOR_FMT_RGB4444   = 1,
    OSD_COLOR_FMT_RGB555    = 2,
    OSD_COLOR_FMT_RGB565    = 3,
    OSD_COLOR_FMT_RGB1555   = 4,
    OSD_COLOR_FMT_RGB888    = 6,
    OSD_COLOR_FMT_RGB8888   = 7,
    OSD_COLOR_FMT_BUTT
} OSD_COLOR_FMT_E;

typedef struct vimOSD_RGB_S {
    VIM_U8 u8B;
    VIM_U8 u8G;
    VIM_U8 u8R;
    VIM_U8 u8Reserved;
} OSD_RGB_S;

typedef struct vimOSD_SURFACE_S {
    OSD_COLOR_FMT_E enColorFmt;                 /* color format */
    VIM_U8          *pu8PhyAddr;                /* physical address */
    VIM_U16         u16Height;                  /* operation height */
    VIM_U16         u16Width;                   /* operation width */
    VIM_U16         u16Stride;                  /* surface stride */
    VIM_U16         u16Reserved;
} OSD_SURFACE_S;

typedef struct tag_OSD_Logo {
    VIM_U32 width;        /* out */
    VIM_U32 height;       /* out */
    VIM_U32 stride;       /* in */
    VIM_U8  *pRGBBuffer;  /* in/out */
} OSD_LOGO_T;

typedef struct tag_OSD_BITMAPINFOHEADER {
    VIM_U16 biSize;
    VIM_U32 biWidth;
    VIM_S32 biHeight;
    VIM_U16 biPlanes;
    VIM_U16 biBitCount;
    VIM_U32 biCompression;
    VIM_U32 biSizeImage;
    VIM_U32 biXPelsPerMeter;
    VIM_U32 biYPelsPerMeter;
    VIM_U32 biClrUsed;
    VIM_U32 biClrImportant;
} OSD_BITMAPINFOHEADER;

typedef struct tag_OSD_BITMAPFILEHEADER {
    VIM_U32 bfSize;
    VIM_U16 bfReserved1;
    VIM_U16 bfReserved2;
    VIM_U32 bfOffBits;
} OSD_BITMAPFILEHEADER;

typedef struct tag_OSD_RGBQUAD {
    VIM_U8 rgbBlue;
    VIM_U8 rgbGreen;
    VIM_U8 rgbRed;
    VIM_U8 rgbReserved;
} OSD_RGBQUAD;

typedef struct tag_OSD_BITMAPINFO {
    OSD_BITMAPINFOHEADER    bmiHeader;
    OSD_RGBQUAD             bmiColors[1];
} OSD_BITMAPINFO;

typedef struct vimOSD_COMPONENT_INFO_S {
    int alen;
    int rlen;
    int glen;
    int blen;
} OSD_COMP_INFO;

VIM_S32 GetBmpInfo(const VIM_CHAR *filename, OSD_BITMAPFILEHEADER *pBmpFileHeader, OSD_BITMAPINFO *pBmpInfo);
int LoadImageSig(const char *filename, RGN_BITMAP_S *pstBitmap, VIM_S32 ColorIdx);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef __LOAD_BMP_H__*/

