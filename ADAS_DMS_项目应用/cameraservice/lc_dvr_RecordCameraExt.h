#ifndef __RECORDCAMERA_EXT_H__
#define __RECORDCAMERA_EXT_H__
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
//---------------------------------------------------
extern int Is_InfoList_Initial(void);
extern void lc_dvr_set_storage_limit(int trate);
extern int RecordGetPerFileSize(int camId);
extern void RecordCameraInitial(void);
extern void RecordCameraRelease(void);
extern int RecordVideoEncParmInit(int camId);
extern void RecordGenPicNameFromVideo(char* name);
extern int RecordInitFileListDir(int camId);
extern int RecordInitEmergencyDir(void);
extern int RecordInitSoundRecordDir(void);
extern int RecordGenEmergencyFile(char *name, int duration, int camId);
extern void RecordSetFileToDstCam(char* name, int camId);
extern int RecordGetfileDir(char *name, int camId);
extern int RecordInserDuvonnFileName(char* name, int camId, int index);
extern int RecordGenEventfile(char *name, int camId);
extern int RecordGenfilename(char *name, int camId);
extern int RecordFileMaxNumber(void);
extern char* RecordFileFetchByIndex(int chanel, int index);
extern int RecordFileListFetch(void* fetchInfo);
extern int RecordSetVideoPath(char* path);
extern void Record_fileInfo_dbsave(void);
extern void Record_fileInfo_initial(void);
extern int RecordEmergencyListFetch(void* fetchInfo);
extern int RecordAudioListFetch(void* fetchInfo);
extern void RecordSetCurEmergerncyType(int warnType);
extern int Record_video_file_copy(int camId, int index, char* dir, char* fileName);
extern int RecordImageListFetch(void* fetchInfo);
extern int RecordInitImageRecordDir(void);
extern int RecordGenImageFile(char *name, int chanel);
extern void Record_File_unLock(char* name);
extern int Record_File_Lock(char *name, char* lockPath);
extern int Record_Fetch_Path_By_ID(int fID, void* fileDetial);
//---------------------------------------------------
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif


