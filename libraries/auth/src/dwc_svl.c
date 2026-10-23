#include "auth/dwc_svl.h"

#include <base/dwc_error.h>
#include <errno.h>
#include <nitro.h>
#include <stdlib.h>
#include <string.h>

#include "auth/dwc_http.h"
#include "auth/util_alloc.h"

#include "dwc_cal.h"
#include "dwc_nastime.h"

typedef struct DWCSvlIntWorkarea {
    DWCAuthAlloc alloc;
    DWCAuthFree free;
    DWCHttp http;
    DWCSvlResult *result;
} DWCSvlIntWorkarea;

static DWCSvlIntWorkarea *intwork;
extern DWCHttpParam DWCauthhttpparam;

static BOOL DWCi_Svl_ParseHttp(void);

BOOL DWC_Svl_Init(DWCAuthAlloc alloc, DWCAuthFree free)
{
    if (intwork != NULL) {
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    intwork = alloc((u32) "ALLOC intwork", sizeof(DWCSvlIntWorkarea));
    if (intwork == NULL) {
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    MI_CpuClear8(intwork, sizeof(DWCSvlIntWorkarea));

    intwork->alloc = alloc;
    intwork->free = free;
    return TRUE;
}

void DWC_Svl_Cleanup(void)
{
    if (intwork == NULL) {
        return;
    }

    DWC_Http_Destroy(&intwork->http);
    intwork->free((u32) "FREE intwork", intwork, 0);
    intwork = NULL;
}

BOOL DWC_Svl_GetTokenAsync(char *svc, DWCSvlResult *result)
{
    DWCHttpParam httpparam;
    DWCCalInfo calinfo;

    intwork->result = result;
    MI_CpuClear8(result, sizeof(DWCSvlResult));

    if (intwork->http.thread.id != 0 && !OS_IsThreadTerminated(&intwork->http.thread)) {
        OS_Panic("You can't request SVL token while SVL module is working.");
    }

    httpparam.url = DWCauthhttpparam.url;
    httpparam.action = DWCHTTP_POST;
    httpparam.len_recvbuf = 4096;
    httpparam.alloc = intwork->alloc;
    httpparam.free = intwork->free;

    if (strcmp(httpparam.url, DWC_AUTH_NAS_URL) != 0) {
        httpparam.ignoreca = TRUE;
    } else {
        httpparam.ignoreca = FALSE;
    }

    httpparam.timeout = 20000;

    if (DWC_Http_Create(&intwork->http, &httpparam) != DWCHTTP_E_NOERR) {
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    if (!DWC_Auth_GetCalInfoFromNVRAM(&calinfo) || !DWC_Auth_SetCalInfoToHttp(&intwork->http, &calinfo, FALSE)) {
        DWC_Http_Destroy(&intwork->http);
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    if (DWC_Http_Add_PostBase64Item(&intwork->http, "action", "SVCLOC", 6) != DWCHTTP_E_NOERR
        || DWC_Http_Add_PostBase64Item(&intwork->http, "svc", svc, 4) != DWCHTTP_E_NOERR) {
        DWC_Http_Destroy(&intwork->http);
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    if (DWC_Http_FinishHeader(&intwork->http) != DWCHTTP_E_NOERR) {
        DWC_Http_Destroy(&intwork->http);
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    DWC_Http_StartThread(&intwork->http, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
    if (intwork->http.thread.id == 0) {
        DWC_Http_Destroy(&intwork->http);
        DWCi_SetError(DWC_ERROR_FATAL, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_FATAL);
        return FALSE;
    }

    return TRUE;
}

DWCSvlState DWC_Svl_Process(void)
{
    if (intwork == NULL) {
        return DWC_SVL_STATE_DIRTY;
    }

    if (intwork->http.thread.id == 0) {
        return DWC_SVL_STATE_IDLE;
    }

    if (OS_IsThreadTerminated(&intwork->http.thread) == TRUE) {
        switch (intwork->http.error) {
        case DWCHTTP_E_FINISH:
            if (!DWCi_Svl_ParseHttp()) {
                DWC_Http_Destroy(&intwork->http);
                return DWC_SVL_STATE_ERROR;
            } else {
                DWC_Http_Destroy(&intwork->http);
                return DWC_SVL_STATE_SUCCESS;
            }
        case DWCHTTP_E_ABORT:
            DWC_Http_Destroy(&intwork->http);
            return DWC_SVL_STATE_CANCELED;
        default:
            DWC_Http_Destroy(&intwork->http);
            DWCi_SetError(DWC_ERROR_SVL_HTTP, DWC_SVL_ERROR_BASENUM + DWC_SVL_ERROR_BADDATA);
            return DWC_SVL_STATE_ERROR;
        }
    }

    return DWC_SVL_STATE_HTTP;
}

static BOOL DWCi_Svl_ParseHttp(void)
{
    char retcode[4];
    DWCHttp *DWChttp = &intwork->http;

    if (!DWC_Http_ParseResult(DWChttp, FALSE)) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM + DWC_SVL_ERROR_BADDATA);
        return FALSE;
    }

    char *strp = DWC_Http_GetResult(DWChttp, "Date");
    if (strp != NULL) {
        DWC_Auth_SetNasTimeDiff(strp);
    }

    strp = DWC_Http_GetResult(DWChttp, "httpresult");
    if (strp == NULL) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM + DWC_SVL_ERROR_BADDATA);
        return FALSE;
    }
    int httpretcode = atoi(strp);
    if (errno == ERANGE) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM + DWC_SVL_ERROR_BADDATA);
        return FALSE;
    }
    if (httpretcode != 200) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM - httpretcode);
        return FALSE;
    }

    if (DWC_Http_GetBase64DecodedResult(DWChttp, "returncd", retcode, 4) <= 0) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM + DWC_SVL_ERROR_BADDATA);
        return FALSE;
    }
    httpretcode = atoi(retcode);
    if (errno == ERANGE) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_HTTPBASENUM + DWC_SVL_ERROR_BADDATA);
        return FALSE;
    }
    if (httpretcode >= 100) {
        DWCi_SetError(DWC_ERROR_SVL_ANY, DWC_SVL_ERROR_BASENUM - httpretcode);
        return FALSE;
    }

    DWC_Http_GetBase64DecodedResult(DWChttp, "svchost", intwork->result->svlhost, DWC_SVL_HOST_LENGTH);
    DWC_Http_GetBase64DecodedResult(DWChttp, "servicetoken", intwork->result->svltoken, DWC_SVL_TOKEN_LENGTH);
    DWC_Http_GetBase64DecodedResult(DWChttp, "statusdata", retcode, DWC_SVL_STATUSDATA_LENGTH);
    if (*retcode == 'Y') {
        intwork->result->status = TRUE;
    } else {
        intwork->result->status = FALSE;
    }

    return TRUE;
}
