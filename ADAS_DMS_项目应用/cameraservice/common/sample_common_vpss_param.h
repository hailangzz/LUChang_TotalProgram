#ifndef __DO_PARAM__H_
#define __DO_PARAM__H_

#include "vim_type.h"
#include <iniparser.h>
#include "dictionary.h"
#include "ini_ctype.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

VIM_BOOL __GetBoolFromIniBykey(dictionary *ini, char *keyname);
VIM_U32 __GetInputTypeFromIniBykey(dictionary *ini, char *keyname);
VIM_U32 __GetInputFormatFromIniBykey(dictionary *ini, char *keyname);
VIM_U32 __GetInputBitFromIniBykey(dictionary *ini, char *keyname);
VIM_U32 __GetInputYcbcrFromIniBykey(dictionary *ini, char *keyname);
VIM_S32 PARAM_GetManCtrlInfoFromIni(char *iniName, VIM_U32 Mask, VIM_U32 u32GrpId);
VIM_S32 PARAM_GetGroup_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini, VIM_U32 Mask);
VIM_S32 PARAM_GetChnOut_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini);
VIM_S32 PARAM_GetGrpIn_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



#endif
