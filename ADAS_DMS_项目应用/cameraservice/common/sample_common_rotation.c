#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "vim_type.h"
#include "mpi_rotation.h"
#include "sample_common_rotation.h"

#include "mpi_vb.h"
#include "mpi_sys.h"

#define HEAP_SIZE (1*512*1024*1024)	 //1GB
#define uint64_t unsigned long long
#define NSEC_PER_SEC 1000000000LL

#define THREAD_STATUS_RUN  1
#define THREAD_STATUS_STOP 0

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define ALIGN_UP(x, a) (((x)+(a)-1)/(a) * (a))
#define ALIGN(x, a) (((x)+(a)-1)/(a) * (a))

//#define FPS

VIM_BOOL s_bExit;
THREAD_HANDLE_S *hThread = NULL;
static 	void *mPattern[MAX_VPSS_VIDEO_POINT][2] = {NULL};

static VIM_U64  Sum = 0;
static VIM_BOOL s8Sample_num = VIM_FALSE;

static VIM_U32  u32SamChnIdIn[MAX_VPSS_GROUP]       = {0};
static VIM_U32  u32SamChnIdOut[MAX_VPSS_GROUP]       = {0};

////////////////////VB////////////////////
static VIM_U32 s32VbPoolIn 			= 0;
static VIM_CHAR VbPoolInName[20] 	= {0};
static VIM_U32	 s32VbInBlk  		= 0;

static VIM_U32 s32VbPoolOut 		= 0;
static VIM_CHAR VbPoolOutName[20] 	= {0};
static VIM_U32	 s32VbOutBlk 		= 0;

static VIM_U32  u32Vbsize 			= 0;
/////////////////////////////////////////



VIM_S32 SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(PIXEL_FORMAT_E enPixfmt, VIM_IMG_FORMAT_E *enFormat, VIM_IMG_BITS_E *enBits)
{
	VIM_S32 s32Ret = 0;

	if((enFormat == NULL) || (enBits == NULL))
	{
		return VIM_VPSS_ERROR_NO_VALID_MEMRY;
	}
	switch(enPixfmt) {

		case PIXEL_FORMAT_YUV_PLANAR_422_P10:
			*enFormat = E_IMG_FORMAT_422P;
			*enBits = E_IMG_BITS_10;
		
			break;
		case PIXEL_FORMAT_YUV_SEMIPLANAR_422_P10:
			*enFormat = E_IMG_FORMAT_422SP;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_YUV_PLANAR_420_P10:
			*enFormat = E_IMG_FORMAT_420P;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10:
			*enFormat = E_IMG_FORMAT_420SP;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_YUV_PLANAR_422:
			*enFormat = E_IMG_FORMAT_422P;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
			*enFormat = E_IMG_FORMAT_422SP;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_YUV_PLANAR_420:
			*enFormat = E_IMG_FORMAT_420P;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
			*enFormat = E_IMG_FORMAT_420SP;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_RGB_PLANAR_601:
			*enFormat = E_IMG_FORMAT_601P;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_RGB_PLANAR_601_P10:
			*enFormat = E_IMG_FORMAT_601P;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_RGB_PLANAR_709:
			*enFormat = E_IMG_FORMAT_709P;
			*enBits = E_IMG_BITS_8;

			break;
		case PIXEL_FORMAT_RGB_PLANAR_709_P10:
			*enFormat = E_IMG_FORMAT_709P;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_YCbCr_PLANAR:
			*enFormat = E_IMG_FORMAT_422SP;
			*enBits = E_IMG_BITS_10;

			break;
		case PIXEL_FORMAT_YUV_422_INTERLEAVE:
			*enFormat = E_IMG_FORMAT_422I;
			*enBits = E_IMG_BITS_8;

			break;
		default:
			s32Ret = -1;
			break;
	}

	return s32Ret;
	
}


