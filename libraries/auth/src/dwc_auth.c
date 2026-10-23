#include "auth/dwc_auth.h"

#include <ac/dwc_ac.h>
#include <base/dwc_report.h>
#include <bm/dwc_backup.h>
#include <bm/dwc_bm_init.h>
#include <errno.h>
#include <nitro.h>
#include <nitroWiFi.h>
#include <stdlib.h>
#include <string.h>

#include "auth/dwc_http.h"

#include "dwc_cal.h"

#define DWC_AUTH_RETRY_WAIT 5000

#define DWC_AUTH_MAX_LABELVALUE 32
#define DWC_AUTH_STACK_SIZE     4096

typedef struct DWCAuth {
    u8 stack[DWC_AUTH_STACK_SIZE];
    int retry;
    DWCAuthError error;
    DWCAuthResult result;
    DWCAuthParam param;
    DWCHttpLabelValue http_labelvalue[DWC_AUTH_MAX_LABELVALUE];
    DWCHttp *http;
    OSThread thread;
    OSMutex mutex;
    BOOL abort;
} DWCAuth;

DWCAuthIngamesnCheckResult DWCauthingamesncheckresult = DWC_AUTH_INGAMESN_NOT_CHECKED;
static BOOL DWCiswfcutil = FALSE;

static DWCAuth *DWCauth = NULL;

static DWCWiFiInfo DWC_auth_wifiinfo;

DWCHttpParam DWCauthhttpparam = {
    .url = "https://nas.test.nintendowifi.net/ac",
    .action = DWCHTTP_POST,
    .len_recvbuf = 4096,
    .alloc = NULL,
    .free = NULL,
    .ignoreca = FALSE,
    .timeout = 20000
};

static void DWCi_Auth_StartThread(void);
static DWCAuthError DWCi_Auth_Start(BOOL getwifiinfofromflash);
static void DWCi_Auth_Thread(void *arg);
static DWCAuthError DWCi_Auth_ParseHttp(void);
static DWCAuthError DWCi_Auth_FillResult(void);
static DWCAuthError DWCi_Auth_Prepare_FirstPost(DWCHttp *http, const char *gsbrcd, const char *ingamesn);
static void DWCi_Auth_SetError(DWCAuthError error);

void DWC_Auth_SetCustomNas(const char *nasaddr)
{
    DWCauthhttpparam.url = nasaddr;
}

DWCAuthError DWC_Auth_Create(DWCAuthParam *param, void *http)
{
    DWCAuthAlloc alloc = param->alloc;

    if (DWCauth != NULL) {
        return DWCAUTH_E_MEMERR;
    }

    DWCauth = alloc((u32) "ALLOC DWCauth", sizeof(DWCAuth));
    if (DWCauth == NULL) {
        return DWCAUTH_E_MEMERR;
    }

    MI_CpuClear8(DWCauth, sizeof(DWCAuth));

    DWCauth->http = http;
    DWCauthingamesncheckresult = DWC_AUTH_INGAMESN_NOT_CHECKED;

    MI_CpuClear8(&DWCauth->result, sizeof(DWCAuthResult));
    DWCauth->result.returncode = 20100;

    DWCauth->param = *param;
    DWCauth->param.ingamesn[DWCAUTH_INGAMESN_LEN - 1] = '\0';
    DWCauth->param.gsbrcd[DWCAUTH_GSBRCDBYTES - 1] = '\0';

    DWCauth->error = DWCi_Auth_Start(TRUE);
    if (DWCauth->error != DWCAUTH_E_NOERR) {
        return DWCauth->error;
    }

    DWCiswfcutil = FALSE;

    DWCi_Auth_StartThread();

    return DWCAUTH_E_NOERR;
}

DWCAuthError DWC_Auth_Create_WFCUtil(DWCAuthParam *param, void *http)
{
    DWCAuthError error = DWC_Auth_Create(param, http);

    DWCiswfcutil = TRUE;

    return error;
}

static void DWCi_Auth_StartThread(void)
{
    OS_InitMutex(&DWCauth->mutex);
    DWCauth->abort = FALSE;

    if (DWCauth->thread.id != 0 && !OS_IsThreadTerminated(&DWCauth->thread)) {
        return;
    }

    OS_CreateThread(&DWCauth->thread, DWCi_Auth_Thread, &DWCauth, DWCauth->stack + sizeof(DWCauth->stack), sizeof(DWCauth->stack), 16);
    OS_WakeupThreadDirect(&DWCauth->thread);
}

