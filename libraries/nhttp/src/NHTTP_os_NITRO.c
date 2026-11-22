#include "NHTTP_os.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_thread.h"

static OSMessage g_CommThreadMessageArray[1];
static OSMutex g_MutexIdList;
static OSMessageQueue g_CommThreadMessageQueue;
static OSThread g_CommThread;

extern volatile BOOL NHTTPi_isThreadEnd;

BOOL NHTTPi_initLockReqList(void)
{
    OS_InitMutex(&g_MutexIdList);
    return TRUE;
}

void NHTTPi_exitLockReqList(void)
{
    return;
}

void NHTTPi_lockReqList(void)
{
    OS_LockMutex(&g_MutexIdList);
}

void NHTTPi_unlockReqList(void)
{
    OS_UnlockMutex(&g_MutexIdList);
}

BOOL NHTTPi_createCommThread(u32 priority, u32 *stack)
{
    if (!OS_IsThreadAvailable()) {
        return FALSE;
    }

    OS_InitMessageQueue(&g_CommThreadMessageQueue, g_CommThreadMessageArray, 1);
    OS_CreateThread(&g_CommThread, NHTTPi_CommThreadProc, NULL, &stack[NHTTP_COMMTHREAD_STACK / sizeof(u32)], NHTTP_COMMTHREAD_STACK, priority);
    OS_WakeupThreadDirect(&g_CommThread);
    return TRUE;
}

void NHTTPi_destroyCommThread(void)
{
    NHTTPi_isThreadEnd = TRUE;
    NHTTPi_kickCommThread();
    OS_JoinThread(&g_CommThread);
}

void NHTTPi_idleCommThread(void)
{
    OSMessage msg;
    OS_ReceiveMessage(&g_CommThreadMessageQueue, &msg, OS_MESSAGE_BLOCK);
}

void NHTTPi_kickCommThread(void)
{
    OS_SendMessage(&g_CommThreadMessageQueue, NULL, OS_MESSAGE_NOBLOCK);
}
