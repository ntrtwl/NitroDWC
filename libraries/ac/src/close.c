#include "close.h"

#include <auth/dwc_netcheck.h>
#include <nitro.h>

#include "ac.h"

static int DisconnectAP(void);
static BOOL CloseSocket(void);

BOOL DWCi_AC_CloseNetwork(u8 *phase)
{
    if (*phase <= AC_PHASE_CONNECT_TEST_START) {
        int result = DisconnectAP();
        if (result == 1) {
            *phase = AC_PHASE_NONE;
            return TRUE;
        } else if (result == -1) {
            *phase = AC_PHASE_FATAL_ERROR;
            return TRUE;
        }
    } else if (*phase == AC_PHASE_CONNECT_TEST_PROCESS) {
        DWC_Netcheck_Abort();
        DWC_Netcheck_Destroy();
        *phase = AC_PHASE_CONNECT_TEST_GET_IP;
    } else if (*phase < AC_PHASE_FATAL_ERROR && CloseSocket() == TRUE) {
        *phase = AC_PHASE_CONNECT_TEST_START;
    }

    return FALSE;
}

static int DisconnectAP(void)
{
    switch (WCM_GetPhase()) {
    case WCM_PHASE_NULL:
        return 1;
    case WCM_PHASE_WAIT:
        WCM_Finish();
        break;
    case WCM_PHASE_IDLE:
        WCM_CleanupAsync();
        break;
    case WCM_PHASE_SEARCH:
        WCM_EndSearchAsync();
        break;
    case WCM_PHASE_DCF:
        WCM_DisconnectAsync();
        break;
    case WCM_PHASE_IRREGULAR:
        WCM_TerminateAsync();
        break;
    case WCM_PHASE_FATAL_ERROR:
        DWCi_AC_SetError(AC_ERROR_WCM_FATAL);
        return -1;
    default:
        break;
    }
    return 0;
}

static BOOL CloseSocket(void)
{
    if (SOCL_CalmDown() != SOCL_ESUCCESS) {
        return FALSE;
    }

    int result = SOC_Cleanup();
    return result == 0 || result == SOC_ENETRESET;
}
