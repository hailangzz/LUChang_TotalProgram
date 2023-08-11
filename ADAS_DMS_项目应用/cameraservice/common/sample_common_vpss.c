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
#include "mpi_vpss.h"
#include "sample_common_vpss.h"


#include "mpi_vb.h"
#include "mpi_sys.h"
#include "vim_comm_vb.h"


#define HEAP_SIZE (1 * 512 * 1024 * 1024)	 //1GB
#define uint64_t unsigned long long
#define NSEC_PER_SEC 1000000000LL

#define THREAD_STATUS_RUN  1
#define THREAD_STATUS_STOP 0

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define ALIGN_UP(x, a) (((x) + (a) - 1) / (a) * (a))
#define ALIGN(x, a) (((x) + (a) - 1) / (a) * (a))

static int ipp_in[3] = {0, 0, 0};
VIM_BOOL s_bExit;
THREAD_HANDLE_S GrphThread[MAX_VPSS_GROUP];
THREAD_HANDLE_S ChnhThread[MAX_VPSS_GROUP][MAX_VPSS_CHN];
static 	void *mPattern[MAX_VPSS_VIDEO_POINT][VI_FRAME_STD_PLANE_MAX] = {NULL};

static VIM_U64  Sum = 0;
static VIM_BOOL s8Sample_num = VIM_FALSE;
static VIM_U32  u32ChnOutMask[MAX_VPSS_GROUP] = {0};
static VIM_S8   s8GroupMask[MAX_VPSS_GROUP]   = {0};
static VIM_U32  u32GrpIn[MAX_VPSS_INPUT]      = {0};
static VIM_U32  u32GrpOut[MAX_VPSS_CHN]       = {0};
VIM_BOOL VbInnum[3] = {VIM_FALSE, VIM_FALSE, VIM_FALSE};
VIM_BOOL VbOutnum[6] = {VIM_FALSE, VIM_FALSE, VIM_FALSE, VIM_FALSE, VIM_FALSE, VIM_FALSE};

VIM_S32 s32VbPoolIn[3];
VIM_S32	 s32VbInBlk  = 0;
VIM_CHAR VbPoolInName[20] = {0};

VIM_S32 s32VbPoolOut[6];
VIM_S32	 s32VbOutBlk  = 0;
VIM_CHAR VbPoolOutName[20] = {0};

VIM_S32 s32VbPoolOutCap[200][6];
VIM_S32	 s32VbOutBlkCap[200][3] ;
VIM_CHAR VbPoolOutNameCap[200][20];
VIM_S32 SAMPLE_VPSS_SetOutCaptureBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_CHN_CFG_ATTR_S *stChnAttr, VIM_U32 u32ChnId, VIM_U32 u32Index);

