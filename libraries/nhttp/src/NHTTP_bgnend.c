#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"
#include "NHTTP_os.h"
#include "NHTTP_request.h"

NHTTPFree NHTTPi_free;
volatile NHTTPi_REQLIST *NHTTPi_reqQueue;
volatile NHTTPError NHTTPi_error;
NHTTPAlloc NHTTPi_alloc;
BOOL NHTTPi_isOpened;
volatile NHTTPi_REQLIST *NHTTPi_reqCurrent;
volatile BOOL NHTTPi_isThreadEnd;
static u32 *g_CommThreadStack;
volatile int NHTTPi_idRequest;
static OSThread g_CleanupThread;
static u32 g_CleanupThreadStack[2048 / sizeof(u32)];

static void NHTTPi_CleanupThreadProc(void *callback);

BOOL NHTTP_Startup(NHTTPAlloc alloc, NHTTPFree free, u32 threadprio)
{
    NHTTPi_alloc = alloc;
    NHTTPi_free = free;
    NHTTPi_error = NHTTP_ERROR_NONE;
    NHTTPi_idRequest = 0;
    NHTTPi_reqQueue = NULL;
    NHTTPi_reqCurrent = NULL;
    NHTTPi_isThreadEnd = FALSE;

    if (!NHTTPi_initLockReqList()) {
        NHTTPi_error = NHTTP_ERROR_NITROSDK;
        return FALSE;
    }

    g_CommThreadStack = NHTTPi_alloc(NHTTP_COMMTHREAD_STACK, 8);
    if (g_CommThreadStack == NULL) {
        NHTTPi_error = NHTTP_ERROR_ALLOC;
        NHTTPi_exitLockReqList();
        return FALSE;
    }

    if (!NHTTPi_createCommThread(threadprio, g_CommThreadStack)) {
        NHTTPi_error = NHTTP_ERROR_NITROSDK;
        NHTTPi_free(g_CommThreadStack);
        NHTTPi_exitLockReqList();
        return FALSE;
    } else {
        NHTTPi_isOpened = TRUE;
        return TRUE;
    }
}

void NHTTP_CleanupAsync(NHTTPCleanupCallback callback)
{
    OS_CreateThread(&g_CleanupThread, NHTTPi_CleanupThreadProc, callback, &g_CleanupThreadStack[sizeof(g_CleanupThreadStack) / sizeof(u32)], sizeof(g_CleanupThreadStack), 16);
    OS_WakeupThreadDirect(&g_CleanupThread);
}

NHTTPError NHTTP_GetError(void)
{
    return NHTTPi_error;
}

static void NHTTPi_CleanupThreadProc(void *callback)
{
    NHTTPi_cancelAllRequests();
    NHTTPi_destroyCommThread();
    NHTTPi_free(g_CommThreadStack);
    NHTTPi_exitLockReqList();
    NHTTPi_isOpened = FALSE;
    ((NHTTPCleanupCallback)callback)();
}
