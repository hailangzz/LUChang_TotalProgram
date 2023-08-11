#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "sample_comm_vi.h"
#include "mpi_viu.h"
#include "mpi_snr.h"
#include "sensor_ctrl.h"
#include "mpi_isp.h"
#include "vim_comm_video.h"
#include "vim_comm_isp.h"
#include "iniparser.h"
#include "do_ubinder_api.h"
#include "mpi_vb.h"
#include "sensor_ioctl.h"
#include "mpi_sys.h"
#include "mpi_ae.h"

static SAMPLE_SIF_ISP_PARA_T g_DevConfig;

static void *vsrc_vptr = NULL;
static int vsrc_len = 0;
static int file_init(char *filename)
{
    void *ptr = NULL;
    int fd = 0, ret = 0;
    struct stat finfo = {0};
    char scmd[200] = {0};

    snprintf(scmd, (sizeof(scmd) - 1), "open file %s", filename);
    fd = open(filename, O_RDWR, S_IRWXU);
    if (fd <= 0)
    {
        perror(scmd);
        return -1;
    }

    ret = fstat(fd, &finfo);
    if (ret < 0) {
        perror(scmd);
        close(fd);
        return -1;
    }
    vsrc_len = finfo.st_size;
    printf ("===> Get File lenght:%ld\n", finfo.st_size);

    ptr = mmap(NULL, finfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr != NULL) {
        vsrc_vptr = ptr;
    } else {
        perror ("mmap");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}

static int file_uninit(void)
{
    if (vsrc_vptr != NULL) {
        munmap(vsrc_vptr, vsrc_len);
    }

    return 0;
}


static void SAMPLE_SIF_Convert_Interface(char *str, int *type)
{
    VIM_U32 i = 0;

	const char *sInterFaceStr[] = {
		"E_INPUT_NOT_CONNECT",
		"E_DVP_INTERFACE",
		"E_MIPI_INTERFACE",
		"E_LVDS_INTERFACE",
		"E_BT656_INTERFACE",
		"E_BT1120_INTERFACE",
		"E_SSI_INTERFACE",
		"E_HISPI_INTERFACE",
	};

    if (str == NULL)
        return;

    *type = 0;

	for (i = 0; i < ARRAY_SIZE(sInterFaceStr); i++)
	{
        if (strcmp(str, sInterFaceStr[i]) == 0)
        {
            *type = i;

			return;
		}
	}

	printf("InterFace fail!\n");
}


static void SAMPLE_SIF_Convert_Input_Code(char *str0, char *str1, int *fmt, int *bits, int *fmtcode)
{
    VIM_U32 i = 0;

	const char *pInputFormat[] = {
		"E_NONE_INPUT_FORMAT",
        "E_INPUT_FORMAT_YUV422",
		"E_INPUT_FORMAT_YUV420",
		"E_INPUT_FORMAT_RAW",
	};

	const char *pOutputBits[] = {
		"E_DATA_BITS_8",
		"E_DATA_BITS_10",
		"E_DATA_BITS_12",
		"E_DATA_BITS_16",
	};

    if (str0 == NULL || str1 == NULL) {
        return;
    }

    *fmtcode = 0;

	for (i = 0; i < ARRAY_SIZE(pInputFormat); i++)
	{
        if (strcmp(str0, pInputFormat[i]) == 0) {
            *fmt = i;
            break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pOutputBits); i++)
	{
        if (strcmp(str1, pOutputBits[i]) == 0) {
            *bits = i;
            break;
		}
	}

    switch (*bits){
        case 0:
            *fmtcode = (*fmt == 3) ? V4L2_MBUS_FMT_SGRBG8_1X8 : 0;
            *fmtcode = (*fmt == 2) ? V4L2_MBUS_FMT_YUV420M : (*fmtcode);
            *fmtcode = (*fmt == 1) ? V4L2_MBUS_FMT_UYVY8_2X8 : (*fmtcode);
            break;
        case 1:
            *fmtcode = (*fmt == 3) ? V4L2_MBUS_FMT_SGRBG10_1X10 : 0;
            *fmtcode = (*fmt == 2) ? V4L2_MBUS_FMT_YUV420M_20 : (*fmtcode);
            *fmtcode = (*fmt == 1) ? V4L2_MBUS_FMT_YUYV10_2X10 : (*fmtcode);
            break;
        case 2:
        case 3:
            *fmtcode = (*fmt == 3) ? V4L2_MBUS_FMT_SGRBG12_1X12 : 0;
            *fmtcode = (*fmt == 1) ? V4L2_MBUS_FMT_VYUY8_1X16 : (*fmtcode);
            break;
    }
}


static VIM_S32 SAMPLE_Get_InfoFromIni(char *iniName, SAMPLE_SIF_ISP_PARA_T *config)
{

	char cmd[64] = {0};
    char *strTemp[4];
    VIM_U32 i = 0;
    dictionary *ini;

	ini = iniparser_load(iniName);
	if (ini == NULL)
		return -1;

    for (i = 0; i < VI_DEV_MAX; i++) {
        sprintf(cmd, "SIF%d_CONFIG:Enable", i);
        config->bSifEnable[i] = iniparser_getint(ini, cmd, 0);
        // if (config->bSifEnable[i] != 0) {
        sprintf(cmd, "SIF%d_CONFIG:Interface", i);
        strTemp[0] = (char *)iniparser_getstring(ini, cmd, NULL);
        SAMPLE_SIF_Convert_Interface(strTemp[0], (int *)&config->InterFace[i]);

        sprintf(cmd, "SIF%d_CONFIG:InputFormat", i);
        strTemp[1] = (char *)iniparser_getstring(ini, cmd, NULL);

        sprintf(cmd, "SIF%d_CONFIG:DataBits", i);
        strTemp[2] = (char *)iniparser_getstring(ini, cmd, NULL);
        SAMPLE_SIF_Convert_Input_Code(strTemp[1], strTemp[2], (int *)&config->Format[i],
                                            (int *)&config->Bits[i], (int *)&config->FormatCode[i]);

        sprintf(cmd, "SIF%d_CONFIG:OutputMode", i);
        config->OutputMode[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:BufferDepth", i);
        config->BufferDepth[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:SensorNum", i);
        config->SensorNum[i] = iniparser_getint(ini, cmd, 0);
        switch(config->SensorNum[i]){
            case SC500AI_MIPI_1520P_25FPS:
            case SONY_IMX334_MIPI_1080P_25FPS:
                config->SensorWidth[i] = 3840;
                config->SensorHeight[i] = 2160;
                break;
            case SONY_IMX347_MIPI_1080P_25FPS:
                config->SensorWidth[i] = 2688;
                config->SensorHeight[i] = 1520;
                break;
            case SONY_IMX385_MIPI_1080P_25FPS:
                config->SensorWidth[i] = 1936;
                config->SensorHeight[i] = 1096;
                break;
            case 10:
            case SAMPLE_VI_MODE_BT1120_1080P:
                config->SensorWidth[i] = 1920;
                config->SensorHeight[i] = 1080;
                break;
            case SAMPLE_VI_MODE_BT1120_1440P:
                config->SensorWidth[i] = 2560;
                config->SensorHeight[i] = 1440;
                break;
            case OV_7725_DC_VGA_480P_25FPS:
                config->SensorWidth[i] = 640;
                config->SensorHeight[i] = 480;
                break;
            case SONY_IMX415_MIPI_4K_25FPS:
                config->SensorWidth[i] = 3840;
                config->SensorHeight[i] = 2160;
                break;
            case VI_MODE_MIPI_YUV_30FPS:
                config->SensorWidth[i] = 3840;
                config->SensorHeight[i] = 2160;
                break;
            default:
                config->SensorHeight[i] = 1080;
                config->SensorWidth[i] = 1920;
                break;
        }
        sprintf(cmd, "SIF%d_CONFIG:LLCEnable", i);
        config->bLLCEnable[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:CropEnable", i);
        config->CropEnable[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:CropLeft", i);
        config->CropLeft[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:CropTop", i);
        config->CropTop[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:CropWidth", i);
        config->CropWidth[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:CropHeight", i);
        config->CropHeight[i] = iniparser_getint(ini, cmd, 0);

        if(config->InterFace[i] == E_INPUT_NOT_CONNECT){
            config->CropEnable[i] = 1;
            config->SensorHeight[i] = config->CropHeight[i];
            config->SensorWidth[i] = config->CropWidth[i];
        }
        sprintf(cmd, "SIF%d_CONFIG:wdrEnable", i);
        config->wdrEnable[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:wdrMode", i);
        config->wdrMode[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:wdrFrmNum", i);
        config->wdrFrmNum[i] = iniparser_getint(ini, cmd, 0);

        sprintf(cmd, "SIF%d_CONFIG:REMAP_REG0", i);
        config->u32Remap0[i] = iniparser_getint(ini, cmd, 0);
        sprintf(cmd, "SIF%d_CONFIG:REMAP_REG1", i);
        config->u32Remap1[i] = iniparser_getint(ini, cmd, 0);
        sprintf(cmd, "SIF%d_CONFIG:REMAP_REG2", i);
        config->u32Remap2[i] = iniparser_getint(ini, cmd, 0);
        sprintf(cmd, "SIF%d_CONFIG:REMAP_REG3", i);
        config->u32Remap3[i] = iniparser_getint(ini, cmd, 0);

    }
    // }

    config->bIspEnable = iniparser_getint(ini, "ISP_CONFIG:Enable", 0);
    if (config->bIspEnable == 1) {
        sprintf(cmd, "ISP_CONFIG:u32Width");
        config->IspWidth = iniparser_getint(ini, cmd, 0xffff);

        sprintf(cmd, "ISP_CONFIG:u32Height");
        config->IspHeight = iniparser_getint(ini, cmd, 0xffff);
        if ((config->IspWidth < 320) || (config->IspHeight < 240))
        {
            printf("u32WidthL:%d u32Height:%d ERROR\n", config->IspWidth, config->IspHeight);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:enBayer");
        config->IspEnBayer = iniparser_getint(ini, cmd, 0xff);
        if (config->IspEnBayer >= BAYER_BUTT)
        {
            printf("==>attr.enBayer error [%d]\n", config->IspEnBayer);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:oformat");
        config->Ispoformat = iniparser_getint(ini, cmd, 0x5);
        if (config->Ispoformat >= BAYER_BUTT)
        {
            printf("==>attr.oformat error [%d]\n", config->Ispoformat);
        } else {
            config->Ispoformat = FMT_YUV422S;
        }

        sprintf(cmd, "ISP_CONFIG:pixel_width");
        config->IspPixelWidth = iniparser_getint(ini, cmd, 0xff);
        if (config->IspPixelWidth > 16)
        {
            printf("==>attr.pixel_width error [%d]\n", config->IspPixelWidth);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:pixel_rate");
        config->IspPixelRate = iniparser_getint(ini, cmd, 0x0);
        if (config->IspPixelRate > 0xf)
        {
            printf("==>attr.pixel_rate error [%d]\n", config->IspPixelRate);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:enIsprMode");
        config->IsprMode = iniparser_getint(ini, cmd, 0xff);
        if (config->IsprMode >= FMT_BUTT_MODE)
        {
            printf("==>attr.enIsprMode error [%d]\n", config->IsprMode);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:enIspwMode");
        config->IspwMode = iniparser_getint(ini, cmd, 0xff);
        if (config->IspwMode >= FMT_BUTT_MODE)
        {
            printf("==>attr.enIspwMode error [%d]\n", config->IspwMode);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:ispr_buff_num");
        config->IsprBuffNum = iniparser_getint(ini, cmd, 0x0);
        if (config->IsprBuffNum >= 256)
        {
            printf("==>attr.ispr_buff_num error [%d]\n", config->IsprBuffNum);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:ispw_buff_num");
        config->IspwBuffNum = iniparser_getint(ini, cmd, 0x0);
        if (config->IspwBuffNum >= 256)
        {
            printf("==>attr.ispw_buff_num error [%d]\n", config->IspwBuffNum);
            return -2;
        }


        sprintf(cmd, "ISP_CONFIG:enWDRMode");
        config->IspWdrMode = iniparser_getint(ini, cmd, 0xff);
        if (config->IspWdrMode >= WDR_MODE_BUTT)
        {
            printf("==>attr.wdr_mode.enWDRMode error [%d]\n", config->IspWdrMode);
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:input_phyaddr");
        config->IspInputPhyAddr = iniparser_getlonglongint(ini, cmd, 0x0);
        if (config->IspInputPhyAddr == 0)
        {
            printf("=====> ISPR get phyaddr error!\n");
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:output_phyaddr");
        config->IspOutputPhyAddr = iniparser_getlonglongint(ini, cmd, 0x0);
        if (config->IspOutputPhyAddr == 0)
        {
            printf("=====> ISPW get  phyaddr error!\n");
            return -2;
        }

        sprintf(cmd, "ISP_CONFIG:src_filename");
        config->IspSrcFile = (char *)iniparser_getstring(ini, cmd, NULL);

        sprintf(cmd, "ISP_CONFIG:frame_mdelay");
        config->IspFrameDelayms = iniparser_getint(ini, cmd, 0x64);
        if (config->IspFrameDelayms <= 5)
        {
            printf("==>set frame_mdelay error [%d]ms, must > 5ms\n", config->IspFrameDelayms);
            return -2;
        } else {
            printf("==>frame_mdelay [%d]\n", config->IspFrameDelayms);
        }

        sprintf(cmd, "ISP_CONFIG:sif_isp_bind");
        config->sif_isp_bind = iniparser_getint(ini, cmd, 0x0);

        sprintf(cmd, "ISP_CONFIG:IspStepMode");
        config->IspStepMode = iniparser_getint(ini, cmd, 0x0);
    }

	return 0;
}


int isp_vmmz_init(unsigned int size, int max_num, char *poolname, int *pool_id)
{
    int ret = 0;
    VIM_U32 id = 0;

    int pgsize = getpagesize();
    int mem_size = size;

    mem_size = (mem_size + (pgsize - 1)) & (~(pgsize - 1));

    id = VIM_MPI_VB_CreatePool(mem_size, max_num, poolname);
    if (id == VB_INVALID_POOLID)
    {
        printf ("===> Alloc ISP VIDEO MMZ MEMORY Failed\n");
        return -1;
    }
    else
    {
        ret = VIM_MPI_VB_MmapPool(id);
        if (ret)
        {
            printf ("===> VIM_MPI_VB_MmapPool failed\n");
            VIM_MPI_VB_DestroyPool(id);
            return -1;
        }

        *pool_id=id;
    }

    return 0;
}

int isp_vmmz_uninit(int pool_id)
{
    return VIM_MPI_VB_DestroyPool(pool_id);
}

void alloc_mmz_buffer(int pool_id, unsigned int mem_size, void **ptr, int i)
{
    int ret = 0;
    unsigned long int phyaddr = 0;
    VB_BLK vbBlk;

    i = i;

    vbBlk = VIM_MPI_VB_GetBlock(pool_id, mem_size, NULL);
    if (vbBlk == VB_INVALID_HANDLE) {
        VIM_MPI_VB_MunmapPool(pool_id);
        VIM_MPI_VB_DestroyPool(pool_id);
        exit(1);
    }

    phyaddr = VIM_MPI_VB_Handle2PhysAddr(vbBlk);
    if (!phyaddr)
    {
        VIM_MPI_VB_MunmapPool(pool_id);
        VIM_MPI_VB_DestroyPool(pool_id);
        exit(1);
    }

    ret = VIM_MPI_VB_GetBlkVirAddr(pool_id, phyaddr, ptr);
    if (ret)
    {
        printf ("========> VIM_MPI_VB_GetBlkVirAddr error!\n");
        VIM_MPI_VB_MunmapPool(pool_id);
        VIM_MPI_VB_DestroyPool(pool_id);
        exit(1);
    }

}


void *alloc_buffer(unsigned int base_addr, void **ptr, int i)
{
    void *vaddr=NULL;
    int fd;
    unsigned int offsetx = 0;

    if (i!=0)
        offsetx = i * 0x1000000;

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd <= 0)
    {
        perror("open mem");
        exit(1);
    }

    vaddr = mmap(0, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_addr + offsetx);

    if (vaddr == MAP_FAILED)
    {
        perror("Mmap error !\n");
        exit(1);
    }

    *ptr = vaddr;
    printf("=====> Alloc  byffer %08x %p\n", (base_addr + offsetx), vaddr);

    close(fd);

    return vaddr;
}

void fill_buffer(void *y_vptr, void *uv_vptr, int slen, int index)
{
    void *vptr = vsrc_vptr;

    vptr = vptr + slen * 2 * index;

    memcpy(y_vptr, vptr, slen);
    vptr += slen;
    memcpy(uv_vptr, vptr, slen);
}
#if 0
void *isp_ex_send_proc(void *pnum)
{
    //Send Frame case
    SAMPLE_SIF_ISP_PARA_T* DevConfig = (SAMPLE_SIF_ISP_PARA_T*)pnum;
    ISP_FRAMES_INFO_S frameinfo_s[128]={0};
    VIM_VI_FRAME_INFO_S sFrameInfo = {0};
    void *vaddr_x=NULL;
    unsigned long int phyaddr=0;

    int buff_num=0;
    buff_num=DevConfig->IsprBuffNum;
    void *vaddr[buff_num];
    int i = 0, acount=0;
    int pool_id=0;
    char cmd[128]={0};
    int ret=-1;
    int xcnt=0;
    int g_u32Width=DevConfig->IspWidth;
    int g_u32Height=DevConfig->IspHeight;
    int g_pixel_width=DevConfig->IspPixelWidth;
    int g_mdelay=DevConfig->IspFrameDelayms;
    int isp_enbayer=DevConfig->IspEnBayer;
    int pgsize = getpagesize();
    sem_wait(&(g_ispr_sem));
    if(1)
    {
        if (DevConfig->IspSrcFile!=NULL)
            file_init(DevConfig->IspSrcFile);
        else
        {
            printf ("======> file open %s error\n",DevConfig->IspSrcFile);
            exit(-7);
        }
        if (buff_num>=128)
        {
            printf ("======> ERROR: Maximum limit --> Input Frame count < 32\n");
            exit(-8);
        }
    #ifdef MMZ_ALLOC
        ret=isp_vmmz_init(g_u32Width*g_u32Height*3, buff_num, "ispr_video_mem", &pool_id);
        if (ret!=0)
        {
            printf ("===> ISPR vmmz init error!\n");
            return;
        }
    #else
        // sprintf(cmd,"ISP_BASE_CFG:input_phyaddr");
        phyaddr=DevConfig->IspInputPhyAddr;
        if (phyaddr==0)
        {
            printf ("=====> ISPR get phyaddr error!\n");
            return;
        } 
    #endif

        if (DevConfig->IspStepMode)
            sem_wait(&(g_ispr_sem));

        for (i=0;i<buff_num;i++)
        {
            vaddr[i]=NULL;
            vaddr_x=NULL;
    #ifdef MMZ_ALLOC
            alloc_mmz_buffer(pool_id,g_u32Width*g_u32Height*3, &vaddr[i],i);
            vaddr_x=((unsigned long long)vaddr[i])+g_u32Width*g_u32Height; 
            vaddr_x =(void*)(((unsigned int)(vaddr_x) + (pgsize - 1)) & (~(pgsize - 1)));
    #else
            alloc_buffer(phyaddr,&vaddr[i],i);
            vaddr_x=((unsigned long long)vaddr[i])+0x300000; 
    #endif

            frameinfo_s[i].video_attrs.width=g_u32Width;
            frameinfo_s[i].video_attrs.height=g_u32Height;

            if (g_pixel_width==8)
            {
                if (isp_enbayer<4)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth=g_u32Width*g_u32Height/2;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr=vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth=g_u32Width*g_u32Height/2;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr=vaddr_x;                    
                }
                else if (isp_enbayer==4)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth=g_u32Width*g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr=vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth=g_u32Width*g_u32Height/2;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr=vaddr_x;                     
                }
                else if (isp_enbayer==5)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth=g_u32Width*g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr=vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth=g_u32Width*g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr=vaddr_x;                     
                }    
            }
            else
            {
                frameinfo_s[i].video_attrs.addrs.y_lenth=g_u32Width*g_u32Height;
                frameinfo_s[i].video_attrs.addrs.y_vaddr=vaddr[i];
                frameinfo_s[i].video_attrs.addrs.uv_lenth=g_u32Width*g_u32Height;
                frameinfo_s[i].video_attrs.addrs.uv_vaddr=vaddr_x;
            }

            frameinfo_s[i].vb_id=i;

            fill_buffer(frameinfo_s[i].video_attrs.addrs.y_vaddr,frameinfo_s[i].video_attrs.addrs.uv_vaddr,frameinfo_s[i].video_attrs.addrs.y_lenth, i);
            printf ("--->%s i=%d vb_id = %d, ylen:%d, uvlen:%d\n",__func__,i,frameinfo_s[i].vb_id, frameinfo_s[i].video_attrs.addrs.y_lenth,frameinfo_s[i].video_attrs.addrs.uv_lenth);
            VIM_MPI_ISP_SendFrame(0, &frameinfo_s[i]);
            sleep(1);
        }
        printf ("--> VIM_MPI_ISP_SendFrame done, next wait 1\n");
        // sleep(3);
        while (1)
        {
            if(acount>30)
            {
                printf ("===>%s  wait new step signal\n",__func__);
                sem_wait(&(g_ispr_sem));
                acount=0;
            }
            else
                acount++;

            for (i=0;i<buff_num;i++)
                VIM_MPI_ISP_ReleaseSendFrame(0,&frameinfo_s[i]);
            for (i=0;i<buff_num;i++)
            {
                VIM_MPI_ISP_SendFrame(0, &frameinfo_s[i]);
                usleep(g_mdelay*1000);
            }
        }

        file_uninit();
        }

}
#endif
void *isp_send_proc(void *pnum)
{
    //Send Frame case
    SAMPLE_SIF_ISP_PARA_T *DevConfig = (SAMPLE_SIF_ISP_PARA_T *)pnum;
    ISP_FRAMES_INFO_S frameinfo_s[128] = {0};
    VIM_VI_FRAME_INFO_S sFrameInfo = {0};
    void *vaddr_x = NULL;

    int buff_num = 0;

    buff_num = DevConfig->IsprBuffNum;
    void *vaddr[buff_num];
    int i = 0;
    int pool_id = 0;
    int ret = -1;
    int xcnt = 0;
    int g_u32Width = DevConfig->IspWidth;
    int g_u32Height = DevConfig->IspHeight;
    int g_pixel_width = DevConfig->IspPixelWidth;
    int g_mdelay = DevConfig->IspFrameDelayms;
    int isp_enbayer = DevConfig->IspEnBayer;
    int pgsize = getpagesize();

    if ((DevConfig->bSifEnable[0] != 0) || (DevConfig->bSifEnable[1] != 0))
    {
        while(1)
        {
            ret = VIM_MPI_VI_GetFrame(0, &sFrameInfo);
            if (ret!=0)
            {
                printf ("===> VIM_MPI_VI_GetFrame error [%d]!\n",ret);
                if (xcnt==0)
                {
                    xcnt++;
                    sleep(2);
                    continue;
                }
            }

            frameinfo_s[i].video_attrs.width = g_u32Width;
            frameinfo_s[i].video_attrs.height = g_u32Height;

            frameinfo_s[i].video_attrs.addrs.y_lenth = sFrameInfo.u32ByteUsedStd[0];
            frameinfo_s[i].video_attrs.addrs.y_vaddr = sFrameInfo.pVirAddrStd[0];
            frameinfo_s[i].video_attrs.addrs.uv_lenth = sFrameInfo.u32ByteUsedStd[1];
            frameinfo_s[i].video_attrs.addrs.uv_vaddr = sFrameInfo.pVirAddrStd[1];
            frameinfo_s[i].vb_id=sFrameInfo.u32Index;
            if (DevConfig->wdrEnable[0] == 0){
                frameinfo_s[i].wdr_mode = WDR_MODE_NONE;
            } else {
                switch (DevConfig->wdrFrmNum[0]) {
                    case 4:
                        frameinfo_s[i].wdr_mode = WDR_MODE_4To1_LINE;
                        frameinfo_s[i].extend_addrs[0].y_vaddr = sFrameInfo.pVirAddrStd[2];
                        frameinfo_s[i].extend_addrs[0].y_lenth = sFrameInfo.u32ByteUsedStd[2];
                        frameinfo_s[i].extend_addrs[0].uv_vaddr = sFrameInfo.pVirAddrStd[3];
                        frameinfo_s[i].extend_addrs[0].uv_lenth = sFrameInfo.u32ByteUsedStd[3];
                        frameinfo_s[i].extend_addrs[1].y_vaddr = sFrameInfo.pVirAddrStd[4];
                        frameinfo_s[i].extend_addrs[1].y_lenth = sFrameInfo.u32ByteUsedStd[4];
                        frameinfo_s[i].extend_addrs[1].uv_vaddr = sFrameInfo.pVirAddrStd[5];
                        frameinfo_s[i].extend_addrs[1].uv_lenth = sFrameInfo.u32ByteUsedStd[5];
                        frameinfo_s[i].extend_addrs[2].y_vaddr = sFrameInfo.pVirAddrStd[6];
                        frameinfo_s[i].extend_addrs[2].y_lenth = sFrameInfo.u32ByteUsedStd[6];
                        frameinfo_s[i].extend_addrs[2].uv_vaddr = sFrameInfo.pVirAddrStd[7];
                        frameinfo_s[i].extend_addrs[2].uv_lenth = sFrameInfo.u32ByteUsedStd[7];
                        break;
                    case 3:
                        frameinfo_s[i].wdr_mode = WDR_MODE_3To1_LINE;
                        frameinfo_s[i].extend_addrs[0].y_vaddr = sFrameInfo.pVirAddrStd[2];
                        frameinfo_s[i].extend_addrs[0].y_lenth = sFrameInfo.u32ByteUsedStd[2];
                        frameinfo_s[i].extend_addrs[0].uv_vaddr = sFrameInfo.pVirAddrStd[3];
                        frameinfo_s[i].extend_addrs[0].uv_lenth = sFrameInfo.u32ByteUsedStd[3];
                        frameinfo_s[i].extend_addrs[1].y_vaddr = sFrameInfo.pVirAddrStd[4];
                        frameinfo_s[i].extend_addrs[1].y_lenth = sFrameInfo.u32ByteUsedStd[4];
                        frameinfo_s[i].extend_addrs[1].uv_vaddr = sFrameInfo.pVirAddrStd[5];
                        frameinfo_s[i].extend_addrs[1].uv_lenth = sFrameInfo.u32ByteUsedStd[5];
                        break;
                    case 2:
                        frameinfo_s[i].wdr_mode = WDR_MODE_2To1_LINE;
                        frameinfo_s[i].extend_addrs[0].y_vaddr = sFrameInfo.pVirAddrStd[2];
                        frameinfo_s[i].extend_addrs[0].y_lenth = sFrameInfo.u32ByteUsedStd[2];
                        frameinfo_s[i].extend_addrs[0].uv_vaddr = sFrameInfo.pVirAddrStd[3];
                        frameinfo_s[i].extend_addrs[0].uv_lenth = sFrameInfo.u32ByteUsedStd[3];
                        break;
                    default:
                        printf("DevConfig->wdrFrmNum err:%d\r\n", DevConfig->wdrFrmNum[0]);
                    break;
                }
            }

            printf ("--->%s i=%d vb_id = %d, ylen:%d, uvlen:%d\n",
                                __func__, i, frameinfo_s[i].vb_id,
                                frameinfo_s[i].video_attrs.addrs.y_lenth, frameinfo_s[i].video_attrs.addrs.uv_lenth);

            VIM_MPI_ISP_SendFrame(0, &frameinfo_s[i]);
            VIM_MPI_ISP_ReleaseSendFrame(0, &frameinfo_s[i]);
            VIM_MPI_VI_ReleaseFrame(0, sFrameInfo.u32Index);
        }
    } else {
        if (DevConfig->IspSrcFile != NULL) {
            file_init(DevConfig->IspSrcFile);
        } else {
            printf("======> file open %s error\n", DevConfig->IspSrcFile);
            exit(-7);
        }

        if (buff_num >= 128)
        {
            printf("======> ERROR: Maximum limit --> Input Frame count < 32\n");
            exit(-8);
        }

    #ifdef MMZ_ALLOC
        ret = isp_vmmz_init((g_u32Width * g_u32Height * 3), buff_num, "ispr_video_mem", &pool_id);
        if (ret != 0)
        {
            printf("===> ISPR vmmz init error!\n");
            return NULL;
        }
    #else
        VIM_U32 phyaddr = 0;

        phyaddr = DevConfig->IspInputPhyAddr;
        if (phyaddr == 0)
        {
            printf("=====> ISPR get phyaddr error!\n");
            return NULL;
        }
    #endif
        if (DevConfig->IspStepMode)
            sem_wait(&(g_ispr_sem));

        for (i = 0; i < buff_num; i++)
        {
            vaddr[i] = NULL;
            vaddr_x = NULL;
    #ifdef MMZ_ALLOC
            alloc_mmz_buffer(pool_id, g_u32Width * g_u32Height * 3, &vaddr[i], i);
            vaddr_x = (vaddr[i]) + g_u32Width * g_u32Height;
            vaddr_x =(void *)(((unsigned int)(vaddr_x) + (pgsize - 1)) & (~(pgsize - 1)));
    #else
            alloc_buffer(phyaddr, &vaddr[i], i);
            vaddr_x = (vaddr[i]) + 0x300000;
    #endif

            frameinfo_s[i].video_attrs.width = g_u32Width;
            frameinfo_s[i].video_attrs.height = g_u32Height;

            if (g_pixel_width == 8)
            {
                if (isp_enbayer < 4)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth = g_u32Width * g_u32Height / 2;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr = vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth = g_u32Width * g_u32Height / 2;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr = vaddr_x;
                }
                else if (isp_enbayer == 4)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth = g_u32Width * g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr = vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth = g_u32Width * g_u32Height / 2;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr = vaddr_x;
                }
                else if (isp_enbayer == 5)
                {
                    frameinfo_s[i].video_attrs.addrs.y_lenth = g_u32Width * g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.y_vaddr = vaddr[i];
                    frameinfo_s[i].video_attrs.addrs.uv_lenth = g_u32Width * g_u32Height;
                    frameinfo_s[i].video_attrs.addrs.uv_vaddr = vaddr_x;
                }
            }
            else
            {
                frameinfo_s[i].video_attrs.addrs.y_lenth = g_u32Width * g_u32Height;
                frameinfo_s[i].video_attrs.addrs.y_vaddr = vaddr[i];
                frameinfo_s[i].video_attrs.addrs.uv_lenth = g_u32Width * g_u32Height;
                frameinfo_s[i].video_attrs.addrs.uv_vaddr = vaddr_x;
            }

            frameinfo_s[i].vb_id = i;

            fill_buffer(frameinfo_s[i].video_attrs.addrs.y_vaddr,
                frameinfo_s[i].video_attrs.addrs.uv_vaddr, frameinfo_s[i].video_attrs.addrs.y_lenth, i);
            printf ("--->%s i=%d vb_id = %d, ylen:%d, uvlen:%d\n",
                                __func__,i,frameinfo_s[i].vb_id,
                                frameinfo_s[i].video_attrs.addrs.y_lenth, frameinfo_s[i].video_attrs.addrs.uv_lenth);

            VIM_MPI_ISP_SendFrame(0, &frameinfo_s[i]);
            sleep(1);
        }

        printf ("--> VIM_MPI_ISP_SendFrame done\n");
        sleep(1);
        while (1)
        {
            if (DevConfig->IspStepMode)
                sem_wait(&(g_ispr_sem));

            for (i = 0; i < buff_num; i++)
            {
                VIM_MPI_ISP_ReleaseSendFrame(0, &frameinfo_s[i]);
            }

            for (i = 0; i < buff_num; i++)
            {
                VIM_MPI_ISP_SendFrame(0, &frameinfo_s[i]);
                usleep(g_mdelay * 1000);
            }

        }

        file_uninit();
    }

}

static int g_recfd = 0;
static int xrec_file_init(int type, char *filename)
{

    if (type== 0) //First
        g_recfd = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, S_IRWXU);
    else if (type == 1)
        g_recfd = open(filename, O_RDWR | O_CREAT | O_APPEND | O_SYNC, S_IRWXU);
    else
        g_recfd = open(filename, O_RDWR);

    if (g_recfd == -1)
    {
        perror ("open");
        return -1;
    }

    return 0;
}

int xrec_file_append(int type, char *filename, unsigned char *buff, int len)
{

    int ret, seek_len = 0;

    ret = xrec_file_init(type, filename);
    if (ret != 0)
        return ret;
    if (len==0)
        return 0;

    while (1)
    {
        ret = write(g_recfd, buff + seek_len, len - seek_len);
        printf ("==> %s vaddr:%08x, len:%d, ret = %d\n",
                __func__, (unsigned int)(buff + seek_len), (len - seek_len), ret);
        seek_len = seek_len + ret;

        if (ret == -1)
        {
            perror ("FILE APPEND");
            return -1;
        }

        if (seek_len == len)
        {
            break;
        }
    }

    return 0;
}

static void xrec_file_uinit(void)
{
    int ret = 0;

    ret = close(g_recfd);

    if (ret != 0)
        perror("close fd:");
    g_recfd = -1;
}

static int g_ispw_width = 0, g_ispw_height = 0, g_ispw_pool_id = 0;
void *isp_get_proc(void *pnum)
{
    //ISPWRITE CASE
    SAMPLE_SIF_ISP_PARA_T *DevConfig = (SAMPLE_SIF_ISP_PARA_T *)pnum;
    ISP_FRAMES_INFO_S frameinfo[199] = {0};
    ISP_FRAMES_INFO_S frameinfo_ex = {0};
    ISP_FRAMES_INFO_S frameinfo_rs[199] = {0};

    int buff_num = 0;

    buff_num = DevConfig->IspwBuffNum;

    int ret = 0;
    void *vaddr[buff_num * 2];
    void *vaddr_x = NULL;
    int i,j;
    char filename[100] = {0};
    int xcnt = 0;
    int pool_id = 0;
    int g_u32Width = DevConfig->IspWidth;
    int g_u32Height = DevConfig->IspHeight;

    g_ispw_width = g_u32Width;
    g_ispw_height = g_u32Height;

    int init_once = 0;

#ifdef MMZ_ALLOC
    ret=isp_vmmz_init(g_u32Width * g_u32Height, buff_num * 3, "ispw_video_mem", &pool_id);
    if (ret != 0)
    {
        printf ("===> ISPW vmmz init error!\n");
        return NULL;
    }
    g_ispw_pool_id = pool_id;
#else
    char cmd[128];
    VIM_U32 phyaddr = 0;

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "ISP_BASE_CFG:output_phyaddr");
    phyaddr = DevConfig->IspOutputPhyAddr;
    if (phyaddr == 0)
    {
        printf("=====> ISPW get  phyaddr error!\n");
        return NULL;
    }
#endif

    sem_wait(&(g_isp_sem));
    for (i = 0; i < buff_num; i++)
    {
        vaddr[i] = NULL;
        vaddr_x = NULL;
#ifdef MMZ_ALLOC
        alloc_mmz_buffer(pool_id, g_u32Width * g_u32Height, &vaddr[i], i);
        alloc_mmz_buffer(pool_id, g_u32Width * g_u32Height, &vaddr_x, i);
#else
        alloc_buffer(phyaddr, &vaddr[i], i);
        vaddr_x = ((unsigned long long)vaddr[i]) + g_u32Width * g_u32Height;
#endif

        frameinfo[i].video_attrs.width = g_u32Width;
        frameinfo[i].video_attrs.height = g_u32Height;
        frameinfo[i].video_attrs.addrs.y_lenth = g_u32Width * g_u32Height;
        frameinfo[i].video_attrs.addrs.y_vaddr = vaddr[i];
        frameinfo[i].video_attrs.addrs.uv_lenth = g_u32Width * g_u32Height;
        frameinfo[i].video_attrs.addrs.uv_vaddr = vaddr_x;
        frameinfo[i].video_attrs.addrs.plane_num=0x2;

        if (DevConfig->Ispoformat == FMT_YUV420S)
            frameinfo[i].video_attrs.pixel_format = EM_FORMAT_YUV420S_8BIT;
        else
            frameinfo[i].video_attrs.pixel_format = EM_FORMAT_YUV422S_8BIT;

        frameinfo[i].vb_id = i;

        printf ("--->Init ISPW  Req frame mem done, vaddr:%p %p, oformat:%d\n",
                frameinfo[i].video_attrs.addrs.y_vaddr, frameinfo[i].video_attrs.addrs.uv_vaddr, DevConfig->Ispoformat);
    }

    while(1)
    {
        sem_wait(&(g_isp_sem));

        //初始化 ISPW BUFF
        if (init_once == 0)
        {
            VIM_MPI_ISP_FrameMemInit(0, &frameinfo[0], buff_num);
            init_once = 1;
        }
        else
        {
            for (i = 0; i < buff_num; i++)
            {
                if (VIM_MPI_ISP_ReleaseGetFrame(0, &frameinfo_rs[i]) != 0)
                    printf (" VIM_MPI_ISP_ReleaseGetFrame error\n");
            }
        }
        memset (filename, '\0', sizeof(filename));
        sprintf (filename, "ISPW_Record_%d_%d_yuv422s_id_%d.yuv", g_u32Width, g_u32Height, xcnt);
        xcnt++;
        j = 0;

        memset (&frameinfo_rs, '\0', sizeof(frameinfo_rs));

        while (j != buff_num)
        {
            memset(&frameinfo_ex, '\0', sizeof(frameinfo_ex));
            if (VIM_MPI_ISP_GetFrame(0, &frameinfo_ex) == 0)
            {
                printf ("---> id:%d Get frame Y:%p len:%d  %p len:%d\n",
                    frameinfo_ex.vb_id,frameinfo_ex.video_attrs.addrs.y_vaddr,
                    frameinfo_ex.video_attrs.addrs.y_lenth,frameinfo_ex.video_attrs.addrs.uv_vaddr,
                    frameinfo_ex.video_attrs.addrs.uv_lenth);
                memcpy(&frameinfo_rs[j++], &frameinfo_ex, sizeof(frameinfo_ex));
            }
            else
            {
                perror("xx getframe:");
            }
        }

        for (i=0; i < j; i++)
        {
            xrec_file_append(1, filename, frameinfo_rs[i].video_attrs.addrs.y_vaddr,
                                                frameinfo_rs[i].video_attrs.addrs.y_lenth);
            xrec_file_append(1, filename, frameinfo_rs[i].video_attrs.addrs.uv_vaddr,
                                                frameinfo_rs[i].video_attrs.addrs.uv_lenth);
        }
        xrec_file_uinit();
    }
    printf ("---> ISPW WINIT Done\n");
}

VIM_S32 SAMPLE_SIFUnBindISP(VI_DEV ViDev, ISP_DEV IspDev)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type = 0;
#ifndef DOUBLE_MIPI_MODE
    SYS_BD_CHN_S srcchn[1] = {0}, dstchn[1] = {0};

    IspDev = IspDev;

    srcchn[0].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[0].s32DevId = ViDev;
    srcchn[0].s32ChnId = ViDev;

    dstchn[0].enModId = VIM_BD_MOD_ID_ISP;
    dstchn[0].s32DevId = 0;
    dstchn[0].s32ChnId = 0;
    type = VIM_BIND_WK_TYPE_DRV;

    s32Ret = VIM_MPI_SYS_UnBind(1, (SYS_BD_CHN_S *)&srcchn, 1, (SYS_BD_CHN_S *)&dstchn, type);
#else
    SYS_BD_CHN_S srcchn[2] = {0}, dstchn[1] = {0};

    srcchn[0].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[0].s32DevId = 0;
    srcchn[0].s32ChnId = 0;
    srcchn[1].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[1].s32DevId = 1;
    srcchn[1].s32ChnId = 1;

    dstchn[0].enModId = VIM_BD_MOD_ID_ISP;
    dstchn[0].s32DevId = 0;
    dstchn[0].s32ChnId = 0;
    type = VIM_BIND_WK_TYPE_DRV;

    s32Ret = VIM_MPI_SYS_UnBind(2, (SYS_BD_CHN_S *)&srcchn, 1, (SYS_BD_CHN_S *)&dstchn, type);
#endif
	if (s32Ret != 0)
		printf("SIF ISP VIM_MPI_SYS_UnBind fail.\n");

	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__, s32Ret);

	return s32Ret;
}


VIM_S32 SAMPLE_SIFBindISP(VI_DEV ViDev, ISP_DEV IspDev)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type = 0;
#ifndef DOUBLE_MIPI_MODE
    SYS_BD_CHN_S srcchn[1] = {0}, dstchn[1] = {0};

    IspDev = IspDev;

    srcchn[0].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[0].s32DevId = ViDev;
    srcchn[0].s32ChnId = ViDev;

    dstchn[0].enModId = VIM_BD_MOD_ID_ISP;
    dstchn[0].s32DevId = 0;
    dstchn[0].s32ChnId = 0;
    type = VIM_BIND_WK_TYPE_DRV;

    s32Ret=VIM_MPI_SYS_Bind(1, (SYS_BD_CHN_S *)&srcchn, 1, (SYS_BD_CHN_S *)&dstchn, type);
#else
    SYS_BD_CHN_S srcchn[2] = {0}, dstchn[1] = {0};

    srcchn[0].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[0].s32DevId = 0;
    srcchn[0].s32ChnId = 0;
    srcchn[1].enModId = VIM_BD_MOD_ID_VIU;
    srcchn[1].s32DevId = 1;
    srcchn[1].s32ChnId = 1;

    dstchn[0].enModId = VIM_BD_MOD_ID_ISP;
    dstchn[0].s32DevId = 0;
    dstchn[0].s32ChnId = 0;
    type = VIM_BIND_WK_TYPE_DRV;

    s32Ret=VIM_MPI_SYS_Bind(2, (SYS_BD_CHN_S *)&srcchn, 1, (SYS_BD_CHN_S *)&dstchn, type);
#endif
	if (s32Ret != 0)
		printf("SIF ISP VIM_MPI_SYS_Bind fail.\n");

	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__, s32Ret);

	return s32Ret;
}



VIM_S32 SIF_ISP_UnInit(void)
{
    VIM_S32 i = 0;
#ifndef DOUBLIE_MIPI_MODE
    for (i = 0;i < VI_DEV_MAX; i++){
        if (g_DevConfig.bSifEnable[i] == 0)
            continue;
        if (g_DevConfig.OutputMode[i] == E_OUTPUT_TO_ISP)
            continue;
        switch (i) {
            case 0:
                SAMPLE_SIFUnBindISP(0, 0);
                break;
#ifndef BINOCULAR_MODE
            case 1:
                SAMPLE_SIFUnBindISP(1, 0);
                break;
#endif
			default:
                break;
        }
    }
#else
    SAMPLE_SIFUnBindISP(0, 0);
#endif 
    sleep(1);

    sem_destroy(&g_isp_sem);
    sem_destroy(&g_ispr_sem);
    VIM_MPI_ISP_Exit(0);

    VIM_MPI_ISPSnr_UnInit(0);

    for (i = 0; i < VI_DEV_MAX; i++) {
        if (g_DevConfig.bSifEnable[i] == 0)
            continue;
        VIM_MPI_ViSnr_UnInit(i);
        VIM_MPI_VI_DisableChn(i);
        VIM_MPI_VI_SetFrameDepth(i, 0);
        VIM_MPI_VI_DisableDev(i);
        VIM_MPI_VI_ReleaseMedia(i);
    }

    return 0;
}

VIM_S32 isp_alg_mod_ctrl(void *param)
{
    char buff[12];
    char *enptr;
    int hexval = 0;
    int ispdev = 0, ret = 0;
    ISP_MODULE_CTRL_U ModCtrl = {0};
    (void) param;

    while(1)
    {
        printf ("===> Please input cmd <m>:quit  <s>: set isp alg_en <g>:get isp alg_en\n");
        memset(buff, '\0', sizeof(buff));
        fgets(buff, sizeof(buff), stdin);
        fflush(stdin);

        if (buff[0] == 'm')
            break;
        else if (buff[0] == 's')
        {
            printf ("===> Input Hex val\n");
            memset(buff, '\0', sizeof(buff));
            fgets(buff, sizeof(buff), stdin);
            fflush (stdin);

            hexval = strtoll(buff, &enptr, 16);
            ModCtrl.u32Key = hexval;
            ret = VIM_MPI_ISP_SetModuleControl(ispdev, &ModCtrl);
            printf ("====> call VIM_MPI_ISP_SetModuleControl 0x%08x, ret=%d\n", hexval, ret);

            continue;
        }
        else if (buff[0] == 'g')
        {
            ret = VIM_MPI_ISP_GetModuleControl(ispdev, &ModCtrl);
            printf ("====>VIM_MPI_ISP_GetModuleControl ret=%d, ALG_EN=%08x\n", ret, ModCtrl.u32Key);
            continue;
        }
        else
            continue;
    }
    return 0;
}

VIM_S32 isp_ldc_reload(void *param)
{
    char buff[10];
    int ret=0;
    (void) param;
    ISP_MODULE_CTRL_U ModCtrl;

    while (1)
    {
        printf("===> Please input cmd <m>:quit  <n>: call VIM_MPI_ISP_LDC_SWITCH <s>:disable ldc <e>:enable ldc\n");
        usleep(500 * 1000);
        memset(buff, '\0', sizeof(buff));
        fgets(buff, sizeof(buff), stdin);
        fflush(stdin);
        if (buff[0] == 'm')
        {
            return 0;
        }
        else if (buff[0] == 'n')
        {
            printf("====> input ldc id\n");
            memset(buff, '\0', sizeof(buff));
            fgets(buff, sizeof(buff), stdin);
            fflush(stdin);
            ret=VIM_MPI_ISP_LDC_SWITCH(0, atoi(buff));
            printf ("===> call VIM_MPI_ISP_LDC_SWITCH %d, ret=%d\n", atoi(buff),ret);
        }
        else if (buff[0] == 's')
        {
            printf("==> disable ldc\n");
            VIM_MPI_ISP_GetModuleControl(0, &ModCtrl);
            ModCtrl.bitLDC_Enable = 0x0;
            VIM_MPI_ISP_SetModuleControl(0, &ModCtrl);
        }
        else if (buff[0] == 'e')
        {
            printf("==> enable ldc\n");
            VIM_MPI_ISP_GetModuleControl(0, &ModCtrl);
            ModCtrl.bitLDC_Enable = 0x1;
            VIM_MPI_ISP_SetModuleControl(0, &ModCtrl);
        }
    }
    return 0;
}

VIM_S32 ispw_mem_init(void *param)
{
    VIM_U32 i = 0, j = 0, xcnt = 0;
    void *vaddr_x = NULL;
    void *vaddr[g_DevConfig.IspwBuffNum * 2];
    int ret = 0;
    int pool_id = 0;
    char filename[100] = {0};
    ISP_FRAMES_INFO_S frameinfo[199] = {0};
    ISP_FRAMES_INFO_S frameinfo_ex = {0};
    ISP_FRAMES_INFO_S frameinfo_rs[199] = {0};
    char buff[10] = {0};
    (void) param;
    int g_u32Width = g_DevConfig.IspWidth;
    int g_u32Height = g_DevConfig.IspHeight;

    g_ispw_width = g_u32Width;
    g_ispw_height = g_u32Height;

#ifdef MMZ_ALLOC
    ret = isp_vmmz_init(g_u32Width * g_u32Height, g_DevConfig.IspwBuffNum * 2, "ispw_video_mem", &pool_id);
    if (ret != 0)
    {
        printf("===> ISPW vmmz init error!\n");
        return 0;
    }
    g_ispw_pool_id = pool_id;
#else
    char cmd[128];
    VIM_U32 phyaddr = 0;

    memset(cmd, 0x00, sizeof(cmd));

    sprintf(cmd, "ISP_BASE_CFG:output_phyaddr");
    phyaddr = g_DevConfig.IspOutputPhyAddr;
    if (phyaddr == 0)
    {
        printf("=====> ISPW get  phyaddr error!\n");
        return 0;
    }
#endif

    for (i = 0; i < g_DevConfig.IspwBuffNum; i++)
    {
        vaddr[i] = NULL;
        vaddr_x = NULL;
        #ifdef MMZ_ALLOC
        alloc_mmz_buffer(g_ispw_pool_id, g_ispw_width * g_ispw_height, &vaddr[i], i);
        alloc_mmz_buffer(g_ispw_pool_id, g_ispw_width * g_ispw_height, &vaddr_x, i);
        #else
        alloc_buffer(phyaddr, &vaddr[i], i);
        vaddr_x = ((unsigned long long)vaddr[i]) + g_ispw_width * g_ispw_height;
        #endif
        frameinfo[i].video_attrs.width = g_ispw_width;
        frameinfo[i].video_attrs.height = g_ispw_height;
        frameinfo[i].video_attrs.addrs.y_lenth = g_ispw_width * g_ispw_height;
        frameinfo[i].video_attrs.addrs.y_vaddr = vaddr[i];
        frameinfo[i].video_attrs.addrs.uv_lenth = g_ispw_width * g_ispw_height;
        frameinfo[i].video_attrs.addrs.uv_vaddr = vaddr_x;
        if (g_DevConfig.Ispoformat == FMT_YUV420S)
            frameinfo[i].video_attrs.pixel_format = EM_FORMAT_YUV420S_8BIT;
        else
            frameinfo[i].video_attrs.pixel_format = EM_FORMAT_YUV422S_8BIT;

        frameinfo[i].vb_id = i;
        printf ("--->A Init ISPW frame mem done, vaddr:%p %p format:%d\n",
            frameinfo[i].video_attrs.addrs.y_vaddr, frameinfo[i].video_attrs.addrs.uv_vaddr, g_DevConfig.Ispoformat);
    }
    VIM_MPI_ISP_FrameMemInit(0, &frameinfo[0], g_DevConfig.IspwBuffNum);

    printf ("\n\n===> Please input cmd <m>:test isp out frame  <*>:quit\n");
    usleep(500 * 1000);
    memset(buff, '\0', sizeof(buff));
    fgets(buff, sizeof(buff), stdin);
    fflush (stdin);

    if (buff[0] == 'm')
    {

        memset(&frameinfo_rs, '\0', sizeof(frameinfo_rs));
        memset(filename, '\0', sizeof(filename));
        if (g_DevConfig.Ispoformat == FMT_YUV420S)
            sprintf(filename, "ISPW_Record_%d_%d_yuv420s_id_%d.yuv", g_u32Width, g_u32Height,xcnt);
        else
            sprintf(filename, "ISPW_Record_%d_%d_yuv422s_id_%d.yuv", g_u32Width, g_u32Height,xcnt);

        xcnt++;
        j = 0;

        memset(&frameinfo_rs, '\0', sizeof(frameinfo_rs));

        while (j != g_DevConfig.IspwBuffNum)
        {
            memset(&frameinfo_ex, '\0', sizeof(frameinfo_ex));
            if (VIM_MPI_ISP_GetFrame(0, &frameinfo_ex) == 0)
            {
                printf ("---> id:%d Get frame Y:%p len:%d  %p len:%d\n",
                    frameinfo_ex.vb_id,frameinfo_ex.video_attrs.addrs.y_vaddr,
                    frameinfo_ex.video_attrs.addrs.y_lenth, frameinfo_ex.video_attrs.addrs.uv_vaddr,
                    frameinfo_ex.video_attrs.addrs.uv_lenth);

                memcpy(&frameinfo_rs[j++], &frameinfo_ex, sizeof(frameinfo_ex));
            }
            else
            {
                perror("xx getframe:");
            }
        }

        for (i = 0; i < j; i++)
        {
            xrec_file_append(1, filename,
                frameinfo_rs[i].video_attrs.addrs.y_vaddr,frameinfo_rs[i].video_attrs.addrs.y_lenth);
            xrec_file_append(1, filename,
                frameinfo_rs[i].video_attrs.addrs.uv_vaddr,frameinfo_rs[i].video_attrs.addrs.uv_lenth);
        }

        xrec_file_uinit();
        return 0;
    }
    else
        return 0;
}

VIM_S32 isp_send_trigger(void *param)
{
    (void) param;
    sem_post(&(g_ispr_sem));
    printf ("===> %s\n", __func__);
    return 0;
}

VIM_S32 isp_get_trigger(void *param)
{
    (void) param;
    sem_post(&(g_isp_sem));
    printf ("===> %s\n", __func__);
    return 0;
}

VIM_S32 sif_snap_trigger(void *param)
{
    VIM_S32 s32Ret = VIM_SUCCESS, snap_num = 0;
    VIM_VI_FRAME_INFO_S sFrameInfo = {0};
    VIM_VI_VIDEO_PARAMS_S sVideoConfig = {0};
    // VIM_VI_WDR_PARAMS_S sWdrConfig = {0};
    VI_CHN ViChn = 0;
    (void) param;

    printf("Input vi channel: ");
	scanf("%d", &ViChn);
    if((VIM_U32)ViChn > 1){
        printf("vi channel only 0 or 1\r\n");
        return 0;      
    }

    printf("Input snap number:");
	scanf("%d", &snap_num);
    if (snap_num > 1024 || snap_num == 0) {
        printf("snap name need more then 0 and less then 200\r\n");
        return 0;
    }

    s32Ret = VIM_MPI_VI_GetChnAttr(ViChn,&sVideoConfig);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_GetChnAttr err:%#x\r\n", s32Ret);
        return 0;
    }
    if (g_DevConfig.OutputMode[ViChn] == 1) {
        SAMPLE_SIFUnBindISP(ViChn, 0);
    }else{
        sVideoConfig.eOutputMode = E_OUTPUT_TO_MEM;
    }

    s32Ret = VIM_MPI_VI_DisableChn(ViChn);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_DisableChn err:%#x\r\n", s32Ret);
        return 0;
    }

    s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, 0);                        /* 释放buffer */
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetFrameDepth err:%#x\r\n", s32Ret);
        return 0;
    }

    s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, snap_num);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetFrameDepth err:%#x\r\n", s32Ret);
        return 0;
    }

    s32Ret = VIM_MPI_VI_SetChnAttr(ViChn,&sVideoConfig);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetChnAttr err:%#x\r\n", s32Ret);
        return 0;
    }    

    s32Ret = VIM_MPI_VI_EnableChn(ViChn);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_EnableChn err:%#x\r\n", s32Ret);
        return 0;
    }
   
    for (VIM_S32 i = 0; i < snap_num; i++) {
        s32Ret = VIM_MPI_VI_GetMem(ViChn, &sFrameInfo);
        if (s32Ret < 0) {
            printf("VIM_MPI_VI_GetMem err:%#x\r\n", s32Ret);
            return 0;
        }

        s32Ret = VIM_MPI_VI_SaveFile(ViChn, &sFrameInfo, 0);       /* 直通无压缩模式 */
        if (s32Ret < 0) {
            printf("VIM_MPI_VI_SaveFile err:%#x\r\n", s32Ret);
            return 0;
        }

            s32Ret = VIM_MPI_VI_RepayMem(ViChn, sFrameInfo.u32Index, 0);
            if (s32Ret < 0){
                printf("VIM_MPI_VI_RepayMem err:%#x\r\n", s32Ret);
                return 0;
            }
    }
    s32Ret = VIM_MPI_VI_DisableChn(ViChn);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_DisableChn err:%#x\r\n", s32Ret);
        return 0;
    }
    s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, 0);                        /* 释放buffer */
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetFrameDepth err:%#x\r\n", s32Ret);
        return 0;
    }

    if (g_DevConfig.OutputMode[ViChn] == 1){
        SAMPLE_SIFBindISP(ViChn, 0);
    }else{
        sVideoConfig.eOutputMode = E_OUTPUT_TO_ISP;        
    }

    s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, 8);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetFrameDepth err:%#x\r\n", s32Ret);
        return 0;
    }

    s32Ret = VIM_MPI_VI_SetChnAttr(ViChn,&sVideoConfig);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_SetChnAttr err:%#x\r\n", s32Ret);
        return 0;
    }

    s32Ret = VIM_MPI_VI_EnableChn(ViChn);
    if (s32Ret < 0) {
        printf("VIM_MPI_VI_EnableChn err:%#x\r\n", s32Ret);
        return 0;
    }

    return 0;
}

