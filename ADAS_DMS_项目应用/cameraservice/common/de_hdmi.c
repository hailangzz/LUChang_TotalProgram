#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "vim_type.h"
#include "de_hdmi.h"
#include <stdlib.h>
#include "vim_comm_gpio.h"

#define IIC2_PATH "/dev/i2c-2"
#define I2C_SMBUS_READ 1
#define I2C_SMBUS_WRITE 0
#define I2C_FUNCS 0x0705
#define I2C_SLAVE_FORCE 0x0706
#define I2C_RDWR 0x0707
#define Debug_Printf printf
///< LT8618SX RGB

#define _LT8618SX_ADR 0x72
#ifdef POLARIS_BD_DEVELOP
#define RESET_CHIP_GPIO AID_GPIO_ID_A0
#else
#define RESET_CHIP_GPIO AID_GPIO_ID_A14
#endif

#define _Ver_U3_ ///< LT8618SXB IC version

#define return_log(x)                                                         \
	{                                                                         \
		if (x < 0)                                                            \
		{                                                                     \
			printf("error file:%s, line:%d,ret:%d\n", __FILE__, __LINE__, x); \
		}                                                                     \
		return (x);                                                           \
	}

struct i2c_ioctl_rdwr_data
{
	struct i2c_msg *msgs; /* ptr to array of simple messages */
	int nmsgs;			  /* number of messages to exchange */
} msgst;

struct i2c_msg
{
	VIM_U16 addr;
	unsigned short flags;
	short len;
	VIM_S8 *buf;
};

/**************************************
 * CI2CA            I2C address
 * -----------------------
 * (0~400mV)----------0x72(default)
 * (400mV~800mV)------0x7a
 * (800mV~1200mV)-----0x90
 * (1200mV~1600mV)	----0x92
 *
 * (2000mV~2400mV)----0x94
 * (2400mV~2800mV)----0x7e
 *(2800mV~3300mV)----0x76
 **************************************/

/***************************************************
 *  The lowest bit 0 of the IIC address 0x72 (0x76) of LT8618SXB is the read-write flag bit.
 *  In the case of Linux IIC, the highest bit is the read-write flag bit,
 *  The IIC address needs to be moved one bit to the right, and the IIC address becomes 0x39 (0x3B).
 *
 *  IIC rate should not exceed 100K.
 *
 *  If the IIC address of LT8618SXB is not 0x72, you need to reset LT8618SXB with the master GPIO,
 *  pull down 100ms, then pull up, delay 100ms, and initialize the LT8618SXB register.
 *
 ****************************************************/

void LT8618SXB_I2C_Write_Byte(unsigned char RegAddr, unsigned char data); ///< IIC write operation
unsigned int LT8618SXB_I2C_Read_Byte(unsigned char RegAddr);			  ///< IIC read operation

///< 1Reset LT8618SX

///< 2LT8618SX Initial setting:

///< #define _LT8618_HDCP_ //If HDMI encrypted output is not required, mask this macro definition

///<#define _Embedded_sync_         ///< Use the synchronization signal inside the bt1120 signal
#define _External_sync_ ///< need to provide external h sync, V sync and de signals to lt8618sxb

///< If bt1120 is SDR mode, the input CLK is pixel CLK; if it is DDR mode, the input CLK is half of pixel CLK.
int Use_DDRCLK; ///< 1: DDR mode; 0: SDR (normal) mode

///< BT1120 Mapping(embedded sync): Y and C can be swapped

/*******************************************************************************
 *Output		16-bit		16-bit 		20-bit		24-bit
 *Port		Mapping1	Mapping2	Mapping		Mapping
 *-------|------------|---------|---------|---------|-----------|---------|---------
 *D0 			Y[0]		X			X	 		Y[0]
 *D1 			Y[1]		X			X 			Y[1]
 *D2 			Y[2]		X			Y[0] 		Y[2]
 *D3 			Y[3]		X			Y[1] 		Y[3]
 *D4 			Y[4]		X			Y[2] 		Y[4]
 *D5 			Y[5]		X			Y[3] 		Y[5]
 *D6 			Y[6]		X			Y[4] 		Y[6]
 *D7 			Y[7]		X			Y[5] 		Y[7]
 *D8 			C[0]		Y[0] 		Y[6] 		Y[8]
 *D9 			C[1]		Y[1] 		Y[7] 		Y[9]
 *D10			C[2]		Y[2] 		Y[8]		Y[10]
 *D11 		C[3]		Y[3] 		Y[9]		Y[11]
 *D12 		C[4]		Y[4] 		X			C[0]
 *D13 		C[5]		Y[5] 		X			C[1]
 *D14 		C[6]		Y[6] 		C[0]		C[2]
 *D15 		C[7]		Y[7] 		C[1]		C[3]
 *D16 		X			C[0]		C[2]		C[4]
 *D17 		X			C[1]		C[3]		C[5]
 *D18 		X			C[2]		C[4]		C[6]
 *D19 		X			C[3]		C[5]		C[7]
 *D20 		X			C[4]		C[6]		C[8]
 *D21 		X			C[5]		C[7]		C[9]
 *D22 		X			C[6]		C[8]		C[10]
 *D23 		X			C[7]		C[9]		C[11]
 *
 *HS			X			X			X			X			X
 *VS			X			X			X			X			X
 *DE			X			X			X			X			X
 *******************************************************************************/

///< YC422 Mapping(Separate  sync): ///< _External_sync_;
/*******************************************************************************
 *Output		16-bit		16-bit 		20-bit		24-bit
 *Port		Mapping1	Mapping2	Mapping		Mapping
 *-------|------------|---------|---------|---------|-----------|---------|---------
 *D0          Y[0]		X			X	 		Y[0]
 *D1 			Y[1]		X			X 			Y[1]
 *D2 			Y[2]		X			Y[0] 		Y[2]
 *D3 			Y[3]		X			Y[1] 		Y[3]
 *D4 			Y[4]		X			X 			C[0]
 *D5 			Y[5]		X			X 			C[1]
 *D6 			Y[6]		X			C[0] 		C[2]
 *D7 			Y[7]		X			C[1] 		C[3]
 *D8 			C[0]		Y[0] 		Y[2] 		Y[4]
 *D9 			C[1]		Y[1] 		Y[3] 		Y[5]
 *D10			C[2]		Y[2] 		Y[4]		Y[6]
 *D11 		C[3]		Y[3] 		Y[5]		Y[7]
 *D12 		C[4]		Y[4] 		Y[6]		Y[8]
 *D13 		C[5]		Y[5] 		Y[7]		Y[9]
 *D14 		C[6]		Y[6] 		Y[8]		Y[10]
 *D15 		C[7]		Y[7] 		Y[9]		Y[11]
 *D16 		X			C[0]		C[2]		C[4]
 *D17 		X			C[1]		C[3]		C[5]
 *D18 		X			C[2]		C[4]		C[6]
 *D19 		X			C[3]		C[5]		C[7]
 *D20 		X			C[4]		C[6]		C[8]
 *D21 		X			C[5]		C[7]		C[9]
 *D22 		X			C[6]		C[8]		C[10]
 *D23 		X			C[7]		C[9]		C[11]
 *
 *HS			Hsync		Hsync		Hsync		Hsync		Hsync
 *VS			Vsync		Vsync		Vsync		Vsync		Vsync
 *DE			DE			DE			DE			DE			DE
 *******************************************************************************/

#define _16bit_ ///< 16bit YC
///<#define _20bit_  ///< 20bit YC
///<#define _24bit_ ///< 24bit YC

#ifdef _16bit_
///< BT1120 16bit

///< BT1120 input from D0 to D15 of LT8618SXB pins. ///< D0 ~ D7 Y ; D8 ~ D15 C
#define _D0_D15_In_ 0x30

///< BT1120 input from D8 to D23 of LT8618SXB pins. ///< D8 ~ D15 Y ; D16 ~ D23 C
#define _D8_D23_In_ 0x70

///< BT1120 input from D0 to D15 of LT8618SXB pins. ///< D0 ~ D7 C ; D8 ~ D15 Y
#define _D0_D15_In_2_ 0x00

///< BT1120 input from D8 to D23 of LT8618SXB pins. ///< D8 ~ D15 C ; D16 ~ D23 Y
#define _D8_D23_In_2_ 0x60

#define _YC_Channel_ _D0_D15_In_2_

#define _Reg0x8246_ 0x00

///< bit1/bit0 = 0:Input data color depth is 8 bit enable for BT
#define _Reg0x8248_D1_D0_ 0x00
#else

#ifdef _Embedded_sync_

///< 20bit
#ifdef _20bit_

///< BT1120 input from D2 ~ D11 Y & D14 ~ D23 C of LT8618SXB pins.
#define _D2_D23_In_ 0x0b

///< BT1120 input from D2 ~ D11 C & D14 ~ D23 Y of LT8618SXB pins.
#define _D2_D23_In_2_ 0x00

#define _YC_Channel_ 0x00
#define _Reg0x8246_ _D2_D23_In_2_ ///< input setting

///< bit1 = 0;bit0 = 1: Input data color depth is 10 bit enable for BT
#define _Reg0x8248_D1_D0_ 0x01

#else
///< 24bit
///<#define _D0_D23_In_		0x0b    ///< BT1120 input from D0 ~ D11 Y & D12 ~ D23 C of LT8618SXB pins.
///<#define _D0_D23_In_2_	0x00    ///< BT1120 input from D0 ~ D11 C & D12 ~ D23 Y of LT8618SXB pins.

///<#define _YC_Channel_ 0x00
///<#define _Reg0x8246_ _D0_D23_In_2_  ///< input setting

///<#define _Reg0x8248_D1_D0_ 0x02  ///< bit1 = 1;bit0 = 0: Input data color depth is 12 bit enable for BT

#endif

#else ///< YC422 Mapping(Separate  sync): ///< _External_sync_;

///< BT1120 20bit/24bit

