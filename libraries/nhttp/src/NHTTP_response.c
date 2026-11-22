#include "NHTTP_response.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_recvbuf.h"
#include "NHTTP_stdlib.h"

extern volatile NHTTPError NHTTPi_error;
extern NHTTPAlloc NHTTPi_alloc;
extern NHTTPFree NHTTPi_free;

void NHTTP_DestroyResponse(NHTTPRes *res)
{
    while (res->pHdrBufBlock != NULL) {
        NHTTPi_HDRBUFLIST *lst = res->pHdrBufBlock->next;
        NHTTPi_free(res->pHdrBufBlock);
        res->pHdrBufBlock = lst;
    }

    if (res->allHeader != NULL) {
        NHTTPi_free(res->allHeader);
    }

    if (res->foundHeader != NULL) {
        NHTTPi_free(res->foundHeader);
    }

    NHTTPi_free(res);
}

int NHTTPi_getHeaderValue(NHTTPRes *res, char *label, int *ppos)
{
    int labeltop, labelend;

    int linetop = NHTTPi_findNextLineHdrRecvBuf(res, 12, res->nLenHeader, &labelend);

    while (linetop > 0) {
        int nextlinetop = NHTTPi_findNextLineHdrRecvBuf(res, linetop, res->nLenHeader, &labelend);

        if (labelend > 0) {
            labeltop = linetop;

            if (NHTTPi_compareTokenN_HdrRecvBuf(res, labeltop, labelend, label, 0) == 0) {
                if (labelend + 1 < res->nLenHeader) {
                    int valueend = NHTTPi_findNextLineHdrRecvBuf(res, labelend + 1, res->nLenHeader, NULL);
                    if (valueend <= 0) {
                        valueend = res->nLenHeader;
                    } else {
                        if (valueend < 2) {
                            return -1;
                        }
                        valueend -= 2;
                    }

                    int valuetop = NHTTPi_skipSpaceHdrRecvBuf(res, labelend + 1, valueend);
                    if (valuetop < 0) {
                        valuetop = valueend;
                    }

                    *ppos = valuetop;
                    return valueend - valuetop;
                } else {
                    return 0;
                }
            }
        }
        linetop = nextlinetop;
    }

    return -1;
}

int NHTTP_GetHeaderField(NHTTPRes *res, char *label, char **value)
{
    int pos;

    if (!res->isSuccess) {
        return -1;
    }

    if (res->foundHeader != NULL) {
        NHTTPi_free(res->foundHeader);
        res->foundHeader = NULL;
    }

    int size = NHTTPi_getHeaderValue(res, label, &pos);
    if (size >= 0) {
        res->foundHeader = NHTTPi_alloc(size + 1, 4);
        res->foundHeader[size] = '\0';
        NHTTPi_loadFromHdrRecvBuf(res, res->foundHeader, pos, size);
        *value = res->foundHeader;
        return size;
    }

    if (NHTTPi_strcmp("HTTPSTATUSCODE", label) == 0) {
        res->foundHeader = NHTTPi_alloc(4, 4);
        res->foundHeader[3] = '\0';
        NHTTPi_loadFromHdrRecvBuf(res, res->foundHeader, 9, 3);
        *value = res->foundHeader;
        return 3;
    }

    return -1;
}

int NHTTP_GetHeaderAll(NHTTPRes *res, char **value)
{
    if (!res->isSuccess || res->nLenHeader == 0) {
        return -1;
    }

    if (res->nLenHeader < NHTTP_HDRRECVBUF_INILEN) {
        *value = res->pHdrBufFirst;
    } else {
        if (res->allHeader == NULL) {
            res->allHeader = NHTTPi_alloc(res->nLenHeader, 4);
            if (res->allHeader == NULL) {
                NHTTPi_error = NHTTP_ERROR_ALLOC;
                return -1;
            }
            NHTTPi_loadFromHdrRecvBuf(res, res->allHeader, 0, res->nLenHeader);
        }
        *value = res->allHeader;
    }

    return res->nLenHeader;
}

int NHTTP_GetBodyAll(NHTTPRes *res, char **value)
{
    if (!res->isSuccess || res->nLenBody == 0) {
        return -1;
    }

    *value = res->pRecvBuf;
    return res->nLenBody;
}
