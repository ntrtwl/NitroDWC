#ifndef ND_LIB
#define ND_LIB

#include <nitro.h>

#include "nd/DWCi_Nd.h"

typedef struct DWCi_Ndi_HTTPREQ {
    int id;
    DWCNdCallbackReason reason;
    DWCNdCallback callback;
    void *threadEvent;
    union {
        struct {
            int *entrynum;
        } count;
        struct {
            DWCNdFileInfo *filelist;
            int intnum;
            char *recvbuf;
            char *offset;
            char *num;
        } list;
        struct {
            char *filename;
            u32 filesize;
        } file;
    } param;
} DWCi_Ndi_HTTPREQ;

#endif // ND_LIB