///< BT1120 10bit input from D2 ~ D3(Y0~Y1),D6 ~ D7(C0~C1),D8 ~ D15(Y2~Y9),D16 ~ D23(C2~C9) of LT8618SXB.
#define _YC_swap_en_ 0x70
///< BT1120 12bit input from D0 ~ D3(Y0~Y3),D4 ~ D7(C0~C3),D8 ~ D15(Y4~Y11),D16 ~ D23(C4~C11) of LT8618SXB.

///< BT1120 10bit input from D2 ~ D3(C0~C1),D6 ~ D7(Y0~Y1),D8 ~ D15(C2~C9),D16 ~ D23(Y2~Y9) of LT8618SXB.
#define _YC_swap_dis_ 0x60
///< BT1120 12bit input from D0 ~ D3(C0~C3),D4 ~ D7(Y0~Y3),D8 ~ D15(C4~C11),D16 ~ D23(Y4~Y11) of LT8618SXB.

#define _YC_Channel_ _YC_swap_dis_ ///< input setting

#define _Reg0x8246_ 0X00

#ifdef _20bit_
#define _Reg0x8248_D1_D0_ 0x01 ///< bit1 = 0;bit0 = 1: Input data color depth is 10 bit enable for BT
#else						   ///< 24bit
#define _Reg0x8248_D1_D0_ 0x02 ///< bit1 = 1;bit0 = 0: Input data color depth is 12 bit enable for BT
#endif

#endif
#endif

#ifdef _Read_TV_EDID_

unsigned char Sink_EDID[256];

unsigned char Sink_EDID2[256];

#endif

unsigned char I2CADR;

unsigned char VIC_Num; ///< vic ,0x10: 1080P ;  0x04 : 720P ;

int flag_Ver_u3;

enum
{
	_32KHz = 0,
	_44d1KHz,
	_48KHz,

	_88d2KHz,
	_96KHz,
	_176Khz,
	_196KHz
};

unsigned short IIS_N[] =
	{
		4096,  ///< 32K
		6272,  ///< 44.1K
		6144,  ///< 48K
		12544, ///< 88.2K
		12288, ///< 96K
		25088, ///< 176K
		24576  ///< 196K
};

unsigned short Sample_Freq[] =
	{
		0x30, ///< 32K
		0x00, ///< 44.1K
		0x20, ///< 48K
		0x80, ///< 88.2K
		0xa0, ///< 96K
		0xc0, ///< 176K
		0xe0  ///<  196K
};

#ifdef _External_sync_
unsigned short hfp, hs_width, hbp, h_act, h_tal, v_act, v_tal, vfp, vs_width, vbp;
int hs_pol, vs_pol;
#else
unsigned short h_act, h_tal, v_act;
#endif

unsigned long CLK_Cnt;

///<--------------------------------------------------------///<

unsigned char CLK_bound;
///<#define CLK_bound _Bound_60_90M  ///< SDR:720P 60/50Hz; DDR:1080P 60/50Hz. -- 74.25M

///<#ifdef _Ver_U3_
static unsigned char LT8618SXB_PLL_u3[3][3] =
	{
		{0x00, 0x9e, 0xaa}, ///< < 50MHz
		{0x00, 0x9e, 0x99}, ///< 50 ~ 100M
		{0x00, 0x9e, 0x88}, ///< > 100M
};
///<#else ///< u2 version
static unsigned char LT8618SXB_PLL_u2[3][3] =
	{
		{0x00, 0x94, 0xaa}, ///< < 50MHz
		{0x01, 0x94, 0x99}, ///< 50 ~ 100M
		{0x03, 0x94, 0x88}, ///< > 100M
};

///<#endif
///<#endif

enum
{
	_Less_than_50M = 0x00,
	///<	_Bound_25_50M,
	_Bound_50_100M,
	_Greater_than_100M
};

unsigned char Resolution_Num;

#define _16_9_ 0x2A ///< 16:9
#define _4_3_ 0x19	///< 4:3

static int Format_Timing[][14] =
	{
		///< H_FP / H_sync / H_BP / H_act / H_total / V_FP / V_sync / V_BP / V_act / V_total / Vic / Pic_Ratio / Clk_bound(SDR) / Clk_bound(DDR)
		///< 480P 60Hz 27MHz 4:3
		///<{ 16,	62,  60,  720,	858,  9, 6, 30, 480,  525,	3,	_16_9_, _Less_than_50M, 	_Less_than_50M	   },	///< 480P 60Hz 27MHz 16:9
		{16, 62, 60, 720, 858, 9, 6, 30, 480, 525, 2, _4_3_, _Less_than_50M, _Less_than_50M},

		///< 576P 50Hz 27MHz 4:3
		///<{ 12,	64,  68,  720,	864,  5, 5, 39, 576,  625,	18, _16_9_, _Less_than_50M, 	_Less_than_50M	   },	///< 576P 50Hz 27MHz 16:9
		{12, 64, 68, 720, 864, 5, 5, 39, 576, 625, 17, _4_3_, _Less_than_50M, _Less_than_50M},

		{110, 40, 220, 1280, 1650, 5, 5, 20, 720, 750, 4, _16_9_, _Bound_50_100M, _Less_than_50M},	 ///< 720P 60Hz 74.25MHz
		{440, 40, 220, 1280, 1980, 5, 5, 20, 720, 750, 19, _16_9_, _Bound_50_100M, _Less_than_50M},	 ///< 720P 50Hz 74.25MHz
		{1760, 40, 220, 1280, 3300, 5, 5, 20, 720, 750, 62, _16_9_, _Bound_50_100M, _Less_than_50M}, ///< 720P 30Hz 74.25MHz
		{2420, 40, 220, 1280, 3960, 5, 5, 20, 720, 750, 61, _16_9_, _Bound_50_100M, _Less_than_50M}, ///< 720P 25Hz 74.25MHz

		{88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125, 16, _16_9_, _Greater_than_100M, _Bound_50_100M},  ///< 1080P  60Hz 148.5MHz
		{528, 44, 148, 1920, 2640, 4, 5, 36, 1080, 1125, 31, _16_9_, _Greater_than_100M, _Bound_50_100M}, ///< 1080P  50Hz 148.5MHz
		{88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125, 34, _16_9_, _Bound_50_100M, _Less_than_50M},	  ///< 1080P  30Hz 74.25MHz
		{528, 44, 148, 1920, 2640, 4, 5, 36, 1080, 1125, 33, _16_9_, _Bound_50_100M, _Less_than_50M},	  ///< 1080P  25Hz 74.25MHz
																									  ///<{ 638,	44,	 148, 1920, 2750, 4, 5,	 36, 1080, 1125, 33, _16_9_, _Bound_50_100M,	 _Less_than_50M		},  ///< 1080P  24Hz 74.25MHz

		{88, 44, 148, 1920, 2200, 2, 5, 15, 540, 562, 5, _16_9_, _Bound_50_100M, _Less_than_50M},	///< 1080i  60Hz 74.25MHz
		{528, 44, 148, 1920, 2640, 2, 5, 25, 540, 562, 20, _16_9_, _Bound_50_100M, _Less_than_50M}, ///< 1080i  50Hz 74.25MHz

		{176, 88, 296, 3840, 4400, 8, 10, 72, 2160, 2250, 95, _16_9_, _Greater_than_100M, _Greater_than_100M}, ///< 4K 30Hz	297MHz

		{40, 128, 88, 800, 1056, 1, 4, 23, 600, 628, 0, _4_3_, _Less_than_50M, _Less_than_50M},	  ///< VESA 800x600 40MHz
		{24, 136, 160, 1024, 1344, 3, 6, 29, 768, 806, 0, _4_3_, _Bound_50_100M, _Less_than_50M}, ///< VESA 1024X768 65MHz
};

enum
{
	H_fp = 0,
	H_sync,
	H_bp,
	H_act,
	H_tol,

	V_fp,
	V_sync,
	V_bp,
	V_act,
	V_tol,

	Vic,

	Pic_Ratio, ///< Image proportion

	Clk_bound_SDR, ///< SDR
	Clk_bound_DDR  ///< DDR
};

enum
{
	_480P60_ = 0,
	_576P50_,

	_720P60_,
	_720P50_,
	_720P30_,
	_720P25_,

	_1080P60_,
	_1080P50_,
	_1080P30_,
	_1080P25_,

	_1080i60_,
	_1080i50_,

	_4K30_,

	_800x600P60_,
	_1024x768P60_
};

#ifdef _LT8618_HDCP_
/***********************************************************/

void LT8618SXB_HDCP_Init(void) ///< luodexing
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x85);
	LT8618SXB_I2C_Write_Byte(0x03, 0xc2);
	LT8618SXB_I2C_Write_Byte(0x07, 0x1f);

	LT8618SXB_I2C_Write_Byte(0xff, 0x80);
	LT8618SXB_I2C_Write_Byte(0x13, 0xfe);
	LT8618SXB_I2C_Write_Byte(0x13, 0xff);

	LT8618SXB_I2C_Write_Byte(0x14, 0x00);
	LT8618SXB_I2C_Write_Byte(0x14, 0xff);

	LT8618SXB_I2C_Write_Byte(0xff, 0x85);
	LT8618SXB_I2C_Write_Byte(0x07, 0x1f);

	///< [7]=force_hpd, [6]=force_rsen, [5]=vsync_pol, [4]=hsync_pol,
	LT8618SXB_I2C_Write_Byte(0x13, 0xfe);

	///< [7]=ri_short_read, [3]=sync_pol_mode, [2]=srm_chk_done,
	LT8618SXB_I2C_Write_Byte(0x17, 0x0f);
	///< [1]=bksv_srm_pass, [0]=ksv_list_vld
	LT8618SXB_I2C_Write_Byte(0x15, 0x05);

	///< [7]=key_ddc_st_sel, [6]=tx_hdcp_en,[5]=tx_auth_en, [4]=tx_re_auth
	///< LT8618SXB_I2C_Write_Byte(0x15,0x65);
}

/***********************************************************/

