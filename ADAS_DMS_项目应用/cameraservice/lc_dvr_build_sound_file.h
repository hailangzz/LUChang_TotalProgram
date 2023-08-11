#ifndef _LC_DVR_SOUND_FILE_H_
#define _LC_DVR_SOUND_FILE_H_
void lc_dvr_sound_record_initial(void);
void lc_dvr_sound_record_unInitial(void);
void lc_dvr_sound_record_start(int duration, int conti);
void lc_dvr_sound_record_stop(void);
void lc_dvr_sound_data_insert(void* data, int len);

#endif