static VIM_S32 Tools_SaveTestFrameToFile(VIM_U32 u32Chn, const VIM_S8* path, VIM_VPSS_FRAME_INFO_S *pstFrameInfo, VIM_U16 EnDrawNo)
{
	char filename[256] = {0};
	FILE *file;
	int ret  = 0;
	int i  = 0;
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;

	char* mbus_formats[] = {
		"YUV422S",
		"YUV422S10",
		"YUV420S10",
		"YUV420S",
		"YUV422P",
		"YUV422P10",
		"YUV420P",
		"YUV420P10",
		"RGB601",
		"RGB601P10",
		"RGB709",
		"RGB709P10",
		"YUV422I",
	};
	
	if (pstFrameInfo == NULL)
	{
		return -1;
	}

	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(pstFrameInfo->enPixfmt, &enFormat, &enBits);
	
    if(enBits == 0)
	{
		enBits  = 1;
	}
    else
	{
		enBits  = 0; 
	}
	printf("EnDrawNo = %d\n",EnDrawNo);
	sprintf(filename, "%s/chn%d-%dx%d-%s-%s-Rot-all.bin", path, u32Chn,
								pstFrameInfo->u16Width,pstFrameInfo->u16Height,
								mbus_formats[enFormat * 2 + enBits],
								"UV");

	file = fopen(filename, "ab+");
	//printf("Tools_SaveTestFrameToFile filename = %s\n",filename);
	if (file != NULL) {

		for(i = 0; i < 2 && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		{
			ret = fwrite( pstFrameInfo->pVirAddrStd[i],1, pstFrameInfo->u32ByteUsedStd[i], file);
			if (ret == 0)
			{
				printf("chn %d write frame error %d/%d\n", u32Chn,ret,pstFrameInfo->u32ByteUsedStd[i]);
			}
		}

		fclose(file);
	}
	
	
	return 0;
}

int aaaa = 0;

static VIM_S32 Tools_GetTestFrameFromFile( VIM_U32 u32Chn, VIM_VPSS_FRAME_INFO_S *pstFrameInfo, VIM_S8* s8FileName, VIM_U32 Dynamic)
{
	char filename[256] = {0};
	VIM_U32 ret  = 0;
	int i  = 0;
	static VIM_U64 FileSize = 0; 
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;
	int fd;
	
	char* mbus_formats[] = {
		"YUV422S",
		"YUV422S10",
		"YUV420S",
		"YUV420S10",
		"YUV422P",
		"YUV422P10",
		"YUV420P",
		"YUV420P10",
		"RGB601",
		"RGB601P10",
		"RGB709",
		"RGB709P10",
	};	
	
	
	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(pstFrameInfo->enPixfmt, &enFormat, &enBits);
	sprintf(filename, "%s/test-%dx%d-%s.bin",
								s8FileName,pstFrameInfo->u16Width,pstFrameInfo->u16Height,
								mbus_formats[enFormat * 2 + enBits]);

				
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		printf("Unable to open test pattern file '%s': %s.\n", filename,
			strerror(errno));
		return -errno;
	}
	if(Dynamic == VIM_TRUE)
	{
		FileSize = lseek(fd,0,SEEK_END);
		lseek(fd,Sum,SEEK_SET);
	}	
	else
		Sum = 0;
	
	for(i = 0; i < 2 && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
	{
		if(aaaa == 0)
		{

			if (mPattern[u32Chn][i] == NULL)
				mPattern[u32Chn][i] = malloc(pstFrameInfo->u32ByteUsedStd[i]);




			if (mPattern[u32Chn][i] == NULL)
			{
				SAMPLE_PRT("chn %d alloc error %d\n", u32Chn,pstFrameInfo->u32ByteUsedStd[i]);
				return -ENOMEM;
			}

        	memset(mPattern[u32Chn][i], 0, pstFrameInfo->u32ByteUsedStd[i]);
			ret = read(fd, mPattern[u32Chn][i], pstFrameInfo->u32ByteUsedStd[i]);
			if (ret != pstFrameInfo->u32ByteUsedStd[i])
			{
				printf("chn %d read frame error %d/%d\n", u32Chn,ret,pstFrameInfo->u32ByteUsedStd[i]);
				close(fd);
				fd = 0;
				return -EINVAL;
			}
			Sum = Sum + pstFrameInfo->u32ByteUsedStd[i];
		}
	}
	if(FileSize <= Sum)
	{
		Sum = 0;
	}


	Sum = 0;
	close(fd);
    //goto retry;

	if (mPattern[u32Chn][0] != NULL)
	{
		for(i = 0; i < 2 && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		{
			if (mPattern[u32Chn][i] != NULL){

				printf("function: %s line:%d from file read image ok.\n " , __func__, __LINE__);
				memcpy(pstFrameInfo->pVirAddrStd[i], mPattern[u32Chn][i], pstFrameInfo->u32ByteUsedStd[i]);
			}
			else
				SAMPLE_PRT("chn %d read frame null error %d\n", u32Chn,pstFrameInfo->u32ByteUsedStd[i]);
			
			
		}
		return 0;
	}


	return 0;
}  

int SotpOut = 0;

static void* Tools_thread_loop(void *parg)
{
    ROT_TEST_THREAD_CTX_S *pstThreadCtx = (ROT_TEST_THREAD_CTX_S*)parg;

    prctl(PR_SET_NAME, (unsigned long)pstThreadCtx->pThreadName, 0, 0, 0);
    pstThreadCtx->bRun = VIM_TRUE;
    VIM_U32 i = 0;
    do
    {
		pthread_mutex_lock(&pstThreadCtx->mutex); 
        VIM_S32 s32_ret;
		//i++;
        s32_ret = pstThreadCtx->entryFunc(pstThreadCtx);
        if (s32_ret == 0)
        {
            printf("function: %s line:%d ret is 0.\n " , __func__, __LINE__);
            break;
        }
		//printf("#####Tools_thread_loop Stop SotpOut1 = %d\n ",SotpOut);
        
        if (((0 == s32_ret) || (VIM_FALSE == pstThreadCtx->bRun) || (i == pstThreadCtx->u32PrintNum)) \
		 	 && ((pstThreadCtx->u32PrintNum != 0) || (SotpOut == 1)))
        {
			//printf("#####Tools_thread_loop Stop SotpOut2 = %d\n ",SotpOut);
            pstThreadCtx->bRun = VIM_FALSE;
			SotpOut = 0;
			SotpOut = 0;
            pthread_mutex_unlock(&pstThreadCtx->mutex);
			//printf("#####Tools_thread_loop Stop SotpOut3 = %d\n ",SotpOut);
            break;
        }
        pthread_mutex_unlock(&pstThreadCtx->mutex);
		//printf("pstThreadCtx->u32PrintNum = %d\n",pstThreadCtx->u32PrintNum);
		//usleep(100);
		
    } while(1);
    
    return NULL;
}

int Frame = 0;
int fps   = 0;


VIM_U32 printNum1 = 0;
static int Thread_CaseOutputLoop(void *parg)
{
    VIM_S32 s32Ret = 0 ;
	ROT_TEST_THREAD_CTX_S *pstThreadCtx = NULL;
	VIM_VPSS_FRAME_INFO_S pstFrameInfo;
	VIM_U32 *u32ChId = NULL;
	VIM_U32 u32Chn = 2;  
#ifdef FPS
	struct timeval start, end;
#endif

	pstThreadCtx = (ROT_TEST_THREAD_CTX_S*)parg;
	printNum1 ++;
	if (pstThreadCtx == NULL)
	{
		printf("function: %s :%s exit because parg is null\n", __func__,  pstThreadCtx->pThreadName);
		pstThreadCtx->enState = 0;
		return 0;
	}	
	if(printNum1 > pstThreadCtx->u32PrintNum)
		pstThreadCtx->bRun =VIM_FALSE;
	
	//u32Chn = u8Indx[1];
				
	u32ChId = pstThreadCtx->pUserData;

	u32Chn = *u32ChId;
	if (pstThreadCtx->bRun == VIM_TRUE)
	{		
		
		/////////////////////
		
		//sleep(1);
		
		memset( &pstFrameInfo, 0, sizeof(pstFrameInfo));

		// if(pstThreadCtx->s8FileName != NULL)
		usleep(3000*1000);
		// else
#ifdef FPS	
		// if(Frame == 0)
		// {
		// 	gettimeofday(&start, NULL);
		// }


		// s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32Chn);
		// if (s32Ret != 0)
		// {
		// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
		// 	return 1;
		// }

		// VIM_MPI_ROTATION_SendFrame_Prepare(u32Chn,&stFrameInfo1);
		// if (s32Ret != 0)
		// {
		// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame_Prepare: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
		// 	return 1;
		// }
		// if(stFrameInfo1.enInputBits == 0)
		// 	stFrameInfo1.enInputBits	= 1;
		// else
		// 	stFrameInfo1.enInputBits	= 0; 

        // if(pstThreadCtx -> s8FileName !=NULL)
        // {
		// 	s32Ret = Tools_GetTestFrameFromFile(u32Chn, &stFrameInfo1, pstThreadCtx->s8FileName, 1);
		// 	if (s32Ret != 0)
        //     {
        //         SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n", pstThreadCtx->pThreadName, u32Chn,  "error");
        //         return 1;
        //     }
        // }

		// s32Ret = VIM_MPI_ROTATION_SendFrame(u32Chn,&stFrameInfo1);
		// if (s32Ret != 0)
		// {
		// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
		// 	return 1;
		// }
		// //sleep(1);

		// s32Ret = VIM_MPI_ROTATION_SendFrame_Release(u32Chn,&stFrameInfo1,50);
		// if (s32Ret != 0)
		// {
		// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame_Release: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
		// 	return 1;
		// }
#endif
	

		s32Ret = VIM_MPI_ROTATION_GetFrame(u32Chn,&pstFrameInfo,3000);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_GetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			printf("pstThreadCtx->u32PrintNum = %d\n",pstThreadCtx->u32PrintNum);
			pstThreadCtx->u32PrintNum++;
			return 1;
		}
		if(pstThreadCtx -> s8FileName !=NULL)
        {
			//printf(" OUT pstThreadCtx->s8FileName : %s\n",pstThreadCtx->s8FileName);
            s32Ret =  Tools_SaveTestFrameToFile(u32Chn, pstThreadCtx->s8FileName, &pstFrameInfo, 0);
            if (s32Ret != 0)
            {
                SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n", pstThreadCtx->pThreadName, u32Chn,  "error");
                return 1;
            }
		}

		s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32Chn);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
		Frame ++;

#ifdef FPS
		// Frame ++;
		// if(Frame == 2000)
		// {
		// 	gettimeofday(&end, NULL);
		// 	fps = (Frame*1000*1000)/((end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec));
		// 	printf("send frame time: %ldus\n", (end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec));
		// 	printf("ROT fps = %d\n",fps);
		// 	Frame = 0;
			
		// }
#endif
        
	}
	else /*thread will be to exit*/
	{
		return 0;
	}
	
	return 1;
}