void LT8618SXB_HDCP_Enable(void) ///< luodexing
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x80);
	LT8618SXB_I2C_Write_Byte(0x14, 0x00);
	LT8618SXB_I2C_Write_Byte(0x14, 0xff);

	LT8618SXB_I2C_Write_Byte(0xff, 0x85);
	LT8618SXB_I2C_Write_Byte(0x15, 0x01); ///< disable HDCP
	LT8618SXB_I2C_Write_Byte(0x15, 0x71); ///< enable HDCP
	LT8618SXB_I2C_Write_Byte(0x15, 0x65); ///< enable HDCP
}

/***********************************************************/

void LT8618SXB_HDCP_Disable(void) ///< luodexing
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x85);
	LT8618SXB_I2C_Write_Byte(0x15, 0x45); ///< enable HDCP
}

#endif

void LT8618SXB_Read_EDID(void)
{
#ifdef _Read_TV_EDID_

	unsigned char i, j;
	unsigned char extended_flag = 0x00;

	LT8618SXB_I2C_Write_Byte(0xff, 0x85);
	///< LT8618SXB_I2C_Write_Byte(0x02,0x0a); //I2C 100K
	LT8618SXB_I2C_Write_Byte(0x03, 0xc9);
	LT8618SXB_I2C_Write_Byte(0x04, 0xa0); ///< 0xA0 is EDID device address
	LT8618SXB_I2C_Write_Byte(0x05, 0x00); ///< 0x00 is EDID offset address
	LT8618SXB_I2C_Write_Byte(0x06, 0x20); ///< length for read
	LT8618SXB_I2C_Write_Byte(0x14, 0x7f);

	for (i = 0; i < 8; i++) ///< block 0 & 1
	{
		LT8618SXB_I2C_Write_Byte(0x05, i * 32); ///< 0x00 is EDID offset address
		LT8618SXB_I2C_Write_Byte(0x07, 0x36);
		LT8618SXB_I2C_Write_Byte(0x07, 0x34);	  ///< 0x31
		LT8618SXB_I2C_Write_Byte(0x07, 0x37);	  ///< 0x37
		usleep(5 * 1000);						  ///< wait 5ms for reading edid data.
		if (LT8618SXB_I2C_Read_Byte(0x40) & 0x02) ///< KEY_DDC_ACCS_DONE=1
		{
			if (LT8618SXB_I2C_Read_Byte(0x40) & 0x50) ///< DDC No Ack or Abitration lost
			{
				printf("\r\nread edid failed: no ack");
				goto end;
			}
			else
			{
				printf("\r\n");
				for (j = 0; j < 32; j++)
				{
					Sink_EDID[i * 32 + j] = LT8618SXB_I2C_Read_Byte(0x83);
					printf(" ", Sink_EDID[i * 32 + j]);

					///<	edid_data = LT8618SXB_I2C_Read_Byte( 0x83 );

					if ((i == 3) && (j == 30))
					{
						///<	extended_flag = edid_data & 0x03;
						extended_flag = Sink_EDID[i * 32 + j] & 0x03;
					}
					///<	printf( "%02bx,", edid_data );
				}
				if (i == 3)
				{
					if (extended_flag < 1) ///< no block 1, stop reading edid.
					{
						goto end;
					}
				}
			}
		}
		else
		{
			printf("\r\nread edid failed: accs not done");
			goto end;
		}
	}

	if (extended_flag < 2) ///< no block 2, stop reading edid.
	{
		goto end;
	}

	for (i = 0; i < 8; i++) ///<	///< block 2 & 3
	{
		LT8618SXB_I2C_Write_Byte(0x05, i * 32);	  ///< 0x00 is EDID offset address
		LT8618SXB_I2C_Write_Byte(0x07, 0x76);	  ///< 0x31
		LT8618SXB_I2C_Write_Byte(0x07, 0x74);	  ///< 0x31
		LT8618SXB_I2C_Write_Byte(0x07, 0x77);	  ///< 0x37
		usleep(5 * 1000);						  ///< wait 5ms for reading edid data.
		if (LT8618SXB_I2C_Read_Byte(0x40) & 0x02) ///< KEY_DDC_ACCS_DONE=1
		{
			if (LT8618SXB_I2C_Read_Byte(0x40) & 0x50) ///< DDC No Ack or Abitration lost
			{
				printf("\r\nread edid failed: no ack");
				goto end;
			}
			else
			{
				printf("\r\n");
				for (j = 0; j < 32; j++)
				{
					Sink_EDID2[i * 32 + j] = LT8618SXB_I2C_Read_Byte(0x83);
					printf(" ", Sink_EDID2[i * 32 + j]);

					///<	edid_data = LT8618SXB_I2C_Read_Byte( 0x83 );
					///<	printf( "%02bx,", edid_data );
				}
				if (i == 3)
				{
					if (extended_flag < 3) ///< no block 1, stop reading edid.
					{
						goto end;
					}
				}
			}
		}
		else
		{
			printf("\r\nread edid failed: accs not done");
			goto end;
		}
	}
	///< printf("\r\nread edid succeeded, checksum = ",Sink_EDID[255]);
end:
	LT8618SXB_I2C_Write_Byte(0x03, 0xc2);
	LT8618SXB_I2C_Write_Byte(0x07, 0x1f);
#endif

	///<#endif
}

int LT8618SXB_Chip_ID(void)
{
	unsigned char id0 = 0, id1 = 0, id2 = 0;
	int errCnt = 0;

	LT8618SXB_I2C_Write_Byte(0xFF, 0x80); ///< register bank
	LT8618SXB_I2C_Write_Byte(0xee, 0x01);

	while ((id0 != 0x17) || (id1 != 0x02) || ((id2 & 0xfc) != 0xe0))
	{
		id0 = LT8618SXB_I2C_Read_Byte(0x00);
		id1 = LT8618SXB_I2C_Read_Byte(0x01);
		id2 = LT8618SXB_I2C_Read_Byte(0x02);
		errCnt++;
		if (errCnt > 10)
		{
			return -1;
		}
		usleep(100 * 1000);
	}

	/********************************
	 * _Ver_U2 ID: 0x17 0x02 0xE1
	 * _Ver_U3 ID: 0x17 0x02 0xE2
	 ********************************/
	if (id2 == 0xe2)
	{
		flag_Ver_u3 = 1; ///< u3
	}
	else if (id2 == 0xe1)
	{
		flag_Ver_u3 = 0; ///< u2
	}
	return 0;
}

///< The threshold value of high HPD detected by lt8618sxb is 1.2V
unsigned char LT8618SXB_HPD_status(void)
{
	unsigned char HPD_Status = 0;

	LT8618SXB_I2C_Write_Byte(0xff, 0x82); ///< Register bank

	if ((LT8618SXB_I2C_Read_Byte(0x5e) & 0x05) == 0x05)
	{
		HPD_Status = 1; ///< HPD is High
	}

	return HPD_Status;
}

void LT8618SXB_RST_PD_Init(void)
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x80);
	LT8618SXB_I2C_Write_Byte(0x11, 0x00); ///< reset MIPI Rx logic.

	LT8618SXB_I2C_Write_Byte(0x13, 0xf1);
	LT8618SXB_I2C_Write_Byte(0x13, 0xf9); ///< Reset TTL video process
}

void LT8618SXB_HDMI_TX_En(int enable)
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x81);
	if (enable)
	{
		LT8618SXB_I2C_Write_Byte(0x30, 0xea);
	}
	else
	{
		LT8618SXB_I2C_Write_Byte(0x30, 0x00);
	}
}

//_Embedded_sync_ mode��lt8618sxb can only detect h total and H / V active.
//_External_sync_ mode, lt8618sxb can detects a detailed timing value.
void LT8618SXB_Video_check(void)
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x82); ///< video

#ifdef _Embedded_sync_

	h_tal = LT8618SXB_I2C_Read_Byte(0x8f);
	h_tal = (h_tal << 8) + LT8618SXB_I2C_Read_Byte(0x90);
	printf("h_tal %d\n", h_tal);

	v_act = LT8618SXB_I2C_Read_Byte(0x8b);
	v_act = (v_act << 8) + LT8618SXB_I2C_Read_Byte(0x8c);
	printf("v_act %d\n", v_act);

	h_act = LT8618SXB_I2C_Read_Byte(0x8d);
	h_act = (h_act << 8) + LT8618SXB_I2C_Read_Byte(0x8e);
	printf("h_act %d\n", h_act);

	CLK_Cnt = (LT8618SXB_I2C_Read_Byte(0x1d) & 0x0f) * 0x10000 +
			  LT8618SXB_I2C_Read_Byte(0x1e) * 0x100 + LT8618SXB_I2C_Read_Byte(0x1f);
	///<	 Pixel CLK =	CLK_Cnt * 1000
	printf("CLK_Cnt %d\n", CLK_Cnt);
#else
	unsigned char temp;

	vs_pol = 0;
	hs_pol = 0;

	///< _External_sync_
	temp = LT8618SXB_I2C_Read_Byte(0x70); ///< hs vs polarity
	if (temp & 0x02)
	{
		vs_pol = 1;
	}
	if (temp & 0x01)
	{
		hs_pol = 1;
	}

	vs_width = LT8618SXB_I2C_Read_Byte(0x71);

	hs_width = LT8618SXB_I2C_Read_Byte(0x72);
	hs_width = (hs_width << 8) + LT8618SXB_I2C_Read_Byte(0x73);

	vbp = LT8618SXB_I2C_Read_Byte(0x74);
	vfp = LT8618SXB_I2C_Read_Byte(0x75);

	hbp = LT8618SXB_I2C_Read_Byte(0x76);
	hbp = (hbp << 8) + LT8618SXB_I2C_Read_Byte(0x77);

	hfp = LT8618SXB_I2C_Read_Byte(0x78);
	hfp = (hfp << 8) + LT8618SXB_I2C_Read_Byte(0x79);

	v_tal = LT8618SXB_I2C_Read_Byte(0x7a);
	v_tal = (v_tal << 8) + LT8618SXB_I2C_Read_Byte(0x7b);

	h_tal = LT8618SXB_I2C_Read_Byte(0x7c);
	h_tal = (h_tal << 8) + LT8618SXB_I2C_Read_Byte(0x7d);

	v_act = LT8618SXB_I2C_Read_Byte(0x7e);
	v_act = (v_act << 8) + LT8618SXB_I2C_Read_Byte(0x7f);

	h_act = LT8618SXB_I2C_Read_Byte(0x80);
	h_act = (h_act << 8) + LT8618SXB_I2C_Read_Byte(0x81);

	CLK_Cnt = (LT8618SXB_I2C_Read_Byte(0x1d) & 0x0f) * 0x10000 +
		LT8618SXB_I2C_Read_Byte(0x1e) * 0x100 + LT8618SXB_I2C_Read_Byte(0x1f);
	///< pixel CLK =	CLK_Cnt * 1000

