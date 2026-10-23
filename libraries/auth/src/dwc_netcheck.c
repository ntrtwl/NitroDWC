#include "auth/dwc_netcheck.h"

#include <base/dwc_report.h>
#include <bm/dwc_backup.h>
#include <bm/dwc_bm_init.h>
#include <errno.h>
#include <nitro.h>
#include <nitroWiFi.h>
#include <stdlib.h>
#include <string.h>

#include "auth/dwc_http.h"
#include "auth/util_alloc.h"

#include "dwc_cal.h"
#include "dwc_memfunc.h"

#define DWC_NETCHECK_STACK_SIZE     4096
#define DWC_NETCHECK_MAX_LABELVALUE 32

typedef struct DWCNetcheck {
    u8 stack[DWC_NETCHECK_STACK_SIZE];
    DWCNetcheckError error;
    int returncode;
    DWCHttpLabelValue http_labelvalue[DWC_NETCHECK_MAX_LABELVALUE];
    DWCNetcheckParam param;
    char *body_302;
    char *body_wayport;
    OSThread thread;
    OSMutex mutex;
} DWCNetcheck;

typedef enum {
    DWC_SIMPLENETCHECK_STAGE_DIRTY = 0,
    DWC_SIMPLENETCHECK_STAGE_CONNECTING,
    DWC_SIMPLENETCHECK_STAGE_RECEIVING,
    DWC_SIMPLENETCHECK_STAGE_DISCONNECTING,
} DWCSimpleNetcheckStage;

#define DWCSIMPLENETCHECK_RECVBUF_SIZE 32

typedef struct DWCSimpleNetcheck {
    int sock;
    SOCSockAddrIn socaddr;
    SOCPollFD pollfd;
    char recvbuf[DWCSIMPLENETCHECK_RECVBUF_SIZE];
    int recvindex;
    DWCSimpleNetcheckStage stage;
    DWCSimpleNetcheckResult result;
    OSTick finishtick;
} DWCSimpleNetcheck;

BOOL DWCishotspot = FALSE;

static DWCNetcheck *DWCnetcheck = NULL;
static DWCHttp *DWChttp = NULL;

static DWCHttpParam DWCnetcheckhttpparam;
static DWCAuthParam DWCnetcheckauthparam;

static char *DWCnetcheckblurl = DWC_NETCHECK_BL_URL;

static DWCSimpleNetcheck *DWCsimplecheckwork;
static u32 DWCconntestip;
static BOOL DWCallow302;

extern DWCHttpParam DWCauthhttpparam;

static void DWCi_Netcheck_StartThread(void);
static void DWCi_Netcheck_Thread(void *arg);
static void DWCi_Netcheck_SetError(DWCNetcheckError error);
static void DWCi_SimpleNetcheckSendRequest(void);
static void DWCi_SimpleNetcheckReceiveReply(void);

DWCNetcheckError DWC_Netcheck_Create(DWCNetcheckParam *param)
{
    DWCAuthAlloc alloc = param->alloc;

    if (DWCnetcheck != NULL) {
        return DWCNETCHECK_E_MEMERR;
    }

    DWCnetcheck = alloc((u32) "ALLOC DWCnetcheck", sizeof(DWCNetcheck));
    if (DWCnetcheck == NULL) {
        return DWCNETCHECK_E_MEMERR;
    }

    MI_CpuClear8(DWCnetcheck, sizeof(DWCNetcheck));

    DWCnetcheck->returncode = -99999;

    DWCnetcheck->param = *param;

    if (DWChttp != NULL) {
        return DWCNETCHECK_E_MEMERR;
    }

    DWChttp = alloc((u32) "ALLOC DWChttp", sizeof(DWCHttp));
    if (DWChttp == NULL) {
        return DWCNETCHECK_E_MEMERR;
    }

    DWCishotspot = FALSE;

    OS_InitMutex(&DWCnetcheck->mutex);
    DWCi_Netcheck_StartThread();

    return DWCNETCHECK_E_NOERR;
}

