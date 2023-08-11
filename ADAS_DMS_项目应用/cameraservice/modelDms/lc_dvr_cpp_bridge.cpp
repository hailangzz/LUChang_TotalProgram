#include "DMS_interface.h"
#include "ADAS_interface.h"

extern "C" int lc_dvr_bridge_dms_test(void* image, int chanel, int *result);
extern "C" int lc_dvr_bridge_dms_initial(void);
extern "C" int lc_dvr_bridge_dms_unInitial(void);


extern int dms_function_test(void* paraIn, int chl, int *result);
extern int dms_function_unInitial(void);
extern int dms_function_initial(void);

int lc_dvr_bridge_dms_test(void* image, int chanel, int *result)
{
	if(image!=NULL)
	{
		dms_function_test(image, chanel, result);
	}
	return 0;
}

int lc_dvr_bridge_dms_initial(void)
{
	dms_function_initial();
	return 0;
}

int lc_dvr_bridge_dms_unInitial(void)
{
	dms_function_unInitial();
	return 0;
}


extern "C" int lc_dvr_bridge_adas_test(void* image, int chanel, int *result);
extern "C" int lc_dvr_bridge_adas_initial(void);
extern "C" int lc_dvr_bridge_adas_unInitial(void);


extern int adas_function_test(void* paraIn, int chl, int *result);
extern int adas_function_unInitial(void);
extern int adas_function_initial(void);

int lc_dvr_bridge_adas_test(void* image, int chanel, int *result)
{
	if(image!=NULL)
	{
		adas_function_test(image, chanel, result);
	}
	return 0;
}

int lc_dvr_bridge_adas_initial(void)
{
	adas_function_initial();
	return 0;
}

int lc_dvr_bridge_adas_unInitial(void)
{
	adas_function_unInitial();
	return 0;
}

