#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <linux/falloc.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>  // mkfifo
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "lc_glob_type.h"
#include "lc_dvr_glob_def.h"
#include "lc_dvr_record_sys_func.h"
#include "lc_dvr_record_log.h"
#include "lc_dvr_RecordCamera.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_dbsave.h"
#include "StorageManager.h"
#include "duvonn_storage.h"

#define MAX_SUPPORT_CAM LC_DVR_SUPPORT_CAMERA_NUMBER 
#define FILE_NO_REMOVE
#define FALLOC_FL_KEEP_SIZE 0x01	/* default is extend size */
#define true  1
#define false 0

#define EXT_VIDEO EXT_VIDEO_MP4
#define SD_REMAIN_SPACE 1
#define EMERGENT_DIR "emergentVideo"
#define DBSAVE_BLOCK_LEN (160*1024)
#define RECORD_DBSAVE_INFO_PATH "/lcRecordInfoList.dat"
#define EMERGENCY_DIR	"emergencyEnv"
#define SOUNDREC_DIR	"soundRecord"
#define IMAGEREC_DIR	"ImageLoop"
#define FLOCK_DIR		"LockDir"
#define FUNTRUE_DEVICE  "/mnt/intsd"

#define EMERGENCY_DURATION 600
//#define DBSAVE_SHOW_FILE_INFO
#define TEMP_FILE_LOCK_NUM 8

char *pVideoPath[MAX_SUPPORT_CAM] = {
	"DMS",
	"ADAS",
	"BrakePedal",
	"rearVideo",
};

char *pPicPath[MAX_SUPPORT_CAM] = {
	"frontPicture0",
	"rearPicture1",
	"leftPicture2",
	"rightPicture3",
};

char *pPicApachePath[MAX_SUPPORT_CAM] = {
	"frontphoto",
	"rearphoto",
	"leftphoto",
	"rightphoto",
};

char *pSuffix[MAX_SUPPORT_CAM] = {
	"front", "rear", "left", "right",
};

typedef struct
{
    uint32_t alarm;     /* 报警标志位定义见表25 */
    uint32_t status;    /* 状态位定义见表24 */
    uint32_t latitude;  /* 纬度 */
    uint32_t longitude; /* 经度 */
    uint16_t altitude;  /* 高程 */
    uint16_t speed;     /* 速度 */
    uint16_t direction; /* 速度 */
    uint8_t bcdtime[6];/* GMT +8时间 */
}PositionMsgInfo; /* 必须使用单字节对齐，总共28字节 */

typedef struct
{
	recordInfoExt *lockFrom;
	recordInfoExt file_temp_lock;
}FILE_LOCK_INFO;

char g_PicPath[MAX_SUPPORT_CAM][PIC_NAME_LEN] = { };   //mod lss
char g_NamePic[MAX_SUPPORT_CAM][CM_MAX_PICTURE_NUM][PIC_NAME_LEN] = { };
char g_VideoDir[PIC_NAME_LEN] = {0};
char g_LicenseSet[PIC_NAME_LEN] = {"YB798154"};
static char g_CamToChar[MAX_SUPPORT_CAM] = {CMA_DMS, CMA_ADAS, CMA_BRAKE, CMA_REAR};
static int g_perFileSize[MAX_SUPPORT_CAM];
static file_status_t *g_emergency_video = NULL;
static file_status_t *g_sound_record = NULL;
static file_status_t *g_image_record = NULL;
static FILE_LOCK_INFO *g_file_temp_lock = NULL;

static int mStorageLime;
static int mInfoListInitial = 0;

RecordCamera *cameraInfo[MAX_SUPPORT_CAM]={NULL};
unsigned int gAFileOccupySizePerMinMb;

int RecordGenfilename(char *name, int camId);
int RecordSetVideoPath(char* path);

extern int fallocate(int fd, int mode, off_t offset, off_t len);

int Is_InfoList_Initial(void)
{
	return mInfoListInitial;
}

int Record_build_file_ID(int fileType, int camId, int listIdx)
{
	return ((fileType<<24)|(camId<<16)|listIdx);
}

void lc_dvr_set_storage_limit(int trate)
{
	mStorageLime = ((EMERGENCY_SIZE/100)*(SZ_1M/MAX_SUPPORT_CAM)*trate);
}

void lc_dvr_show_bcd_time(char *log, char* time)
{
	logd_lc("%s->%02x-%02x-%02x %02x:%02x:%02x",log, time[0], time[1], time[2], time[3], time[4], time[5]);
}

uint64_t lc_dvr_bcdTime_to_int(uint8_t time[])
{
    struct tm tt;
	int year, mon, day, hour, min, sec;

    year = ((time[0]>>4)&0x0f)*10+(time[0]&0x0f);
    mon  = ((time[1]>>4)&0x0f)*10+(time[1]&0x0f);
    day  = ((time[2]>>4)&0x0f)*10+(time[2]&0x0f);
    hour = ((time[3]>>4)&0x0f)*10+(time[3]&0x0f);
    min = ((time[4]>>4)&0x0f)*10+(time[4]&0x0f);
    sec = ((time[5]>>4)&0x0f)*10+(time[5]&0x0f);
    year += 2000;
	
    memset(&tt, 0, sizeof(tt));
    tt.tm_year = year - 1900;
    tt.tm_mon = mon - 1;
    tt.tm_mday = day;
    tt.tm_hour = hour;
    tt.tm_min = min;
    tt.tm_sec = sec;
	
    return (uint64_t)mktime(&tt);
}

void lc_dvr_int_to_bcdTime(uint8_t timeO[], uint64_t timeSec)
{
	struct tm *local = localtime((time_t*)(&timeSec));// time_t
	local->tm_year -= 100;
	local->tm_mon += 1;
	
	timeO[0] = local->tm_year/10;
	timeO[0] <<= 4;
	timeO[0] += local->tm_year%10;
	
	timeO[1] = local->tm_mon/10;
	timeO[1] <<= 4;
	timeO[1] += local->tm_mon%10;
	
	timeO[2] = local->tm_mday/10;
	timeO[2] <<= 4;
	timeO[2] += local->tm_mday%10;
	
	timeO[3] = local->tm_hour/10;
	timeO[3] <<= 4;
	timeO[3] += local->tm_hour%10;
	
	timeO[4] = local->tm_min/10;
	timeO[4] <<= 4;
	timeO[4] += local->tm_min%10;
	
	timeO[5] = local->tm_sec/10;
	timeO[5] <<= 4;
	timeO[5] += local->tm_sec%10;
}

int RecordGetPerFileSize(int camId)
{
	return g_perFileSize[camId];
}

void RecordGetInfoDir(char *path)
{
	sprintf(path,"%s/dvrInfo", g_VideoDir);
}

void Record_video_status_info_save(int camId, int index, int size, char* start, char* stop)
{
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	p->fileInfo[index].info.size += size;
	memcpy(p->fileInfo[index].info.startTime, start, 6);
	memcpy(p->fileInfo[index].info.stopTime, stop, 6);
	logd_lc("Cam %d file[%d]->s:%d t:{%02x:%02x-%02x:%02x}",camId,index,size,start[4],start[5],stop[4],stop[5]);	
}

void Record_fileInfo_dbsave(void)
{
	char namePath[256];
	int datLen = sizeof(recordInfo_t)*((LC_DVR_SUPPORT_CAMERA_NUMBER*CM_MAX_RECORDFILE_NUM)+CM_MAX_EMERGENCY_NUM
										+SOUND_REC_FILE_NUM+IMAGE_REC_FILE_NUM);
	recordInfo_t *dbSave = (recordInfo_t*)malloc(DBSAVE_BLOCK_LEN);
	file_status_t *p;
	int index = 0;
	
	memset(dbSave, 0, DBSAVE_BLOCK_LEN);	
	memset(namePath, 0, sizeof(256));
	RecordGetInfoDir(namePath);
	strcat(namePath, RECORD_DBSAVE_INFO_PATH);		

	for(int i=0; i<LC_DVR_SUPPORT_CAMERA_NUMBER; i++)
	{
		p = &(cameraInfo[i]->FileManger);
		for(int idx=0; idx<CM_MAX_RECORDFILE_NUM; idx++)
		{		
			dbSave[index++] = p->fileInfo[idx].info;
		}
	}

	p = g_emergency_video;
	if(p!=NULL)
	{
		for(int idx=0; idx<CM_MAX_EMERGENCY_NUM; idx++)
		{		
			dbSave[index++] = p->fileInfo[idx].info;		
		}
	}

	p = g_sound_record;
	if(p != NULL)
	{
		for(int idx=0; idx<SOUND_REC_FILE_NUM; idx++)
		{		
			dbSave[index++] = p->fileInfo[idx].info;
		}
	}

	p = g_image_record;	
	if(p != NULL)
	{
		for(int idx=0; idx<IMAGE_REC_FILE_NUM; idx++)
		{		
			dbSave[index++] = p->fileInfo[idx].info;
		}
	}
	
	lc_dvr_dbsave_content(namePath, dbSave, datLen, DBSAVE_BLOCK_LEN);
	free(dbSave);	
	logd_lc("fileInfo_save(%d)->%s",datLen, namePath);
}

void Record_fileInfo_initial(void)
{
	char namePath[256];
	char time[64];
	char *start;
	int datLen = sizeof(recordInfo_t)*((LC_DVR_SUPPORT_CAMERA_NUMBER*CM_MAX_RECORDFILE_NUM)+CM_MAX_EMERGENCY_NUM
										+SOUND_REC_FILE_NUM+IMAGE_REC_FILE_NUM);
	recordInfo_t *dbSave = (recordInfo_t*)malloc(DBSAVE_BLOCK_LEN);
	int index = 0, idx;	
	logd_lc("Record_fileInfo_initial->datLen:%d!!", datLen);
	memset(namePath, 0, sizeof(256));
	memset(dbSave, 0, DBSAVE_BLOCK_LEN);
	RecordGetInfoDir(namePath);
	strcat(namePath, RECORD_DBSAVE_INFO_PATH);

	if(lc_dvr_dbget_content(namePath, dbSave, DBSAVE_BLOCK_LEN)>0)
	{
		file_status_t *p;
		for(int i=0; i<LC_DVR_SUPPORT_CAMERA_NUMBER; i++)
		{
			p = &(cameraInfo[i]->FileManger);
			for(int idx=0; idx<CM_MAX_RECORDFILE_NUM; idx++)
			{
				if(p->fileInfo[idx].cur_filename[0]!=0)
				{
					start = dbSave[index].startTime;
					sprintf(time, "%02x%02x%02x%02x%02x%02x", start[0], start[1], start[2], start[3], start[4], start[5]);
					if(strstr(p->fileInfo[idx].cur_filename, time)!=NULL) /**文件名与记录一致**/
					{
						p->fileInfo[idx].info = dbSave[index];
#ifdef DBSAVE_SHOW_FILE_INFO		
						if(i == 0)
						{
							logd_lc("infVideo[%d]->%s", idx, p->fileInfo[idx].cur_filename);
							lc_dvr_show_bcd_time("start:",p->fileInfo[idx].info.startTime);
							lc_dvr_show_bcd_time("stop:",p->fileInfo[idx].info.stopTime);
						}
#endif						
					}
				}
				index++;
			}
		}

		p = g_emergency_video;
		if(p!=NULL)
		{
			for(int idx=0; idx<CM_MAX_EMERGENCY_NUM; idx++)
			{		
				if(p->fileInfo[idx].cur_filename[0]!=0)
				{
					start = dbSave[index].startTime;
					sprintf(time, "%02x%02x%02x%02x%02x%02x", start[0], start[1], start[2], start[3], start[4], start[5]);
//					if(strstr(p->fileInfo[idx].cur_filename, time)!=NULL) /**文件名与记录一致**/
					{
						p->fileInfo[idx].info = dbSave[index];
#ifdef DBSAVE_SHOW_FILE_INFO
						
						{
							logd_lc("infEmg[%d](%x)->%s", idx, p->fileInfo[idx].info.size, p->fileInfo[idx].cur_filename);
							lc_dvr_show_bcd_time("start:",p->fileInfo[idx].info.startTime);
							lc_dvr_show_bcd_time("stop:",p->fileInfo[idx].info.stopTime);
						}
#endif						
					}
				}
				index++;

			}
		}

		p = g_sound_record;
		if(p!=NULL)
		{
			for(int idx=0; idx<SOUND_REC_FILE_NUM; idx++)
			{		
				if(p->fileInfo[idx].cur_filename[0]!=0)
				{
					start = dbSave[index].startTime;
					sprintf(time, "%02x%02x%02x%02x%02x%02x", start[0], start[1], start[2], start[3], start[4], start[5]);
					if(strstr(p->fileInfo[idx].cur_filename, time)!=NULL) /**文件名与记录一致**/
					{
						p->fileInfo[idx].info = dbSave[index];
#ifdef DBSAVE_SHOW_FILE_INFO						
						logd_lc("infSound[%d]->%s", idx, p->fileInfo[idx].cur_filename);
						lc_dvr_show_bcd_time("start:",p->fileInfo[idx].info.startTime);
						lc_dvr_show_bcd_time("stop:",p->fileInfo[idx].info.stopTime);
#endif						
					}
				}
				index++;
			}	
		}
	}
	free(dbSave);
}

