#include "makelist.h"

#include <nitro.h>

#include "ac.h"
#include "shop_usb.h"

#define FREESPOT_SSID_LENGTH    8
#define WAYPORT_SSID_LENGTH     8
#define NINTENDOWFC_SSID_LENGTH 11

static const u8 FREESPOT_SSID[FREESPOT_SSID_LENGTH] = "FREESPOT";
static const u8 WAYPORT_SSID[WAYPORT_SSID_LENGTH] = "Wayport2";
static const u8 NINTENDOWFC_SSID[NINTENDOWFC_SSID_LENGTH] = "NINTENDOWFC";

static u8 MakeAroundList(ACWORK *wk);
static u8 MakeStealthList(ACWORK *wk);
static u8 MakeUserList(ACWORK *wk);
static u8 MakeDifferChannelList(ACWORK *wk);
static u8 CheckDifferChannelStart(ACWORK *wk);

u8 DWCi_AC_MakeSearchList(int type)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    switch (type) {
    case SEARCH_LIST_TYPE_AROUND:
        MI_CpuClear32(wk->searchList, sizeof(ACSearchList) * AC_SEARCH_LIST_COUNT);
        wk->searchListNum = MakeAroundList(wk);
        break;
    case SEARCH_LIST_TYPE_DIFFER_CHANNEL:
        wk->searchListNum = MakeDifferChannelList(wk);
        wk->searchListNo = CheckDifferChannelStart(wk);
        break;
    case SEARCH_LIST_TYPE_STEALTH:
        MI_CpuClear32(wk->searchList, sizeof(ACSearchList) * AC_SEARCH_LIST_COUNT);
        wk->searchListNo = 0;
        wk->searchListNum = MakeStealthList(wk);
        break;
    }

    return wk->searchListNum;
}

u8 DWCi_AC_CheckFreespot(WMBssDesc *bssdesc)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    if ((wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_FREESPOT + 1)
        && strncmp(bssdesc->ssid, FREESPOT_SSID, FREESPOT_SSID_LENGTH) == 0) {
        return AP_TYPE_FREESPOT;
    } else {
        return 0;
    }
}

static u8 MakeAroundList(ACWORK *wk)
{
    ACSearchList *list = wk->searchList;
    u8 num = MakeUserList(wk);
    list += num;

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_FREESPOT + 1) {
        MI_CpuCopy8(FREESPOT_SSID, list->ssid, FREESPOT_SSID_LENGTH);
        list->length = FREESPOT_SSID_LENGTH;
        list->type = AP_TYPE_FREESPOT;
        num++;
    }

    return num;
}

static u8 MakeStealthList(ACWORK *wk)
{
    u8 num;
    ACSearchList *list = wk->searchList;
    num = MakeUserList(wk);
    list += num;

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_USB + 1) {
        MI_CpuCopy8(NINTENDO_USB_SSID, list->ssid, NINTENDO_USB_SSID_LENGTH);
        list->length = NINTENDO_USB_SSID_LENGTH;
        list->type = AP_TYPE_USB;
        num++;
        list++;
    }

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_WAYPORT + 1) {
        MI_CpuCopy8(WAYPORT_SSID, list->ssid, WAYPORT_SSID_LENGTH);
        list->length = WAYPORT_SSID_LENGTH;
        list->type = AP_TYPE_WAYPORT;
        num++;
        list++;
    }

    if (wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_NINTENDOWFC + 1) {
        MI_CpuCopy8(NINTENDOWFC_SSID, list->ssid, NINTENDOWFC_SSID_LENGTH);
        list->length = NINTENDOWFC_SSID_LENGTH;
        list->type = AP_TYPE_NINTENDOWFC;
        num++;
    }

    return num;
}

static inline BOOL iCopySSID(int no, DWCMemPage *info, ACSearchList *list, int base_type)
{
    u8 i;
    for (i = 0; i < WM_SIZE_SSID; i++) {
        if (info->ap.ssid[no][i] == 0) {
            break;
        }
        list->ssid[i] = info->ap.ssid[no][i];
    }

    if (i != 0) {
        list->length = i;
        list->type = base_type;
        return TRUE;
    } else {
        return FALSE;
    }
}

static u8 MakeUserList(ACWORK *wk)
{
    int i;
    u8 num = 0;
    DWCMemPage *info = wk->userInfo;
    ACSearchList *list = wk->searchList;

    for (i = 0; i < 3; i++) {
        if ((wk->connectType == 0 || wk->connectType == DWC_AC_AP_TYPE_USER1 + i + 1) && info->ap.setType != DWC_SETTYPE_NONE) {
            if (iCopySSID(SSID_NORMAL, info, list, AP_TYPE_USER1 + i) != FALSE) {
                num++;
                list++;
            }

            if (info->ap.setType == DWC_SETTYPE_AOSS && iCopySSID(SSID_AOSS, info, list, AP_TYPE_AOSS_USER1 + i) != FALSE) {
                num++;
                list++;
            }
        }
        info++;
    }

    return num;
}

static u8 MakeDifferChannelList(ACWORK *wk)
{
    int i;
    u8 num = 0;

    for (i = 0; i < wk->findListNum; i++) {
        if (wk->findList[i].state == AP_CAN_CONNECT && wk->bssDesc[i].channel - 1 != wk->findList[i].channel) {
            wk->findList[i].find = FALSE;
            num++;
        } else {
            wk->findList[i].find = TRUE;
        }
    }

    return num;
}

static u8 CheckDifferChannelStart(ACWORK *wk)
{
    u8 i;
    u8 no = 0;

    for (i = 0; i < wk->findListNum; i++) {
        if (!wk->findList[i].find) {
            no = i;
            break;
        }
    }

    return no;
}
