#ifndef NHTTP_LIST_H
#define NHTTP_LIST_H

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"

int NHTTPi_setReqQueue(NHTTPReq *req);
NHTTPi_REQLIST *NHTTPi_findReqQueue(int id);
BOOL NHTTPi_freeReqQueue(int id);
void NHTTPi_allFreeReqQueue(void);
NHTTPi_REQLIST *NHTTPi_getReqFromReqQueue(void);

#endif // NHTTP_LIST_H
