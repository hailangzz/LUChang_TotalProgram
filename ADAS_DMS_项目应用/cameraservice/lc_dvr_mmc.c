#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include "errno.h"
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <stdlib.h>
#include <semaphore.h>
#include "lc_dvr_semaphore.h"
#include "lc_dvr_mmc.h"
#include "lc_dvr_record_log.h"
#include "SpecAreaLib.h"

#define FCPM_P1_EMW		0x00
#define FCPM_P1_ICR1	0x10
#define FCPM_P1_TDT		0x18
#define FCPM_P1_WSM		0x14
#define FCPM_P1_WDAL	0x15
#define FCPM_P1_WDA		0x20

#define FCPM_P2_ICR2	0x00
#define FCPM_P2_EMR		0x10
#define FCPM_P2_SR		0x04
#define FCPM_P2_SID		0x05
#define FCPM_P2_CDT		0x08
#define FCPM_P2_PSR		0x20
#define FCPM_P2_PDAL	0x2E
#define FCPM_P2_PDA		0x80

int		mmcFd = -1;
int		mINDFileSector;
char	*mSectorBuf = NULL;
static char	*mFileSectorBuf = NULL;

char mSectorOne[EMMC_PAGE]={
	0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0xfb, 0x50, 0x07, 0x50, 0x1f, 0xfc, 0xbe, 0x1b, 0x7c,
	0xbf, 0x1b, 0x06, 0x50, 0x57, 0xb9, 0xe5, 0x01, 0xf3, 0xa4, 0xcb, 0xbd, 0xbe, 0x07, 0xb1, 0x04,
	0x38, 0x6e, 0x00, 0x7c, 0x09, 0x75, 0x13, 0x83, 0xc5, 0x10, 0xe2, 0xf4, 0xcd, 0x18, 0x8b, 0xf5,
	0x83, 0xc6, 0x10, 0x49, 0x74, 0x19, 0x38, 0x2c, 0x74, 0xf6, 0xa0, 0xb5, 0x07, 0xb4, 0x07, 0x8b,
	0xf0, 0xac, 0x3c, 0x00, 0x74, 0xfc, 0xbb, 0x07, 0x00, 0xb4, 0x0e, 0xcd, 0x10, 0xeb, 0xf2, 0x88,
	0x4e, 0x10, 0xe8, 0x46, 0x00, 0x73, 0x2a, 0xfe, 0x46, 0x10, 0x80, 0x7e, 0x04, 0x0b, 0x74, 0x0b,
	0x80, 0x7e, 0x04, 0x0c, 0x74, 0x05, 0xa0, 0xb6, 0x07, 0x75, 0xd2, 0x80, 0x46, 0x02, 0x06, 0x83,
	0x46, 0x08, 0x06, 0x83, 0x56, 0x0a, 0x00, 0xe8, 0x21, 0x00, 0x73, 0x05, 0xa0, 0xb6, 0x07, 0xeb,
	0xbc, 0x81, 0x3e, 0xfe, 0x7d, 0x55, 0xaa, 0x74, 0x0b, 0x80, 0x7e, 0x10, 0x00, 0x74, 0xc8, 0xa0,
	0xb7, 0x07, 0xeb, 0xa9, 0x8b, 0xfc, 0x1e, 0x57, 0x8b, 0xf5, 0xcb, 0xbf, 0x05, 0x00, 0x8a, 0x56,
	0x00, 0xb4, 0x08, 0xcd, 0x13, 0x72, 0x23, 0x8a, 0xc1, 0x24, 0x3f, 0x98, 0x8a, 0xde, 0x8a, 0xfc,
	0x43, 0xf7, 0xe3, 0x8b, 0xd1, 0x86, 0xd6, 0xb1, 0x06, 0xd2, 0xee, 0x42, 0xf7, 0xe2, 0x39, 0x56,
	0x0a, 0x77, 0x23, 0x72, 0x05, 0x39, 0x46, 0x08, 0x73, 0x1c, 0xb8, 0x01, 0x02, 0xbb, 0x00, 0x7c,
	0x8b, 0x4e, 0x02, 0x8b, 0x56, 0x00, 0xcd, 0x13, 0x73, 0x51, 0x4f, 0x74, 0x4e, 0x32, 0xe4, 0x8a,
	0x56, 0x00, 0xcd, 0x13, 0xeb, 0xe4, 0x8a, 0x56, 0x00, 0x60, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
	0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00, 0x0a, 0x00,
	0x0b, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00, 0x10, 0x00, 0x11, 0x00, 0x12, 0x00,
	0x13, 0x00, 0x14, 0x00, 0x15, 0x00, 0x16, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x00, 0x1a, 0x00,
	0x1b, 0x00, 0x1c, 0x00, 0x1d, 0x00, 0x1e, 0x00, 0x1f, 0x00, 0x20, 0x00, 0x21, 0x00, 0x22, 0x00,
	0x23, 0x00, 0x24, 0x00, 0x25, 0x00, 0x26, 0x00, 0x27, 0x00, 0x69, 0x6f, 0x6e, 0x20, 0x74, 0x61,
	0x62, 0x6c, 0x65, 0x00, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x20, 0x6c, 0x6f, 0x61, 0x64, 0x69, 0x6e,
	0x67, 0x20, 0x6f, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74,
	0x65, 0x6d, 0x00, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x6f, 0x70, 0x65, 0x72, 0x61,
	0x74, 0x69, 0x6e, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x47, 0x42, 0x4a, 0x70, 0x00, 0xc0, 0x00, 0x00, 0x21, 0x67, 0x61, 0x21, 0xff, 0x00, 0x00, 0x0a,
	0x09, 0x02, 0x07, 0xfe, 0xff, 0xff, 0x00, 0x80, 0x00, 0x00, 0x00, 0xa0, 0x87, 0x03, 0x00, 0xfe,
	0xff, 0xff, 0x56, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x78, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa,
};