static int Thread_IPCCaseOutputLoop(void *parg)
{
    VIM_S32 s32Ret = 0 ;
	ROT_TEST_THREAD_CTX_S *pstThreadCtx = NULL;
	VIM_VPSS_FRAME_INFO_S pstFrameInfo;
	VIM_U32 *u32ChId = NULL;
	VIM_U32 u32Chn = 2;  


	pstThreadCtx = (ROT_TEST_THREAD_CTX_S*)parg;

	if (pstThreadCtx == NULL)
	{
		printf("function: %s :%s exit because parg is null\n", __func__,  pstThreadCtx->pThreadName);
		pstThreadCtx->enState = 0;
		return 0;
	}
	

	//u32Chn = u8Indx[1];
				
	u32ChId = pstThreadCtx->pUserData;

	u32Chn = *u32ChId;
	if (pstThreadCtx->bRun == VIM_TRUE)
	{		
		
		memset( &pstFrameInfo, 0, sizeof(pstFrameInfo));

		s32Ret = VIM_MPI_ROTATION_GetFrame(u32Chn,&pstFrameInfo,3000);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_GetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			printf("pstThreadCtx->u32PrintNum = %d\n",pstThreadCtx->u32PrintNum);
			pstThreadCtx->u32PrintNum++;
			return 1;
		}
		if(pstThreadCtx -> s8FileName !=NULL)
        {
			//printf(" OUT pstThreadCtx->s8FileName : %s\n",pstThreadCtx->s8FileName);
            s32Ret =  Tools_SaveTestFrameToFile(u32Chn, pstThreadCtx->s8FileName, &pstFrameInfo, 0);
            if (s32Ret != 0)
            {
                SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n", pstThreadCtx->pThreadName, u32Chn,  "error");
                return 1;
            }
		}

		s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32Chn);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
        
	}
	else /*thread will be to exit*/
	{
		return 0;
	}
	
	return 1;
}

VIM_U32 printNum = 0;

