#include "NHTTP_request.h"

#include <limits.h>
#include <nitro.h>
#include <nitroWiFi.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"
#include "NHTTP_list.h"
#include "NHTTP_os.h"
#include "NHTTP_socket.h"
#include "NHTTP_stdlib.h"
#include "NHTTP_token.h"

const char NHTTPi_strMultipartBound[] = "--t9Sf4yfjf1RtvDu3AA";

extern volatile NHTTPError NHTTPi_error;
extern NHTTPAlloc NHTTPi_alloc;
extern NHTTPFree NHTTPi_free;
extern volatile NHTTPi_REQLIST *NHTTPi_reqCurrent;

NHTTPReq *NHTTP_CreateRequest(char *url, NHTTPReqMethod method, char *buf, u32 len, NHTTPReqCallback callback, void *param)
{
    int i;
    int suflen, urllen, prelen;
    int nhostenc, deccount;
    int declen, hostend;

    char *prefix;
    NHTTPReq *req = NULL;

    switch (method) {
    case NHTTP_REQMETHOD_GET:
    case NHTTP_REQMETHOD_POST:
    case NHTTP_REQMETHOD_HEAD:
        break;
    default:
        NHTTPi_error = NHTTP_ERROR_UNKNOWN;
        goto err;
    }

    if (len == 0) {
        NHTTPi_error = NHTTP_ERROR_UNKNOWN;
        goto err;
    }

    req = NHTTPi_alloc(sizeof(NHTTPReq), 4);

    if (req == NULL) {
        NHTTPi_error = NHTTP_ERROR_ALLOC;
        goto err;
    }

    NHTTPi_memclr(req, sizeof(NHTTPReq));

    req->response = NHTTPi_alloc(sizeof(NHTTPRes), 4);

    if (req->response == NULL) {
        NHTTPi_error = NHTTP_ERROR_ALLOC;
        goto err;
    }

    NHTTPi_memclr(req->response, sizeof(NHTTPRes));
    req->response->pRecvBuf = buf;
    req->response->nLenRecvBuf = len;

    urllen = NHTTPi_strlen(url);

    if (urllen <= 7) {
        NHTTPi_error = NHTTP_ERROR_DNS;
        goto err;
    }

    req->port = NHTTP_HTTP_PORT;
    suflen = 7;

    if (NHTTPi_strnicmp(url, "http://", 7) != 0) {
        if (NHTTPi_strnicmp(url, "https://", 8) != 0) {
            NHTTPi_error = NHTTP_ERROR_DNS;
            goto err;
        }
        req->isSSL = TRUE;
        req->port = NHTTP_HTTPS_PORT;
        suflen = 8;
    }

    prefix = url + suflen;
    prelen = urllen - suflen;

    if (prelen <= 0) {
        NHTTPi_error = NHTTP_ERROR_DNS;
        goto err;
    }

    for (i = 0, nhostenc = 0, deccount = 0; i < prelen && prefix[i] != '/'; i++) {
        if (deccount == 2) {
            deccount--;
        } else if (deccount == 1) {
            char c = NHTTPi_strToHex(&prefix[i - 1], 2);
            deccount--;
            if (c < 0) {
                NHTTPi_error = NHTTP_ERROR_DNS;
                goto err;
            }
            if (c == '/') {
                nhostenc--;
                break;
            }
        } else {
            if (prefix[i] == '%') {
                deccount = 2;
                nhostenc++;
            }
        }
    }

    if (deccount != 0) {
        NHTTPi_error = NHTTP_ERROR_DNS;
        goto err;
    }

    req->pURL = NHTTPi_alloc(suflen + prelen - nhostenc * 2 + 1, 4);

    if (req->pURL == NULL) {
        NHTTPi_error = NHTTP_ERROR_ALLOC;
        goto err;
    }
    NHTTPi_memcpy(req->pURL, url, suflen);

    for (i = 0, declen = 0, deccount = 0, hostend = FALSE; i < prelen; i++) {
        if (deccount == 2) {
            deccount--;
        } else if (deccount == 1) {
            char c = NHTTPi_strToHex(&prefix[i - 1], 2);
            deccount--;
            req->pURL[suflen + declen - 1] = c;
            if (c == '/') {
                hostend = TRUE;
            }
        } else {
            if (prefix[i] == '/') {
                hostend = TRUE;
            }
            if (!hostend && prefix[i] == '%') {
                deccount = 2;
            } else {
                req->pURL[suflen + declen] = prefix[i];
            }
            declen++;
        }
    }

    req->pURL[suflen + declen] = '\0';

    prefix = &req->pURL[suflen];
    prelen = declen;

    for (i = 0; i < prelen; i++) {
        if (prefix[i] == '/' || prefix[i] == ':') {
            req->nLenHost = i + suflen;
            break;
        }
    }

    if (i == prelen) {
        req->nLenHostDesc = req->nLenHost = i + suflen;
    } else {
        if (prefix[i] == '/') {
            req->nLenHostDesc = req->nLenHost;
        } else if (prefix[i] == ':') {
            for (; i < prelen; i++) {
                if (prefix[i] == '/') {
                    req->nLenHostDesc = i + suflen;
                    break;
                }
            }

            if (i == prelen) {
                req->nLenHostDesc = i + suflen;
            } else {
                int port = NHTTPi_strtonum(&req->pURL[req->nLenHost + 1], req->nLenHostDesc - (req->nLenHost + 1));

                if (port < 0) {
                    port = req->port;
                } else if (port > USHRT_MAX) {
                    NHTTPi_error = NHTTP_ERROR_DNS;
                    goto err;
                }

                req->port = port & USHRT_MAX;
            }
        }
    }

    if (req->isSSL) {
        int hostlen = req->nLenHost - suflen;

        req->sslConn = NHTTPi_alloc(sizeof(SOCSslConnection), 4);
        if (req->sslConn == NULL) {
            NHTTPi_error = NHTTP_ERROR_ALLOC;
            goto err;
        }

        NHTTPi_memclr(req->sslConn, sizeof(SOCSslConnection));
        req->sslConn->server_name = NHTTPi_alloc(hostlen + 1, 4);
        if (req->sslConn->server_name == NULL) {
            NHTTPi_error = NHTTP_ERROR_ALLOC;
            goto err;
        }

        NHTTPi_memcpy(req->sslConn->server_name, req->pURL + suflen, hostlen);
        req->sslConn->server_name[hostlen] = '\0';
    }

    NHTTPi_memcpy(req->tagPost, NHTTPi_strMultipartBound, sizeof(req->tagPost));
    req->param = param;
    req->method = method;
    req->pCallback = callback;

    return req;

err:
    if (req != NULL) {
        if (req->sslConn != NULL) {
            if (req->sslConn->server_name != NULL) {
                NHTTPi_free(req->sslConn->server_name);
            }
            NHTTPi_free(req->sslConn);
        }

        if (req->pURL != NULL) {
            NHTTPi_free(req->pURL);
        }

        if (req->response != NULL) {
            NHTTPi_free(req->response);
        }

        NHTTPi_free(req);
    }

    return NULL;
}