void DWC_Netcheck_SetAllow302(BOOL allow302)
{
    DWCallow302 = allow302;
}

void DWC_Netcheck_Destroy(void)
{
    DWCAuthFree free = DWCnetcheck->param.free;

    if (DWChttp != NULL) {
        DWC_Http_Destroy(DWChttp);
        free((u32) "FREE DWChttp", DWChttp, 0);
        DWChttp = NULL;
    }

    DWC_Auth_Destroy();

    if (DWCnetcheck != NULL) {
        if (DWCnetcheck->body_302 != NULL) {
            free((u32) "FREE DWCnetcheck->body_302", DWCnetcheck->body_302, 0);
            DWCnetcheck->body_302 = NULL;
        }
        if (DWCnetcheck->body_wayport != NULL) {
            free((u32) "FREE DWCnetcheck->body_wayport", DWCnetcheck->body_wayport, 0);
            DWCnetcheck->body_wayport = NULL;
        }
        free((u32) "FREE DWCnetcheck", DWCnetcheck, 0);
        DWCnetcheck = NULL;
    }

    return;
}

void DWC_Netcheck_Abort(void)
{
    if (DWCnetcheck == NULL) {
        return;
    }

    if (DWChttp != NULL) {
        DWC_Http_Abort(DWChttp);
    }

    DWC_Auth_Abort();

    if (DWCnetcheck->thread.id != 0) {
        OS_JoinThread(&DWCnetcheck->thread);
    }

    DWCnetcheck->returncode = -7;
}

DWCNetcheckError DWC_Netcheck_GetError(void)
{
    OS_LockMutex(&DWCnetcheck->mutex);
    DWCNetcheckError error = DWCnetcheck->error;
    OS_UnlockMutex(&DWCnetcheck->mutex);

    return error;
}

int DWC_Netcheck_GetReturnCode(void)
{
    return DWCnetcheck->returncode;
}

void DWC_Netcheck_SetCustomBlacklist(const char *url)
{
    DWCnetcheckblurl = url;
}

static void DWCi_Netcheck_StartThread(void)
{
    if (DWCnetcheck->thread.id != 0 && !OS_IsThreadTerminated(&DWCnetcheck->thread)) {
        return;
    }

    OS_CreateThread(&DWCnetcheck->thread, DWCi_Netcheck_Thread, DWCnetcheck, DWCnetcheck->stack + sizeof(DWCnetcheck->stack), sizeof(DWCnetcheck->stack), 16);
    OS_WakeupThreadDirect(&DWCnetcheck->thread);
}