static int Thread_CaseInputLoop(void *parg)
{
	ROT_TEST_THREAD_CTX_S *pstThreadCtx = (ROT_TEST_THREAD_CTX_S*)parg;	
	VIM_VPSS_FRAME_INFO_S stFrameInfo1 = {0};
	VIM_U32 *u32ChId = NULL;
	VIM_U32 u32Chn = 0;
	VIM_S32 s32Ret = 0 ;

	pstThreadCtx = (ROT_TEST_THREAD_CTX_S*)parg;
	if (pstThreadCtx == NULL)
	{
		printf("%s exit because parg is nil\n",  pstThreadCtx->pThreadName);
		pstThreadCtx->enState = 0;
		return 0;
	}
	u32ChId = pstThreadCtx->pUserData;
	u32Chn = *u32ChId;
	printNum ++;
	if(printNum > pstThreadCtx->u32PrintNum)
		pstThreadCtx->bRun =VIM_FALSE;
	
	if (pstThreadCtx->bRun == VIM_TRUE)
	{		
		//sleep(1);
		
		// memset( &stFrameInfo1, 0, sizeof(stFrameInfo1));
		
		// if(pstThreadCtx->s8FileName != NULL)
		// 	usleep(1000*1000);
		usleep(3000*1000);
		

		VIM_MPI_ROTATION_EnRotChn(u32Chn); 
		//printf("VIM_MPI_ROTATION_EnableROT u32Chn = %d\n",u32Chn);
		/*s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32Chn);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
		*/
		VIM_MPI_ROTATION_SendFrame_Prepare(u32Chn,&stFrameInfo1);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame_Prepare: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
		//printf("stFrameInfo1.pVirAddrStd[0] = 0x%x\n",stFrameInfo1.pVirAddrStd[0]);
		//printf("stFrameInfo1.pVirAddrStd[1] = 0x%x\n",stFrameInfo1.pVirAddrStd[1]);
		//printf("stFrameInfo1.enInputBits = 0x%x\n",stFrameInfo1.enInputBits);

        if(pstThreadCtx -> s8FileName !=NULL)
        {
			printf(" IN pstThreadCtx->s8FileName : %s\n",pstThreadCtx->s8FileName);
			s32Ret = Tools_GetTestFrameFromFile(u32Chn, &stFrameInfo1, pstThreadCtx->s8FileName, 0);
			if (s32Ret != 0)
            {
                SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n", pstThreadCtx->pThreadName, u32Chn,  "error");
                return 1;
            }
        }
		
		// usleep(5000*1000);
		// if (s32Ret != 0)
		// {

		// 	SAMPLE_PRT("chn %d Tools_GetTestFrameFromFile: %s:%d...\n",u32Chn, "error",s32Ret);
		// 	return 1;
		// }
		// printf("Tools_GetTestFrameFromFile sucess\n");

		// if(u32Chn == 0)
		// {
		// 		// pstFrameInfo.u32ByteUsedStd[0] = 2073600;
		// 		// pstFrameInfo.u32ByteUsedStd[1] = 1036800;
		// 		file = fopen("/mnt/nfs/cum/23.bin", "ab+");
		// 		printf("#########Tools_SaveTestFrameToFile filename = %s\n","23.bin");
		// }
		// // if(u32Chn == 1)
		// // {
		// // 		file = fopen("/mnt/sd/ipp/case0/24.bin", "ab+");
		// // 		printf("#########Tools_SaveTestFrameToFile filename = %s\n","24.bin");
		// // }
		
		// if (file != NULL) 
		// {	
		// 		//for(i = 0; i < 2 && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		// 		for(int i = 0; i < 2; i++)
		// 		{
		// 			if(stFrameInfo1.pVirAddrStd[0] == 0)
		// 			{
		// 				break;
		// 			}
		// 			//if (i ==  0 && EnDrawNo){
		// 			//	char str[50];
		// 			//	sprintf(str, "DST:%8d", pstFrameInfo->u16FrameNo);  
		// 			//	Tools_DrawYUVString(10, 32, pstFrameInfo->u16Width, pstFrameInfo->u16Height, str, pstFrameInfo->pVirAddrStd[0]) ;
		// 			//}
		// 			s32Ret = fwrite( stFrameInfo1.pVirAddrStd[i], 1, stFrameInfo1.u32ByteUsedStd[i], file);
		// 			if (s32Ret != 1)
		// 			{
		// 				printf("chn %d write frame %d\n", u32Chn,s32Ret);
		// 			}
		// 		}
		// 		printf("########1\n");
		// 		fclose(file);
		// 		printf("########2\n");
		// }

		s32Ret = VIM_MPI_ROTATION_SendFrame(u32Chn,&stFrameInfo1);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
		printf("VIM_MPI_ROTATION_SendFrame succes \n");
		//sleep(1);

		s32Ret = VIM_MPI_ROTATION_SendFrame_Release(u32Chn,&stFrameInfo1,50);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_SendFrame_Release: %s:%d...\n", pstThreadCtx->pThreadName,  u32Chn, "error",s32Ret);
			return 1;
		}
		printf("VIM_MPI_ROTATION_SendFrame_Release succes \n");
		//sleep(1);
		// s32Ret = VIM_MPI_ROTATION_GetFrame(u32Chn,&pstFrameInfo,3000);
		// printf("#####lyh OUT pstFrameInfo->enInputFormat = %d\n",pstFrameInfo.enFormat );
		// printf("#####lyh OUT pstFrameInfo->enInputBits   = %d\n",pstFrameInfo.enBits   );
		// printf("#####lyh OUT pstFrameInfo->u16Width      = %d\n",pstFrameInfo.u16Width      );
		// printf("#####lyh OUT pstFrameInfo->u16Height     = %d\n",pstFrameInfo.u16Height     );
		// printf("#####lyh OUT pstFrameInfo->pPhyAddrStd[0] = = %lx\n",pstFrameInfo.pPhyAddrStd[0]);
		// printf("#####lyh OUT pstFrameInfo->pVirAddrStd[0] = = %lx\n",pstFrameInfo.pVirAddrStd[0]);
		// printf("#####lyh OUT pstFrameInfo->u32ByteUsedStd[0]= %lx\n",pstFrameInfo.u32ByteUsedStd[0]);
		// printf("#####lyh OUT pstFrameInfo->pPhyAddrStd[1] = = %lx\n",pstFrameInfo.pPhyAddrStd[1]);
		// printf("#####lyh OUT pstFrameInfo->pVirAddrStd[1] = = %lx\n",pstFrameInfo.pVirAddrStd[1]);
		// printf("#####lyh OUT pstFrameInfo->u32ByteUsedStd[1]= %lx\n",pstFrameInfo.u32ByteUsedStd[1]);
		// if(u32Chn == 0)
		// {
		// 		// pstFrameInfo.u32ByteUsedStd[0] = 2073600;
		// 		// pstFrameInfo.u32ByteUsedStd[1] = 1036800;
		// 		file = fopen("23.bin", "ab+");
		// 		printf("#########Tools_SaveTestFrameToFile filename = %s\n","23.bin");
		// }
		// if(u32Chn == 1)
		// {
		// 		file = fopen("24.bin", "ab+");
		// 		printf("#########Tools_SaveTestFrameToFile filename = %s\n","24.bin");
		// }
		
		// if (file != NULL) 
		// {	
		// 		//for(i = 0; i < 2 && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		// 		for(int i = 0; i < 2; i++)
		// 		{
		// 			if(pstFrameInfo.pVirAddrStd[0] == 0)
		// 			{
		// 				break;
		// 			}
		// 			//if (i ==  0 && EnDrawNo){
		// 			//	char str[50];
		// 			//	sprintf(str, "DST:%8d", pstFrameInfo->u16FrameNo);  
		// 			//	Tools_DrawYUVString(10, 32, pstFrameInfo->u16Width, pstFrameInfo->u16Height, str, pstFrameInfo->pVirAddrStd[0]) ;
		// 			//}
		// 			s32Ret = fwrite( pstFrameInfo.pVirAddrStd[i], 1, pstFrameInfo.u32ByteUsedStd[i], file);
		// 			if (s32Ret != 1)
		// 			{
		// 				printf("chn %d write frame %d\n", u32Chn,s32Ret);
		// 			}
		// 		}
		// 		printf("########1\n");
		// 		fclose(file);
		// 		printf("########2\n");
		// }
		
		//u8Indx[1] 	 = u32Chn;
			
	}
	else /*thread will be to exit*/
	{
	    printf("function: %s line:%d thread will be to exit.\n " , __func__, __LINE__);

		return 0;
	}
	
	return 1;
}

