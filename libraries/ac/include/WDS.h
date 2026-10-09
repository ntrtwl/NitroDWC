#ifndef DWC_AC_WDS_H
#define DWC_AC_WDS_H

#include <nitro.h>

#include "ac/dwc_ac.h"

#define WDS_AP_INFO_COUNT 16

#define WDS_MAX_HOTSPOT_NAME_LENGTH 24

enum WDSInfoFlag {
    WDS_INFO_FLAG_NOTIFY = 1 << 0,
    WDS_INFO_FLAG_ALLOW_AUTOCONNECT = 1 << 1,
};

typedef struct WDSApInfo {
    u8 ssid[WM_SIZE_SSID];
    u8 apnum[AP_ID_LENGTH];
    u16 hotspotid;
    u8 hotspotname[WDS_MAX_HOTSPOT_NAME_LENGTH];
    u8 wepkey[DWC_WDS_WEPKEY_BUF_SIZE];
    u8 channel;
    u8 encryptflag;
    u8 infoflag;
    u8 reserve[5];
    u16 mtu;
    u16 crc;
} WDSApInfo;

typedef struct WDSBriefApInfo {
    BOOL isvalid;
    u16 rssi;
    WDSApInfo apinfo;
} WDSBriefApInfo;

u32 WDS_GetWorkAreaSize(void);
int WDS_Initialize(void *wdsWork, WMCallbackFunc callback, u16 dmaNo);
int WDS_InitializeEx(void *wdsWork, WMCallbackFunc callback, u16 dmaNo, WDSBriefApInfo *apinfo);
int WDS_End(WMCallbackFunc callback);
int WDS_StartScan(WMCallbackFunc callback);
int WDS_EndScan(WMCallbackFunc callback);
int WDS_GetApInfoNum(void);
int WDS_GetApInfoByIndex(int index, WDSBriefApInfo *briefapinfo);
int WDS_GetApInfoAll(WDSBriefApInfo *briefapinfo);
int WDS_SetConnectTargetByIndex(int index);
int WDS_SetConnectTargetByBriefApInfo(WDSBriefApInfo *briefapinfo);
int WDS_GetApDescriptionUTF16(WDSBriefApInfo *briefapinfo, void *outbuf);

#endif // DWC_AC_WDS_H