static void DWCi_Netcheck_Thread(void *arg)
{
    char *httpretcode;
    int httpretcodei;
    DWCWiFiInfo DWC_netcheck_wifiinfo;
    DWCAuthResult authresult;

    DWCAuthAlloc alloc = DWCnetcheck->param.alloc;
    DWCAuthFree free = DWCnetcheck->param.free;

    char *body302 = NULL;
    char *bodywayport = NULL;
    char retcode[3 + 1];

    char *url = NULL;
    int url_len;
    char *data = NULL;
    int data_len;
    char *wait = NULL;
    int wait_len;

    DWCCalInfo calinfo;

    while (TRUE) {
        DWCnetcheckhttpparam.url = DWCnetcheckblurl;
        DWCnetcheckhttpparam.action = DWCHTTP_GET;
        DWCnetcheckhttpparam.len_recvbuf = 4096;
        DWCnetcheckhttpparam.alloc = alloc;
        DWCnetcheckhttpparam.free = free;
        DWCnetcheckhttpparam.timeout = 20000;

        DWCnetcheck->returncode = -2;

        if (DWC_Http_Create(DWChttp, &DWCnetcheckhttpparam) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        if (DWC_Http_FinishHeader(DWChttp) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        DWC_Http_StartThread(DWChttp, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
        if (DWChttp->thread.id != 0) {
            OS_JoinThread(&DWChttp->thread);
        }

        switch (DWChttp->error) {
        case DWCHTTP_E_FINISH:
            break;
        case DWCHTTP_E_DNSERR:
            DWCnetcheck->returncode = -1;
        default:
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPERR);
            goto err;
        }

        if (DWC_Http_ParseResult(DWChttp, FALSE) != TRUE) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }

        httpretcode = DWC_Http_GetResult(DWChttp, "httpresult");
        httpretcodei = atoi(httpretcode);
        if (errno == ERANGE) {
            // bug: no DWC_Http_Destroy call
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }

        switch (httpretcodei) {
        case 200:
            DWCconntestip = DWChttp->hostip;
            break;
        case 302:
            DWCishotspot = TRUE;
            if (DWCnetcheck->body_wayport != NULL) {
                DWCnetcheck->returncode = -6;

                DWC_Http_Destroy(DWChttp);

                DWCnetcheckhttpparam.url = DWCauthhttpparam.url;
                DWCnetcheckhttpparam.action = DWCHTTP_POST;
                DWCnetcheckhttpparam.len_recvbuf = 512;
                DWCnetcheckhttpparam.alloc = alloc;
                DWCnetcheckhttpparam.free = free;
                DWCnetcheckhttpparam.timeout = 20000;

                if (strcmp(DWCauthhttpparam.url, DWC_AUTH_NAS_URL) != 0) {
                    DWCnetcheckhttpparam.ignoreca = TRUE;
                }

                if (DWC_Http_Create(DWChttp, &DWCnetcheckhttpparam) != DWCHTTP_E_NOERR) {
                    DWC_Http_Destroy(DWChttp);
                    DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
                    goto err;
                }

                if (!DWC_Auth_GetCalInfoFromNVRAM(&calinfo) || !DWC_Auth_SetCalInfoToHttp(DWChttp, &calinfo, TRUE)) {
                    DWC_Http_Destroy(DWChttp);
                    DWCi_Netcheck_SetError(DWCNETCHECK_E_CANTADDPOSTITEM);
                    goto err;
                }

                if (DWC_Http_Add_PostBase64Item(DWChttp, "action", "message", 7) != DWCHTTP_E_NOERR
                    || DWC_Http_Add_PostBase64Item(DWChttp, "HotSpotResponse", DWCnetcheck->body_wayport, strlen(DWCnetcheck->body_wayport)) != DWCHTTP_E_NOERR) {
                    DWC_Http_Destroy(DWChttp);
                    DWCi_Netcheck_SetError(DWCNETCHECK_E_CANTADDPOSTITEM);
                    goto err;
                }

                free((u32) "FREE DWCnetcheck->body_wayport", DWCnetcheck->body_wayport, 0);
                DWCnetcheck->body_wayport = NULL;

                if (DWC_Http_FinishHeader(DWChttp) != DWCHTTP_E_NOERR) {
                    DWC_Http_Destroy(DWChttp);
                    DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
                    goto err;
                }

                DWC_Http_StartThread(DWChttp, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
                if (DWChttp->thread.id != 0) {
                    OS_JoinThread(&DWChttp->thread);
                }

                switch (DWChttp->error) {
                case DWCHTTP_E_FINISH:
                    break;
                case DWCHTTP_E_DNSERR:
                    DWCnetcheck->returncode = -1;
                default:
                    DWC_Http_Destroy(DWChttp);
                    DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPERR);
                    goto err;
                }

                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_302TWICE);
                goto err;
            }

            body302 = DWChttp->rep.buffer;
            if (body302 == NULL) {
                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
                goto err;
            }

            DWCnetcheck->body_302 = alloc((u32) "ALLOC DWCnetcheck->body_302", strlen(body302) + 1);
            if (DWCnetcheck->body_302 == NULL) {
                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_MEMERR);
                goto err;
            }
            strncpy(DWCnetcheck->body_302, body302, strlen(body302) + 1);
            break;
        default:
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NETUNAVAIL);
            goto err;
        }

        DWC_Http_Destroy(DWChttp);

        DWCi_BM_GetWiFiInfo(&DWC_netcheck_wifiinfo);

        if (DWC_netcheck_wifiinfo.attestedUserId == 0) {
            DWCnetcheck->returncode = -3;

            DWCnetcheckauthparam.ingamesn[0] = '\0';
            DWCnetcheckauthparam.gsbrcd[0] = '\0';
            DWCnetcheckauthparam.alloc = DWCnetcheck->param.alloc;
            DWCnetcheckauthparam.free = DWCnetcheck->param.free;

            if (DWC_Auth_Create(&DWCnetcheckauthparam, DWChttp) != DWCAUTH_E_NOERR) {
                DWCi_Netcheck_SetError(DWCNETCHECK_E_AUTHINITERR);
                goto err;
            }

            DWC_Auth_Join();

            if (DWC_Auth_GetError() != DWCAUTH_E_FINISH) {
                if (DWC_Auth_GetError() == DWCAUTH_E_DNSERR) {
                    DWCnetcheck->returncode = -1;
                } else {
                    DWC_Auth_GetResult(&authresult);
                    if (DWCallow302 == TRUE && (authresult.returncode == -23302 || DWC_Auth_GetError() == DWCAUTH_E_HTTPCONERR)) {
                        DWCnetcheck->returncode = 0;
                        DWC_Auth_Destroy();
                        DWCi_Netcheck_SetError(DWCNETCHECK_E_NETAVAIL);
                        goto err;
                    }
                    DWCnetcheck->returncode = authresult.returncode;
                }
                DWC_Auth_Destroy();
                DWCi_Netcheck_SetError(DWCNETCHECK_E_AUTHERR);
                goto err;
            }

            DWC_Auth_Destroy();
        }

        if (httpretcodei == 200) {
            DWCnetcheck->returncode = 0;
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NETAVAIL);
            goto err;
        }

        DWCnetcheck->returncode = -4;

        DWCnetcheckhttpparam.url = DWCauthhttpparam.url;
        DWCnetcheckhttpparam.action = DWCHTTP_POST;
        DWCnetcheckhttpparam.len_recvbuf = 4096;
        DWCnetcheckhttpparam.alloc = alloc;
        DWCnetcheckhttpparam.free = free;
        DWCnetcheckhttpparam.timeout = 40000;

        if (strcmp(DWCauthhttpparam.url, DWC_AUTH_NAS_URL) != 0) {
            DWCnetcheckhttpparam.ignoreca = TRUE;
        }

        if (DWC_Http_Create(DWChttp, &DWCnetcheckhttpparam) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        if (!DWC_Auth_GetCalInfoFromNVRAM(&calinfo) || !DWC_Auth_SetCalInfoToHttp(DWChttp, &calinfo, TRUE)) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_CANTADDPOSTITEM);
            goto err;
        }

        if (DWC_Http_Add_PostBase64Item(DWChttp, "action", "parse", 5) != DWCHTTP_E_NOERR
            || DWC_Http_Add_PostBase64Item(DWChttp, "HTML", DWCnetcheck->body_302, strlen(DWCnetcheck->body_302)) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_CANTADDPOSTITEM);
            goto err;
        }

        free((u32) "FREE DWCnetcheck->body_302", DWCnetcheck->body_302, 0);
        DWCnetcheck->body_302 = NULL;

        if (DWC_Http_FinishHeader(DWChttp) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        DWC_Http_StartThread(DWChttp, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
        if (DWChttp->thread.id != 0) {
            OS_JoinThread(&DWChttp->thread);
        }

        switch (DWChttp->error) {
        case DWCHTTP_E_FINISH:
            break;
        case DWCHTTP_E_CONERR:
            DWC_Http_Destroy(DWChttp);
            if (DWCallow302 == TRUE) {
                DWCnetcheck->returncode = 0;
                DWCi_Netcheck_SetError(DWCNETCHECK_E_NETAVAIL);
            } else {
                DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPERR);
            }
            goto err;
        case DWCHTTP_E_DNSERR:
            DWCnetcheck->returncode = -1;
        default:
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPERR);
            goto err;
        }

        if (DWC_Http_ParseResult(DWChttp, FALSE) != TRUE) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }

        httpretcode = DWC_Http_GetResult(DWChttp, "httpresult");
        httpretcodei = atoi(httpretcode);
        if (errno == ERANGE) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }
        if (httpretcodei != 200) {
            DWC_Http_Destroy(DWChttp);
            if (DWCallow302 == TRUE && httpretcodei == 302) {
                DWCnetcheck->returncode = 0;
                DWCi_Netcheck_SetError(DWCNETCHECK_E_NETAVAIL);
            } else {
                DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            }
            goto err;
        }

        if (DWC_Http_GetBase64DecodedResult(DWChttp, "returncd", retcode, 4) <= 0) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }
        httpretcodei = atoi(retcode);
        if (errno == ERANGE) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }
        if (DWCallow302 == TRUE && httpretcodei == 114) {
            DWCnetcheck->returncode = 0;
            // bug: no DWC_Http_Destroy call
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NETAVAIL);
            goto err;
        }
        if (httpretcodei >= 100) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_AUTHERR);
            goto err;
        }

        url_len = DWC_Http_GetBase64DecodedResult(DWChttp, "url", NULL, 0);
        if (url_len <= 0) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }

        data_len = DWC_Http_GetBase64DecodedResult(DWChttp, "data", NULL, 0);
        if (data_len <= 0) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }

        wait_len = DWC_Http_GetBase64DecodedResult(DWChttp, "wait", NULL, 0);

        url = alloc((u32) "ALLOC url", url_len + 1);
        if (url == NULL) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_MEMERR);
            goto err;
        }
        data = alloc((u32) "ALLOC data_len", data_len + 1);
        if (data == NULL) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_MEMERR);
            goto err;
        }
        if (wait_len > 0) {
            wait = alloc((u32) "ALLOC wait_len", wait_len + 1);
            if (wait == NULL) {
                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_MEMERR);
                goto err;
            }
        }

        url_len = DWC_Http_GetBase64DecodedResult(DWChttp, "url", url, url_len + 1);
        if (url_len < 0) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }
        url[url_len] = '\0';
        data_len = DWC_Http_GetBase64DecodedResult(DWChttp, "data", data, data_len + 1);
        if (data_len < 0) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
            goto err;
        }
        data[data_len] = '\0';

        int wait_sec = 0;
        if (wait_len > 0) {
            wait_len = DWC_Http_GetBase64DecodedResult(DWChttp, "wait", wait, wait_len + 1);
            if (wait_len < 0) {
                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
                goto err;
            }
            wait[wait_len] = '\0';
            wait_sec = atoi(wait);
            if (errno == ERANGE) {
                DWC_Http_Destroy(DWChttp);
                DWCi_Netcheck_SetError(DWCNETCHECK_E_NASPARSEERR);
                goto err;
            }
            wait_sec *= 1000;
            if (wait_sec > 180000) {
                wait_sec = 180000;
            }
        }

        DWC_Http_Destroy(DWChttp);

        DWCnetcheck->returncode = -5;

        DWCnetcheckhttpparam.url = url;
        DWCnetcheckhttpparam.action = DWCHTTP_POST;
        DWCnetcheckhttpparam.len_recvbuf = 4096;
        DWCnetcheckhttpparam.alloc = alloc;
        DWCnetcheckhttpparam.free = free;
        DWCnetcheckhttpparam.timeout = 120000;

        if (DWC_Http_Create(DWChttp, &DWCnetcheckhttpparam) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        if (DWC_Http_Add_Body(DWChttp, data) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_CANTADDPOSTITEM);
            goto err;
        }

        if (DWC_Http_FinishHeader(DWChttp) != DWCHTTP_E_NOERR) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPINITERR);
            goto err;
        }

        DWC_Http_StartThread(DWChttp, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
        if (DWChttp->thread.id != 0) {
            OS_JoinThread(&DWChttp->thread);
        }

        switch (DWChttp->error) {
        case DWCHTTP_E_FINISH:
            break;
        case DWCHTTP_E_DNSERR:
            DWCnetcheck->returncode = -1;
        default:
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPERR);
            goto err;
        }

        if (DWC_Http_ParseResult(DWChttp, TRUE) != TRUE) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }

        bodywayport = DWChttp->rep.buffer;
        if (bodywayport == NULL) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_HTTPPARSEERR);
            goto err;
        }

        DWCnetcheck->body_wayport = alloc((u32) "ALLOC DWCnetcheck->body_wayport", strlen(bodywayport) + 1);
        if (DWCnetcheck->body_wayport == NULL) {
            DWC_Http_Destroy(DWChttp);
            DWCi_Netcheck_SetError(DWCNETCHECK_E_MEMERR);
            goto err;
        }
        strncpy(DWCnetcheck->body_wayport, bodywayport, strlen(bodywayport) + 1);

        DWC_Http_Destroy(DWChttp);

        OS_Sleep(wait_sec);
    }
