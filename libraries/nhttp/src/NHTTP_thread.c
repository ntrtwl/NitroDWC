#include "NHTTP_thread.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_control.h"
#include "NHTTP_lib.h"
#include "NHTTP_list.h"
#include "NHTTP_os.h"
#include "NHTTP_recvbuf.h"
#include "NHTTP_request.h"
#include "NHTTP_resolve.h"
#include "NHTTP_response.h"
#include "NHTTP_socket.h"
#include "NHTTP_stdlib.h"
#include "NHTTP_token.h"

typedef struct SocketInfo {
    int s;
    SOCSslConnection *con;
} SocketInfo;

#define COMMBUF_LEN 1024

static int sdClosingQueue_Rp;
static int sdClosingQueue_Wp;
static SocketInfo sdClosingQueue[8];
static char g_commBuf[COMMBUF_LEN];

static const char strPostDispos[] = "Content-Disposition: form-data; name=\"";
static const char strPostTypeBin[] = "Content-Type: application/octet-stream\r\nContent-Transfer-Encoding: binary\r\n";

extern volatile NHTTPError NHTTPi_error;
extern NHTTPAlloc NHTTPi_alloc;
extern NHTTPFree NHTTPi_free;
extern volatile NHTTPi_REQLIST *NHTTPi_reqCurrent;
extern volatile BOOL NHTTPi_isThreadEnd;
extern volatile NHTTPi_REQLIST *NHTTPi_reqQueue;

BOOL NHTTP_GetProgress(u32 *received, u32 *contentlen)
{
    BOOL rc = FALSE;

    *received = 0;
    *contentlen = 0;

    NHTTPi_lockReqList();
    if (NHTTPi_reqCurrent != NULL) {
        if (NHTTPi_reqCurrent->req->nContentLength != 0) {
            *received = NHTTPi_reqCurrent->req->response->nLenBody;
            *contentlen = NHTTPi_reqCurrent->req->nContentLength == -1 ? 0 : NHTTPi_reqCurrent->req->nContentLength;
        }
        rc = TRUE;
    } else {
        rc = NHTTPi_reqQueue != NULL;
    }
    NHTTPi_unlockReqList();

    return rc;
}

static int saveBuf(NHTTPReq *req, int s, int *pbufwp, char *src, int srclen)
{
    for (int len = srclen; len > 0;) {
        if (req->doCancel) {
            return -1;
        }

        int n = COMMBUF_LEN - *pbufwp;
        n = NHTTP_MIN(len, n);
        NHTTPi_memcpy(&g_commBuf[*pbufwp], src, n);
        src += n;
        *pbufwp += n;
        len -= n;

        if (*pbufwp == COMMBUF_LEN) {
            int sendbyte = NHTTPi_SocSend(req, s, g_commBuf, COMMBUF_LEN, 0);
            if (sendbyte <= 0) {
                return sendbyte;
            }
            *pbufwp -= sendbyte;
        }
    }

    return srclen;
}

static int skipLineBuf(NHTTPReq *req, int s)
{
    char buf[2];
    int p;
    int rc = 0;

    buf[0] = '\0';
    buf[1] = '\0';

    for (p = 0; !(buf[p & 1] == '\r' && buf[(p - 1) & 1] == '\n'); p++) {
        int recvbyte = NHTTPi_SocRecv(req, s, &buf[p & 1], 1, 0);
        if (recvbyte <= 0) {
            return recvbyte;
        }
        rc += recvbyte;
    }

    return rc;
}

static void freeSslConnection(SOCSslConnection *con)
{
    if (con != NULL) {
        if (con->server_name != NULL) {
            NHTTPi_free(con->server_name);
        }
        NHTTPi_free(con);
    }
}

static void putSocketToCloseQueue(int s, SOCSslConnection **conp)
{
    int nextWp = (sdClosingQueue_Wp + 1) & 7;
    if (nextWp == sdClosingQueue_Rp) {
        NHTTPi_SocCloseWait(NULL, sdClosingQueue[sdClosingQueue_Rp].s);
        freeSslConnection(sdClosingQueue[sdClosingQueue_Rp].con);
        sdClosingQueue_Rp = (sdClosingQueue_Rp + 1) & 7;
    }

    if (*conp != NULL) {
        sdClosingQueue[sdClosingQueue_Wp].s = s;
        sdClosingQueue[sdClosingQueue_Wp].con = *conp;
        *conp = NULL;
        sdClosingQueue_Wp = nextWp;
    }
}

