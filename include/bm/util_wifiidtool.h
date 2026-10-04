#ifndef DWC_UTIL_WIFIIDTOOL_H_
#define DWC_UTIL_WIFIIDTOOL_H_

#include <nitro.h>

#include "bm/dwc_backup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u64 uId;
    u64 notAttestedId;
    u32 flag;
} DWCAuthWiFiId;

BOOL DWCi_AUTH_GetNewWiFiInfo(DWCWiFiInfo *wifiinfo);
BOOL DWCi_AUTH_MakeWiFiID(void *work);
void DWC_Auth_GetId(DWCAuthWiFiId *id);
BOOL DWC_Auth_CheckPseudoWiFiID(void);
BOOL DWC_Auth_CheckWiFiIDNeedCreate(void);

#ifdef __cplusplus
}
#endif

#endif
