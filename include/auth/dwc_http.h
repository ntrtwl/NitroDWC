#ifndef DWC_HTTP_H_
#define DWC_HTTP_H_

#include <nitro.h>
#include <nitroWiFi.h>

#include "util_alloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DWCHTTP_MAXURL         256
#define DWCHTTP_STACKSIZE      4096
#define DWCHTTP_MAX_LABELVALUE 32

#define DWC_NETCHECK_BL_URL "http://conntest.nintendowifi.net/"
#define DWC_AUTH_NAS_URL    "https://nas.nintendowifi.net/ac"

typedef enum {
    DWCHTTP_E_NOERR,
    DWCHTTP_E_MEMERR,
    DWCHTTP_E_DNSERR,
    DWCHTTP_E_CONERR,
    DWCHTTP_E_SENDTOUT,
    DWCHTTP_E_SENDERR,
    DWCHTTP_E_RECVTOUT,
    DWCHTTP_E_ABORT,
    DWCHTTP_E_FINISH,
    DWCHTTP_E_MAX
} DWCHttpError;

typedef enum {
    DWCHTTP_POST,
    DWCHTTP_GET
} DWCHttpAction;

typedef struct DWCHttpBuffer {
    char *buffer;
    char *write_index;
    char *buffer_tail;
    u32 length;
} DWCHttpBuffer;

typedef struct DWCHttpLabelValue {
    char *label;
    char *value;
} DWCHttpLabelValue;

typedef struct DWCHttpParseResult {
    DWCHttpLabelValue *entry;
    int len;
    int index;
} DWCHttpParseResult;

typedef struct DWCHttpParam {
    const char *url;
    DWCHttpAction action;
    u32 len_recvbuf;
    DWCAuthAlloc alloc;
    DWCAuthFree free;
    BOOL ignoreca;
    int timeout;
} DWCHttpParam;

typedef struct DWCHttp {
    u8 stack[DWCHTTP_STACKSIZE];
    u8 initflag;
    DWCHttpParam param;
    DWCHttpError error;
    char url[DWCHTTP_MAXURL];
    char *hostname;
    char *filepath;
    u32 hostip;
    BOOL ssl_enabled;
    u16 port;
    CPSSoc soc;
    CPSSslConnection con;
    u8 *lowrecvbuf;
    u8 *lowsendbuf;
    u32 lowentropydata[OS_LOW_ENTROPY_DATA_SIZE / sizeof(u32)];
    int num_postitem;
    DWCHttpBuffer req;
    DWCHttpBuffer rep;
    OSMutex content_len_mutex;
    int content_len;
    int receivedbody_len;
    DWCHttpLabelValue labelvalue[DWCHTTP_MAX_LABELVALUE];
    OSThread thread;
    OSMutex mutex;
    BOOL abort;
} DWCHttp;

DWCHttpError DWC_Http_Create(DWCHttp *http, DWCHttpParam *param);

void DWC_Http_Destroy(DWCHttp *http);
void DWC_Http_StartThread(DWCHttp *http, u32 prio);
void DWC_Http_Abort(DWCHttp *http);

int DWC_Http_GetRecvProgress(DWCHttp *http);

DWCHttpError DWC_Http_Add_HeaderItem(DWCHttp *http, char *label, char *data);
DWCHttpError DWC_Http_Add_PostBase64Item(DWCHttp *http, const char *label, const char *data, u32 data_len);
DWCHttpError DWC_Http_Add_Body(DWCHttp *http, const char *data);
DWCHttpError DWC_Http_FinishHeader(DWCHttp *http);

BOOL DWC_Http_ParseResult(DWCHttp *http, BOOL noparsebody);

char *DWC_Http_GetResult(DWCHttp *http, char *label);
int DWC_Http_GetBase64DecodedResult(DWCHttp *http, char *label, char *buffer, u32 buffer_len);
BOOL DWC_Http_GetRawResult(DWCHttp *http, char *label, char *buffer, int buffer_len);

#ifdef __cplusplus
}
#endif

#endif
