#include "ac/dwc_ac.h"

#include <auth/dwc_netcheck.h>
#include <bm/dwc_backup.h>
#include <nitro.h>
#include <nitroWiFi.h>

#include "ac.h"
#include "close.h"
#include "connect.h"
#include "error.h"
#include "retry.h"
#include "search.h"
#include "start.h"
#include "test.h"

static ACHOLD *acHold = NULL;
static ACWORK *acWork;
static void *wcmBuffer;
static SOCConfig *socConfig;
static DWCNetcheckParam *netCheck;

static void Free_Disused(void);
static int CheckDuplicate(void);

BOOL DWC_AC_Create(DWCACConfig *config)
{
    acHold = config->alloc(AC_NAME_HOLD, sizeof(ACHOLD));
    MI_CpuClear32(acHold, sizeof(ACHOLD));

    ACHOLD *hd = acHold;
    hd->alloc = config->alloc;
    hd->free = config->free;
    hd->phase = AC_PHASE_START;
    hd->endSequence = AC_PHASE_START;
    hd->allocState = AC_NAME_HOLD;

    acWork = DWCi_AC_Alloc(AC_NAME_WORK, sizeof(ACWORK));
    wcmBuffer = DWCi_AC_Alloc(AC_NAME_WCM_BUFFER, WCM_WORK_SIZE);
    socConfig = DWCi_AC_Alloc(AC_NAME_SOC_CONFIG, sizeof(SOCConfig));
    netCheck = DWCi_AC_Alloc(AC_NAME_NETCHECK, sizeof(DWCNetcheckParam));

    MI_CpuClear32(acWork, sizeof(ACWORK));
    MI_CpuClear32(wcmBuffer, WCM_WORK_SIZE);
    MI_CpuClear32(socConfig, sizeof(SOCConfig));
    MI_CpuClear32(netCheck, sizeof(DWCNetcheckParam));

    ACWORK *wk = acWork;
    wk->dmaNo = config->dmaNo;
    wk->powerMode = config->powerMode;

    DWCNetcheckParam *nc = netCheck;
    nc->alloc = config->alloc;
    nc->free = config->free;
    nc->bmworkarea = NULL;

    wk->connectType = config->option.connectType;
    wk->skipNetCheck = config->option.skipNetCheck;

    DWCi_BM_GetApInfo(wk->userInfo);

    int result = WCM_Init(wcmBuffer, WCM_WORK_SIZE);
    if (result == WCM_RESULT_FAILURE || result > WCM_RESULT_REJECT) {
        DWCi_AC_FreeAll();
        return FALSE;
    }
    return TRUE;
}

int DWC_AC_Process(void)
{
    u8 phase = DWCi_AC_GetPhase();

    if (phase == AC_PHASE_START) {
        phase = DWCi_AC_Start();
    } else if (phase < AC_PHASE_CONNECT_START) {
        OSIntrMode irq = OS_DisableInterrupts();
        phase = DWCi_AC_SearchAP();
        DWCi_AC_SetPhase(phase);
        OS_RestoreInterrupts(irq);
    } else if (phase < AC_PHASE_CONNECT_RETRY) {
        phase = DWCi_AC_ConnectAP();
    } else if (phase < AC_PHASE_CONNECT_TEST_START) {
        phase = DWCi_AC_ConnectRetryAP();
    } else if (phase < AC_PHASE_COMPLETE) {
        phase = DWCi_AC_ConnectTest();
    } else if (phase == AC_PHASE_ERROR) {
        phase = DWCi_AC_Error();
    }

    DWCi_AC_SetPhase(phase);

    if (phase == AC_PHASE_COMPLETE) {
        int ret = CheckDuplicate();
        Free_Disused();
        return ret;
    } else if (phase == AC_PHASE_FATAL_ERROR) {
        Free_Disused();
        return -1;
    }

    return 0;
}

int DWC_AC_GetStatus(void)
{
    int ret;
    u8 phase = DWCi_AC_GetPhase();

    if (phase <= AC_PHASE_START) {
        ret = DWC_AC_STATE_NULL;
    } else if (phase < AC_PHASE_CONNECT_START) {
        ret = DWC_AC_STATE_SEARCH;
    } else if (phase == AC_PHASE_CONNECT_RETRY) {
        ret = DWC_AC_STATE_RETRY;
    } else if (phase < AC_PHASE_CONNECT_TEST_START) {
        ret = DWC_AC_STATE_CONNECT;
    } else if (phase == AC_PHASE_CONNECT_TEST_RETRY) {
        ret = DWC_AC_STATE_RETRY;
    } else if (phase < AC_PHASE_COMPLETE) {
        ret = DWC_AC_STATE_TEST;
    } else if (phase == AC_PHASE_COMPLETE) {
        ret = DWC_AC_STATE_COMPLETE;
    } else if (phase == AC_PHASE_ERROR) {
        ret = DWC_AC_STATE_RETRY;
    } else {
        ret = DWCi_AC_GetResult();
    }

    return ret;
}

