#include "WDS.h"

#include <crypto.h>
#include <nitro.h>

#include "ac/dwc_ac.h"

typedef struct WDSWork {
    u8 wmwork[WM_SYSTEM_BUF_SIZE];
    u8 scanbuf[WM_SIZE_SCAN_EX_BUF] ATTRIBUTE_ALIGN(32);
    WMScanExParam scanparam;
    WMCallbackFunc scancb;
    WDSApInfo apinfo[WDS_AP_INFO_COUNT];
    u16 rssi[WDS_AP_INFO_COUNT];
    u16 tgid[WDS_AP_INFO_COUNT];
    s32 apnum;
    s32 apindex;
    u32 status;
    MATHCRC16Context crcContext;
    MATHCRC16Table crcTable;
} WDSWork;

#define WDS_AP_INFO_GGID 0x857

#define WDS_HOTSPOT_ENCODING_MASK  0xF000
#define WDS_HOTSPOT_ENCODING_UTF8  0x0000
#define WDS_HOTSPOT_ENCODING_UTF16 0x1000

enum WDSStatus {
    WDS_STATUS_INIT = 0,
    WDS_STATUS_SCAN_START,
    WDS_STATUS_SCAN_END,
    WDS_STATUS_END,
};

static WDSWork *gWdsWork = NULL;

static u8 WDSGetRssi8(u8 rssi)
{
    return rssi & 2 ? rssi >> 2 : (rssi >> 2) + 25;
}

static u16 bu_UTF8_To_UCS2(u8 *c, s32 *bytes_used)
{
    u16 ret;

    if ((*c & 0x80) == 0) {
        ret = *c;
        *bytes_used = 1;
    } else if ((*c & 0xE0) == 0xC0) {
        u8 byte1 = (c[0] & 0x1F);
        u8 byte2 = c[1] & 0x3F;
        ret = (byte1 << 6) | byte2;
        *bytes_used = 2;
    } else if ((*c & 0xF0) == 0xE0) {
        u8 byte1 = c[0] & 0xF;
        u8 byte2 = c[1] & 0x3F;
        u8 byte3 = c[2] & 0x3F;
        ret = (byte1 << 12) | (byte2 << 6) | byte3;
        *bytes_used = 3;
    } else {
        ret = '?';
        *bytes_used = 1;
    }

    return ret;
}

static void WDSScanCallback(void *arg)
{
    WMStartScanExCallback *pParam = arg;
    u32 i, j;

    CRYPTORC4FastContext rc4context;
    u8 rc4key[8];

    DC_InvalidateRange(&gWdsWork->scanbuf, WM_SIZE_SCAN_EX_BUF);
    if (pParam->errcode == WM_ERRCODE_SUCCESS && pParam->state == WM_STATECODE_PARENT_FOUND) {
        for (i = 0; i < pParam->bssDescCount; i++) {
            if (WM_IsValidGameInfo(&pParam->bssDesc[i]->gameInfo, sizeof(WMGameInfo)) && pParam->bssDesc[i]->gameInfo.ggid == WDS_AP_INFO_GGID) {
                BOOL duplicated = FALSE;
                for (j = 0; j < gWdsWork->apnum; j++) {
                    if (gWdsWork->tgid[j] == pParam->bssDesc[i]->gameInfo.tgid) {
                        duplicated = TRUE;
                        break;
                    }
                }

                if (duplicated == TRUE) {
                    continue;
                }

                MI_CpuCopy8(pParam->bssDesc[i]->gameInfo.userGameInfo, &gWdsWork->apinfo[gWdsWork->apindex], sizeof(WDSApInfo));

                const u32 magic = 'WDS!';
                MI_CpuCopy8(&magic, &rc4key[0], 4);
                MI_CpuCopy8(&pParam->bssDesc[i]->bssid[2], &rc4key[4], 4);
                CRYPTO_RC4FastInit(&rc4context, &rc4key, 8);
                CRYPTO_RC4FastEncrypt(&rc4context, &gWdsWork->apinfo[gWdsWork->apindex], sizeof(WDSApInfo), &gWdsWork->apinfo[gWdsWork->apindex]);

                u8 *pCrcData = (u8 *)&gWdsWork->apinfo[gWdsWork->apindex];
                u32 crcLength = sizeof(WDSApInfo) - sizeof(gWdsWork->apinfo[gWdsWork->apindex].crc);
                MATH_CRC16Update(&gWdsWork->crcTable, &gWdsWork->crcContext, pCrcData, crcLength);
                u16 crc = MATH_CalcCRC16(&gWdsWork->crcTable, pCrcData, crcLength);
                if (crc != gWdsWork->apinfo[gWdsWork->apindex].crc && gWdsWork->apinfo[gWdsWork->apindex].crc != 0) {
                    MI_CpuClear8(&gWdsWork->apinfo[gWdsWork->apindex], sizeof(WDSApInfo));
                    continue;
                }

                gWdsWork->rssi[gWdsWork->apindex] = WDSGetRssi8(pParam->bssDesc[i]->rssi);
                gWdsWork->tgid[gWdsWork->apindex] = pParam->bssDesc[i]->gameInfo.tgid;
                gWdsWork->apindex = (gWdsWork->apindex + 1) % WDS_AP_INFO_COUNT;
                gWdsWork->apnum++;
                if (gWdsWork->apnum > WDS_AP_INFO_COUNT) {
                    gWdsWork->apnum = WDS_AP_INFO_COUNT;
                }
            }
        }
    }

    gWdsWork->status = WDS_STATUS_SCAN_END;

    if (gWdsWork->scancb != NULL) {
        gWdsWork->scancb(pParam);
    }
}

