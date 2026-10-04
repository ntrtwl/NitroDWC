#include "connect.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "ac.h"
#include "shop_usb.h"

static u8 ConnectStart(ACWORK *wk);
static u8 ConnectAP(ACWORK *wk);
static u8 GetConnectType(ACWORK *wk);
static u32 GetPowerMode(ACWORK *wk);
static u32 GetAuthMode(ACWORK *wk);
static BOOL GetWepKey(ACWORK *wk, u8 type, WCMWepDesc *wepkey);

u8 DWCi_AC_ConnectAP(void)
{
    u8 phase = DWCi_AC_GetPhase();
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    switch (phase) {
    case AC_PHASE_CONNECT_START:
        phase = ConnectStart(wk);
        break;
    case AC_PHASE_CONNECT_AP:
        phase = ConnectAP(wk);
        break;
    default:
        break;
    }
    return phase;
}

static u8 ConnectStart(ACWORK *wk)
{
    WMBssDesc *bssdesc = &wk->bssDesc[wk->connectNo];
    wk->connectApType = GetConnectType(wk);

    MI_CpuClear8(&wk->wepKey, sizeof(WCMWepDesc));

    if (GetWepKey(wk, wk->connectApType, &wk->wepKey)) {
        wk->authMode = TRUE;
        if (((bssdesc->capaInfo >> 4) & 1) == FALSE) {
            wk->findList[wk->connectNo].state = AP_WEP_FAILURE;
            return AC_PHASE_CONNECT_RETRY;
        }
        if (wk->connectApType == AP_TYPE_USB && bssdesc->ssid[NINTENDO_USB_CONNECT_FLAG] == 0) {
            wk->findList[wk->connectNo].state = AP_WEP_FAILURE;
            return AC_PHASE_CONNECT_RETRY;
        }
    } else {
        wk->authMode = FALSE;
        if (((bssdesc->capaInfo >> 4) & 1) == TRUE) {
            wk->findList[wk->connectNo].state = AP_WEP_FAILURE;
            return AC_PHASE_CONNECT_RETRY;
        }
    }

    wk->count = 0;
    wk->connectResult = AC_CONNECT_START;
    return AC_PHASE_CONNECT_AP;
}

static u8 ConnectAP(ACWORK *wk)
{
    int phase = WCM_GetPhase();
    WMBssDesc *bssdesc = &wk->bssDesc[wk->connectNo];

    if (phase == WCM_PHASE_IDLE) {
        u32 power_mode = GetPowerMode(wk);

        wk->count++;
        if (wk->count > 3) {
            wk->count = 0;
            wk->findList[wk->connectNo].state = AP_CANT_CONNECT;
            return AC_PHASE_CONNECT_RETRY;
        }

        if (wk->count != 1) {
            if (wk->connectResult == AC_CONNECT_FAILURE_AUTH) {
                wk->authMode = FALSE;
            } else if (wk->connectResult == AC_CONNECT_FAILURE_WEP) {
                wk->count = 0;
                wk->findList[wk->connectNo].state = AP_WEP_FAILURE;
                return AC_PHASE_CONNECT_RETRY;
            } else if (wk->connectResult == AC_CONNECT_FAILURE_CAPACITY) {
                wk->count = 0;
                wk->findList[wk->connectNo].state = AP_OVER_CAPACITY;
                return AC_PHASE_CONNECT_RETRY;
            } else if (wk->count == 3) {
                wk->authMode = FALSE;
            }
        }

        u32 auth_mode = GetAuthMode(wk);
        WCM_ConnectAsync(bssdesc, &wk->wepKey, power_mode | auth_mode);
    } else if (phase == WCM_PHASE_DCF) {
        wk->count = 0;
        wk->timeOut = OS_GetTick();
        return AC_PHASE_CONNECT_TEST_START;
    }

    return AC_PHASE_CONNECT_AP;
}

static u8 GetConnectType(ACWORK *wk)
{
    WMBssDesc *bssdesc = &wk->bssDesc[wk->connectNo];
    int type = 0;
    int i, j;

    if (!wk->duplicateFlag) {
        j = 0;
        if (bssdesc->ssidLength == WM_SIZE_SSID) {
            type = DWCi_AC_CheckNintendoSSID(bssdesc);
            if (type > 0) {
                j++;
            } else {
                type = 0;
            }
        } else if (bssdesc->ssidLength == 8) {
            type = DWCi_AC_CheckFreespot(bssdesc);

            if (type != 0) {
                j++;
            } else {
                type = 0;
            }
        }

        for (i = 0; i < wk->searchListNum; i++) {
            if (bssdesc->ssidLength == wk->searchList[i].length) {
                if (strncmp(bssdesc->ssid, wk->searchList[i].ssid, bssdesc->ssidLength) == 0) {
                    if (j == 0) {
                        type = wk->searchList[i].type;
                    } else {
                        wk->searchList[i].duplicate = TRUE;
                        wk->duplicateFlag = TRUE;
                    }
                    j++;
                }
            }
        }
    } else {
        for (i = 0, j = 0; i < wk->searchListNum; i++) {
            if (wk->searchList[i].duplicate == TRUE) {
                if (j == 0) {
                    wk->searchList[i].duplicate = FALSE;
                    type = wk->searchList[i].type;
                }
                j++;
            }
        }

        if (j == 1) {
            wk->duplicateFlag = FALSE;
        }
    }

    return type;
}

static u32 GetPowerMode(ACWORK *wk)
{
    if (wk->powerMode == TRUE) {
        return WCM_OPTION_POWER_ACTIVE;
    } else {
        return WCM_OPTION_POWER_SAVE;
    }
}

static u32 GetAuthMode(ACWORK *wk)
{
    if (wk->authMode == TRUE) {
        return WCM_OPTION_AUTH_SHAREDKEY;
    } else {
        return WCM_OPTION_AUTH_OPENSYSTEM;
    }
}

static BOOL GetWepKey(ACWORK *wk, u8 type, WCMWepDesc *wepkey)
{
    DWCMemPage *info = wk->userInfo;

    switch (type) {
    case AP_TYPE_USER3:
        info++;
    case AP_TYPE_USER2:
        info++;
    case AP_TYPE_USER1:
        wepkey->mode = info->ap.wepMode;
        MI_CpuCopy8(&info->ap.wep[0][0], wepkey->key, 16);
        break;
    case AP_TYPE_AOSS_USER3:
        info++;
    case AP_TYPE_AOSS_USER2:
        info++;
    case AP_TYPE_AOSS_USER1:
        wepkey->mode = WM_WEPMODE_40BIT;
        MI_CpuCopy8(&info->ap.wep2[0][0], wepkey->key, 5);
        break;
    case AP_TYPE_USB:
        wepkey->mode = WM_WEPMODE_104BIT;
        DWCi_AC_GetNintendoUSBWepKey(wk->bssDesc[wk->connectNo].ssid, wepkey->key);
        break;
    case AP_TYPE_SHOP:
        wepkey->mode = WM_WEPMODE_104BIT;
        DWCi_AC_GetNintendoShopWepKey(wk->bssDesc[wk->connectNo].ssid, wepkey->key);
        break;
    case AP_TYPE_FREESPOT:
    case AP_TYPE_WAYPORT:
    default:
        break;
    }

    return wepkey->mode != WM_WEPMODE_NO;
}