void lc_dvr_mem_debug(void* membuf, int len)
{
	char diplay[512];
	char *sectorBuf = (char*)membuf;
	int k=0;
	
	for(int i=0; i<1024; i++)
	{
		int j, position=0;
		
		position += sprintf(diplay+position, "0x%04x: ", k);
		for(j=0; j<16; j++)
		{
			if(k>=len)
			{
				i=2048;
				break;
			}
			position += sprintf(diplay+position, "%02x ", sectorBuf[k++]);			
		}

		if(j>0)	printf("%s\n", diplay);
	}
}

int WriteSectors(char *p, int len, loff_t offset)
{
	int readByte, remain = len;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	int cLen, ret;
	char* s8Ptr = p;
	
	// logd_lc("WriteSectors %d @ %llx", len, offset);
	SemaphoreP();
	if (lseek64(mmcFd, offset ,SEEK_SET) == -1 ){
		printf("WriteSectors2 lseek(%llx) err!!\n", offset);
		SemaphoreV();
		return (-1);//seek failed
	}

	for(;;)
	{
		cLen = remain>pageSize?pageSize:remain;
		memcpy(mSectorBuf, s8Ptr, cLen);
		ret = write(mmcFd, mSectorBuf, cLen); /**连续写32K数据大概为3毫秒**/

		if(ret < 0) 
		{
			loge_lc("duvonn write Err!");
			break;
		}
		remain -= cLen;
		s8Ptr += cLen;
		if(remain == 0) break;	
	}
	SemaphoreV();
	return len-remain;
}