u8 DWC_AC_GetApType(void)
{
    u8 type = DWC_AC_AP_TYPE_FALSE;
    u8 phase = DWCi_AC_GetPhase();

    if (AC_PHASE_CONNECT_TEST_START <= phase && phase <= AC_PHASE_COMPLETE) {
        type = acHold->type;
    }

    return type;
}

BOOL DWC_AC_GetApSpotInfo(u8 *apSpotInfo)
{
    BOOL result = FALSE;
    u8 phase = DWCi_AC_GetPhase();

    if (AC_PHASE_CONNECT_TEST_START <= phase && phase <= AC_PHASE_COMPLETE
        && (acHold->type == DWC_AC_AP_TYPE_SHOP || acHold->type == DWC_AC_AP_TYPE_NINTENDOSPOT)) {
        MI_CpuCopy8(acHold->apSpotInfo, apSpotInfo, AP_ID_LENGTH);
        result = TRUE;
    }

    return result;
}

BOOL DWC_AC_Destroy(void)
{
    u8 phase = DWCi_AC_GetPhase();

    if (phase == AC_PHASE_NONE || phase == AC_PHASE_FATAL_ERROR) {
        DWCi_AC_FreeAll();
        return TRUE;
    }

    DWCi_AC_CloseNetwork(&phase);
    DWCi_AC_SetPhase(phase);

    return FALSE;
}

void DWC_AC_SetSpecifyApEx(const void *ssid, const void *wep, int wepMode, const char *apSpotInfo, int overrideType)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);

    if (apSpotInfo != NULL) {
        MI_CpuCopy8(apSpotInfo, hd->apSpotInfo, AP_ID_LENGTH);
    } else {
        MI_CpuClear8(hd->apSpotInfo, AP_ID_LENGTH);
    }

    hd->overrideType = overrideType;
    DWC_AC_SetSpecifyAp(ssid, wep, wepMode);
}

void DWC_AC_SetSpecifyAp(const void *ssid, const void *wep, int wepMode)
{
    u8 *p = ssid;

    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    wk->connectType = DWC_AC_AP_TYPE_USER1 + 1;
    MI_CpuClear8(&wk->userInfo[0].ap, sizeof(DWCBMApInfo));

    for (int i = 0; i < WM_SIZE_SSID; i++) {
        if (p[i] == '\0') {
            break;
        }

        wk->userInfo[0].ap.ssid[SSID_NORMAL][i] = p[i];
    }

    if (wep == NULL || wepMode == WM_WEPMODE_NO) {
        wk->userInfo[0].ap.wepMode = WM_WEPMODE_NO;
        return;
    }

    u32 len;
    if (wepMode == WM_WEPMODE_40BIT) {
        len = 5;
    } else if (wepMode == WM_WEPMODE_104BIT) {
        len = 13;
    } else {
        len = 16;
    }

    MI_CpuCopy8(wep, wk->userInfo[0].ap.wep, len);
    wk->userInfo[0].ap.wepMode = wepMode;
    return;
}

BOOL DWC_AC_CheckWiFiStation(const void *ssid, u16 len)
{
    if (len != WM_SIZE_SSID) {
        return FALSE;
    }

    return DWCi_AC_CheckNintendoShopAP(ssid);
}

void DWCi_AC_InsertApInfo(int no, DWCBMApInfo *info)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    MI_CpuCopy32(info, &wk->userInfo[no].ap, sizeof(DWCBMApInfo));
}

void *DWCi_AC_Alloc(u32 name, s32 size)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    if ((hd->allocState & name) == 0) {
        hd->allocState |= name;
        return hd->alloc(name, size);
    } else {
        return NULL;
    }
}

void DWCi_AC_Free(u32 name, void *ptr, s32 size)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    if (hd && (hd->allocState & name) != 0) {
        hd->allocState &= ~name;
        hd->free(name, ptr, size);

        if (name == AC_NAME_HOLD) {
            acHold = NULL;
        }
    }
}