char* RecordGetLicense(void)
{
	return g_LicenseSet;
}

int xxCreateMyFile(char *szFileName, int nFileLength)
{
	int res, fd;
	
	struct timeval start, end;	
	int interval, temp;
	gettimeofday(&start, NULL);
	printf("xxCreateMyFile:%s\n", szFileName);
	fd = open(szFileName, O_RDWR | O_CREAT, 0666);

	if (fd < 0) {
		fprintf(stderr, "openfail line %d %s\n", __LINE__, strerror(errno));
		close(fd);
		return -1;
	}

	res = fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, nFileLength);
	if (res) {
		fprintf(stderr, "fallocate line %d len=%d %s\n", __LINE__, nFileLength, strerror(errno));
		close(fd);
		return -2;
	}
	else
	{		
		// printf("fallocate no error!\n");
	}		
	
	close(fd);

	gettimeofday(&end, NULL);
	temp = start.tv_sec*1000 + (start.tv_usec/1000);
	interval = end.tv_sec*1000 + (end.tv_usec/1000);
	// printf("cur:%d, pre:%d -> ", interval, temp);
	interval -= temp;
	printf("file %d byte used %dms\n", nFileLength, interval);

	return 0;
}

time_t getDateTime(struct tm **local_time)
{
	time_t timer;
	timer = time(NULL);
    *local_time = localtime(&timer);
	return timer;
}

int getFileSize(char *filename)
{
	struct stat statbuf;
	stat(filename, &statbuf);
	int size = statbuf.st_size >> 20;

	return size;
}

int getFileOccupySizeMb(char *filename)
{
	struct stat statbuf;
	stat(filename, &statbuf);
	/*文件实际占用空间=st_blocksize * st_blocks*/
	int blksize = statbuf.st_blocks >> 11;

	return blksize;
}

/**************************************
Function:
Description:
***************************************/
void RecordCameraInitial(void)
{
	int elementSize = LC_ALIGN_4K(sizeof(RecordCamera) + (sizeof(recordInfoExt)*(CM_MAX_RECORDFILE_NUM-16)));
	char* memBuf;
	char* pathDefault = (char*)sys_msg_get_memcache(128);
	if(cameraInfo[0] == NULL)
	{
		memBuf = malloc(elementSize*MAX_SUPPORT_CAM);
	}
	else
	{
		memBuf = cameraInfo[0];
	}
	memset(memBuf, 0, elementSize*MAX_SUPPORT_CAM);
	memset(pathDefault, 0, 128);
	logd_lc("RecordCameraInitial!!");
	gAFileOccupySizePerMinMb = 24;	//bit_rate=7M

	if(strlen(g_VideoDir)==0)
	{
		strcpy(pathDefault, LC_DVR_RECORD_BASE_DIR);
		strcpy(pathDefault+64, "YA74576");
		RecordSetVideoPath(pathDefault);
	}

	for(int i=0; i<MAX_SUPPORT_CAM; i++)
	{
		cameraInfo[i] = (RecordCamera*)(memBuf+i*elementSize);
		cameraInfo[i]->mCameraId = i;		
		cameraInfo[i]->storage_state = 0;
		cameraInfo[i]->iMaxPicNum = MAX_PIC_NUM;
		cameraInfo[i]->ppSuffix = pSuffix;
		cameraInfo[i]->mDuration = 60; /**默认1分钟**/
	}
	
	if(g_emergency_video == NULL)
	{
		int len = LC_ALIGN_4K(sizeof(RecordCamera) + (sizeof(recordInfoExt)*(CM_MAX_EMERGENCY_NUM-16)));
		g_emergency_video = (file_status_t*)malloc(len);
		memset(g_emergency_video, 0, len);
	}	

	if(g_sound_record==NULL)
	{
		int len = LC_ALIGN_4K(sizeof(RecordCamera) + (sizeof(recordInfoExt)*(SOUND_REC_FILE_NUM-16)));
		g_sound_record = (file_status_t*)malloc(len);
		memset(g_sound_record, 0, len);
	}

	if(g_image_record==NULL)
	{
		int len = LC_ALIGN_4K(sizeof(RecordCamera) + (sizeof(recordInfoExt)*(IMAGE_REC_FILE_NUM-16)));
		g_image_record = (file_status_t*)malloc(len);
		memset(g_image_record, 0, len);		
	}

	if(g_file_temp_lock==NULL)
	{
		int len = LC_ALIGN_4K(sizeof(FILE_LOCK_INFO)*TEMP_FILE_LOCK_NUM); 
		g_file_temp_lock = (FILE_LOCK_INFO*)malloc(len);
		memset(g_file_temp_lock, 0, len);
	}
	
	duvonn_suffering_initial(LC_DUVONN_CAMERA_NUMBER, RecordGetLicense());

	mStorageLime = ((EMERGENCY_SIZE/10)*(SZ_1M/MAX_SUPPORT_CAM)*9);
	mInfoListInitial = 1;
	logd_lc("ReInitial finish!");
}

void RecordCameraRelease(void)
{
	if(g_file_temp_lock!=NULL)
	{
		free(g_file_temp_lock);
	}

	if(cameraInfo[0]!=NULL)
	{
		free(cameraInfo[0]);
		cameraInfo[0] = NULL;
	}
	if(g_emergency_video!=NULL)
	{
		free(g_emergency_video);
		g_emergency_video = NULL;
	}

	if(g_sound_record!=NULL)
	{
		free(g_sound_record);
		g_sound_record = NULL;
	}

	if(g_image_record!=NULL)
	{
		free(g_image_record);
		g_image_record = NULL;
	}	
	duvonn_suffering_unInitial();
	mInfoListInitial = 0;
}

void RecordGenPicNameFromVideo(char* name)
{
	int len = strlen(name); /* 把视频的路径修改成缩略图的路径*/
	char picName[128];
	int i = len;
	if(len>0)
	{
		for(; i>0; --i)
		{
			if(name[i]=='/')
			{
				break;
			}
		}
		strcpy(picName, name+i+1);	
		strchr(picName, '.')[0] = '\0';	
		sprintf(name+i+1,"%s%s%s", CM_THUMB_DIR, picName, EXT_PIC);
	}
}

int RecordGetfileDir(char *name, int camId)
{
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	int len = sprintf(name,"%s/%s", g_VideoDir, p->cur_filedir);	
	return len;
}

int RecordInserDuvonnFileName(char* name, int camId, int index)
{
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	p->cur_file_idx = index;
	if (strlen(p->fileInfo[index].cur_filename)!=0)
	{						
		char buf2[256];
		strcpy(buf2, p->fileInfo[index].cur_filename);
		RecordGenPicNameFromVideo(buf2);
		logd_lc("RecordInserDuvonnFileName[%d]:%s", index, buf2);
		if (access(buf2, F_OK) == 0) {
			logd_lc("remove last picture!");
			remove(buf2);
		}
	}
	memset(p->fileInfo[index].cur_filename, 0, CM_MAX_FILE_LEN);
	strcpy(p->fileInfo[index].cur_filename, name);
	return 0;
}

/**把都万存储器的文件列表复制到总表来，方便之后的检索与图片的存储**/
int RecordInitListFromDuvonn(int camId)
{
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	int curIdx, maxFile;
	char dirPath[128];
	if(is_duvonn_device()==0) return -1;
	
	maxFile = duvonn_video_get_list_info(camId, &curIdx);	
	
	RecordGetfileDir(dirPath, camId);
	
	p->cur_max_filenum = maxFile;
	if(curIdx<0) curIdx+=maxFile;
	logd_lc("maxFile:%d curIdx:%d", maxFile, curIdx);
	for(int i=0; i<maxFile; i++)
	{
		char *fileName = duvonn_video_name_get(camId, curIdx);
		if(fileName!=NULL)
		{
			sprintf(p->fileInfo[curIdx].cur_filename, "%s/%s", dirPath, fileName);
			// logd_lc("duvonn[%d]->%s", curIdx, p->fileInfo[curIdx].cur_filename);
		}		
		else
		{
			memset(p->fileInfo[curIdx].cur_filename, 0, CM_MAX_FILE_LEN);
		}
		curIdx--;
		if(curIdx<0) curIdx += maxFile;
	}
	logd_lc("[%d]cur_file_idx -> %d", camId, p->cur_file_idx);
	return 0;
}

int RecordGenEventfile(char *name, int camId)
{
	struct tm *tm = NULL;
	int strP;		
	getDateTime(&tm);
	strP = sprintf(name, "%s/%s/%s_%d_", g_VideoDir, EMERGENT_DIR, STANDAR_CODE, camId);		
	strP += sprintf(name+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																 tm->tm_hour, tm->tm_min, tm->tm_sec, EXT_VIDEO);
	logd_lc("Emergent->%s", name);
	return 0;
}

