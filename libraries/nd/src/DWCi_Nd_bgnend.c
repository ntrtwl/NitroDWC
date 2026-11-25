#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_DWC.h"
#include "DWCi_Nd_attr.h"
#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_os.h"
#include "DWCi_Nd_stdlib.h"

DWCNdAlloc DWCi_Ndi_alloc;
char *DWCi_Ndi_authtoken;
char *DWCi_Ndi_serverURL;
DWCNdCallback DWCi_Ndi_dlcallback;
int DWCi_Ndi_nhttpid;
static DWCNdCleanupCallback cleanupCallback;
BOOL DWCi_Ndi_isExistApinfo;
BOOL DWCi_Ndi_isOpened;
DWCNdError DWCi_Ndi_error;
DWCNdFree DWCi_Ndi_free;
char *DWCi_Ndi_dlattr[3];
char DWCi_Ndi_dwcgamecd[9];
char DWCi_Ndi_macaddr[17];
char DWCi_Ndi_strApinfo[21];
char DWCi_Ndi_password[25];
char DWCi_Ndi_userAgent[33];

static char *strAlloc(char *src)
{
    int n = DWCi_Ndi_strlen(src);
    char *dst = DWCi_Ndi_alloc(n + 1, 4);
    if (dst != NULL) {
        DWCi_Ndi_memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return dst;
}

BOOL DWCi_NdStartup(DWCNdAlloc alloc, DWCNdFree free, char *url, char *token, char *gamecd, char *passwd, DWCNdCallback callback)
{
    DWCi_Ndi_alloc = alloc;
    DWCi_Ndi_free = free;
    DWCi_Ndi_dlcallback = callback;
    DWCi_Ndi_error = DWC_ND_ERROR_NONE;
    DWCi_Ndi_dlattr[0] = NULL;
    DWCi_Ndi_dlattr[1] = NULL;
    DWCi_Ndi_dlattr[2] = NULL;
    DWCi_Ndi_isExistApinfo = FALSE;
    DWCi_Ndi_nhttpid = -1;
    DWCi_Ndi_authtoken = NULL;
    DWCi_Ndi_serverURL = NULL;
    DWCi_Ndi_getMacAddress(DWCi_Ndi_macaddr);
    DWCi_Ndi_getUserAgent(DWCi_Ndi_userAgent);

    DWCi_Ndi_serverURL = strAlloc(url);
    if (DWCi_Ndi_serverURL != NULL) {
        DWCi_Ndi_authtoken = strAlloc(token);
        if (DWCi_Ndi_authtoken != NULL) {
            DWCi_Ndi_dwcgamecd[DWCi_Ndi_encodeBASE64(DWCi_Ndi_dwcgamecd, gamecd, DWC_ND_LENGTH_GAMECODE)] = '\0';
            DWCi_Ndi_password[DWCi_Ndi_encodeBASE64(DWCi_Ndi_password, passwd, DWC_ND_LENGTH_PASSWORD)] = '\0';

            if (DWCi_Ndi_initHttpCallback() && DWCi_Ndi_initLockGlobal()) {
                if (NHTTP_Startup(alloc, free, DWC_ND_NHTTP_THREAD_PRIORITY)) {
                    DWCi_Ndi_isExistApinfo = DWCi_Ndi_getApInfo(DWCi_Ndi_strApinfo);
                    DWCi_Ndi_isOpened = TRUE;
                    return TRUE;
                }
                DWCi_Ndi_exitLockGlobal();
            }

            DWCi_Ndi_free(DWCi_Ndi_authtoken);
            DWCi_Ndi_free(DWCi_Ndi_serverURL);
            DWCi_Ndi_error = DWC_ND_ERROR_FATAL;
            return FALSE;
        }
    }

    if (DWCi_Ndi_authtoken != NULL) {
        DWCi_Ndi_free(DWCi_Ndi_authtoken);
    }
    if (DWCi_Ndi_serverURL != NULL) {
        DWCi_Ndi_free(DWCi_Ndi_serverURL);
    }

    DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;

    return FALSE;
}

static void nhttpCleanupCallback(void)
{
    DWCi_Ndi_freeAttr();
    DWCi_Ndi_exitLockGlobal();
    DWCi_Ndi_exitHttpCallback();
    DWCi_Ndi_free(DWCi_Ndi_authtoken);
    DWCi_Ndi_free(DWCi_Ndi_serverURL);
    cleanupCallback();
}

void DWCi_NdCleanupAsync(DWCNdCleanupCallback callback)
{
    DWCi_Ndi_isOpened = FALSE;
    cleanupCallback = callback;
    NHTTP_CleanupAsync(nhttpCleanupCallback);
}

DWCNdError DWCi_NdGetError(void)
{
    return DWCi_Ndi_error;
}