typedef struct sALG_LIB_S
{
    int s32Id;
    char acLibName[20];
} HALG_LIB_S;

void register_aelib(void)
{
    int ret = 0;
    HALG_LIB_S ae_lib = {0};

    ae_lib.s32Id = 0x0;
    memset(ae_lib.acLibName, 0x00, sizeof(ae_lib.acLibName));

    memcpy(ae_lib.acLibName, "vim_ae_lib", strlen("vim_ae_lib"));
    printf ("======>call VIM_MPI_AE_Register [%s]\n", ae_lib.acLibName);
    ret = VIM_MPI_AE_Register(0, (ALG_LIB_S *)&ae_lib);
    printf ("======>call VIM_MPI_AE_Register done: %d\n", ret);
    getchar();
}

VIM_S32 ISPW_Init(VIM_CHAR *configPath)
{
    pthread_t pid_geter;
    static SAMPLE_SIF_ISP_PARA_T DevConfig;
    int ret = 0;

    SAMPLE_Get_InfoFromIni(configPath, &DevConfig);
    sem_init(&g_isp_sem, 0, 0x0);
    sem_init(&g_ispr_sem, 0, 0x0);

    memcpy(&g_DevConfig, &DevConfig, sizeof(SAMPLE_SIF_ISP_PARA_T));
    printf("===> %s %d\n", __func__, __LINE__);
    ret = pthread_create(&pid_geter, NULL, isp_get_proc, (void *)&DevConfig);
    if (ret != 0) {
        printf("pthread_create failed !\r\n");
    }

    return ret;
}

VIM_S32 SAMPLE_VI_ISP_WDR_Ctrl(void *param)
{
    VIM_S32 s32Ret = 0;
	VIM_U32 ViChn,u32wdrmode;
    ISP_WDR_MODE_S stWDRMode = {0};
    (void) param;
	printf("Input vi channel: ");
	scanf("%d", &ViChn);
	if(ViChn>1)
	{
	    printf("sif number error.\n");
		return 0;
	}
	printf("Input wdr mode: ");
	scanf("%d", &u32wdrmode);
	if(u32wdrmode == 0 || u32wdrmode > 2)
	{
	    printf("Set wdr mode err.\n");
		return 0;
	}

	if(g_DevConfig.bSifEnable[ViChn] == 0)
	{
		printf("sif %d not enable.\n", ViChn);
		return 0;
	}

    // stWDRMode.enWDRMode = (u32wdrmode == 1)?WDR_MODE_NONE:(3*(u32wdrmode-2)+2);
    switch(u32wdrmode){
        case 1:
            stWDRMode.enWDRMode = WDR_MODE_NONE;
            break;
        case 2:
            stWDRMode.enWDRMode = WDR_MODE_2To1_LINE;
			break;
        default:
            printf("no suport \r\n");
            return 0;
    }
    VIM_MPI_ISP_SetWDRMode(0,&stWDRMode);

	return s32Ret;    
}