int ReadSectors(char *p, int len, loff_t offset)
{
	int readByte, remain = len;	
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	int cLen, ret;
	char* s8Ptr = p;
	
	// logd_lc("ReadSectors @ %llx", offset);
	SemaphoreP();
	if (lseek64(mmcFd, offset ,SEEK_SET) == -1 ){
		logd_lc("ReadSectors lseek(%llx) err!!", offset);
		SemaphoreV();
		return (-1);//seek failed
	}

	for(;;)
	{
		cLen = remain>pageSize?pageSize:remain;
		ret = read(mmcFd, mSectorBuf, cLen);
		// logd_lc("ReadSectors-->len:%d, ret:%d", cLen, ret);
		if(ret < 0) break;
		memcpy(s8Ptr, mSectorBuf, cLen);
		remain -= cLen;
		s8Ptr += cLen;
		if(remain == 0) break;
	}	
	SemaphoreV();
	return len-remain;
}

/**读写均以页为单位**/
int lc_dvr_duvonn_get_ind(char* cptr, int len, int offset)
{
	FILE* fp = fopen("/mnt/intsd/GBT19056_Index_File.IND", "rb");
	int result, fLen = 0;
	if(fp!=NULL)
	{
		fseek(fp, 0, SEEK_END);
		fLen =ftell(fp);		
		fseek(fp, 0, SEEK_SET);
		result = fread(cptr, 1, fLen, fp);
		fclose(fp);
		logd_lc("index file read len:%d", result);
	}
	return fLen;	
}

int lc_dvr_duvonn_read_sector(char* cptr, int len, int offset)
{
	loff_t realOffset = offset;
	realOffset *= EMMC_PAGE;
	return ReadSectors(cptr, len, realOffset);	
}

int lc_dvr_duvonn_write_sector(char* cptr, int len, int offset)
{
	loff_t realOffset = offset;
	realOffset *= EMMC_PAGE;

	return WriteSectors(cptr, len, realOffset);	
}

int is_duvonn_device(void)
{
	return (mmcFd<0)?0:1;
}
#if 0
void lc_dvr_duvonn_storage_initial(void)
{
	char path[256];
	char *s8Ptr;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	
	sprintf(path, "/dev/mmcblk0");
	mSectorBuf = memalign(pageSize, pageSize);
	mmcFd = open(path, O_RDWR|O_DIRECT, 606);
	logd_lc("duvonn Fd :%x", mmcFd);
	if(mmcFd <0) return;
//lc_dvr_mmc_initial();
//lc_dvr_mmc_test(0);
#if 0
	{
	/**下面程序是灾备有问题后的恢复方法**/
		char saveName[128];
		loff_t offset = 0x00;
		int readLen = 32*1024;
		FILE *fp;		
#if 0	
		/**从功能完好的灾备里读取分区表以及引导取的信息**/
		sprintf(saveName, "/mnt/intsd/duvonn_ini.raw");
		fp = fopen(saveName, "wb+");
		logd_lc("begin load duvonn!");
		if(fp!=NULL)
		{		
			for(int i=0; i<0xf000; i++)
			{
				if((i&0xff)==0) logd_lc("go-->%x", i);
				ReadSectors(mSectorBuf, EMMC_PAGE, offset);
				fwrite(mSectorBuf, 1, EMMC_PAGE, fp);
				offset += EMMC_PAGE;
			}	
			fclose(fp);
		}
#else
		/**把备份的功能完好的读完开头的15M数据写回灾备以恢复**/
		sprintf(saveName, "/mnt/intsd/duvonn_ini_ok.raw");
		fp = fopen(saveName, "rb");
		logd_lc("begin down duvonn!");
		if(fp!=NULL)
		{		
			fseek(fp, 0, SEEK_SET);
			for(int i=0; i<0xf000; i++)
			{
				if((i&0xff)==0) logd_lc("ld-->%x", i);
				fread(mSectorBuf, 1, EMMC_PAGE, fp);
				WriteSectors(mSectorBuf, EMMC_PAGE, offset);			
				offset += EMMC_PAGE;
			}	
			fclose(fp);
		}
#endif	
		logd_lc("finish duvonn reset!");
	}
#endif	
	ReadSectors(mSectorBuf, EMMC_PAGE, 0);
	s8Ptr = (char*)mSectorBuf;

	mINDFileSector = -1;

	for(int i=0; i<508; i++)
	{
		if(s8Ptr[0] == 'G')
		{
			if(s8Ptr[1] == 'B')
			{
				if((s8Ptr[2] == 'J')&&(s8Ptr[3] == 'p'))
				{
					mINDFileSector = s8Ptr[4]|(s8Ptr[5]<<8)|(s8Ptr[6]<<16)|(s8Ptr[7]<<24);
					logd_lc("Found IND file secotr --> 0x%x", mINDFileSector);
				}
			}
		}
		s8Ptr++;
	}
	
//	lc_dvr_mmc_test(0);
}
#endif