#endif
}

void LT8618SXB_TTL_Input_Analog(void)
{
	///< TTL mode
	LT8618SXB_I2C_Write_Byte(0xff, 0x81); ///< register bank
	LT8618SXB_I2C_Write_Byte(0x02, 0x66);
	LT8618SXB_I2C_Write_Byte(0x0a, 0x06);
	LT8618SXB_I2C_Write_Byte(0x15, 0x06);

	LT8618SXB_I2C_Write_Byte(0x4e, 0xa8);

	LT8618SXB_I2C_Write_Byte(0xff, 0x82);
	LT8618SXB_I2C_Write_Byte(0x1b, 0x75);
	LT8618SXB_I2C_Write_Byte(0x1c, 0x30); ///< 30000
}

/***********************************************************

***********************************************************/
void LT8618SXB_TTL_Input_Digtal(void)
{
	LT8618SXB_I2C_Write_Byte(0xff, 0x80);
#ifdef _Embedded_sync_
	///< Internal generate sync/de control logic clock enable
	LT8618SXB_I2C_Write_Byte(0x0A, 0xF0);
#else  ///< _External_sync_
	LT8618SXB_I2C_Write_Byte(0x0A, 0xC0);
#endif

	///< TTL_Input_Digtal
	LT8618SXB_I2C_Write_Byte(0xff, 0x82);		  ///< register bank
	LT8618SXB_I2C_Write_Byte(0x45, _YC_Channel_); ///< YC channel swap
	LT8618SXB_I2C_Write_Byte(0x46, _Reg0x8246_);
	LT8618SXB_I2C_Write_Byte(0x50, 0x00);

	if (Use_DDRCLK)
	{
		LT8618SXB_I2C_Write_Byte(0x4f, 0x80); ///< 0x80: dclk
	}
	else
	{
		LT8618SXB_I2C_Write_Byte(0x4f, 0x40); ///< 0x40: txpll_clk
	}

#ifdef _Embedded_sync_
	///< Select BT rx decode det_vs/hs/de
	LT8618SXB_I2C_Write_Byte(0x51, 0x42);
	///< Embedded sync mode input enable.
	LT8618SXB_I2C_Write_Byte(0x48, 0x0c + _Reg0x8248_D1_D0_);
#else ///< _External_sync_
	LT8618SXB_I2C_Write_Byte(0x51, 0x00); ///< Select TTL process module input video data
	LT8618SXB_I2C_Write_Byte(0x48, 0x00 + _Reg0x8248_D1_D0_);
#endif
}

void LT8618SXB_PLL_setting(void)
{
	unsigned char read_val;
	unsigned char j;
	unsigned char cali_done;
	unsigned char cali_val;
	unsigned char lock;

	CLK_bound = (unsigned char)Format_Timing[Resolution_Num][Clk_bound_SDR + (unsigned char)(Use_DDRCLK)];

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);
	LT8618SXB_I2C_Write_Byte(0x23, 0x40);
	if (flag_Ver_u3)
	{
		LT8618SXB_I2C_Write_Byte(0x24, 0x61); ///< icp set
	}
	else
	{
		LT8618SXB_I2C_Write_Byte(0x24, 0x64); ///< icp set
	}
	LT8618SXB_I2C_Write_Byte(0x26, 0x55);

	LT8618SXB_I2C_Write_Byte(0x29, 0x04); ///< for U3 for U3 SDR/DDR fixed phase

	if (flag_Ver_u3)
	{
		LT8618SXB_I2C_Write_Byte(0x25, LT8618SXB_PLL_u3[CLK_bound][0]);
		LT8618SXB_I2C_Write_Byte(0x2c, LT8618SXB_PLL_u3[CLK_bound][1]);
		LT8618SXB_I2C_Write_Byte(0x2d, LT8618SXB_PLL_u3[CLK_bound][2]);
	}
	else
	{
		LT8618SXB_I2C_Write_Byte(0x25, LT8618SXB_PLL_u2[CLK_bound][0]);
		LT8618SXB_I2C_Write_Byte(0x2c, LT8618SXB_PLL_u2[CLK_bound][1]);
		LT8618SXB_I2C_Write_Byte(0x2d, LT8618SXB_PLL_u2[CLK_bound][2]);
	}

	if (Use_DDRCLK)
	{
		if (flag_Ver_u3)
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x05);
			LT8618SXB_I2C_Write_Byte(0x27, 0x60); ///< 0x60 //ddr 0x66
			LT8618SXB_I2C_Write_Byte(0x28, 0x88);
		}
		else
		{
			read_val = LT8618SXB_I2C_Read_Byte(0x2c) & 0x7f;
			read_val = read_val * 2 | 0x80;
			LT8618SXB_I2C_Write_Byte(0x2c, read_val);

			LT8618SXB_I2C_Write_Byte(0x4d, 0x04);
			LT8618SXB_I2C_Write_Byte(0x27, 0x60); ///< 0x60
			LT8618SXB_I2C_Write_Byte(0x28, 0x88);
		}
	}
	else
	{
		if (flag_Ver_u3)
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x09);
			LT8618SXB_I2C_Write_Byte(0x27, 0x60); ///< 0x06
			LT8618SXB_I2C_Write_Byte(0x28, 0x88); ///< 0x88
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x00);
			LT8618SXB_I2C_Write_Byte(0x27, 0x60); ///< 0x06
			LT8618SXB_I2C_Write_Byte(0x28, 0x00); ///< 0x88
		}
	}

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);

	read_val = LT8618SXB_I2C_Read_Byte(0x2b);
	LT8618SXB_I2C_Write_Byte(0x2b, read_val & 0xfd); ///< sw_en_txpll_cal_en

	read_val = LT8618SXB_I2C_Read_Byte(0x2e);
	LT8618SXB_I2C_Write_Byte(0x2e, read_val & 0xfe); ///< sw_en_txpll_iband_set

	LT8618SXB_I2C_Write_Byte(0xff, 0x82);
	LT8618SXB_I2C_Write_Byte(0xde, 0x00);
	LT8618SXB_I2C_Write_Byte(0xde, 0xc0);

	LT8618SXB_I2C_Write_Byte(0xff, 0x80);
	LT8618SXB_I2C_Write_Byte(0x16, 0xf1);
	LT8618SXB_I2C_Write_Byte(0x18, 0xdc); ///< txpll _sw_rst_n
	LT8618SXB_I2C_Write_Byte(0x18, 0xfc);
	LT8618SXB_I2C_Write_Byte(0x16, 0xf3);

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);

	///<#ifdef _Ver_U3_
	if (flag_Ver_u3)
	{
		if (Use_DDRCLK)
		{
			LT8618SXB_I2C_Write_Byte(0x2a, 0x10);
			LT8618SXB_I2C_Write_Byte(0x2a, 0x30);
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0x2a, 0x00);
			LT8618SXB_I2C_Write_Byte(0x2a, 0x20);
		}
	}
	///<#endif

	for (j = 0; j < 0x05; j++)
	{
		usleep(10 * 1000);
		LT8618SXB_I2C_Write_Byte(0xff, 0x80);
		LT8618SXB_I2C_Write_Byte(0x16, 0xe3); /* pll lock logic reset */
		LT8618SXB_I2C_Write_Byte(0x16, 0xf3);

		LT8618SXB_I2C_Write_Byte(0xff, 0x82);
		lock = 0x80 & LT8618SXB_I2C_Read_Byte(0x15);
		cali_val = LT8618SXB_I2C_Read_Byte(0xea);
		cali_done = 0x80 & LT8618SXB_I2C_Read_Byte(0xeb);

		if (lock && cali_done && (cali_val != 0xff))
		{
#ifdef _DEBUG_MODE_

			Debug_Printf("\r\nTXPLL Lock");
			LT8618SXB_I2C_Write_Byte(0xff, 0x82);
			Debug_DispStrNum("\r\n0x82ea=%bx", LT8618SXB_I2C_Read_Byte(0xea));
			Debug_DispStrNum("\r\n0x82eb=%bx", LT8618SXB_I2C_Read_Byte(0xeb));
			Debug_DispStrNum("\r\n0x82ec=%bx", LT8618SXB_I2C_Read_Byte(0xec));
			Debug_DispStrNum("\r\n0x82ed=%bx", LT8618SXB_I2C_Read_Byte(0xed));
			Debug_DispStrNum("\r\n0x82ee=%bx", LT8618SXB_I2C_Read_Byte(0xee));
			Debug_DispStrNum("\r\n0x82ef=%bx", LT8618SXB_I2C_Read_Byte(0xef));

#endif

			///<	return TRUE;
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0xff, 0x80);
			LT8618SXB_I2C_Write_Byte(0x16, 0xf1);
			LT8618SXB_I2C_Write_Byte(0x18, 0xdc); ///< txpll _sw_rst_n
			LT8618SXB_I2C_Write_Byte(0x18, 0xfc);
			LT8618SXB_I2C_Write_Byte(0x16, 0xf3);
#ifdef _DEBUG_MODE_
			Debug_Printf("\r\nTXPLL Reset");
#endif
		}
	}
}

