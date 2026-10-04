#include "bm/util_wifiidtool.h"

#include <nitro.h>

#include "bm/dwc_backup.h"

#define DWC_UTIL_WIFI_ID_KEY_1 0xD6
#define DWC_UTIL_WIFI_ID_KEY_2 0x67

const u8 DWCi_util_wifiid_ttable[16] = { 5, 9, 1, 14, 12, 2, 10, 0, 11, 13, 3, 4, 8, 6, 15, 7 };
const u8 DWCi_util_wifiid_exctable[8] = { 1, 2, 0, 4, 3, 5, 6, 7 };

typedef union DWCUtilUni64 {
    u64 data;
    u32 half[2];
    u8 i[8];
} DWCUtilUni64;

static u64 DWCi_Util_WiFiId_scrambleUid(u16 rand, u32 mac, u8 vend, u8 mainunit)
{
    DWCUtilUni64 uni64;
    u8 uidtemp[8];
    int i;

    uni64.data = ((u64)mainunit & 3) | ((u64)vend & 1) << 2 | ((u64)mac & 0xFFFFFF) << 3 | ((u64)rand & 0xFFFF) << 27;

    for (i = 0; i < 6; i++) {
        uni64.i[i] ^= DWC_UTIL_WIFI_ID_KEY_1;
    }

    for (i = 0; i < 5; i++) {
        uni64.i[i] = (DWCi_util_wifiid_ttable[(uni64.i[i] >> 4) & 0xF] << 4) | (DWCi_util_wifiid_ttable[uni64.i[i] & 0xF]);
    }

    MI_CpuCopy8(uni64.i, uidtemp, sizeof(uidtemp));

    for (i = 0; i < 5; i++) {
        uni64.i[DWCi_util_wifiid_exctable[i]] = uidtemp[i];
    }

    uni64.i[7] = 0;
    uni64.i[6] = 0;
    uni64.i[5] &= 7;

    uni64.data = uni64.data << 1;
    uni64.i[0] |= ((uni64.i[5] >> 3) & 1);

    for (i = 0; i < 6; i++) {
        uni64.i[i] ^= DWC_UTIL_WIFI_ID_KEY_2;
    }

    uni64.i[7] = 0;
    uni64.i[6] = 0;
    uni64.i[5] &= 7;

    return uni64.data;
}

BOOL DWCi_AUTH_GetNewWiFiInfo(DWCWiFiInfo *wifiinfo)
{
    u8 macaddr[6];
    RTCDate date;
    RTCTime time;
    MATHRandContext16 rand;

    DWCi_BM_GetWiFiInfo(wifiinfo);

    RTC_Init();
    if (RTC_GetDate(&date) != RTC_RESULT_SUCCESS) {
        return FALSE;
    }
    if (RTC_GetTime(&time) != RTC_RESULT_SUCCESS) {
        return FALSE;
    }

    s64 seed = RTC_ConvertDateTimeToSecond(&date, &time);

    if (seed < 0) {
        return FALSE;
    }

    if (OS_IsTickAvailable()) {
        seed += OS_GetTick; // bug
    }

    MATH_InitRand16(&rand, seed);
    OS_GetMacAddress(macaddr);

    u32 macUP3 = (macaddr[0] << 16) | (macaddr[1] << 8) | macaddr[2];
    u8 vend = macUP3 != 0x9BF;
    u32 macDW3 = (macaddr[3] << 16) | (macaddr[4] << 8) | macaddr[5];

    wifiinfo->pass = MATH_Rand16(&rand, 1000);
    wifiinfo->attestedUserId = 0;

    if (wifiinfo->randomHistory == 0) {
        wifiinfo->notAttestedId = 0;
        while (wifiinfo->notAttestedId == 0) {
            MATH_Rand16(&rand, 0);
            while (rand.x == 0) {
                MATH_Rand16(&rand, 0);
            }

            wifiinfo->randomHistory = rand.x;
            wifiinfo->notAttestedId = DWCi_Util_WiFiId_scrambleUid(wifiinfo->randomHistory, macDW3, vend, 0);
        }
    } else {
        wifiinfo->notAttestedId = 0;
        while (wifiinfo->notAttestedId == 0) {
            wifiinfo->randomHistory++;
            wifiinfo->notAttestedId = DWCi_Util_WiFiId_scrambleUid(wifiinfo->randomHistory, macDW3, vend, 0);
        }
    }

    return TRUE;
}