void lc_dvr_duvonn_storage_unInitial(void)
{
	if(mSectorBuf != NULL)
	{
		free(mSectorBuf);
		mSectorBuf = NULL;
	}
	
	if(mmcFd>=0)
	{
		close(mmcFd);
		mmcFd = -1;
	}
}

/**凡澈科技**/
int lc_dvr_duvonn_storage_initial(void* decryptDat)
{
	char path[256];
	char *s8Ptr;
	int decryptDatLen = 0;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	logd_lc("lc_dvr_duvonn_storage_initial!!");
	
	sprintf(path, "/dev/mmcblk0");	
	if(mmcFd<0)	mmcFd = open(path, O_RDWR|O_DIRECT, 606);	
	
	if(mmcFd>=0)
	{
		logd_lc("mmcFd:%d", mmcFd);
		CreateSemaphore();
		if(mSectorBuf == NULL)	mSectorBuf = memalign(pageSize, pageSize);
		//===============================================
		memset(mSectorBuf, 0, EMMC_PAGE);	
		ReadSectors(mSectorBuf, EMMC_PAGE, 8*EMMC_PAGE);
		//logd_lc("xxxgo0xxx-->%s", s8Ptr);
		//lc_dvr_mem_debug(mSectorBuf, 64);		
		mSectorBuf[32] = '\0';
		if(strstr(mSectorBuf, "FC-32-")==NULL)
		{
			logd_lc("normal sdcard");
			lc_dvr_duvonn_storage_unInitial();
			return mmcFd;
		}
		else
		{
			logd_lc("funtrue storage detec!");
		}
		//===============================================			
		memset(mSectorBuf, 0, EMMC_PAGE);	
		s8Ptr = mSectorBuf + FCPM_P1_EMW;
		strcpy(s8Ptr, "GB/T19056V0409EN");	
		s8Ptr = mSectorBuf + FCPM_P1_ICR1;
		strcpy(s8Ptr, "FC01");	
		WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);
	
		memset(mSectorBuf, 0, EMMC_PAGE);
		ReadSectors(mSectorBuf, EMMC_PAGE, 2*EMMC_PAGE);	
		s8Ptr = mSectorBuf + FCPM_P2_EMR;
		if(strstr(s8Ptr,"ENV0409GB/T19056")!=NULL)
		{
			logd_lc("disk status: %x", mSectorBuf[FCPM_P2_PSR]);
			if(mSectorBuf[FCPM_P2_PSR]!=0x07)
			{
				logd_lc("Enter unlock mode!");
#if 1
				{		
					uint8_t errNum;
					int retval;
					
					memset(mSectorBuf, 0, EMMC_PAGE);
					s8Ptr = mSectorBuf + FCPM_P1_EMW;	
					strcpy(s8Ptr, "GB/T19056V0409QT");
					s8Ptr = mSectorBuf + FCPM_P1_ICR1;	
					strcpy(s8Ptr, "FC01");
					WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);	
					logd_lc("QUIT Commond");	
					
					retval = fn_wp_flow_app("/dev/mmcblk0", &errNum);
					logd_lc("fn_wp_flow_app->ret:%d err:%d", retval, errNum);				
				}	
#else
				memset(mSectorBuf, 0, EMMC_PAGE);
				s8Ptr = mSectorBuf + FCPM_P1_EMW;
				strcpy(s8Ptr, "GB/T19056V0409EN");
				s8Ptr = mSectorBuf + FCPM_P1_ICR1;
				strcpy(s8Ptr, "WPEN");
				WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);
				usleep(2*1000);
				memset(mSectorBuf, 0, EMMC_PAGE);
				ReadSectors(mSectorBuf, EMMC_PAGE, 2*512);
				
				if(mSectorBuf[4]==0x06)
				{
					logd_lc("All ready be initial!");
				}
				else if(mSectorBuf[4]==0x00)
				{
					uint16_t *datLen = (uint16_t*)(mSectorBuf+FCPM_P2_PDAL);
					decryptDatLen = datLen[0];
					if(decryptDat!=NULL)
					{
						memcpy(decryptDat, mSectorBuf+FCPM_P2_PDA, decryptDatLen);
					}
				}
#endif			
			}
		}
		
		if(decryptDatLen == 0)
		{
			char *s8Ptr;			
			memset(mSectorBuf, 0, EMMC_PAGE);
			s8Ptr = mSectorBuf + FCPM_P1_EMW;	
			strcpy(s8Ptr, "GB/T19056V0409QT");
			s8Ptr = mSectorBuf + FCPM_P1_ICR1;	
			strcpy(s8Ptr, "FC01");
			WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);	
			logd_lc("QUIT -->");	
		}
	}
	mINDFileSector = -1;
	return decryptDatLen;
}