err:
    if (url != NULL) {
        free((u32) "FREE url", url, 0);
    }
    if (data != NULL) {
        free((u32) "FREE data", data, 0);
    }
    if (wait != NULL) {
        free((u32) "FREE wait", wait, 0);
    }
}

static void DWCi_Netcheck_SetError(DWCNetcheckError error)
{
    OS_LockMutex(&DWCnetcheck->mutex);
    DWCnetcheck->error = error;
    OS_UnlockMutex(&DWCnetcheck->mutex);
}

BOOL DWC_SimpleNetcheckAsync(int timeout)
{
    if (DWCsimplecheckwork != NULL || DWCconntestip == 0) {
        return FALSE;
    }

    DWCsimplecheckwork = DWC_Alloc(DWC_ALLOCTYPE_AUTH, sizeof(DWCSimpleNetcheck));
    if (DWCsimplecheckwork == NULL) {
        return FALSE;
    }
    MI_CpuClear8(DWCsimplecheckwork, sizeof(DWCSimpleNetcheck));

    DWCsimplecheckwork->sock = -1;
    DWCsimplecheckwork->sock = SOC_Socket(SOC_PF_INET, SOC_SOCK_STREAM, 0);
    if (DWCsimplecheckwork->sock < 0) {
        DWC_Free(DWC_ALLOCTYPE_AUTH, DWCsimplecheckwork, 0);
        DWCsimplecheckwork = 0;
        return FALSE;
    }

    int val = SOC_Fcntl(DWCsimplecheckwork->sock, SOC_F_GETFL, 0);
    SOC_Fcntl(DWCsimplecheckwork->sock, SOC_F_SETFL, val | SOC_O_NONBLOCK);

    DWCsimplecheckwork->socaddr.len = sizeof(SOCSockAddrIn);
    DWCsimplecheckwork->socaddr.family = SOC_PF_INET;
    DWCsimplecheckwork->socaddr.port = SOC_HtoNs(80);
    DWCsimplecheckwork->socaddr.addr.addr = SOC_HtoNl(DWCconntestip);

    val = SOC_Connect(DWCsimplecheckwork->sock, &DWCsimplecheckwork->socaddr);
    if (val < 0 && val != SOC_EINPROGRESS) {
        DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_SYSFAIL;
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
        return TRUE;
    }

    DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_CONNECTING;

    if (timeout > 0) {
        DWCsimplecheckwork->finishtick = OS_GetTick() + OS_MilliSecondsToTicks(timeout);
    } else {
        DWCsimplecheckwork->finishtick = OS_GetTick() + OS_MilliSecondsToTicks(3000);
    }

    return TRUE;
}