void DWC_Auth_Abort(void)
{
    if (DWCauth == NULL) {
        return;
    }

    OS_LockMutex(&DWCauth->mutex);
    DWCauth->abort = TRUE;
    OS_UnlockMutex(&DWCauth->mutex);

    if (DWCauth->http != NULL) {
        DWC_Http_Abort(DWCauth->http);
    }

    if (DWCauth->thread.id != 0) {
        OS_JoinThread(&DWCauth->thread);
    }
}

void DWC_Auth_Destroy(void)
{
    if (DWCauth == NULL) {
        return;
    }

    DWCAuthFree free = DWCauth->param.free;

    if (DWCauth->http != NULL) {
        DWC_Http_Destroy(DWCauth->http);
    }

    free((u32) "FREE DWCauth", DWCauth, 0);
    DWCauth = NULL;
}

void DWC_Auth_Join(void)
{
    if (DWCauth->thread.id != 0) {
        OS_JoinThread(&DWCauth->thread);
    }
}

DWCAuthError DWC_Auth_GetError(void)
{
    if (DWCauth == NULL) {
        return DWCAUTH_E_NOTINITIALIZED;
    }

    OS_LockMutex(&DWCauth->mutex);
    DWCAuthError error = DWCauth->error;
    OS_UnlockMutex(&DWCauth->mutex);

    return error;
}

void DWC_Auth_GetResult(DWCAuthResult *result)
{
    if (DWCauth == NULL) {
        MI_CpuClear8(result, sizeof(DWCAuthResult));
    }

    MI_CpuCopy8(&DWCauth->result, result, sizeof(DWCAuthResult));

    if (result->returncode < 20000 || result->returncode >= 30000) {
        result->returncode = 20998;
    }
    if (result->returncode >= 20100) {
        result->returncode = -result->returncode;
    }
}

static DWCAuthError DWCi_Auth_Start(BOOL getwifiinfofromflash)
{
    if (strcmp(DWCauthhttpparam.url, DWC_AUTH_NAS_URL) != 0) {
        DWCauthhttpparam.ignoreca = TRUE;
    }

    DWCauthhttpparam.alloc = DWCauth->param.alloc;
    DWCauthhttpparam.free = DWCauth->param.free;

    if (DWC_Http_Create(DWCauth->http, &DWCauthhttpparam) != DWCHTTP_E_NOERR) {
        return DWCAUTH_E_HTTPINITERR;
    }

    if (getwifiinfofromflash == TRUE) {
        DWCi_BM_GetWiFiInfo(&DWC_auth_wifiinfo);
    }

    DWCauth->error = DWCi_Auth_Prepare_FirstPost(DWCauth->http, DWCauth->param.gsbrcd, DWCauth->param.ingamesn);
    if (DWCauth->error != DWCAUTH_E_NOERR) {
        return DWCAUTH_E_HTTPINITERR;
    }

    if (DWC_Http_FinishHeader(DWCauth->http) != DWCHTTP_E_NOERR) {
        return DWCAUTH_E_HTTPINITERR;
    }

    DWC_Http_StartThread(DWCauth->http, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);

    return DWCAUTH_E_NOERR;
}