BOOL DWCi_AUTH_MakeWiFiID(void *work)
{
    DWCWiFiInfo wifiinfo;
    if (DWCi_AUTH_GetNewWiFiInfo(&wifiinfo) == FALSE) {
        return FALSE;
    }

    return DWCi_BM_SetWiFiInfo(&wifiinfo, work) != FALSE;
}

BOOL DWCi_AUTH_UpDateWiFiID(DWCWiFiInfo *wifiinfo, void *work)
{
    DWCWiFiInfo wifiinfotemp;
    DWCi_BM_GetWiFiInfo(&wifiinfotemp);

    wifiinfo->attestedUserId = wifiinfo->notAttestedId;
    wifiinfo->notAttestedId = wifiinfotemp.notAttestedId;

    return DWCi_BM_SetWiFiInfo(wifiinfo, work) != FALSE;
}

BOOL DWCi_AUTH_RemakeWiFiID(DWCWiFiInfo *wifiinfo)
{
    u8 macaddr[6] = { 0 };
    RTCDate date;
    RTCTime time;
    MATHRandContext16 rand;

    DWCi_BM_GetWiFiInfo(wifiinfo);

    RTC_Init();
    if (RTC_GetDate(&date) != RTC_RESULT_SUCCESS) {
        return FALSE;
    }
    if (RTC_GetTime(&time) != RTC_RESULT_SUCCESS) {
        return FALSE;
    }

    s64 seed = RTC_ConvertDateTimeToSecond(&date, &time);

    if (seed < 0) {
        return FALSE;
    }

    if (OS_IsTickAvailable()) {
        seed += OS_GetTick; // bug
    }

    MATH_InitRand16(&rand, seed);
    OS_GetMacAddress(macaddr);

    u32 macUP3 = (macaddr[0] << 16) | (macaddr[1] << 8) | macaddr[2];
    u8 vend = macUP3 != 0x9BF;
    u32 macDW3 = (macaddr[3] << 16) | (macaddr[4] << 8) | macaddr[5];

    wifiinfo->pass = MATH_Rand16(&rand, 1000);

    wifiinfo->notAttestedId = 0;
    while (wifiinfo->notAttestedId == 0) {
        MATH_Rand16(&rand, 0);
        while (rand.x == 0 || wifiinfo->randomHistory == (u16)rand.x) {
            MATH_Rand16(&rand, 0);
        }

        wifiinfo->randomHistory = rand.x;
        wifiinfo->notAttestedId = DWCi_Util_WiFiId_scrambleUid(wifiinfo->randomHistory, macDW3, vend, 0);
    }

    return TRUE;
}

void DWC_Auth_GetId(DWCAuthWiFiId *id)
{
    DWCWiFiInfo wifiinfo;
    DWCi_BM_GetWiFiInfo(&wifiinfo);

    id->uId = wifiinfo.attestedUserId;
    id->notAttestedId = wifiinfo.notAttestedId;
    id->flag = wifiinfo.attestedUserId != 0;
}

BOOL DWC_Auth_CheckPseudoWiFiID(void)
{
    DWCWiFiInfo wifiinfo;
    DWCi_BM_GetWiFiInfo(&wifiinfo);

    return wifiinfo.notAttestedId != 0;
}

BOOL DWC_Auth_CheckWiFiIDNeedCreate(void)
{
    DWCWiFiInfo wifiinfo;
    DWCi_BM_GetWiFiInfo(&wifiinfo);

    return wifiinfo.notAttestedId == 0 && wifiinfo.attestedUserId == 0;
}
