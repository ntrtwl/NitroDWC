#include "auth/dwc_prof.h"

#include <ac/dwc_ac.h>
#include <base/dwc_error.h>
#include <errno.h>
#include <nitro.h>
#include <stdlib.h>
#include <string.h>

#include "dwc_cal.h"
#include "dwc_memfunc.h"

typedef struct DWCProfWorkarea {
    DWCHttp http;
    BOOL http_initialized;
    char url[DWCHTTP_MAXURL + 1];
    char *result;
    int *badwordsnum;
    int wordsnum;
} DWCProfWorkarea;

DWCProfWorkarea *intwork;

static void *DWCi_Prof_Alloc(u32 name, s32 size);
static void DWCi_Prof_Free(u32 name, void *ptr, s32 size);

extern BOOL DWCishotspot;
extern DWCHttpParam DWCauthhttpparam;

static BOOL DWCi_CheckProfanityAsync(const u16 **words, int wordsnum, const char *reserved, int timeout, char *result, int *badwordsnum, char *region)
{
    DWCHttpParam httpparam;
    DWCCalInfo calinfo;
    int totalnum_wchar;
    int wchar_index;
    u16 *sendstring = NULL;

    if (words == 0 || wordsnum <= 0 || wordsnum > 50 || result == NULL || badwordsnum == 0) {
        return FALSE;
    }

    totalnum_wchar = 0;
    for (int i = 0; i < wordsnum; i++) {
        totalnum_wchar += wcslen(words[i]) + 1;
    }
    totalnum_wchar--;
    if (totalnum_wchar <= 0 || totalnum_wchar > 500) {
        return FALSE;
    }

    if (intwork != NULL) {
        return FALSE;
    }

    MI_CpuClear8(result, wordsnum);

    intwork = DWC_Alloc(DWC_ALLOCTYPE_AUTH, sizeof(DWCProfWorkarea));
    if (intwork == NULL) {
        goto err;
    }

    MI_CpuClear8(intwork, sizeof(DWCProfWorkarea));

    intwork->http_initialized = FALSE;

    MI_CpuClear8(&calinfo, sizeof(DWCCalInfo));

    intwork->result = result;
    intwork->badwordsnum = badwordsnum;
    intwork->wordsnum = wordsnum;
    strncpy(intwork->url, DWCauthhttpparam.url, DWCHTTP_MAXURL);
    intwork->url[DWCHTTP_MAXURL] = '\0';

    char *pos = strstr(intwork->url, "/ac");
    if (pos == 0) {
        goto err;
    }
    MI_CpuCopy8("/pr", pos, 4);

    httpparam.url = intwork->url;
    httpparam.action = 0;
    httpparam.len_recvbuf = 1024;
    httpparam.alloc = DWCi_Prof_Alloc;
    httpparam.free = DWCi_Prof_Free;
    httpparam.ignoreca = 0;
    if (timeout <= 0) {
        httpparam.timeout = 10000;
    } else {
        httpparam.timeout = timeout;
    }

    if (DWC_Http_Create(&intwork->http, &httpparam) != 0) {
        goto err;
    }
    intwork->http_initialized = TRUE;

    if (!DWC_Auth_GetCalInfoFromNVRAM(&calinfo) || !DWC_Auth_SetCalInfoToHttp(&intwork->http, &calinfo, DWCishotspot)) {
        goto err;
    }

    if (DWC_Http_Add_PostBase64Item(&intwork->http, "wregion", region, 1) != DWCHTTP_E_NOERR
        || DWC_Http_Add_PostBase64Item(&intwork->http, "wenc", "UTF-16LE", 8) != DWCHTTP_E_NOERR) {
        goto err;
    }

    if (reserved != NULL && DWC_Http_Add_PostBase64Item(&intwork->http, "wtype", reserved, 4) != DWCHTTP_E_NOERR) {
        goto err;
    }

    if (DWC_Http_Add_Body(&intwork->http, "&wtype=") != DWCHTTP_E_NOERR) {
        goto err;
    }

    sendstring = DWC_Alloc(DWC_ALLOCTYPE_AUTH, (totalnum_wchar + 1) * sizeof(u16));
    if (sendstring == NULL) {
        goto err;
    }

    wchar_index = 0;
    for (int i = 0; i < wordsnum; i++) {
        wcscpy(&sendstring[wchar_index], words[i]);
        wchar_index += wcslen(words[i]);
        if (wchar_index >= totalnum_wchar) {
            break;
        }
        sendstring[wchar_index++] = '\t';
    }

    if (DWC_Http_Add_PostBase64Item(&intwork->http, "words", sendstring, totalnum_wchar * sizeof(u16)) != DWCHTTP_E_NOERR) {
        goto err;
    }

    DWC_Free(DWC_ALLOCTYPE_AUTH, sendstring, 0);
    sendstring = NULL;

    if (DWC_Http_FinishHeader(&intwork->http) != DWCHTTP_E_NOERR) {
        goto err;
    }

    DWC_Http_StartThread(&intwork->http, OS_GetThreadPriority(OS_GetCurrentThread()) - 1);
    if (intwork->http.thread.id == 0) {
        goto err;
    }
    return TRUE;
err:
    if (sendstring != NULL) {
        DWC_Free(DWC_ALLOCTYPE_AUTH, sendstring, 0);
    }

    if (intwork != NULL) {
        if (intwork->http_initialized == TRUE) {
            DWC_Http_Destroy(&intwork->http);
        }
        DWC_Free(DWC_ALLOCTYPE_AUTH, intwork, 0);
        intwork = NULL;
    }

    return FALSE;
}

#define HW_ROM_HEADER_REGION_LETTER (HW_ROM_HEADER_BUF + 0xF)