DWCSimpleNetcheckResult DWC_SimpleNetcheckProcess(void)
{
    DWCSimpleNetcheckResult result;

    if (DWCsimplecheckwork == NULL || DWCsimplecheckwork->stage == DWC_SIMPLENETCHECK_STAGE_DIRTY) {
        return DWC_SIMPLENETCHECK_NOTINITIALIZED;
    }

    if (DWCsimplecheckwork->result == DWC_SIMPLENETCHECK_PROCESSING
        && DWCsimplecheckwork->finishtick != 0 && DWCsimplecheckwork->finishtick < OS_GetTick()) {
        DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_FAIL;
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
    }

    switch (DWCsimplecheckwork->stage) {
    case DWC_SIMPLENETCHECK_STAGE_CONNECTING:
        int val = SOC_Connect(DWCsimplecheckwork->sock, &DWCsimplecheckwork->socaddr);
        if (val < 0 && val != SOC_EINPROGRESS) {
            DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_SYSFAIL;
            DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
            SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
            SOC_Close(DWCsimplecheckwork->sock);
            return DWC_SIMPLENETCHECK_PROCESSING;
        }
        DWCi_SimpleNetcheckSendRequest();
        return DWC_SIMPLENETCHECK_PROCESSING;
    case DWC_SIMPLENETCHECK_STAGE_RECEIVING:
        DWCi_SimpleNetcheckReceiveReply();
        return DWC_SIMPLENETCHECK_PROCESSING;
    case DWC_SIMPLENETCHECK_STAGE_DISCONNECTING:
        result = DWCsimplecheckwork->result;
        DWC_Free(DWC_ALLOCTYPE_AUTH, DWCsimplecheckwork, 0);
        DWCsimplecheckwork = NULL;
        return result;
    default:
        DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_SYSFAIL;
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
        return DWC_SIMPLENETCHECK_PROCESSING;
    }
}

