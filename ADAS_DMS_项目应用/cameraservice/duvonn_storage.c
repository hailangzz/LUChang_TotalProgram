#include <time.h>
#include <wchar.h>
#include <iconv.h>
#include <pthread.h>
#include "lc_glob_type.h"
#include "duvonn_storage.h"
#define LC_LOG_LEVEL LC_LOG_LEVEL_ALL
#include "lc_dvr_record_log.h"

#define LC_CACHE_PAGE 		   EMMC_PAGE
#define LC_ALIGN_PAGE(len) ((len+LC_CACHE_PAGE-1)&(~(LC_CACHE_PAGE-1)))

#define LC_PAGE_OFFSET		 	5 		/**根据文件名的长度凑出来的页数，从而保证一次能读取出完整的文件名**/
#define LC_WRITE_CACHE_SIZE     (16*1024)
#define EXT_PIC_JPG 			".JPG"
#define EXT_PIC_BMP 			".BMP"
#define EXT_PIC 				EXT_PIC_JPG
#define EXT_VIDEO 				EXT_VIDEO_MP4
#define LC_DUVONN_TEST_LEN  	(32*1024)

typedef struct {
	DUVONN_RECORD_LIST fileCtrl;
	char recordCache[LC_CACHE_PAGE];
}GBT19056_INFO_FILE;

typedef struct {
	GBT19056_INFO_FILE baseConfig;				/**配置信息文件**/
	GBT19056_INFO_FILE travel_record_file;		/**行车数据记录**/
	GBT19056_INFO_FILE doubtful_record_file;	/**疑点数据记录**/
	GBT19056_INFO_FILE overTime_record_file;	/**超时驾驶记录**/
	GBT19056_INFO_FILE driver_record_file;		/**驾驶员信息记录**/
	GBT19056_INFO_FILE daily_record_file;		/**日志信息记录**/	
	GBT19056_INFO_FILE faceFeat_file;			/**驾驶员脸部图片**/
	GBT19056_INFO_FILE video_record_file;
}DUVONN_GBT19056_INFO_FILE;

typedef struct {
    int CameraId;
    int Duration;
    int storage_state;
    int maxFileNum;
	int currentIdx;
	DUVONN_RECORD_LIST *video_record_file;	/**视频文件记录**/
}DUVONN_GBT19056_VIDEO_FILE;

static char g_CamToChar[4] = {CMA_DMS, CMA_ADAS, CMA_BRAKE, CMA_REAR};

static DUVONN_RECORD_LIST *mListItem=NULL;
static DUVONN_GBT19056_INFO_FILE  *mDuvonnInfoRecord;
static DUVONN_GBT19056_VIDEO_FILE mDuvonnVideoRecord[LC_DUVONN_CAMERA_NUMBER];
static char mVehicle_license[32]={0};

int mDebugCnt = 0;
//Defined Directory Entries /**exFAT文件名规则**/
//EntryType  Primary  Critical  Code  Directory Entry
//===============================================================
//0x81          ?        ?       1    Allocation Bitmap
//0x82          ?        ?       2    Up-case Table
//0x83          ?        ?       3    Volume Label
//0x85          ?                5    File
//0xA0          ?                0    Volume GUID
//0xA1          ?                1    TexFAT Padding
//0xA2          ?                2    Windows CE Access Control Table
//0xC0                   ?       0    Stream Extension
//0xC1                   ?       1    File Name

/*代码转换:从一种编码转为另一种编码*/

#define LC_UNICODE_EXCHANGE_LEN  48

static uint16_t lc_unicode_tab[LC_UNICODE_EXCHANGE_LEN*2] ={
	0x6D4B,0x8BD5,0x4EAC,0x6D25,0x6CAA,0x6E1D,0x5B81,0x65B0,
	0x85CF,0x6842,0x8499,0x8FBD,0x5409,0x9ED1,0x5180,0x664B,
	0x82CF,0x6D59,0x7696,0x95FD,0x8D63,0x9C81,0x8C6B,0x9102,
	0x6E58,0x7CA4,0x743C,0x5DDD,0x8D35,0x4E91,0x9655,0x7518,
	0x9752,
};

static char lc_gbk_tab[LC_UNICODE_EXCHANGE_LEN*2]={
	"测试京津沪渝宁新藏桂蒙辽吉黑冀晋苏浙皖闽赣鲁豫鄂湘粤琼川贵云陕甘青客车货挂出租"
};

static char lc_url_tab[LC_UNICODE_EXCHANGE_LEN*3]={
	0xE6,0xB5,0x8B,0xE8,0xAF,0x95,0xE4,0xBA,0xAC,0xE6,0xB4,0xA5,
	0xE6,0xB2,0xAA,0xE6,0xB8,0x9D,0xE5,0xAE,0x81,0xE6,0x96,0xB0,
	0xE8,0x97,0x8F,0xE6,0xA1,0x82,0xE8,0x92,0x99,0xE8,0xBE,0xBD,
	0xE5,0x90,0x89,0xE9,0xBB,0x91,0xE5,0x86,0x80,0xE6,0x99,0x8B,
	0xE8,0x8B,0x8F,0xE6,0xB5,0x99,0xE7,0x9A,0x96,0xE9,0x97,0xBD,
	0xE8,0xB5,0xA3,0xE9,0xB2,0x81,0xE8,0xB1,0xAB,0xE9,0x84,0x82,
	0xE6,0xB9,0x98,0xE7,0xB2,0xA4,0xE7,0x90,0xBC,0xE5,0xB7,0x9D,
	0xE8,0xB4,0xB5,0xE4,0xBA,0x91,0xE9,0x99,0x95,0xE7,0x94,0x98,
	0xE9,0x9D,0x92,0xE5,0xAE,0xA2,0xE8,0xBD,0xA6,0xE8,0xB4,0xA7,
	0xE6,0x8C,0x82,0xE5,0x87,0xBA,0xE7,0xA7,0x9F,
};

static time_t getDateTime(struct tm **local_time)
{
	time_t timer;
	timer = time(NULL);
    *local_time = localtime(&timer);
	return timer;
}

/*UNICODE码转为GB2312码*/
static uint8_t *Unicode_2_GBK(void *inUnicode)
{
	uint8_t  *u8Ptr = (uint8_t*)inUnicode;
	uint16_t unicode = (uint16_t)u8Ptr[0]+(uint16_t)(u8Ptr[1]<<8);
	uint16_t *gbkTab = (uint16_t*)lc_gbk_tab;
	uint16_t *unicodeTab = (uint16_t*)lc_unicode_tab;

	for(int i=0; i<LC_UNICODE_EXCHANGE_LEN; i++)
	{
		if(unicode == unicodeTab[i])
		{
			return &gbkTab[i];
		}
	}
	return 0;
}

/*GB2312码转为UNICODE码*/
static uint16_t GBK_2_Unicode(void *inGbk)
{
	uint8_t  *u8Ptr = (uint8_t*)inGbk;
	uint16_t gbkWord = (uint16_t)u8Ptr[0]+(uint16_t)(u8Ptr[1]<<8);
	uint16_t *gbkTab = (uint16_t*)lc_gbk_tab;
	uint16_t *unicodeTab = (uint16_t*)lc_unicode_tab;
		
	for(int i=0; i<LC_UNICODE_EXCHANGE_LEN; i++)
	{
		if(gbkWord == gbkTab[i])
		{
			return unicodeTab[i];
		}
	}
	return 0;
}

