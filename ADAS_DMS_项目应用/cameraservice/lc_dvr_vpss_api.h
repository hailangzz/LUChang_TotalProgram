#ifndef _LC_DVR_VPSS_API_H_
#define _LC_DVR_VPSS_API_H_
#include "lc_dvr_comm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mpi_sys.h"

void *lc_dvr_assist_vpss_frame_send(void *frame, int group, int funPara);
VIM_VOID lc_dvr_assist_vpss_run(int times);
VIM_S32 lc_dvr_assist_vpss_creat(VIM_U32 inW, VIM_U32 inH, VIM_U32 otW, VIM_U32 otH, void* ackFunc);
VIM_S32 lc_dvr_assist_vpss_config(VIM_U32 width, VIM_U32 height);
VIM_S32 lc_dvr_assist_vpss_destroy(VIM_VOID);

#ifdef __cplusplus
}
#endif
#endif

