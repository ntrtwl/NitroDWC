#ifndef NHTTP_REQUEST_H
#define NHTTP_REQUEST_H

#include "nhttp/NHTTP.h"

BOOL NHTTPi_destroyRequestObject(NHTTPReq *req);
void NHTTPi_cancelAllRequests(void);

#endif // NHTTP_REQUEST_H