VIM_S32 SAMPLE_VI_Init(VIM_CHAR *configPath)
{
    static SAMPLE_SIF_ISP_PARA_T DevConfig;
    VIM_VI_PHY_PARAMS_S sPhyConfig = {0};
    VIM_VI_VIDEO_PARAMS_S sVideoConfig = {0};
    VIM_VI_WDR_PARAMS_S sWdrConfig = {0};
    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;

    s32Ret = SAMPLE_Get_InfoFromIni(configPath, &DevConfig);

    for (i = 0; i < VI_DEV_MAX; i++) {
        if (DevConfig.bSifEnable[i] != 0) {
            VI_CHN ViChn = i;

            s32Ret = VIM_MPI_VI_GetMedia(ViChn);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_GetMedia err: %#x\r\n", s32Ret);
                return s32Ret;
            }

            sPhyConfig.eInputInterface = DevConfig.InterFace[i];
            sPhyConfig.eInputFormat = (DevConfig.Format[i] == 3) ? E_INPUT_FORMAT_RAW : E_INPUT_FORMAT_YUV;
            sPhyConfig.u32Remap0 = (VIM_U32)DevConfig.u32Remap0;
            sPhyConfig.u32Remap1 = (VIM_U32)DevConfig.u32Remap1;
            sPhyConfig.u32Remap2 = (VIM_U32)DevConfig.u32Remap2;
            sPhyConfig.u32Remap3 = (VIM_U32)DevConfig.u32Remap3;

            s32Ret = VIM_MPI_VI_SetDevAttr(ViChn, &sPhyConfig);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetDevAttr err: %#x\r\n", s32Ret);
                return s32Ret;
            }

            s32Ret = VIM_MPI_VI_EnableDev(ViChn);
            if(s32Ret != VIM_SUCCESS){
                printf("VIM_MPI_VI_EnableDev err: %#x\r\n", s32Ret);
                return s32Ret;
            }

            sVideoConfig.eOutputBits = DevConfig.Bits[i];
            sVideoConfig.eOutputMode = DevConfig.OutputMode[i] ? E_OUTPUT_TO_MEM : E_OUTPUT_TO_ISP;
            sVideoConfig.u32DataFmt = DevConfig.FormatCode[i];
            sVideoConfig.u32Width = DevConfig.SensorWidth[i];
            sVideoConfig.u32Height = DevConfig.SensorHeight[i];
            sVideoConfig.u32CropLeft = DevConfig.CropLeft[i];
            sVideoConfig.u32CropTop = DevConfig.CropTop[i];
            sVideoConfig.u32CropWidth = DevConfig.CropWidth[i];
            sVideoConfig.u32CropHeight = DevConfig.CropHeight[i];
            sVideoConfig.bCropEnable = DevConfig.CropEnable[i];
            sVideoConfig.bLLCEnable = DevConfig.bLLCEnable[i];

            s32Ret = VIM_MPI_VI_SetChnAttr(ViChn, &sVideoConfig);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetChnAttr err: %d\r\n", s32Ret);
                return s32Ret;
            }

            sWdrConfig.bWdrEnable = DevConfig.wdrEnable[i];
            if (sWdrConfig.bWdrEnable == 1) {
                switch (DevConfig.InterFace[i]) {
                    case E_MIPI_INTERFACE:
                        sWdrConfig.eInputInterface = E_MIPI_INTERFACE;
                        sWdrConfig.sMipiConfig.wdr_config.wdr_mode = DevConfig.wdrMode[i];
                        sWdrConfig.sMipiConfig.wdr_config.wdr_num = DevConfig.wdrFrmNum[i];
                        if (DevConfig.SensorNum[i] == SONY_IMX385_MIPI_1080P_25FPS)
                            sWdrConfig.sMipiConfig.wdr_config.ed_col_n = 9;
                        else
                            sWdrConfig.sMipiConfig.wdr_config.ed_col_n = 0;
                    break;
                    case E_LVDS_INTERFACE:
                        sWdrConfig.eInputInterface = E_LVDS_INTERFACE;
                        sWdrConfig.sLvdsConfig.wdr_config.wdr_mode = DevConfig.wdrMode[i];
                        sWdrConfig.sLvdsConfig.wdr_config.wdr_num = DevConfig.wdrFrmNum[i];
                    break;
                    default:
                    break;
                }

                VIM_MPI_VI_SetChnWdr(ViChn, &sWdrConfig);
            }

            s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, 8);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetFrameDepth err: %#x\r\n", s32Ret);
                return s32Ret;
            }

            s32Ret = VIM_MPI_VI_EnableChn(ViChn);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_EnableChn err: %#x\r\n", s32Ret);
                return s32Ret;
            }


            s32Ret = VIM_MPI_ViSnr_Init(ViChn, DevConfig.SensorNum[i],
                NULL, DevConfig.SensorWidth[i], DevConfig.SensorHeight[i]);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_ViSnr_Init err: %#x\r\n", s32Ret);
                return s32Ret;
            }
            printf("\r\n %s:%d SIF Init End\r\n", __func__, __LINE__);
        }
    }

    return 0;
}

