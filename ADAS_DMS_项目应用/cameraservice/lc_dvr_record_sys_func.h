#ifndef LC_DVR_RECORD_SYS_FUNC_H
#define LC_DVR_RECORD_SYS_FUNC_H
#include "lc_dvr_comm_def.h"
#ifdef __cplusplus
extern "C" {
#endif

void sys_debug_enable(int state);
void sys_remove_folder(char name[]);
void sys_set_rtc_time(uint64_t *time);
void sys_get_rtc_time(char time_chr[]);
void* sys_msg_get_memcache(int size);
int sys_msg_queu_in(LC_DVR_RECORD_MESSAGE *msg);
LC_DVR_RECORD_MESSAGE* sys_msg_queu_out(void);
int  sys_msg_queu_remain(void);
void sys_msg_queu_pool_initial(void);
void sys_msg_queu_pool_uninitial(void);
void vc_message_send(int eMsg, int val, void* para, void* func);

#ifdef __cplusplus
}
#endif
#endif

