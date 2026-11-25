#include "DWCi_Nd_os.h"

#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_lib.h"
#include "DWCi_Nd_string.h"

static BOOL g_HTTPComm;
static OSMutex g_MutexAttr;
static OSMutex g_MutexHttp;

extern DWCNdAlloc DWCi_Ndi_alloc;
extern DWCNdFree DWCi_Ndi_free;

BOOL DWCi_Ndi_initLockGlobal(void)
{
    OS_InitMutex(&g_MutexAttr);
    return TRUE;
}

void DWCi_Ndi_exitLockGlobal(void)
{
    return;
}

void DWCi_Ndi_lockGlobal(void)
{
    OS_LockMutex(&g_MutexAttr);
}

void DWCi_Ndi_unlockGlobal(void)
{
    OS_UnlockMutex(&g_MutexAttr);
}

BOOL DWCi_Ndi_initHttpCallback(void)
{
    OS_InitMutex(&g_MutexHttp);
    g_HTTPComm = FALSE;
    return TRUE;
}

void DWCi_Ndi_exitHttpCallback(void)
{
    return;
}

BOOL DWCi_Ndi_prepareHttpCallback(void)
{
    OS_LockMutex(&g_MutexHttp);
    BOOL rc = !g_HTTPComm;
    if (rc) {
        g_HTTPComm = TRUE;
    }
    OS_UnlockMutex(&g_MutexHttp);

    return rc;
}

void DWCi_Ndi_unprepareHttpCallback(void)
{
    OS_LockMutex(&g_MutexHttp);
    g_HTTPComm = FALSE;
    OS_UnlockMutex(&g_MutexHttp);
}

THREADEVENTDESC *DWCi_Ndi_createThreadEvent(void)
{
    THREADEVENTDESC *p = DWCi_Nd_Alloc(sizeof(THREADEVENTDESC), 8);
    OS_InitMessageQueue(&p->queue, p->msg, 1);
    return p;
}

void DWCi_Ndi_destroyThreadEvent(THREADEVENTDESC *threadEvent)
{
    DWCi_Ndi_free(threadEvent);
}

void DWCi_Ndi_kickThread(THREADEVENTDESC *threadEvent)
{
    OS_SendMessage(&threadEvent->queue, NULL, OS_MESSAGE_NOBLOCK);
}

void DWCi_Ndi_idleThread(THREADEVENTDESC *threadEvent)
{
    OSMessage msg;
    OS_ReceiveMessage(&threadEvent->queue, &msg, OS_MESSAGE_BLOCK);
}

void DWCi_Ndi_getMacAddress(char *macaddr)
{
    u8 maddr[WM_SIZE_MACADDR];
    char strmaddr[12];

    OS_GetMacAddress(maddr);

    for (int i = 0; i < WM_SIZE_MACADDR; i++) {
        DWCi_Ndi_hexToStrFigure(&strmaddr[i * 2], maddr[i], 2);
    }

    DWCi_Ndi_encodeBASE64(macaddr, strmaddr, 12);
}
