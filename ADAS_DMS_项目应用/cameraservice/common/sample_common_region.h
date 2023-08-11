#ifndef __SAMPLE_COMM_REGION_H__
#define __SAMPLE_COMM_REGION_H__
//#include "vim_comm_region.h"
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

VIM_VOID SAMPLE_RGN_Init(VIM_S32 GrpId);
VIM_VOID SAMPLE_RGN_Exit(VIM_S32 GrpId);
VIM_S32 SAMPLE_RGN_CoverChannelConfig(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32PicWidth, VIM_U32 u32PicHeight);
VIM_S32 SAMPLE_RGN_OSDChannelConfig(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32PicWidth, VIM_U32 u32PicHeight);
VIM_S32 SAMPLE_RGN_SetCoverDefaultAttr(VIM_S32 GrpId, VIM_S32 ChnId);

VIM_S32 SAMPLE_RGN_SetOsdChnInvertAttr(VIM_S32 GrpId, VIM_S32 ChnId,
                        VIM_U32 u32InvertValue,VIM_U32 u32Width,VIM_U32 u32Height);

VIM_S32 SAMPLE_RGN_SetOsdDefaultAttr(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_DestroyRegion(VIM_S32 GrpId, VIM_S32 Handle);

VIM_S32 SAMPLE_RGN_ShowStringOsd(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle, VIM_S32 u32Width,
                        VIM_S32 u32Height,VIM_S32 s32X,VIM_S32 s32Y, VIM_CHAR *Str, VIM_S32 State);

VIM_S32 SAMPLE_RGN_ChgStringOsd(VIM_S32 GrpId, VIM_S32 Handle, VIM_CHAR *Str);

VIM_S32 SAMPLE_RGN_AddCoverToVipp(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_AddOsdStringToVipp(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_AddOsdToVipp(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_AddRectangleToVipp(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_AddMasaicToVipp(VIM_S32 GrpId, VIM_S32 ChnId);
VIM_S32 SAMPLE_RGN_AddOsdToDec2Vo(VIM_S32 GrpId, VIM_S32 ChnId, VIM_U32 u32Width, VIM_U32 u32Height);

VIM_S32 SAMPLE_RGN_AddInvertArea(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 key,
                        VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y);

VIM_S32 SAMPLE_RGN_DelInvertArea(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 key);

VIM_S32 SAMPLE_RGN_ShowOsdLogo(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
                        VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y);

VIM_S32 SAMPLE_RGN_AddLogoToVipp(VIM_S32 GrpId, VIM_S32 ChnId, VIM_S32 Handle,
                        VIM_S32 u32Width, VIM_S32 u32Height, VIM_S32 s32X, VIM_S32 s32Y);

VIM_U32 DO_VI_GetRGB_YUV_10Bit(VIM_U32 u32RGBColor);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* __SAMPLE_COMM_REGION_H__ */
