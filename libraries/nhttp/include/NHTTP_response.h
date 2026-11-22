#ifndef NHTTP_RESPONSE_H
#define NHTTP_RESPONSE_H

#include "nhttp/NHTTP.h"

int NHTTPi_getHeaderValue(NHTTPRes *res, char *label, int *ppos);

#endif // NHTTP_RESPONSE_H
