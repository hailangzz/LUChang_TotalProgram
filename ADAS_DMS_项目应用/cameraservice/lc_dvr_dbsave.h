#ifndef LC_DVR_DBSAVE_H
#define LC_DVR_DBSAVE_H
#ifdef __cplusplus
extern "C" {
#endif

void lc_dvr_dbsave_content(char* fname, void* input, int datLen, int blockLen);
int  lc_dvr_dbget_content(char* fname, void* output, int blockLen);

#ifdef __cplusplus
	}
#endif
#endif