u32 WDS_GetWorkAreaSize(void)
{
    return sizeof(WDSWork);
}

int WDS_Initialize(void *wdsWork, WMCallbackFunc callback, u16 dmaNo)
{
    if (callback == NULL) {
        return -1;
    }

    gWdsWork = wdsWork;
    MI_CpuClear8(gWdsWork, WDS_GetWorkAreaSize());
    gWdsWork->status = WDS_STATUS_INIT;

    MATH_CRC16Init(&gWdsWork->crcContext);
    MATH_CRC16InitTable(&gWdsWork->crcTable);

    WMErrCode errcode = WM_Initialize(gWdsWork->wmwork, callback, dmaNo);
    if (errcode != WM_ERRCODE_OPERATING) {
        return errcode;
    }
    return WM_ERRCODE_SUCCESS;
}

int WDS_InitializeEx(void *wdsWork, WMCallbackFunc callback, u16 dmaNo, WDSBriefApInfo *apinfo)
{
    if (callback == NULL) {
        return -1;
    }

    gWdsWork = wdsWork;
    MI_CpuClear8(gWdsWork, WDS_GetWorkAreaSize());
    gWdsWork->status = WDS_STATUS_INIT;

    for (int i = 0; i < WDS_AP_INFO_COUNT; i++) {
        if (apinfo[i].isvalid == TRUE) {
            gWdsWork->apnum++;
            gWdsWork->apindex = (gWdsWork->apindex + 1) % WDS_AP_INFO_COUNT;
            gWdsWork->apinfo[i] = apinfo[i].apinfo;
            gWdsWork->rssi[i] = apinfo[i].rssi;
        }
    }

    WMErrCode errcode = WM_Initialize(gWdsWork->wmwork, callback, dmaNo);
    if (errcode != WM_ERRCODE_OPERATING) {
        return errcode;
    }

    gWdsWork->status = WDS_STATUS_SCAN_END;
    return WM_ERRCODE_SUCCESS;
}

int WDS_End(WMCallbackFunc callback)
{
    if (callback == NULL) {
        return -1;
    }

    gWdsWork->status = WDS_STATUS_END;
    gWdsWork = NULL;

    WMErrCode errcode = WM_End(callback);
    if (errcode != WM_ERRCODE_OPERATING) {
        return errcode;
    }

    return WM_ERRCODE_SUCCESS;
}

int WDS_StartScan(WMCallbackFunc callback)
{
    if (callback == NULL) {
        return -1;
    }

    gWdsWork->scanparam.scanBuf = (WMBssDesc *)gWdsWork->scanbuf;
    gWdsWork->scanparam.scanBufSize = WM_SIZE_SCAN_EX_BUF;
    gWdsWork->scanparam.channelList = WM_GetAllowedChannel();
    gWdsWork->scanparam.maxChannelTime = WM_GetDispersionScanPeriod();
    gWdsWork->scanparam.scanType = WM_SCANTYPE_PASSIVE;
    MI_CpuFill8(gWdsWork->scanparam.bssid, 0xFF, WM_SIZE_BSSID);

    gWdsWork->scancb = callback;
    gWdsWork->status = WDS_STATUS_SCAN_START;

    WMErrCode errcode = WM_StartScanEx(WDSScanCallback, &gWdsWork->scanparam);
    if (errcode != WM_ERRCODE_OPERATING) {
        return errcode;
    }

    return WM_ERRCODE_SUCCESS;
}

