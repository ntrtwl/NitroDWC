#include <nitro.h>
#include <base/dwc_memfunc.h>

#include "ac/dwc_ac.h"

#include "WDS.h"

typedef struct DWCWDSWork {
    DWCWDSData *nspotInfoBuf;
    OSTick wdsScanBeginTick;
    WDSBriefApInfo briefapinfo[WDS_AP_INFO_COUNT];
    int selectedIndex;
} DWCWDSWork;

typedef enum {
    APP_STATE_WDSINIT = 0,
    APP_STATE_WDSWAITINIT,
    APP_STATE_WDSSCAN,
    APP_STATE_WDSWAITSCAN,
    APP_STATE_WDSCOMPLETESCAN,
    APP_STATE_WDSENDSCAN,
    APP_STATE_WDSWAITENDSCAN,
    APP_STATE_WDSCOMPLETEENDSCAN,
    APP_STATE_WDSWAITEND,
    APP_STATE_WDSCOMPLETEEND,
    APP_STATE_ERRORENDPROCESS,
    APP_STATE_ERROREND,
} AppState;

static void *wdsSysBuf = NULL;
static DWCWDSWork *gDwcWdsWork = NULL;
static AppState gAppState;

static BOOL IsValidApnum(u8 *apnum);
static void WDS_Initialize_CB(void *arg);
static void WDS_StartScan_CB(void *arg);
static void WDS_EndScan_CB(void *arg);
static void WDS_End_CB(void *arg);
static void WDS_Error_End_CB(void *arg);

BOOL DWC_AC_StartupGetWDSInfo(DWCWDSData *nspotInfo)
{
    wdsSysBuf = DWC_Alloc(DWC_ALLOCTYPE_AC, WDS_GetWorkAreaSize());

    gDwcWdsWork = DWC_Alloc(DWC_ALLOCTYPE_AC, sizeof(DWCWDSWork));
    MI_CpuClear8(gDwcWdsWork, sizeof(DWCWDSWork));
    gDwcWdsWork->nspotInfoBuf = nspotInfo;

    gAppState = APP_STATE_WDSINIT;
    return TRUE;
}

