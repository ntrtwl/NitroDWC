#include "dwc_cal.h"

#include <ac/dwc_ac.h>
#include <bm/dwc_backup.h>
#include <nitro.h>

#include "auth/dwc_http.h"

#include "dwc_version.h"

BOOL DWC_Auth_GetCalInfoFromNVRAM(DWCCalInfo *cal)
{
    DWCWiFiInfo wifiinfo;
    DWCi_BM_GetWiFiInfo(&wifiinfo);
    return DWC_Auth_GetCalInfoFromWiFiInfo(cal, &wifiinfo);
}

#define HW_ROM_HEADER_GAME_CODE  (HW_ROM_HEADER_BUF + 0xC)
#define HW_ROM_HEADER_MAKER_CODE (HW_ROM_HEADER_BUF + 0x10)

BOOL DWC_Auth_GetCalInfoFromWiFiInfo(DWCCalInfo *cal, DWCWiFiInfo *wifiinfo)
{
    s32 i;
    u8 macaddr[WM_SIZE_MACADDR];
    OSOwnerInfo ownerinfo;
    RTCDate rtcdate;
    RTCTime rtctime;

    MI_CpuClear8(cal, sizeof(DWCCalInfo));

    if (wifiinfo->attestedUserId != 0) {
        OS_SNPrintf(cal->userid, 14, "%013llu", wifiinfo->attestedUserId);
    } else {
        OS_SNPrintf(cal->userid, 14, "%013llu", wifiinfo->notAttestedId);
    }

    OS_SNPrintf(cal->passwd, 7, "%03u", wifiinfo->pass);

    if (*(u8 *)HW_ROM_HEADER_GAME_CODE == 0) {
        OS_Panic("The GameCode in this ROM header is invalid. Check your ROM header template.\n");
    }
    MI_CpuCopy8((u8 *)HW_ROM_HEADER_GAME_CODE, cal->gamecd, 4);

    if (*(u8 *)HW_ROM_HEADER_MAKER_CODE == 0) {
        OS_Panic("The MakerCode in this ROM header is invalid. Check your .rsf file.\n");
    }
    MI_CpuCopy8((u8 *)HW_ROM_HEADER_MAKER_CODE, cal->makercd, 2);

    cal->unitcd[0] = '0';

    OS_GetMacAddress(macaddr);
    for (i = 0; i < WM_SIZE_MACADDR; i++) {
        OS_SPrintf(&cal->macaddr[i * 2], "%02x", macaddr[i]);
    }

    OS_GetOwnerInfo(&ownerinfo);

    if (ownerinfo.language >= OS_LANGUAGE_CODE_MAX) {
        ownerinfo.language = OS_LANGUAGE_ENGLISH;
    }
    OS_SNPrintf(cal->lang, 3, "%02x", ownerinfo.language);

    MI_CpuCopy8(ownerinfo.nickName, cal->devname, OS_OWNERINFO_NICKNAME_MAX * sizeof(u16));

    OS_SNPrintf(cal->birth, 5, "%02x%02x", ownerinfo.birthday.month, ownerinfo.birthday.day);

    if (RTC_GetDateTime(&rtcdate, &rtctime) != RTC_RESULT_SUCCESS) {
        return FALSE;
    }
    OS_SNPrintf(cal->devtime, 13, "%02d%02d%02d%02d%02d%02d", rtcdate.year, rtcdate.month, rtcdate.day, rtctime.hour, rtctime.minute, rtctime.second);

    OSIntrMode preirq = OS_DisableInterrupts();
    u8 *wcmbssid = WCM_GetApMacAddress();
    DC_InvalidateRange(wcmbssid, WM_SIZE_BSSID);
    if (wcmbssid == NULL) {
        OS_RestoreInterrupts(preirq);
        return FALSE;
    }
    for (i = 0; i < WM_SIZE_BSSID; i++) {
        OS_SPrintf(&cal->bssid[i * 2], "%02x", wcmbssid[i]);
    }

    u8 aptype = DWC_AC_GetApType();
    if (aptype == DWC_AC_AP_TYPE_FALSE || aptype > DWC_AC_AP_TYPE_UNKNOWN) {
        aptype = DWC_AC_AP_TYPE_UNKNOWN;
    }
    OS_SNPrintf(cal->apinfo, 14, "%02d:0000000-00", aptype);

    u16 wcmessidlen;
    u8 *wcmessid = WCM_GetApEssid(&wcmessidlen);
    DC_InvalidateRange(wcmessid, WM_SIZE_SSID);
    if (wcmessid == NULL) {
        OS_RestoreInterrupts(preirq);
        return FALSE;
    }
    MI_CpuCopy8(wcmessid, cal->ssid, WM_SIZE_SSID);

    DWC_AC_GetApSpotInfo(&cal->apinfo[3]);
    OS_RestoreInterrupts(preirq);
    return TRUE;
}

BOOL DWC_Auth_SetCalInfoToHttp(DWCHttp *http, DWCCalInfo *calinfo, BOOL allowessid)
{
    char buf[33];
    OS_SNPrintf(buf, sizeof(buf), "%03d%03d", DWC_VERSION_MAJOR, DWC_VERSION_MINOR);

    if (DWC_Http_Add_PostBase64Item(http, "sdkver", buf, strlen(buf)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "userid", calinfo->userid, strlen(calinfo->userid)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "passwd", calinfo->passwd, strlen(calinfo->passwd)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "bssid", calinfo->bssid, strlen(calinfo->bssid)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "apinfo", calinfo->apinfo, strlen(calinfo->apinfo)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "gamecd", calinfo->gamecd, strlen(calinfo->gamecd)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "makercd", calinfo->makercd, strlen(calinfo->makercd)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "unitcd", calinfo->unitcd, strlen(calinfo->unitcd)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "macadr", calinfo->macaddr, strlen(calinfo->macaddr)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "lang", calinfo->lang, strlen(calinfo->lang)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "birth", calinfo->birth, strlen(calinfo->birth)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "devtime", calinfo->devtime, strlen(calinfo->devtime)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_PostBase64Item(http, "devname", calinfo->devname, wcslen(calinfo->devname) * sizeof(u16)) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (allowessid == TRUE) {
        if (DWC_Http_Add_PostBase64Item(http, "ssid", calinfo->ssid, strlen(calinfo->ssid)) != DWCHTTP_E_NOERR) {
            return FALSE;
        }
    }

    OS_SNPrintf(buf, sizeof(buf), "Nitro WiFi SDK/%d.%d", DWC_VERSION_MAJOR, DWC_VERSION_MINOR);
    if (DWC_Http_Add_HeaderItem(http, "User-Agent", buf) != DWCHTTP_E_NOERR) {
        return FALSE;
    }
    if (DWC_Http_Add_HeaderItem(http, "HTTP_X_GAMECD", calinfo->gamecd) != DWCHTTP_E_NOERR) {
        return FALSE;
    }

    return TRUE;
}
