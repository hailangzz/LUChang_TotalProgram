#ifndef LC_DVR_RECORD_MCU_PARSE_H_
#define LC_DVR_RECORD_MCU_PARSE_H_
#ifdef __cplusplus
extern "C" {
#endif

int lc_dvr_mcu_get_speed(void);
void lc_dvr_mcu_notify(int data);
void lc_dvr_mcu_connect_initial(void);
void lc_dvr_mcu_connect_uninitial(void);
void lc_dvr_storageErr_notify(int data);
void lc_dvr_camErr_notify(int data);
void lc_dvr_algorithem_notify(int data);
void lc_dvr_mcu_emergency_size(int rate);

#ifdef __cplusplus
}
#endif
#endif