VIM_S32 Tools_StartThread(THREAD_HANDLE_S *handle, FN_THREAD_LOOP entryFunc,
                    const char *pThreadName, void *pUserData, int PrintNum, VIM_S8* s8FileName)
{
    VIM_S32 s32_ret;
    ROT_TEST_THREAD_CTX_S *pstThreadCtx;
    ROT_TEST_RESULT_S *pstResult;
	struct sched_param s_parm;
	pthread_attr_t  pthread_pro;

    pstThreadCtx = malloc(sizeof(ROT_TEST_THREAD_CTX_S));
    if(NULL == pstThreadCtx)
    {
        return VIM_FAILURE;
    }
	memset(pstThreadCtx, 0, sizeof(ROT_TEST_THREAD_CTX_S));
	
	pstResult = malloc(sizeof(ROT_TEST_RESULT_S));
    if(NULL == pstResult)
    {
        return VIM_FAILURE;
    }
	memset(pstResult, 0, sizeof(ROT_TEST_RESULT_S));
	
    pthread_mutex_init(&pstThreadCtx->mutex, NULL);
    pstThreadCtx->bRun  = VIM_FALSE;
    pstThreadCtx->entryFunc = entryFunc;
    pstThreadCtx->pUserData = pUserData;
    pstThreadCtx->enState =  ROT_TEST_THREAD_INITIALIZED;
    pstThreadCtx->pstResult = pstResult;
    pstThreadCtx->pThreadName = pThreadName ? strdup(pThreadName) : NULL;
	pstThreadCtx->u32PrintNum = PrintNum;
	pstThreadCtx->s8FileName  = s8FileName;
    
	pthread_attr_init(&pthread_pro);
  	pthread_attr_setschedpolicy(&pthread_pro, SCHED_RR);
  	s_parm.sched_priority = sched_get_priority_max(SCHED_RR);
	s_parm.__sched_priority = 3 - (int)pstThreadCtx->pUserData;
 	pthread_attr_setschedparam(&pthread_pro, &s_parm);

    s32_ret = pthread_create(&pstThreadCtx->thread, &pthread_pro, Tools_thread_loop, pstThreadCtx);
    if (VIM_SUCCESS != s32_ret)
    {
        SAMPLE_PRT("create tools_start_thread thread failed, %s\n", strerror(s32_ret));
        pthread_mutex_destroy(&pstThreadCtx->mutex);
        if (NULL != pstThreadCtx->pThreadName)
        {
            free(pstThreadCtx->pThreadName);
        }
        free(pstThreadCtx);
        return VIM_FAILURE;
    }

    *handle = (THREAD_HANDLE_S)pstThreadCtx;
    return VIM_SUCCESS;
}

