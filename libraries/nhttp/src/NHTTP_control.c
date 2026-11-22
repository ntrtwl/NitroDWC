#include "NHTTP_control.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_token.h"

extern volatile NHTTPError NHTTPi_error;
extern NHTTPAlloc NHTTPi_alloc;
extern const char NHTTPi_strMultipartBound[];

static BOOL addHdrList(NHTTPi_DATALIST **pplst, char *label, char *value, u32 listsize)
{
    BOOL isFound = FALSE;
    NHTTPi_DATALIST *sl;

    if (*pplst != NULL) {
        sl = *pplst;
        if (NHTTPi_compareToken(label, sl->label) != 0) {
            for (sl = sl->next; sl != *pplst; sl = sl->next) {
                if (NHTTPi_compareToken(label, sl->label) == 0) {
                    isFound = TRUE;
                    break;
                }
            }
        } else {
            isFound = TRUE;
        }
    }

    if (isFound) {
        sl->value = value;
    } else {
        sl = NHTTPi_alloc(listsize, 4);
        if (sl == NULL) {
            NHTTPi_error = NHTTP_ERROR_ALLOC;
            return FALSE;
        }
        sl->label = label;
        sl->value = value;
        sl->length = 0;
        sl->isBinary = FALSE;

        if (*pplst) {
            sl->prev = (*pplst)->prev;
            sl->next = *pplst;
            (*pplst)->prev->next = sl;
            (*pplst)->prev = sl;
        } else {
            sl->next = sl;
            sl->prev = sl;
            *pplst = sl;
        }
    }

    return TRUE;
}

NHTTPi_DATALIST *NHTTPi_getHdrFromList(NHTTPi_DATALIST **pplst)
{
    NHTTPi_DATALIST *lst = *pplst;

    if (lst != NULL) {
        if (lst != lst->prev) {
            lst->prev->next = lst->next;
            lst->next->prev = lst->prev;
            *pplst = lst->next;
        } else {
            *pplst = NULL;
        }
    }

    return lst;
}

static char incAscii(u8 c)
{
    c++;

    if (c == 'z' + 1) {
        c = '0';
    } else if (c == 'Z' + 1) {
        c = 'a';
    } else if (c == '9' + 1) {
        c = 'A';
    }

    return c;
}

static BOOL checkTagPost(NHTTPReq *req, char *value, u32 length)
{
    if (NHTTPi_memfind(value, length, &req->tagPost[2], MULTIPART_BOUNDARY_LEN) < 0) {
        return TRUE;
    }

    for (int n = 2 + MULTIPART_BOUNDARY_LEN - 1; n >= 2; n--) {
        char c = req->tagPost[n];
        while (TRUE) {
            c = incAscii(c);
            req->tagPost[n] = c;

            if (c == NHTTPi_strMultipartBound[n]) {
                break;
            }
            if (NHTTPi_memfind(value, length, &req->tagPost[2], MULTIPART_BOUNDARY_LEN) < 0) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL NHTTP_AddHeaderField(NHTTPReq *req, char *label, char *value)
{
    if (req->isStarted) {
        return FALSE;
    }

    return addHdrList(&req->pListHeader, label, value, sizeof(NHTTPi_DATALIST));
}

BOOL NHTTP_AddPostDataAscii(NHTTPReq *req, char *label, char *value)
{
    BOOL rc = FALSE;
    if (req->isStarted) {
        return FALSE;
    }

    u32 length = NHTTPi_strlen(value);
    if (checkTagPost(req, value, length)) {
        rc = addHdrList(&req->pListPost, label, value, sizeof(NHTTPi_DATALIST));
        if (rc) {
            req->pListPost->prev->length = length;
        }
    }
    return rc;
}

BOOL NHTTP_AddPostDataBinary(NHTTPReq *req, char *label, char *value, u32 length)
{
    if (req->isStarted) {
        return FALSE;
    }

    BOOL rc = FALSE;
    if (checkTagPost(req, value, length)) {
        rc = addHdrList(&req->pListPost, label, value, sizeof(NHTTPi_DATALIST));
        if (rc) {
            req->pListPost->prev->length = length;
            req->pListPost->prev->isBinary = TRUE;
        }
    }
    return rc;
}

BOOL NHTTP_SetCAChain(NHTTPReq *req, SOCCaInfo **cainfo, int cabuiltins)
{
    if (req->isStarted) {
        return FALSE;
    }

    req->ca_info = cainfo;
    req->n_ca = cabuiltins;
    return TRUE;
}