static VIM_S32 Tools_SaveTestFrameToFile(VIM_U32 u32Chn, const char *path,
                            VIM_VPSS_FRAME_INFO_S *pstFrameInfo, VIM_U16 EnDrawNo)
{
	char filename[256] = {0};
	FILE *file;
	int ret = 0;
	int i = 0;
	VIM_IMG_FORMAT_E enFormat;
	VIM_IMG_BITS_E enBits;

	static const char * const mbus_formats[] = {
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
		"YUV422I",
	};

    EnDrawNo = EnDrawNo;

	if (pstFrameInfo == NULL)
	{
		printf("%s pstFrameInfo is NULL\n", __func__);
		return -1;
	}

	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(pstFrameInfo->enPixfmt, &enFormat, &enBits);

	sprintf(filename, "%s/chn%d-%dx%d-%s-%s-all.bin", path, u32Chn,
								pstFrameInfo->u16Width,pstFrameInfo->u16Height,
								mbus_formats[enFormat * 2 + enBits],
								"UV");

	file = fopen(filename, "ab+");
	if (file != NULL) {
		for (i = 0; i < VI_FRAME_STD_PLANE_MAX
            && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		{

			ret = fwrite(pstFrameInfo->pVirAddrStd[i], pstFrameInfo->u32ByteUsedStd[i], 1, file);
			if (ret != 1)
			{
				SAMPLE_PRT("chn %d write frame error %d/%d\n",
                        u32Chn, ret, pstFrameInfo->u32ByteUsedStd[i]);
			}
		}

		//cframe
		if (pstFrameInfo->enCframe && pstFrameInfo->enLoss == VIM_FALSE)
		{
			for (i = 0; i < VI_FRAME_CFRAME_PLANE_MAX
                && pstFrameInfo->u32ByteUsedCfr[i] && pstFrameInfo->pVirAddrCfr[i]; i++)
			{
				ret = fwrite(pstFrameInfo->pVirAddrCfr[i],
                        pstFrameInfo->u32ByteUsedCfr[i], 1, file);
				if (ret != 1)
				{
					SAMPLE_PRT("chn %d write cframe error %d/%d\n",
                            u32Chn, ret, pstFrameInfo->u32ByteUsedCfr[i]);
				}
			}
		}

		fclose(file);
	}

	return 0;
}

static VIM_S32 Tools_GetTestFrameFromFile( VIM_U32 u32Chn, VIM_VPSS_FRAME_INFO_S *pstFrameInfo,
                    const char *s8FileName, VIM_U64 u64StartInputFrameNo, VIM_U16 EnDrawNo, VIM_U32 Dynamic)
{
	char filename[256] = {0};
	VIM_U32 ret = 0;
	int i = 0;
	static VIM_U64 FileSize;
	VIM_IMG_FORMAT_E enFormat;
	VIM_IMG_BITS_E enBits;
	int fd;

	static const char * const mbus_formats[] = {
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

    EnDrawNo = EnDrawNo;
    u64StartInputFrameNo = u64StartInputFrameNo;

	if (pstFrameInfo == NULL)
	{
		printf("%s pstFrameInfo is NULL\n", __func__);
		return -1;
	}

	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(pstFrameInfo->enPixfmt, &enFormat, &enBits);
	sprintf(filename, "%s/test-%dx%d-%s.bin", s8FileName, pstFrameInfo->u16Width,
                        pstFrameInfo->u16Height, mbus_formats[enFormat * 2 + enBits]);

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		printf("Unable to open test pattern file '%s': %s.\n", filename,
			strerror(errno));
		return -errno;
	}

	if (Dynamic == VIM_TRUE)
	{
        FileSize = lseek(fd, 0, SEEK_END);
        lseek(fd, Sum, SEEK_SET);
	}
	else
    {
        Sum = 0;
    }

	for (i = 0; i < VI_FRAME_STD_PLANE_MAX
        && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
	{

		if (mPattern[u32Chn][i] == NULL)
		{
            printf(" malloc file memry...\n");
			mPattern[u32Chn][i] = malloc(pstFrameInfo->u32ByteUsedStd[i]);
			if (mPattern[u32Chn][i] == NULL)
			{
				SAMPLE_PRT("chn %d alloc error %d\n", u32Chn, pstFrameInfo->u32ByteUsedStd[i]);
                close(fd);
				return -ENOMEM;
			}
		}

        memset(mPattern[u32Chn][i], 0, pstFrameInfo->u32ByteUsedStd[i]);

		ret = read(fd, mPattern[u32Chn][i], pstFrameInfo->u32ByteUsedStd[i]);
		if (ret != pstFrameInfo->u32ByteUsedStd[i])
		{
			SAMPLE_PRT("chn %d read frame error %d/%d\n", u32Chn, ret, pstFrameInfo->u32ByteUsedStd[i]);
			close(fd);
			fd = 0;
			return -EINVAL;
		}

		Sum = Sum + pstFrameInfo->u32ByteUsedStd[i];
	}

	if (FileSize <= Sum)
	{
        Sum = 0;
	}


	if (fd)
		close(fd);

	if (mPattern[u32Chn][0] != NULL)
	{
		for (i = 0; i < VI_FRAME_STD_PLANE_MAX
                && pstFrameInfo->u32ByteUsedStd[i] && pstFrameInfo->pVirAddrStd[i]; i++)
		{
			if (mPattern[u32Chn][i] != NULL) {

				printf("function: %s line:%d from file read image ok.\n" , __func__, __LINE__);
				memcpy(pstFrameInfo->pVirAddrStd[i],
                        mPattern[u32Chn][i], pstFrameInfo->u32ByteUsedStd[i]);

			}
			else
            {
				SAMPLE_PRT("chn %d read frame null error %d\n", u32Chn,pstFrameInfo->u32ByteUsedStd[i]);
            }
		}
		return 0;
	}

	return 0;
}

static void *Tools_thread_loop(void *parg)
{
    TEST_THREAD_CTX_S *pstThreadCtx = (TEST_THREAD_CTX_S *)parg;

    prctl(PR_SET_NAME, (unsigned long)pstThreadCtx->pThreadName, 0, 0, 0);
    pstThreadCtx->bRun = VIM_TRUE;
    VIM_U32 i = 0;

    do
    {
		pthread_mutex_lock(&pstThreadCtx->mutex);
        VIM_S32 s32_ret;
		i++;
        s32_ret = pstThreadCtx->entryFunc(pstThreadCtx);
        if (s32_ret == 0)
        {
            printf("function: %s line:%d ret is 0.\n ", __func__, __LINE__);
            pthread_mutex_unlock(&pstThreadCtx->mutex);
            break;
        }

        if (((0 == s32_ret) || (VIM_FALSE == pstThreadCtx->bRun)
            || (i == pstThreadCtx->u32PrintNum)) && (pstThreadCtx->u32PrintNum != 0))
        {
            pstThreadCtx->bRun = VIM_FALSE;
            pthread_mutex_unlock(&pstThreadCtx->mutex);
            break;
        }
        pthread_mutex_unlock(&pstThreadCtx->mutex);

    } while(1);

    return NULL;
}

static VIM_S32 Tools_StartThread(THREAD_HANDLE_S *handle, FN_THREAD_LOOP entryFunc,
                const char *pThreadName, VIM_VOID *pUserData, VIM_U32 PrintNum, const char *s8FileName, VIM_U8 u8Dynamic)
{
    VIM_S32 s32_ret;
    TEST_THREAD_CTX_S *pstThreadCtx;
    TEST_RESULT_S *pstResult;
	struct sched_param s_parm;
	pthread_attr_t  pthread_pro;

    pstThreadCtx = malloc(sizeof(TEST_THREAD_CTX_S));
    if (NULL == pstThreadCtx)
    {
        return VIM_FAILURE;
    }

	memset(pstThreadCtx, 0, sizeof(TEST_THREAD_CTX_S));

	pstResult = malloc(sizeof(TEST_RESULT_S));
    if (NULL == pstResult)
    {
        free(pstThreadCtx);
        return VIM_FAILURE;
    }

	memset(pstResult, 0, sizeof(TEST_RESULT_S));

    pthread_mutex_init(&pstThreadCtx->mutex, NULL);
    pstThreadCtx->bRun  = VIM_FALSE;
    pstThreadCtx->entryFunc = entryFunc;
    pstThreadCtx->pUserData = pUserData;
    pstThreadCtx->enState = TEST_THREAD_INITIALIZED;
    pstThreadCtx->pstResult = pstResult;
    pstThreadCtx->pThreadName = pThreadName ? strdup(pThreadName) : NULL;
	pstThreadCtx->u32PrintNum = PrintNum;
    pstThreadCtx->s8FileName  = s8FileName;
	pstThreadCtx->u8Dynamic  = u8Dynamic;
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

VIM_VPSS_FRAME_INFO_S *stFrameWaitSave = NULL;
VIM_U32 u32FrameCnt = 0;
static int Thread_CaseOutputContinousCapLoop(void *parg)
{
    VIM_S32 s32Ret = 0 ;
	TEST_THREAD_CTX_S *pstThreadCtx = NULL;
	VIM_U32 *u32Grp = NULL;
	VIM_VPSS_FRAME_INFO_S stFrameInfo;
	VIM_U32 u32Chn = 0;
	VIM_U32 u32GrpId = 0;
	VIM_U32 i = 0;
	VIM_IMG_FORMAT_E enFormat = E_IMG_FORMAT_422SP;
	VIM_IMG_BITS_E enBits = E_IMG_BITS_8;
	VIM_CHN_CFG_ATTR_S stChnAttr;

	if (s_bExit == VIM_TRUE)
	{
        printf("function: %s line:%d thread will be to exit.\n " , __func__, __LINE__);
		return 0;
	}

	pstThreadCtx = (TEST_THREAD_CTX_S *)parg;
	if (pstThreadCtx == NULL)
	{
		printf("function: %s :%s exit because parg is null\n", __func__, pstThreadCtx->pThreadName);
		pstThreadCtx->enState = 0;
		return 0;
	}

	u32Grp   = pstThreadCtx->pUserData;
	u32GrpId = (*u32Grp >> 16) & 0xffff;
	u32Chn   = (*u32Grp >> 0) & 0xffff;
	if (!stFrameWaitSave)
	{
		printf("stFrameWaitSave is NULL.\n");
		return 0;
	}

	if (pstThreadCtx->bRun == VIM_TRUE)
	{
		memset(&stFrameInfo, 0, sizeof(stFrameInfo));

		s32Ret = VIM_MPI_VPSS_GetFrame(u32GrpId, u32Chn, &stFrameInfo, 3000);
		if (s32Ret != 0)
		{
			printf("VIM_MPI_VPSS_GetFrame error..\n");
		}
		else
		{
			printf("addr:0x%x.\n", stFrameInfo.pPhyAddrStd[0]);
		}
		if (s32Ret != 0)
		{
			printf("[%s] ChnByOuput chn %d VIM_MPI_VPSS_GetFrame: %s:%d...\n\n",
                                pstThreadCtx->pThreadName, u32Chn, "error", s32Ret);
			return 1;
		}

		SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(stFrameInfo.enPixfmt, &enFormat, &enBits);
		
		stFrameWaitSave[u32FrameCnt].u16Width      = stFrameInfo.u16Width;
		stFrameWaitSave[u32FrameCnt].u16Height     = stFrameInfo.u16Height;
		stFrameWaitSave[u32FrameCnt].enPixfmt      = stFrameInfo.enPixfmt;

		//printf("cnt:%d addr:0x%x fno:%d enPixfmt:%d.\n", u32FrameCnt, stFrameInfo.pPhyAddrStd[0], stFrameInfo.u16FrameNo, stFrameInfo.enPixfmt);

		stFrameWaitSave[u32FrameCnt].u32ByteUsedStd[0] = stFrameInfo.u32ByteUsedStd[0];
		stFrameWaitSave[u32FrameCnt].u32ByteUsedStd[1] = stFrameInfo.u32ByteUsedStd[1];
		stFrameWaitSave[u32FrameCnt].u32ByteUsedStd[2] = stFrameInfo.u32ByteUsedStd[2];

		//printf("(stFrameInfo.u32ByteUsedStd %d %d %d...\n", stFrameInfo.u32ByteUsedStd[0], stFrameInfo.u32ByteUsedStd[1], stFrameInfo.u32ByteUsedStd[2]);

		memcpy(stFrameWaitSave[u32FrameCnt].pVirAddrStd[0],
                stFrameInfo.pVirAddrStd[0], stFrameInfo.u32ByteUsedStd[0]);
		memcpy(stFrameWaitSave[u32FrameCnt].pVirAddrStd[1],
                stFrameInfo.pVirAddrStd[1], stFrameInfo.u32ByteUsedStd[1]);
		memcpy(stFrameWaitSave[u32FrameCnt].pVirAddrStd[2],
                stFrameInfo.pVirAddrStd[2], stFrameInfo.u32ByteUsedStd[2]);

		s32Ret = VIM_MPI_VPSS_GetFrame_Release(u32GrpId, u32Chn, &stFrameInfo);
		if (s32Ret != 0)
		{
			printf("[%s] ChnByOuput chn %d VIM_MPI_VPSS_GetFrame_Release: %s:%d...\n\n",
                                        pstThreadCtx->pThreadName, u32Chn,  "error", s32Ret);
			return 1;
		}

		u32FrameCnt++;
		if (u32FrameCnt == pstThreadCtx->u32PrintNum)
		{
			printf("will save %d frame:\n", u32FrameCnt);
			VIM_MPI_VPSS_GetChnOutAttr(u32GrpId, u32Chn, &stChnAttr);

            for (i = 0; i < u32FrameCnt; i++)
            {
                if(pstThreadCtx->s8FileName !=NULL)
                {
                    s32Ret = Tools_SaveTestFrameToFile(u32Chn, pstThreadCtx->s8FileName, &stFrameWaitSave[i], 0);
                    if (s32Ret != 0)
                    {
                        SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n",
                                    pstThreadCtx->pThreadName, u32Chn,  "error");
                        return 1;
                    }
				}
				printf("SAMPLE_VPSS_FreedOutContinuousBuff i = %d\n", i);
				SAMPLE_VPSS_FreedOutContinuousBuff(&stChnAttr, u32Chn, i);
            }

			printf("will stop thread continous frame:%d...\n", u32FrameCnt);
			if (stFrameWaitSave != NULL)
				free(stFrameWaitSave);
			u32FrameCnt = 0;
		}

		return 1;
	}
	else /*thread will be to exit*/
	{
		return 0;
	}

	return 1;
}

int llll = 0;
static int Thread_CaseOutputLoop(void *parg)
{
    VIM_S32 s32Ret = 0 ;
	TEST_THREAD_CTX_S *pstThreadCtx = NULL;
	VIM_U32 *u32Grp = NULL;
	VIM_VPSS_FRAME_INFO_S stFrameInfo;
	VIM_U32 u32Chn = 0;
	VIM_U32 u32GrpId = 0;

	if (s_bExit == VIM_TRUE)
	{
        printf("function: %s line:%d thread will be to exit.\n", __func__, __LINE__);
		return 0;
	}

	pstThreadCtx = (TEST_THREAD_CTX_S *)parg;
	if (pstThreadCtx == NULL)
	{
		printf("function: %s :%s exit because parg is null\n", __func__,  pstThreadCtx->pThreadName);
		pstThreadCtx->enState = 0;
		return 0;
	}

	u32Grp   = pstThreadCtx->pUserData;
	u32GrpId = (*u32Grp >> 16) & 0xffff;
	u32Chn   = (*u32Grp >> 0) & 0xffff;


	if (pstThreadCtx->bRun == VIM_TRUE && (pstThreadCtx->u32PrintNum > 0))
	{
		memset(&stFrameInfo, 0, sizeof(stFrameInfo));

		s32Ret = VIM_MPI_VPSS_GetFrame(u32GrpId, u32Chn, &stFrameInfo, 3000);
		if (s32Ret != 0)
		{
			printf("[%s] ChnByOuput chn %d VIM_MPI_VPSS_GetFrame: %s:%d...\n\n",
                                pstThreadCtx->pThreadName, u32Chn,  "error", s32Ret);
			return 1;
		}
		printf("Grp:%d chn:%d vdev:%d vchn:%d fno:%d cnt:%d.. \n", u32GrpId, u32Chn, stFrameInfo.u32SrcVidDev, stFrameInfo.u32SrcVidChn, stFrameInfo.u16FrameNo, llll);
        if (pstThreadCtx->s8FileName !=NULL)
        {
        	printf("save frame file...\n");
            s32Ret =  Tools_SaveTestFrameToFile(u32Chn, pstThreadCtx->s8FileName, &stFrameInfo, 0);
            if (s32Ret != 0)
            {
                SAMPLE_PRT("[%s] chn %d SaveFrameToFile: %s...\n",
                                pstThreadCtx->pThreadName, u32Chn,  "error");
                return 1;
            }
        }

		s32Ret = VIM_MPI_VPSS_GetFrame_Release(u32GrpId, u32Chn, &stFrameInfo);
		if (s32Ret != 0)
		{
			printf("[%s] ChnByOuput chn %d VIM_MPI_VPSS_GetFrame_Release: %s:%d...\n\n",
                                        pstThreadCtx->pThreadName, u32Chn, "error", s32Ret);
			return 1;
		}

		llll++;

		return 1;
	}
	else /*thread will be to exit*/
	{
		return 0;
	}

	return 1;
}

static int Thread_CaseInputLoop(void *parg)
{
	TEST_THREAD_CTX_S *pstThreadCtx = (TEST_THREAD_CTX_S *)parg;
	VIM_U32 *u32Grp = NULL;
	VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};
	VIM_U32 u32GrpId = 0;
	VIM_S32 s32Ret = 0 ;
	VIM_S32 s32StartFrameNo = 50;//mstManInfo.s32StartInputFrameNo[pChnAttr->u8BindChn];

	if (s_bExit == VIM_TRUE)
		return 0;

	pstThreadCtx = (TEST_THREAD_CTX_S *)parg;
	if (pstThreadCtx == NULL)
	{
		printf("%s exit because parg is nil\n",  __func__);
		return 0;
	}

	u32Grp   = pstThreadCtx->pUserData;
	u32GrpId = *u32Grp;

	if (pstThreadCtx->bRun == VIM_TRUE && (pstThreadCtx->u32PrintNum > 0))
	{
        if(pstThreadCtx->s8FileName !=NULL)
        {
            sleep(4);
        }
        else
        {
            usleep(50 * 1000);
        }

		s32Ret = VIM_MPI_VPSS_SendFrame_Prepare(u32GrpId, &stFrameInfo);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] u32GrpId %d VIM_MPI_VPSS_SendFrame_Prepare: %s:%d...\n",
                                    pstThreadCtx->pThreadName, u32GrpId, "error", s32Ret);
			return 1;
		}


        if (pstThreadCtx->s8FileName !=NULL)
        {
            s32Ret = Tools_GetTestFrameFromFile(u32GrpId, &stFrameInfo, pstThreadCtx->s8FileName,
                                                        s32StartFrameNo, 0, pstThreadCtx->u8Dynamic);
            if (s32Ret != 0)
            {
                SAMPLE_PRT("[%s] u32GrpId %d Tools_GetTestFrameFromFile: %s:%d...\n",
                                    pstThreadCtx->pThreadName,  u32GrpId, "error",s32Ret);
                return 1;
            }
        }

		s32Ret = VIM_MPI_VPSS_SendFrame(u32GrpId, &stFrameInfo, 3000);
		if (s32Ret != 0)
		{
			SAMPLE_PRT("[%s] u32GrpId %d VIM_MPI_VPSS_SendFrame: %s:%d...\n",
                            pstThreadCtx->pThreadName,  u32GrpId, "error",s32Ret);
			return 1;
		}

		return 1;

	}
	else /*thread will be to exit*/
	{
        printf("function: %s line:%d thread will be to exit.\n" , __func__, __LINE__);
		return 0;
	}

	return 1;
}