static VIM_S32 __StartThreadForChnIn(THREAD_HANDLE_S *hThread, VIM_U32 *u32Grp, int PrintNum, VIM_S8* s8FileName)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnIn = *u32Grp;


	sprintf(s8ThreadName, "ROT_CHN%d-IN", *u32Grp);
	s32Ret = Tools_StartThread( &hThread[u32ChnIn], Thread_CaseInputLoop, s8ThreadName, u32Grp, PrintNum, s8FileName);
	if (s32Ret != VIM_SUCCESS)
	{
		printf("function: %s line:%d index:%d  exit2..\n " , __func__, __LINE__,u32ChnIn);
		goto end;
	}

	return 0;	
end:
	
	return -1;
}

static VIM_S32 __StartThreadForChaOut(THREAD_HANDLE_S *hThread, VIM_U32 *grp, int PrintNum, VIM_S8* s8FileName)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnOut = 0;

    u32ChnOut = *grp;


	sprintf(s8ThreadName, "ROT_CHN-%d-OUT", u32ChnOut);
	s32Ret = Tools_StartThread( &hThread[u32ChnOut], Thread_CaseOutputLoop, s8ThreadName, grp, PrintNum, s8FileName);
	if (s32Ret != VIM_SUCCESS)
		goto end;

	return 0;
	
end:
	
	return -1;
}

static VIM_S32 __StartThreadForIPCOut(THREAD_HANDLE_S *hThread, VIM_U32 *grp, int PrintNum, VIM_S8* s8FileName)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnOut = 0;

    u32ChnOut = *grp;


	sprintf(s8ThreadName, "ROT_CHN-%d-OUT", u32ChnOut);
	s32Ret = Tools_StartThread( &hThread[u32ChnOut], Thread_IPCCaseOutputLoop, s8ThreadName, grp, PrintNum, s8FileName);
	if (s32Ret != VIM_SUCCESS)
		goto end;

	return 0;
	
end:
	
	return -1;
}

VIM_S32 SAMPLE_COMMON_ROTATION_CalcFrameSinglePlaneSize(VIM_U8 u8Plane, 
									VIM_IMG_FORMAT_E  enInputFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U16 u16Width,
									VIM_U16 u16Height)
{
	VIM_U32 u32Size = 0;
	
	if (u8Plane == 0)
	{
		if (enInputFormat != E_IMG_FORMAT_422I)
		{
			switch((VIM_U32)enInputBits)
			{
				case E_IMG_BITS_10:
					u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16) * u16Height;
					printf("wbytes %x %d\n", ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16), ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16));
					break;
				case E_IMG_BITS_8:
					u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 8), 16) * u16Height;
					break;
			}
		}
		else
		{
			u32Size = 8 * DIV_ROUND_UP (u16Width * u16Height * 2, 8);
		}		
	}
	else
	{
		switch(enInputFormat) {
			case E_IMG_FORMAT_422SP:
				if (u8Plane > 1) {
					u32Size = 0;
					break;
				}
				switch((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16) * (u16Height);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 8), 16) * (u16Height);
						break;
				}
				break;
			case E_IMG_FORMAT_420SP:
				if (u8Plane > 1) {
					u32Size = 0;
					break;
				}
				switch((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16) * (u16Height / 2);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 8), 16) * (u16Height / 2);
						break;
				}					
				break;
			case E_IMG_FORMAT_422P:			
				switch((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 12), 16) * (u16Height);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 16), 16) * (u16Height);
						break;
				}					
				break;
			case E_IMG_FORMAT_420P:			
				switch((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 12), 16) * (u16Height / 2);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 16), 16) * (u16Height / 2);
						break;
				}
				break;
			case E_IMG_FORMAT_601P:
			case E_IMG_FORMAT_709P:
				switch((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16) * u16Height;
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 8), 16) * u16Height;
						break;
				}
				break;
			default :
				u32Size = 0;
				break;
		}
	}

	return u32Size;
}

VIM_S32 SAMPLE_ROTATION_SetInUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_ROFORMAT_S *RotAttr,VIM_U32 u32Chn)
{
    VIM_U32 i = 0;
	VIM_S32 s32Ret = 0;
	VIM_U32 u32Plane = 0;
	VIM_S32 s32StdBytes[2] = {0};
	VIM_S32 u32InBit = 0;

	if(RotAttr->u32Inbit == ROTATION_PIX_BIT_10)
	{
		u32InBit = 1;
	}
    else
	{
		u32InBit = 0;
	}

	for(i = 0; i < 2; i++ )
	{
		s32StdBytes[i] = SAMPLE_COMMON_ROTATION_CalcFrameSinglePlaneSize(i,
															E_IMG_FORMAT_420SP,
															u32InBit,
															RotAttr->u32Width,
															RotAttr->u32Height);
			printf("yuv %d - %d -%x\n", i, s32StdBytes[i], s32StdBytes[i]);
	}


	if(s32VbPoolIn == 0)
	{
		sprintf(VbPoolInName,"InRotChn%d",u32Chn);
		printf("VbPoolInName = %s\n",VbPoolInName);
		s32VbPoolIn = VIM_MPI_VB_CreatePool(s32StdBytes[0],12, VbPoolInName);
		if(s32VbPoolIn == VB_INVALID_POOLID ) {
			printf("VIM_MPI_VB_CreatePool failed!\n");
			goto out;
		}
		printf("s32VbPoolIn = %d\n",s32VbPoolIn);
		s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolIn);
		if(s32Ret) {
			printf("VIM_MPI_VB_MmapPool failed!\n");
			VIM_MPI_VB_DestroyPool(s32VbPoolIn);
			goto out;
		}
	}

	for (u32Plane = 0; u32Plane < 2; u32Plane++ )
	{
		if(!s32StdBytes[u32Plane])
			continue;
		

		s32VbInBlk = VIM_MPI_VB_GetBlock(s32VbPoolIn, u32Vbsize, NULL);
		printf("######s32Vbinblk=%d\n",s32VbInBlk);
        if (s32VbInBlk == VB_INVALID_HANDLE){
            printf("VIM_MPI_VB_GetBlock failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn);
            goto out;
        }

        pstUserBuff->pPhyAddrStd[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbInBlk);
        if (!pstUserBuff->pPhyAddrStd[u32Plane]) {
            printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn);
            goto out;
        }

        s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolIn,pstUserBuff->pPhyAddrStd[u32Plane], (VIM_VOID*)&(pstUserBuff->pVirAddrStd[u32Plane]));
        if(s32Ret) {
            printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn);
            goto out;
		}
        
		pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];	
		if (pstUserBuff->pVirAddrStd[u32Plane] == NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);

		// pstUserBuff->pVirAddrStd[u32Plane] = DO_HEAP_Malloc(s32StdBytes[u32Plane]);
		
		// if (pstUserBuff->pVirAddrStd[u32Plane] == NULL)
		// {
		// 	printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
		// 	return -1;
		// }
			
		// pstUserBuff->pPhyAddrStd[u32Plane] = (VIM_U32)DO_HEAP_GetPhys((size_t)(pstUserBuff->pVirAddrStd[u32Plane]));
		// pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];
	}
