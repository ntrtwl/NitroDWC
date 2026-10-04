#include "error.h"

#include <nitro.h>

#include "ac.h"
#include "close.h"

static int GetProgramError(int error);
static int GetIrregularError(void);
static int GetNotFoundAP(ACHOLD *hd);
static int GetNotFoundInet(ACHOLD *hd);

u8 DWCi_AC_Error(void)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);

    if (DWCi_AC_CloseNetwork(&hd->phaseError) == TRUE) {
        return AC_PHASE_FATAL_ERROR;
    } else {
        return AC_PHASE_ERROR;
    }
}

int DWCi_AC_GetResult(void)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    int error = DWCi_AC_GetError();

    if (error < AC_ERROR_WCM_IRREGULAR) {
        return GetProgramError(error);
    } else if (error < AC_ERROR_AP_NOT_FOUND) {
        return GetIrregularError();
    } else if (error == AC_ERROR_AP_NOT_FOUND) {
        return GetNotFoundAP(hd);
    } else {
        return GetNotFoundInet(hd);
    }
}

static int GetProgramError(int error)
{
    switch (error) {
    case AC_ERROR_WCM_STARTUP:
        return DWC_AC_STATE_ERROR_START_UP;
    case AC_ERROR_WCM_FATAL:
        return DWC_AC_STATE_ERROR_FATAL;
    case AC_ERROR_SOC_STARTUP:
        return DWC_AC_STATE_ERROR_SOCKET_START;
    case AC_ERROR_NETCHECK_CREATE:
        return DWC_AC_STATE_ERROR_NETCHECK_CREATE;
    default:
        return DWC_AC_STATE_NULL;
    }
}

static int GetIrregularError(void)
{
    return DWC_AC_STATE_ERROR_IRREGULAR;
}

static int GetNotFoundAP(ACHOLD *hd)
{
    if (hd->findAP == FALSE) {
        return DWC_AC_ECODE_AP_NOT_FOUND;
    } else {
        return DWC_AC_ECODE_SSID_MISMATCH;
    }
}

static int GetNotFoundInet(ACHOLD *hd)
{
    int ret;
    int endType = (hd->overrideType ? hd->overrideType : hd->endType);

    if (hd->endSequence < AC_PHASE_CONNECT_TEST_START) {
        if (hd->endState == AP_WEP_FAILURE) {
            ret = DWC_AC_ECODE_WEP_FAILURE - endType;
        } else if (hd->endState == AP_OVER_CAPACITY) {
            ret = DWC_AC_ECODE_OVER_CAPACITY - endType;
        } else {
            ret = DWC_AC_ECODE_NOT_CONNECTED - endType;
        }
    } else if (hd->endSequence < AC_PHASE_CONNECT_TEST_CREATE) {
        ret = DWC_AC_ECODE_DHCP - endType;
    } else if (hd->errorTest == 0) {
        ret = DWC_AC_ECODE_DISCONNECTED - endType;
    } else if (hd->errorTest == -1) {
        ret = DWC_AC_ECODE_NETCHECK_DNS - endType;
    } else if (hd->errorTest == -2) {
        ret = DWC_AC_ECODE_NETCHECK_1 - endType;
    } else if (hd->errorTest == -3) {
        ret = DWC_AC_ECODE_NETCHECK_2 - endType;
    } else if (hd->errorTest == -4) {
        ret = DWC_AC_ECODE_AUTH_1 - endType;
    } else if (hd->errorTest == -5) {
        ret = DWC_AC_ECODE_AUTH_2 - endType;
    } else if (hd->errorTest == -6) {
        ret = DWC_AC_ECODE_AUTH_3 - endType;
    } else {
        ret = hd->errorTest;
    }

    return ret;
}