static VIM_S32 __StartThreadForChnIn(THREAD_HANDLE_S *hThread, VIM_U32 *u32Grp,
                            VIM_U32 u32PrintNum, const char *s8FileName, VIM_U8 u8Dynamic)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnIn = 0;

	u32ChnIn = *u32Grp & 0xffff;

	ipp_in[u32ChnIn] = 1;
	printf("function: %s line:%d u32Grp:%d u32ChnIn:%d.\n" , __func__, __LINE__, *u32Grp, u32ChnIn);

	sprintf(s8ThreadName, "CHN-IPP-%d-IN", u32ChnIn);
    printf("%s s8FileName = %s\n", __func__, s8FileName);
	s32Ret = Tools_StartThread( hThread, Thread_CaseInputLoop, s8ThreadName,
                                        u32Grp, u32PrintNum, s8FileName, u8Dynamic);
	if (s32Ret != VIM_SUCCESS)
	{
		printf("function: %s line:%d index:%d  exit2..\n" ,__func__, __LINE__, u32ChnIn);
		goto end;
	}
	printf("case chn<%d> input thread is started.\n", u32ChnIn);

	return 0;
end:
	ipp_in[u32ChnIn] = 0;

	return -1;
}

static VIM_S32 __StartThreadForChaOut(THREAD_HANDLE_S *hThread, VIM_U32 *grp,
                        VIM_U32 u32PrintNum, const char *s8FileName, VIM_U8 u8Dynamic)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnOut = 0;

    u32ChnOut = *grp & 0xffff;
	u32ChnOut += 3;
	printf("function: %s line:%d grp:%d u32ChnOut:%d.\n", __func__, __LINE__, *grp, u32ChnOut);
	sprintf(s8ThreadName, "CHN-%d-OUT", u32ChnOut);
	printf("u32ChnOut:%d s8FileName=%s\n",u32ChnOut,s8FileName);
	s32Ret = Tools_StartThread( hThread, Thread_CaseOutputLoop, s8ThreadName, grp,
                                                    u32PrintNum, s8FileName, u8Dynamic);
	if (s32Ret != VIM_SUCCESS)
		goto end;

	printf("case chn<%d> output thread is started.\n", u32ChnOut);

	return 0;