DWCWDSState DWC_AC_ProcessGetWDSInfo(void)
{
    int i;
    u16 maxRssi;

    if (wdsSysBuf == NULL || gDwcWdsWork == NULL) {
        return DWC_WDS_STATE_FAILED;
    }

    int result = DWC_WDS_STATE_PROCESS;

    switch (gAppState) {
    case APP_STATE_WDSINIT:
        gAppState = APP_STATE_WDSWAITINIT;
        if (WDS_Initialize(wdsSysBuf, WDS_Initialize_CB, 0) != WM_ERRCODE_SUCCESS) {
            gAppState = APP_STATE_ERROREND;
        }
        break;
    case APP_STATE_WDSSCAN:
        gAppState = APP_STATE_WDSWAITSCAN;
        if (WDS_StartScan(WDS_StartScan_CB) == WM_ERRCODE_SUCCESS) {
            if (gDwcWdsWork->wdsScanBeginTick == 0) {
                gDwcWdsWork->wdsScanBeginTick = OS_GetTick();
            }
        } else {
            gAppState = APP_STATE_ERRORENDPROCESS;
        }
        break;
    case APP_STATE_WDSCOMPLETESCAN:
        if (OS_TicksToMilliSeconds(OS_GetTick() - gDwcWdsWork->wdsScanBeginTick) < 3000) {
            gAppState = APP_STATE_WDSSCAN;
        } else {
            gAppState = APP_STATE_WDSENDSCAN;
        }
        break;
    case APP_STATE_WDSENDSCAN:
        gAppState = APP_STATE_WDSWAITENDSCAN;
        if (WDS_EndScan(WDS_EndScan_CB) != WM_ERRCODE_SUCCESS) {
            gAppState = APP_STATE_ERRORENDPROCESS;
        }
        break;
    case APP_STATE_WDSCOMPLETEENDSCAN:
        if (WDS_GetApInfoAll(gDwcWdsWork->briefapinfo) != 0) {
            gAppState = APP_STATE_ERRORENDPROCESS;
        }

        for (i = 0, maxRssi = 0, gDwcWdsWork->selectedIndex = -1; i < WDS_AP_INFO_COUNT; i++) {
            if (gDwcWdsWork->briefapinfo[i].isvalid
                && gDwcWdsWork->briefapinfo[i].apinfo.infoflag & WDS_INFO_FLAG_ALLOW_AUTOCONNECT
                && gDwcWdsWork->briefapinfo[i].apinfo.encryptflag <= WM_WEPMODE_128BIT
                && IsValidApnum(gDwcWdsWork->briefapinfo[i].apinfo.apnum)
                && gDwcWdsWork->briefapinfo[i].rssi >= maxRssi) {
                gDwcWdsWork->selectedIndex = i;
                maxRssi = gDwcWdsWork->briefapinfo[i].rssi;
            }
        }

        gAppState = APP_STATE_WDSWAITEND;
        if (WDS_End(WDS_End_CB) != WM_ERRCODE_SUCCESS) {
            gAppState = APP_STATE_ERROREND;
        }
        break;
    case APP_STATE_WDSCOMPLETEEND:
        if (gDwcWdsWork->selectedIndex < 0) {
            gAppState = APP_STATE_ERROREND;
        } else {
            MI_CpuCopy8(&gDwcWdsWork->briefapinfo[gDwcWdsWork->selectedIndex].apinfo.ssid, gDwcWdsWork->nspotInfoBuf->ssid, WM_SIZE_SSID);
            MI_CpuCopy8(&gDwcWdsWork->briefapinfo[gDwcWdsWork->selectedIndex].apinfo.wepkey, gDwcWdsWork->nspotInfoBuf->wep, DWC_WDS_WEPKEY_BUF_SIZE);
            gDwcWdsWork->nspotInfoBuf->wepMode = gDwcWdsWork->briefapinfo[gDwcWdsWork->selectedIndex].apinfo.encryptflag;
            MI_CpuCopy8(&gDwcWdsWork->briefapinfo[gDwcWdsWork->selectedIndex].apinfo.apnum, gDwcWdsWork->nspotInfoBuf->apnum, AP_ID_LENGTH);
            result = DWC_WDS_STATE_COMPLETED;
        }
        break;
    case APP_STATE_ERRORENDPROCESS:
        gAppState = APP_STATE_WDSWAITEND;
        if (WDS_End(WDS_Error_End_CB) != WM_ERRCODE_SUCCESS) {
            gAppState = APP_STATE_ERROREND;
        }
        break;
    case APP_STATE_ERROREND:
        result = DWC_WDS_STATE_FAILED;
        break;
    default:
        break;
    }

    return result;
}

void DWC_AC_CleanupGetWDSInfo(void)
{
    if (wdsSysBuf != NULL) {
        DWC_Free(DWC_ALLOCTYPE_AC, wdsSysBuf, 0);
        wdsSysBuf = NULL;
    }
    if (gDwcWdsWork != NULL) {
        DWC_Free(DWC_ALLOCTYPE_AC, gDwcWdsWork, 0);
        gDwcWdsWork = NULL;
    }
}

static BOOL IsValidApnum(u8 *apnum)
{
    for (int i = 0; i < AP_ID_LENGTH; i++) {
        if (apnum[i] < ' ' || '~' < apnum[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

static void WDS_Initialize_CB(void *arg)
{
    gAppState = APP_STATE_WDSSCAN;
}

static void WDS_StartScan_CB(void *arg)
{
    gAppState = APP_STATE_WDSCOMPLETESCAN;
}

static void WDS_EndScan_CB(void *arg)
{
    gAppState = APP_STATE_WDSCOMPLETEENDSCAN;
}

static void WDS_End_CB(void *arg)
{
    gAppState = APP_STATE_WDSCOMPLETEEND;
}

static void WDS_Error_End_CB(void *arg)
{
    gAppState = APP_STATE_ERROREND;
}