int RecordGenfilename(char *name, int camId)
{
	char rec_filename[256];
	char real_path[256];
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	int cur_file_idx;
	int rmidx = 0;
	int ret;

	if (name == NULL)
		return -1;

	sprintf(real_path,"%s/%s", g_VideoDir, p->cur_filedir);	
	
	unsigned long long as = availSize(real_path);	
	printf("\nGenVideofile[%d]-->%s!!\n", p->cur_file_idx, real_path);
	//unsigned long afile_size = ((cameraInfo[camId]->mDuration * cameraInfo[camId]->bit_rate) >> 23) + (13 * cameraInfo[camId]->mDuration) / 60;
	int afile_blksize = (cameraInfo[camId]->mDuration / 60) * gAFileOccupySizePerMinMb;	/* 计算录像需要的空间*/
	logi_lc("as is %lluM,re is %dM dur=%d bitrate=0x%x", as, (int) (SD_REMAIN_SPACE*afile_blksize + RESERVED_SIZE), cameraInfo[camId]->mDuration, cameraInfo[camId]->encode_param.bit_rate);

	if(as == 0) return -1;

	p->cur_file_idx++;
	logd_lc("index->%d/%d", p->cur_file_idx, p->cur_max_filenum);
	if(p->cur_file_idx >= p->cur_max_filenum) /**单个文件由小改大(录像时间增长)后，可能出现序号大于cur_max_filenum的**/
	{
		int nullFileCnt = 0; /**文件不存在则检索后面是不是都不存在**/
		for(rmidx=(p->cur_file_idx+1);rmidx<CM_MAX_RECORDFILE_NUM; rmidx++)
		{
			if(p->fileInfo[rmidx].cur_filename[0]!=0) nullFileCnt++; /**检测该文件之后是否全部已经清空**/
		}												  /**如果存在大于当前最大文件数的序号，则优先清理这些文件**/

		if(nullFileCnt==0) //如果是最末尾的文件则从头开始循环
		{
			rmidx = p->cur_file_idx;
			if (strlen(p->fileInfo[rmidx].cur_filename)!=0)
			{								
				if (access(p->fileInfo[rmidx].cur_filename, F_OK) == 0){ /**超序的文件存在则直接删除**/
					char buf2[256];									
					strcpy(buf2, p->fileInfo[rmidx].cur_filename);
					RecordGenPicNameFromVideo(buf2);
					if (access(buf2, F_OK) == 0) {
						remove(buf2);
					}	
					remove(p->fileInfo[rmidx].cur_filename);
					memset(p->fileInfo[rmidx].cur_filename, 0, CM_MAX_FILE_LEN);
					logd_lc("remove last file(%d)!", rmidx);
				}
			}
			p->cur_file_idx = 0;
		}
	}

	//logd_lc("cur_file:%d, max_file:%d", p->cur_file_idx, p->cur_max_filenum);	
	cur_file_idx = p->cur_file_idx;
	rmidx = cur_file_idx;	
	
	do {
		as = availSize(real_path); //

		if (as > (SD_REMAIN_SPACE*afile_blksize + RESERVED_SIZE)) {
//			logd_lc("Have enough room!");
			break;				//we have enough room
		}
		else 
		{			
			if (strlen(p->fileInfo[rmidx].cur_filename) != 0) {
				logd_lc("dbg-rec as is %lldM,re is %dM", as, (int) (SD_REMAIN_SPACE*afile_blksize + RESERVED_SIZE));
				logd_lc("dbg-rec no av room!rm [%d] %s,FileSize=%dM,OccupySizeMb=%d", rmidx, p->fileInfo[rmidx].cur_filename, getFileSize(p->fileInfo[rmidx].cur_filename),
					  		getFileOccupySizeMb(p->fileInfo[rmidx].cur_filename));
				
				char buf2[256];
				strcpy(buf2, p->fileInfo[rmidx].cur_filename);
				RecordGenPicNameFromVideo(buf2);
				logd_lc("remove1 pic:%s", buf2);
				if (access(buf2, F_OK) == 0) {
					logd_lc("do1 dele pic!");
					remove(buf2);
				}

				if (access(p->fileInfo[rmidx].cur_filename, F_OK) != 0) {	/* 文件不存在，直接清空文件名*/
					logd_lc("file[%d] %s doesn't exist,pass this file.", rmidx,
						  p->fileInfo[rmidx].cur_filename);
					memset(p->fileInfo[rmidx].cur_filename, 0, CM_MAX_FILE_LEN);
					if (rmidx >= CM_MAX_RECORDFILE_NUM) {
						rmidx = 0;
					}
					else {
						rmidx++;
					}
					continue;
				}

				int oldfileSize = getFileOccupySizeMb(p->fileInfo[rmidx].cur_filename);		/* 修改录像时间长度，会需要调整文件的大小*/	

				if (afile_blksize <= (oldfileSize - gAFileOccupySizePerMinMb)) {	/* 旧文件的空间小于新文件或者远大于新文件则要删除后重新分配*/
					logd_lc("dbg-rec file size dismatch,remove idx[%d] file now,oldfilesize=%d", rmidx, oldfileSize);
					ret = remove(p->fileInfo[rmidx].cur_filename);	// this file is too large, it will occupy too more space, so delete it.
					if (ret == 0)
						memset(p->fileInfo[rmidx].cur_filename, 0, CM_MAX_FILE_LEN);
					else
						loge_lc("dbg-rec old file room is too large,but rm file %s fail %d", p->fileInfo[rmidx].cur_filename, errno);
				}
				else if ((afile_blksize <= oldfileSize) && (afile_blksize >= (oldfileSize - gAFileOccupySizePerMinMb))) {
					if (rmidx == cur_file_idx) {	/*can use curr file	，该文件的空间刚好合适*/
						logd_lc("dbg-rec no need to rm[%d],oldfilesize=%d,just use the old one", rmidx, oldfileSize);
						break;
					}

					logd_lc("dbg-rec rmidx != curr_idx,can not use,rm[%d]", rmidx);

					ret = remove(p->fileInfo[rmidx].cur_filename);	/* 删除该文件*/
					if (ret == 0) {
						memset(p->fileInfo[rmidx].cur_filename, 0, CM_MAX_FILE_LEN);
					}
					else {
						loge_lc("dbg-rec file room is match but rm file %s fail %d",
							  p->fileInfo[rmidx].cur_filename, errno);
					}
				}
				else {	
					logd_lc("dbg-rec need to rm[%d]", rmidx);
					ret = remove(p->fileInfo[rmidx].cur_filename);	// this file is too big, so delete it
					if (ret == 0) {
						memset(p->fileInfo[rmidx].cur_filename, 0, CM_MAX_FILE_LEN);
					}
					else {
						loge_lc("dbg-rec old file room is too small,but rm file %s fail %d",
							  p->fileInfo[rmidx].cur_filename, errno);
					}
				}
			}

			rmidx++;
			if (rmidx >= CM_MAX_RECORDFILE_NUM){
				rmidx = 0;
			}
		}
	}while (1);

	{
		struct tm *tm = NULL;
		int strP;		
		getDateTime(&tm);
		//GBT19056_XXXXXXXX_X_yyyymmddhhmmss
		strP =  sprintf(rec_filename, "%s%c%03d-%s_%s_%c_", real_path, '/', p->cur_file_idx, STANDAR_CODE, p->vehicle_license, g_CamToChar[camId]);		
		strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																	 tm->tm_hour, tm->tm_min, tm->tm_sec, EXT_VIDEO);
	}

	// int perfilesize = (gAFileOccupySizePerMinMb << 20) * (cameraInfo[camId]->mDuration / 60);
	
	if (access(p->fileInfo[cur_file_idx].cur_filename, F_OK) == 0) {	/* 文件存在，则在原有的文件上创建*/
		//remove thumb
		char buf3[256];
		strcpy(buf3, p->fileInfo[cur_file_idx].cur_filename);
		RecordGenPicNameFromVideo(buf3);
		logd_lc("remove2 pic:%s", buf3);
		if (access(buf3, F_OK) == 0) {
			logd_lc("do2 dele pic!");
			remove(buf3);
		}		
		
		logw_lc("dbg-rec t a file %s ", p->fileInfo[cur_file_idx].cur_filename);
		ret = remove(p->fileInfo[cur_file_idx].cur_filename);
		if (ret == 0) {
			logw_lc("dbg-rec remove and then create it again");
			// ret = xxCreateMyFile(rec_filename, perfilesize);	//this is needed to prevent kernel crash
		}
		else {
			loge_lc("dbg-rec old file OccupySize is not match the new one ,but rm old file err%d", errno);
		}
	}
	else {
		logd_lc("dbg-rec f a file %s", rec_filename);
		// xxCreateMyFile(rec_filename, perfilesize); /**如果是全新的文件则直接创建**/
	}
	
	p->fileInfo[cur_file_idx].info.size = 0;
	strcpy(p->fileInfo[cur_file_idx].cur_filename, rec_filename);
	strcpy(name, rec_filename);

	return 0;
}

/**************************************
Function:
Description:
***************************************/
int RecordVideoEncParmInit(int camId)
{
	int duraton = vc_record_video_para_get(VC_IDX_DURATION);
	int bitrate = vc_record_video_para_get(VC_IDX_BITRATE);
	int miniBitRate = ((bitrate*60>>3)+1023);
	miniBitRate >>= 10;
	gAFileOccupySizePerMinMb = miniBitRate+2;

	memset(&cameraInfo[camId]->encode_param, 0, sizeof(encode_param_t));
	cameraInfo[camId]->mDuration = duraton;
	cameraInfo[camId]->encode_param.bit_rate = bitrate * 1024; /**输入的bitrate 以K为单位，这里转成byte**/
	g_perFileSize[camId] = (gAFileOccupySizePerMinMb << 20) * (cameraInfo[camId]->mDuration / 60);
	logd_lc("VideoEncParmInit[%d]->%d", camId, g_perFileSize[camId]);
	logd_lc("encode.bitrate:%d, bitrate:%d", cameraInfo[camId]->encode_param.bit_rate, bitrate);
	return 0;
}

int RecordGetCurrentIndex(file_status_t* fileStatus, int num)
{
	struct tm fileTime;
	unsigned long u64Time;
	unsigned long timeLast = 0;
	char curFile[256], *s8Ptr;
	int lastIndex = -1;
	int bSize;

	for(int i=0; i<num; i++)
	{
		bSize = strlen(fileStatus->fileInfo[i].cur_filename);
		if(bSize>0)
		{
			memcpy(curFile, fileStatus->fileInfo[i].cur_filename, bSize);
			curFile[bSize] = '\0';
			for(int k=bSize; k>0; k--)
			{
				if(curFile[k]=='_')
				{
					s8Ptr = curFile+k+1;
					break;
				}
			}

			fileTime.tm_year = (s8Ptr[0]-'0')*1000;
			fileTime.tm_year += (s8Ptr[1]-'0')*100;
			fileTime.tm_year += (s8Ptr[2]-'0')*10;
			fileTime.tm_year += (s8Ptr[3]-'0');
			fileTime.tm_year -= 1900;
			
			fileTime.tm_mon = (s8Ptr[4]-'0')*10;
			fileTime.tm_mon += (s8Ptr[5]-'0');

			fileTime.tm_mday = (s8Ptr[6]-'0')*10;
			fileTime.tm_mday += (s8Ptr[7]-'0');

			fileTime.tm_hour = (s8Ptr[8]-'0')*10;
			fileTime.tm_hour += (s8Ptr[9]-'0');

			fileTime.tm_min = (s8Ptr[10]-'0')*10;
			fileTime.tm_min += (s8Ptr[11]-'0');

			fileTime.tm_sec = (s8Ptr[12]-'0')*10;
			fileTime.tm_sec += (s8Ptr[13]-'0');
			u64Time = mktime(&fileTime);
			//logd_lc("[%d]:%d-%d-%d_%d:%d:%d --> %ld", i, fileTime.tm_year, fileTime.tm_mon, fileTime.tm_mday, fileTime.tm_hour, fileTime.tm_min, fileTime.tm_sec, u64Time);
			if(u64Time >= timeLast)
			{
				timeLast = u64Time;
				lastIndex = i;
			}
		}
	}
	
	return lastIndex;
}

int RecordFileMaxNumber(void)
{
	return CM_MAX_RECORDFILE_NUM;
}

char* RecordFileFetchByIndex(int chanel, int index)
{
	file_status_t *p = &(cameraInfo[chanel]->FileManger);
	return p->fileInfo[index].cur_filename;
}

int RecordFileListFetch(void* fetchInfo)
{
	LC_RECORD_LIST_FETCH* listInfo = (LC_RECORD_LIST_FETCH*)fetchInfo;
	file_status_t *p = &(cameraInfo[listInfo->camChnl]->FileManger);
	int curIdx, endIdx, camId = listInfo->camChnl;
	GBT19056_FILE_FETCH_DAT* fileList = (GBT19056_FILE_FETCH_DAT*)listInfo->fileList;
	int totalCnt=0, realNum=0, dirPosition = 0;	

	if(listInfo->offset>=0) curIdx = listInfo->offset;
	else curIdx = p->cur_file_idx;

	endIdx = p->cur_file_idx;
	
	do{
		if(p->fileInfo[curIdx].cur_filename[0]!=0)
		{
			if(dirPosition == 0)/**获取文件夹路径**/
			{
				char* strPtr = p->fileInfo[curIdx].cur_filename;
				for(int ps = (CM_MAX_FILE_LEN-1); ps>0; ps--)
				{
					if(strPtr[ps]=='/')
					{
						dirPosition = ps+1;
						break;
					}
				}
				
				memset(listInfo->fileDir, 0, 56);
				memcpy(listInfo->fileDir, p->fileInfo[curIdx].cur_filename, dirPosition);	
			}

			fileList->size = p->fileInfo[curIdx].info.size;
			memcpy(fileList->fileName, p->fileInfo[curIdx].cur_filename+dirPosition, 48);
			memcpy(fileList->start_time, p->fileInfo[curIdx].info.startTime, 6);
			memcpy(fileList->stop_time, p->fileInfo[curIdx].info.stopTime, 6);
			fileList->id = Record_build_file_ID(LC_NORMAL_VIDEO_RECORD, camId, curIdx);
			
			// logd_lc("found[%d]-->%s", curIdx, fileList);
			
			realNum++;
			if(realNum>=listInfo->fileNumber)
			{
				curIdx--;
				if(curIdx<0) curIdx = CM_MAX_RECORDFILE_NUM-1;			
				break;
			}
			fileList++;
		}
		
		totalCnt++;
		if(totalCnt == CM_MAX_RECORDFILE_NUM) 
		{
			curIdx = 0;
			break;
		}
		
		curIdx--;
		if(curIdx<0) curIdx = CM_MAX_RECORDFILE_NUM-1;		
	}while(endIdx != curIdx);

	listInfo->offset = curIdx;
	listInfo->bytePerFile = sizeof(GBT19056_FILE_FETCH_DAT);
	listInfo->fileNumber = realNum;

	return realNum;
}

/**************************************
Function:
Description:
***************************************/
int RecordInitFileListDir(int camId)
{
	DIR *dir_ptr;
	struct dirent *direntp;
	unsigned int i;
	int idx;
	char tmpbuf[256];
	char rmbuf[256];

	char real_path[256];
	char camera_path[256];
	char thumb_path[256];
	int lastFile = 0;
	char *dirname = g_VideoDir;

	file_status_t *p = &(cameraInfo[camId]->FileManger);

	unsigned long long totMB = 0;
	int rmret = 0;
	int l32_CameraId = cameraInfo[camId]->mCameraId;
	strcpy(p->vehicle_license, RecordGetLicense());
	
	loge_lc("----RecordInitFileListDir(%d)----", camId);
	loge_lc("vehicle_license --> %s", p->vehicle_license);
	
	if (l32_CameraId > MAX_SUPPORT_CAM) {
		l32_CameraId = 0;
	}

	if (dirname == NULL) {
		loge_lc("dirname == null");
		return false;
	}

	int len = strlen(dirname);

	if (len < 2) {
		loge_lc("video dir can toshort ");
		return false;
	}

	strcpy(tmpbuf, dirname);
	for (i = 0; i < len; i++) {
		if (tmpbuf[len - 1 - i] == '/') {
			tmpbuf[len - 1 - i] = 0;
		}
		else {
			break;
		}
	}

	rmret = readlink(tmpbuf, real_path, sizeof(real_path));
	if (rmret < 0) {
		logd_lc("mount path not a symlink %s err=%d", tmpbuf, errno);
		strcpy(real_path, tmpbuf);
	}
	else {
		logd_lc("mount real path is %s \r\n", real_path);
	}
#if 0
	if (isMounted(real_path)) {
		logd_lc("-------sdmmc------mounted");
		cameraInfo[camId]->storage_state = 1;
	}
	else {
		loge_lc("!!!-------sdmmc------unmounted");
		return false;
	}
#endif
	
	memset(p->cur_filedir, 0, CM_MAX_FILE_LEN);
	strcpy(p->cur_filedir, pVideoPath[l32_CameraId]);

//	memset(tmpbuf, 0, sizeof(256));
//	sprintf(tmpbuf, "%s/%s", g_VideoDir, EMERGENT_DIR);	

//	if (createdir(tmpbuf) != 0) {
//		loge_lc("create emergent_path dir %s fail", tmpbuf);
//		return false;
//	}	

	RecordGetInfoDir(tmpbuf);
	logd_lc("dvrInfo:%s", tmpbuf);
	if (createdir(tmpbuf) != 0) {
		loge_lc("create dvrInfodir %s fail", tmpbuf);
		return false;
	}
	
	memset(camera_path, 0, sizeof(camera_path));
	sprintf(camera_path, "%s/%s", real_path, pVideoPath[l32_CameraId]);	
	strcpy(tmpbuf, camera_path);

	if (createdir(tmpbuf) != 0) {
		loge_lc("create camera_path dir %s fail", tmpbuf);
		return false;
	}

	memset(thumb_path, 0, sizeof(thumb_path));
	sprintf(thumb_path, "%s/%s", camera_path, CM_THUMB_DIR);
	strcpy(tmpbuf, thumb_path);
	if (createdir(tmpbuf) != 0) {
		loge_lc("create thumb dir %s fail", tmpbuf);
		//return false;
	}

//	memset(g_PicPath[l32_CameraId], 0, sizeof(g_PicPath[l32_CameraId]));
//	sprintf(g_PicPath[l32_CameraId], "%s/%s", real_path, (char *)pPicPath[l32_CameraId]);
//	strcpy(tmpbuf, g_PicPath[l32_CameraId]);
//	if (createdir(tmpbuf) != 0) {
//		loge_lc("create picture_path dir %s fail", tmpbuf);
//		return false;
//	}

	memset(tmpbuf, 0, 256);
	sprintf(tmpbuf, "%s/%s", real_path, pPicApachePath[l32_CameraId]);	

	if (createdir(tmpbuf) != 0) {
		loge_lc("create apache photo dir %s fail", tmpbuf);
		return false;
	}

//create parkmonitor path

//	memset(rmbuf, 0, sizeof(rmbuf));
//	sprintf(rmbuf, "%s/%s", real_path, PARK_MONITOR);
//	strcpy(tmpbuf, rmbuf);
//	if (createdir(tmpbuf) != 0) {
//		loge_lc("create parkMonitor dir %s fail", tmpbuf);
//		//return false;
//	}

	if ((dir_ptr = opendir(camera_path)) == NULL){
	}
	else{
		totMB = totalSize(real_path);	/* 文件夹为空，需要重新设置参数，bitrate = byte>>3*/
		unsigned long afile_size = ((cameraInfo[camId]->mDuration * cameraInfo[camId]->encode_param.bit_rate) >> 23) + (3 * cameraInfo[camId]->mDuration) / 60;
		/**每段1分钟，码率为4MBit 换算成Byte则为 60*4*1024*1024 >> (20+3)，其中3为byte 转 bit**/
		logw_lc("----total size=%llu,afilesize =%lu syctime=%d bit=0x%x", totMB, afile_size, cameraInfo[camId]->mDuration, cameraInfo[camId]->encode_param.bit_rate);
		if (afile_size == 0) {
			logw_lc("get afilesize =0");
			afile_size = (totMB - EMERGENCY_SIZE - SOUND_REC_SIZE - IMAGE_REC_SIZE - RESERVED_SIZE) / CM_MAX_RECORDFILE_NUM;
		}

		//= (bit_rate*60/8)+13 => bit_rate*7.5+13 ==>bit_rate*8+13 ==>(bit_rate>>20)<<3 + 13==>(bit_rate>>17)+ 13;
		/**计算文件最小占用的空间大小**/
		if (gAFileOccupySizePerMinMb < ((cameraInfo[camId]->encode_param.bit_rate >> 17) + 2)) {
			gAFileOccupySizePerMinMb = (cameraInfo[camId]->encode_param.bit_rate >> 17) + 2;
		}
		else {
			logd_lc("dbg-rec only use the higher bit_rate[%d] to calc A file occupy size",
				  (gAFileOccupySizePerMinMb - 23) >> 3);
		}
		logd_lc("gAFileOccupySizePerMinMb is : %d", gAFileOccupySizePerMinMb);
		if(is_duvonn_device())
		{
			if((camId==LC_DUVONN_CHANEL_0)||(camId==LC_DUVONN_CHANEL_0))
			{
				p->cur_max_filenum = 55; /**确定最大文件数量，注意每个摄像头的文件数量是一样的**/
			}
			else
			{
				p->cur_max_filenum = (totMB - EMERGENCY_SIZE - SOUND_REC_SIZE - IMAGE_REC_SIZE - RESERVED_SIZE) / ((cameraInfo[camId]->mDuration / 60) * gAFileOccupySizePerMinMb*2);
			}
		}
		else
			p->cur_max_filenum = (totMB - EMERGENCY_SIZE - SOUND_REC_SIZE - IMAGE_REC_SIZE- RESERVED_SIZE) / ((cameraInfo[camId]->mDuration / 60) * gAFileOccupySizePerMinMb*4);
		
		if (p->cur_max_filenum > CM_MAX_RECORDFILE_NUM)	p->cur_max_filenum = CM_MAX_RECORDFILE_NUM;

		//config_cam_set_normalvideomax(cameraInfo[camId]->mCameraId, p->cur_max_filenum);
		logd_lc("dbg-rec max_filenum=%d", p->cur_max_filenum);

		//logw_lc("----max file num =%d ",p->cur_max_filenum);
		logw_lc("AW :TotalMB= %lluMB  afile_size = %luMB -max_file_num =%d\n", totMB, afile_size,
 			  p->cur_max_filenum);

		//LOGI("--------dvr mode%d------%d,%d,%d,%d",p->camera_mode,totMB,afile_size,p->cur_max_filenum,p->pcamera_param->continue_record);
		while ((direntp = readdir(dir_ptr)) != NULL) {
			//logw_lc("file[%d]-->%s", p->cur_dir_file_num, direntp->d_name);
			for (i = 0; i < strlen(direntp->d_name); i++) {
				
				if (direntp->d_name[i] == '-') { /* 文件格式为： 序号-年月日_时分秒_ pSuffix[].mp4*/				
					memset(tmpbuf, 0, 256);
					memcpy(tmpbuf, direntp->d_name, i);
					idx = atoi(tmpbuf);
					if ((idx < CM_MAX_RECORDFILE_NUM) && (idx >= 0)) {						
						if (strlen(p->fileInfo[idx].cur_filename) == 0) {
							strcpy(p->fileInfo[idx].cur_filename, camera_path);
							strcat(p->fileInfo[idx].cur_filename, "/");
							strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
							// logd_lc("cur file %d %s ",idx,p->fileInfo[idx].cur_filename);
							p->cur_dir_file_num++;
						}
						else {
							memset(rmbuf, 0, 256);
							strcat(rmbuf, direntp->d_name);
							if (strstr(p->fileInfo[idx].cur_filename, rmbuf) == NULL) {
								logw_lc("find duplicate  so rm old %s idx=%d  name=%s \n", p->fileInfo[idx].cur_filename, idx, direntp->d_name);
								rmret = remove(p->fileInfo[idx].cur_filename);
								logw_lc("rmfile ret %s", rmret == 0 ? "OK" : "FAIL");
								strcpy(p->fileInfo[idx].cur_filename, camera_path);
								strcat(p->fileInfo[idx].cur_filename, "/");
								strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
								logw_lc("newidx file %s len=%d", p->fileInfo[idx].cur_filename, strlen(p->fileInfo[idx].cur_filename));
								//remove thumb
								strcpy(tmpbuf, p->fileInfo[idx].cur_filename);
								RecordGenPicNameFromVideo(tmpbuf);										
								remove(tmpbuf); /* 删除该文件对应的缩略图*/
							}
						}
						//LOGI("parse success  idx= %d  name= %s \n",idx,direntp->d_name);
					}
				}
			}
		}

		p->cur_file_idx = RecordGetCurrentIndex(p, CM_MAX_RECORDFILE_NUM);
		closedir(dir_ptr);

		if(p->cur_dir_file_num)
		{
			lastFile = p->cur_file_idx;			
			logd_lc("lastFile[%d]->%s", p->cur_file_idx, p->fileInfo[lastFile].cur_filename);
		}	
	}
	return true;
}

int RecordEmergencyListFetch(void* fetchInfo)
{
	LC_RECORD_LIST_FETCH* listInfo = (LC_RECORD_LIST_FETCH*)fetchInfo;
	file_status_t *p = g_emergency_video;
	int curIdx, endIdx;
	int cType, wantType, chanel;
	GBT19056_FILE_FETCH_DAT* fileList = (GBT19056_FILE_FETCH_DAT*)listInfo->fileList;	
	int totalCnt=0, realNum=0, dirPosition = 0;	
	char tempPath[128];
	int camId, len;

	if(listInfo->offset>=0) curIdx = listInfo->offset;
	else curIdx = p->cur_file_idx;
	endIdx = p->cur_file_idx;

	wantType = listInfo->fileType;
	chanel = listInfo->camChnl;

	logd_lc("wt:%d, chl:%d endIndex = %d",wantType, chanel, endIdx);
	do{
		if(p->fileInfo[curIdx].cur_filename[0]!=0)
		{
			cType = (p->fileInfo[curIdx].info.size>>LC_EMERGENCY_MASK_BIT)&0x03;

			strcpy(tempPath, p->fileInfo[curIdx].cur_filename);
			len = strlen(tempPath);
			
			for(int s=len-1; s>=0; s--)
			{
				if(tempPath[s]=='_')
				{
					s--;
					for(camId=0; camId<MAX_SUPPORT_CAM; camId++)
					{
						if(tempPath[s]==g_CamToChar[camId]) break;
					}					
					break;
				}
			}
				
			if((cType == wantType)&&(camId==chanel))
			{								
				if(dirPosition == 0)/**获取文件夹路径**/
				{
					char* strPtr = tempPath;
					for(int ps = (len-1); ps>0; ps--)
					{
						if(strPtr[ps]=='/')
						{
							dirPosition = ps+1;
							break;
						}
					}										
					memset(listInfo->fileDir, 0, 56);
					memcpy(listInfo->fileDir, tempPath, dirPosition);	
				}
				
				{			
					FILE *fp =	fopen(p->fileInfo[curIdx].cur_filename, "rb");
					int fileLen, type;
					
					fileLen = p->fileInfo[curIdx].info.size&(~0xc0000000);
					type = p->fileInfo[curIdx].info.size&0xc0000000;
					type >>= LC_EMERGENCY_MASK_BIT;
					
					if(fp!=NULL)
					{
						fseeko64(fp, (__off64_t)0, SEEK_END);
						fileLen = (int)ftello64(fp);
						p->fileInfo[curIdx].info.size &= 0xc0000000;
						p->fileInfo[curIdx].info.size += fileLen;
						fclose(fp);
					}
					
					fileList->size = fileLen;				
					memcpy(fileList->fileName, tempPath+dirPosition, 44);
					memcpy(fileList->start_time, p->fileInfo[curIdx].info.startTime, 6);
					memcpy(fileList->stop_time, p->fileInfo[curIdx].info.stopTime, 6);	
					fileList->id = Record_build_file_ID(cType, camId, curIdx);					
				}
				
//				logd_lc("Efile:%s, size:%x, w:%d", fileList->fileName, fileList->size, cType);
//				lc_dvr_show_bcd_time("start:",fileList->start_time);
//				lc_dvr_show_bcd_time("stop:",fileList->stop_time);

				realNum++;
				if(realNum>=listInfo->fileNumber)
				{
					curIdx--;
					if(curIdx<0) curIdx = CM_MAX_EMERGENCY_NUM-1;			
					break;
				}
				fileList++;
			}
			// logd_lc("Efile:%s, c:%d, w:%d", p->fileInfo[curIdx].cur_filename, cType, wantType);
		}
		
		totalCnt++;
		if(totalCnt == CM_MAX_EMERGENCY_NUM) 
		{
			curIdx = 0;	//遍历完所有的文件
			break;
		}
		
		curIdx--;
		if(curIdx<0) curIdx = CM_MAX_EMERGENCY_NUM-1;				
	}while(endIdx != curIdx);

	listInfo->offset = curIdx;
	listInfo->bytePerFile = sizeof(GBT19056_FILE_FETCH_DAT);
	listInfo->fileNumber = realNum;

	return realNum;
}

void RecordTimeToBCD(time_t *timeIn, char *timeOut)
{
	struct tm *tm = NULL;
	int year, month; 
	tm = localtime(timeIn); 		

	year = tm->tm_year-100;
	timeOut[0] = year/10;
	timeOut[0] <<= 4;
	timeOut[0] += year%10;

	month = tm->tm_mon + 1;
	timeOut[1] = month/10;
	timeOut[1] <<= 4;
	timeOut[1] += month%10;

	timeOut[2] = tm->tm_mday/10;
	timeOut[2] <<= 4;
	timeOut[2] += tm->tm_mday%10;

	timeOut[3] = tm->tm_hour/10;
	timeOut[3] <<= 4;
	timeOut[3] += tm->tm_hour%10;

	timeOut[4] = tm->tm_min/10;
	timeOut[4] <<= 4;
	timeOut[4] += tm->tm_min%10;

	timeOut[5] = tm->tm_sec/10;
	timeOut[5] <<= 4;
	timeOut[5] += tm->tm_sec%10;	
}

void RecordEmergencyFinsh(char *name, int size)
{
	file_status_t *p = g_emergency_video;
	int cur_file_idx = p->cur_file_idx;
	int len = strlen(name);
	time_t timer = time(NULL);

	for(int i=len-1; i>=0; i--)
	{
		if(name[i]=='/')
		{
			cur_file_idx = atoi(name+i+1);
			break;
		}
	}
	
	{
		int fileType = p->fileInfo[cur_file_idx].info.size&=0xc0000000;
		if(fileType == (LC_SPECIAL_EMERGNECY_EVENT<<LC_EMERGENCY_MASK_BIT))
		{
			timer -= 60;
		}
		RecordTimeToBCD(&timer, p->fileInfo[cur_file_idx].info.stopTime);
		p->fileInfo[cur_file_idx].info.size&=0xc0000000;
		p->fileInfo[cur_file_idx].info.size += size;
		logd_lc("EmergencyFinsh(%d):%s Size:%x", cur_file_idx, p->fileInfo[cur_file_idx].cur_filename, p->fileInfo[cur_file_idx].info.size);	
	}

	//logd_lc("EmgStop[%d]", cur_file_idx);
	//lc_dvr_show_bcd_time("stopTime:",p->fileInfo[cur_file_idx].info.stopTime);
}

int RecordGetEmergencySize(int sec)
{
	return ((sec*cameraInfo[0]->encode_param.bit_rate>>3));
}

int RecordGetCurRecordInfoIndex(int type, int chanel)
{
	file_status_t *p = g_emergency_video;
	switch(type)
	{
		case LC_NORMAL_VIDEO_RECORD:
			p = &cameraInfo[chanel]->FileManger;
			break;
		case LC_NORMAL_EMERGNECY_EVENT:
		case LC_SPECIAL_EMERGNECY_EVENT:
			p = g_emergency_video;
			break;
		case LC_NORMAL_AUDIO_RECORD:
			p = g_sound_record;
			break;
		case LC_LOOP_IMAGE_RECORD:
			p = g_image_record;
			break;			
	}	
	logd_lc("RecordGetCurRecordInfoIndex->%d", p->cur_file_idx);
	return p->cur_file_idx;
}

void RecordSetCurEmergerncyType(int warnType)
{
	file_status_t *p = g_emergency_video;
	int cur_file_idx = p->cur_file_idx;
	uint32_t size =	p->fileInfo[cur_file_idx].info.size&0x3fffffff;
	
	size |= (warnType<<LC_EMERGENCY_MASK_BIT);
	p->fileInfo[cur_file_idx].info.size = size;
	logd_lc("EmergencyType -> %x", size);

	if(warnType == LC_SPECIAL_EMERGNECY_EVENT)
	{
		uint64_t cTime = lc_dvr_bcdTime_to_int(p->fileInfo[cur_file_idx].info.startTime);
		cTime -= 60;
		lc_dvr_int_to_bcdTime(p->fileInfo[cur_file_idx].info.startTime, cTime);		
	}
	lc_dvr_show_bcd_time("realStart:", p->fileInfo[cur_file_idx].info.startTime);
}

int RecordGenEmergencyFile(char *name, int duration, int camId)
{
	char rec_filename[128];
	char real_path[128];
	char tempPath[128];
	file_status_t *p = g_emergency_video;
	int cur_file_idx;
	char *idxPos1, *idxPos2;
	int firstDel = -1;
	int ret;

	if (name == NULL)
		return -1;

	sprintf(real_path,"%s/%s", g_VideoDir, p->cur_filedir);	
	
	unsigned long long as = availSize(real_path);	
	printf("GenEmergency[%d]-->%s!!\n", p->cur_file_idx, real_path);
	if(as == 0) return -1;
	p->cur_file_idx++;
	
	logd_lc("index->%d/%d", p->cur_file_idx, CM_MAX_EMERGENCY_NUM);
	if(p->cur_file_idx >= CM_MAX_EMERGENCY_NUM)
	{
		p->cur_file_idx = 0;
	}
	
	{
		int fIndex, fileCnt = 0;		
		unsigned long long fileTotalSize = 0;
		int needSize = RecordGetEmergencySize(duration)+(512*1024);
		char buf3[128];

		for(fIndex=0; fIndex<CM_MAX_EMERGENCY_NUM; fIndex++)
		{
			if(p->fileInfo[fIndex].cur_filename[0]!=0)
			{
				fileTotalSize += (p->fileInfo[fIndex].info.size&0x3fffffff);
				// logd_lc("emgFile:%s, ocupylen:%x (%x)", p->fileInfo[fIndex].cur_filename, (p->fileInfo[fIndex].info.size&0x3fffffff), (p->fileInfo[fIndex].info.size&0x3fffffff));
			}
		}
		
		fIndex = p->cur_file_idx;
		fileTotalSize += needSize;
		logd_lc("EmgFileTotalSize[%d]->%lld, max:%d", fIndex, fileTotalSize, (EMERGENCY_SIZE*(SZ_1M)));		
		do{
			if(p->fileInfo[fIndex].cur_filename[0]!=0)
			{				
				if(firstDel<0) firstDel = fIndex;

				strcpy(tempPath, p->fileInfo[fIndex].cur_filename);
				strcpy(buf3, tempPath);
				RecordGenPicNameFromVideo(buf3);	/**删除对应摄像头目录下的文件**/					
				if (access(buf3, F_OK) == 0) {
					remove(buf3);
				}		

				logw_lc("Remove Pic&Vi %s ", tempPath);
				ret = remove(tempPath);

				memset(p->fileInfo[fIndex].cur_filename, 0, CM_MAX_FILE_LEN);
				fileTotalSize -= (p->fileInfo[fIndex].info.size&0x3fffffff);				
				logd_lc("(%x)curSize[%d]->%lld",p->fileInfo[fIndex].info.size, fIndex, fileTotalSize);
				memset(&p->fileInfo[fIndex], 0, sizeof(recordInfoExt));
			}
			
			fIndex++;
			if(fIndex >= CM_MAX_EMERGENCY_NUM)
			{
				fIndex = 0;
			}			
		}while((fileTotalSize)>(EMERGENCY_SIZE*(SZ_1M)));
		
		if(fileTotalSize > mStorageLime)
		{			
			int remainSize = fileTotalSize*100/(EMERGENCY_SIZE*(SZ_1M));
			vc_message_send(ENU_LC_DVR_EMERGENCY_SPACE_OVERLIMIT, remainSize, NULL, NULL);
		}
	}
	
	if(firstDel>=0) p->cur_file_idx = firstDel;
	
	cur_file_idx = p->cur_file_idx;

	{
		struct tm *tm = NULL;
		char startTime[6];
		time_t timer;
		int strP;		
		timer = getDateTime(&tm);		
		RecordTimeToBCD(&timer, p->fileInfo[cur_file_idx].info.startTime);
		
		//GBT19056_XXXXXXXX_X_yyyymmddhhmmss
		strP =  sprintf(rec_filename, "%s/%03d-%s_%s_%c_", real_path, cur_file_idx, STANDAR_CODE, p->vehicle_license, g_CamToChar[camId]);		
		strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																	 tm->tm_hour, tm->tm_min, tm->tm_sec, ".TS");
	}

	p->fileInfo[cur_file_idx].info.size = 0;
	strcpy(p->fileInfo[cur_file_idx].cur_filename, rec_filename);
	strcpy(name, rec_filename);
	
	logd_lc("emergency[%d] file ->%s",cur_file_idx, name);

	return 0;
}

int RecordInitEmergencyDir(void)
{
	DIR *dir_ptr;
	struct dirent *direntp;
	unsigned int i;
	int idx;
	char tmpbuf[256];
	char rmbuf[256];

	char real_path[256];
	char camera_path[256];
	char thumb_path[256];
	int lastFile = 0;
	char *dirname = g_VideoDir;

	file_status_t *p = g_emergency_video;

	unsigned long long totMB = 0;
	int rmret = 0;
	strcpy(p->vehicle_license, RecordGetLicense());
	
	loge_lc("----RecordInitEmergencyDir(%d)----");

	if (dirname == NULL) {
		loge_lc("dirname == null");
		return false;
	}

	int len = strlen(dirname);

	if (len < 2) {
		loge_lc("video dir can toshort ");
		return false;
	}

	strcpy(tmpbuf, dirname);
	for (i = 0; i < len; i++) {
		if (tmpbuf[len - 1 - i] == '/') {
			tmpbuf[len - 1 - i] = 0;
		}
		else {
			break;
		}
	}

	rmret = readlink(tmpbuf, real_path, sizeof(real_path));
	if (rmret < 0) {
		logd_lc("mount path not a symlink %s err=%d", tmpbuf, errno);
		strcpy(real_path, tmpbuf);
	}
	else {
		logd_lc("mount real path is %s \r\n", real_path);
	}
	
	memset(p->cur_filedir, 0, CM_MAX_FILE_LEN);
	strcpy(p->cur_filedir, EMERGENCY_DIR);
	
	memset(camera_path, 0, sizeof(camera_path));
	sprintf(camera_path, "%s/%s", real_path, FLOCK_DIR);
	strcpy(tmpbuf, camera_path);

	if (createdir(tmpbuf) != 0) {
		loge_lc("create lock dir %s fail", tmpbuf);
		return false;
	}

	memset(camera_path, 0, sizeof(camera_path));
	sprintf(camera_path, "%s/%s/%s", real_path, EMERGENCY_DIR, CM_THUMB_DIR);
	strcpy(tmpbuf, camera_path);

	if (createdir(tmpbuf) != 0) {
		loge_lc("create emergency thumb dir %s fail", tmpbuf);
		return false;
	}
	
	memset(camera_path, 0, sizeof(camera_path));
	sprintf(camera_path, "%s/%s", real_path, EMERGENCY_DIR);
	strcpy(tmpbuf, camera_path);

	if (createdir(tmpbuf) != 0) {
		loge_lc("create emergency dir %s fail", tmpbuf);
		return false;
	}

	logd_lc("Emergency Dir -> %s", camera_path);
	if ((dir_ptr = opendir(camera_path)) == NULL){
		
	}
	else{
		unsigned long afile_size = ((EMERGENCY_DURATION * cameraInfo[0]->encode_param.bit_rate) >> 23) + 8;
		totMB = EMERGENCY_SIZE*SZ_1M; /**预设了固定大小**/
		
		logw_lc("----total size=%llu,afilesize =%lu", totMB, afile_size);
		
		p->cur_max_filenum = (EMERGENCY_SIZE) / (afile_size*MAX_SUPPORT_CAM);
		if (p->cur_max_filenum > CM_MAX_EMERGENCY_NUM) p->cur_max_filenum = CM_MAX_EMERGENCY_NUM;

		//config_cam_set_normalvideomax(cameraInfo[camId]->mCameraId, p->cur_max_filenum);
		logd_lc("dbg-rec max_filenum=%d", p->cur_max_filenum);
		while ((direntp = readdir(dir_ptr)) != NULL) {
			// logw_lc("file[%d]-->%s", p->cur_dir_file_num, direntp->d_name);
			for (i = 0; i < strlen(direntp->d_name); i++) {
				
				if (direntp->d_name[i] == '-') { /* 文件格式为： 序号-年月日_时分秒_ pSuffix[].mp4*/				
					memset(tmpbuf, 0, 256);
					memcpy(tmpbuf, direntp->d_name, i);
					idx = atoi(tmpbuf);
					if ((idx < CM_MAX_EMERGENCY_NUM) && (idx >= 0)) {
						if (strlen(p->fileInfo[idx].cur_filename) == 0) {
							strcpy(p->fileInfo[idx].cur_filename, camera_path);
							strcat(p->fileInfo[idx].cur_filename, "/");
							strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
							p->cur_dir_file_num++;
						}
						else {
							memset(rmbuf, 0, 256);
							strcat(rmbuf, direntp->d_name);
							if (strstr(p->fileInfo[idx].cur_filename, rmbuf) == NULL) {
								logw_lc("find duplicate  so rm old %s idx=%d  name=%s \n", p->fileInfo[idx].cur_filename, idx, direntp->d_name);
								rmret = remove(p->fileInfo[idx].cur_filename);
								logw_lc("rmfile ret %s", rmret == 0 ? "OK" : "FAIL");
								strcpy(p->fileInfo[idx].cur_filename, camera_path);
								strcat(p->fileInfo[idx].cur_filename, "/");
								strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
								logw_lc("newidx file %s len=%d", p->fileInfo[idx].cur_filename, strlen(p->fileInfo[idx].cur_filename));
								//remove thumb
								strcpy(tmpbuf, p->fileInfo[idx].cur_filename);
								RecordGenPicNameFromVideo(tmpbuf);										
								remove(tmpbuf); /* 删除该文件对应的缩略图*/
							}
						}
						//LOGI("parse success  idx= %d  name= %s \n",idx,direntp->d_name);
					}
				}
			}
		}

		p->cur_file_idx = RecordGetCurrentIndex(p, CM_MAX_EMERGENCY_NUM);
		closedir(dir_ptr);

		if(p->cur_dir_file_num)
		{
			lastFile = p->cur_file_idx;			
			logd_lc("lastFile[%d]->%s", p->cur_file_idx,p->fileInfo[lastFile].cur_filename);
		}
		logd_lc("emergency finish");
	}
	return true;
}
//==========================================================
int RecordAudioListFetch(void* fetchInfo)
{
	LC_RECORD_LIST_FETCH* listInfo = (LC_RECORD_LIST_FETCH*)fetchInfo;
	file_status_t *p = g_sound_record;
	int curIdx, endIdx;
	int cType, wantType, chanel;
	GBT19056_FILE_FETCH_DAT* fileList = (GBT19056_FILE_FETCH_DAT*)listInfo->fileList;	
	int totalCnt=0, realNum=0, dirPosition = 0; 
	char tempPath[128];

	if(listInfo->offset>=0) curIdx = listInfo->offset;
	else curIdx = p->cur_file_idx;
	endIdx = p->cur_file_idx;

	wantType = listInfo->fileType;
	chanel = listInfo->camChnl;

	logd_lc("endIndex = %d", endIdx);
	do{
		if(p->fileInfo[curIdx].cur_filename[0]!=0)
		{
			strcpy(tempPath, p->fileInfo[curIdx].cur_filename);			
			if(dirPosition == 0)/**获取文件夹路径**/
			{
				char* strPtr = tempPath;
				for(int ps = (CM_MAX_FILE_LEN-1); ps>0; ps--)
				{
					if(strPtr[ps]=='/')
					{
						dirPosition = ps+1;
						break;
					}
				}										
				memset(listInfo->fileDir, 0, 56);
				memcpy(listInfo->fileDir, tempPath, dirPosition);	
			}

			fileList->size = p->fileInfo[curIdx].info.size;
			memcpy(fileList->fileName, tempPath+dirPosition, 44);
			memcpy(fileList->start_time, p->fileInfo[curIdx].info.startTime, 6);
			memcpy(fileList->stop_time, p->fileInfo[curIdx].info.stopTime, 6);
			fileList->id = Record_build_file_ID(LC_NORMAL_AUDIO_RECORD, 0, curIdx);			
//			logd_lc("Soundfile:%s, size:%x, w:%d", fileList->fileName, fileList->size, cType);
//			lc_dvr_show_bcd_time("start:",fileList->start_time);
//			lc_dvr_show_bcd_time("stop:",fileList->stop_time);
			
			realNum++;
			if(realNum>=listInfo->fileNumber)
			{
				curIdx--;
				if(curIdx<0) curIdx = SOUND_REC_FILE_NUM-1;			
				break;
			}
			fileList++;

		}
		
		totalCnt++;
		if(totalCnt == SOUND_REC_FILE_NUM) 
		{
			curIdx = 0; //遍历完所有的文件
			break;
		}
		
		curIdx--;
		if(curIdx<0) curIdx = SOUND_REC_FILE_NUM-1;				
	}while(endIdx != curIdx);

	listInfo->offset = curIdx;
	listInfo->bytePerFile = sizeof(GBT19056_FILE_FETCH_DAT);
	listInfo->fileNumber = realNum;

	return realNum;
}

int RecordGetCurSoundIndex(void)
{
	file_status_t *p = g_sound_record;
	return p->cur_file_idx;
}

void RecordSoundFinsh(int size)
{
	file_status_t *p = g_sound_record;
	int cur_file_idx = p->cur_file_idx;
	time_t timer = time(NULL);

	RecordTimeToBCD(&timer, p->fileInfo[cur_file_idx].info.stopTime);
	p->fileInfo[cur_file_idx].info.size = size;
	logd_lc("RecordSoundFinsh:%s Size:%x", p->fileInfo[cur_file_idx].cur_filename, p->fileInfo[cur_file_idx].info.size);
	lc_dvr_show_bcd_time("stop:",p->fileInfo[cur_file_idx].info.stopTime);
}

int RecordGenSoundFile(char *name)
{
	char rec_filename[128];
	char real_path[128];
	char tempPath[128];
	file_status_t *p = g_sound_record;
	int cur_file_idx;
	char *idxPos1, *idxPos2;
	int ret;

	if (name == NULL)
		return -1;

	sprintf(real_path,"%s/%s", g_VideoDir, p->cur_filedir);	
	
	unsigned long long as = availSize(real_path);	
	printf("\nGenSoundfile[%d]-->%s!!\n", p->cur_file_idx, real_path);
	if(as == 0) return -1;

	p->cur_file_idx++;
	logd_lc("index->%d/%d", p->cur_file_idx, p->cur_max_filenum);
	if(p->cur_file_idx >= p->cur_max_filenum)
	{
		p->cur_file_idx = 0;
	}

	cur_file_idx = p->cur_file_idx;
	{
		struct tm *tm = NULL;
		time_t timer;
		int strP;		
		timer = getDateTime(&tm);
		RecordTimeToBCD(&timer, p->fileInfo[cur_file_idx].info.startTime);
		//GBT19056_XXXXXXXX_X_yyyymmddhhmmss
		strP =  sprintf(rec_filename, "%s/%03d-%s_%s_", real_path, cur_file_idx, STANDAR_CODE, p->vehicle_license);		
		strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																	 tm->tm_hour, tm->tm_min, tm->tm_sec, ".WAVE");
	}

	if(p->fileInfo[cur_file_idx].cur_filename[0]!='\0')
	{
		strcpy(tempPath, p->fileInfo[cur_file_idx].cur_filename);		
		if (access(tempPath, F_OK) == 0) {	/* 文件存在，则在原有的文件上创建*/
			//remove thumb
			logw_lc("dbg-rec t a file %s ", tempPath);
			ret = remove(tempPath);
			if (ret == 0) {
				logw_lc("dbg-rec remove and then create it again");
				// ret = xxCreateMyFile(rec_filename, perfilesize); //this is needed to prevent kernel crash
			}
		}
	}
	
	strcpy(p->fileInfo[cur_file_idx].cur_filename, rec_filename);
	strcpy(name, rec_filename);
	
	logd_lc("sound file ->%s", name);

	return 0;
}

int RecordInitSoundRecordDir(void)
{
	DIR *dir_ptr;
	struct dirent *direntp;
	unsigned int i;
	int idx;
	char tmpbuf[256];
	char rmbuf[256];

	char real_path[256];
	char sound_path[256];
	int lastFile = 0;
	char *dirname = g_VideoDir;

	file_status_t *p = g_sound_record;

	unsigned long long totMB = 0;
	int rmret = 0;
	strcpy(p->vehicle_license, RecordGetLicense());
	
	loge_lc("----RecordInitSoundRecDir(%d)----");

	if (dirname == NULL) {
		loge_lc("dirname == null");
		return false;
	}

	int len = strlen(dirname);

	if (len < 2) {
		loge_lc("video dir can toshort ");
		return false;
	}

	strcpy(tmpbuf, dirname);
	for (i = 0; i < len; i++) {
		if (tmpbuf[len - 1 - i] == '/') {
			tmpbuf[len - 1 - i] = 0;
		}
		else {
			break;
		}
	}

	rmret = readlink(tmpbuf, real_path, sizeof(real_path));
	if (rmret < 0) {
		logd_lc("mount path not a symlink %s err=%d", tmpbuf, errno);
		strcpy(real_path, tmpbuf);
	}
	else {
		logd_lc("mount real path is %s \r\n", real_path);
	}
	
	memset(p->cur_filedir, 0, CM_MAX_FILE_LEN);
	strcpy(p->cur_filedir, SOUNDREC_DIR);
	
	
	sprintf(sound_path, "%s/%s", real_path, SOUNDREC_DIR);
	strcpy(tmpbuf, sound_path);
	if (createdir(tmpbuf) != 0) {
		loge_lc("create thumb dir %s fail", tmpbuf);
	}

	logd_lc("SoundRecord Dir -> %s", sound_path);
	if ((dir_ptr = opendir(sound_path)) == NULL){
		
	}
	else{
		totMB = SOUND_REC_SIZE*FILE_SZ_1M; /**预设了固定大小**/		
		p->cur_max_filenum = totMB/(3*60*16*2);
		if(p->cur_max_filenum > SOUND_REC_FILE_NUM)
			p->cur_max_filenum = SOUND_REC_FILE_NUM;
		//config_cam_set_normalvideomax(cameraInfo[camId]->mCameraId, p->cur_max_filenum);
		logd_lc("dbg-rec max_filenum=%d", p->cur_max_filenum);
		while ((direntp = readdir(dir_ptr)) != NULL) {
			// logw_lc("file[%d]-->%s", p->cur_dir_file_num, direntp->d_name);
			for (i = 0; i < strlen(direntp->d_name); i++) {
				
				if (direntp->d_name[i] == '-') { /* 文件格式为： 序号-年月日_时分秒_ pSuffix[].mp4*/				
					memset(tmpbuf, 0, 256);
					memcpy(tmpbuf, direntp->d_name, i);
					idx = atoi(tmpbuf);
					if ((idx < SOUND_REC_FILE_NUM) && (idx >= 0)) {						
						if (strlen(p->fileInfo[idx].cur_filename) == 0) {
							strcpy(p->fileInfo[idx].cur_filename, sound_path);
							strcat(p->fileInfo[idx].cur_filename, "/");
							strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
							p->cur_dir_file_num++;
						}
						else {
							memset(rmbuf, 0, 256);
							strcat(rmbuf, direntp->d_name);
							if (strstr(p->fileInfo[idx].cur_filename, rmbuf) == NULL) {
								logw_lc("find duplicate  so rm old %s idx=%d  name=%s \n", p->fileInfo[idx].cur_filename, idx, direntp->d_name);
								rmret = remove(p->fileInfo[idx].cur_filename);
								logw_lc("rmfile ret %s", rmret == 0 ? "OK" : "FAIL");
								strcpy(p->fileInfo[idx].cur_filename, sound_path);
								strcat(p->fileInfo[idx].cur_filename, "/");
								strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
								logw_lc("newidx file %s len=%d", p->fileInfo[idx].cur_filename, strlen(p->fileInfo[idx].cur_filename));
							}
						}
						//LOGI("parse success  idx= %d  name= %s \n",idx,direntp->d_name);
					}
				}
			}
		}

		p->cur_file_idx = RecordGetCurrentIndex(p, SOUND_REC_FILE_NUM);
		closedir(dir_ptr);

		if(p->cur_dir_file_num)
		{
			lastFile = p->cur_file_idx;			
			logd_lc("lastFile[%d]->%s", p->cur_file_idx, p->fileInfo[lastFile].cur_filename);
		}
		logd_lc("sound record finish");
	}
	return true;
}
//=====================================================
void RecordImageSetSize(char* name, int size)
{
	file_status_t *p = g_image_record;
	int curIdx= p->cur_file_idx;
	for(int ps = (CM_MAX_FILE_LEN-1); ps>0; ps--)
	{
		if(name[ps]=='/')
		{
			curIdx = atoi(name+ps+1);
			logd_lc("image idx is %d", curIdx);
			break;
		}
	}		
	p->fileInfo[curIdx].info.size = size;
}

int RecordImageListFetch(void* fetchInfo)
{
	LC_RECORD_LIST_FETCH* listInfo = (LC_RECORD_LIST_FETCH*)fetchInfo;
	file_status_t *p = g_image_record;
	int curIdx=0, endIdx;
	int cType, wantType, chanel;
	GBT19056_FILE_FETCH_DAT* fileList = (GBT19056_FILE_FETCH_DAT*)listInfo->fileList;	
	int totalCnt=0, realNum=0, dirPosition = 0; 
	char tempPath[128];
	int flagPos = 0;

	if(listInfo->offset>=0) curIdx = listInfo->offset;
	else curIdx = p->cur_file_idx;
	endIdx = p->cur_file_idx;

	wantType = listInfo->fileType;
	chanel = listInfo->camChnl;

	logd_lc("endIndex = %d", endIdx);
	do{
		if(p->fileInfo[curIdx].cur_filename[0]!=0)
		{
			strcpy(tempPath, p->fileInfo[curIdx].cur_filename);
			if(dirPosition == 0)/**获取文件夹路径**/
			{
				char* strPtr = tempPath;
				for(int ps = (CM_MAX_FILE_LEN-1); ps>0; ps--)
				{
					if(strPtr[ps]=='/')
					{
						dirPosition = ps+1;
						break;
					}

					if((strPtr[ps]=='_')&&(flagPos==0))
					{
						flagPos = ps-1;
					}
				}										
				memset(listInfo->fileDir, 0, 56);
				memcpy(listInfo->fileDir, tempPath, dirPosition);	
			}
				
			if(tempPath[flagPos]==g_CamToChar[chanel])
			{
				fileList->size = p->fileInfo[curIdx].info.size;
				memcpy(fileList->fileName, tempPath+dirPosition, 44);
				memcpy(fileList->start_time, p->fileInfo[curIdx].info.startTime, 6);
				memcpy(fileList->stop_time, p->fileInfo[curIdx].info.stopTime, 6);
				fileList->id = Record_build_file_ID(LC_LOOP_IMAGE_RECORD, 0, curIdx);				
//				logd_lc("ImageF:%s, size:%x", fileList->fileName, fileList->size);
//				lc_dvr_show_bcd_time("start:",fileList->start_time);
//				lc_dvr_show_bcd_time("stop:",fileList->stop_time);
				
				realNum++;
				if(realNum>=listInfo->fileNumber)
				{
					curIdx--;
					if(curIdx<0) curIdx = IMAGE_REC_FILE_NUM-1;			
					break;
				}
				fileList++;
			}
		}
		
		totalCnt++;
		if(totalCnt == IMAGE_REC_FILE_NUM) 
		{
			curIdx = 0; //遍历完所有的文件
			break;
		}
		
		curIdx--;
		if(curIdx<0) curIdx = IMAGE_REC_FILE_NUM-1;				
	}while(endIdx != curIdx);

	listInfo->offset = curIdx;
	listInfo->bytePerFile = sizeof(GBT19056_FILE_FETCH_DAT);
	listInfo->fileNumber = realNum;

	return realNum;
}

int RecordGenImageFile(char *name, int chanel)
{
	char rec_filename[128];
	char real_path[128];
	char tempPath[128];
	file_status_t *p = g_image_record;
	int cur_file_idx;
	int ret;

	if (name == NULL)
		return -1;

	sprintf(real_path,"%s/%s", g_VideoDir, p->cur_filedir);	
	
	unsigned long long as = availSize(real_path);
	printf("\nGenImagefile[%d]-->%s!!\n", p->cur_file_idx, real_path);
	if(as == 0) return -1;

	p->cur_file_idx++;
	logd_lc("index->%d/%d", p->cur_file_idx, p->cur_max_filenum);
	if(p->cur_file_idx >= p->cur_max_filenum)
	{
		p->cur_file_idx = 0;
	}

	cur_file_idx = p->cur_file_idx;
	{
		struct tm *tm = NULL;
		time_t timer;
		int strP;		
		timer = getDateTime(&tm);		
		RecordTimeToBCD(&timer, p->fileInfo[cur_file_idx].info.startTime);
		memcpy(p->fileInfo[cur_file_idx].info.stopTime, p->fileInfo[cur_file_idx].info.startTime, 6);
		strP =  sprintf(rec_filename, "%s/%03d-%s_%s_%c_", real_path, cur_file_idx, STANDAR_CODE, p->vehicle_license, g_CamToChar[chanel]);		
		strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																	 tm->tm_hour, tm->tm_min, tm->tm_sec, ".JPG");
	}

	if(p->fileInfo[cur_file_idx].cur_filename[0]!='\0')
	{
		strcpy(tempPath, p->fileInfo[cur_file_idx].cur_filename);		
		if (access(tempPath, F_OK) == 0) {	/* 文件存在，则在原有的文件上创建*/
			//remove thumb
			logw_lc("dbg-rec t a file %s ", tempPath);
			ret = remove(tempPath);
			if (ret == 0) {
				logw_lc("dbg-rec remove and then create it again");
			}
		}
	}
	
	strcpy(p->fileInfo[cur_file_idx].cur_filename, rec_filename);
	strcpy(name, rec_filename);
	
	logd_lc("image file ->%s", name);

	return 0;
}

int RecordInitImageRecordDir(void)
{
	DIR *dir_ptr;
	struct dirent *direntp;
	unsigned int i;
	int idx;
	char tmpbuf[256];
	char rmbuf[256];

	char real_path[256];
	char sound_path[256];
	int lastFile = 0;
	char *dirname = g_VideoDir;

	file_status_t *p = g_image_record;

	unsigned long long totMB = 0;
	int rmret = 0;
	strcpy(p->vehicle_license, RecordGetLicense());
	
	loge_lc("----RecordInitSoundRecDir(%d)----");

	if (dirname == NULL) {
		loge_lc("dirname == null");
		return false;
	}

	int len = strlen(dirname);

	if (len < 2) {
		loge_lc("video dir can toshort ");
		return false;
	}

	strcpy(tmpbuf, dirname);
	for (i = 0; i < len; i++) {
		if (tmpbuf[len - 1 - i] == '/') {
			tmpbuf[len - 1 - i] = 0;
		}
		else {
			break;
		}
	}

	rmret = readlink(tmpbuf, real_path, sizeof(real_path));
	if (rmret < 0) {
		logd_lc("mount path not a symlink %s err=%d", tmpbuf, errno);
		strcpy(real_path, tmpbuf);
	}
	else {
		logd_lc("mount real path is %s \r\n", real_path);
	}
	
	memset(p->cur_filedir, 0, CM_MAX_FILE_LEN);
	strcpy(p->cur_filedir, IMAGEREC_DIR);
	
	
	sprintf(sound_path, "%s/%s", real_path, IMAGEREC_DIR);
	strcpy(tmpbuf, sound_path);
	if (createdir(tmpbuf) != 0) {
		loge_lc("create thumb dir %s fail", tmpbuf);
	}

	logd_lc("IMAGEREC_DIR Dir -> %s", sound_path);
	if ((dir_ptr = opendir(sound_path)) == NULL){
		
	}
	else{
		totMB = IMAGE_REC_SIZE*FILE_SZ_1M; /**预设了固定大小**/		
		p->cur_max_filenum = IMAGE_REC_FILE_NUM;
		//config_cam_set_normalvideomax(cameraInfo[camId]->mCameraId, p->cur_max_filenum);
		logd_lc("dbg-rec max_filenum=%d", p->cur_max_filenum);
		while ((direntp = readdir(dir_ptr)) != NULL) {
			//logw_lc("file[%d]-->%s", p->cur_dir_file_num, direntp->d_name);
			for (i = 0; i < strlen(direntp->d_name); i++) {
				
				if (direntp->d_name[i] == '-') { /* 文件格式为： 序号-年月日_时分秒_ pSuffix[].mp4*/				
					memset(tmpbuf, 0, 256);
					memcpy(tmpbuf, direntp->d_name, i);
					idx = atoi(tmpbuf);
					if ((idx < IMAGE_REC_FILE_NUM) && (idx >= 0)) {						
						if (strlen(p->fileInfo[idx].cur_filename) == 0) {
							strcpy(p->fileInfo[idx].cur_filename, sound_path);
							strcat(p->fileInfo[idx].cur_filename, "/");
							strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
							p->cur_dir_file_num++;
						}
						else {
							memset(rmbuf, 0, 256);
							strcat(rmbuf, direntp->d_name);
							if (strstr(p->fileInfo[idx].cur_filename, rmbuf) == NULL) {
								logw_lc("find duplicate  so rm old %s idx=%d  name=%s \n", p->fileInfo[idx].cur_filename, idx, direntp->d_name);
								rmret = remove(p->fileInfo[idx].cur_filename);
								logw_lc("rmfile ret %s", rmret == 0 ? "OK" : "FAIL");
								strcpy(p->fileInfo[idx].cur_filename, sound_path);
								strcat(p->fileInfo[idx].cur_filename, "/");
								strcat(p->fileInfo[idx].cur_filename, direntp->d_name);
								logw_lc("newidx file %s len=%d", p->fileInfo[idx].cur_filename, strlen(p->fileInfo[idx].cur_filename));
							}
						}
						//LOGI("parse success  idx= %d  name= %s \n",idx,direntp->d_name);
					}
				}
			}
		}

		p->cur_file_idx = RecordGetCurrentIndex(p, IMAGE_REC_FILE_NUM);
		closedir(dir_ptr);

		if(p->cur_dir_file_num)
		{
			lastFile = p->cur_file_idx;			
			logd_lc("lastFile[%d]->%s", p->cur_file_idx, p->fileInfo[lastFile].cur_filename);
		}
		logd_lc("image record finish");
	}
	return true;
}
//=====================================================

int RecordSetVideoPath(char* path)
{
	logd_lc("RecordSetVideoPath-->%s", path);
	if(strcmp(g_VideoDir, path)==0) 
	{
		logd_lc("path already initial!");
		return 0;
	}
		
	memset(g_VideoDir, 0, PIC_NAME_LEN);
	strcpy(g_VideoDir, path);
	
	memset(g_LicenseSet, 0, PIC_NAME_LEN);
	strcpy(g_LicenseSet, path+64);
	//strcpy(g_LicenseSet, "YB046837");

	logd_lc("VideoDir:%s", g_VideoDir);
	logd_lc("LicenseSet:%s", g_LicenseSet);

	return 1;
}

int Record_video_file_copy(int camId, int index, char* dir, char* fileName)
{
	char srcPath[128];
	unsigned long long as;
	int retval = 0;
	FILE* srcFp;
	file_status_t *p = &(cameraInfo[camId]->FileManger);
	int curIdx = index;
	int len, remain;

	sprintf(srcPath, "%s", p->fileInfo[curIdx].cur_filename);
	srcFp =fopen(srcPath, "rb");
	if(srcFp == NULL) return -1;
	
	fseek(srcFp, 0L, SEEK_END);
	len =ftell(srcFp);
	fseek(srcFp, 0L, SEEK_SET);

	as = availSize(dir)*1024*1024;
	// logd_lc("file(%s) len:%d as:%llu", srcPath, len, as);
	if(as > len)
	{	
		char *memReadBuf = (char*)malloc(512*1024);
		char savePath[128];	
		int byteNum;
		
		sprintf(savePath, "%s/%s", dir, fileName);
		// logd_lc("SrcFile:%s", srcPath);
		logd_lc("DstFile(%d):%s", len, savePath);
		
		FILE* dstFp = fopen(savePath, "wb+");
		for(remain=len; remain>0;)
		{
			int cpLen = remain>512*1024?512*1024:remain;
			usleep(1000);
			byteNum = fread(memReadBuf, 1, cpLen, srcFp);
			if(byteNum!=cpLen){ 
				logd_lc("Source copy error!");
				retval = -1;
				break;
			}
			
			byteNum = fwrite(memReadBuf, 1, cpLen, dstFp);			
			if(byteNum!=cpLen){ 
				logd_lc("Dest write error!");
				retval = -2;
				break;
			}
			
			if(vc_video_file_copy_status() == 0)
			{
				retval = -3;
				break;
			}			
			remain -= cpLen;
		}
		fclose(dstFp);
		free(memReadBuf);
		logd_lc("sd copy finish!");
	}
	
	fclose(srcFp);
	return retval;
}