end:

	return -1;
}
static VIM_S32 __StartThreadForChaOutContinousCap(THREAD_HANDLE_S *hThread, VIM_U32 *grp,
                                    VIM_U32 u32PrintNum, VIM_S8 *s8FileName, VIM_U8 u8Dynamic)
{
	char s8ThreadName[50] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32ChnOut = 0;
	VIM_U32 u32GrpId = 0;
	VIM_CHN_CFG_ATTR_S stChnAttr;

    u32ChnOut = *grp & 0xffff;
	u32GrpId = (*grp >> 16) & 0xffff;

	sprintf(s8ThreadName, "CHN-%d-OUT", u32ChnOut);

	printf("u32GrpId:%d u32ChnOut:%d s8FileName=%s\n", u32GrpId, u32ChnOut, s8FileName);
	VIM_MPI_VPSS_GetChnOutAttr(u32GrpId, u32ChnOut, &stChnAttr);

	#if 0
	while(1)
	{
		s32Ret = VIM_MPI_VPSS_GetFrame(u32GrpId, u32ChnOut, &stFrameInfo, 1000);
		if ((s32Ret != 0) && (s32Get>0))
		{
			printf("buff clean ok..\n");
			break;
		}
		else if(s32Ret == 0)
		{
			s32Get++;
			printf("buff clean run ..\n");
		}
		else
		{}
	}

	s32Ret = VIM_MPI_VPSS_GetFrame_Release(u32GrpId, u32ChnOut, &stFrameInfo);
	if (s32Ret != 0)
	{
		printf("Pre fail GetFrame_Release...\n");
	}
	#endif

	s32Ret = Tools_StartThread(hThread, Thread_CaseOutputContinousCapLoop,
                        s8ThreadName, grp, u32PrintNum, (const char *)s8FileName, u8Dynamic);
	if (s32Ret != VIM_SUCCESS)
		goto end;

	printf("case chn<%d> output thread is started.\n", u32ChnOut);

	return 0;

end:
	printf("case chn<%d> output thread run error.\n", u32ChnOut);
	return -1;
}


VIM_S32 SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(VIM_U8 u8Plane,
									VIM_IMG_FORMAT_E enInputFormat,
									VIM_IMG_BITS_E enInputBits,
									VIM_U16 u16Width,
									VIM_U16 u16Height)
{
	VIM_U32 u32Size = 0;

	if (u8Plane == 0)
	{
		if (enInputFormat != E_IMG_FORMAT_422I)
		{
			switch ((VIM_U32)enInputBits)
			{
				case E_IMG_BITS_10:
					u32Size  = ALIGN_UP(8 * DIV_ROUND_UP (u16Width, 6), 16) * u16Height;
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
		switch (enInputFormat) {
			case E_IMG_FORMAT_422SP:
				if (u8Plane > 1) {
					u32Size = 0;
					break;
				}

				switch ((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 6), 16)
                                                                    * (u16Height);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 8), 16)
                                                                    * (u16Height);
						break;
				}
				break;
			case E_IMG_FORMAT_420SP:
				if (u8Plane > 1) {
					u32Size = 0;
					break;
				}
				switch ((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 6), 16)
                                                                * (u16Height / 2);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 8), 16)
                                                                * (u16Height / 2);
						break;
				}
				break;
			case E_IMG_FORMAT_422P:
				switch ((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 12), 16)
                                                                    * (u16Height);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 16), 16)
                                                                    * (u16Height);
						break;
				}
				break;
			case E_IMG_FORMAT_420P:
				switch ((VIM_U32)enInputBits)
				{
					case E_IMG_BITS_10:
						u32Size  = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 12), 16)
                                                                * (u16Height / 2);
						break;
					case E_IMG_BITS_8:
						u32Size = ALIGN_UP(8 * DIV_ROUND_UP(u16Width, 16), 16)
                                                                * (u16Height / 2);
						break;
				}
				break;
			case E_IMG_FORMAT_601P:
			case E_IMG_FORMAT_709P:
				switch ((VIM_U32)enInputBits)
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

VIM_S32 SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneCframeSizeMain(
									VIM_U8  u8Plane,
									VIM_IMG_FORMAT_E  enFormat,
									VIM_IMG_BITS_E 	enInputBits,
									VIM_U8  u8BitDepth,
									VIM_U8  u8BitDepthu,
									VIM_BOOL enLoss,
									VIM_U16 u16Width,
									VIM_U16 u16Height)
{
	VIM_U32 u32Size = 0;
	VIM_U32 Lossless_slice_size = 0;
	VIM_U32 Number_of_slice = 0;
	VIM_U32 Lossless_compressed_bytes = 0;
	VIM_U32 Lossy_slice_size = 0;
	VIM_U32 tx16 = 0;
	VIM_U16 bit_depth = enInputBits == E_IMG_BITS_8? 8:10;

    enFormat = enFormat;

	if (enLoss == VIM_FALSE)
	{
		Lossless_slice_size = ALIGN(128 * bit_depth / 8, 4);//bit-depth为分量深�?
		Number_of_slice = (u16Width + 127)/128 * u16Height;//
		Lossless_compressed_bytes = Number_of_slice * Lossless_slice_size;
	}
	else
	{
		if (u8Plane == 0)
        {
			tx16 = u8BitDepth;
            Lossy_slice_size = ALIGN(tx16, 32);// Tx16为压缩率
			Number_of_slice = ((ALIGN(u16Width, 32) + 127)/128) * ALIGN(u16Height, 4);//
			Lossless_compressed_bytes = Number_of_slice * Lossy_slice_size;//
		}
		else
		{
			tx16 = u8BitDepthu;
            Lossy_slice_size = ALIGN(tx16, 32);// Tx16为压缩率
			Number_of_slice = ((ALIGN(u16Width, 32) / 2 + 127) / 128) * ALIGN(u16Height, 4) / 2;//
			Lossless_compressed_bytes = Number_of_slice * Lossy_slice_size;//
		}

	}
	u32Size =  Lossless_compressed_bytes;

	return u32Size;
}