void DWCi_AC_FreeAll(void)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    if (hd == NULL) {
        return;
    }

    if ((hd->allocState & AC_NAME_WORK) != 0) {
        ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
        hd->allocState &= ~AC_NAME_WORK;
        hd->free(AC_NAME_WORK, wk, sizeof(ACWORK));
    }

    if ((hd->allocState & AC_NAME_NETCHECK) != 0) {
        DWCNetcheckParam *wk = DWCi_AC_GetMemPtr(AC_NAME_NETCHECK);
        hd->allocState &= ~AC_NAME_NETCHECK;
        hd->free(AC_NAME_NETCHECK, wk, sizeof(DWCNetcheckParam));
    }

    if ((hd->allocState & AC_NAME_SOC_CONFIG) != 0) {
        SOCConfig *wk = DWCi_AC_GetMemPtr(AC_NAME_SOC_CONFIG);
        hd->allocState &= ~AC_NAME_SOC_CONFIG;
        hd->free(AC_NAME_SOC_CONFIG, wk, sizeof(SOCConfig));
    }

    if ((hd->allocState & AC_NAME_WCM_BUFFER) != 0) {
        void *wk = DWCi_AC_GetMemPtr(AC_NAME_WCM_BUFFER);
        hd->allocState &= ~AC_NAME_WCM_BUFFER;
        hd->free(AC_NAME_WCM_BUFFER, wk, WCM_WORK_SIZE);
    }

    if ((hd->allocState & AC_NAME_HOLD) != 0) {
        hd->allocState &= ~AC_NAME_HOLD;
        hd->free(AC_NAME_HOLD, hd, sizeof(ACHOLD));
        acHold = NULL;
    }
}

void *DWCi_AC_GetMemPtr(u32 name)
{
    if ((name & AC_NAME_HOLD) != 0) {
        return acHold;
    } else if ((name & AC_NAME_WCM_BUFFER) != 0) {
        return wcmBuffer;
    } else if ((name & AC_NAME_SOC_CONFIG) != 0) {
        return socConfig;
    } else if ((name & AC_NAME_NETCHECK) != 0) {
        return netCheck;
    } else if ((name & AC_NAME_WORK) != 0) {
        return acWork;
    } else {
        return NULL;
    }
}

void DWCi_AC_SetPhase(u8 phase)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    hd->phase = phase;

    if (phase < AC_PHASE_COMPLETE && phase > hd->endSequence) {
        hd->endSequence = phase;

        if (phase > AC_PHASE_CONNECT_START) {
            hd->endType = DWCi_ConvConnectAPType(wk->connectApType);
            hd->endState = wk->findList[wk->connectNo].state;
        }
    }
}

u8 DWCi_AC_GetPhase(void)
{
    if (acHold != NULL) {
        return acHold->phase;
    } else {
        return AC_PHASE_NONE;
    }
}

void DWCi_AC_SetError(int error)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);

    hd->error = error;
    hd->phaseError = DWCi_AC_GetPhase();
}

int DWCi_AC_GetError(void)
{
    return acHold->error;
}

void DWCi_AC_SetApType(u8 type)
{
    acHold->type = acHold->overrideType != 0 ? acHold->overrideType : DWCi_ConvConnectAPType(type);

    u8 *essid = WCM_GetApEssid(NULL);
    if (essid != NULL) {
        DC_InvalidateRange(essid, WM_SIZE_SSID);
        DWCi_AC_GetPostalCode(essid, acHold->apSpotInfo);
    }

    for (int i = 0; i < AP_ID_LENGTH; i++) {
        if (acHold->apSpotInfo[i] < ' ' || acHold->apSpotInfo[i] > '~') {
            MI_CpuClear8(acHold->apSpotInfo, AP_ID_LENGTH);
            return;
        }
    }
}

u8 DWCi_ConvConnectAPType(u8 type)
{
    if (type > AP_TYPE_USER3) {
        return type - AP_TYPE_AOSS_USER1;
    } else {
        return type;
    }
}

static void Free_Disused(void)
{
    DWCi_AC_Free(AC_NAME_NETCHECK, netCheck, sizeof(DWCNetcheckParam));
    DWCi_AC_Free(AC_NAME_WORK, acWork, sizeof(ACWORK));
}

static int CheckDuplicate(void)
{
    ACWORK *wk = acWork;
    u8 *ssid1 = wk->bssDesc[wk->connectNo].bssid;

    if (wk->connectApType >= AP_TYPE_USB) {
        return 1;
    }

    for (u8 i = 0; i < wk->findListNum; i++) {
        if (i != wk->connectNo && wk->findList[i].type < AP_TYPE_USB) {
            u16 length = wk->bssDesc[i].ssidLength;
            u8 *ssid2 = wk->bssDesc[i].bssid;
            if (strncmp(ssid1, ssid2, length) == 0) {
                return 2;
            }
        }
    }

    return 1;
}