char* Record_get_info_from_file_name(char *name, int *index, int *type, int *camId)
{
	int i, len = strlen(name);
	char *typePtr, *cIdPtr = NULL;
	
	*index = -1;
	*type = -1;
	*camId = -1;
	cIdPtr = name;
	
	for(i=(len-1); i>=0; i--)
	{	
		if(name[i]=='/') 
		{			
			cIdPtr = name+i+1;
			break;
		}

		if(name[i]=='_')
		{			
			if(camId[0]<0)
			{
				for(int l=0; l<MAX_SUPPORT_CAM; l++)
				{
					if(name[i-1] == g_CamToChar[l]) *camId = l;
				}
			}
		}
		
		if(name[i]=='.') 
		{
			typePtr = name;
			if(strstr(typePtr, ".JPG"))
			{
				*type = LC_LOOP_IMAGE_RECORD;
			}
			else if(strstr(typePtr, ".TS"))
			{
				*type = LC_NORMAL_EMERGNECY_EVENT;
			}
			else if(strstr(typePtr, ".WAVE"))
			{
				*type = LC_NORMAL_AUDIO_RECORD;
			}
			else if(strstr(typePtr, EXT_VIDEO))
			{
				*type = LC_NORMAL_VIDEO_RECORD;
			}			
		}
	}
	
	*index = atoi(cIdPtr);
	
	return cIdPtr;
}

void Record_File_unLock(char* name)
{
	char* fName;
	int len; 
	int i;

	if(name == NULL) return;
	
	len = strlen(name);
	if(len>0)
	{
		fName = name;
		for(i=len-1; i>=0; i--)
		{
			if(name[i]=='/') 
			{
				fName = name+i+1;
				break;
			}
		}

		for(i=0; i<TEMP_FILE_LOCK_NUM; i++)
		{		
			if(strstr(g_file_temp_lock[i].file_temp_lock.cur_filename, fName)!= NULL) 
			{
				char sysCmd[256], nameVideo[128];
				recordInfoExt *cLockFrom = g_file_temp_lock[i].lockFrom;
				
				if(cLockFrom!=NULL)
				{
					int len = sprintf(nameVideo, "%s/%s/%s",g_VideoDir, FLOCK_DIR, fName);
					if(cLockFrom->cur_filename[0]!=0)
					{
						sprintf(sysCmd, "rm %s",nameVideo); /**如果锁定文件的位置已经有了新文件，则直接把已经锁定的文件删除**/
						system(sysCmd);
						printf("cmd_rm1->%s\n", sysCmd);
						
						for(; len>=0; len--) if(nameVideo[len]=='.') break;
						sprintf(nameVideo+len,".JPG");
						if(access(nameVideo, F_OK) == 0)
						{
							sprintf(sysCmd, "rm %s",nameVideo);	
							system(sysCmd);
							printf("cmd_rm2->%s\n", sysCmd);
						}
					}
					else
					{
						sprintf(sysCmd, "mv %s %s",nameVideo, g_file_temp_lock[i].file_temp_lock.cur_filename);
						system(sysCmd);
						printf("cmd_ulk1->%s\n", sysCmd); /**如果锁定文件的位置还没有填充新文件，则把该文件归还到原来位置**/
						
						for(; len>=0; len--) if(nameVideo[len]=='.') break;
						sprintf(nameVideo+len,".JPG");
						strcpy(cLockFrom->cur_filename, g_file_temp_lock[i].file_temp_lock.cur_filename);
						
						RecordGenPicNameFromVideo(g_file_temp_lock[i].file_temp_lock.cur_filename);
						if(access(nameVideo, F_OK) == 0)
						{
							sprintf(sysCmd, "mv %s %s",nameVideo, g_file_temp_lock[i].file_temp_lock.cur_filename);	
							system(sysCmd);
							printf("cmd_ulk2->%s\n", sysCmd);
						}
					}
				}
				break;
			}
		}
		
		printf("replace: %d\n", i);
		for(;i<TEMP_FILE_LOCK_NUM-1; i++)
		{
			memcpy(&g_file_temp_lock[i], &g_file_temp_lock[i+1], sizeof(FILE_LOCK_INFO));
		}

		if(i<TEMP_FILE_LOCK_NUM)
		{
			memset(&g_file_temp_lock[i], 0, sizeof(FILE_LOCK_INFO));
		}
	}
}

void Record_get_video_relateto_file(char* fileName)
{
	DIR *dir_ptr;
	struct dirent *direntp;	
	char tempFeat[128], serchFeat[64];
	int i;

	logd_lc("duvonn cur file->%s", fileName);
	if ((dir_ptr = opendir(FUNTRUE_DEVICE)) == NULL)
	{

	}
	else{
		strcpy(tempFeat, fileName);
		for(i=strlen(tempFeat)-1; i>=0; i--)
		{
			if(tempFeat[i]=='_')
			{
				tempFeat[i+1] = 0;
				break;
			}
		}

		for(; i>=0; i--)
		{
			if(tempFeat[i]=='/')
			{
				i++;
				strcpy(serchFeat, tempFeat+i);
				break;
			}
		}
		
		while ((direntp = readdir(dir_ptr)) != NULL) {
			if(strstr(direntp->d_name, serchFeat)!=NULL)
			{
				sprintf(fileName, "%s/%s", FUNTRUE_DEVICE, direntp->d_name);
				logd_lc("relateto -> %s", fileName);
				break;
			}
		}
	}
}

int Record_File_Lock(char *name, char* lockPath)
{
	int fileType, camId, listIdx, len, l;	
	file_status_t *p;
	FILE_LOCK_INFO *infoStore = NULL;
	char sysCmd[256], nameTemp[128], fileTemp[128];
	char *namePtr =	Record_get_info_from_file_name(name, &listIdx, &fileType, &camId);

	logd_lc("(%s) Idx:%d, Type:%d, camId:%d",namePtr, listIdx, fileType, camId);
	if(namePtr == NULL) return -1;
	
	if((fileType<0)||(camId<0)||(listIdx<0))
	{
		return -2;
	}

	for(int i=0; i<TEMP_FILE_LOCK_NUM; i++)
	{
		if(g_file_temp_lock[i].lockFrom == NULL) 
		{
			infoStore = &g_file_temp_lock[i];
			logd_lc("lock save in %d", i);
			break;
		}
	}

	if(infoStore!=NULL)
	{
		switch(fileType)
		{
			case LC_LOOP_IMAGE_RECORD:
				{
					p = g_image_record;	
					memcpy(&infoStore->file_temp_lock , &p->fileInfo[listIdx], sizeof(recordInfoExt));

					sprintf(fileTemp, "%s/%s/%s", g_VideoDir, FLOCK_DIR, namePtr);
					sprintf(sysCmd, "mv %s %s", infoStore->file_temp_lock.cur_filename, fileTemp);					
					system(sysCmd);
					printf("cmd_lk1->%s\n", sysCmd);
					memset(p->fileInfo[listIdx].cur_filename, 0, CM_MAX_FILE_LEN);
					infoStore->lockFrom = &p->fileInfo[listIdx];					
				}
				break;
			case LC_NORMAL_AUDIO_RECORD:
				{
					p = g_sound_record;
					memcpy(&infoStore->file_temp_lock , &p->fileInfo[listIdx], sizeof(recordInfoExt));

					sprintf(fileTemp, "%s/%s/%s", g_VideoDir, FLOCK_DIR, namePtr);
					sprintf(sysCmd, "mv %s %s", infoStore->file_temp_lock.cur_filename, fileTemp);					
					system(sysCmd);
					printf("cmd_lk1->%s\n", sysCmd);
					memset(p->fileInfo[listIdx].cur_filename, 0, CM_MAX_FILE_LEN);
					infoStore->lockFrom = &p->fileInfo[listIdx];					
				}
				break;			
			case LC_NORMAL_EMERGNECY_EVENT:
				{
					p = g_emergency_video;
					memcpy(&infoStore->file_temp_lock , &p->fileInfo[listIdx], sizeof(recordInfoExt));

					sprintf(fileTemp, "%s/%s/%s", g_VideoDir, FLOCK_DIR, namePtr);
					sprintf(sysCmd, "mv %s %s", infoStore->file_temp_lock.cur_filename, fileTemp);					
					system(sysCmd);
					printf("cmd_lk1->%s\n", sysCmd);
					
					strcpy(nameTemp, infoStore->file_temp_lock.cur_filename);
					RecordGenPicNameFromVideo(nameTemp);
					
					len = sprintf(sysCmd, "mv %s %s", nameTemp, fileTemp);
				
					for(l = len-1; l>0; l--)
					{
						if(sysCmd[l]=='.')
						{
							sprintf(sysCmd+l,".JPG");
							break;
						}
					}

					system(sysCmd);
					printf("cmd_lk2->%s\n", sysCmd);
					memset(p->fileInfo[listIdx].cur_filename, 0, CM_MAX_FILE_LEN);
					infoStore->lockFrom = &p->fileInfo[listIdx];
				}
				break;

			case LC_NORMAL_VIDEO_RECORD:
				{
					p = &cameraInfo[camId]->FileManger;
					memcpy(&infoStore->file_temp_lock , &p->fileInfo[listIdx], sizeof(recordInfoExt));

					sprintf(fileTemp, "%s/%s/%s", g_VideoDir, FLOCK_DIR, namePtr);
					sprintf(sysCmd, "mv %s %s", infoStore->file_temp_lock.cur_filename, fileTemp);					
					system(sysCmd);
					printf("cmd_lk1->%s\n", sysCmd);
					
					strcpy(nameTemp, infoStore->file_temp_lock.cur_filename);
					RecordGenPicNameFromVideo(nameTemp);
					
					len = sprintf(sysCmd, "mv %s %s", nameTemp, fileTemp);				
					for(l = len-1; l>0; l--)
					{
						if(sysCmd[l]=='.')
						{
							sprintf(sysCmd+l,".JPG");
							break;
						}
					}

					system(sysCmd);
					printf("cmd_lk2->%s\n", sysCmd);
					memset(p->fileInfo[listIdx].cur_filename, 0, CM_MAX_FILE_LEN);
					infoStore->lockFrom = &p->fileInfo[listIdx];

				}
				break;			
		}
	}
	strcpy(lockPath, fileTemp);
	return 0;
}

int Record_Fetch_Path_By_ID(int fID, void* fileDetial)
{
	int fileType, camId, listIdx, len, l;	
	file_status_t *p = NULL;
	GBT19056_FILE_DETAIL_INFO *fileInfo = (GBT19056_FILE_DETAIL_INFO*)fileDetial;
	int retval = -1;
	
	fileType = (fID>>24)&0xff;
	camId = (fID>>16)&0xff;
	listIdx = fID&0xffff;
	logd_lc("type:%d, camId:%d, listIdx:%d", fileType, camId, listIdx);
	
	switch(fileType)
	{
		case LC_LOOP_IMAGE_RECORD:
			{
				p = g_image_record; 
			}
			break;
		case LC_NORMAL_AUDIO_RECORD:
			{
				p = g_sound_record;
			}
			break;			
		case LC_NORMAL_EMERGNECY_EVENT:
		case LC_SPECIAL_EMERGNECY_EVENT:
			{
				p = g_emergency_video;
			}			
			break;
			
		case LC_NORMAL_VIDEO_RECORD:
			{
				p = &cameraInfo[camId]->FileManger;	
				
			}
			break;			
	}

	if(p!=NULL)
	{
		fileInfo->size = p->fileInfo[listIdx].info.size&0x3fffffff;
		strcpy(fileInfo->fileName, p->fileInfo[listIdx].cur_filename);
		memcpy(fileInfo->start_time, p->fileInfo[listIdx].info.startTime, 6);
		memcpy(fileInfo->stop_time, p->fileInfo[listIdx].info.stopTime, 6);
		memcpy(fileInfo->envInfo, p->fileInfo[listIdx].info.information, 28);
		if((fileType == LC_NORMAL_VIDEO_RECORD)&&is_duvonn_device())
		{			
			switch(camId)
			{
				case LC_DUVONN_CHANEL_0:
				case LC_DUVONN_CHANEL_1:
					{
						Record_get_video_relateto_file(fileInfo->fileName);
					}					
					break;
				default:
					break;
			}
		}		
		retval = 0;
	}

	return retval;
}