/*GB2312码转为UNICODE码*/
uint8_t* URL_2_GBK(void *inUrl)
{
	uint8_t  *u8Ptr = (uint8_t*)inUrl;
	uint8_t *urlTab;
	uint8_t *GBKTab = (uint8_t*)lc_gbk_tab;
		
	for(int i=0; i<LC_UNICODE_EXCHANGE_LEN; i++)
	{
		urlTab =lc_url_tab + i*3;
		if((u8Ptr[0] == urlTab[0])&&(u8Ptr[1] == urlTab[1])&&(u8Ptr[2] == urlTab[2]))
		{
			// logd_lc("Found url code %d !!", i);
			return GBKTab+i*2;
		}
	}
	return NULL;
}

uint8_t* GBK_2_URL(void *inGbk)
{
	uint8_t  *u8Ptr = (uint8_t*)inGbk;
	uint16_t gbkWord = (uint16_t)u8Ptr[0]+(uint16_t)(u8Ptr[1]<<8);
	uint16_t *gbkTab = (uint16_t*)lc_gbk_tab;
	uint16_t *unicodeTab = (uint16_t*)lc_unicode_tab;
		
	for(int i=0; i<LC_UNICODE_EXCHANGE_LEN; i++)
	{
		if(gbkWord == gbkTab[i])
		{
			return lc_url_tab + i*3;
		}
	}
	return NULL;
}


// fileName points to up-cased file name  
uint16_t duvonn_exFAT_NameHashCal(uint16_t hashIn, void *fileName, int nameLength)  
{  
	uint16_t hash = hashIn;  
	unsigned char *data = ( unsigned char*) fileName;   
	for ( int i = 0; i<nameLength; i++)  
	{
		hash = (hash << 15) | (hash >> 1) + data[i];  
	}
	return hash;
}

static uint16_t exFAT_GetNameHashVal(void *fileName)  
{  
	uint16_t hashVal = 0;  
	uint8_t *u8Ptr = (uint8_t*)fileName;
	
	hashVal = u8Ptr[37];
	hashVal <<= 8;
	hashVal += u8Ptr[36];
	return hashVal;
}

static void exFAT_SetNameHashVal(void *fileName, uint16_t hashVal)  
{  	
	uint8_t *u8Ptr = (uint8_t*)fileName;
	u8Ptr[36] = hashVal&0xff;
	u8Ptr[37] = (hashVal>>8)&0xff;
}

static void exFAT_SetBlockHashVal(void *fileName, uint16_t hashVal)  
{  	
	uint8_t *u8Ptr = (uint8_t*)fileName;
	u8Ptr[2] = hashVal&0xff;
	u8Ptr[3] = (hashVal>>8)&0xff;
}


static int exFAT_GetNameLen(void *fileName)
{  
	uint8_t *fileLen = (uint8_t*)fileName;
	return fileLen[35];
}

static void exFAT_SetNameLen(void *fileName, int fileLen)
{  
	uint8_t *u8Ptr = (uint8_t*)fileName;
	u8Ptr[35] = fileLen;
}

static void exFAT_SetStartSector(void *fileName, int start)
{
	uint32_t* u32Ptr = (uint32_t*)(fileName+0x34);
	//logd_lc("sector_start:%x", start);
	start -= 0x00100000;
	start /= 0x00080000;
	start *=256;
	start += 0x01f0;
	*u32Ptr = start;
}

static void exFAT_SetFileLen(void *fileName, int len)
{
	uint32_t* u32Ptr = (uint32_t*)(fileName+0x28);
	*u32Ptr = len;
	//logd_lc("file size:%x", len);
	u32Ptr = (uint32_t*)(fileName+0x38);
	*u32Ptr = len;
}

static void exFAT_SetFileTime(void *fileName)
{
	uint32_t* u32Ptr = (uint32_t*)(fileName+0x08);
	uint8_t* u8Ptr = (uint8_t*)fileName;
	struct tm *tm = NULL;
	uint32_t dos_time;	
	getDateTime(&tm);
	
	dos_time = (tm->tm_sec/2)&0x1f;    //5
	dos_time |= (tm->tm_min<<5)&0x07e0;  // 6
	dos_time |= (tm->tm_hour<<11)&0xf800; // 5
	dos_time |= (tm->tm_mday<<16)&0x1f0000; // 5
	dos_time |= (tm->tm_mon<<21)&0x1e00000; // 4
	dos_time |= ((tm->tm_year - 80)<<25)&0xfe000000; // 7
	
	u32Ptr[0] = dos_time;
	u32Ptr[1] = dos_time;
	u32Ptr[2] = dos_time;

	u8Ptr[0x14] = 0x25;
}


static uint16_t exFAT_ReHashVal(void *fileName)  
{  
	uint16_t hashVal = 0;  
	int len = exFAT_GetNameLen(fileName);
	uint8_t *u8Ptr = (uint8_t*)fileName;
	char unicode[256];
	int k=0;
	
	memset(unicode, 0, 256);
	u8Ptr+=64;

	for(int i=0; i<3*32; i++)
	{
		if(u8Ptr[i]==0xc1) i++;
		else unicode[k++] = u8Ptr[i];
	}
	
	//lc_dvr_mem_debug(unicode, len*2);
	hashVal = duvonn_exFAT_NameHashCal(0, unicode, len*2);
	return hashVal;
}

static void exFAT_SetNameContent(void *fileName, char* strPtr, int fileLen)
{  
	uint8_t *u8Ptr = (uint8_t*)fileName;
	char unicodeStr[256];
	int cLen = 0;

	fileLen<<=1;
	memcpy(unicodeStr, strPtr, fileLen); 
	
	for(int i=0x40; i<0xa0; i+=32)
	{
		u8Ptr[i]=0xc1;
		u8Ptr[i+1]=0;
		for(int k=2; k<32; k++) 
		{			
			if(cLen < fileLen)
			{	
				u8Ptr[i+k] = unicodeStr[cLen++];				
			}
			else
			{
				u8Ptr[i+k] = 0;
			}
		}
		
		if(cLen>=fileLen) break;
	}
}

void duvonn_set_licence_info(char *licence)
{
	strcpy(mVehicle_license, licence);
}

char* duvonn_get_licence_info(void)
{
	return mVehicle_license;
}

static int duvonn_get_one_line(char* inBuf, char* oneLine)
{
	char* s8Ptr = inBuf;
	int lineByte = 0;

	for(;(*s8Ptr!='\r')&&(*s8Ptr!='\n'); s8Ptr++) 
	{
		oneLine[lineByte++] = *s8Ptr;
		if(*s8Ptr=='\0') 
		{
			return -1;
		}	
	}

	oneLine[lineByte] = 0;
	for(; ;s8Ptr++)
	{		
		if((*s8Ptr!='\r')&&(*s8Ptr!='\n')&&(*s8Ptr!='\t')) break;
		lineByte++;
	}
	return lineByte;
}

static int duvonn_atohex(char* text)
{
	int ctmp, value = 0;

	for(int i=0; i<8 ;i++)
	{		
		if((text[i]>='0')&&(text[i]<='9')) 
			ctmp = text[i]-'0';
		else if((text[i]>='a')&&(text[i]<='f')) 
			ctmp = text[i]-'a'+0x0a;
		else if((text[i]>='A')&&(text[i]<='F')) 
			ctmp = text[i]-'A'+0x0a;
		else 
			break;	
		value <<= 4;
		value += ctmp;
	}
	return value;
}
/**listBuf 获取的文件的信息储存于该列表**/
/**feat 关键字，用于检索特定类型的文件**/
/**IND_File 输入的IND文件的内容**/
/**wantNum 想要获取的特定类型文件的个数**/

