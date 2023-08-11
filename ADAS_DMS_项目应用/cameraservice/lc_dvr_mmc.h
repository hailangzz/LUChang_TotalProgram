#ifndef _LC_DVR_MMC_H_
#define _LC_DVR_MMC_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include <stdlib.h> /**loff_t��ͷ�ļ�**/
#include <stdint.h>

#define EMMC_PAGE 512
#define LC_PAGESIZE_TIMES 8

enum{
	TRAVEL_RECORD = 0X21,	// ��ʻ��¼
	DOUBTFUL_RECORD,		// �ɵ��¼
	OVER_TIME_RECORD,		// ��ʱ��¼
	DRIVER_RECORD,			// ��ʻԱ��¼
	DAILY_RECORD,			// ��־��¼
	SYSTEM_DB_SAVE,			// ϵͳ��������Ϣ
	VIDEO_FILE_RECORD,		// ��ǰ����¼�Ƶ���Ƶ
};//MFmt_Type;

int read_extcsd(int fd, uint8_t *ext_csd);
int write_extcsd_value(int fd, uint8_t index, uint8_t value);
void lc_dvr_mmc_test(int seed);
int lc_dvr_mmc_initial(void);
int is_duvonn_device(void);

int WriteSectors(char *p, int len, loff_t offset);
int ReadSectors(char *p, int len, loff_t offset);

void lc_dvr_mem_debug(void* membuf, int len);
int  lc_dvr_duvonn_get_ind(char* cptr, int len, int offset);
int  lc_dvr_duvonn_read_sector(char* cptr,  int len, int offset);
int  lc_dvr_duvonn_write_sector(char* cptr,   int len, int offset);

int lc_dvr_duvonn_storage_initial(void* decryptDat);
void lc_dvr_duvonn_storage_unInitial(void);
void lc_dvr_duvonn_unlock(void *decryptDat, int len);


int direct_file_initial(void);
int direct_file_unInitial(void);
int direct_file_open(char *path);
int direct_file_close(int fp);
int direct_file_write(int fp, char *p, int len, loff_t offset);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif


