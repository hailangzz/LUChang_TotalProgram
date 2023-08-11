#ifndef _STORAGE_MANAGER_H
#define _STORAGE_MANAGER_H

#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <fcntl.h>

//long long totalSize(const char *path);
//long long availSize(const char *path);

#define MOUNT_POINT "/mnt/UDISK/"
#define MOUNT_PATH MOUNT_POINT"/"
#define SZ_1M ((unsigned long long)(1<<20))
#define SZ_1G ((unsigned long long)(1<<30))
//#define RESERVED_SIZE (SZ_1M * 300)
//#define RESERVED_SIZE ((unsigned long long)((SZ_1M*500)/SZ_1M))
/**保留给报警事件的空间为4G**/
#define EMERGENCY_SIZE (4*1024)
#define SOUND_REC_SIZE (1*1024)
#define IMAGE_REC_SIZE (1*1024)

#define RESERVED_SIZE ((unsigned long long)((SZ_1M*(500))/SZ_1M))

#define FILE_SZ_1M ((uint32_t)(1*1024*1024))

#define CSI_PREALLOC_SIZE ((uint32_t)(FILE_SZ_1M * 200))
#define UVC_PREALLOC_SIZE ((uint32_t)(FILE_SZ_1M * 200))
#define AWMD_PREALLOC_SIZE ((uint32_t)(FILE_SZ_1M * 100))


#define CDR_TABLE "CdrFile"

#define FILE_EXSIST(PATH)	(!access(PATH, F_OK))

enum
{
    RET_IO_OK = 0,
    RET_IO_NO_RECORD = -150,
    RET_IO_DB_READ = -151,
    RET_IO_DEL_FILE = -152,
    RET_IO_OPEN_FILE = -153,
    RET_NOT_MOUNT = -154,
    RET_DISK_FULL = -155,
    RET_NEED_LOOP_COVERAGE = -156
};

int needDeleteFiles();
unsigned long long totalSize(const char *path);

unsigned long long availSize(const char *path);

int isMounted(const char *checkPath);
int  createdir(char *path);
unsigned long get_file_size(const char *path);

#endif