/***********************************************************

***********************************************************/
void LT8618SXB_Audio_setting(void)
{
	///<----------------IIS-----------------------
	///< IIS Input
	LT8618SXB_I2C_Write_Byte(0xff, 0x82); ///< register bank
	LT8618SXB_I2C_Write_Byte(0xd6, 0x8e); ///< bit7 = 0 : DVI output; bit7 = 1: HDMI output
	LT8618SXB_I2C_Write_Byte(0xd7, 0x04); ///< sync polarity

	LT8618SXB_I2C_Write_Byte(0xff, 0x84); ///< register bank
	LT8618SXB_I2C_Write_Byte(0x06, 0x08);
	LT8618SXB_I2C_Write_Byte(0x07, 0x10); ///< SD0 channel selected

	LT8618SXB_I2C_Write_Byte(0x09, 0x00); ///< 0x00 :Left justified; default

	///<-----------------SPDIF---------------------

	/*	SPDIF Input
	   LT8618SXB_I2C_Write_Byte( 0xff, 0x82 );///< register bank
	   LT8618SXB_I2C_Write_Byte( 0xd6, 0x8e );
	   LT8618SXB_I2C_Write_Byte( 0xd7, 0x00 );	//sync polarity

	   LT8618SXB_I2C_Write_Byte( 0xff, 0x84 );///< register bank
	   LT8618SXB_I2C_Write_Byte( 0x06, 0x0c );
	   LT8618SXB_I2C_Write_Byte( 0x07, 0x10 );
	 */

	LT8618SXB_I2C_Write_Byte(0x0f, 0x0b + Sample_Freq[_48KHz]);

	LT8618SXB_I2C_Write_Byte(0x34, 0xd4); ///< CTS_N / 2; 32bit
	///<LT8618SXB_I2C_Write_Byte( 0x34, 0xd5 );	//CTS_N / 4; 16bit

	LT8618SXB_I2C_Write_Byte(0x35, (unsigned char)(IIS_N[_48KHz] / 0x10000));
	LT8618SXB_I2C_Write_Byte(0x36, (unsigned char)((IIS_N[_48KHz] & 0x00FFFF) / 0x100));
	LT8618SXB_I2C_Write_Byte(0x37, (unsigned char)(IIS_N[_48KHz] & 0x0000FF));
	LT8618SXB_I2C_Write_Byte(0x3c, 0x21); ///< Null packet enable
}

///< LT8618SXB only supports three color space convert: YUV422, yuv444 and rgb888.
///< Color space convert of YUV420 is not supported.
void LT8618SXB_CSC_setting(void)
{
	///< color space config
	LT8618SXB_I2C_Write_Byte(0xff, 0x82); ///< register bank
										  ///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0x08 );///< YCbCr444 to RGB
	LT8618SXB_I2C_Write_Byte(0xb9, 0x18); ///< YCbCr422 to RGB

	///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0x80 );///< RGB to YCbCr444
	///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0xa0 );///< RGB to YCbCr422

	///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0x10 );///< YCbCr422 to YCbCr444
	///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0x20 );///< YCbCr444 to YCbCr422

	///<	LT8618SXB_I2C_Write_Byte( 0xb9, 0x00 ); ///< No csc
}

void LT8618SXB_AVI_setting(void)
{
	///< AVI
	unsigned char AVI_PB0 = 0x00;
	unsigned char AVI_PB1 = 0x00;
	unsigned char AVI_PB2 = 0x00;

	/********************************************************************************
	   The 0x43 register is checksums,
	   changing the value of the 0x45 or 0x47 register,
	   and the value of the 0x43 register is also changed.
	   0x43, 0x44, 0x45, and 0x47 are the sum of the four register values is 0x6F.
	 *********************************************************************************/

	///<	VIC_Num = 0x04; ///< 720P 60; Corresponding to the resolution to be output
	///<	VIC_Num = 0x10;	///< 1080P 60
	///<	VIC_Num = 0x1F;	///< 1080P 50
	///<	VIC_Num = 0x5F;	///< 4K30;

	///<================================================================///<

	///< Please refer to function: void LT8618SXB_CSC_setting( void )

	/****************************************************
	   Because the color space of BT1120 is YUV422,
	   if lt8618sxb does not do color space convert (no CSC),
	   the color space of output HDMI is YUV422.
	 *****************************************************/

	VIC_Num = 0x10;

	///< if No csc
	///<	AVI_PB1 = 0x30; ///< PB1,color space: YUV444 0x50;YUV422 0x30; RGB 0x10

	///<	if YCbCr422 to RGB CSC
	AVI_PB1 = 0x10; ///< PB1,color space: YUV444 0x50;YUV422 0x30; RGB 0x10

	AVI_PB2 = Format_Timing[Resolution_Num][Pic_Ratio]; ///< PB2; picture aspect rate: 0x19:4:3 ;     0x2A : 16:9

	AVI_PB0 = ((AVI_PB1 + AVI_PB2 + VIC_Num) <= 0x6f) ?
				(0x6f - AVI_PB1 - AVI_PB2 - VIC_Num) :
				(0x16f - AVI_PB1 - AVI_PB2 - VIC_Num);

	LT8618SXB_I2C_Write_Byte(0xff, 0x84);	 ///< register bank
	LT8618SXB_I2C_Write_Byte(0x43, AVI_PB0); ///< PB0,avi packet checksum
	LT8618SXB_I2C_Write_Byte(0x44, AVI_PB1); ///< PB1,color space: YUV444 0x50;YUV422 0x30; RGB 0x10
	LT8618SXB_I2C_Write_Byte(0x45, AVI_PB2); ///< PB2;picture aspect rate: 0x19:4:3 ; 0x2A : 16:9
	LT8618SXB_I2C_Write_Byte(0x47, VIC_Num); ///< PB4;vic ,0x10: 1080P ;  0x04 : 720P

	///<	LT8618SXB_I2C_Write_Byte(0xff,0x84);
	///<	8618SXB hdcp1.4
	///<	LT8618SXB_I2C_Write_Byte( 0x10, 0x30 );             //data iland
	///<	LT8618SXB_I2C_Write_Byte( 0x12, 0x64 );             //act_h_blank

	///< VS_IF, 4k 30hz need send VS_IF packet. Please refer to hdmi1.4 spec 8.2.3
	if (VIC_Num == 95)
	{
		///<	   LT8618SXB_I2C_Write_Byte(0xff,0x84);
		LT8618SXB_I2C_Write_Byte(0x3d, 0x2a); ///< UD1 infoframe enable
		LT8618SXB_I2C_Write_Byte(0x74, 0x81);
		LT8618SXB_I2C_Write_Byte(0x75, 0x01);
		LT8618SXB_I2C_Write_Byte(0x76, 0x05);
		LT8618SXB_I2C_Write_Byte(0x77, 0x49);
		LT8618SXB_I2C_Write_Byte(0x78, 0x03);
		LT8618SXB_I2C_Write_Byte(0x79, 0x0c);
		LT8618SXB_I2C_Write_Byte(0x7a, 0x00);
		LT8618SXB_I2C_Write_Byte(0x7b, 0x20);
		LT8618SXB_I2C_Write_Byte(0x7c, 0x01);
	}
	else
	{
		///< LT8618SXB_I2C_Write_Byte(0xff,0x84);
		LT8618SXB_I2C_Write_Byte(0x3d, 0x0a); ///< UD1 infoframe disable
	}
}

void LT8618SXB_TX_Phy(void)
{
	///< HDMI_TX_Phy
	LT8618SXB_I2C_Write_Byte(0xff, 0x81); ///< register bank
	LT8618SXB_I2C_Write_Byte(0x30, 0xea);

	LT8618SXB_I2C_Write_Byte(0x31, 0x73); ///< HDMDITX_dc_det_en 0x44
	LT8618SXB_I2C_Write_Byte(0x32, 0xea);
	LT8618SXB_I2C_Write_Byte(0x33, 0x4a); ///< 0x0b


	LT8618SXB_I2C_Write_Byte(0x34, 0x00);
	LT8618SXB_I2C_Write_Byte(0x35, 0x00);
	LT8618SXB_I2C_Write_Byte(0x36, 0x00);
	LT8618SXB_I2C_Write_Byte(0x37, 0x44);
	LT8618SXB_I2C_Write_Byte(0x3f, 0x0f);

	LT8618SXB_I2C_Write_Byte(0x40, 0xe0); ///< 0xa0 -- CLK tap0 swing
	LT8618SXB_I2C_Write_Byte(0x41, 0xe0); ///< 0xa0 -- D0 tap0 swing
	LT8618SXB_I2C_Write_Byte(0x42, 0xe0); ///< 0xa0 -- D1 tap0 swing
	LT8618SXB_I2C_Write_Byte(0x43, 0xe0); ///< 0xa0 -- D2 tap0 swing

	LT8618SXB_I2C_Write_Byte(0x44, 0x0a);
}