static void DWCi_Auth_Thread(void *arg)
{
    int retrycount = 0;
    BOOL getwifiinfofromflash;

    while (TRUE) {
        if (DWCauth->http->thread.id != 0) {
            OS_JoinThread(&DWCauth->http->thread);
        }

        if (DWCauth->http->error != DWCHTTP_E_FINISH) {
            DWCauth->result.returncode = 20100;

            if (DWCauth->http->error == DWCHTTP_E_ABORT) {
                DWCi_Auth_SetError(DWCAUTH_E_ABORT);
                return;
            }

            if (retrycount > 2) {
                if (DWCauth->http->error == DWCHTTP_E_DNSERR) {
                    DWCi_Auth_SetError(DWCAUTH_E_DNSERR);
                } else if (DWCauth->http->error == DWCHTTP_E_CONERR) {
                    DWCi_Auth_SetError(DWCAUTH_E_HTTPCONERR);
                } else {
                    DWCi_Auth_SetError(DWCAUTH_E_HTTPERR);
                }

                return;
            }

            retrycount++;
            getwifiinfofromflash = TRUE;
            goto retry;
        }

        DWCAuthError result_httpparse = DWCi_Auth_ParseHttp();

        switch (result_httpparse) {
        case DWCAUTH_E_FINISH:
            DWCi_Auth_SetError(DWCAUTH_E_FINISH);
            return;
        case DWCAUTH_E_NASALREADYEXIST:
            if (retrycount > 2) {
                DWCi_Auth_SetError(DWCAUTH_E_NASALREADYEXIST);
                return;
            }

            retrycount++;
            getwifiinfofromflash = FALSE;
            goto retry;
        case DWCAUTH_E_NASACCTEXPIRE:
            DWCi_Auth_SetError(result_httpparse);
            return;
        default:
            if (retrycount >= 2) {
                DWCi_Auth_SetError(result_httpparse);
                return;
            }

            retrycount++;
            getwifiinfofromflash = TRUE;
            goto retry;
        }

retry:
        OSTick startwait = OS_GetTick();

        while (OS_TicksToMilliSeconds(OS_GetTick() - startwait) < DWC_AUTH_RETRY_WAIT) {
            OS_LockMutex(&DWCauth->mutex);
            if (DWCauth->abort == TRUE) {
                DWCauth->result.returncode = 20100;
                OS_UnlockMutex(&DWCauth->mutex);
                DWCi_Auth_SetError(DWCAUTH_E_ABORT);
                return;
            }
            OS_UnlockMutex(&DWCauth->mutex);
            OS_Sleep(DWC_AUTH_RETRY_WAIT);
        }

        DWC_Http_Destroy(DWCauth->http);
        OS_LockMutex(&DWCauth->mutex);
        DWCauth->error = DWCi_Auth_Start(getwifiinfofromflash);

        if (DWCauth->error != DWCAUTH_E_NOERR) {
            DWCauth->result.returncode = 20100;
            OS_UnlockMutex(&DWCauth->mutex);
            return;
        }

        OS_UnlockMutex(&DWCauth->mutex);
    }
}

static DWCAuthError DWCi_Auth_ParseHttp(void)
{
    DWCAuthAlloc alloc = DWCauth->param.alloc;
    DWCAuthFree free = DWCauth->param.free;
    DWCHttp *http = DWCauth->http;
    u8 *bmwork = NULL;

    if (DWC_Http_ParseResult(http, FALSE) != TRUE) {
        DWCauth->result.returncode = 20100;
        return DWCAUTH_E_NASPARSEERR;
    }

    if (DWCi_Auth_FillResult() != DWCAUTH_E_NOERR) {
        return DWCAUTH_E_NASPARSEERR;
    }

    if (DWCauth->result.returncode < 20100) {
        if (DWCauth->result.returncode == 20002) {
            bmwork = alloc((u32) "ALLOC bmwork", DWC_BM_INIT_WORK_SIZE + 31);
            if (bmwork == NULL) {
                DWCauth->result.returncode = 20100;
                return DWCAUTH_E_MEMERR;
            }

            if (DWCi_AUTH_UpdateWiFiID(&DWC_auth_wifiinfo, (void *)((u32)(bmwork + 31) & ~31)) != TRUE) {
                free((u32) "FREE bmwork", bmwork, 0);
                DWCauth->result.returncode = 20100;
                return DWCAUTH_E_BMERR;
            }

            free((u32) "FREE bmwork", bmwork, 0);
        }

        return DWCAUTH_E_FINISH;
    }

    switch (DWCauth->result.returncode) {
    case 20104:
        DWCi_AUTH_RemakeWiFiID(&DWC_auth_wifiinfo);
        DWCauth->result.returncode = 20104;
        return DWCAUTH_E_NASALREADYEXIST;
    case 20108:
        bmwork = alloc((u32) "ALLOC bmwork", DWC_BM_INIT_WORK_SIZE);
        if (bmwork == NULL) {
            DWCauth->result.returncode = 20108;
            return DWCAUTH_E_NASACCTEXPIRE;
        }

        DWCi_AUTH_MakeWiFiID((void *)((u32)(bmwork + 31) & ~31));

        free((u32) "FREE bmwork", bmwork, 0);
        DWCauth->result.returncode = 20108;
        return DWCAUTH_E_NASACCTEXPIRE;
    }

    return DWCAUTH_E_NASERR;
}