VIM_S32 SAMPLE_VI_UnInit(VIM_VOID)
{
    VIM_VI_PHY_PARAMS_S sPhyConfig = {0};
    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_U32 i = 0;

    for (i = 0; i < VI_DEV_MAX; i++) {
        s32Ret = VIM_MPI_VI_GetDevAttr(i, &sPhyConfig);
        if (s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_GetDevAttr err:%#x\r\n", s32Ret);
            return s32Ret;
        }

        if (sPhyConfig.eInputFormat == E_NONE_INPUT_FORMAT)
            continue;

        s32Ret = VIM_MPI_VI_DisableChn(i);
        if (s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_DisableChn err:%#x\r\n", s32Ret);
            return s32Ret;
        }

        s32Ret = VIM_MPI_VI_SetFrameDepth(i, 0);
        if(s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_SetFrameDepth err:%#x\r\n", s32Ret);
            return s32Ret;
        }

        s32Ret = VIM_MPI_VI_DisableDev(i);
        if (s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_DisableDev err:%#x\r\n", s32Ret);
            return s32Ret;
        }

        s32Ret = VIM_MPI_VI_ReleaseMedia(i);
        if (s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_ReleaseMedia err:%#x\r\n", s32Ret);
            return s32Ret;
        }
    }
    return 0;
}