unsigned char LT8618SX_Phase_1(void)
{
	unsigned char temp = 0;
	unsigned char read_val = 0;
	///<	unsigned char		Pre_read_val   = 0;
	///<	int	b_ok		   = 0;

	unsigned char OK_CNT = 0x00;
	unsigned char OK_CNT_1 = 0x00;
	unsigned char OK_CNT_2 = 0x00;
	unsigned char OK_CNT_3 = 0x00;
	unsigned char Jump_CNT = 0x00;
	unsigned char Jump_Num = 0x00;
	unsigned char Jump_Num_1 = 0x00;
	unsigned char Jump_Num_2 = 0x00;
	unsigned char Jump_Num_3 = 0x00;
	int temp0_ok = 0;
	int temp9_ok = 0;
	int b_OK = 0;

	LT8618SXB_I2C_Write_Byte(0xff, 0x80); ///< register bank
	LT8618SXB_I2C_Write_Byte(0x13, 0xf1);
	usleep(5 * 1000);
	LT8618SXB_I2C_Write_Byte(0x13, 0xf9); ///< Reset TTL video process
	usleep(10 * 1000);

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);

	for (temp = 0; temp < 0x0a; temp++)
	{

		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + temp));
		usleep(5 * 1000);
		if (Use_DDRCLK)
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x05);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x0d);
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x01);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x09);
		}
		usleep(100 * 1000);
		read_val = LT8618SXB_I2C_Read_Byte(0x50) & 0x01;

		if (read_val == 0)
		{
			OK_CNT++;

			if (b_OK == 0)
			{
				b_OK = 1;
				Jump_CNT++;

				if (Jump_CNT == 1)
				{
					Jump_Num_1 = temp;
				}
				else if (Jump_CNT == 3)
				{
					Jump_Num_2 = temp;
				}
				else if (Jump_CNT == 5)
				{
					Jump_Num_3 = temp;
				}
			}

			if (Jump_CNT == 1)
			{
				OK_CNT_1++;
			}
			else if (Jump_CNT == 3)
			{
				OK_CNT_2++;
			}
			else if (Jump_CNT == 5)
			{
				OK_CNT_3++;
			}

			if (temp == 0)
			{
				temp0_ok = 1;
			}
			if (temp == 9)
			{
				Jump_CNT++;
				temp9_ok = 1;
			}
		}
		else
		{
			if (b_OK)
			{
				b_OK = 0;
				Jump_CNT++;
			}
		}
	}

	if ((Jump_CNT == 0) || (Jump_CNT > 6))
	{
		return 0;
	}

	if ((temp9_ok == 1) && (temp0_ok == 1))
	{
		if (Jump_CNT == 6)
		{
			OK_CNT_3 = OK_CNT_3 + OK_CNT_1;
			OK_CNT_1 = 0;
		}
		else if (Jump_CNT == 4)
		{
			OK_CNT_2 = OK_CNT_2 + OK_CNT_1;
			OK_CNT_1 = 0;
		}
	}
	if (Jump_CNT >= 2)
	{
		if (OK_CNT_1 >= OK_CNT_2)
		{
			if (OK_CNT_1 >= OK_CNT_3)
			{
				OK_CNT = OK_CNT_1;
				Jump_Num = Jump_Num_1;
			}
			else
			{
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		}
		else
		{
			if (OK_CNT_2 >= OK_CNT_3)
			{
				OK_CNT = OK_CNT_2;
				Jump_Num = Jump_Num_2;
			}
			else
			{
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		}
	}

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);

	if ((Jump_CNT == 2) || (Jump_CNT == 4) || (Jump_CNT == 6))
	{
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + (Jump_Num + (OK_CNT / 2)) % 0x0a));
	}
	else if (OK_CNT >= 0x09)
	{
		LT8618SXB_I2C_Write_Byte(0x27, 0x66);
	}

#ifdef _Phase_Debug_
	///<	Debug_Printf( "\r\nRead LT8618SXB ID " );
	///<	Debug_Printf( "\r\n " );
	Debug_DispStrNum("\r\nReg0x8127 = :", LT8618SXB_I2C_Read_Byte(0x27));
	///<	Debug_DispStrNum( " ", HDMI_ReadI2C_Byte( 0x01 ) );
	///<	Debug_DispStrNum( " ", HDMI_ReadI2C_Byte( 0x02 ) );
	///<	Debug_Printf( "\r\n " );
#endif

	return 1;
}
unsigned char LT8618SX_Phase_2(void)
{
	unsigned char temp = 0;
	unsigned char read_val = 0;
	unsigned char Pre_read_val = 0;

	unsigned char cnt_1 = 0x00;
	unsigned char cnt_0 = 0x00;

	int b_OK = 0;
	unsigned char value_ok = 0;
	LT8618SXB_I2C_Write_Byte(0xff, 0x80); ///< register bank
	LT8618SXB_I2C_Write_Byte(0x13, 0xf1);
	usleep(5 * 1000);
	LT8618SXB_I2C_Write_Byte(0x13, 0xf9); ///< Reset TTL video process
	usleep(10 * 1000);

	for (temp = 0; temp < 0x0a; temp++)
	{
		LT8618SXB_I2C_Write_Byte(0xff, 0x81);
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + temp));

		if (Use_DDRCLK)
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x05);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x0d);
			///<	Delay_ms( 50 );
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x01);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x09);
		}

		usleep(10 * 1000);
		read_val = LT8618SXB_I2C_Read_Byte(0x50);

		if ((read_val == 0) && !b_OK)
		{
			cnt_0++;

			if (Pre_read_val)
			{
				b_OK = 1;
				value_ok = temp;
				///<	break;
			}
		}
		else
		{
			Pre_read_val = read_val;

			if ((temp == 9) && (cnt_1 < 9))
			{
				b_OK = 1;
				LT8618SXB_I2C_Write_Byte(0x27, 0x60);
				break;
			}

			cnt_1++;
		}
	}

	if ((cnt_0 >= 9) || (cnt_1 >= 9))
	{
		return 0;
	}

	LT8618SXB_I2C_Write_Byte(0x27, (0x60 + value_ok));
	return 0;
}

/***********************************************************

***********************************************************/
#ifdef _Embedded_sync_
void LT8618SXB_BT_Timing_setting(void)
{
	///< The synchronization signal in BT1120 is only DE. Without external sync and DE, Video check can only detect H total and H/V active.
	///< BT1120 Timing value settings can not be set by reading the value of the timing status register through Video check, which needs to be set according to the front BT1120 resolution.

	///< LT8618SX_BT1120_Set
	///< Pls refer to array : Format_Timing[][14]
	LT8618SXB_I2C_Write_Byte(0xff, 0x82);
	LT8618SXB_I2C_Write_Byte(0x20, (unsigned char)(Format_Timing[Resolution_Num][H_act] / 256));
	LT8618SXB_I2C_Write_Byte(0x21, (unsigned char)(Format_Timing[Resolution_Num][H_act] % 256));
	LT8618SXB_I2C_Write_Byte(0x22, (unsigned char)(Format_Timing[Resolution_Num][H_fp] / 256));
	LT8618SXB_I2C_Write_Byte(0x23, (unsigned char)(Format_Timing[Resolution_Num][H_fp] % 256));
	LT8618SXB_I2C_Write_Byte(0x24, (unsigned char)(Format_Timing[Resolution_Num][H_sync] / 256));
	LT8618SXB_I2C_Write_Byte(0x25, (unsigned char)(Format_Timing[Resolution_Num][H_sync] % 256));
	LT8618SXB_I2C_Write_Byte(0x26, 0x00);
	LT8618SXB_I2C_Write_Byte(0x27, 0x00);
	LT8618SXB_I2C_Write_Byte(0x36, (unsigned char)(Format_Timing[Resolution_Num][V_act] / 256));
	LT8618SXB_I2C_Write_Byte(0x37, (unsigned char)(Format_Timing[Resolution_Num][V_act] % 256));
	LT8618SXB_I2C_Write_Byte(0x38, (unsigned char)(Format_Timing[Resolution_Num][V_fp] / 256));
	LT8618SXB_I2C_Write_Byte(0x39, (unsigned char)(Format_Timing[Resolution_Num][V_fp] % 256));
	LT8618SXB_I2C_Write_Byte(0x3a, (unsigned char)(Format_Timing[Resolution_Num][V_bp] / 256));
	LT8618SXB_I2C_Write_Byte(0x3b, (unsigned char)(Format_Timing[Resolution_Num][V_bp] % 256));
	LT8618SXB_I2C_Write_Byte(0x3c, (unsigned char)(Format_Timing[Resolution_Num][V_sync] / 256));
	LT8618SXB_I2C_Write_Byte(0x3d, (unsigned char)(Format_Timing[Resolution_Num][V_sync] % 256));
	printf("(H_act)  %d\n", Format_Timing[Resolution_Num][H_act] / 256);
	printf("(H_act).  %d\n", Format_Timing[Resolution_Num][H_act] % 256);
	printf("(H_fp)  %d\n", Format_Timing[Resolution_Num][H_fp] / 256);
	printf("(H_fp).  %d\n", Format_Timing[Resolution_Num][H_fp] % 256);
	printf("(H_sync)  %d\n", Format_Timing[Resolution_Num][H_sync] / 256);
	printf("(H_sync).  %d\n", Format_Timing[Resolution_Num][H_sync] % 256);
	printf("(V_act)  %d\n", Format_Timing[Resolution_Num][V_act] / 256);
	printf("(V_act).  %d\n", Format_Timing[Resolution_Num][V_act] % 256);
	printf("(V_fp)  %d\n", Format_Timing[Resolution_Num][V_fp] / 256);
	printf("(V_fp).  %d\n", Format_Timing[Resolution_Num][V_fp] % 256);
	printf("(V_bp)  %d\n", Format_Timing[Resolution_Num][V_bp] / 256);
	printf("(V_bp).  %d\n", Format_Timing[Resolution_Num][V_bp] % 256);
	printf("(V_sync)  %d\n", Format_Timing[Resolution_Num][V_sync] / 256);
	printf("(V_sync).  %d\n", Format_Timing[Resolution_Num][V_sync] % 256);
}

#if 1
unsigned char LT8618SX_Phase(void)
{
	unsigned char temp = 0;
	unsigned char read_value = 0;
	unsigned char b_ok = 0;
	unsigned char Temp_f = 0;

	for (temp = 0; temp < 0x0a; temp++)
	{
		LT8618SXB_I2C_Write_Byte(0xff, 0x81);
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + temp));

		if (Use_DDRCLK)
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x05);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x0d);
		}
		else
		{
			LT8618SXB_I2C_Write_Byte(0x4d, 0x01);
			usleep(5 * 1000);
			LT8618SXB_I2C_Write_Byte(0x4d, 0x09);
		}

		usleep(50 * 1000);

		read_value = LT8618SXB_I2C_Read_Byte(0x50);
		if (read_value == 0x00)
		{
			if (b_ok == 0)
			{
				Temp_f = temp;
			}
			b_ok = 1;
		}
		else
		{
			b_ok = 0;
		}
	}

	LT8618SXB_I2C_Write_Byte(0xff, 0x81);
	LT8618SXB_I2C_Write_Byte(0x27, (0x60 + Temp_f));

	return Temp_f;
}

