#ifndef _SAMPLE_STRING_H_
#define _SAMPLE_STRING_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "vim_comm_region.h"
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
void SAMPLE_RGN_SelectFont(VIM_S32  u32FontSize);
VIM_S32 SAMPLE_RGN_LoadFont(VIM_S32  u32FontSize);
VIM_S32 SAMPLE_RGN_ReleaseFont(VIM_S32  u32FontSize);
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
void SAMPLE_RGN_FillStringToBitmapBuffer(RGN_BITMAP_S *pstBitmap, char *sString, VIM_S32 ColorIdx);
VIM_S32 draw_internal_stringTobitmap(RGN_BITMAP_S *pstBitmap, char *sString, VIM_S32 ColorIdx,VIM_S32  u32FontSize);
#ifdef __cplusplus
}
#endif

#endif 