static void clearCloseQueue(void)
{
    while (sdClosingQueue_Wp != sdClosingQueue_Rp) {
        NHTTPi_SocCloseWait(NULL, sdClosingQueue[sdClosingQueue_Rp].s);
        freeSslConnection(sdClosingQueue[sdClosingQueue_Rp].con);
        sdClosingQueue_Rp = (sdClosingQueue_Rp + 1) & 7;
    }
}

#define TRY_SAVE_BUF(src, srclen)                                   \
    {                                                               \
        int sdDatalength = srclen;                                  \
        if (sdDatalength) {                                         \
            int stat = saveBuf(req, sd, &bufwp, src, sdDatalength); \
            if (stat < 0) {                                         \
                goto done;                                          \
            }                                                       \
            if (stat == 0) {                                        \
                goto retry;                                         \
            }                                                       \
        }                                                           \
    }

void NHTTPi_CommThreadProc(void *param)
{
    NHTTPError result;
    int sd = -1;
    SOCSslConnection *sslConnection = NULL;
    u32 hostaddr, prev_hostaddr = -1;
    int bufwp;
    int n;
    int lenurl;
    int recvbyte;
    int id;
    int labelend;
    int httpStatus;
    int contentLength;
    int prev_port = -1;
    BOOL isMultipartForm = FALSE;
    BOOL isKeepAlive = FALSE;
    BOOL isChunked = FALSE;
    BOOL prev_isSSL = FALSE;
    NHTTPReq *req;
    NHTTPRes *response;
    NHTTPi_HDRBUFLIST *hdrbuf;
    NHTTPi_DATALIST *datalst;
    char tmpBuf[12];

    sdClosingQueue_Wp = 0;
    sdClosingQueue_Rp = 0;

    while (!NHTTPi_isThreadEnd) {
        NHTTPi_lockReqList();
        NHTTPi_REQLIST *lst = NHTTPi_getReqFromReqQueue();
        if (lst != NULL) {
            id = lst->id;
            req = lst->req;
            NHTTPi_reqCurrent = lst;
        } else {
            id = -1;
        }
        NHTTPi_unlockReqList();

        if (id < 0) {
            NHTTPi_idleCommThread();
            continue;
        }

        response = req->response;
        if (req->doCancel) {
            goto done;
        }

        hostaddr = NHTTPi_resolveHostname(req);
        if (hostaddr == 0) {
            result = NHTTP_ERROR_DNS;
            goto done;
        }

        isKeepAlive &= hostaddr == prev_hostaddr && req->port == prev_port && req->isSSL == prev_isSSL;
        prev_hostaddr = hostaddr;
        prev_port = req->port;
        prev_isSSL = req->isSSL;

retry:
        result = NHTTP_ERROR_NONE;
        if (sd >= 0) {
            SOCPollFD pollfd;
            pollfd.fd = sd;
            pollfd.events = SOC_POLLRDNORM | SOC_POLLWRNORM;
            if (SOC_Poll(&pollfd, 1, OS_MilliSecondsToTicks(100)) <= 0) {
                isKeepAlive = FALSE;
            } else if (pollfd.revents & (SOC_POLLERR | SOC_POLLHUP | SOC_POLLNVAL)) {
                isKeepAlive = FALSE;
            }
        }

        if (!isKeepAlive) {
            if (sd >= 0) {
                if (NHTTPi_SocClose(req, sd) < 0) {
                    result = NHTTP_ERROR_NITROWIFI;
                }
                putSocketToCloseQueue(sd, &sslConnection);
                sd = -1;
                if (result != NHTTP_ERROR_NONE) {
                    goto done;
                }
            }

            sd = NHTTPi_SocOpen(req);
            if (sd < 0) {
                result = NHTTP_ERROR_SOCKET;
                goto done;
            }

            if (req->isSSL) {
                sslConnection = req->sslConn;
                req->sslConn = NULL;
            }

            NHTTPi_lockReqList();
            NHTTPi_reqCurrent->socket = sd;
            NHTTPi_unlockReqList();

            if (req->doCancel) {
                goto done;
            }

            isKeepAlive = NHTTPi_SocConnect(req, sd, hostaddr, req->port) >= 0;
        } else {
            if (req->isSSL) {
                freeSslConnection(req->sslConn);
                req->sslConn = NULL;
            }
            NHTTPi_SetSSLSeed();

            NHTTPi_lockReqList();
            NHTTPi_reqCurrent->socket = sd;
            NHTTPi_unlockReqList();
        }

        if (req->doCancel) {
            goto done;
        }

        if (!isKeepAlive) {
            result = NHTTP_ERROR_CONNECT;
            goto done;
        }

        isKeepAlive = FALSE;
        bufwp = 0;
        lenurl = NHTTPi_strlen(req->pURL);
        result = NHTTP_ERROR_NITROWIFI;

        switch (req->method) {
        case NHTTP_REQMETHOD_GET:
            TRY_SAVE_BUF("GET ", 4);
            break;
        case NHTTP_REQMETHOD_POST:
            TRY_SAVE_BUF("POST ", 5);
            break;
        case NHTTP_REQMETHOD_HEAD:
            TRY_SAVE_BUF("HEAD ", 5);
            break;
        default:
            break;
        }

        if (lenurl > req->nLenHostDesc) {
            TRY_SAVE_BUF(req->pURL + req->nLenHostDesc, lenurl - req->nLenHostDesc);
        } else {
            TRY_SAVE_BUF("/", 1);
        }
        TRY_SAVE_BUF(" HTTP/" NHTTP_STR_HTTPVER "\r\n", 11);

        n = req->isSSL ? 8 : 7;
        TRY_SAVE_BUF("Host: ", 6);
        TRY_SAVE_BUF(&req->pURL[n], req->nLenHost - n);
        TRY_SAVE_BUF("\r\n", 2);

        datalst = NHTTPi_getHdrFromList(&req->pListHeader);
        while (datalst != NULL) {
            TRY_SAVE_BUF(datalst->label, NHTTPi_strlen(datalst->label));
            TRY_SAVE_BUF(": ", 2);
            TRY_SAVE_BUF(datalst->value, NHTTPi_strlen(datalst->value));
            TRY_SAVE_BUF("\r\n", 2);
            NHTTPi_free(datalst);
            datalst = NHTTPi_getHdrFromList(&req->pListHeader);
        }

        if (req->method == NHTTP_REQMETHOD_POST) {
            contentLength = 0;

            for (isMultipartForm = FALSE, datalst = req->pListPost; datalst != NULL; datalst = datalst->next) {
                if (datalst->isBinary) {
                    isMultipartForm = TRUE;
                    break;
                }
                if (datalst == req->pListPost->prev) {
                    break;
                }
            }

            if (isMultipartForm) {
                for (datalst = req->pListPost; datalst != NULL; datalst = datalst->next) {
                    contentLength += sizeof(req->tagPost) + 2;
                    contentLength += (sizeof(strPostDispos) - 1) + NHTTPi_strlen(datalst->label) + 3;
                    if (datalst->isBinary) {
                        contentLength += sizeof(strPostTypeBin) - 1;
                    }
                    contentLength += 2;
                    contentLength += datalst->length;
                    contentLength += 2;
                    if (datalst == req->pListPost->prev) {
                        break;
                    }
                }

                contentLength += sizeof(req->tagPost) + 4;

                TRY_SAVE_BUF("Content-Type: multipart/form-data; boundary=", 44);
                TRY_SAVE_BUF(&req->tagPost[2], MULTIPART_BOUNDARY_LEN);
                TRY_SAVE_BUF("\r\n", 2);
            } else {
                for (datalst = req->pListPost; datalst != NULL; datalst = datalst->next) {
                    contentLength += NHTTPi_getUrlEncodedSize(datalst->label);
                    contentLength++;
                    contentLength += NHTTPi_getUrlEncodedSize(datalst->value);
                    if (datalst == req->pListPost->prev) {
                        break;
                    }
                    contentLength++;
                }

                TRY_SAVE_BUF("Content-Type: application/x-www-form-urlencoded\r\n", 49);
            }

            TRY_SAVE_BUF("Content-Length: ", 16);
            n = NHTTPi_intToStr(tmpBuf, contentLength);
            TRY_SAVE_BUF(tmpBuf, n);
            TRY_SAVE_BUF("\r\n", 2);
        }

        TRY_SAVE_BUF("\r\n", 2);

        if (req->method == NHTTP_REQMETHOD_POST) {
            if (isMultipartForm) {
                for (datalst = req->pListPost; datalst != NULL; datalst = datalst->next) {
                    TRY_SAVE_BUF(req->tagPost, sizeof(req->tagPost));
                    TRY_SAVE_BUF("\r\n", 2);
                    TRY_SAVE_BUF(strPostDispos, sizeof(strPostDispos) - 1);
                    TRY_SAVE_BUF(datalst->label, NHTTPi_strlen(datalst->label));
                    TRY_SAVE_BUF("\"\r\n", 3);

                    if (datalst->isBinary) {
                        TRY_SAVE_BUF(strPostTypeBin, sizeof(strPostTypeBin) - 1);
                    }
                    TRY_SAVE_BUF("\r\n", 2);
                    TRY_SAVE_BUF(datalst->value, datalst->length);
                    TRY_SAVE_BUF("\r\n", 2);
                    if (datalst == req->pListPost->prev) {
                        break;
                    }
                }

                TRY_SAVE_BUF(req->tagPost, sizeof(req->tagPost));
                TRY_SAVE_BUF("--\r\n", 4);
            } else {
                for (datalst = req->pListPost; datalst != NULL; datalst = datalst->next) {
                    int p;
                    for (p = 0; datalst->label[p] != '\0'; p++) {
                        n = NHTTPi_encodeUrlChar(tmpBuf, datalst->label[p]);
                        TRY_SAVE_BUF(tmpBuf, n);
                    }
                    TRY_SAVE_BUF("=", 1);

                    for (p = 0; datalst->value[p] != '\0'; p++) {
                        n = NHTTPi_encodeUrlChar(tmpBuf, datalst->value[p]);
                        TRY_SAVE_BUF(tmpBuf, n);
                    }
                    if (datalst == req->pListPost->prev) {
                        break;
                    }
                    TRY_SAVE_BUF("&", 1);
                }
            }
        }

        if (bufwp > 0) {
            int stat = NHTTPi_SocSend(req, sd, g_commBuf, bufwp, 0);
            if (stat < 0) {
                goto done;
            }
            if (stat == 0) {
                goto retry;
            }
        }

        result = NHTTP_ERROR_HTTPPARSE;
        response->nLenHeader = 0;

        tmpBuf[0] = '\0';
        tmpBuf[1] = '\0';
        tmpBuf[2] = '\0';
        tmpBuf[3] = '\0';

        for (hdrbuf = response->pHdrBufBlock, bufwp = 0;;) {
            if (req->doCancel) {
                goto done;
            }

            if (bufwp < NHTTP_HDRRECVBUF_INILEN) {
                recvbyte = NHTTPi_SocRecv(req, sd, response->pHdrBufFirst + bufwp, 1, 0);
                tmpBuf[bufwp & 3] = response->pHdrBufFirst[bufwp];
            } else {
                int wp = bufwp & NHTTP_HDRRECVBUF_BLOCKMASK;
                if (wp == 0) {
                    if (hdrbuf != NULL) {
                        hdrbuf->next = NHTTPi_alloc(sizeof(NHTTPi_HDRBUFLIST), 4);
                        hdrbuf = hdrbuf->next;
                    } else {
                        hdrbuf = NHTTPi_alloc(sizeof(NHTTPi_HDRBUFLIST), 4);
                        response->pHdrBufBlock = hdrbuf;
                    }

                    if (hdrbuf == NULL) {
                        result = NHTTP_ERROR_ALLOC;
                        goto done;
                    }

                    hdrbuf->next = NULL;
                }

                recvbyte = NHTTPi_SocRecv(req, sd, &hdrbuf->block[wp], 1, 0);
                tmpBuf[bufwp & 3] = hdrbuf->block[wp];
            }

            if (recvbyte <= 0) {
                result = NHTTP_ERROR_NITROWIFI;
                goto done;
            }

            bufwp += recvbyte;
            if (tmpBuf[(bufwp - 4) & 3] == '\r' && tmpBuf[(bufwp - 3) & 3] == '\n' && tmpBuf[(bufwp - 2) & 3] == '\r' && tmpBuf[(bufwp - 1) & 3] == '\n') {
                response->nLenHeader = bufwp;
                break;
            }
        }

        if (response->nLenHeader == 0) {
            goto done;
        }
        if (!NHTTPi_loadFromHdrRecvBuf(response, g_commBuf, 0, 14)) {
            goto done;
        }
        if (NHTTPi_strnicmp(g_commBuf, "HTTP/", 5) != 0) {
            goto done;
        }
        if (g_commBuf[8] != ' ') {
            goto done;
        }
        httpStatus = NHTTPi_strToInt(&g_commBuf[9], 3);
        if (httpStatus < 0) {
            goto done;
        }
        if (NHTTPi_findNextLineHdrRecvBuf(response, 12, response->nLenHeader, &labelend) < 0) {
            goto done;
        }
        contentLength = NHTTPi_getHeaderValue(response, "Content-Length", &n);
        if (contentLength == 0) {
            result = NHTTP_ERROR_NONE;
            goto done;
        }
        if (contentLength > COMMBUF_LEN) {
            goto done;
        }

        if (contentLength > 0) {
            if (!NHTTPi_loadFromHdrRecvBuf(response, g_commBuf, n, contentLength)) {
                goto done;
            }

            contentLength = NHTTPi_strToInt(g_commBuf, contentLength);
            if (contentLength < 0) {
                goto done;
            }

            req->nContentLength = contentLength;
        } else {
            req->nContentLength = -1;
        }

        isKeepAlive = NHTTPi_getHeaderValue(response, "Connection", &n);
        if (isKeepAlive == 0) {
            goto done;
        }
        if (isKeepAlive < 0) {
            char versionString[3];
            versionString[0] = g_commBuf[5];
            versionString[1] = g_commBuf[7];
            versionString[2] = 0;
            isKeepAlive = NHTTPi_strToInt(versionString, 2) >= 11;
        } else if (isKeepAlive > COMMBUF_LEN) {
            isKeepAlive = FALSE;
        } else if (NHTTPi_compareTokenN_HdrRecvBuf(response, n, n + isKeepAlive, "Keep-Alive", 0) == 0) {
            isKeepAlive = TRUE;
        } else {
            isKeepAlive = FALSE;
        }

        isChunked = NHTTPi_getHeaderValue(response, "Transfer-Encoding", &n);
        if (isChunked == 0) {
            goto done;
        }
        if (isChunked > COMMBUF_LEN) {
            isChunked = FALSE;
        } else {
            isChunked = isChunked > 0 ? NHTTPi_compareTokenN_HdrRecvBuf(response, n, n + isChunked, "chunked", ';') == 0 : 0;
        }

        if (req->method == NHTTP_REQMETHOD_HEAD) {
            goto done;
        }

        if (contentLength >= 0) {
            while (contentLength > 0 && !NHTTPi_isRecvBufFull(response, response->nLenBody)) {
                recvbyte = NHTTPi_RecvBufN(req, sd, response->nLenBody, contentLength, 0);
                if (recvbyte < 0) {
                    goto done;
                }
                if (recvbyte == 0) {
                    break;
                }
                response->nLenBody += recvbyte;
                contentLength -= recvbyte;
            }

            if (contentLength != 0) {
                result = NHTTPi_isRecvBufFull(response, response->nLenBody) ? NHTTP_ERROR_BUFFULL : NHTTP_ERROR_NITROWIFI;
                goto done;
            }

            result = NHTTP_ERROR_NONE;
            goto done;
        }

        result = NHTTP_ERROR_NITROWIFI;

        if (isChunked) {
            int chunkSize;
            while (TRUE) {
                tmpBuf[0] = '\0';
                tmpBuf[1] = '\0';
                for (bufwp = 0; bufwp < COMMBUF_LEN; bufwp++) {
                    recvbyte = NHTTPi_SocRecv(req, sd, &g_commBuf[bufwp], 1, 0);
                    if (recvbyte < 0) {
                        goto done;
                    }

                    tmpBuf[bufwp & 1] = g_commBuf[bufwp];
                    if (tmpBuf[bufwp & 1] == ';' || (tmpBuf[bufwp & 1] == '\n' && tmpBuf[(bufwp - 1) & 1] == '\r')) {
                        if (tmpBuf[bufwp & 1] == '\n') {
                            n = bufwp - 1;
                        } else {
                            n = bufwp;
                            if (skipLineBuf(req, sd) <= 0) {
                                goto done;
                            }
                        }

                        if (n == 0) {
                            goto done;
                        }

                        chunkSize = NHTTPi_strToHex(g_commBuf, n);
                        if (chunkSize < 0) {
                            goto done;
                        }
                        break;
                    }
                }

                if (bufwp == COMMBUF_LEN) {
                    result = NHTTP_ERROR_HTTPPARSE;
                    goto done;
                }

                if (chunkSize > 0) {
                    while (chunkSize > 0) {
                        recvbyte = NHTTPi_RecvBufN(req, sd, response->nLenBody, chunkSize, 0);
                        if (recvbyte <= 0) {
                            goto done;
                        }
                        response->nLenBody += recvbyte;
                        chunkSize -= recvbyte;
                        if (chunkSize == 0) {
                            recvbyte = NHTTPi_SocRecv(req, sd, g_commBuf, 1, 0);
                            if (recvbyte <= 0) {
                                goto done;
                            }
                            recvbyte = NHTTPi_SocRecv(req, sd, g_commBuf, 1, 0);
                            if (recvbyte <= 0) {
                                goto done;
                            }
                        }
                    }
                } else {
                    skipLineBuf(req, sd);
                    result = NHTTP_ERROR_NONE;
                    goto done;
                }
            }
        } else {
            while (!NHTTPi_isRecvBufFull(response, response->nLenBody)) {
                recvbyte = NHTTPi_RecvBuf(req, sd, response->nLenBody, 0);
                if (recvbyte < 0) {
                    goto done;
                }
                if (recvbyte == 0) {
                    result = NHTTP_ERROR_NONE;
                    goto done;
                }
                response->nLenBody += recvbyte;
                if (NHTTPi_isRecvBufFull(response, response->nLenBody)) {
                    recvbyte = NHTTPi_SocRecv(req, sd, g_commBuf, 1, 0);
                    if (recvbyte < 0) {
                        goto done;
                    }
                    if (recvbyte != 0) {
                        result = NHTTP_ERROR_BUFFULL;
                        goto done;
                    }
                }
            }
        }
done:
        NHTTPi_lockReqList();
        NHTTPi_free(NHTTPi_reqCurrent);
        NHTTPi_reqCurrent = NULL;
        NHTTPi_unlockReqList();

        if (req->doCancel) {
            result = NHTTP_ERROR_CANCELED;
        }

        if (sd >= 0 && (!isKeepAlive || result != NHTTP_ERROR_NONE)) {
            if (NHTTPi_SocClose(req, sd) < 0) {
                result = NHTTP_ERROR_NITROWIFI;
            }
            putSocketToCloseQueue(sd, &sslConnection);
            sd = -1;
            isKeepAlive = FALSE;
        }

        if (result == NHTTP_ERROR_NONE) {
            response->isSuccess = TRUE;
        } else {
            response->isSuccess = FALSE;
            NHTTPi_error = result;
        }

        void *param = req->param;
        NHTTPReqCallback callback = req->pCallback;
        NHTTPi_destroyRequestObject(req);
        callback(result, response, param);
    }

    if (sd >= 0) {
        NHTTPi_SocClose(req, sd);
        putSocketToCloseQueue(sd, &sslConnection);
    }

    clearCloseQueue();
}