out:
	return s32Ret;
}

VIM_S32 SAMPLE_ROTATION_SetOutUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_ROFORMAT_S *RotAttr,VIM_U32 u32Chn)
{
    VIM_U32 i = 0;
	VIM_S32 s32Ret = 0;
	VIM_U32 u32Plane = 0;
	VIM_S32 s32StdBytes[2] = {0};
	VIM_S32 u32OutBit = 0;

	if(RotAttr->u32Outbit == ROTATION_PIX_BIT_10)
	{
        u32OutBit = 1;
	}
    else
	{
        u32OutBit = 0;
	}


	for(i = 0; i < 2; i++ )
	{
		s32StdBytes[i] = SAMPLE_COMMON_ROTATION_CalcFrameSinglePlaneSize(i,
															E_IMG_FORMAT_420SP,
															u32OutBit,
															RotAttr->u32Width,
															RotAttr->u32Height);
			printf("yuv %d - %d -%x\n", i, s32StdBytes[i], s32StdBytes[i]);
	}

	if(s32VbPoolOut == 0)
	{
		sprintf(VbPoolOutName,"OutRotChn%d",u32Chn);
		printf("VbPoolOutName = %s\n",VbPoolOutName);
		s32VbPoolOut = VIM_MPI_VB_CreatePool(s32StdBytes[0]+16,13, VbPoolOutName);
		if(s32VbPoolOut == VB_INVALID_POOLID ) {
			printf("VIM_MPI_VB_CreatePool failed!\n");
			goto out;
		}
		printf("s32VbPoolOut = %d\n",s32VbPoolOut);
		s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolOut);
		if(s32Ret) {
			printf("VIM_MPI_VB_MmapPool failed! s32Ret = 0x%x\n",s32Ret);
			VIM_MPI_VB_DestroyPool(s32VbPoolOut);
			goto out;
		}
	}

	for (u32Plane = 0; u32Plane < 2; u32Plane++ )
	{
		if(!s32StdBytes[u32Plane])
			continue;
		
		// pstUserBuff->pVirAddrStd[u32Plane] = DO_HEAP_Malloc(s32StdBytes[u32Plane]);
		
		// if (pstUserBuff->pVirAddrStd[u32Plane] == NULL)
		// {
		// 	printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
		// 	return -1;
		// }
			
		// pstUserBuff->pPhyAddrStd[u32Plane] = (VIM_U32)DO_HEAP_GetPhys((size_t)(pstUserBuff->pVirAddrStd[u32Plane]));
		// pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];

		s32VbOutBlk = VIM_MPI_VB_GetBlock(s32VbPoolOut, u32Vbsize, NULL);
        if (s32VbOutBlk == VB_INVALID_HANDLE){
            printf("VIM_MPI_VB_GetBlock failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut);
            goto out;
        }

        pstUserBuff->pPhyAddrStd[u32Plane]= VIM_MPI_VB_Handle2PhysAddr(s32VbOutBlk);
        if (!pstUserBuff->pPhyAddrStd[u32Plane]) {
            printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut);
            goto out;
        }

        s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolOut,pstUserBuff->pPhyAddrStd[u32Plane], (VIM_VOID*)&(pstUserBuff->pVirAddrStd[u32Plane]));
        if(s32Ret) {
            printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut);
            goto out;
        }
		
		pstUserBuff->u32StdSize[u32Plane]  = s32StdBytes[u32Plane];	
		if (pstUserBuff->pVirAddrStd[u32Plane] == NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);

	
	}
out:
	return s32Ret;
}


VIM_S32 SAMPLE_COMMON_ROTATION_StopChn(VIM_U32 u32ChnId)
{
	VIM_S32 s32Ret      = 0;

	// VIM_MPI_ROTATION_GetRoCtrl(u32ChnId, &RoForMat1);

	// RoForMat1.u32Width = 720;
	// RoForMat1.u32Height = 576;
    s32Ret = VIM_MPI_ROTATION_StopGrpId(u32ChnId);
    CHECK_ROTATION_RET_RETURN(s32Ret);

    s32Ret = VIM_MPI_ROTATION_DisableROT(u32ChnId);
    CHECK_ROTATION_RET_RETURN(s32Ret);

	// s32Ret = VIM_MPI_ROTATION_SetRoCtrl(u32ChnId, &RoForMat1);
	// CHECK_ROTATION_RET_RETURN(s32Ret);

	// s32Ret = VIM_MPI_ROTATION_EnableROT(u32ChnId); 
	// CHECK_ROTATION_RET_RETURN(s32Ret);
	
	// s32Ret = VIM_MPI_ROTATION_StartGrpId(u32ChnId); 
	// CHECK_ROTATION_RET_RETURN(s32Ret);

    return 0;

}