void lc_dvr_duvonn_unlock(void *decryptDat, int len)
{
	char *s8Ptr;
	if(decryptDat!=NULL)
	{
		uint16_t *dataLen;
		memset(mSectorBuf, 0, EMMC_PAGE);
		s8Ptr = mSectorBuf + FCPM_P1_EMW;
		strcpy(s8Ptr, "GB/T19056V0409EN");
		s8Ptr = mSectorBuf + FCPM_P1_ICR1;
		strcpy(s8Ptr, "WPAT");
		dataLen = (uint16_t*)(mSectorBuf + FCPM_P1_WDAL);
		dataLen[0] = len;
		memcpy(mSectorBuf+FCPM_P1_WDA, decryptDat, len);
		WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);	
	}
	
	memset(mSectorBuf, 0, EMMC_PAGE);
	s8Ptr = mSectorBuf + FCPM_P1_EMW;
	strcpy(s8Ptr, "GB/T19056V0409EN");
	s8Ptr = mSectorBuf + FCPM_P1_ICR1;
	strcpy(s8Ptr, "WPUL");
	WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);	
	
	memset(mSectorBuf, 0, EMMC_PAGE);
	ReadSectors(mSectorBuf, EMMC_PAGE, 2*EMMC_PAGE);
	logd_lc("WPUL --> %d", mSectorBuf[4]);		/**用凡澈的库绑定后，该流程无效**/

	memset(mSectorBuf, 0, EMMC_PAGE);
	s8Ptr = mSectorBuf + FCPM_P1_EMW;	
	strcpy(s8Ptr, "GB/T19056V0409QT");
	s8Ptr = mSectorBuf + FCPM_P1_ICR1;	
	strcpy(s8Ptr, "FC01");
	WriteSectors(mSectorBuf, EMMC_PAGE, 1*EMMC_PAGE);	
	logd_lc("QUIT -->");
	// lc_dvr_mem_debug(mSectorBuf, EMMC_PAGE);
}

#define DUVONN_P0TC      0x1B8
#define DUVONN_P0CM1     0x1BC
#define DUVONN_P0CM2     0x1BD

#define DUVONN_P1TC      0x00
#define DUVONN_P1CM1     0x06
#define DUVONN_P1CM2     0x07
#define DUVONN_P1MC1     0x04
#define DUVONN_P1MC2     0x05
#define DUVONN_P1NDT     0x12


void lc_dvr_read_sector(int sector)
{
	char membuf[EMMC_PAGE];
	ReadSectors(membuf, EMMC_PAGE, sector*EMMC_PAGE);
	logd_lc("Show sector @ 0x%03x", sector);
	lc_dvr_mem_debug(membuf, EMMC_PAGE);
	logd_lc("en read sector!");
}

int lc_dvr_mmc_initial(void)
{
	int tryTime;
	int k=0, ret=0;
	off_t sectorAddr = 0;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	char *memBuf = (char*)malloc(4*1024);	
	char *tc_val;

	
	for(tryTime=0; tryTime<3; tryTime++){
		sectorAddr = 0*EMMC_PAGE;
		tc_val = memBuf + DUVONN_P0TC;
		memset(memBuf, 0, EMMC_PAGE);
		ReadSectors(memBuf, EMMC_PAGE, sectorAddr);		
		logd_lc("try(%d) sector %x", tryTime, sectorAddr);
		
		lc_dvr_mem_debug(memBuf, EMMC_PAGE);		
		
		if((tc_val[0]=='!')&&(tc_val[1]=='G')&&(tc_val[2]=='A')&&(tc_val[3]=='!'))
		{					
			sectorAddr = 0*EMMC_PAGE;			
			tc_val = memBuf + DUVONN_P0TC;			
			tc_val[0] = '!';
			tc_val[1] = 'g';
			tc_val[2] = 'a';
			tc_val[3] = '!';
			memBuf[DUVONN_P0CM1] = 0xff;
			memBuf[DUVONN_P0CM2] = 0x00;
			memBuf[0x1fe] = 0x55;
			memBuf[0x1ff] = 0xaa;
			ret = WriteSectors(memBuf, EMMC_PAGE, sectorAddr);
			
			logd_lc("allready initial!");
			lc_dvr_mem_debug(memBuf, EMMC_PAGE);
			return 0;
		}
		else if((tc_val[0]=='!')&&(tc_val[1]=='g')&&(tc_val[2]=='a')&&(tc_val[3]=='!'))
		{		
			break;
		}		
		else
		{
			logd_lc("into special mode!");
			sectorAddr = 0*EMMC_PAGE;			
			tc_val = memBuf + DUVONN_P0TC;			
			tc_val[0] = '!';
			tc_val[1] = 'g';
			tc_val[2] = 'a';
			tc_val[3] = '!';
			memBuf[DUVONN_P0CM1] = 0x00;
			memBuf[DUVONN_P0CM2] = 0xff;
			memBuf[0x1fe] = 0x55;
			memBuf[0x1ff] = 0xaa;
			ret = WriteSectors(memBuf, EMMC_PAGE, sectorAddr);		
		}
		
		if(tryTime==3)
		{
			free(memBuf);
			return -1;
		}
	}
	
	//------------------------------------------------
	logd_lc("Device is normal storage disk!");
	
	memset(memBuf, 0, 512);
	sectorAddr = 1*EMMC_PAGE;
	tc_val = memBuf+DUVONN_P1TC;
	tc_val[0] = '!';
	tc_val[1] = 'g';
	tc_val[2] = 'a';
	tc_val[3] = '!';
	memBuf[DUVONN_P1MC1] = 'l';
	memBuf[DUVONN_P1MC2] = 's';		
	
	memBuf[DUVONN_P1CM1] = 0x80;
	memBuf[DUVONN_P1CM2] = 0x80;
	
	memBuf[DUVONN_P1NDT+0] = 0x23;
	memBuf[DUVONN_P1NDT+1] = 0x04;
	memBuf[DUVONN_P1NDT+2] = 0x12;
	memBuf[DUVONN_P1NDT+3] = 0x14;
	memBuf[DUVONN_P1NDT+4] = 0x54;
	memBuf[DUVONN_P1NDT+5] = 0x23;
	WriteSectors(memBuf, EMMC_PAGE, sectorAddr);		

	sectorAddr = 2*EMMC_PAGE;
	ReadSectors(memBuf, EMMC_PAGE, sectorAddr); /**读取加密过的初始化命令**/
	logd_lc("show sector %x", sectorAddr);
	lc_dvr_mem_debug(memBuf, EMMC_PAGE);
		
	free(memBuf);
	close(mmcFd);
}

