#ifndef NHTTP_SOCKET_H
#define NHTTP_SOCKET_H

#include "nhttp/NHTTP.h"

void NHTTPi_SetSSLSeed(void);
int NHTTPi_SocOpen(NHTTPReq *req);
int NHTTPi_SocClose(NHTTPReq *req, int s);
int NHTTPi_SocCloseWait(NHTTPReq *req, int s);
int NHTTPi_SocConnect(NHTTPReq *req, int s, u32 hostaddr, int port);
int NHTTPi_SocRecv(NHTTPReq *req, int s, char *buf, int len, int flags);
int NHTTPi_SocSend(NHTTPReq *req, int s, char *buf, int len, int flags);
void NHTTPi_SocCancel(NHTTPReq *req, int s);

#endif // NHTTP_SOCKET_H