VIM_S32 SAMPLE_VPSS_SetOutCaptureBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff,
                VIM_CHN_CFG_ATTR_S *stChnAttr, VIM_U32 u32ChnId, VIM_U32 u32Index)
{
	VIM_U32 u32Plane		= 0;
	VIM_S32 i				= 0;
	VIM_S32 s32StdBytes[VI_FRAME_STD_PLANE_MAX] = {0};
	VIM_S32 s32CfrBytes[VI_FRAME_CFRAME_PLANE_MAX] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32StdBytesNum = 0;
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;

	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(stChnAttr->enPixfmt, &enFormat, &enBits);

	SAMPLE_VPSS_NULL_PTR(stChnAttr);

	for (i = 0; i < VI_FRAME_STD_PLANE_MAX; i++)
	{
		if (stChnAttr->enCframe == VIM_TRUE)
		{
			s32CfrBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneCframeSizeMain(i, enFormat, enBits,
        stChnAttr->u8BitDepth, stChnAttr->u8BitDepthu, stChnAttr->enLoss, stChnAttr->u16Width, stChnAttr->u16Height);

			if (s32CfrBytes[i] > 0)
				u32StdBytesNum++;
		}
		else
		{
			s32StdBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(i, enFormat, enBits,
                                                        stChnAttr->u16Width, stChnAttr->u16Height);

			if (s32StdBytes[i] > 0)
				u32StdBytesNum++;
		}
	}

	if ((u32StdBytesNum > 0) && (stChnAttr->u16BufferNum > 0))
	{
		if (stChnAttr->enCframe == VIM_TRUE)
		{
			sprintf(VbPoolOutNameCap[u32Index], "InVpssChn%d", u32ChnId);
			s32VbPoolOutCap[u32Index][u32ChnId] = VIM_MPI_VB_CreatePool(s32CfrBytes[0],
                                            1 * u32StdBytesNum, VbPoolOutNameCap[u32Index]);
			if (s32VbPoolOutCap[u32Index][u32ChnId] == (VIM_S32)VB_INVALID_POOLID ) {
				printf("VIM_MPI_VB_CreatePool failed!\n");
				return -1;
			}
			s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			if (s32Ret) {
				printf("VIM_MPI_VB_MmapPool failed!\n");
				VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
                return -1;
			}
		}
		else
		{
			sprintf(VbPoolOutNameCap[u32Index], "InVpssChn%d", u32ChnId);
			s32VbPoolOutCap[u32Index][u32ChnId] = VIM_MPI_VB_CreatePool(s32StdBytes[0],
                                            1 * u32StdBytesNum, VbPoolOutNameCap[u32Index]);
			if (s32VbPoolOutCap[u32Index][u32ChnId] == (VIM_S32)VB_INVALID_POOLID ) {
				printf("VIM_MPI_VB_CreatePool failed!\n");
				return -1;
			}
			s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			if (s32Ret) {
				printf("VIM_MPI_VB_MmapPool failed!\n");
				VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
				return -1;
			}
		}
	}

	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++)
	{
		if (!s32StdBytes[u32Plane])
			continue;

		s32VbOutBlkCap[u32Index][u32Plane] = VIM_MPI_VB_GetBlock(s32VbPoolOutCap[u32Index][u32ChnId],
                                                                            s32StdBytes[u32Plane], NULL);
		if (s32VbOutBlkCap[u32Index][u32Plane] == (VIM_S32)VB_INVALID_HANDLE) {
			printf("VIM_MPI_VB_GetBlock failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		pstUserBuff->pPhyAddrStd[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbOutBlkCap[u32Index][u32Plane]);
		if (!pstUserBuff->pPhyAddrStd[u32Plane]) {
			printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolOutCap[u32Index][u32ChnId],
                pstUserBuff->pPhyAddrStd[u32Plane], (VIM_VOID *)&(pstUserBuff->pVirAddrStd[u32Plane]));
		if (s32Ret) {
			printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];
		if (pstUserBuff->pVirAddrStd[u32Plane]== NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);

	}


	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if (!s32CfrBytes[u32Plane])
			continue;

		s32VbOutBlkCap[u32Index][u32Plane] = VIM_MPI_VB_GetBlock(s32VbPoolOutCap[u32Index][u32ChnId],
                                                                            s32StdBytes[u32Plane], NULL);
		if (s32VbOutBlkCap[u32Index][u32Plane] == (VIM_S32)VB_INVALID_HANDLE){
			printf("VIM_MPI_VB_GetBlock failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		pstUserBuff->pPhyAddrCfr[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbOutBlkCap[u32Index][u32Plane]);
		if (!pstUserBuff->pPhyAddrCfr[u32Plane]) {
			printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolOutCap[u32Index][u32ChnId],
                pstUserBuff->pPhyAddrCfr[u32Plane], (VIM_VOID *)&(pstUserBuff->pVirAddrCfr[u32Plane]));
		if (s32Ret) {
			printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
			VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);
			return -1;
		}

		pstUserBuff->u32CfrSize[u32Plane] = s32StdBytes[u32Plane];
		if (pstUserBuff->pVirAddrCfr[u32Plane] == NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
	}

	return 0;

}

VIM_S32 SAMPLE_VPSS_FreedOutContinuousBuff(VIM_CHN_CFG_ATTR_S *stChnAttr, VIM_U32 u32ChnId, VIM_U32 u32Index)
{
	VIM_U32 u32Plane		= 0;
	VIM_S32 i				= 0;
	VIM_S32 s32StdBytes[VI_FRAME_STD_PLANE_MAX] = {0};
	VIM_S32 s32CfrBytes[VI_FRAME_CFRAME_PLANE_MAX] = {0};
	VIM_U32 u32StdBytesNum = 0;
	VIM_IMG_FORMAT_E    enFormat = E_IMG_FORMAT_422SP;
	VIM_IMG_BITS_E  	enBits = E_IMG_BITS_8;

    for (i = 0; i < VI_FRAME_STD_PLANE_MAX; i++ )
	{
		if (stChnAttr->enCframe == VIM_TRUE)
		{
			s32CfrBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneCframeSizeMain( i, enFormat, enBits,
        stChnAttr->u8BitDepth, stChnAttr->u8BitDepthu, stChnAttr->enLoss, stChnAttr->u16Width, stChnAttr->u16Height);

			if (s32CfrBytes[i] > 0)
				u32StdBytesNum++;
		}
		else
		{
			s32StdBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(i, enFormat, enBits,
                                                            stChnAttr->u16Width, stChnAttr->u16Height);

			if (s32StdBytes[i] > 0)
				u32StdBytesNum++;
		}
	}


	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if(! s32StdBytes[u32Plane])
				continue;
		VIM_MPI_VB_ReleaseBlock(s32VbOutBlkCap[u32Index][u32Plane]);
	}
	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if(! s32CfrBytes[u32Plane])
				continue;
		VIM_MPI_VB_ReleaseBlock(s32VbOutBlkCap[u32Index][u32Plane]);
	}

	VIM_MPI_VB_MunmapPool(s32VbPoolOutCap[u32Index][u32ChnId]);
    VIM_MPI_VB_DestroyPool(s32VbPoolOutCap[u32Index][u32ChnId]);


	return 0;

}

