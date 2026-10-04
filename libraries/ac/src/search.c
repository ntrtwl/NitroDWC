#include <nitro.h>

#include "ac.h"
#include "makelist.h"

static const u32 DWC_AC_SEARCH_CHANNEL[] = {
    WCM_OPTION_CHANNEL_1,
    WCM_OPTION_CHANNEL_2,
    WCM_OPTION_CHANNEL_3,
    WCM_OPTION_CHANNEL_4,
    WCM_OPTION_CHANNEL_5,
    WCM_OPTION_CHANNEL_6,
    WCM_OPTION_CHANNEL_7,
    WCM_OPTION_CHANNEL_8,
    WCM_OPTION_CHANNEL_9,
    WCM_OPTION_CHANNEL_10,
    WCM_OPTION_CHANNEL_11,
    WCM_OPTION_CHANNEL_12,
    WCM_OPTION_CHANNEL_13,
};

static u8 SearchStart(ACWORK *wk);
static u8 SearchAround2ndLap(ACWORK *wk);
static u8 SearchAround(ACWORK *wk);
static u8 SearchDifferChannel(ACWORK *wk);
static u8 SearchStealth(ACWORK *wk);
static u8 SearchEndCheck(ACWORK *wk, u8 phase);
static u8 NextSearchCheck(ACWORK *wk, u8 phase);
static u8 CheckStartStealthSearch(ACWORK *wk);
static void ScanStart(void *bssid, void *essid, int channel, int mode);

u8 DWCi_AC_SearchAP(void)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    u8 dwc_phase = DWCi_AC_GetPhase();
    int wcm_phase = WCM_GetPhase();

    if (dwc_phase == AC_PHASE_SEARCH_START && wcm_phase == WCM_PHASE_IDLE) {
        dwc_phase = SearchStart(wk);
    } else if (dwc_phase == AC_PHASE_SEARCH_END) {
        dwc_phase = SearchEndCheck(wk, dwc_phase);
    } else if (wcm_phase == WCM_PHASE_IDLE || wcm_phase == WCM_PHASE_SEARCH) {
        dwc_phase = SearchEndCheck(wk, dwc_phase);
        if (dwc_phase != AC_PHASE_CONNECT_START) {
            if (dwc_phase == AC_PHASE_SEARCH_AROUND) {
                dwc_phase = SearchAround(wk);
            } else if (dwc_phase == AC_PHASE_SEARCH_DIFFER_CHANNEL) {
                dwc_phase = SearchDifferChannel(wk);
            } else if (dwc_phase == AC_PHASE_SEARCH_STEALTH) {
                dwc_phase = SearchStealth(wk);
            }
        }
    }

    return dwc_phase;
}

void DWCi_AC_SetStealthChannel(u16 channel)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    if (channel > 13) {
        channel = 13;
    }

    wk->stealthChannel |= (1 << (channel - 1));
}

s8 DWCi_AC_GetStealthChannel(u16 no)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    u16 channel = wk->stealthChannel;
    if (channel == 0) {
        return -1;
    }

    for (u8 i = 0, j = 0; i < 13; i++) {
        if ((channel & (1 << i)) != 0) {
            if (j == no) {
                return i;
            }
            j++;
        }
    }
    return -1;
}

static inline void iScanStartAround(ACWORK *wk)
{
    wk->timeOut = OS_GetTick();
    ScanStart(WCM_BSSID_ANY, WCM_ESSID_ANY, wk->searchChannel, WCM_OPTION_SCANTYPE_PASSIVE);
}

static inline void iScanStartDifferChannel(ACWORK *wk)
{
    wk->timeOut = OS_GetTick();
    ScanStart(wk->bssDesc[wk->searchListNo].bssid, wk->bssDesc[wk->searchListNo].ssid, wk->bssDesc[wk->searchListNo].channel - 1, WCM_OPTION_SCANTYPE_ACTIVE);
}

static inline void iScanStartStealth(ACWORK *wk)
{
    wk->timeOut = OS_GetTick();
    ScanStart(WCM_BSSID_ANY, wk->searchList[wk->searchListNo].ssid, wk->searchChannel, WCM_OPTION_SCANTYPE_ACTIVE);
}

void DWCi_AC_SearchRestart(u8 phase)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    switch (phase) {
    case AC_PHASE_SEARCH_AROUND:
        iScanStartAround(wk);
        break;
    case AC_PHASE_SEARCH_DIFFER_CHANNEL:
        iScanStartDifferChannel(wk);
        break;
    case AC_PHASE_SEARCH_STEALTH:
        iScanStartStealth(wk);
        break;
    }
}

static u8 SearchStart(ACWORK *wk)
{
    wk->timeOut = OS_GetTick();
    wk->searchChannel = 0;
    iScanStartAround(wk);
    return AC_PHASE_SEARCH_AROUND;
}

static u8 SearchAround(ACWORK *wk)
{
    u64 ms = OS_TicksToMilliSeconds(OS_GetTick() - wk->timeOut);

    if (ms >= 300) {
        wk->searchChannel += 2;
        if (wk->searchChannel >= 13) {
            return NextSearchCheck(wk, AC_PHASE_SEARCH_AROUND);
        } else {
            iScanStartAround(wk);
        }
    }

    return AC_PHASE_SEARCH_AROUND;
}

