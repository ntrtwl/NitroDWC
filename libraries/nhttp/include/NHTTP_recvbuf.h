#ifndef NHTTP_RECVBUF_H
#define NHTTP_RECVBUF_H

#include "nhttp/NHTTP.h"

int NHTTPi_findNextLineHdrRecvBuf(NHTTPRes *res, int pos, int limit, int *pcolon);
int NHTTPi_skipSpaceHdrRecvBuf(NHTTPRes *res, int pos, int limit);
int NHTTPi_compareTokenN_HdrRecvBuf(NHTTPRes *res, int pos, int limit, char *str, char extdeli);
BOOL NHTTPi_loadFromHdrRecvBuf(NHTTPRes *res, char *dst, int pos, int len);
BOOL NHTTPi_isRecvBufFull(NHTTPRes *res, int pos);
int NHTTPi_RecvBuf(NHTTPReq *req, int s, int pos, int flags);
int NHTTPi_RecvBufN(NHTTPReq *req, int s, int pos, int len, int flags);

#endif // NHTTP_RECVBUF_H
