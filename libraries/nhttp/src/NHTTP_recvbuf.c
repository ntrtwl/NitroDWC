#include "NHTTP_recvbuf.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"
#include "NHTTP_socket.h"
#include "NHTTP_stdlib.h"

static void setupGetCharFromHdrRecvBuf(NHTTPRes *res, int pos, NHTTPi_HDRBUFLIST **pphdrbuf, int *pblockpos)
{
    NHTTPi_HDRBUFLIST *hdrbuf;

    if (pos < NHTTP_HDRRECVBUF_INILEN) {
        *pphdrbuf = NULL;
        *pblockpos = pos;
        return;
    }

    pos -= NHTTP_HDRRECVBUF_INILEN;
    int blockidx = pos >> NHTTP_HDRRECVBUF_BLOCKSHIFT;

    for (hdrbuf = res->pHdrBufBlock; blockidx--;) {
        hdrbuf = hdrbuf->next;
    }

    pos &= NHTTP_HDRRECVBUF_BLOCKMASK;
    *pphdrbuf = hdrbuf;
    *pblockpos = pos;
}

static char getCharFromHdrRecvBuf(NHTTPRes *res, NHTTPi_HDRBUFLIST **pphdrbuf, int *pblockpos)
{
    if (*pphdrbuf == NULL) {
        if (*pblockpos < NHTTP_HDRRECVBUF_INILEN) {
            return res->pHdrBufFirst[(*pblockpos)++];
        }
        *pblockpos = 0;
        *pphdrbuf = res->pHdrBufBlock;
    } else {
        if (*pblockpos == NHTTP_HDRRECVBUF_BLOCKLEN) {
            *pblockpos = 0;
            *pphdrbuf = (*pphdrbuf)->next;
        }
    }

    return (*pphdrbuf)->block[(*pblockpos)++];
}

int NHTTPi_findNextLineHdrRecvBuf(NHTTPRes *res, int pos, int limit, int *pcolon)
{
    NHTTPi_HDRBUFLIST *hdrbuf;
    int blockpos;

    BOOL foundCR = FALSE;
    if (pcolon != NULL) {
        *pcolon = -1;
    }

    if (pos < limit) {
        setupGetCharFromHdrRecvBuf(res, pos, &hdrbuf, &blockpos);
        for (; pos < limit; pos++) {
            char c = getCharFromHdrRecvBuf(res, &hdrbuf, &blockpos);

            if (c == ':' && pcolon != NULL && *pcolon < 0) {
                *pcolon = pos;
            }

            if (!foundCR) {
                if (c == '\r') {
                    foundCR = TRUE;
                }
            } else if (c == '\n') {
                if (pos == limit - 1) {
                    return 0;
                } else {
                    return pos + 1;
                }
            } else {
                foundCR = FALSE;
            }
        }
    }

    return -1;
}

int NHTTPi_skipSpaceHdrRecvBuf(NHTTPRes *res, int pos, int limit)
{
    NHTTPi_HDRBUFLIST *hdrbuf;
    int blockpos;

    if (pos < limit) {
        setupGetCharFromHdrRecvBuf(res, pos, &hdrbuf, &blockpos);
        for (; pos < limit; pos++) {
            if (getCharFromHdrRecvBuf(res, &hdrbuf, &blockpos) != ' ') {
                return pos;
            }
        }
    }

    return -1;
}

int NHTTPi_compareTokenN_HdrRecvBuf(NHTTPRes *res, int pos, int limit, char *str, char extdeli)
{
    NHTTPi_HDRBUFLIST *hdrbuf;
    int blockpos;
    char c;

    if (pos < limit) {
        setupGetCharFromHdrRecvBuf(res, pos, &hdrbuf, &blockpos);

        for (c = getCharFromHdrRecvBuf(res, &hdrbuf, &blockpos); toLower(c) == toLower(*str); pos++, str++) {
            if (*str == '\0' || *str == ' ' || *str == extdeli || pos == limit - 1) {
                return 0;
            }

            c = getCharFromHdrRecvBuf(res, &hdrbuf, &blockpos);
        }
    }

    return -1;
}

BOOL NHTTPi_loadFromHdrRecvBuf(NHTTPRes *res, char *dst, int pos, int len)
{
    NHTTPi_HDRBUFLIST *hdrbuf;

    if (pos + len <= res->nLenHeader) {
        if (len != 0) {
            if (pos < NHTTP_HDRRECVBUF_INILEN) {
                int blocklen = NHTTP_MIN(len, NHTTP_HDRRECVBUF_INILEN - pos);
                NHTTPi_memcpy(dst, &res->pHdrBufFirst[pos], blocklen);
                pos += blocklen;
                len -= blocklen;
                dst += blocklen;
            }

            if (len != 0) {
                pos -= NHTTP_HDRRECVBUF_INILEN;
                int blockidx = pos >> NHTTP_HDRRECVBUF_BLOCKSHIFT;
                pos &= NHTTP_HDRRECVBUF_BLOCKMASK;

                for (hdrbuf = res->pHdrBufBlock; blockidx--;) {
                    hdrbuf = hdrbuf->next;
                }

                while (len != 0) {
                    int blocklen = NHTTP_MIN(len, NHTTP_HDRRECVBUF_BLOCKLEN - pos);
                    NHTTPi_memcpy(dst, &hdrbuf->block[pos], blocklen);
                    hdrbuf = hdrbuf->next;
                    pos += blocklen;
                    pos &= NHTTP_HDRRECVBUF_BLOCKMASK;
                    len -= blocklen;
                    dst += blocklen;
                }
            }
        }
        return TRUE;
    }

    return FALSE;
}

BOOL NHTTPi_isRecvBufFull(NHTTPRes *res, int pos)
{
    return res->nLenRecvBuf <= pos;
}

int NHTTPi_RecvBuf(NHTTPReq *req, int s, int pos, int flags)
{
    NHTTPRes *res = req->response;
    return NHTTPi_SocRecv(req, s, &res->pRecvBuf[pos], res->nLenRecvBuf - pos, flags);
}

int NHTTPi_RecvBufN(NHTTPReq *req, int s, int pos, int len, int flags)
{
    NHTTPRes *res = req->response;
    if (NHTTPi_isRecvBufFull(res, pos)) {
        return NHTTP_SOCKET_ERROR_BUFFULL;
    }

    len = NHTTP_MIN(len, (int)(res->nLenRecvBuf - pos));
    return NHTTPi_SocRecv(req, s, &res->pRecvBuf[pos], len, flags);
}