static u8 SearchAround2ndLap(ACWORK *wk)
{
    wk->count = 0;
    wk->aroundCount++;
    DWCi_AC_MakeSearchList(SEARCH_LIST_TYPE_AROUND);
    wk->searchChannel = 1;

    return AC_PHASE_SEARCH_AROUND;
}

static u8 SearchDifferChannel(ACWORK *wk)
{
    u64 ms = OS_TicksToMilliSeconds(OS_GetTick() - wk->timeOut);

    if (ms >= 150 || wk->findList[wk->searchListNo].find == TRUE) {
        wk->findList[wk->searchListNo].find = TRUE;
        while (wk->searchListNo < wk->findListNum) {
            if (wk->findList[wk->searchListNo].find == FALSE) {
                break;
            }
            wk->searchListNo++;
        }

        if (wk->findListNum <= wk->searchListNo) {
            wk->searchListNo = 0;
            return NextSearchCheck(wk, AC_PHASE_SEARCH_DIFFER_CHANNEL);
        } else {
            iScanStartDifferChannel(wk);
        }
    }

    return AC_PHASE_SEARCH_DIFFER_CHANNEL;
}

static u8 SearchStealth(ACWORK *wk)
{
    u64 ms = OS_TicksToMilliSeconds(OS_GetTick() - wk->timeOut);

    if (ms >= 150 || wk->searchList[wk->searchListNo].find == TRUE) {
        wk->searchList[wk->searchListNo].find = FALSE;
        wk->searchListNo++;

        if (wk->searchListNum <= wk->searchListNo) {
            wk->count++;
            wk->searchListNo = 0;
            wk->searchChannel = DWCi_AC_GetStealthChannel(wk->count);
        }

        if (wk->searchChannel < 0) {
            wk->count = 0;
            return NextSearchCheck(wk, AC_PHASE_SEARCH_STEALTH);
        } else {
            iScanStartStealth(wk);
        }
    }

    return AC_PHASE_SEARCH_STEALTH;
}

static u8 NextSearchCheck(ACWORK *wk, u8 phase)
{
    switch (phase) {
    case AC_PHASE_SEARCH_AROUND:
        if (wk->findListNum != 0 || wk->stealthChannel != 0) {
            if (DWCi_AC_MakeSearchList(SEARCH_LIST_TYPE_DIFFER_CHANNEL) != 0) {
                phase = AC_PHASE_SEARCH_DIFFER_CHANNEL;
            } else {
                phase = CheckStartStealthSearch(wk);
            }

        } else {
            if (wk->aroundCount < 1) {
                phase = SearchAround2ndLap(wk);
            } else {
                phase = AC_PHASE_SEARCH_END;
            }
        }
        break;
    case AC_PHASE_SEARCH_DIFFER_CHANNEL:
        phase = CheckStartStealthSearch(wk);
        break;
    case AC_PHASE_SEARCH_STEALTH:
        if (wk->aroundCount < 1) {
            phase = SearchAround2ndLap(wk);
        } else {
            phase = AC_PHASE_SEARCH_END;
        }
        break;
    }

    DWCi_AC_SearchRestart(phase);
    return phase;
}

static u8 SearchEndCheck(ACWORK *wk, u8 phase)
{
    u8 no;

    if (phase == AC_PHASE_ERROR) {
        return phase;
    }

    for (no = 0; no < wk->findListNum; no++) {
        if (wk->findList[no].state == AP_CAN_CONNECT) {
            break;
        }
    }

    if (phase == AC_PHASE_SEARCH_END) {
        if (wk->findListNum == no) {
            if (no == 0) {
                DWCi_AC_SetError(AC_ERROR_AP_NOT_FOUND);
            } else {
                DWCi_AC_SetError(AC_ERROR_INET_NOT_FOUND);
            }
            return AC_PHASE_ERROR;
        }
    } else if (wk->findListNum == 0) {
        return phase;
    } else if (wk->findListNum == no) {
        return phase;
    } else if (wk->findList[no].rssi < 20) {
        return phase;
    }

    wk->connectNo = no;

    if (WCM_EndSearchAsync() != WCM_RESULT_FAILURE) {
        wk->phaseBak = phase;
        phase = AC_PHASE_CONNECT_START;
    }
    return phase;
}

static u8 CheckStartStealthSearch(ACWORK *wk)
{
    if (wk->stealthChannel != 0 && DWCi_AC_MakeSearchList(SEARCH_LIST_TYPE_STEALTH) != 0) {
        wk->searchChannel = DWCi_AC_GetStealthChannel(0);
        return AC_PHASE_SEARCH_STEALTH;
    } else if (wk->aroundCount < 1) {
        return SearchAround2ndLap(wk);
    } else {
        return AC_PHASE_SEARCH_END;
    }
}

static void ScanStart(void *bssid, void *essid, int channel, int mode)
{
    if (channel > 12) {
        channel = 12;
    }

    WCM_SearchAsync(bssid, essid, DWC_AC_SEARCH_CHANNEL[channel] | mode);
}