VIM_S32 SAMPLE_COMMON_ROTATION_StartChn(VIM_U32 u32ChnId, VIM_ROFORMAT_S *RoCtrl)
{
	VIM_S32 s32Ret  = 0;

    s32Ret = VIM_MPI_ROTATION_EnableDev();
	CHECK_ROTATION_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_ROTATION_SetRoCtrl(u32ChnId, RoCtrl);
	CHECK_ROTATION_RET_RETURN(s32Ret);

	//VIM_MPI_ROTATION_EnableROT(u32ChnId); 

	//VIM_MPI_ROTATION_StartGrpId(u32ChnId);
	//printf("###In u32Chn : %d\n",u32Chn);
	

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_ROTATION_Send(VIM_U32 u32ChnId, VIM_U32 u32PrintNum, VIM_S8* s8FileName)
{
	VIM_S32 s32Ret   = 0;
	u32SamChnIdIn[u32ChnId] = u32ChnId;
    if(s8Sample_num == VIM_FALSE)
    {   

	    hThread = malloc( MAX_VPSS_VIDEO_POINT * sizeof(hThread));
	    if (!hThread)
	    {

	    	SAMPLE_PRT("Init chn thread error.");
	    	return 0;
	    }

	    memset(hThread, 0, MAX_VPSS_VIDEO_POINT * sizeof(hThread));
        s8Sample_num = VIM_TRUE;
    }

	// s32Ret = VIM_MPI_ROTATION_GetRoCtrl(u32ChnId, &Sam_StGrpAttr);
    // CHECK_ROTATION_RET_RETURN(s32Ret);
	// s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32ChnId);
	// if (s32Ret != 0)
	// {
	// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", "IN_ROT",  u32ChnId, "error",s32Ret);
	// 	return 1;
	// }
	// s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32ChnId);
	// if (s32Ret != 0)
	// {
	// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", "IN_ROT",  u32ChnId, "error",s32Ret);
	// 	return 1;
	// }
	// s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32ChnId);
	// if (s32Ret != 0)
	// {
	// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", "IN_ROT",  u32ChnId, "error",s32Ret);
	// 	return 1;
	// }
	// s32Ret = VIM_MPI_ROTATION_PressInGetFrame(u32ChnId);
	// if (s32Ret != 0)
	// {
	// 	SAMPLE_PRT("[%s] chn %d VIM_MPI_ROTATION_PressInGetFrame: %s:%d...\n", "IN_ROT",  u32ChnId, "error",s32Ret);
	// 	return 1;
	// }
	
	s32Ret = __StartThreadForChnIn(hThread, &u32SamChnIdIn[u32ChnId], u32PrintNum, s8FileName);
    CHECK_ROTATION_RET_RETURN(s32Ret);
	usleep(100*1000);
    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_ROTATION_SetInUserBuff(VIM_U32 u32ChnId, VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_U32 u32BuffOfset)
{
    VIM_S32 s32Ret = 0;

    s32Ret = VIM_MPI_ROTATION_SetChnInUserBufs(u32ChnId, u32BuffOfset, pstUserBuff);
    CHECK_ROTATION_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_ROTATION_SetOutUserBuff(VIM_U32 u32ChnId, VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_U32 u32BuffOfset)
{
    VIM_S32 s32Ret = 0;

    s32Ret = VIM_MPI_ROTATION_SetChnOutUserBufs(u32ChnId,u32BuffOfset,pstUserBuff);
    CHECK_ROTATION_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_ROTATION_Capture( VIM_U32 u32ChnId, VIM_U32 u32CaptureNum, VIM_S8* s8SavePush)
{

    VIM_S32 s32Ret      = 0;
    u32SamChnIdOut[u32ChnId] = u32ChnId;
    if(s8Sample_num == VIM_FALSE)
    {    
	    hThread = malloc( MAX_VPSS_VIDEO_POINT * sizeof(hThread));
	    if (!hThread)
	    {
	    	SAMPLE_PRT("Init chn thread error.");
	    	return 0;
	    }
	    memset(hThread, 0, MAX_VPSS_VIDEO_POINT * sizeof(hThread));
        s8Sample_num = VIM_TRUE;
    }
    
    s32Ret = __StartThreadForChaOut(hThread, &u32SamChnIdOut[u32ChnId], u32CaptureNum, s8SavePush);
    CHECK_ROTATION_RET_RETURN(s32Ret); 
	usleep(100*1000);
    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_ROTATION_IPCCapture( VIM_U32 u32ChnId, VIM_U32 u32CaptureNum, VIM_S8* s8SavePush)
{

    VIM_S32 s32Ret      = 0;
    u32SamChnIdOut[u32ChnId] = u32ChnId;
    if(s8Sample_num == VIM_FALSE)
    {    
	    hThread = malloc( MAX_VPSS_VIDEO_POINT * sizeof(hThread));
	    if (!hThread)
	    {
	    	SAMPLE_PRT("Init chn thread error.");
	    	return 0;
	    }
	    memset(hThread, 0, MAX_VPSS_VIDEO_POINT * sizeof(hThread));
        s8Sample_num = VIM_TRUE;
    }
    
    s32Ret = __StartThreadForIPCOut(hThread, &u32SamChnIdOut[u32ChnId], u32CaptureNum, s8SavePush);
    CHECK_ROTATION_RET_RETURN(s32Ret); 
	usleep(100*1000);
    return VIM_SUCCESS;
}
