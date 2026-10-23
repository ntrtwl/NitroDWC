#ifndef DWC_CAL_H
#define DWC_CAL_H

#include <bm/dwc_backup.h>
#include <nitro.h>

#include "auth/dwc_http.h"

typedef struct DWCCalInfo {
    char userid[14];
    char passwd[7];
    char gamecd[5];
    char makercd[3];
    char unitcd[2];
    char macaddr[13];
    char lang[3];
    char birth[5];
    char devtime[13];
    char bssid[13];
    char ssid[33];
    char apinfo[14];
    u16 devname[11];
} DWCCalInfo;

BOOL DWC_Auth_GetCalInfoFromNVRAM(DWCCalInfo *cal);
BOOL DWC_Auth_GetCalInfoFromWiFiInfo(DWCCalInfo *cal, DWCWiFiInfo *wifiinfo);
BOOL DWC_Auth_SetCalInfoToHttp(DWCHttp *http, DWCCalInfo *calinfo, BOOL allowessid);

#endif // DWC_CAL_H
