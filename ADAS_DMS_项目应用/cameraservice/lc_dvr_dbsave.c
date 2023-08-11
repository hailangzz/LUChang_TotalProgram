#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "lc_dvr_comm_def.h"
//==============================================
#include "lc_dvr_record_log.h"
//==============================================
typedef struct{
	int initSign;
	int checksum;
	int length;
	int saveIndex;
	char content[16];
}LC_DBSAVE_HEAD;

int lc_dvr_db_verify(LC_DBSAVE_HEAD* dbsave)
{
	int checkSum = LC_DBSAVE_INITIAL_SIGN+dbsave->length+dbsave->saveIndex;
	char *dat = (char*)dbsave->content;
	for(int i=0; i<dbsave->length; i++)
	{
		checkSum+=dat[i];
	}
	return checkSum;
}

void lc_dvr_dbsave_content(char* fname, void* input, int datLen, int blockLen)
{
	FILE* fp = fopen(fname, "rb+");
	logd_lc("lc_dvr_dbsave_content(%x)->%s", fp, fname);
	if(fp != NULL)
	{				
		char *memBuf = (char*)malloc(blockLen*2);
		LC_DBSAVE_HEAD *dbSaveH[2];
		int i, curIdx = -1;
		
		memset(memBuf, 0, blockLen*2);
		dbSaveH[0] = (LC_DBSAVE_HEAD*)memBuf;
		dbSaveH[1] = (LC_DBSAVE_HEAD*)(memBuf+blockLen);
		fseek(fp, 0, SEEK_SET);
		fread(memBuf, 1, blockLen*2, fp);

		for(i=0; i<2; i++)
		{
			if(dbSaveH[i]->initSign==LC_DBSAVE_INITIAL_SIGN)
			{
				if(dbSaveH[i]->checksum==lc_dvr_db_verify(dbSaveH[i]))
				{
					if(dbSaveH[i]->saveIndex>curIdx) 
					{
						curIdx = dbSaveH[i]->saveIndex;					
						logd_lc("sCurIdx->%d", curIdx);
					}
				}
			}
		}

		curIdx++;
		i=curIdx&0x01;
		logd_lc("new save curIdx[%d]->%d", i, curIdx);
		memset(dbSaveH[i], 0, blockLen);
		dbSaveH[i]->saveIndex = curIdx;
		dbSaveH[i]->initSign = LC_DBSAVE_INITIAL_SIGN;
		dbSaveH[i]->length = datLen;
		memcpy(dbSaveH[i]->content, input, datLen);
		dbSaveH[i]->checksum = lc_dvr_db_verify(dbSaveH[i]);

		fseek(fp, i*blockLen, SEEK_SET);
		fwrite(dbSaveH[i], 1, blockLen, fp);
		fdatasync(fileno(fp));
		fclose(fp);
		free(memBuf);
	}
}

int lc_dvr_dbget_content(char* fname, void* output, int blockLen)
{
	FILE* fp = fopen(fname, "rb+");
	int datLen=0;
	logd_lc("lc_dvr_dbget_content(%x)->%s", fp, fname);
	if(fp != NULL)
	{				
		char *memBuf = (char*)malloc(blockLen*2);
		LC_DBSAVE_HEAD *dbSaveH[2];
		int i, curIdx = -1;
		
		memset(memBuf, 0, blockLen*2);
		dbSaveH[0] = (LC_DBSAVE_HEAD*)memBuf;
		dbSaveH[1] = (LC_DBSAVE_HEAD*)(memBuf+blockLen);	
		fseek(fp, 0, SEEK_SET);
		fread(memBuf, 1, blockLen*2, fp);

		fclose(fp);

		for(i=0; i<2; i++)
		{
			if(dbSaveH[i]->initSign==LC_DBSAVE_INITIAL_SIGN)
			{
				if(dbSaveH[i]->checksum==lc_dvr_db_verify(dbSaveH[i]))
				{
					if(dbSaveH[i]->saveIndex>curIdx)
					{
						curIdx = dbSaveH[i]->saveIndex;
						logd_lc("gCurIdx->%d", curIdx);
					}
				}
			}
		}

		if(curIdx>=0)
		{
			i=curIdx&0x01;
			memcpy(output, dbSaveH[i]->content, dbSaveH[i]->length);
			datLen = dbSaveH[i]->length;
			logd_lc("last get curIdx[%d]->%d(%d)", i, curIdx, dbSaveH[i]->length);
		}
		else
		{
			logd_lc("no info store!");
		}

		free(memBuf);
	}else{
		if(access(fname, F_OK) != 0)//
		{
			logd_lc("creat record list file!");
			xxCreateMyFile(fname, blockLen*2);
		}
	}

	return datLen;
}




