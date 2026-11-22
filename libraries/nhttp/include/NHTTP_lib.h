#ifndef NHTTP_LIB_H
#define NHTTP_LIB_H

#include <nitro.h>

#include "nhttp/NHTTP.h"

typedef struct NHTTPi_REQLIST NHTTPi_REQLIST;

typedef struct NHTTPi_REQLIST {
    NHTTPi_REQLIST *prev;
    NHTTPi_REQLIST *next;
    int id;
    NHTTPReq *req;
    int socket;
} NHTTPi_REQLIST;

#define NHTTP_MIN(a, b) (((a) > (b)) ? (b) : (a))

#define toLower(c) (('A' <= c && c <= 'Z') ? c - ('A' - 'a') : c)

enum NHTTPSocketError {
    NHTTP_SOCKET_ERROR_NETWORK = -1001,
    NHTTP_SOCKET_ERROR_CANCELED = -1002,
    NHTTP_SOCKET_ERROR_BUFFULL = -1003,
};

#endif // NHTTP_LIB_H