BOOL DWC_CheckProfanityAsync(const u16 **words, int wordsnum, const char *reserved, int timeout, char *result, int *badwordsnum)
{
    char regionLetter = *(char *)HW_ROM_HEADER_REGION_LETTER;
    return DWCi_CheckProfanityAsync(words, wordsnum, reserved, timeout, result, badwordsnum, &regionLetter);
}

BOOL DWC_CheckProfanityExAsync(const u16 **words, int wordsnum, const char *reserved, int timeout, char *result, int *badwordsnum, DWCProfRegion region)
{
    char regionLetter;

    switch (region) {
    case DWC_PROF_REGION_ALL:
        regionLetter = 'A';
        break;
    default:
        regionLetter = *(char *)HW_ROM_HEADER_REGION_LETTER;
        break;
    }

    return DWCi_CheckProfanityAsync(words, wordsnum, reserved, timeout, result, badwordsnum, &regionLetter);
}

DWCProfState DWC_CheckProfanityProcess(void)
{
    DWCProfState retval;

    if (intwork == NULL || intwork->http.thread.id == 0) {
        return DWC_PROF_STATE_NOT_INITIALIZED;
    }

    if (OS_IsThreadTerminated(&intwork->http.thread) == TRUE) {
        u8 aptype = DWC_AC_GetApType();
        if (aptype == DWC_AC_AP_TYPE_FALSE || aptype > DWC_AC_AP_TYPE_UNKNOWN) {
            aptype = DWC_AC_AP_TYPE_UNKNOWN;
        }
        switch (intwork->http.error) {
        case DWCHTTP_E_FINISH:
            if (!DWCi_Prof_ParseResponse()) {
                retval = DWC_PROF_STATE_FAIL;
            } else {
                retval = DWC_PROF_STATE_SUCCESS;
            }
            break;
        case DWCHTTP_E_DNSERR:
            DWCi_SetError(DWC_ERROR_DISCONNECTED, -52100 - aptype); // todo: DWC_AC_ECODE_NETCHECK_DNS from ac
            retval = DWC_PROF_STATE_FAIL;
            break;
        case DWCHTTP_E_CONERR:
        case DWCHTTP_E_SENDTOUT:
        case DWCHTTP_E_RECVTOUT:
            DWCi_SetError(DWC_ERROR_PROF_HTTP, DWC_ECODE_SEQ_ADDINS + DWC_ECODE_FUNC_PROF + DWC_ECODE_TYPE_PROF_CONN);
            retval = DWC_PROF_STATE_FAIL;
            break;
        case DWCHTTP_E_NOERR:
        case DWCHTTP_E_MEMERR:
        case DWCHTTP_E_SENDERR:
        case DWCHTTP_E_ABORT:
        default:
            DWCi_SetError(DWC_ERROR_PROF_HTTP, DWC_ECODE_SEQ_ADDINS + DWC_ECODE_FUNC_PROF + DWC_ECODE_TYPE_PROF_OTHER);
            retval = DWC_PROF_STATE_FAIL;
            break;
        }
    } else {
        return DWC_PROF_STATE_OPERATING;
    }

    DWC_Http_Destroy(&intwork->http);
    DWC_Free(DWC_ALLOCTYPE_AUTH, intwork, 0);
    intwork = NULL;
    return retval;
}

BOOL DWCi_Prof_ParseResponse(void)
{
    char retcode[4];
    char result[DWC_PROF_RESULT_STRING_LENGTH];
    DWCHttp *DWChttp = &intwork->http;

    if (DWC_Http_ParseResult(DWChttp, FALSE) == DWCHTTP_E_NOERR) {
        goto err;
    }

    char *strp = DWC_Http_GetResult(DWChttp, "Date");
    if (strp != NULL) {
        DWC_Auth_SetNasTimeDiff(strp);
    }

    strp = DWC_Http_GetResult(DWChttp, "httpresult");
    if (strp == NULL) {
        goto err;
    }
    int httpretcode = atoi(strp);
    if (errno == ERANGE) {
        goto err;
    }
    if (httpretcode != 200) {
        goto err;
    }

    if (DWC_Http_GetBase64DecodedResult(DWChttp, "returncd", retcode, 4) <= 0) {
        goto err;
    }
    httpretcode = atoi(retcode);
    if (errno == ERANGE) {
        goto err;
    }
    if (httpretcode == 1 || (httpretcode >= 100 && httpretcode != 100)) {
        DWCi_SetError(DWC_ERROR_PROF_ANY, DWC_ECODE_SEQ_ADDINS + DWC_ECODE_FUNC_PROF - httpretcode);
        return FALSE;
    }

    DWC_Http_GetBase64DecodedResult(DWChttp, "prwords", result, DWC_PROF_RESULT_STRING_LENGTH);

    *intwork->badwordsnum = 0;
    for (int i = 0; i < intwork->wordsnum; i++) {
        if (result[i] == '0') {
            intwork->result[i] = FALSE;
        } else {
            intwork->result[i] = TRUE;
            (*intwork->badwordsnum)++;
        }
    }

    return TRUE;
err:
    DWCi_SetError(DWC_ERROR_PROF_ANY, DWC_ECODE_SEQ_ADDINS + DWC_ECODE_FUNC_PROF + DWC_ECODE_TYPE_PROF_PARSE);
    return FALSE;
}

static void *DWCi_Prof_Alloc(u32 name, s32 size)
{
    return DWC_Alloc(DWC_ALLOCTYPE_AUTH, size);
}

static void DWCi_Prof_Free(u32 name, void *ptr, s32 size)
{
    DWC_Free(DWC_ALLOCTYPE_AUTH, ptr, size);
}
