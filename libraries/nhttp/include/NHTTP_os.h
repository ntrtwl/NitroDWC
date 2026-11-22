#ifndef NHTTP_OS_H
#define NHTTP_OS_H

#include "nhttp/NHTTP.h"

BOOL NHTTPi_initLockReqList(void);
void NHTTPi_exitLockReqList(void);
void NHTTPi_lockReqList(void);
void NHTTPi_unlockReqList(void);
BOOL NHTTPi_createCommThread(u32 priority, u32 *stack);
void NHTTPi_destroyCommThread(void);
void NHTTPi_idleCommThread(void);
void NHTTPi_kickCommThread(void);

#endif // NHTTP_OS_H
