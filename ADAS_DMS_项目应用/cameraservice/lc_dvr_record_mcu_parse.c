#include "lc_dvr_comm_def.h"
//==============================================
#include "lc_dvr_record_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/types.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <private/android_filesystem_config.h>
#include "binder.h"
#include "settings.h"
#include "settingclient.h"
#include "mcu_protocol_client.h"
//==============================================
static int mMcuConnect;
static int mMcuNotify;

int lc_dvr_mcu_get_speed(void)
{
	int speed = 0;
	if(mMcuConnect>=0)
	{
		speed = get_car_speed();
	}
	// printf("car speed %d\n", speed);
	return speed;
}

void lc_dvr_algorithem_notify(int data)
{
	if(mMcuNotify>=0)
	{
		settings_setprop(SETTINGS_PROP_DMS_EVENT, data);
	}	
}

void lc_dvr_storageErr_notify(int data)
{
	if(mMcuNotify>=0)
	{
		settings_setprop(SETTINGS_PROP_STORAGE_DEV_FAIL, data);
	}
}

void lc_dvr_camErr_notify(int data)
{
	if(mMcuNotify>=0)
	{
		settings_setprop(SETTINGS_PROP_CAMERA_LOSS, data);
	}
}

void lc_dvr_mcu_notify(int data)
{
	if(mMcuNotify>=0)
	{
		settings_setprop(SETTINGS_PROP_VIDEO_REC_STATE, data);
	}
}

void lc_dvr_mcu_connect_initial(void)
{
	mMcuConnect = connect_to_mcuprotocol_service();
	if (mMcuConnect < 0) {
		printf("connect to mcuprotocol server failed\n");
	}

	mMcuNotify = connect_to_settings_service();
	if (mMcuNotify < 0) {
		loge_lc("connect to setting client failed\n");
	}	
}

void lc_dvr_mcu_connect_uninitial(void)
{
	disconnect_to_mcuprotocol_service();
	disconnect_to_settings_service();
	mMcuConnect = -1;
	mMcuNotify = -1;
}

void lc_dvr_mcu_emergency_size(int rate)
{
	settings_setprop(SETTINGS_PROP_EMERGENCY_SIZE_EVENT, rate);
}

