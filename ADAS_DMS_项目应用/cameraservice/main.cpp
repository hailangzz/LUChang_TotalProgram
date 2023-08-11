#include "lc_dvr_comm_def.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>    /**PTHREAD_CREATE_DETACHED**/
#include "lc_glob_type.h"
#include "dvr_event_loop.h"

#define MAINVER 1
#define SUBVER1 0
#define SUBVER2 1
 
#define STR(s)     #s 
#define VERSION(a,b,c)  "RRvtdr V" STR(a) "." STR(b) "." STR(c) " "__DATE__ " "__TIME__
// __DATE__  /**日期的宏定义**/
// __TIME__  /**日期的宏定义**/
//#define VERSTR  "System V2.0.1.2017.9.13"

int main(void)
{
	printf("------------------------------------------------\n");
	printf("Soft Date:%s\n",VERSION(MAINVER,SUBVER1,SUBVER2));
	printf("------------------------------------------------\n");
	// test_algorithem_func();
	dvr_event_loop();
	return 0;
}