int LT8618SXB_Phase_config(void)
{
	unsigned char Temp = 0x00;
	unsigned char Temp_f = 0x00;
	unsigned char OK_CNT = 0x00;
	unsigned char OK_CNT_1 = 0x00;
	unsigned char OK_CNT_2 = 0x00;
	unsigned char OK_CNT_3 = 0x00;
	unsigned char Jump_CNT = 0x00;
	unsigned char Jump_Num = 0x00;
	unsigned char Jump_Num_1 = 0x00;
	unsigned char Jump_Num_2 = 0x00;
	unsigned char Jump_Num_3 = 0x00;
	int temp0_ok = 0;
	int temp9_ok = 0;
	int b_OK = 0;
	unsigned short V_ACT = 0x0000;
	unsigned short H_ACT = 0x0000;
	unsigned short H_TOTAL = 0x0000;
	///<	unsigned short		V_TOTAL	   = 0x0000;
	///<	unsigned char		H_double   = 1;

	Temp_f = LT8618SX_Phase(); ///< it's setted before video check

	while (Temp <= 0x09)
	{
		LT8618SXB_I2C_Write_Byte(0xff, 0x81);
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + Temp));
		LT8618SXB_I2C_Write_Byte(0xff, 0x80);
		LT8618SXB_I2C_Write_Byte(0x13, 0xf1); ///< ttl video process reset///20191121
		LT8618SXB_I2C_Write_Byte(0x12, 0xfb); ///< video check reset//20191121
		usleep(5 * 1000);					  ///< add 20191121
		LT8618SXB_I2C_Write_Byte(0x12, 0xff); ///< 20191121
		LT8618SXB_I2C_Write_Byte(0x13, 0xf9); ///< 20191121

		usleep(80 * 1000); ///<

		LT8618SXB_I2C_Write_Byte(0xff, 0x81);
		LT8618SXB_I2C_Write_Byte(0x51, 0x42);

		LT8618SXB_I2C_Write_Byte(0xff, 0x82);
		H_TOTAL = LT8618SXB_I2C_Read_Byte(0x8f);
		H_TOTAL = (H_TOTAL << 8) + LT8618SXB_I2C_Read_Byte(0x90);
		V_ACT = LT8618SXB_I2C_Read_Byte(0x8b);
		V_ACT = (V_ACT << 8) + LT8618SXB_I2C_Read_Byte(0x8c);
		H_ACT = LT8618SXB_I2C_Read_Byte(0x8d);
		H_ACT = (H_ACT << 8) + LT8618SXB_I2C_Read_Byte(0x8e) - 0x04; ///< note

		///<					LT8618SXB_I2C_Write_Byte( 0xff, 0x80 );
		///<					LT8618SXB_I2C_Write_Byte( 0x09, 0xfe );

		if ((V_ACT > (Format_Timing[Resolution_Num][V_act] - 5)) &&
			(V_ACT < (Format_Timing[Resolution_Num][V_act] + 5)) &&
			(H_ACT > (Format_Timing[Resolution_Num][H_act] - 5)) &&
			(H_ACT < (Format_Timing[Resolution_Num][H_act] + 5)) &&
			(H_TOTAL > (Format_Timing[Resolution_Num][H_tol] - 5)) &&
			(H_TOTAL < (Format_Timing[Resolution_Num][H_tol] + 5)))

		{
			OK_CNT++;

			if (b_OK == 0)
			{
				b_OK = 1;
				Jump_CNT++;

				if (Jump_CNT == 1)
				{
					Jump_Num_1 = Temp;
				}
				else if (Jump_CNT == 3)
				{
					Jump_Num_2 = Temp;
				}
				else if (Jump_CNT == 5)
				{
					Jump_Num_3 = Temp;
				}
			}

			if (Jump_CNT == 1)
			{
				OK_CNT_1++;
			}
			else if (Jump_CNT == 3)
			{
				OK_CNT_2++;
			}
			else if (Jump_CNT == 5)
			{
				OK_CNT_3++;
			}

			if (Temp == 0)
			{
				temp0_ok = 1;
			}
			if (Temp == 9)
			{
				Jump_CNT++;
				temp9_ok = 1;
			}

#ifdef _Phase_Debug_
			Debug_Printf("\r\n this phase is ok,temp=");
			///< Debug_DispNum( Temp );
			Debug_Printf("\r\n Jump_CNT=");
			///< Debug_DispNum( Jump_CNT );
#endif
		}
		else

		{
			if (b_OK)
			{
				b_OK = 0;
				Jump_CNT++;
			}
#ifdef _Phase_Debug_
			Debug_Printf("\r\n this phase is fail,temp=");
			///< Debug_DispNum( Temp );
			Debug_Printf("\r\n Jump_CNT=");
			///< Debug_DispNum( Jump_CNT );
#endif
		}

		Temp++;
	}

#ifdef _Phase_Debug_
	Debug_Printf("\r\n OK_CNT_1=");
	///< Debug_DispNum( OK_CNT_1 );
	Debug_Printf("\r\n OK_CNT_2=");
	///< Debug_DispNum( OK_CNT_2 );
	Debug_Printf("\r\n OK_CNT_3=");
	///< Debug_DispNum( OK_CNT_3 );
#endif

	if ((Jump_CNT == 0) || (Jump_CNT > 6))
	{
#ifdef _Phase_Debug_
		Debug_Printf("\r\ncali phase fail");
#endif
		return 0;
	}

	if ((temp9_ok == 1) && (temp0_ok == 1))
	{
		if (Jump_CNT == 6)
		{
			OK_CNT_3 = OK_CNT_3 + OK_CNT_1;
			OK_CNT_1 = 0;
		}
		else if (Jump_CNT == 4)
		{
			OK_CNT_2 = OK_CNT_2 + OK_CNT_1;
			OK_CNT_1 = 0;
		}
	}
	if (Jump_CNT >= 2)
	{
		if (OK_CNT_1 >= OK_CNT_2)
		{
			if (OK_CNT_1 >= OK_CNT_3)
			{
				OK_CNT = OK_CNT_1;
				Jump_Num = Jump_Num_1;
			}
			else
			{
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		}
		else
		{
			if (OK_CNT_2 >= OK_CNT_3)
			{
				OK_CNT = OK_CNT_2;
				Jump_Num = Jump_Num_2;
			}
			else
			{
				OK_CNT = OK_CNT_3;
				Jump_Num = Jump_Num_3;
			}
		}
	}
	LT8618SXB_I2C_Write_Byte(0xff, 0x81);

	if ((Jump_CNT == 2) || (Jump_CNT == 4) || (Jump_CNT == 6))
	{
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + (Jump_Num + (OK_CNT / 2)) % 0x0a));
	}

	if (OK_CNT == 0x0a)
	{
		LT8618SXB_I2C_Write_Byte(0x27, (0x60 + (Temp_f + 5) % 0x0a));
	}

#ifdef _Phase_Debug_
	Debug_DispStrNum("cail phase is 0x%x", LT8618SXB_I2C_Read_Byte(0x27));
#endif

	return 1;
}

#endif
#endif

void LT8618SXB_Reset(void)
{
	VIM_GPIO_SetValue(RESET_CHIP_GPIO, 0);
	usleep(100 * 1000);
	VIM_GPIO_SetValue(RESET_CHIP_GPIO, 1);
	usleep(10 * 1000);
}

int de_hdmi_init(int hdmi_mode)
{
	int ret = -1;

	Use_DDRCLK = 0; ///< 1: DDR mode; 0: SDR (normal) mode

	Resolution_Num = hdmi_mode; ///< Parameters required by LT8618SXB_BT_Timing_setting( void )

	///< Parameters required by LT8618SXB_PLL_setting( void )
	CLK_bound = Format_Timing[Resolution_Num][Clk_bound_SDR + (unsigned char)(Use_DDRCLK)];

	///< Parameters required by LT8618SXB_AVI_setting( void )
	VIC_Num = Format_Timing[Resolution_Num][Vic];

	///<  With different resolutions, Vic has different values, Refer to the following list

	/**********************************************************************************************
	 *  Resolution			VIC_Num ( VIC: Video Format Identification Code )
	 *  --------------------------------------
	 *  640x480				1
	 *  720x480P 60Hz		2
	 *  720x480i 60Hz		6
	 *
	 *  720x576P 50Hz		17
	 *  720x576i 50Hz		21
	 *
	 *  1280x720P 24Hz		60
	 *  1280x720P 25Hz		61
	 *  1280x720P 30Hz		62
	 *  1280x720P 50Hz		19
	 *  1280x720P 60Hz		4
	 *
	 *  1920x1080P 24Hz		32
	 *  1920x1080P 25Hz		33
	 *  1920x1080P 30Hz		34
	 *
	 *  1920x1080i 50Hz		20
	 *  1920x1080i 60Hz		5
	 *  1920x1080P 50Hz		31
	 *  1920x1080P 60Hz		16
	 *  3840x2160 30Hz		95 ///< 4K30
	 *  Other resolution     0(default) ///< Such as 800x600 / 1024x768 / 1366x768 / 1280x1024.....
	 **********************************************************************************************/

	I2CADR = _LT8618SX_ADR >> 1;

	LT8618SXB_Reset();

	ret = i2c_init((VIM_S8 *)IIC2_PATH, I2CADR);
	if (0 != ret)
	{
		return ret;
	}
	///< Before initializing lt8168sxb, you need to enable IIC of lt8618sxb
	LT8618SXB_I2C_Write_Byte(0xff, 0x80); ///< register bank
	LT8618SXB_I2C_Write_Byte(0xee, 0x01); ///< enable IIC

	ret = LT8618SXB_Chip_ID(); ///< for debug
	if (0 != ret)
	{
		return ret;
	}

	LT8618SXB_RST_PD_Init();

	///< TTL mode
	LT8618SXB_TTL_Input_Analog();
	LT8618SXB_TTL_Input_Digtal();

	///< Wait for the signal to be stable and decide
	///< whether the delay is necessary according to the actual situation
	printf("hdmi sleep 1\n");
	usleep(50 * 1000);
	printf("hdmi sleep Over\n");

	LT8618SXB_Video_check(); ///< For debug

	///< PLL
	LT8618SXB_PLL_setting();
	LT8618SXB_Audio_setting();
	LT8618SXB_CSC_setting();
#ifdef _LT8618_HDCP_
	LT8618SXB_HDCP_Init();
#endif

	LT8618SXB_AVI_setting();

	///< This operation is not necessary. Read TV EDID if necessary.
	LT8618SXB_Read_EDID(); ///< Read TV  EDID

#ifdef _LT8618_HDCP_
	LT8618SXB_HDCP_Enable();
#endif

#ifdef _Embedded_sync_
	LT8618SXB_BT_Timing_setting();

	if (flag_Ver_u3)
	{
		LT8618SX_Phase_1();
	}
	else
	{
		LT8618SXB_Phase_config();
	}

#else

	LT8618SX_Phase_1();
#endif
	///< HDMI_TX_Phy
	LT8618SXB_TX_Phy();
	for (int j = AID_GPIO_ID_C0; j <= AID_GPIO_ID_C27; j++)
	{
		VIM_GPIO_SelectDrive(j, GPIO_DRIVE_STRONG7);
	}

	///<	system("devmem 0X60052000 32 0xC80");
	printf("init hdmi OVER\n");

	return 0;
}