#define SECTOR_FEAT_SSEC 	"StartAddr:"
#define SECTOR_FEAT_ESEC 	"EndAddr:"
#define SECTOR_FEAT_FNAME	"FileName:"
#define SECTOR_FREAT_VIDEO	".TMP"

//#define SECTOR_FEAT_SSEC 	"SSec:"
//#define SECTOR_FEAT_ESEC 	"ESec:"
//#define SECTOR_FEAT_FNAME	"FName:"
//#define SECTOR_FREAT_VIDEO	".DAT"

static int duvonn_get_file_info(void* listBuf, char* feat, char *IND_File, int wantNum)
{
	DUVONN_RECORD_LIST *cListBuf = (DUVONN_RECORD_LIST*)listBuf;
	char oneLine[512];
	char *s8Ptr;
	int skip = 0, listCnt=0;
	int offset = 0;
	// logd_lc("feat is:%s", feat);
	for(;;)
	{
		skip = duvonn_get_one_line(IND_File+offset, oneLine);
		if(strstr(oneLine, "[End]")!=NULL) break;
		s8Ptr = strstr(oneLine, feat);
		if(s8Ptr!=NULL) 
		{
			// logd_lc("Find -> %s", oneLine);
			s8Ptr = strstr(oneLine, SECTOR_FEAT_SSEC);
			if(s8Ptr!=NULL)
			{
				s8Ptr+=strlen(SECTOR_FEAT_SSEC)+2;
				cListBuf[listCnt].fileInfo.sector_start = duvonn_atohex(s8Ptr);
			}
			
			s8Ptr = strstr(oneLine, SECTOR_FEAT_ESEC);
			if(s8Ptr!=NULL)
			{
				s8Ptr+=strlen(SECTOR_FEAT_ESEC)+2;
				cListBuf[listCnt].fileInfo.fileSize = duvonn_atohex(s8Ptr)-cListBuf[listCnt].fileInfo.sector_start+1;
				cListBuf[listCnt].fileInfo.fileSize *= 512;
			}

			s8Ptr = strstr(oneLine, SECTOR_FEAT_FNAME);
			if(s8Ptr!=NULL)
			{
				s8Ptr+=strlen(SECTOR_FEAT_FNAME)+2;
				cListBuf[listCnt].fileInfo.fileNameSector = duvonn_atohex(s8Ptr);
				for(;*s8Ptr!='.';s8Ptr++);
				s8Ptr+=3;
				cListBuf[listCnt].fileInfo.fileNameOffset = duvonn_atohex(s8Ptr);
			}

			cListBuf[listCnt].fileInfo.index = listCnt;
			listCnt++;			
			if(wantNum>0)
			{
				wantNum--;
				if(wantNum==0) break;
			}
		}
		if(skip>0) 
			offset+=skip;
		else 
			break;
	}

	return listCnt;
}

static void duvonn_unicoed_to_ascii(char* nameBuf, DUVONN_RECORD_LIST* videoInfo)
{
	char *license = duvonn_get_licence_info();
	int len = exFAT_GetNameLen(nameBuf);
	int cnt = 0;
	char unicodeName[4];
	char GBKName[4];
	uint16_t *u16Ptr = (uint16_t*)nameBuf;
	char *GBKPtr = (char*)videoInfo->fileName;

	if(len > 0)
	{
		int gLen;
		for(int i=0; i<(0xa0/2); i+=16)
		{
			uint16_t *c16Ptr = &u16Ptr[i];
			if(c16Ptr[0] == 0x00c1)
			{
				for(int k=1; k<16; k++)
				{
					if(c16Ptr[k]<0x100) 
					{
						GBKPtr[cnt++] = c16Ptr[k];
					}
					else
					{					
						uint8_t *urlcode = Unicode_2_GBK(&c16Ptr[k]); /**unicode 转国标码**/
						GBKPtr[cnt++] = urlcode[0];
						GBKPtr[cnt++] = urlcode[1];
					}
					
					len--;					
					if(len == 0)
					{
						i = 1024;
						break;
					}
				}
			}				
		}		
	}
}

static int duvonn_get_current_index(DUVONN_GBT19056_VIDEO_FILE* videoInfo)
{
	struct tm fileTime;
	unsigned long u64Time;
	unsigned long timeLast = 0;
	char *license = duvonn_get_licence_info();
	char curFile[256], *s8Ptr;
	int lastIndex = -1;
	int bSize;
	DUVONN_RECORD_LIST *videoFile = videoInfo->video_record_file;

	if(strlen(license)==0) return -1;

	for(int i=0; i<videoInfo->maxFileNum; i++)
	{		
		if(strstr(videoFile[i].fileName, license)!=NULL)
		{
			bSize = strlen(videoFile[i].fileName);
			memcpy(curFile, videoFile[i].fileName, bSize);
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
			// logd_lc("[%d]:%d-%d-%d_%d:%d:%d --> %ld", i, fileTime.tm_year, fileTime.tm_mon, fileTime.tm_mday, fileTime.tm_hour, fileTime.tm_min, fileTime.tm_sec, u64Time);
			if(u64Time > timeLast)
			{
				timeLast = u64Time;
				lastIndex = i;
			}
		}
	}	
	
	return lastIndex;
}

static void duvonn_get_file_name(DUVONN_RECORD_LIST *gFile)
{
	int i, len, sectorB;
	char *pageBuf = (char*)malloc(4096);
	char* fileName;
	// logd_lc("Name->sector:%x, offset:%x",gFile->fileInfo.fileNameSector,gFile->fileInfo.fileNameOffset);
	sectorB = gFile->fileInfo.fileNameSector;	
	len = EMMC_PAGE;
	if((gFile->fileInfo.fileNameOffset+0xa0)>EMMC_PAGE)
	{
		len += EMMC_PAGE;
	}

	lc_dvr_duvonn_read_sector(pageBuf, len, sectorB);	
	fileName = pageBuf+ gFile->fileInfo.fileNameOffset;
	memset(gFile->fileName, 0, 128);	
	duvonn_unicoed_to_ascii(fileName, gFile);
	
	// logd_lc("RecordFile-->%s", gFile->fileName);
	free(pageBuf);
}

static void duvonn_get_video_list_info(DUVONN_GBT19056_VIDEO_FILE* videoInfo)
{
	int i, sectorB=0, sectorE=0;
	int cOffset;
	int curIndex;
	char *pageBuf = (char*)malloc(4096);
	DUVONN_RECORD_LIST *cVideoFile = videoInfo->video_record_file;
	char* fileName;

	for(i=0; i<videoInfo->maxFileNum; i++)	
	{				
		if(cVideoFile[i].fileInfo.fileNameSector >= sectorE)
		{
			sectorB = cVideoFile[i].fileInfo.fileNameSector;
			sectorE = cVideoFile[i].fileInfo.fileNameSector*EMMC_PAGE + cVideoFile[i].fileInfo.fileNameOffset;
			for(;;)
			{
				sectorE += 0xa0;
				if((sectorE%EMMC_PAGE)==0) break;
			}
			sectorE /= EMMC_PAGE;
			
			lc_dvr_duvonn_read_sector(pageBuf, (sectorE-sectorB)*EMMC_PAGE, sectorB);
			//logd_lc("load sector from %x to %x", sectorB, sectorE);
		}
		
		//logd_lc("file[%d] secotr:%x, offset:%x", i, cVideoFile[i].fileInfo.fileNameSector, cVideoFile[i].fileInfo.fileNameOffset);
		fileName = pageBuf+ (cVideoFile[i].fileInfo.fileNameSector-sectorB)*EMMC_PAGE + cVideoFile[i].fileInfo.fileNameOffset;
		
		//lc_dvr_mem_debug(fileName, 0xa0);
		//logd_lc("len:%d, hashVal:%x, calHash:%x", exFAT_GetNameLen(fileName), exFAT_GetNameHashVal(fileName), exFAT_ReHashVal(fileName));
		
		memset(cVideoFile[i].fileName, 0, 128);
		duvonn_unicoed_to_ascii(fileName, &cVideoFile[i]);
		// logd_lc("file[%02d]-->%s", i, cVideoFile[i].fileName);
	}
	free(pageBuf);

	curIndex = duvonn_get_current_index(videoInfo);
	logd_lc("curIndex[%d] --> %s", curIndex, videoInfo->video_record_file[curIndex].fileName);
	videoInfo->currentIdx = curIndex; /**为-1时，记录文件不存在，创建时加1刚好为0**/
}

//Hash 校验
static uint16_t duvonn_EntrySetChecksum(unsigned char octets[], long NumberOfBytes)
{
	unsigned short checksum = 0;
	long index = 0;
	for (index = 0; index < NumberOfBytes; index++)
	{
		if (index == 2 || index == 3)
		{
			continue;
		}
		checksum = ((checksum <<15) | (checksum>> 1)) + (unsigned short)octets[index];
	}
	return checksum;
}

//#define DEBUG_RESET_FILENAME
static void duvonn_file_name_write(DUVONN_RECORD_LIST* fileInfo, int type)
{
	if(fileInfo!=NULL)
	{
		uint8_t *u8Ptr = (uint8_t*)malloc(1024);
		uint8_t *wrkPtr;
		char unicodeStr[256];		
		char *s8Ptr = fileInfo->fileName;
		int p=0, len = strlen(s8Ptr), rLen=0;
		int blockLen = 512;
		int hashLen;
		uint16_t nameHash;
		uint16_t blockHash;
		
		logd_lc("new name:%s", s8Ptr);

		if(type)
			hashLen = 160;
		else 
			hashLen = 128;
		
		if((fileInfo->fileInfo.fileNameOffset+0xa0)>512) blockLen+=512;
		memset(unicodeStr, 0, 256);

		for(int i=0; i<len; i++)
		{
			if(s8Ptr[i]&0x80)
			{
				uint16_t exUnicode = GBK_2_Unicode(&s8Ptr[i]);
				unicodeStr[p++] = exUnicode&0xff;
				unicodeStr[p++] = (exUnicode>>8)&0xff;
				i+=1;				
			}
			else
			{
				unicodeStr[p++] = s8Ptr[i];
				unicodeStr[p++] = 0;
			}
			rLen++;
		}

		lc_dvr_duvonn_read_sector(u8Ptr, blockLen, fileInfo->fileInfo.fileNameSector); /**读取**/
		
		wrkPtr = u8Ptr+fileInfo->fileInfo.fileNameOffset;	/**修改**/
		
//		logd_lc("read back content:");
//		lc_dvr_mem_debug(wrkPtr, 0xa0);

		wrkPtr[0x00] = 0x85;
		wrkPtr[0x01] = 1+(rLen*2+29)/30; /**根据文件名长度定附加项**/
		wrkPtr[0x04] = 0x25;  		   /**文件只读属性**/	
		wrkPtr[0x20] = 0xC0;		/**属性2**/
		wrkPtr[0x21] = 0x03;		/**连续簇**/
		wrkPtr[0x16] = 0xa0;		/**保留项目**/
		wrkPtr[0x17] = 0xa0;
		wrkPtr[0x18] = 0xa0;
		
		exFAT_SetFileLen(wrkPtr, fileInfo->fileInfo.fileSize); /**文件的长度**/
		exFAT_SetStartSector(wrkPtr, fileInfo->fileInfo.sector_start); /**开始簇位置**/

		exFAT_SetNameLen(wrkPtr, rLen); /**写文件名**/
		exFAT_SetFileTime(wrkPtr);		/**文件创建，修改，访问时间**/
		nameHash = duvonn_exFAT_NameHashCal(0, unicodeStr, rLen*2);
		exFAT_SetNameHashVal(wrkPtr, nameHash);
		exFAT_SetNameContent(wrkPtr, unicodeStr, rLen);	/**文件名里不能有小写字母，否则系统不识别**/
		blockHash = duvonn_EntrySetChecksum(wrkPtr, hashLen);
		exFAT_SetBlockHashVal(wrkPtr, blockHash);

//		logd_lc("write back content:");
//		lc_dvr_mem_debug(wrkPtr, 0xa0);
	
		lc_dvr_duvonn_write_sector(u8Ptr, blockLen, fileInfo->fileInfo.fileNameSector); /**回写**/
		free(u8Ptr);
	}
}

void duvonn_gbt19056_file_initial(char *memBuf)
{
	char dstName[256];
	logd_lc("duvonn_gbt19056_file_initial!");
	duvonn_get_file_info(&(mDuvonnInfoRecord->baseConfig.fileCtrl), "6100.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->travel_record_file.fileCtrl),  "2100.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->doubtful_record_file.fileCtrl),"2200.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->overTime_record_file.fileCtrl),"2300.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->driver_record_file.fileCtrl),  "2400.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->daily_record_file.fileCtrl),	 "2500.VDR", memBuf, 1);
	duvonn_get_file_info(&(mDuvonnInfoRecord->faceFeat_file.fileCtrl), "5100.VDR", memBuf, 1);

	duvonn_get_file_name(&mDuvonnInfoRecord->travel_record_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->doubtful_record_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->overTime_record_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->driver_record_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->daily_record_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->faceFeat_file.fileCtrl);
	duvonn_get_file_name(&mDuvonnInfoRecord->baseConfig.fileCtrl);

	sprintf(dstName, "GBT19056_6100.VDR");
	if(strstr(mDuvonnInfoRecord->baseConfig.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->baseConfig.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->baseConfig.fileCtrl, 0);
	}		

	sprintf(dstName, "GBT19056_2100.VDR");
	if(strstr(mDuvonnInfoRecord->travel_record_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->travel_record_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->travel_record_file.fileCtrl, 0);
	}

	sprintf(dstName, "GBT19056_2200.VDR");
	if(strstr(mDuvonnInfoRecord->doubtful_record_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->doubtful_record_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->doubtful_record_file.fileCtrl, 0);
	}

	sprintf(dstName, "GBT19056_2300.VDR");
	if(strstr(mDuvonnInfoRecord->overTime_record_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->overTime_record_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->overTime_record_file.fileCtrl, 0);
	}

	sprintf(dstName, "GBT19056_2400.VDR");
	if(strstr(mDuvonnInfoRecord->driver_record_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->driver_record_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->driver_record_file.fileCtrl, 0);
	}

	sprintf(dstName, "GBT19056_2500.VDR");
	if(strstr(mDuvonnInfoRecord->daily_record_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->daily_record_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->daily_record_file.fileCtrl, 0);
	}

	sprintf(dstName, "GBT19056_5100.VDR");
	if(strstr(mDuvonnInfoRecord->faceFeat_file.fileCtrl.fileName, dstName)==NULL)
	{
		strcpy(mDuvonnInfoRecord->faceFeat_file.fileCtrl.fileName, dstName);
		duvonn_file_name_write(&mDuvonnInfoRecord->faceFeat_file.fileCtrl, 0);
	}	
}

void duvonn_suffering_initial(int cameraNumber, char *license)
{
	char *memBuf = (char*)malloc(1024*1024);	
	char *authorizeDat = (char*)sys_msg_get_memcache(128);	
	int total, i, filePerCam=0;
	pthread_t duvonn_thread;
	int ret;
	
	duvonn_set_licence_info(license);
	logd_lc("duvonn_suffering_initial -> %s", duvonn_get_licence_info());
	
	ret = lc_dvr_duvonn_storage_initial(authorizeDat);	/**灾备存储器驱动初始化**/
	//======================================================	
	if(ret>0)
	{
		lc_dvr_duvonn_unlock(authorizeDat, 0); /**需要解密处理**/
	}
	//======================================================	
	if(is_duvonn_device())
	{
		printf("[pid: %d] Start Test!!\n", getpid());	
		lc_dvr_duvonn_get_ind(memBuf, 1024*1024, 0);	
		mListItem = (DUVONN_RECORD_LIST*)malloc(sizeof(DUVONN_RECORD_LIST)*256+sizeof(DUVONN_GBT19056_INFO_FILE));
		mDuvonnInfoRecord = (((char*)mListItem)+sizeof(DUVONN_RECORD_LIST)*256);

		memset(mListItem, 0, (sizeof(DUVONN_RECORD_LIST)*256));
		memset(mDuvonnInfoRecord, 0, sizeof(DUVONN_GBT19056_INFO_FILE));	
		// duvonn_gbt19056_file_initial(memBuf);	/**初始化数据记录相关的文件**/	
		
		/**==================视频相关=================**/
		total = duvonn_get_file_info(mListItem, SECTOR_FREAT_VIDEO, memBuf, 0); //duvonn 第二个参数 "DAT"
		filePerCam = total/cameraNumber;

		logd_lc("total file:%d", total);
		for(i=0; i<cameraNumber; i++) //cameraNumber
		{
			logd_lc("camer %d start!", i);
			memset(&mDuvonnVideoRecord[i], 0, sizeof(DUVONN_GBT19056_VIDEO_FILE));
			mDuvonnVideoRecord[i].CameraId = i;
			mDuvonnVideoRecord[i].maxFileNum = filePerCam;
			mDuvonnVideoRecord[i].video_record_file = &mListItem[i*filePerCam];
			duvonn_get_video_list_info(&mDuvonnVideoRecord[i]);
		}

		for(; i<LC_DUVONN_CAMERA_NUMBER; i++)
		{
			mDuvonnVideoRecord[i].CameraId = -1;
		}
		//===============================================
	}
	free(memBuf);
}

void duvonn_suffering_unInitial(void)
{
	if(is_duvonn_device()==0) return;
	
	lc_dvr_duvonn_storage_unInitial();
	free(mListItem);
	mListItem = NULL;
}

void duvonn_camID_to_lisID(int* camID)
{
	switch(camID[0])
	{
		case LC_DUVONN_CHANEL_0:
			camID[0] = 0;
			break;
		case LC_DUVONN_CHANEL_1:
			camID[0] = 1;
			break;
	}
}

DUVONN_RECORD_LIST* duvonn_camera_get_curfile_info(int camID)
{
	DUVONN_RECORD_LIST *fileInfo = NULL;
	duvonn_camID_to_lisID(&camID);
	if(camID<LC_DUVONN_CAMERA_NUMBER)
	{
		if(mDuvonnVideoRecord[camID].CameraId>=0)
		{
			int index = mDuvonnVideoRecord[camID].currentIdx;
			fileInfo = &mDuvonnVideoRecord[camID].video_record_file[index];		
		}
	}
	return fileInfo;
}

int duvonn_camera_reset_file(int camID)
{
	GBT19056_INFO_FILE *fragCtr = NULL;
	duvonn_camID_to_lisID(&camID);

	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		int index = 0;
		struct tm *tm = NULL;
		DUVONN_RECORD_LIST *fileInfo;
		
		//duvonn_video_file_flush(camID);	/**保证遗留的数据被排空**/
		mDuvonnVideoRecord[camID].currentIdx = 0;
		getDateTime(&tm);
		for(;index<mDuvonnVideoRecord[camID].maxFileNum; index++)
		{
			fileInfo = duvonn_camera_get_curfile_info(camID);
			mDuvonnVideoRecord[camID].currentIdx++;
			{					
				char*rec_filename = fileInfo->fileName;
				int strP;
				
				logd_lc("rCam[%d].Idx-->%d/%d", camID, index, mDuvonnVideoRecord[camID].maxFileNum);
				logd_lc("rPrev name: %s", fileInfo->fileName);

				strP = sprintf(rec_filename, "%03d-GBT19056_%s_%d_", index, "YBXXXXX", camID);
				strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																			 tm->tm_hour, index/60, index%60, EXT_VIDEO);
				/**修改的文件名称必须为大写字母和unicode字符，如果有小写字母则系统显示不出来**/
				/**需要umount后再mount才能看到新的修改名称，否则文件名不变**/
				duvonn_file_name_write(fileInfo, 1);
			}			
		}				
		mDuvonnVideoRecord[camID].currentIdx = -1;
	}

	return mDuvonnVideoRecord[camID].currentIdx;
}

int duvonn_camera_creat_new_file(int camID)
{
	GBT19056_INFO_FILE *fragCtr = NULL;
	int listId = camID;
	duvonn_camID_to_lisID(&camID);
	
	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		int index;
		DUVONN_RECORD_LIST *fileInfo;
		
		//duvonn_video_file_flush(camID);	/**保证遗留的数据被排空**/
		mDuvonnVideoRecord[camID].currentIdx++;
		if(mDuvonnVideoRecord[camID].currentIdx>=mDuvonnVideoRecord[camID].maxFileNum)
		{
			mDuvonnVideoRecord[camID].currentIdx = 0;
		}
		index = mDuvonnVideoRecord[camID].currentIdx;
		fileInfo = duvonn_camera_get_curfile_info(camID);
		{
			struct tm *tm = NULL;
			char*rec_filename = fileInfo->fileName;
			int strP;
			getDateTime(&tm);
			//GBT19056_XXXXXXXX_X_yyyymmddhhmmss
			logd_lc("cam[%d].Idx-->%d/%d", listId, index, mDuvonnVideoRecord[camID].maxFileNum);
			logd_lc("prev name: %s", fileInfo->fileName);

			strP = sprintf(rec_filename, "%03d-GBT19056_%s_%c_", index, duvonn_get_licence_info(), g_CamToChar[listId]);
			strP += sprintf(rec_filename+strP, "%d%02d%02d%02d%02d%02d%s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
																		 tm->tm_hour, tm->tm_min, tm->tm_sec, EXT_VIDEO);
			/**修改的文件名称必须为大写字母和unicode字符，如果有小写字母则系统显示不出来**/
			/**需要umount后再mount才能看到新的修改名称，否则文件名不变**/
			duvonn_file_name_write(fileInfo, 1);
		}				
	}

	return mDuvonnVideoRecord[camID].currentIdx;
}


void *duvonn_record_handle(int rcType)
{
	GBT19056_INFO_FILE *oprateFile = NULL;
	switch(rcType)
	{
		case TRAVEL_RECORD:
			oprateFile = &(mDuvonnInfoRecord->travel_record_file);
			break;
		case DOUBTFUL_RECORD:
			oprateFile = &(mDuvonnInfoRecord->doubtful_record_file);
			break;
		case OVER_TIME_RECORD:
			oprateFile = &(mDuvonnInfoRecord->overTime_record_file);
			break;
		case DRIVER_RECORD:
			oprateFile = &(mDuvonnInfoRecord->driver_record_file);
			break;
		case DAILY_RECORD:
			oprateFile = &(mDuvonnInfoRecord->daily_record_file);
			break;	
		//case FACEFEAT_RECORD:
		//	oprateFile = &(mDuvonnInfoRecord->faceFeat_file);
		//	break;
		case SYSTEM_DB_SAVE:
			oprateFile = &(mDuvonnInfoRecord->baseConfig);
			break;
		case VIDEO_FILE_RECORD:
			oprateFile = &(mDuvonnInfoRecord->video_record_file);
			break;		
	}

	return oprateFile;
}

int duvonn_ctrl_seek(void* handle, int offset, int from)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)(handle);
	//logd_lc("duvonn_ctrl_seek-->%x!!", offset);
	//if(fragmentPtr->fp!=NULL)
	{
		int sector = offset/LC_CACHE_PAGE;
		int sectorCur = fragmentPtr->fileCtrl.fileInfo.sector_current;

		if(sector!=sectorCur)
		{		
			loff_t realPosition;
			if(fragmentPtr->fileCtrl.fileInfo.dirty)
			{
				realPosition = (loff_t)(sectorCur+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
				WriteSectors(fragmentPtr->recordCache, LC_CACHE_PAGE, realPosition);
			}

			realPosition = (loff_t)(sector+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
			ReadSectors(fragmentPtr->recordCache, LC_CACHE_PAGE, realPosition);
			fragmentPtr->fileCtrl.fileInfo.sector_current = sector;
			fragmentPtr->fileCtrl.fileInfo.dirty = 0;
		}
		
		fragmentPtr->fileCtrl.fileInfo.curPosition = offset;		
	}	

	return 0;
}

int duvonn_ctrl_write(void* wrBuf, int len, void* handle)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)(handle);
	//logd_lc("duvonn_write(%d)-->%x!!", len, fragmentPtr->fileCtrl.fileInfo.curPosition);
	//if(fragmentPtr->fp!=NULL)
	{
		int cachePt = fragmentPtr->fileCtrl.fileInfo.curPosition&(LC_CACHE_PAGE-1);
		int sector = fragmentPtr->fileCtrl.fileInfo.curPosition/LC_CACHE_PAGE;
		int sectorCur = fragmentPtr->fileCtrl.fileInfo.sector_current;
		char* cacheMem = (char*)fragmentPtr->recordCache;
		int remain = len;
		int inOffset = 0;

		loff_t realPosition = (loff_t)(sector+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
		fragmentPtr->fileCtrl.fileInfo.curPosition += len;
		
		if(cachePt>0)
		{
			if(sector!=sectorCur)
			{		
				ReadSectors(cacheMem, LC_CACHE_PAGE, realPosition);
				fragmentPtr->fileCtrl.fileInfo.sector_current = sector;
			}
			
			inOffset = LC_CACHE_PAGE-cachePt;
			if(inOffset>len)
			{			
				inOffset = len;
			}
			
			memcpy(cacheMem+cachePt, wrBuf, inOffset);
			remain -= inOffset;	

			if(remain)
			{				
				WriteSectors(cacheMem, LC_CACHE_PAGE, realPosition);
				realPosition += LC_CACHE_PAGE;
				fragmentPtr->fileCtrl.fileInfo.dirty = 0; /**整页则关闭的时候不回写**/
			}
			else
			{			
				fragmentPtr->fileCtrl.fileInfo.dirty = 1;
			}
		}
		
	    if(remain>0)
	    {			    
	    	int byteCnt = 0;
			if(remain>LC_CACHE_PAGE)
			{
				byteCnt = WriteSectors(wrBuf+inOffset, remain&(~(LC_CACHE_PAGE-1)), realPosition);
			}

			remain &= (LC_CACHE_PAGE-1);						
			
			if(remain)
			{
				ReadSectors(fragmentPtr->recordCache, LC_CACHE_PAGE, realPosition+byteCnt);			
				memcpy(cacheMem, wrBuf+inOffset+byteCnt, remain);
				fragmentPtr->fileCtrl.fileInfo.dirty = 1;				
				fragmentPtr->fileCtrl.fileInfo.sector_current = fragmentPtr->fileCtrl.fileInfo.curPosition/LC_CACHE_PAGE;
			}
	    }
		
	}

	return len;
}


int duvonn_ctrl_read(void* rdBuf, int len, void* handle)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)(handle);
	//logd_lc("duvonn_read(%d)<--%x!!", len, fragmentPtr->fileCtrl.fileInfo.curPosition);
	//if(fragmentPtr->fp!=NULL)
	{
		int cachePt = fragmentPtr->fileCtrl.fileInfo.curPosition&(LC_CACHE_PAGE-1);
		int sector = fragmentPtr->fileCtrl.fileInfo.curPosition/LC_CACHE_PAGE;
		int sectorCur = fragmentPtr->fileCtrl.fileInfo.sector_current;
		char* cacheMem = (char*)fragmentPtr->recordCache;
		int remain = len;
		int inOffset = 0;

		loff_t realPosition = (loff_t)(sector+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
		fragmentPtr->fileCtrl.fileInfo.curPosition += len;

		if(cachePt>0)
		{
			if(sector!=sectorCur) /**open后第一次读**/
			{					
				ReadSectors(cacheMem, LC_CACHE_PAGE, realPosition);
				fragmentPtr->fileCtrl.fileInfo.sector_current = sector;
				fragmentPtr->fileCtrl.fileInfo.dirty = 0;				
			}
			
			inOffset = LC_CACHE_PAGE-cachePt;
			if(inOffset>len)
			{
				inOffset = len;
			}
			
			memcpy(rdBuf, cacheMem+cachePt, inOffset);
			remain -= inOffset;	

			if(remain)
			{	/**超过一页**/
				if(fragmentPtr->fileCtrl.fileInfo.dirty)
				{
					loff_t backPosition = (loff_t)(sectorCur+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
					WriteSectors(cacheMem, LC_CACHE_PAGE, backPosition); /**先写操作，然后再读超过一个页，这个数据要回写**/
					fragmentPtr->fileCtrl.fileInfo.dirty = 0; /**清除回写标记**/
				}	
				realPosition += LC_CACHE_PAGE;
			}
		}
		
	    if(remain>0)
	    {			
	    	int byteCnt = 0;
			if(remain>LC_CACHE_PAGE)
			{			
				byteCnt = ReadSectors(rdBuf+inOffset, remain&(~(LC_CACHE_PAGE-1)), realPosition);				 
			}

			remain &= (LC_CACHE_PAGE-1);						
			
			if(remain)
			{				
				ReadSectors(fragmentPtr->recordCache, LC_CACHE_PAGE, realPosition+byteCnt);			
				memcpy(rdBuf+inOffset+byteCnt, cacheMem, remain);
				fragmentPtr->fileCtrl.fileInfo.dirty = 0;
				fragmentPtr->fileCtrl.fileInfo.sector_current = fragmentPtr->fileCtrl.fileInfo.curPosition/LC_CACHE_PAGE;
			}
	    }		
	}	
	
	return len;  /**duvonn是预设大小的文件，所以一定会读取成功**/
}

void duvonn_ctrl_fflush(void* handle)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)(handle);
	//logd_lc("duvonn_ctrl_fflush!!");
	if(fragmentPtr->fileCtrl.fileInfo.dirty)
	{
		int sectorCur = fragmentPtr->fileCtrl.fileInfo.sector_current;
		loff_t realPosition = (loff_t)(sectorCur+fragmentPtr->fileCtrl.fileInfo.sector_start)*LC_CACHE_PAGE;
		char* cacheMem = (char*)fragmentPtr->recordCache;

		// logd_lc("write back dirty page-->%llx!", realPosition);		
		WriteSectors(cacheMem, LC_CACHE_PAGE, realPosition);
		fragmentPtr->fileCtrl.fileInfo.dirty = 0;
	}
}

/**灾备存储器的open跟close必须成对出现，否则其他进程的调用会被卡住**/
void *duvonn_ctrl_open(int rcType, char para[])
{
	GBT19056_INFO_FILE *fragmentPtr = duvonn_record_handle(rcType);
	logd_lc("duvonn_ctrl_open->%s!!", fragmentPtr->fileCtrl.fileName);
	fragmentPtr->fileCtrl.fileInfo.curPosition = 0;
	fragmentPtr->fileCtrl.fileInfo.sector_current = 0xffffffff;	
	return fragmentPtr;
}

void *duvonn_video_open(int camID, int index)
{
	GBT19056_INFO_FILE *fragmentPtr = duvonn_record_handle(VIDEO_FILE_RECORD);
//	logd_lc("duvonn_video_open-->%s!!", fragmentPtr->fileCtrl.fileName);
	duvonn_camID_to_lisID(&camID);

	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		int cIdx = index;
		DUVONN_RECORD_LIST *fileInfo;
		
		if(cIdx<0) cIdx += mDuvonnVideoRecord[camID].maxFileNum;
		fileInfo = &mDuvonnVideoRecord[camID].video_record_file[cIdx];
		memcpy(&fragmentPtr->fileCtrl, fileInfo, sizeof(DUVONN_RECORD_LIST));
	}


	fragmentPtr->fileCtrl.fileInfo.curPosition = 0;
	fragmentPtr->fileCtrl.fileInfo.sector_current = 0xffffffff;	
	logd_lc("Video file open:%s", fragmentPtr->fileCtrl.fileName);
	return fragmentPtr;
}

int duvonn_get_video_tail(int camID, int index)
{
	duvonn_camID_to_lisID(&camID);
	GBT19056_INFO_FILE *fragmentPtr = &mDuvonnVideoRecord[camID].video_record_file[index];
	return fragmentPtr->fileCtrl.fileInfo.fileSize;
}

void *duvonn_get_video_name(void* handle)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)handle;
	return fragmentPtr->fileCtrl.fileName;
}

int duvonn_video_write(int camID, int index, void* inBuf, int len, int offset) /**offset必须页对齐，len也需要为4K的整数倍**/
{
	DUVONN_RECORD_LIST *fileInfo = NULL;
	int remain = -1;	
	if(is_duvonn_device()==0) return -1;
	duvonn_camID_to_lisID(&camID);
	
	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		fileInfo = &mDuvonnVideoRecord[camID].video_record_file[index];
	}

	if(fileInfo!=NULL)
	{
		int sector = offset/LC_CACHE_PAGE;
		loff_t realPosition = (loff_t)(sector+fileInfo->fileInfo.sector_start)*LC_CACHE_PAGE;
		// logd_lc("duvonn_video(%d).C[%d](%x):%d->%llx",index, camID, inBuf, len, realPosition);
		remain = WriteSectors(inBuf, len, realPosition);
	}
	
	return remain;
}

int duvonn_video_read(int camID, int index, void* inBuf, int len, int offset) /**offset必须页对齐，len也需要为4K的整数倍**/
{
	DUVONN_RECORD_LIST *fileInfo = NULL;
	int remain = 0;	
	if(is_duvonn_device()==0) return -1;
	duvonn_camID_to_lisID(&camID);
	
	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		fileInfo = &mDuvonnVideoRecord[camID].video_record_file[index];
	}

	if(fileInfo!=NULL)
	{
		int sector = offset/LC_CACHE_PAGE;
		remain = len;		
		loff_t realPosition = (loff_t)(sector+fileInfo->fileInfo.sector_start)*LC_CACHE_PAGE;
		ReadSectors(inBuf, remain, realPosition);
	}
	
	return remain;
}


char* duvonn_video_name_get(int camID, int index)
{
	DUVONN_RECORD_LIST *fileInfo = NULL;
//	logd_lc("duvonn_video_open-->%s!!", fragmentPtr->fileCtrl.fileName);
	duvonn_camID_to_lisID(&camID);
	
	if(mDuvonnVideoRecord[camID].CameraId>=0)
	{
		fileInfo = &mDuvonnVideoRecord[camID].video_record_file[index];
		if(strstr(fileInfo->fileName, duvonn_get_licence_info())==NULL) fileInfo = NULL;
	}


	if(fileInfo!=NULL)
	{
		return fileInfo->fileName;
	}
	else
	{
		return NULL;
	}
}

int duvonn_video_get_list_info(int camID, int* curIdx)
{
	duvonn_camID_to_lisID(&camID);
	*curIdx = mDuvonnVideoRecord[camID].currentIdx;
	return mDuvonnVideoRecord[camID].maxFileNum;
}

/**一定要释放灾备存储器，否则其他进程的调用会被信号量卡住**/
void duvonn_ctrl_close(void* handle)
{
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)handle;	
	duvonn_ctrl_fflush(handle);
	logd_lc("duvonn_ctrl_close-->%s!!", fragmentPtr->fileCtrl.fileName);
}

void duvonn_ctrl_factory_reset(int rcType)
{
	void* handle = duvonn_ctrl_open(rcType, NULL);	
	GBT19056_INFO_FILE *fragmentPtr = (GBT19056_INFO_FILE*)handle;
	int blockLen = 4096;
	char *memBuf = (char*)malloc(blockLen);
	
	memset(memBuf, 0xff, blockLen);	
	duvonn_ctrl_seek(handle, 0, SEEK_SET);
	for(int len = 0; len < fragmentPtr->fileCtrl.fileInfo.fileSize; len += blockLen)
	{
		duvonn_ctrl_write(memBuf, blockLen, handle);
	}
	duvonn_ctrl_close(handle);
	
	free(memBuf);
}

void sem_test_thread(void *arg)
{
	int ret = CreateSemaphore();
	printf("[pid: %d] Start Test!!\n", getpid());
    while(1)
    {
	    ret = SemaphoreP();
	    if(ret < 0)
	    {
	        printf("SemaphoreP failed!\n");
	        break;
	    }    
		/**-------------------------临界代码区间------------------------------**/
	    printf("[pid: %d] FileWrite critical zone\n", getpid());
	    sleep(1); // 测试一下别的进程是不是在等待
	 	//---------------------------------------------------------------------		
		ret = SemaphoreV();
		if(ret < 0)
		{
			printf("SemaphoreV failed!\n");
			break;
		}
    } 

    return 0;
}