#define DWC_SIMPLENETCHECK_REQUEST_MESSAGE "GET / HTTP/1.0\r\nHost: conntest.nintendowifi.net\r\n\r\n"

static void DWCi_SimpleNetcheckSendRequest(void)
{
    int val = SOC_Write(DWCsimplecheckwork->sock, DWC_SIMPLENETCHECK_REQUEST_MESSAGE, strlen(DWC_SIMPLENETCHECK_REQUEST_MESSAGE));
    if (val == SOC_ENOTCONN) {
        return;
    }

    if (val <= 0) {
        DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_FAIL;
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
        return;
    }

    DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_RECEIVING;
    DWCsimplecheckwork->recvindex = 0;
}

static void DWCi_SimpleNetcheckReceiveReply(void)
{
    int i;

    int result = SOC_Read(DWCsimplecheckwork->sock, DWCsimplecheckwork->recvbuf + DWCsimplecheckwork->recvindex, DWCSIMPLENETCHECK_RECVBUF_SIZE - DWCsimplecheckwork->recvindex);
    if (result == SOCL_EWOULDBLOCK) {
        return;
    }

    if (result >= 0) {
        DWCsimplecheckwork->recvindex += result;
        if (result != 0 && DWCsimplecheckwork->recvindex < DWCSIMPLENETCHECK_RECVBUF_SIZE) {
            return;
        }

        for (i = 0; DWCsimplecheckwork->recvbuf[i] != ' ' && i < DWCSIMPLENETCHECK_RECVBUF_SIZE; i++) {
        }
        i++;
        if (i >= DWCSIMPLENETCHECK_RECVBUF_SIZE) {
            DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_FAIL;
        } else if (strncmp(&DWCsimplecheckwork->recvbuf[i], "200", 3) != 0) {
            DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_FAIL;
        } else {
            DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_SUCCESS;
        }
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
    } else {
        DWCsimplecheckwork->result = DWC_SIMPLENETCHECK_FAIL;
        DWCsimplecheckwork->stage = DWC_SIMPLENETCHECK_STAGE_DISCONNECTING;
        SOC_Shutdown(DWCsimplecheckwork->sock, SOC_SHUT_RDWR);
        SOC_Close(DWCsimplecheckwork->sock);
    }
}