static DWCAuthError DWCi_Auth_FillResult(void)
{
    char *endptr = NULL;

    endptr = DWC_Http_GetResult(DWCauth->http, "Date");
    if (endptr != NULL) {
        DWC_Auth_SetNasTimeDiff(endptr);
    }

    endptr = NULL;
    errno = ENOERR;

    int returncode = atoi(DWC_Http_GetResult(DWCauth->http, "httpresult"));
    if (errno == ERANGE) {
        DWCauth->result.returncode = 20101;
        return DWCAUTH_E_HTTPPARSEERR;
    }

    if (returncode != 200) {
        DWCauth->result.returncode = returncode + 23000;
        return DWCAUTH_E_NASERR;
    }

    if (DWC_Http_GetBase64DecodedResult(DWCauth->http, "returncd", DWCauth->result.retcode, sizeof(DWCauth->result.retcode)) <= 0) {
        DWCauth->result.returncode = 20101;
        return DWCAUTH_E_NASPARSEERR;
    }

    returncode = strtol(DWCauth->result.retcode, &endptr, 10);
    if (endptr != DWCauth->result.retcode + strlen(DWCauth->result.retcode)) {
        DWCauth->result.returncode = 20101;
        return DWCAUTH_E_HTTPPARSEERR;
    }

    DWCauth->result.returncode = returncode + 20000;

    if (returncode < 100) {
        DWCauth->result.token[0] = '\0';
        DWCauth->result.locator[0] = '\0';
        DWCauth->result.challenge[0] = '\0';
        DWCauth->result.datetime[0] = '\0';
        DWCauth->result.cookie[0] = '\0';
        DWC_Http_GetBase64DecodedResult(DWCauth->http, "token", DWCauth->result.token, sizeof(DWCauth->result.token));
        DWC_Http_GetBase64DecodedResult(DWCauth->http, "locator", DWCauth->result.locator, sizeof(DWCauth->result.locator));
        DWC_Http_GetBase64DecodedResult(DWCauth->http, "challenge", DWCauth->result.challenge, sizeof(DWCauth->result.challenge));
        DWC_Http_GetBase64DecodedResult(DWCauth->http, "datetime", DWCauth->result.datetime, sizeof(DWCauth->result.datetime));
        DWC_Http_GetRawResult(DWCauth->http, "Set-Cookie", DWCauth->result.cookie, sizeof(DWCauth->result.cookie));

        DWCauth->result.cookie[43] = '\0';

        if (returncode != 40) {
            DWCauthingamesncheckresult = DWC_AUTH_INGAMESN_VALID;
        } else {
            DWCauthingamesncheckresult = DWC_AUTH_INGAMESN_INVALID;
        }
    }
    return DWCAUTH_E_NOERR;
}

extern BOOL DWCishotspot;

DWCAuthError DWCi_Auth_Prepare_FirstPost(DWCHttp *http, const char *gsbrcd, const char *ingamesn)
{
    if (DWC_auth_wifiinfo.attestedUserId == 0) {
        if (DWC_Http_Add_PostBase64Item(http, "action", "acctcreate", strlen("acctcreate")) != DWCAUTH_E_NOERR) {
            return DWCAUTH_E_CANTADDPOSTITEM;
        }
    } else {
        if (DWC_Http_Add_PostBase64Item(http, "action", "login", strlen("login")) != DWCAUTH_E_NOERR) {
            return DWCAUTH_E_CANTADDPOSTITEM;
        }
        if (DWC_Http_Add_PostBase64Item(http, "gsbrcd", gsbrcd, strlen(gsbrcd)) != DWCAUTH_E_NOERR) {
            return DWCAUTH_E_CANTADDPOSTITEM;
        }
    }

    DWCCalInfo calinfo;
    if (!DWC_Auth_GetCalInfoFromWiFiInfo(&calinfo, &DWC_auth_wifiinfo)) {
        return DWCAUTH_E_CALERR;
    }
    if (!DWC_Auth_SetCalInfoToHttp(http, &calinfo, DWCishotspot)) {
        return DWCAUTH_E_CANTADDPOSTITEM;
    }

    if (DWCiswfcutil == TRUE && DWC_Http_Add_PostBase64Item(http, "iswfc", "Y", strlen("Y")) != DWCAUTH_E_NOERR) {
        return DWCAUTH_E_CANTADDPOSTITEM;
    }
    if (wcslen(ingamesn) != 0 && DWC_Http_Add_PostBase64Item(http, "ingamesn", ingamesn, wcslen(ingamesn) * sizeof(u16)) != DWCAUTH_E_NOERR) {
        return DWCAUTH_E_CANTADDPOSTITEM;
    }

    return DWCAUTH_E_NOERR;
}

static void DWCi_Auth_SetError(DWCAuthError error)
{
    OS_LockMutex(&DWCauth->mutex);
    DWCauth->error = error;
    OS_UnlockMutex(&DWCauth->mutex);
}
