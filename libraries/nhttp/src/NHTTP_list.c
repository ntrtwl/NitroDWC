#include "NHTTP_list.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_control.h"
#include "NHTTP_lib.h"
#include "NHTTP_request.h"

extern NHTTPAlloc NHTTPi_alloc;
extern NHTTPFree NHTTPi_free;
extern volatile int NHTTPi_idRequest;
extern volatile NHTTPi_REQLIST *NHTTPi_reqQueue;

int NHTTPi_setReqQueue(NHTTPReq *req)
{
    int id = -1;
    NHTTPi_REQLIST *lst = NHTTPi_alloc(sizeof(NHTTPi_REQLIST), 4);
    if (lst != NULL) {
        if (NHTTPi_reqQueue != NULL) {
            lst->prev = NHTTPi_reqQueue->prev;
            lst->next = NHTTPi_reqQueue;
            NHTTPi_reqQueue->prev->next = lst;
            NHTTPi_reqQueue->prev = lst;
        } else {
            lst->prev = lst;
            lst->next = lst;
            NHTTPi_reqQueue = lst;
        }

        lst->id = NHTTPi_idRequest++;
        lst->req = req;
        lst->socket = -1;
        id = lst->id;
        if (NHTTPi_idRequest < 0) {
            NHTTPi_idRequest = 0;
        }
    }
    return id;
}

NHTTPi_REQLIST *NHTTPi_findReqQueue(int id)
{
    NHTTPi_REQLIST *lst;
    NHTTPi_REQLIST *lstRet = NULL;

    if (NHTTPi_reqQueue != NULL) {
        lst = NHTTPi_reqQueue;
        if (lst->id == id) {
            lstRet = lst;
        } else {
            for (lst = lst->next; lst != NHTTPi_reqQueue; lst = lst->next) {
                if (lst->id == id) {
                    lstRet = lst;
                    break;
                }
            }
        }
    }

    return lstRet;
}

BOOL NHTTPi_freeReqQueue(int id)
{
    BOOL rc = FALSE;
    NHTTPi_REQLIST *lst = NHTTPi_findReqQueue(id);

    if (lst != NULL) {
        if (NHTTPi_reqQueue != NHTTPi_reqQueue->prev) {
            lst->prev->next = lst->next;
            lst->next->prev = lst->prev;
            if (NHTTPi_reqQueue == lst) {
                NHTTPi_reqQueue = lst->next;
            }
        } else {
            NHTTPi_reqQueue = NULL;
        }

        NHTTPReqCallback callback = lst->req->pCallback;
        NHTTPRes *res = lst->req->response;
        void *param = lst->req->param;
        NHTTPi_destroyRequestObject(lst->req);
        NHTTPi_free(lst);
        callback(NHTTP_ERROR_CANCELED, res, param);
        rc = TRUE;
    }

    return rc;
}

void NHTTPi_allFreeReqQueue(void)
{
    while (NHTTPi_reqQueue != NULL) {
        NHTTPi_freeReqQueue(NHTTPi_reqQueue->id);
    }
}

NHTTPi_REQLIST *NHTTPi_getReqFromReqQueue(void)
{
    return (NHTTPi_REQLIST *)NHTTPi_getHdrFromList((NHTTPi_DATALIST **)&NHTTPi_reqQueue);
}
