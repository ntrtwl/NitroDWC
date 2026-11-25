#ifndef ND_OS
#define ND_OS

#include <nitro.h>

#include "nd/DWCi_Nd.h"

typedef struct THREADEVENTDESC {
    OSMessageQueue queue;
    OSMessage msg[1];
} THREADEVENTDESC;

BOOL DWCi_Ndi_initLockGlobal(void);
void DWCi_Ndi_exitLockGlobal(void);
void DWCi_Ndi_lockGlobal(void);
void DWCi_Ndi_unlockGlobal(void);
BOOL DWCi_Ndi_initHttpCallback(void);
void DWCi_Ndi_exitHttpCallback(void);
BOOL DWCi_Ndi_prepareHttpCallback(void);
void DWCi_Ndi_unprepareHttpCallback(void);
THREADEVENTDESC *DWCi_Ndi_createThreadEvent(void);
void DWCi_Ndi_destroyThreadEvent(THREADEVENTDESC *threadEvent);
void DWCi_Ndi_kickThread(THREADEVENTDESC *threadEvent);
void DWCi_Ndi_idleThread(THREADEVENTDESC *threadEvent);
void DWCi_Ndi_getMacAddress(char *macaddr);

#endif // ND_OS