void NHTTP_DestroyRequest(NHTTPReq *req)
{
    NHTTPi_free(req->response);
    NHTTPi_destroyRequestObject(req);
}

static void deleteStrList(NHTTPi_DATALIST *plst)
{
    while (plst != NULL) {
        if (plst != plst->prev) {
            NHTTPi_DATALIST *p = plst->prev->prev;
            NHTTPi_free(plst->prev);
            plst->prev = p;
        } else {
            NHTTPi_free(plst);
            plst = NULL;
        }
    }
}

BOOL NHTTPi_destroyRequestObject(NHTTPReq *req)
{
    deleteStrList(req->pListHeader);
    deleteStrList(req->pListPost);

    if (req->sslConn != NULL) {
        if (req->sslConn->server_name != NULL) {
            NHTTPi_free(req->sslConn->server_name);
        }
        NHTTPi_free(req->sslConn);
    }

    NHTTPi_free(req->pURL);
    NHTTPi_free(req);
    return TRUE;
}

int NHTTP_SendRequestAsync(NHTTPReq *req)
{
    if (req->isStarted) {
        NHTTPi_error = NHTTP_ERROR_UNKNOWN;
        return -1;
    }
    if (req->method == NHTTP_REQMETHOD_POST && req->pListPost == NULL) {
        NHTTPi_error = NHTTP_ERROR_UNKNOWN;
        return -1;
    }

    NHTTPi_lockReqList();
    int id = NHTTPi_setReqQueue(req);
    if (id >= 0) {
        req->isStarted = TRUE;
        NHTTPi_kickCommThread();
    } else {
        NHTTPi_error = NHTTP_ERROR_ALLOC;
    }
    NHTTPi_unlockReqList();

    return id;
}

BOOL NHTTP_CancelRequestAsync(int id)
{
    BOOL rc = FALSE;

    NHTTPi_lockReqList();
    if (NHTTPi_reqCurrent != NULL && NHTTPi_reqCurrent->id == id && !NHTTPi_reqCurrent->req->doCancel) {
        NHTTPi_reqCurrent->req->doCancel = TRUE;
        NHTTPi_SocCancel(NHTTPi_reqCurrent->req, NHTTPi_reqCurrent->socket);
        rc = TRUE;
    }
    if (!rc) {
        rc = NHTTPi_freeReqQueue(id);
    }
    NHTTPi_unlockReqList();

    return rc;
}

void NHTTPi_cancelAllRequests(void)
{
    NHTTPi_lockReqList();
    if (NHTTPi_reqCurrent != NULL && !NHTTPi_reqCurrent->req->doCancel) {
        NHTTPi_reqCurrent->req->doCancel = TRUE;
        NHTTPi_SocCancel(NHTTPi_reqCurrent->req, NHTTPi_reqCurrent->socket);
    }
    NHTTPi_allFreeReqQueue();
    NHTTPi_unlockReqList();
}
