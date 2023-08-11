/*****************************************************************
* Copyright (C), 2001-2020 Beijing Vimicro Technology Co., Ltd.
* ALL Rights Reserved
*
* Name      : socket_comm.h
* Summary   :
* Date      : 2022.8.27
* Author    :
*
*****************************************************************/

#ifndef _OSD_H_
#define _OSD_H_

#include "vim_comm_region.h"
#include "sample_common_region.h"


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */


VIM_S32 dvr_DO_REGION_OSD_Init(VIM_S32 s32GrpId, VIM_S32 s32ChnId,
        VIM_U32 u32Width, VIM_U32 u32Height, VIM_CHAR *sTestString, VIM_BOOL bInvert);

void *osd_thread(void *arg);
void osd_reset_license(char *license);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* _OSD_H_ */

