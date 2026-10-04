#include <nitro.h>
#include <nitroWiFi.h>

#include "ac.h"
#include "search.h"

u8 DWCi_AC_ConnectRetryAP(void)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    u8 phase = AC_PHASE_CONNECT_RETRY;

    switch (WCM_GetPhase()) {
    case WCM_PHASE_IDLE:
        phase = wk->phaseBak;

        if (wk->duplicateFlag == TRUE) {
            phase = AC_PHASE_CONNECT_START;
            wk->findList[wk->connectNo].state = AP_CAN_CONNECT;
        } else if (phase >= AC_PHASE_SEARCH_AROUND && phase <= AC_PHASE_SEARCH_STEALTH) {
            DWCi_AC_SearchRestart(phase);
        }

        break;
    case WCM_PHASE_SEARCH:
        WCM_EndSearchAsync();
        break;
    case WCM_PHASE_DCF:
        WCM_DisconnectAsync();
        break;
    case WCM_PHASE_IRREGULAR:
        WCM_TerminateAsync();
        DWCi_AC_SetError(AC_ERROR_WCM_IRREGULAR);
        phase = AC_PHASE_ERROR;
        break;
    case WCM_PHASE_FATAL_ERROR:
        DWCi_AC_SetError(AC_ERROR_WCM_FATAL);
        phase = AC_PHASE_ERROR;
        break;
    default:
        break;
    }

    return phase;
}