#define DIRECT_MEM_LEN	(64*1024)

int direct_file_initial(void)
{
	if(mFileSectorBuf==NULL)
	{
		int pageSize = DIRECT_MEM_LEN;
		mFileSectorBuf = memalign(getpagesize(), pageSize);
	}
	return 0;
}

int direct_file_unInitial(void)
{
	if(mFileSectorBuf!=NULL)
	{
		free(mFileSectorBuf);
		mFileSectorBuf = NULL;
	}
	
	return 0;
}


int direct_file_open(char *path)
{
	int retval;
	SemaphoreP();
	retval = open(path, O_RDWR|O_DIRECT, 606);
	SemaphoreV();
	return retval;
}

int direct_file_close(int fp)
{
	int retval;
	SemaphoreP();
	retval = close(fp);
	SemaphoreV();
	return retval;
}

int direct_file_write(int fp, char *p, int len, loff_t offset)
{
	int readByte, remain = len;
	int pageSize = DIRECT_MEM_LEN;
	int cLen, ret;
	char* s8Ptr = p;
	
	SemaphoreP();
	if (lseek64(fp, offset ,SEEK_SET) == -1 ){
		printf("WriteSectors1 lseek(%llx) err!!\n", offset);
		SemaphoreV();
		return (-1);//seek failed
	}

	for(;;)
	{
		cLen = remain>pageSize?pageSize:remain;
		memcpy(mFileSectorBuf, s8Ptr, cLen);
		ret = write(fp, mFileSectorBuf, cLen); /**连续写32K数据大概为3毫秒**/
		if(ret < 0) 
		{
			loge_lc("duvonn write Err!");
			break;
		}
		remain -= cLen;
		s8Ptr += cLen;
		
		if(remain == 0) break;	
	}

	SemaphoreV();
	return len-remain;
}

int direct_file_data_save(char* path, void* data, int len)
{
    int fd, result;
    int pageSize = getpagesize();
    int reaLen = (len+pageSize-1)&(~(pageSize-1));

    SemaphoreP();
    fd = open(path, O_RDWR | O_CREAT, 666);
    if(fd >= 0)
    {
        char *FileBuf = (char*)memalign(pageSize, reaLen);
        memcpy(FileBuf, data, len);
        logd_lc("Len:%d rLen:%d --> %d", len, reaLen, fd);
        result = write(fd, FileBuf, len);
        close(fd);
        free(FileBuf);
    }
    else
    {
        logd_lc("errno=%d,%s!\n", errno, strerror(errno));
    }
    SemaphoreV();
    return result;
}