pthread_t isp_pth, pid_lp;

void *get_chninfo(void *param)
{
    VIM_S32 s32Ret = VIM_SUCCESS;
    VIM_VI_QUERY_INFO_S sChnInfo = {0};

    param = param;

    printf("%s:%d start \r\n", __func__, __LINE__);
    while (1) {
        memset(&sChnInfo, 0, sizeof(VIM_VI_QUERY_INFO_S));
        s32Ret = VIM_MPI_VI_Query(0, &sChnInfo);
        if (s32Ret != VIM_SUCCESS) {
            printf("VIM_MPI_VI_Query err: %#x\r\n", s32Ret);
        } else {
            printf("VIM_MPI_VI_Query success: %#x\r\n", s32Ret);
        }
        sleep(1);
    }

    return NULL;
}


VIM_S32 SIF_ISP_Init(VIM_CHAR *configPath)
{
    static SAMPLE_SIF_ISP_PARA_T DevConfig;
    ISP_PUB_ATTR_S attr = {0};
	VIM_S32 s32Ret = VIM_SUCCESS;
	VIM_VI_PHY_PARAMS_S sPhyConfig = {0};
	VIM_VI_VIDEO_PARAMS_S sVideoConfig = {0};
    VIM_VI_WDR_PARAMS_S sWdrConfig = {0};
    pthread_t pid_sender, pid_geter;
    static int run_once = 0;
    int ret = 0, i = 0;

    SAMPLE_Get_InfoFromIni(configPath, &DevConfig);
    sem_init(&g_isp_sem, 0, 0x0);
    sem_init(&g_ispr_sem, 0, 0x0);

    memcpy(&g_DevConfig, &DevConfig, sizeof(SAMPLE_SIF_ISP_PARA_T));
    if (DevConfig.bIspEnable == 1) {

        ret = VIM_MPI_SYS_Init();
        printf ("-->call VIM_MPI_SYS_Init done, ret=%d\n", ret);

        ret=VIM_MPI_ISP_MemInit(0);
        printf ("-->call VIM_MPI_ISP_MemInit done, ret=%d\n", ret);

        attr.stWndRect.s32X = 0;
        attr.stWndRect.s32Y = 0;
        attr.stWndRect.u32Width = DevConfig.IspWidth;
        attr.stWndRect.u32Height = DevConfig.IspHeight;
        attr.stSnsSize.u32Width = attr.stWndRect.u32Width;
        attr.stSnsSize.u32Height = attr.stWndRect.u32Height;
        attr.f32FrameRate = 30;
        attr.enBayer = DevConfig.IspEnBayer;
        attr.pixel_width = DevConfig.IspPixelWidth;
        attr.pixel_rate = DevConfig.IspPixelRate;
        attr.b_cmp_enbale = DevConfig.bLLCEnable[0];
        attr.enIsprMode = DevConfig.IsprMode;//FMT_DIRECT_MODE;
        attr.enIspwMode = DevConfig.IspwMode;
        attr.ispr_buff_num = DevConfig.IsprBuffNum;
        attr.ispw_buff_num = DevConfig.IspwBuffNum;

        attr.wdr_mode.enWDRMode = DevConfig.IspWdrMode;//WDR_MODE_2To1_LINE;
        ret = VIM_MPI_ISP_SetPubAttr(0, &attr);
        printf("---> call VIM_MPI_ISP_SetPubAttr ret %d\n", ret);
        ret = VIM_MPI_ISP_GetPubAttr(0, &attr);
        printf("---> call VIM_MPI_ISP_GetPubAttr ret %d\n", ret);
        VIM_MPI_ISPSnr_Init(0,DevConfig.bSifEnable[0]?DevConfig.SensorNum[0]:(DevConfig.bSifEnable[1]?DevConfig.SensorNum[1]:DevConfig.SensorNum[0]), NULL);
        printf("========>call VIM_MPI_ISPSnr_Init\n");

        ret = VIM_MPI_ISP_Init(0);
        printf("---> call VIM_MPI_ISP_Init ret %d\n", ret);
        printf("\r\n %s:%d ISP Init End \r\n", __func__, __LINE__);

    #ifdef ALG_EN
		if((DevConfig.bSifEnable[0]?DevConfig.SensorNum[0]:(DevConfig.bSifEnable[1]?DevConfig.SensorNum[1]:DevConfig.SensorNum[0])) != 0){
			ret = pthread_create(&isp_pth, NULL, (void *)VIM_MPI_ISP_Run, NULL);
        	if (ret != 0) {
        	    printf("pthread_create failed !\r\n");
        	}
        	pthread_detach(isp_pth);

			ret = pthread_create(&pid_lp, NULL, (void *)VIM_MPI_ISP_LowPriorityRun, NULL);
			if (ret != 0) {
        	    printf("pthread_create lp failed !\r\n");
			}
        	pthread_detach(pid_lp);
		}
    #endif
        if (run_once == 0)
        {
            // if ((attr.enIsprMode == FMT_MEMORY_MODE))
            if ((attr.enIsprMode == FMT_MEMORY_MODE)&& (DevConfig.sif_isp_bind == 0))

            {
                // ret = pthread_create(&pid_sender, NULL, isp_ex_send_proc, (void *)&DevConfig);
                ret = pthread_create(&pid_sender, NULL, isp_send_proc, (void *)&DevConfig);

                if (ret != 0) {
                    printf("pthread_create failed !\r\n");
                }
            }

            sleep(1);

            if (attr.enIspwMode == FMT_MEMORY_MODE) {
                ret = pthread_create(&pid_geter, NULL, isp_get_proc, (void *)&DevConfig);
                if (ret != 0) {
                    printf("pthread_create failed !\r\n");
                }
            }

            run_once++;
        }

    }
#ifndef DOUBLE_MIPI_MODE
    /* binder init */
    for (i = 0; i < VI_DEV_MAX; i++) {
        if (DevConfig.bSifEnable[i] == 0)
            continue;
        if (DevConfig.OutputMode[i] == E_OUTPUT_TO_ISP)
            continue;
        switch (i) {
            case 0:
                SAMPLE_SIFBindISP(0, 0);
                break;
#ifndef BINOCULAR_MODE
            case 1:
                SAMPLE_SIFBindISP(1, 0);
                break;
#endif
			default:
                break;
        }
    }
#else
    SAMPLE_SIFBindISP(0, 0);
#endif


    /* sif init */
    for (i = 0; i < VI_DEV_MAX; i++){
        if (DevConfig.bSifEnable[i] != 0) {
            VI_CHN ViChn = i;

            s32Ret = VIM_MPI_VI_GetMedia(ViChn);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_GetMedia err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            sPhyConfig.eInputInterface = DevConfig.InterFace[i];
            sPhyConfig.eInputFormat = (DevConfig.Format[i] == 3) ? E_INPUT_FORMAT_RAW : E_INPUT_FORMAT_YUV;
            sPhyConfig.u32Remap0 = (VIM_U32)DevConfig.u32Remap0;
            sPhyConfig.u32Remap1 = (VIM_U32)DevConfig.u32Remap1;
            sPhyConfig.u32Remap2 = (VIM_U32)DevConfig.u32Remap2;
            sPhyConfig.u32Remap3 = (VIM_U32)DevConfig.u32Remap3;

            s32Ret = VIM_MPI_VI_SetDevAttr(ViChn, &sPhyConfig);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetDevAttr err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            s32Ret = VIM_MPI_VI_EnableDev(ViChn);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_EnableDev err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            sVideoConfig.eOutputBits = DevConfig.Bits[i];
            sVideoConfig.eOutputMode = DevConfig.OutputMode[i] ? E_OUTPUT_TO_MEM : E_OUTPUT_TO_ISP;
            sVideoConfig.u32DataFmt = DevConfig.FormatCode[i];
            sVideoConfig.u32Width = DevConfig.SensorWidth[i];
            sVideoConfig.u32Height = DevConfig.SensorHeight[i];
            sVideoConfig.u32CropLeft = DevConfig.CropLeft[i];
            sVideoConfig.u32CropTop = DevConfig.CropTop[i];
            sVideoConfig.u32CropWidth = DevConfig.CropWidth[i];
            sVideoConfig.u32CropHeight = DevConfig.CropHeight[i];
            sVideoConfig.bCropEnable = DevConfig.CropEnable[i];
            sVideoConfig.bLLCEnable = DevConfig.bLLCEnable[i];
            s32Ret = VIM_MPI_VI_SetChnAttr(ViChn, &sVideoConfig);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetChnAttr err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            sWdrConfig.bWdrEnable = DevConfig.wdrEnable[i];
            if (sWdrConfig.bWdrEnable == 1) {
                switch (DevConfig.InterFace[i]) {
                    case E_MIPI_INTERFACE:
                        sWdrConfig.eInputInterface = E_MIPI_INTERFACE;
                        sWdrConfig.sMipiConfig.wdr_config.wdr_mode = DevConfig.wdrMode[i];
                        sWdrConfig.sMipiConfig.wdr_config.wdr_num = DevConfig.wdrFrmNum[i];
                        if (DevConfig.SensorNum[i] == SONY_IMX385_MIPI_1080P_25FPS)
                            sWdrConfig.sMipiConfig.wdr_config.ed_col_n = 9;
                        else
                            sWdrConfig.sMipiConfig.wdr_config.ed_col_n = 0;
                    break;
                    case E_LVDS_INTERFACE:
                        sWdrConfig.eInputInterface = E_LVDS_INTERFACE;
                        sWdrConfig.sLvdsConfig.wdr_config.wdr_mode = DevConfig.wdrMode[i];
                        sWdrConfig.sLvdsConfig.wdr_config.wdr_num = DevConfig.wdrFrmNum[i];
                    break;
                    default:
                    break;
                }
                VIM_MPI_VI_SetChnWdr(ViChn, &sWdrConfig);
            }

            s32Ret = VIM_MPI_VI_SetFrameDepth(ViChn, 8);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_SetFrameDepth err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            s32Ret = VIM_MPI_VI_EnableChn(ViChn);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_VI_EnableChn err: %#x\r\n", s32Ret);
                goto unbinder;
            }

            s32Ret = VIM_MPI_ViSnr_Init(ViChn, DevConfig.SensorNum[i],
                    NULL, DevConfig.SensorWidth[i], DevConfig.SensorHeight[i]);
            if (s32Ret != VIM_SUCCESS) {
                printf("VIM_MPI_ViSnr_Init err: %#x\r\n", s32Ret);
                goto unbinder;
            }
            printf("\r\n %s:%d SIF Init End \r\n", __func__, __LINE__);
        }

    }

    printf("%s:%d Preview Init End \r\n", __func__, __LINE__);
	return 0;
unbinder:
    SIF_ISP_UnInit();
    return s32Ret;
}




