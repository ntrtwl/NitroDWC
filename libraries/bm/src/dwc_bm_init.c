#include "bm/dwc_bm_init.h"

#include <nitro.h>

#include "bm/dwc_backup.h"
#include "bm/util_wifiidtool.h"

static BOOL checkAp(const DWCBMApInfo *info);
static int init(DWCMemMap *mm);
static void initPage(DWCMemMap *mm, int page);

const u8 DWCi_SETTING_NONE[4] = { 0 };

int DWC_BM_Init(void *work)
{
    MEMINIT *wk = work;
    BOOL c[4];

    MI_CpuClear8(wk, DWC_BM_INIT_WORK_SIZE);

    if (DWCi_BACKUPlInit(wk) == 0) {
        return DWC_BM_INIT_READ_ERROR;
    }

    MATH_CRC16InitTable(&wk->table);

    if (DWCi_BACKUPlRead(&wk->mm) == 0) {
        return DWC_BM_INIT_READ_ERROR;
    }

    MI_CpuClear8(c, sizeof(c));

    for (int i = 0; i < 3; i++) {
        u16 hash = MATH_CalcCRC16(&wk->table, &wk->mm.page[i], sizeof(DWCMemPage) - sizeof(wk->mm.page[i].crc));
        if (hash == wk->mm.page[i].crc && checkAp(&wk->mm.page[i].ap)) {
            c[i] = TRUE;
        }
    }

    u16 hash = MATH_CalcCRC16(&wk->table, &wk->mm.page[3], sizeof(DWCMemPage) - sizeof(wk->mm.page[3].crc));
    if (hash == wk->mm.page[3].crc) {
        c[3] = TRUE;
    }

    if (c[0] && c[1] && c[2] && c[3]) {
        DWCi_BACKUPlSetWiFi(wk->mm.page[0].wifi);
        return DWC_BM_INIT_OK;
    }

    if (!c[0] && !c[1] && !c[2] && !c[3]) {
        init(&wk->mm);
        if (DWCi_BACKUPlWriteAll(work)) {
            return DWC_BM_INIT_OK;
        } else {
            return DWC_BM_INIT_WRITE_ERROR;
        }
    }

    if ((!c[0] || !c[1]) && (!c[2] || !c[3])) {
        init(&wk->mm);
        if (DWCi_BACKUPlWriteAll(work)) {
            return DWC_BM_INIT_OK;
        } else {
            return DWC_BM_INIT_WRITE_ERROR;
        }
    }

    if (!c[0] && !c[1]) {
        init(&wk->mm);
        if (DWCi_BACKUPlWriteAll(work)) {
            return DWC_BM_INIT_OK_INIT;
        } else {
            return DWC_BM_INIT_WRITE_ERROR;
        }
    }

    if (!c[0]) {
        initPage(&wk->mm, 0);
        MI_CpuCopy8(&wk->mm.page[1].wifi, &wk->mm.page[0].wifi, sizeof(wk->mm.page[0].wifi));
        wk->mm.page[0].ap.state = wk->mm.page[1].ap.state;
    } else if (!c[1]) {
        initPage(&wk->mm, 1);
        MI_CpuCopy8(&wk->mm.page[0].wifi, &wk->mm.page[1].wifi, sizeof(wk->mm.page[0].wifi));
        wk->mm.page[1].ap.state = wk->mm.page[0].ap.state;
    }

    DWCi_BACKUPlSetWiFi(wk->mm.page[0].wifi);

    if (!c[2]) {
        initPage(&wk->mm, 2);
    }

    if (!c[3]) {
        MI_CpuClear16(&wk->mm.page[3], sizeof(DWCMemPage));
    }

    BOOL clear = FALSE;
    for (int i = 0; i < 3; i++) {
        if (!c[i] && wk->mm.page[0].ap.state & (1 << i)) {
            wk->mm.page[0].ap.state &= ~(1 << i);
            clear = TRUE;
            wk->mm.page[1].ap.state = wk->mm.page[0].ap.state;
        }
    }

    if (!DWCi_BACKUPlWriteAll(work)) {
        return DWC_BM_INIT_WRITE_ERROR;
    }

    if (clear) {
        return DWC_BM_INIT_OK_CLEAR;
    } else {
        return DWC_BM_INIT_OK;
    }
}

static BOOL checkAp(const DWCBMApInfo *info)
{
    u8 snm[4];

    if (info->setType == DWC_SETTYPE_NONE) {
        return TRUE;
    }

    if (info->setType > DWC_SETTYPE_EASY) {
        return FALSE;
    }

    if (!DWC_BACKUPlCheckSsid(info->ssid[SSID_NORMAL])) {
        return FALSE;
    }

    if (memcmp(info->ip, DWCi_SETTING_NONE, sizeof(info->ip)) != 0) {
        if (!DWC_BACKUPlCheckAddress(info->gateway)) {
            return FALSE;
        }

        if (info->netmask > 32) {
            return FALSE;
        }

        DWCi_BACKUPlConvMaskAddr(info->netmask, snm);

        if (!DWC_BACKUPlCheckIp(info->ip, snm)) {
            return FALSE;
        }
    }

    if (memcmp(info->dns[0], DWCi_SETTING_NONE, sizeof(info->dns[0])) != 0
        && !DWC_BACKUPlCheckAddress(info->dns[0]) && !DWC_BACKUPlCheckAddress(info->dns[1])) {
        return FALSE;
    }
    return TRUE;
}

static int init(DWCMemMap *mm)
{
    DWCWiFiInfo info;

    MI_CpuClear16(mm, sizeof(DWCMemMap));

    for (int i = 0; i < 3; i++) {
        mm->page[i].ap.setType = DWC_SETTYPE_NONE;
    }

    DWCi_AUTH_GetNewWiFiInfo(&info);
    u8 *addr = DWCi_BACKUPlConvWifiInfo(&info);

    for (int i = 0; i < 2; i++) {
        MI_CpuCopy8(addr, &mm->page[i].wifi, sizeof(mm->page[i].wifi));
    }

    return 0;
}

static void initPage(DWCMemMap *mm, int page)
{
    MI_CpuClear16(&mm->page[page], sizeof(DWCMemPage));
    mm->page[page].ap.setType = DWC_SETTYPE_NONE;
}