void duvonn_randm_write_read(void)
{				
	GBT19056_INFO_FILE *file = duvonn_video_open(0, 3);
	int sectorStart = file->fileCtrl.fileInfo.sector_start;
	char* cacheMem = (char*)malloc(4096+LC_DUVONN_TEST_LEN);
	char* wrkMem = cacheMem+LC_DUVONN_TEST_LEN;
	uint16_t* u16Ptr = (uint16_t*)cacheMem;
	int writeLen = 356;
	int writePosition = 418;
	int loopTimes = 0;
	int curPosition = 0;
	int verify;
		
	logd_lc("duvonn_randm_serial_read start-->%x!!", sectorStart);
	srand(1345);
	
	for(int k=0; k<LC_DUVONN_TEST_LEN/sizeof(uint16_t); k++) u16Ptr[k] = k;
	lc_dvr_duvonn_write_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);

	for(; loopTimes<8; loopTimes++)
	{	
		uint16_t *cPtr = (uint16_t*)wrkMem;
		uint8_t *wrtPtr = NULL;
		int cmpCnt, startVal;
		writeLen = rand()&(0x7ff);
		writeLen *=2;
		curPosition = rand()&(8*1024-1);
		curPosition *= 2;
		duvonn_ctrl_seek(file, curPosition, 0);
		
		logd_lc("loopTimes[%d]-->Write:%d byte @ %d", loopTimes,  writeLen, curPosition);

		/**测试随机读写**/
		wrtPtr = cacheMem+curPosition;
		duvonn_ctrl_write(wrtPtr, writeLen, file);
		memset(cacheMem, 0xff, LC_DUVONN_TEST_LEN);
		lc_dvr_duvonn_read_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);
		startVal = 0;
		for(cmpCnt=0; cmpCnt<LC_DUVONN_TEST_LEN/sizeof(uint16_t); cmpCnt++, startVal++)
		{
			if(u16Ptr[cmpCnt]!=(startVal))
			{
				logd_lc("Write no equal @ %d", cmpCnt);
				break;
			}
		}

		writeLen = rand()&(0x7ff);
		writeLen *=2;
		curPosition = rand()&(8*1024-1);
		curPosition *= 2;
		duvonn_ctrl_seek(file, curPosition, 0);

		logd_lc("Read:%d byte @ %d", writeLen, curPosition);
		/**测试随机读写**/
		memset(wrkMem, 0xff, 4096);
		duvonn_ctrl_read(wrkMem, writeLen, file);
		//lc_dvr_mem_debug(wrkMem, 1024);
		startVal = curPosition/2;
		for(cmpCnt=0; cmpCnt<writeLen/sizeof(uint16_t); cmpCnt++, startVal++)
		{
			if(cPtr[cmpCnt]!=(startVal))
			{
				logd_lc("Read no equal @ %d", cmpCnt);
				break;
			}
		}		
	}
	
	memset(cacheMem, 0xff, LC_DUVONN_TEST_LEN);
	lc_dvr_duvonn_read_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);
	for(verify=0; verify<LC_DUVONN_TEST_LEN/sizeof(uint16_t); verify++) 
	{
		if(u16Ptr[verify] != verify)
		{
			logd_lc("Mem&Flash [%d] no equal!", verify);
			break;
		}
	}
	
	duvonn_ctrl_close(file);
	logd_lc("Test finish --> %d!", verify);
	free(cacheMem);
}

/**随机连续写测试**/
void duvonn_randm_serial_read(void)
{				
	GBT19056_INFO_FILE *file = duvonn_video_open(0, 3);
	int sectorStart = file->fileCtrl.fileInfo.sector_start;
	char* cacheMem = (char*)malloc(4096+LC_DUVONN_TEST_LEN);
	char* wrkMem = cacheMem+LC_DUVONN_TEST_LEN;
	uint16_t* u16Ptr = (uint16_t*)cacheMem;
	int writeLen = 356;
	int writePosition = 418;
	int loopTimes = 0;
	int curPosition = 0;
	int verify;
		
	logd_lc("duvonn_randm_serial_read start-->%x!!", sectorStart);
	srand(1345);
	
	for(int k=0; k<LC_DUVONN_TEST_LEN/sizeof(uint16_t); k++) u16Ptr[k] = k;
	lc_dvr_duvonn_write_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);

	for(; loopTimes<8; loopTimes++)
	{	
		uint16_t *cPtr = (uint16_t*)wrkMem;
		uint8_t *wrtPtr = NULL;
		int cmpCnt, startVal;
		writeLen = rand()&(0x1ff);
		writeLen *=2;

		if((curPosition+writeLen)>LC_DUVONN_TEST_LEN)
		{
			curPosition = rand()&(0x1ff);
			curPosition *= 2;
			duvonn_ctrl_seek(file, curPosition, 0);
			logd_lc("seek to %d", curPosition);
		}
		
		logd_lc("\nloopTimes[%d]-->OPT:%d byte @ %d", loopTimes,  writeLen, curPosition);
#if 1
		/**测试连续随机写**/
		wrtPtr = cacheMem+curPosition;
		duvonn_ctrl_write(wrtPtr, writeLen, file);
		memset(cacheMem, 0xff, LC_DUVONN_TEST_LEN);
		lc_dvr_duvonn_read_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);
		startVal = 0;
		for(cmpCnt=0; cmpCnt<LC_DUVONN_TEST_LEN/sizeof(uint16_t); cmpCnt++, startVal++)
		{
			if(u16Ptr[cmpCnt]!=(startVal))
			{
				logd_lc("no equal @ %d", cmpCnt);
				break;
			}
		}
		logd_lc("mem verify num %d", cmpCnt);	
		curPosition+=writeLen;
#endif
#if 0
		/**测试连续随机读**/
		memset(wrkMem, 0xff, 4096);
		duvonn_ctrl_read(wrkMem, writeLen, file);
		//lc_dvr_mem_debug(wrkMem, 1024);
		startVal = curPosition/2;
		for(cmpCnt=0; cmpCnt<writeLen/sizeof(uint16_t); cmpCnt++, startVal++)
		{
			if(cPtr[cmpCnt]!=(startVal))
			{
				logd_lc("no equal @ %d", cmpCnt);
				break;
			}
		}
		logd_lc("mem verify num %d", cmpCnt);
		curPosition+=writeLen;
#endif		
		
	}
	
	memset(cacheMem, 0xff, LC_DUVONN_TEST_LEN);
	lc_dvr_duvonn_read_sector(cacheMem, LC_DUVONN_TEST_LEN, sectorStart);
	for(verify=0; verify<LC_DUVONN_TEST_LEN/sizeof(uint16_t); verify++) 
	{
		if(u16Ptr[verify] != verify)
		{
			logd_lc("Mem&Flash [%d] no equal!", verify);
			break;
		}
	}
	
	duvonn_ctrl_close(file);
	logd_lc("Test finish --> %d!", verify);
	free(cacheMem);
}

void duvonn_storage_test(void)
{	
	char *cacheMem = (char*)malloc(8192);
	uint16_t *u16Ptr = (uint16_t*)cacheMem;
	int pageSize = getpagesize()*LC_PAGESIZE_TIMES;	
		
	duvonn_suffering_initial(LC_DUVONN_CAMERA_NUMBER, "CEA123456");		/**初始化摄像头数**/
//	duvonn_camera_creat_new_file(0); 					/**对应摄像头通道创建一个新文件名**/

//	duvonn_randm_serial_read();	/**测试连续读或者连续写**/
	duvonn_randm_write_read();	/**测试随机读写**/
	
	free(cacheMem);
	duvonn_suffering_unInitial();	
}

/**videoData 需要存储到非易失存储设备的数据缓存的指针**/
/**datLen    本次需要写入的字节数**/
/**encrypt   写入数据是否为加密状态（加密状态表示经过国密芯片进行加密处理后的数据）**/
//void vimicro_mp4_data_write_callback(void* videoData, int datLen, int encrypt);