#if 1
void lc_dvr_mmc_test(int seed)
{
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	char *memBuf = (char*)malloc(64*1024);
	loff_t realPosition = 0x260700000;
	int i, *s32Ptr = (int*)memBuf;
	
	srand(i);
	for(i=0; i<4; i++)
	{
		memset(memBuf, 0xff, 4096);
		logd_lc("Sector Read:%llx", realPosition);
		ReadSectors(memBuf, 4096, realPosition);
		lc_dvr_mem_debug(memBuf, 128);
		realPosition += 4096;
	}
	
	realPosition = 0x260700000;
	for(i=0; i<4; i++)
	{
		for(int k=0; k<1024; k++) s32Ptr[k] = rand();
		WriteSectors(memBuf, 4096, realPosition);
		logd_lc("Sector Write:%llx", realPosition);
		lc_dvr_mem_debug(memBuf, 128);
		realPosition += 4096;
	}	
}
#else
void lc_dvr_mmc_test(int seed)
{
	char path[256];
	
	int k=0, ret=0;
	off_t addrOffset = 0;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;
	char *memBuf = (char*)malloc(64*1024);	
	char *tc_val = memBuf + DUVONN_P0TC;
	lc_dvr_read_sector(0);
	memcpy(memBuf, mSectorOne, EMMC_PAGE);
	addrOffset = 0*EMMC_PAGE;	
	tc_val[0] = '!';
	tc_val[1] = 'g';
	tc_val[2] = 'a';
	tc_val[3] = '!';
	memBuf[DUVONN_P0CM1] = 0x00;
	memBuf[DUVONN_P0CM2] = 0xff;
	memBuf[0x1fe] = 0x55;
	memBuf[0x1ff] = 0xaa;
	ret = WriteSectors(memBuf, EMMC_PAGE, addrOffset);
	logd_lc("Write to MMC @ %llx", addrOffset);
	lc_dvr_mem_debug(memBuf, EMMC_PAGE);

	memset(memBuf, 0x00, pageSize);	
	addrOffset = 0*EMMC_PAGE;
	ReadSectors(memBuf, EMMC_PAGE, addrOffset);
	logd_lc("Read from MMC @ 0x%llx", addrOffset);
	lc_dvr_mem_debug(memBuf, EMMC_PAGE);	
	//------------------------------------------------
	//if((tc_val[0]=='!')&&(tc_val[1]=='g')&&(tc_val[2]=='a')&&(tc_val[3]=='!'))
	{
		logd_lc("Device is normal storage disk!");
		memset(memBuf, 0, 512);
		addrOffset = 1*EMMC_PAGE;
		tc_val = memBuf+DUVONN_P1TC;
		tc_val[0] = '!';
		tc_val[1] = 'g';
		tc_val[2] = 'a';
		tc_val[3] = '!';
		memBuf[DUVONN_P1MC1] = 'l';
		memBuf[DUVONN_P1MC2] = 's';		
		
		memBuf[DUVONN_P1CM1] = 0x80;
		memBuf[DUVONN_P1CM2] = 0x80;
		
		memBuf[DUVONN_P1NDT+0] = 0x23;
		memBuf[DUVONN_P1NDT+1] = 0x04;
		memBuf[DUVONN_P1NDT+2] = 0x12;
		memBuf[DUVONN_P1NDT+3] = 0x14;
		memBuf[DUVONN_P1NDT+4] = 0x54;
		memBuf[DUVONN_P1NDT+5] = 0x23;
		WriteSectors(memBuf, EMMC_PAGE, addrOffset);
		logd_lc("Write to MMC @ %llx", addrOffset);
		lc_dvr_mem_debug(memBuf, EMMC_PAGE);		
	}

	lc_dvr_read_sector(2);
	
	logd_lc("quit duvonn!");
	memset(memBuf, 0, 512);
	addrOffset = 1*EMMC_PAGE;
	tc_val = memBuf+DUVONN_P1TC;
	tc_val[0] = '!';
	tc_val[1] = 'g';
	tc_val[2] = 'a';
	tc_val[3] = '!';
	memBuf[DUVONN_P1MC1] = 'l';
	memBuf[DUVONN_P1MC2] = 's'; 	
	
	memBuf[DUVONN_P1CM1] = 0xff;
	memBuf[DUVONN_P1CM2] = 0x00;
	
	memBuf[DUVONN_P1NDT+0] = 0x23;
	memBuf[DUVONN_P1NDT+1] = 0x04;
	memBuf[DUVONN_P1NDT+2] = 0x12;
	memBuf[DUVONN_P1NDT+3] = 0x14;
	memBuf[DUVONN_P1NDT+4] = 0x54;
	memBuf[DUVONN_P1NDT+5] = 0x30;
	WriteSectors(memBuf, EMMC_PAGE, addrOffset);
	logd_lc("Write to MMC @ %llx", addrOffset);
	lc_dvr_mem_debug(memBuf, EMMC_PAGE);
	
	lc_dvr_read_sector(2);
	
	free(memBuf);
	close(mmcFd);
}
#endif