///< When the lt8618sxb works, the resolution of the bt1120 signal changes.
///< The following settings need to be configured.
void Resolution_change(unsigned char Resolution)
{
	Resolution_Num = Resolution; ///< Parameters required by LT8618SXB_BT_Timing_setting( void )

	///< Parameters required by LT8618SXB_PLL_setting( void )
	CLK_bound = Format_Timing[Resolution_Num][Clk_bound_SDR + (unsigned char)(Use_DDRCLK)];

	VIC_Num = Format_Timing[Resolution_Num][Vic];

	LT8618SXB_PLL_setting();

	LT8618SXB_AVI_setting();

#ifdef _Embedded_sync_
	///<-------------------------------------------

	LT8618SXB_BT_Timing_setting();

	///<-------------------------------------------

	if (!flag_Ver_u3)
	{
		LT8618SXB_Phase_config();
	}
#endif
}

void LT8618SXB_I2C_Write_Byte(unsigned char RegAddr, unsigned char data)
{

	i2cv_write((VIM_S8 *)IIC2_PATH, I2CADR, RegAddr, data, 0x1, 0x1, 1);
}
unsigned int LT8618SXB_I2C_Read_Byte(unsigned char RegAddr)
{
	unsigned int regVal = 0;
	int ret = -1;

	ret = i2cv_read((VIM_S8 *)IIC2_PATH, I2CADR, RegAddr, &regVal, 0x1, 0x1, 1);
	if (0 != ret)
	{
		printf("%s err ret = %d", __func__, ret);
	}

	return regVal;
}

VIM_S32 i2c_init(VIM_S8 *name, VIM_S32 slave_addr)
{

	VIM_S32 i2c_fd = -1;
	VIM_U32 funcs;

	if (!name)
	{
		return_log(-1);
	}
	/*1 open i2c device	*/
	i2c_fd = open((const char *)name, O_RDWR);
	if (i2c_fd < 0)
	{
		printf("open [%s] faild!\n", name);
		return -1;
	}

	/* check adapter functionality */
	if (ioctl(i2c_fd, I2C_FUNCS, &funcs) < 0)
	{
		close(i2c_fd);
		printf("Error: Could not get the adapter");
		return -2;
	}

	/*2 set slave addr*/
	if (ioctl(i2c_fd, I2C_SLAVE_FORCE, slave_addr) < 0)
	{
		close(i2c_fd);
		printf("set slave addr error!\n");
		return -2;
	}

	close(i2c_fd);
	printf("%s OK\n", __func__);
	return 0;
}

VIM_S32 i2cv_write(VIM_S8 *name,
				   VIM_S32 slave_addr,
				   VIM_S32 reg_addr,
				   VIM_S32 reg_val,
				   VIM_S32 addr_width,
				   VIM_S32 val_width,
				   VIM_S32 msb_flag)
{
	VIM_S32 ret = 0;
	VIM_U8 buf0[8];
	struct i2c_msg msg[2];
	VIM_S32 i2c_fd = -1;

	if (!name)
		return_log(-1);

	/*1 open i2c device	*/
	i2c_fd = open((const char *)name, O_RDWR);
	if (i2c_fd < 0)
	{
		printf("open [%s] faild!\n", name);
		return -1;
	}

	/*2 set slave addr*/
	if (ioctl(i2c_fd, I2C_SLAVE_FORCE, slave_addr) < 0)
	{
		close(i2c_fd);
		printf("set slave addr error!\n");
		return -2;
	}

	/*3 read register	*/
	if (addr_width == 1)
	{
		buf0[0] = reg_addr & 0xff;
	}
	else if (addr_width == 2)
	{
		buf0[1] = reg_addr & 0xff;
		buf0[0] = reg_addr >> 8;
	}
	else
	{
	}

	if (val_width == 1)
	{
		buf0[addr_width] = reg_val;
	}
	else if (val_width == 2)
	{
		if (!msb_flag)
		{
			buf0[addr_width + 1] = reg_val & 0xff;
			buf0[addr_width + 0] = reg_val >> 8;
		}
		else
		{
			buf0[addr_width + 0] = reg_val & 0xff;
			buf0[addr_width + 1] = reg_val >> 8;
		}
	}
	else if (val_width == 4)
	{
		if (!msb_flag)
		{
			buf0[addr_width + 3] = reg_val & 0xff;
			buf0[addr_width + 2] = (reg_val >> 8) & 0xff;
			buf0[addr_width + 1] = (reg_val >> 16) & 0xff;
			buf0[addr_width + 0] = (reg_val >> 24) & 0xff;
		}
		else
		{
			buf0[addr_width + 0] = reg_val & 0xff;
			buf0[addr_width + 1] = (reg_val >> 8) & 0xff;
			buf0[addr_width + 2] = (reg_val >> 16) & 0xff;
			buf0[addr_width + 3] = (reg_val >> 24) & 0xff;
		}
	}
	else
	{
	}

	msg[0].addr = slave_addr;
	msg[0].flags = I2C_SMBUS_WRITE;
	msg[0].len = addr_width + val_width;
	msg[0].buf = (VIM_S8 *)buf0;

	msgst.msgs = msg;
	msgst.nmsgs = 1;

	if ((ret = ioctl(i2c_fd, I2C_RDWR, &msgst)) < 0)
	{
		close(i2c_fd);
		printf("ioctl msgs error, error number:%d,errno %x  %s\n", ret, errno, strerror(errno));
		return ret;
	}

	close(i2c_fd);

	return 0;
}

VIM_S32 i2cv_read(VIM_S8 *name,
				  VIM_S32 slave_addr,
				  VIM_U32 reg_addr,
				  VIM_U32 *reg_val,
				  VIM_S32 addr_width,
				  VIM_S32 val_width,
				  VIM_S32 msb_flag)
{
	VIM_S32 ret = 0;
	VIM_S8 buf0[2];
	VIM_S8 buf1[4];
	struct i2c_msg msg[2];
	VIM_S32 i2c_fd = -1;
	VIM_S8 flag = 0;

	if (!name)
		return_log(-1);

	/*1 open i2c device	*/
	i2c_fd = open((const char *)name, O_RDWR);
	if (i2c_fd < 0)
	{
		printf("open [%s] faild!\n", name);
		return -1;
	}

	/*2 set slave addr*/
	if (ioctl(i2c_fd, I2C_SLAVE_FORCE, slave_addr) < 0)
	{
		close(i2c_fd);
		printf("set slave addr error!\n");
		return -2;
	}

	/*3 read register	*/
	if (addr_width == 1)
	{
		msg[0].addr = slave_addr;
		msg[0].flags = I2C_SMBUS_WRITE;
		msg[0].len = 1;
		buf0[0] = reg_addr & 0xff;
		msg[0].buf = buf0;
	}
	else if (addr_width == 2)
	{
		msg[0].addr = slave_addr;
		msg[0].flags = I2C_SMBUS_WRITE;
		msg[0].len = 2;

		if (strstr((const char *)name, "-1") != NULL)
			flag = msb_flag;
		else
			flag = !msb_flag;
		if (flag)
		{
			buf0[1] = reg_addr & 0xff;
			buf0[0] = reg_addr >> 8;
		}
		else
		{
			buf0[1] = reg_addr & 0xff;
			buf0[0] = reg_addr >> 8;
		}
		msg[0].buf = buf0;
	}
	else
	{
	}

	if (val_width == 1)
	{
		msg[1].addr = slave_addr;
		msg[1].flags = I2C_SMBUS_READ;
		msg[1].len = 1;
		msg[1].buf = buf1;
	}
	else if (val_width == 2)
	{
		msg[1].addr = slave_addr;
		msg[1].flags = I2C_SMBUS_READ;
		msg[1].len = 2;
		msg[1].buf = buf1;
	}
	else if (val_width == 4)
	{
		msg[1].addr = slave_addr;
		msg[1].flags = I2C_SMBUS_READ;
		msg[1].len = 4;
		msg[1].buf = buf1;
	}
	else
	{
	}

	msgst.msgs = msg;
	msgst.nmsgs = 2;

	if ((ret = ioctl(i2c_fd, I2C_RDWR, &msgst)) < 0)
	{
		close(i2c_fd);
		printf("ioctl msgs error, error number:%d", ret);
		return -3;
	}

	if (val_width == 1)
	{
		*reg_val = buf1[0];
	}
	else if (val_width == 2)
	{
		if (!msb_flag)
			*reg_val = ((buf1[0] & 0xff) << 8) | (buf1[1] & 0xff);
		else
			*reg_val = ((buf1[1] & 0xff) << 8) | (buf1[0] & 0xff);
	}
	else if (val_width == 4)
	{
		if (!msb_flag)
			*reg_val = ((buf1[0] & 0xff) << 24) |
					   ((buf1[1] & 0xff) << 16) |
					   ((buf1[2] & 0xff) << 8) |
					   (buf1[3] & 0xff);
		else
			*reg_val = ((buf1[3] & 0xff) << 24) |
					   ((buf1[2] & 0xff) << 16) |
					   ((buf1[1] & 0xff) << 8) |
					   (buf1[0] & 0xff);
	}
	else
	{
	}

	close(i2c_fd);

	return 0;
}