int WDS_EndScan(WMCallbackFunc callback)
{
    if (callback == NULL) {
        return -1;
    }

    gWdsWork->status = WDS_STATUS_SCAN_END;

    WMErrCode errcode = WM_EndScan(callback);
    if (errcode != WM_ERRCODE_OPERATING) {
        return errcode;
    }

    return WM_ERRCODE_SUCCESS;
}

int WDS_GetApInfoNum(void)
{
    if (gWdsWork->status != WDS_STATUS_SCAN_END) {
        return -1;
    }

    return gWdsWork->apnum;
}

int WDS_GetApInfoByIndex(int index, WDSBriefApInfo *briefapinfo)
{
    if (gWdsWork->status != WDS_STATUS_SCAN_END) {
        return -1;
    }
    if (index < 0 || index >= gWdsWork->apnum) {
        return -1;
    }

    MI_CpuClear8(briefapinfo, sizeof(WDSBriefApInfo));
    briefapinfo->isvalid = TRUE;
    briefapinfo->rssi = gWdsWork->rssi[index];
    MI_CpuCopy8(&gWdsWork->apinfo[index], &briefapinfo->apinfo, sizeof(WDSApInfo));
    return 0;
}

int WDS_GetApInfoAll(WDSBriefApInfo *briefapinfo)
{
    if (gWdsWork->status != WDS_STATUS_SCAN_END) {
        return -1;
    }

    MI_CpuClear8(briefapinfo, sizeof(WDSBriefApInfo) * WDS_AP_INFO_COUNT);
    for (int index = 0; index < WDS_AP_INFO_COUNT; index++) {
        briefapinfo[index].isvalid = FALSE;
    }

    for (int index = 0; index < gWdsWork->apnum; index++) {
        if (WDS_GetApInfoByIndex(index, &briefapinfo[index]) == -1) {
            break;
        }
    }

    return 0;
}

int WDS_SetConnectTargetByIndex(int index)
{
    if (gWdsWork->status != WDS_STATUS_SCAN_END) {
        return -1;
    }
    if (index < 0 || index >= gWdsWork->apnum) {
        return -1;
    }

    DWC_AC_SetSpecifyAp(gWdsWork->apinfo[index].ssid, gWdsWork->apinfo[index].wepkey, gWdsWork->apinfo[index].encryptflag);

    return 0;
}

int WDS_SetConnectTargetByBriefApInfo(WDSBriefApInfo *briefapinfo)
{
    if (gWdsWork->status != WDS_STATUS_SCAN_END) {
        return -1;
    }

    DWC_AC_SetSpecifyAp(briefapinfo->apinfo.ssid, briefapinfo->apinfo.wepkey, briefapinfo->apinfo.encryptflag);

    return 0;
}

int WDS_GetApDescriptionUTF16(WDSBriefApInfo *briefapinfo, void *outbuf)
{
    MI_CpuClear8(outbuf, WDS_MAX_HOTSPOT_NAME_LENGTH + sizeof(u16));

    if ((briefapinfo->apinfo.hotspotid & WDS_HOTSPOT_ENCODING_MASK) == WDS_HOTSPOT_ENCODING_UTF8) {
        u8 *pStr = briefapinfo->apinfo.hotspotname;
        u8 *pEndStr = &briefapinfo->apinfo.hotspotname[WDS_MAX_HOTSPOT_NAME_LENGTH];
        u16 *pUCS2 = outbuf;

        while (pStr != pEndStr && *pStr != '\0') {
            s32 bytes_used;
            *pUCS2++ = bu_UTF8_To_UCS2(pStr, &bytes_used);
            pStr += bytes_used;
        }
    } else {
        MI_CpuCopy8(briefapinfo->apinfo.hotspotname, outbuf, WDS_MAX_HOTSPOT_NAME_LENGTH);
    }

    return 0;
}
