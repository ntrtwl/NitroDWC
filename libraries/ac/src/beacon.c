#include "beacon.h"

#include <nitro.h>

#include "ac.h"
#include "search.h"
#include "shop_usb.h"

static int CompareList(WMBssDesc *bssdesc, u8 num, ACSearchList *list);
static int CompareListDiff(WMBssDesc *bssdesc, ACWORK *wk);
static int AddList(int type, WMBssDesc *bssdesc, u8 channel, ACWORK *wk);
static void SetDataListTail(u8 type, WMBssDesc *bssdesc, u8 channel, ACWORK *wk);
static void UpdateList(int no, WMBssDesc *bssdesc, u8 channel, ACWORK *wk);
static void SortList(int no, ACWORK *wk);

void DWCi_AC_GetBeacon(WMBssDesc *bssdesc)
{
    int ap_type = -1;
    u8 channel;

    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    hd->findAP = TRUE;
    switch (DWCi_AC_GetPhase()) {
    case AC_PHASE_SEARCH_AROUND:
        channel = wk->searchChannel;
        if (bssdesc->ssidLength == 0 || bssdesc->ssid[0] == '\0') {
            DWCi_AC_SetStealthChannel(bssdesc->channel);
        } else if (bssdesc->ssidLength == 1 && bssdesc->ssid[0] == ' ') {
            DWCi_AC_SetStealthChannel(bssdesc->channel);
            ap_type = CompareList(bssdesc, wk->searchListNum, wk->searchList);
        } else {
            ap_type = CompareList(bssdesc, wk->searchListNum, wk->searchList);
        }
        break;
    case AC_PHASE_SEARCH_DIFFER_CHANNEL:
        channel = wk->bssDesc[wk->searchListNo].channel - 1;
        ap_type = CompareListDiff(bssdesc, wk);
        if (ap_type >= 0) {
            wk->findList[wk->searchListNo].find = TRUE;
        }
        break;
    case AC_PHASE_SEARCH_STEALTH:
        channel = wk->searchChannel;
        ap_type = CompareList(bssdesc, 1, &wk->searchList[wk->searchListNo]);
        if (ap_type >= 0) {
            wk->searchList[wk->searchListNo].find = TRUE;
        }
        break;
    default:
        return;
    }

    if (ap_type >= 0) {
        int no = AddList(ap_type, bssdesc, channel, wk);
        SortList(no, wk);
    }
}

int DWCi_AC_CheckNintendoSSID(WMBssDesc *bssdesc)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_USB + 1) {
        u8 capa = (bssdesc->capaInfo >> 4) & 1;
        if (capa == TRUE && DWCi_AC_CheckNintendoUsbAP(bssdesc->ssid) == TRUE) {
            return AP_TYPE_USB;
        }
    }

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_SHOP + 1) {
        u8 capa = (bssdesc->capaInfo >> 4) & 1;
        if (capa == TRUE && DWCi_AC_CheckNintendoShopAP(bssdesc->ssid) == TRUE) {
            return AP_TYPE_SHOP;
        }
    }

    return -1;
}

static int CompareList(WMBssDesc *bssdesc, u8 num, ACSearchList *list)
{
    if (bssdesc->ssidLength == WM_SIZE_SSID) {
        int type = DWCi_AC_CheckNintendoSSID(bssdesc);
        if (type > 0) {
            return type;
        }
    }

    for (int i = 0; i < num; i++) {
        u8 length = bssdesc->ssidLength;
        if (length == list->length) {
            if (strncmp(bssdesc->ssid, list->ssid, bssdesc->ssidLength) == 0) {
                return list->type;
            }
        }
        list++;
    }

    return -1;
}

static int CompareListDiff(WMBssDesc *bssdesc, ACWORK *wk)
{
    if (bssdesc->ssidLength == WM_SIZE_SSID) {
        int type = DWCi_AC_CheckNintendoSSID(bssdesc);
        if (type > 0) {
            return type;
        }
    }

    for (int i = 0; i < wk->findListNum; i++) {
        if (bssdesc->ssidLength == wk->bssDesc[i].ssidLength) {
            if (strncmp(bssdesc->ssid, wk->bssDesc[i].ssid, bssdesc->ssidLength) == 0) {
                return wk->findList[i].type;
            }
        }
    }

    return -1;
}

static int AddList(int type, WMBssDesc *bssdesc, u8 channel, ACWORK *wk)
{
    int find = -1;

    for (int i = 0; i < wk->findListNum; i++) {
        if (WM_IsBssidEqual(bssdesc->bssid, (u8 *)&wk->bssDesc[i].bssid)) {
            find = i;
            break;
        }
    }

    if (find == -1) {
        SetDataListTail(type, bssdesc, channel, wk);

        if (wk->findListNum < AC_FIND_LIST_MAX) {
            wk->findListNum++;
        }

        find = AC_FIND_LIST_MAX;
    } else {
        UpdateList(find, bssdesc, channel, wk);
    }

    return find;
}

static inline u8 iExtractRssi(u16 rssi)
{
    if (rssi & 2) {
        rssi = (u8)(rssi >> 2);
    } else {
        rssi = (u8)((rssi >> 2) + 25);
    }
    return rssi;
}

static void SetDataListTail(u8 type, WMBssDesc *bssdesc, u8 channel, ACWORK *wk)
{
    ACFindList *list_end = &wk->findList[AC_FIND_LIST_MAX];
    WMBssDesc *bssdesc_end = &wk->bssDesc[AC_FIND_LIST_MAX];

    list_end->type = type;
    list_end->rssi = iExtractRssi(bssdesc->rssi);
    list_end->channel = channel;

    MI_CpuCopy32(bssdesc, bssdesc_end, sizeof(WMBssDesc));
}

static void UpdateList(int no, WMBssDesc *bssdesc, u8 channel, ACWORK *wk)
{
    ACFindList *list = &wk->findList[no];
    WMBssDesc *bssdesc_no = &wk->bssDesc[no];
    u8 rssi = iExtractRssi(bssdesc->rssi);

    if (rssi > list->rssi) {
        list->rssi = rssi;
        list->channel = channel;
    }

    MI_CpuCopy32(bssdesc, bssdesc_no, sizeof(WMBssDesc));
}

static void SortList(int no, ACWORK *wk)
{
    int i;
    ACFindList *list = wk->findList;
    WMBssDesc *bssdesc = wk->bssDesc;

    for (i = no - 1; i >= 0; i--) {
        if (list[no].rssi >= list[i].rssi) {
            ACFindList list_buf;
            MI_CpuCopy32(&list[i], &list_buf, sizeof(ACFindList));
            MI_CpuCopy32(&list[no], &list[i], sizeof(ACFindList));
            MI_CpuCopy32(&list_buf, &list[no], sizeof(ACFindList));

            WMBssDesc bssdesc_buf;
            MI_CpuCopy32(&bssdesc[i], &bssdesc_buf, sizeof(WMBssDesc));
            MI_CpuCopy32(&bssdesc[no], &bssdesc[i], sizeof(WMBssDesc));
            MI_CpuCopy32(&bssdesc_buf, &bssdesc[no], sizeof(WMBssDesc));
            no = i;
        } else {
            break;
        }
    }

    MI_CpuClear32(&list[AC_FIND_LIST_MAX], sizeof(ACFindList));
    MI_CpuClear32(&bssdesc[AC_FIND_LIST_MAX], sizeof(WMBssDesc));
}