VIM_S32 SAMPLE_VPSS_SetOutUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_CHN_CFG_ATTR_S *stChnAttr, VIM_U32 u32ChnId)
{
	VIM_U32 u32Plane 		= 0;
	VIM_S32 i 	   			= 0;
	VIM_S32 s32StdBytes[VI_FRAME_STD_PLANE_MAX] = {0};
	VIM_S32 s32CfrBytes[VI_FRAME_CFRAME_PLANE_MAX] = {0};
	VIM_S32 s32Ret = 0;
	VIM_U32 u32StdBytesNum = 0;
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;

	SAMPLE_VPSS_NULL_PTR(stChnAttr);
	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(stChnAttr->enPixfmt, &enFormat, &enBits);

	for (i = 0; i < VI_FRAME_STD_PLANE_MAX; i++ )
	{
		if (stChnAttr->enCframe == VIM_TRUE)
		{
			s32CfrBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneCframeSizeMain( i, enFormat, enBits,
                                                    stChnAttr->u8BitDepth, stChnAttr->u8BitDepthu,
                                                    stChnAttr->enLoss, stChnAttr->u16Width, stChnAttr->u16Height);
			if(s32CfrBytes[i] > 0)
				u32StdBytesNum++;
		}
		else
		{
			s32StdBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(i, enFormat, enBits,
                                                        stChnAttr->u16Width, stChnAttr->u16Height);
			if(s32StdBytes[i] > 0)
				u32StdBytesNum++;
		}
	}

	if (VbOutnum[u32ChnId] == VIM_FALSE && (u32StdBytesNum > 0) && (stChnAttr->u16BufferNum > 0))
	{
		if (stChnAttr->enCframe == VIM_TRUE)
		{
			sprintf(VbPoolOutName,"InVpssChn%d", u32ChnId);
			printf("Sample_VbPoolOutName = %s\n", VbPoolOutName);
			s32VbPoolOut[u32ChnId] = VIM_MPI_VB_CreatePool(s32CfrBytes[0],
                        stChnAttr->u16BufferNum * u32StdBytesNum, VbPoolOutName);
			if (s32VbPoolOut[u32ChnId] == (VIM_S32)VB_INVALID_POOLID) {
				printf("VIM_MPI_VB_CreatePool failed!\n");
				return -1;
			}
			printf("s32VbPoolOut[u32ChnId] = %d\n", s32VbPoolOut[u32ChnId]);
			s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolOut[u32ChnId]);
			if (s32Ret) {
				printf("VIM_MPI_VB_MmapPool failed!\n");
				VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
                return -1;
			}
			VbOutnum[u32ChnId] = VIM_TRUE;
		}
		else
		{
			sprintf(VbPoolOutName,"InVpssChn%d", u32ChnId);
			printf("Sample_VbPoolOutName = %s\n", VbPoolOutName);
			s32VbPoolOut[u32ChnId] = VIM_MPI_VB_CreatePool(s32StdBytes[0],
                        stChnAttr->u16BufferNum * u32StdBytesNum, VbPoolOutName);
			if (s32VbPoolOut[u32ChnId] == (VIM_S32)VB_INVALID_POOLID ) {
				printf("VIM_MPI_VB_CreatePool failed!\n");
				return -1;
			}
			printf("s32VbPoolOut[u32ChnId] = %d\n", s32VbPoolOut[u32ChnId]);
			s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolOut[u32ChnId]);
			if (s32Ret) {
				printf("VIM_MPI_VB_MmapPool failed!\n");
				VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
				return -1;
			}
			VbOutnum[u32ChnId] = VIM_TRUE;
		}
	}

	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if (!s32StdBytes[u32Plane])
			continue;

		s32VbOutBlk = VIM_MPI_VB_GetBlock(s32VbPoolOut[u32ChnId], s32StdBytes[u32Plane], NULL);
        if (s32VbOutBlk == (VIM_S32)VB_INVALID_HANDLE){
            printf("VIM_MPI_VB_GetBlock failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

        pstUserBuff->pPhyAddrStd[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbOutBlk);
        if (!pstUserBuff->pPhyAddrStd[u32Plane]) {
            printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

        s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolOut[u32ChnId],
            pstUserBuff->pPhyAddrStd[u32Plane], (VIM_VOID *)&(pstUserBuff->pVirAddrStd[u32Plane]));
        if (s32Ret) {
            printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

		pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];
		if (pstUserBuff->pVirAddrStd[u32Plane]== NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
	}

	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if (!s32CfrBytes[u32Plane])
			continue;

		s32VbOutBlk = VIM_MPI_VB_GetBlock(s32VbPoolOut[u32ChnId], s32StdBytes[u32Plane], NULL);
        if (s32VbOutBlk == (VIM_S32)VB_INVALID_HANDLE){
            printf("VIM_MPI_VB_GetBlock failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

        pstUserBuff->pPhyAddrCfr[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbOutBlk);
        if (!pstUserBuff->pPhyAddrCfr[u32Plane]) {
            printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

        s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolOut[u32ChnId],
                pstUserBuff->pPhyAddrCfr[u32Plane], (VIM_VOID *)&(pstUserBuff->pVirAddrCfr[u32Plane]));
        if(s32Ret) {
            printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolOut[u32ChnId]);
            VIM_MPI_VB_DestroyPool(s32VbPoolOut[u32ChnId]);
            return -1;
        }

		pstUserBuff->u32CfrSize[u32Plane]  = s32StdBytes[u32Plane];
		if (pstUserBuff->pVirAddrCfr[u32Plane] == NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
	}

	return 0;
}

VIM_S32 SAMPLE_VPSS_SetInUserBuff(VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_VPSS_GROUP_ATTR_S *vpssGrpAttr)
{
    VIM_U32 i = 0;
	VIM_S32 s32Ret = 0;
	VIM_U32 u32Plane = 0;
	VIM_S32 s32StdBytes[VI_FRAME_STD_PLANE_MAX] = {0};
	VIM_U32 u32StdBytesNum = 0;
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;

	SAMPLE_VPSS_NULL_PTR(vpssGrpAttr);
	SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(vpssGrpAttr->stGrpInput.enPixfmt, &enFormat, &enBits);

	for (i = 0; i < VI_FRAME_STD_PLANE_MAX; i++ )
	{
		s32StdBytes[i] = SAMPLE_COMMON_VPSS_CalcFrameSinglePlaneSize(i, enFormat, enBits,
                        vpssGrpAttr->stGrpInput.u16Width, vpssGrpAttr->stGrpInput.u16Height);

		if (s32StdBytes[i] > 0)
			u32StdBytesNum++;
	}

	if (VbInnum[vpssGrpAttr->u32GrpDataSrc] == VIM_FALSE
            && (vpssGrpAttr->stGrpInput.u16BufferNum >0) && (u32StdBytesNum >0))
	{
		sprintf(VbPoolInName,"InVpssSrc%d", vpssGrpAttr->u32GrpDataSrc);
		printf("Sample_VbPoolInName = %s\n", VbPoolInName);
		s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc] = VIM_MPI_VB_CreatePool(s32StdBytes[0],
                    vpssGrpAttr->stGrpInput.u16BufferNum * u32StdBytesNum, VbPoolInName);
		if (s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc] == (VIM_S32)VB_INVALID_POOLID) {
			printf("VIM_MPI_VB_CreatePool failed!\n");
			return -1;
		}

		printf("s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc] = %d\n", s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
		s32Ret = VIM_MPI_VB_MmapPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
		if (s32Ret) {
			printf("VIM_MPI_VB_MmapPool failed!\n");
			VIM_MPI_VB_DestroyPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
			return -1;
		}
		VbInnum[vpssGrpAttr->u32GrpDataSrc] = VIM_TRUE;
	}


	for (u32Plane = 0; u32Plane < VI_FRAME_STD_PLANE_MAX; u32Plane++ )
	{
		if (!s32StdBytes[u32Plane])
			continue;

		s32VbInBlk = VIM_MPI_VB_GetBlock(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc], s32StdBytes[u32Plane], NULL);
        if (s32VbInBlk == (VIM_S32)VB_INVALID_HANDLE){
            printf("VIM_MPI_VB_GetBlock failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            return -1;
        }

        pstUserBuff->pPhyAddrStd[u32Plane] = VIM_MPI_VB_Handle2PhysAddr(s32VbInBlk);
        if (!pstUserBuff->pPhyAddrStd[u32Plane]) {
            printf("VIM_MPI_VB_Handle2PhysAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            return -1;
        }

        s32Ret = VIM_MPI_VB_GetBlkVirAddr(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc],
            pstUserBuff->pPhyAddrStd[u32Plane], (VIM_VOID *)&(pstUserBuff->pVirAddrStd[u32Plane]));
        if (s32Ret) {
            printf("VIM_MPI_VB_GetBlkVirAddr failed!\n");
            VIM_MPI_VB_MunmapPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            VIM_MPI_VB_DestroyPool(s32VbPoolIn[vpssGrpAttr->u32GrpDataSrc]);
            return -1;
        }

		pstUserBuff->u32StdSize[u32Plane] = s32StdBytes[u32Plane];
		if (pstUserBuff->pVirAddrStd[u32Plane] == NULL)
			printf("malloc std plane %d size %d is null\n", i, s32StdBytes[u32Plane]);
	}

	return s32Ret;
}

VIM_S32 SAMPLE_COMMON_VPSS_StopGroup(VIM_U32 u32Group)
{
	VIM_S32 s32Ret      = 0;
	VIM_S8  i           = 0;
	VIM_VPSS_GROUP_ATTR_S vpssGrpAttr 	= {0};

    for (i=0; i<MAX_VPSS_GROUP; i++)
    {
        if (s8GroupMask[u32Group] == 0)
            goto out;
    }

	VIM_MPI_VPSS_GetGrpAttr(u32Group, &vpssGrpAttr);

    for (i = 0; i < MAX_VPSS_CHN; i++)
    {
        if (((u32ChnOutMask[u32Group] >> i) & 0x1) == 0)
            continue;

		printf("vpss will stop group %d chn:%d.\n", u32Group, i);
        s32Ret = VIM_MPI_VPSS_StopChn(u32Group, i);
        CHECK_VPSS_RET_RETURN(s32Ret);
		printf("vpss stop group %d chn:%d ok.\n", u32Group, i);
		printf("vpss will disable chn group %d chn:%d.\n", u32Group, i);
        s32Ret = VIM_MPI_VPSS_DisableChn(u32Group, i);
        CHECK_VPSS_RET_RETURN(s32Ret);
		printf("vpss disable chn group %d chn:%d ok.\n", u32Group, i);
		if (VbOutnum[i] == VIM_TRUE)
        {
			VIM_MPI_VB_MunmapPool(s32VbPoolOut[i]);
			VIM_MPI_VB_DestroyPool(s32VbPoolOut[i]);
		}
    }

    u32ChnOutMask[u32Group] = 0;
	printf("vpss will DestroyGrp group %d.\n",u32Group);
    s32Ret = VIM_MPI_VPSS_DestroyGrp(u32Group);
	printf("vpss DestroyGrp group %d ok.\n",u32Group);
    CHECK_VPSS_RET_RETURN(s32Ret);
    s8GroupMask[u32Group] = 0;

	for (i = 0; i<MAX_VPSS_GROUP; i++)
    {
        if(s8GroupMask[u32Group] == 1)
            goto out;
    }

	if (VbInnum[vpssGrpAttr.u32GrpDataSrc] == VIM_TRUE)
	{
		VIM_MPI_VB_MunmapPool(s32VbPoolIn[vpssGrpAttr.u32GrpDataSrc]);
        VIM_MPI_VB_DestroyPool(s32VbPoolIn[vpssGrpAttr.u32GrpDataSrc]);
	}

	printf("vpss will DisableDev.\n");

	VIM_MPI_VPSS_DisableDev();
	CHECK_VPSS_RET_RETURN(s32Ret);
	printf("vpss DisableDev ok.\n");
out:

    return 0;
}

VIM_S32 SAMPLE_COMMON_VPSS_StartGroup(VIM_U32 u32Group, VIM_VPSS_GROUP_ATTR_S *pStGrpAttr)
{
	VIM_S32 s32Ret  = 0;

	SAMPLE_VPSS_NULL_PTR(pStGrpAttr);

    s32Ret = VIM_MPI_VPSS_EnableDev();
	CHECK_VPSS_RET_RETURN(s32Ret);

    if (s8GroupMask[u32Group] == 0)
    {
        s32Ret = VIM_MPI_VPSS_CreatGrp(pStGrpAttr);
        CHECK_VPSS_RET_RETURN(s32Ret);
        s32Ret = VIM_MPI_VPSS_SetGrpInAttr(u32Group, pStGrpAttr);
        CHECK_VPSS_RET_RETURN(s32Ret);
        s8GroupMask[u32Group] = 1;
    }

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_StartChn(VIM_U32 u32Group, VIM_U32 u32ChnId, VIM_CHN_CFG_ATTR_S *pStChnAttr)
{
    VIM_S32 s32Ret = 0;

	SAMPLE_VPSS_NULL_PTR(pStChnAttr);

    s32Ret = VIM_MPI_VPSS_SetChnOutAttr(u32Group, u32ChnId, pStChnAttr);
    CHECK_VPSS_RET_RETURN(s32Ret);

    s32Ret = VIM_MPI_VPSS_EnableChn(u32Group, u32ChnId);
    CHECK_VPSS_RET_RETURN(s32Ret);

	s32Ret = VIM_MPI_VPSS_StartChn(u32Group, u32ChnId);
    u32ChnOutMask[u32Group] |= 1U << u32ChnId;
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_StopChn(VIM_U32 u32Group, VIM_U32 u32ChnId)
{
    VIM_S32 s32Ret = 0;

	s32Ret = VIM_MPI_VPSS_DisableChn(u32Group, u32ChnId);
    CHECK_VPSS_RET_RETURN(s32Ret);
	
    s32Ret = VIM_MPI_VPSS_StopChn(u32Group, u32ChnId);
    CHECK_VPSS_RET_RETURN(s32Ret);

    u32ChnOutMask[u32Group] &= ~(1U << u32ChnId);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_Send(VIM_U32 u32Group, VIM_U32 u32PrintNum, const char *s8FileName, VIM_U8 u8Dynamic)
{
	VIM_S32 s32Ret                      = 0;
    VIM_VPSS_GROUP_ATTR_S Sam_StGrpAttr = {0};

    u32GrpIn[u32Group] = u32Group;
	GrphThread[u32Group] = malloc( MAX_VPSS_VIDEO_POINT * sizeof(GrphThread[u32Group]));
	if (!GrphThread[u32Group])
	{
		SAMPLE_PRT("Init chn thread error.");
		return 0;
	}

	memset(GrphThread[u32Group], 0, MAX_VPSS_VIDEO_POINT * sizeof(GrphThread[u32Group]));
    s8Sample_num = VIM_TRUE;


	s32Ret = VIM_MPI_VPSS_GetGrpAttr(u32Group, &Sam_StGrpAttr);
    CHECK_VPSS_RET_RETURN(s32Ret);
	s32Ret = __StartThreadForChnIn(&GrphThread[u32Group], &u32GrpIn[u32Group], u32PrintNum, s8FileName, u8Dynamic);
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_SetInUserBuff(VIM_U32 u32Group, VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_U32 u32BuffOfset)
{
    VIM_S32 s32Ret = 0;

	SAMPLE_VPSS_NULL_PTR(pstUserBuff);

    s32Ret = VIM_MPI_VPSS_SetGrpInUserBufs(u32Group, u32BuffOfset, pstUserBuff);
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_SetOutUserBuff(VIM_U32 u32Group, VIM_U32 u32ChnId,
                        VIM_STREAM_DMA_BUFFER *pstUserBuff, VIM_U32 u32BuffOfset)
{
    VIM_S32 s32Ret = 0;

	SAMPLE_VPSS_NULL_PTR(pstUserBuff);

    s32Ret = VIM_MPI_VPSS_SetChnOutUserBufs(u32Group, u32ChnId, u32BuffOfset, pstUserBuff);
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_Capture(VIM_U32 u32Group, VIM_U32 u32ChnId,
                VIM_U32 u32CaptureNum, const char *s8SavePush, VIM_U8 u8Dynamic)
{

    VIM_S32 s32Ret = 0;

    if (s8Sample_num == VIM_FALSE)
    {
	    ChnhThread[u32Group][u32ChnId] = malloc(MAX_VPSS_VIDEO_POINT * sizeof(ChnhThread[u32Group][u32ChnId]));
        if (!ChnhThread[u32Group][u32ChnId])
        {
            SAMPLE_PRT("Init chn thread error.");
            return 0;
        }

	    memset(ChnhThread[u32Group][u32ChnId], 0, MAX_VPSS_VIDEO_POINT * sizeof(ChnhThread[u32Group][u32ChnId]));
        s8Sample_num = VIM_TRUE;
    }

    u32GrpOut[u32ChnId]  = (u32Group & 0xffffU) <<16;
	u32GrpOut[u32ChnId] |= u32ChnId;

    s32Ret = __StartThreadForChaOut(&ChnhThread[u32Group][u32ChnId],
                &u32GrpOut[u32ChnId], u32CaptureNum, s8SavePush, u8Dynamic);
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMMON_VPSS_Capture_ContinuousFrm(VIM_U32 u32Group, VIM_U32 u32ChnId,
                                VIM_U32 u32CaptureNum, VIM_S8 *s8SavePush, VIM_U8 u8Dynamic)
{

    VIM_S32 s32Ret = 0;
	VIM_U32 i = 0;
	VIM_STREAM_DMA_BUFFER stUserBuff;
	VIM_CHN_CFG_ATTR_S stChnAttr;

    if (s8Sample_num == VIM_FALSE && u32Group < MAX_VPSS_GROUP && u32ChnId < MAX_VPSS_CHN)
    {
	    ChnhThread[u32Group][u32ChnId] = malloc( MAX_VPSS_VIDEO_POINT * sizeof(ChnhThread[u32Group][u32ChnId]));
        if (!ChnhThread[u32Group][u32ChnId])
        {
            SAMPLE_PRT("Init chn thread error.");
            return 0;
        }
	    memset(ChnhThread[u32Group][u32ChnId], 0, MAX_VPSS_VIDEO_POINT * sizeof(ChnhThread[u32Group][u32ChnId]));
        s8Sample_num = VIM_TRUE;
    }


	VIM_MPI_VPSS_GetChnOutAttr(u32Group, u32ChnId, &stChnAttr);

	stFrameWaitSave = malloc(200 * sizeof(VIM_VPSS_FRAME_INFO_S));
	memset( &stFrameWaitSave[0], 0, 200 * sizeof(VIM_VPSS_FRAME_INFO_S));
	for (i = 0; i<u32CaptureNum; i++)
	{
		SAMPLE_VPSS_SetOutCaptureBuff(&stUserBuff, &stChnAttr, u32ChnId, i);
		stFrameWaitSave[i].pVirAddrStd[0] = stUserBuff.pVirAddrStd[0];

		if(!stFrameWaitSave[i].pVirAddrStd[0])
			return VIM_FAILURE;
		stFrameWaitSave[i].pVirAddrStd[1] = stUserBuff.pVirAddrStd[1];

		if(!stFrameWaitSave[i].pVirAddrStd[1])
			return VIM_FAILURE;
		stFrameWaitSave[i].pVirAddrStd[2] = stUserBuff.pVirAddrStd[2];
	}

	u32FrameCnt=0;

    u32GrpOut[u32ChnId]  = (u32Group & 0xffffU) <<16;
	u32GrpOut[u32ChnId] |= u32ChnId;

    s32Ret = __StartThreadForChaOutContinousCap(&ChnhThread[u32Group][u32ChnId],
                            &u32GrpOut[u32ChnId], u32CaptureNum, s8SavePush, u8Dynamic);
    CHECK_VPSS_RET_RETURN(s32Ret);

    return VIM_SUCCESS;
}



VIM_S32 SAMPLE_COMMON_VPSS_PixFmtToVpssFmt(PIXEL_FORMAT_E enPixfmt,
                        VIM_IMG_FORMAT_E *enFormat, VIM_IMG_BITS_E *enBits)
{
	VIM_S32 s32Ret = 0;

	if ((enFormat == NULL) || (enBits == NULL))
	{
		return VIM_VPSS_ERROR_NO_VALID_MEMRY;
	}

	switch (enPixfmt) {
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

VIM_S32 SAMPLE_COMMON_VPSS_VpssFmtToPixFmt(VIM_IMG_FORMAT_E enFormat, VIM_IMG_BITS_E enBits, PIXEL_FORMAT_E *enPixfmt)
{
	VIM_S32 s32Ret = 0;

	if (enPixfmt == NULL)
	{
		return VIM_VPSS_ERROR_NO_VALID_MEMRY;
	}

	switch (enFormat) {
		case E_IMG_FORMAT_422SP:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_YUV_SEMIPLANAR_422;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_YUV_SEMIPLANAR_422_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_422P:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_YUV_PLANAR_422;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_YUV_PLANAR_422_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_420SP:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_420P:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_YUV_PLANAR_420;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_YUV_PLANAR_420_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_601P:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_RGB_PLANAR_601;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_RGB_PLANAR_601_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_709P:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_RGB_PLANAR_709;
			else if(enBits == E_IMG_BITS_10)
				*enPixfmt = PIXEL_FORMAT_RGB_PLANAR_709_P10;
			else
				s32Ret = -1;
		}
		break;

		case E_IMG_FORMAT_422I:
		{
			if(enBits == E_IMG_BITS_8)
				*enPixfmt = PIXEL_FORMAT_YUV_422_INTERLEAVE;
			else
				s32Ret = -1;
		}
		break;

		default:
			s32Ret = -1;
		break;
	}

	return s32Ret;
}


