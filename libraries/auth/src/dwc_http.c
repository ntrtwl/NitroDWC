#include "auth/dwc_http.h"

#include <nitro.h>
#include <nitroWiFi.h>
#include <stdlib.h>
#include <string.h>

#define DWC_HTTP_LOWSENDBUF_SIZE 1514
#define DWC_HTTP_LOWRECVBUF_SIZE 1460 * 2

BOOL DWChttpignoreca = FALSE;

extern CPSCaInfo DWCca_nas_self;
extern CPSCaInfo DWCca_verisign_c3pcag3v2;
extern CPSCaInfo DWCac_verisign_c3pcag2v2;
extern CPSCaInfo DWCca_verisign_pca3ssg4;
extern CPSCaInfo DWCca_verisign_secureserver;
extern CPSCaInfo DWCca_cybertrust_globalsign;
extern CPSCaInfo DWCca_thawte_server;
extern CPSCaInfo DWCca_thawte_premiumserver;
extern CPSCaInfo DWCca_cybertrust_gteglroot;
extern CPSCaInfo DWCca_cybertrust_gteroot;
extern CPSCaInfo DWCca_cybertrust_baltimore;
extern CPSCaInfo DWCca_globalsign_rootcar2;

CPSCaInfo *DWCcainfo[] = {
    &DWCca_verisign_c3pcag3v2,
    &DWCac_verisign_c3pcag2v2,
    &DWCca_verisign_pca3ssg4,
    &DWCca_verisign_secureserver,
    &DWCca_cybertrust_globalsign,
    &DWCca_cybertrust_gteglroot,
    &DWCca_cybertrust_gteroot,
    &DWCca_cybertrust_baltimore,
    &DWCca_thawte_server,
    &DWCca_thawte_premiumserver,
    &DWCca_globalsign_rootcar2,
    &DWCca_nas_self
};

static void DWCi_Http_Thread(void *arg);
static DWCHttpError DWCi_Http_WriteBasicHeader(DWCHttp *http);
static int DWCi_Http_CPSCallback(int result, CPSSslConnection *con, int level);
static BOOL DWCi_Http_AllocBuffer(DWCHttp *http, DWCHttpBuffer *buf, u32 size);
static void DWCi_Http_FreeBuffer(DWCHttp *http, DWCHttpBuffer *buf);
static BOOL DWCi_Http_ReallocBuffer(DWCHttp *http, DWCHttpBuffer *buf, int size);
static BOOL DWCi_Http_SetHostinfo(DWCHttp *http, const char *url);
static void DWCi_Http_DestroyResult(DWCHttp *http, DWCHttpLabelValue *array_entry, int array_len);

DWCHttpError DWC_Http_Create(DWCHttp *http, DWCHttpParam *param)
{
    DWCAuthAlloc alloc = param->alloc;

    MI_CpuClear8(http, sizeof(DWCHttp));

    http->content_len = -1;
    http->receivedbody_len = -1;

    http->param = *param;

    http->lowrecvbuf = alloc((u32) "ALLOC http->lowrecvbuf", DWC_HTTP_LOWRECVBUF_SIZE);
    if (http->lowrecvbuf == NULL) {
        http->error = DWCHTTP_E_MEMERR;
        return DWCHTTP_E_MEMERR;
    }

    http->lowsendbuf = alloc((u32) "ALLOC http->lowsendbuf", DWC_HTTP_LOWSENDBUF_SIZE);
    if (http->lowsendbuf == NULL) {
        http->error = DWCHTTP_E_MEMERR;
        return DWCHTTP_E_MEMERR;
    }

    if (!DWCi_Http_AllocBuffer(http, &http->rep, http->param.len_recvbuf)) {
        http->error = DWCHTTP_E_MEMERR;
        return DWCHTTP_E_MEMERR;
    }

    if (!DWCi_Http_SetHostinfo(http, param->url)) {
        http->error = DWCHTTP_E_MEMERR;
        return DWCHTTP_E_MEMERR;
    }

    http->error = DWCi_Http_WriteBasicHeader(http);

    if (http->error == DWCHTTP_E_NOERR) {
        http->initflag = 0xFF;
    }

    return http->error;
}

DWCHttpError DWC_Http_FinishHeader(DWCHttp *http)
{
    char str_contentlen[8];

    if (DWC_Http_Add_HeaderItem(http, "Connection", "close") != DWCHTTP_E_NOERR) {
        return DWCHTTP_E_MEMERR;
    }

    DWCHttpBuffer *temp_buffer = &http->req;

    char *endofheader = strstr(temp_buffer->buffer, "\r\n\r\n");
    endofheader += 4;

    int contentlen = strlen(endofheader);
    if (contentlen != 0) {
        OS_SNPrintf(str_contentlen, 7, "%d", contentlen);
        if (DWC_Http_Add_HeaderItem(http, "Content-Length", str_contentlen) != DWCHTTP_E_NOERR) {
            return DWCHTTP_E_MEMERR;
        }
    }

    return DWCHTTP_E_NOERR;
}

void DWC_Http_StartThread(DWCHttp *http, u32 prio)
{
    http->abort = FALSE;
    OS_InitMutex(&http->mutex);
    OS_InitMutex(&http->content_len_mutex);

    if (http->param.ignoreca == TRUE) {
        DWChttpignoreca = TRUE;
    } else {
        DWChttpignoreca = FALSE;
    }

    if (http->thread.id != 0) {
        if (OS_IsThreadTerminated(&http->thread) == FALSE) {
            return;
        }
    }

    OS_CreateThread(&http->thread, DWCi_Http_Thread, http, http->stack + sizeof(http->stack), sizeof(http->stack), prio);
    OS_WakeupThreadDirect(&http->thread);
}

void DWC_Http_Abort(DWCHttp *http)
{
    if (http->initflag != 0xFF) {
        return;
    }

    OS_LockMutex(&http->mutex);
    http->abort = TRUE;
    OS_UnlockMutex(&http->mutex);

    if (http->thread.id != 0) {
        OS_JoinThread(&http->thread);
    }
}

static BOOL DWCi_Http_Yield(DWCHttp *http)
{
    if (http->ssl_enabled == TRUE) {
        OS_GetLowEntropyData(http->lowentropydata);
        CPS_SslAddRandomSeed(http->lowentropydata, OS_LOW_ENTROPY_DATA_SIZE);
    }

    OS_LockMutex(&http->mutex);
    if (http->abort == TRUE) {
        OS_UnlockMutex(&http->mutex);
        return FALSE;
    }
    OS_UnlockMutex(&http->mutex);

    OS_Sleep(10);
    return TRUE;
}

int DWC_Http_GetRecvProgress(DWCHttp *http)
{
    OS_LockMutex(&http->content_len_mutex);
    if (http->receivedbody_len < 0 || http->content_len < 0) {
        OS_UnlockMutex(&http->content_len_mutex);
        return -1;
    }

    int recvsize = http->receivedbody_len / http->content_len;

    OS_UnlockMutex(&http->content_len_mutex);

    return recvsize;
}

static void DWCi_Http_InitCpsSocket(DWCHttp *http)
{
    MI_CpuClear8(&http->soc, sizeof(CPSSoc));

    http->soc.rcvbuf.size = DWC_HTTP_LOWRECVBUF_SIZE;
    http->soc.rcvbuf.data = http->lowrecvbuf;
    http->soc.sndbuf.size = DWC_HTTP_LOWSENDBUF_SIZE;
    http->soc.sndbuf.data = http->lowsendbuf;

    CPS_SocRegister(&http->soc);
}

static CPSInAddr DWCi_Http_Resolve(DWCHttp *http)
{
    CPSInAddr ip = CPS_Resolve(http->hostname);
    return ip;
}

static BOOL DWCi_Http_CheckHeaderReceived(DWCHttp *http)
{
    DWCHttpBuffer *rep = &http->rep;

    if (strstr(rep->buffer, "\r\n\r\n") == NULL) {
        return FALSE;
    }
    char *endof_header = strstr(rep->buffer, "\r\n\r\n") + 4;

    OS_LockMutex(&http->content_len_mutex);
    http->receivedbody_len = rep->write_index - endof_header;
    OS_UnlockMutex(&http->content_len_mutex);

    char *content_length = strstr(rep->buffer, "Content-Length: ");
    if (content_length == NULL) {
        return TRUE;
    }

    content_length += strlen("Content-Length: ");
    char *content_end = strstr(content_length, "\r\n");
    char content_temp = *content_end;
    *content_end = '\0';

    OS_LockMutex(&http->content_len_mutex);
    http->content_len = atoi(content_length);
    OS_UnlockMutex(&http->content_len_mutex);

    *content_end = content_temp;
    return TRUE;
}

static void DWCi_Http_Thread(void *arg)
{
    BOOL header_received = FALSE;

    DWCHttp *http = arg;
    CPSSoc *soc = &http->soc;
    CPSSslConnection *con = &http->con;
    DWCHttpBuffer *rep = &http->rep;

    int timeout;
    if (http->param.timeout <= 0) {
        timeout = 60000;
    } else {
        timeout = http->param.timeout;
    }

    DWCi_Http_InitCpsSocket(http);

    CPSInAddr ip = DWCi_Http_Resolve(http);

    if (ip == 0) {
        http->error = DWCHTTP_E_DNSERR;
        return;
    }

    http->hostip = ip;

    CPS_SocUse();

    if (http->ssl_enabled == TRUE) {
        MI_CpuClear8(con, sizeof(CPSSslConnection));
        con->auth_callback = DWCi_Http_CPSCallback;
        con->server_name = http->hostname;
        soc->con = con;
        CPS_SetRootCa(DWCcainfo, sizeof(DWCcainfo) / sizeof(DWCcainfo[0]));
        CPS_SetSsl(TRUE);
    }

    CPS_SocBind(0, http->port, ip);

    u32 tcpconnect = CPS_TcpConnect();
    if (tcpconnect) {
        http->error = DWCHTTP_E_CONERR;

        CPS_SocRelease();
        CPS_SocUnregister();

        return;
    }

    s32 len = CPS_SocWrite(http->req.buffer, strlen(http->req.buffer));

    if (len <= 0) {
        http->error = DWCHTTP_E_SENDERR;
        goto err;
    }

    CPS_SocFlush();

    if (DWCi_Http_Yield(http) == FALSE) {
        http->error = DWCHTTP_E_ABORT;
        goto err;
    }

    rep->write_index = rep->buffer;
    rep->buffer_tail = rep->buffer + rep->length;

    OSTick lastactivetime = OS_GetTick();

    while (TRUE) {
        if (CPSMyIp == 0) {
            http->error = DWCHTTP_E_SENDERR;
            goto err;
        }

        len = CPS_SocGetLength();
        if (len < 0) {
            break;
        }
        if (len > 0) {
            lastactivetime = OS_GetTick();
            u8 *s = CPS_SocRead((u32 *)&len);
            if (s == NULL) {
                break;
            }

            u32 copy_length = len < rep->buffer_tail - 1 - rep->write_index ? len : rep->buffer_tail - 1 - rep->write_index;
            MI_CpuCopy8(s, rep->write_index, copy_length);
            rep->write_index += copy_length;
            *rep->write_index = '\0';

            if (header_received == TRUE) {
                OS_LockMutex(&http->content_len_mutex);
                http->receivedbody_len += copy_length;
                OS_UnlockMutex(&http->content_len_mutex);
            } else {
                header_received = DWCi_Http_CheckHeaderReceived(http);
            }

            if (len > copy_length) {
                CPS_SocConsume(len);
                break;
            } else {
                CPS_SocConsume(copy_length);
            }
        }

        if (http->content_len >= 0 && http->receivedbody_len >= http->content_len) {
            break;
        }

        OSTick nowtime = OS_GetTick();

        if (OS_TicksToMilliSeconds(nowtime - lastactivetime) > timeout) {
            http->error = DWCHTTP_E_RECVTOUT;
            goto err;
        }

        if (DWCi_Http_Yield(http) == FALSE) {
            http->error = DWCHTTP_E_ABORT;
            goto err;
        }
    }

    CPS_TcpShutdown();
    CPS_TcpClose();

    CPS_SocRelease();
    CPS_SocUnregister();

    http->error = DWCHTTP_E_FINISH;

    return;

err:
    CPS_TcpShutdown();
    CPS_TcpClose();

    CPS_SocRelease();
    CPS_SocUnregister();
}

void DWC_Http_Destroy(DWCHttp *http)
{
    DWCAuthFree free = http->param.free;

    if (http == NULL) {
        return;
    }

    DWCi_Http_DestroyResult(http, http->labelvalue, DWCHTTP_MAX_LABELVALUE);

    DWCi_Http_FreeBuffer(http, &http->rep);
    DWCi_Http_FreeBuffer(http, &http->req);

    if (http->lowrecvbuf != NULL) {
        free((u32) "FREE http->lowrecvbuf", http->lowrecvbuf, 0);
        http->lowrecvbuf = NULL;
    }
    if (http->lowsendbuf != NULL) {
        free((u32) "FREE http->lowsendbuf", http->lowsendbuf, 0);
        http->lowsendbuf = NULL;
    }

    MI_CpuClear8(http, sizeof(DWCHttp));
}

static DWCHttpError DWCi_Http_WriteBasicHeader(DWCHttp *http)
{
    DWCHttpBuffer *req = &http->req;
    char *format;

    if (http->param.action == DWCHTTP_POST) {
        format = "POST /%s HTTP/1.0\r\nContent-type: application/x-www-form-urlencoded\r\nHost: %s\r\n\r\n";
    } else {
        format = "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n";
    }

    int result_len = strlen(format) - 2 + strlen(http->filepath) - 2 + strlen(http->hostname);

    if (DWCi_Http_AllocBuffer(http, &http->req, result_len + 1024) != TRUE) {
        return DWCHTTP_E_MEMERR;
    }

    req->write_index += OS_SNPrintf(req->write_index, req->length, format, http->filepath, http->hostname);

    return DWCHTTP_E_NOERR;
}

DWCHttpError DWC_Http_Add_HeaderItem(DWCHttp *http, char *label, char *data)
{
    char temp;
    char *temp_index;
    DWCHttpBuffer *temp_buffer = &http->req;

    int result_len = strlen("%s: %s\r\n") - 4 + strlen(label) + strlen(data);
    int buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;

    if (result_len + 1 > buf_remain) {
        if (!DWCi_Http_ReallocBuffer(http, temp_buffer, result_len - buf_remain + 1)) {
            return DWCHTTP_E_MEMERR;
        }
        buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;
    }

    temp_index = strstr(temp_buffer->buffer, "\r\n\r\n");
    temp_index += 2;
    temp = *temp_index;

    memmove(temp_index + result_len, temp_index, strlen(temp_index) + 1);

    temp_index += OS_SNPrintf(temp_index, result_len + 1, "%s: %s\r\n", label, data);

    *temp_index = temp;

    temp_buffer->write_index += result_len;

    return DWCHTTP_E_NOERR;
}

DWCHttpError DWC_Http_Add_PostBase64Item(DWCHttp *http, const char *label, const char *data, u32 data_len)
{
    int result_len, result_base64, buf_remain;
    DWCHttpBuffer *temp_buffer = &http->req;
    char *format;

    if (http->num_postitem == 0) {
        format = "%s=";
    } else {
        format = "&%s=";
    }

    http->num_postitem++;

    result_base64 = DWC_Auth_Base64Encode(data, data_len, NULL, 0);
    result_len = strlen(format) - 2 + strlen(label) + result_base64;
    buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;

    if (result_len > buf_remain) {
        if (!DWCi_Http_ReallocBuffer(http, temp_buffer, result_len - buf_remain + 1)) {
            return DWCHTTP_E_MEMERR;
        }

        buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;
    }

    temp_buffer->write_index += OS_SNPrintf(temp_buffer->write_index, buf_remain, format, label);

    buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;
    if (DWC_Auth_Base64Encode(data, data_len, temp_buffer->write_index, buf_remain - 1) < 0) {
        return DWCHTTP_E_MEMERR;
    }

    temp_buffer->write_index += result_base64;
    *temp_buffer->write_index = '\0';

    return DWCHTTP_E_NOERR;
}

DWCHttpError DWC_Http_Add_Body(DWCHttp *http, const char *data)
{
    int result_len, buf_remain;
    DWCHttpBuffer *temp_buffer = &http->req;

    result_len = strlen(data);
    buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;

    if (result_len > buf_remain) {
        if (!DWCi_Http_ReallocBuffer(http, temp_buffer, result_len - buf_remain + 1)) {
            return DWCHTTP_E_MEMERR;
        }

        buf_remain = temp_buffer->buffer_tail - temp_buffer->write_index;
    }

    int temp = OS_SNPrintf(temp_buffer->write_index, buf_remain, "%s", data);
    if (temp != result_len) {
        return DWCHTTP_E_MEMERR;
    }
    temp_buffer->write_index += temp;

    return DWCHTTP_E_NOERR;
}

static int DWCi_Http_CPSCallback(int result, CPSSslConnection *con, int level)
{
    if (result & CPS_CERT_OUTOFDATE) {
        result &= ~CPS_CERT_OUTOFDATE;
    }

    return result;
}

static BOOL DWCi_Http_AllocBuffer(DWCHttp *http, DWCHttpBuffer *buf, u32 size)
{
    DWCAuthAlloc alloc = http->param.alloc;

    if (size <= 0) {
        return FALSE;
    }

    buf->buffer = alloc((u32) "ALLOC buf->buffer", size);
    if (buf->buffer == NULL) {
        return FALSE;
    }

    buf->write_index = buf->buffer;
    buf->length = size;
    buf->buffer_tail = buf->buffer + buf->length;

    return TRUE;
}

static void DWCi_Http_FreeBuffer(DWCHttp *http, DWCHttpBuffer *buf)
{
    DWCAuthFree free = http->param.free;
    if (buf->buffer) {
        free((u32) "FREE buf->buffer", buf->buffer, 0);
    }
    MI_CpuClear8(buf, sizeof(DWCHttpBuffer));
}

static BOOL DWCi_Http_ReallocBuffer(DWCHttp *http, DWCHttpBuffer *buf, int size)
{
    DWCAuthAlloc alloc = http->param.alloc;
    DWCAuthFree free = http->param.free;

    if (size <= 0) {
        return FALSE;
    }

    void *newptr = alloc((u32) "ALLOC newptr", buf->length + size);
    if (newptr == NULL) {
        return FALSE;
    }

    MI_CpuCopy8(buf->buffer, newptr, buf->length);

    free((u32) "FREE buf->buffer", buf->buffer, 0);

    if (newptr == NULL) {
        return FALSE;
    }

    buf->write_index += (char *)newptr - buf->buffer;
    buf->length += size;
    buf->buffer = newptr;
    buf->buffer_tail = (char *)newptr + buf->length;

    return TRUE;
}

static BOOL DWCi_Http_SetHostinfo(DWCHttp *http, const char *url)
{
    char *temp = NULL;
    char *portnum = NULL;

    if (strlen(url) >= DWCHTTP_MAXURL) {
        return FALSE;
    }
    strncpy(http->url, url, DWCHTTP_MAXURL);
    if (strlen(url) != strlen(http->url)) {
        return FALSE;
    }

    temp = strstr(http->url, "http://");
    if (temp != NULL) {
        http->hostname = http->url + 7;
        http->ssl_enabled = FALSE;
        http->port = 80;
    } else {
        temp = strstr(http->url, "https://");
        if (temp == NULL) {
            return FALSE;
        }
        http->hostname = temp + 8;
        http->ssl_enabled = TRUE;
        http->port = 443;
    }

    temp = strstr(http->hostname, ":");
    if (temp != NULL) {
        *temp = '\0';
        portnum = temp + 1;
    }

    temp = strstr(http->hostname, "/");
    if (temp == NULL) {
        http->filepath = NULL;
    } else {
        *temp = '\0';
        http->filepath = temp + 1;
    }

    if (portnum != NULL) {
        http->port = atoi(portnum);
    }

    return TRUE;
}

static BOOL DWCi_Http_AddResult(DWCHttp *http, DWCHttpParseResult *result, char *label, char *value)
{
    DWCAuthAlloc alloc = http->param.alloc;
    DWCAuthFree free = http->param.free;

    if (result->index > result->len) {
        return FALSE;
    }

    result->entry[result->index].label = alloc((u32) "ALLOC result->entry[i].label", strlen(label) + 1);
    if (result->entry[result->index].label == NULL) {
        goto err;
    }

    result->entry[result->index].value = alloc((u32) "ALLOC result->entry[i].value", strlen(value) + 1);
    if (result->entry[result->index].value == NULL) {
        goto err;
    }

    strcpy(result->entry[result->index].label, label);
    strcpy(result->entry[result->index].value, value);

    result->index++;
    return TRUE;

err:
    if (result->entry[result->index].label != NULL) {
        free((u32) "FREE result->entry[i].label", result->entry[result->index].label, 0);
        result->entry[result->index].label = NULL;
    }

    if (result->entry[result->index].value != NULL) {
        free((u32) "FREE result->entry[i].value", result->entry[result->index].value, 0);
        result->entry[result->index].value = NULL;
    }

    return FALSE;
}

BOOL DWC_Http_ParseResult(DWCHttp *http, BOOL noparsebody)
{
    char *p_char1, *p_char2;
    char p_char1_temp, p_char2_temp;

    DWCHttpParseResult parse_result;

    parse_result.entry = http->labelvalue;
    parse_result.len = DWCHTTP_MAX_LABELVALUE;
    parse_result.index = 0;
    MI_CpuClear8(http->labelvalue, sizeof(DWCHttpLabelValue) * DWCHTTP_MAX_LABELVALUE);

    char *buffer = http->rep.buffer;
    char *httpbody_top = strstr(buffer, "\r\n\r\n");
    if (httpbody_top == NULL) {
        return FALSE;
    }

    httpbody_top += 4;
    char *httpbody_end = httpbody_top + strlen(httpbody_top);

    p_char1 = strstr(buffer, " ");
    if (p_char1 == NULL) {
        return FALSE;
    }
    char *value = p_char1 + 1;

    char temp = value[3];
    value[3] = '\0';

    if (DWCi_Http_AddResult(http, &parse_result, "httpresult", value) != TRUE) {
        value[3] = temp;
        return FALSE;
    }
    value[3] = temp;

    if (noparsebody == TRUE || strncmp(value, "200", 3) != 0) {
        return TRUE;
    }

    p_char1 = strstr(value + 4, "\r\n");

    if (p_char1 == NULL) {
        return FALSE;
    }

    char *index = p_char1 + 2;

    while (index[0] != '\r' && index[1] != '\n') {
        char *label = index;

        p_char1 = strstr(index, ": ");
        if (p_char1 == NULL) {
            break;
        }
        p_char1_temp = *p_char1;
        *p_char1 = '\0';

        value = p_char1 + 2;
        p_char2 = strstr(value, "\r\n");
        if (p_char2 == NULL) {
            *p_char1 = p_char1_temp;
            break;
        }
        p_char2_temp = *p_char2;
        *p_char2 = '\0';

        if (DWCi_Http_AddResult(http, &parse_result, label, value) != TRUE) {
            *p_char1 = p_char1_temp;
            *p_char2 = p_char2_temp;
            return FALSE;
        }

        index = value + strlen(value) + 2;
        *p_char1 = p_char1_temp;
        *p_char2 = p_char2_temp;
    }

    index = httpbody_top;

    while (index < httpbody_end) {
        char *label = index;
        p_char1 = strstr(label, "=");
        if (p_char1 == NULL) {
            break;
        }

        p_char1_temp = *p_char1;
        *p_char1 = '\0';

        value = p_char1 + 1;
        p_char2 = strstr(value, "&");
        if (p_char2 == NULL) {
            p_char2 = strstr(value, "\r\n");
        }

        if (p_char2 != NULL) {
            p_char2_temp = *p_char2;
            *p_char2 = '\0';
        }

        if (DWCi_Http_AddResult(http, &parse_result, label, value) != TRUE) {
            *p_char1 = p_char1_temp;
            if (p_char2 != NULL) {
                *p_char2 = p_char2_temp;
            }
            return FALSE;
        }

        index = value + strlen(value) + 1;
        *p_char1 = p_char1_temp;
        if (p_char2 != NULL) {
            *p_char2 = p_char2_temp;
        }
    }

    return TRUE;
}

char *DWC_Http_GetResult(DWCHttp *http, char *label)
{
    for (int i = 0; i < DWCHTTP_MAX_LABELVALUE; i++) {
        if (http->labelvalue[i].label == NULL) {
            break;
        }

        if (strcmp(label, http->labelvalue[i].label) == 0) {
            return http->labelvalue[i].value;
        }
    }

    return NULL;
}

int DWC_Http_GetBase64DecodedResult(DWCHttp *http, char *label, char *buffer, u32 buffer_len)
{
    char *value = DWC_Http_GetResult(http, label);
    if (value == NULL) {
        return 0;
    }

    int result_len = DWC_Auth_Base64Decode(value, strlen(value), buffer, buffer_len);
    if (result_len == -1 || result_len >= buffer_len) {
        return result_len;
    }

    buffer[result_len] = '\0';

    return result_len;
}

BOOL DWC_Http_GetRawResult(DWCHttp *http, char *label, char *buffer, int buffer_len)
{
    char *value = DWC_Http_GetResult(http, label);
    if (value == NULL) {
        return FALSE;
    }

    int value_len = strlen(value);
    if (value_len >= buffer_len) {
        return FALSE;
    }

    strcpy(buffer, value);
    return TRUE;
}

static void DWCi_Http_DestroyResult(DWCHttp *http, DWCHttpLabelValue *array_entry, int array_len)
{
    DWCAuthFree free = http->param.free;

    for (int i = 0; i < array_len; i++) {
        if (array_entry[i].label != NULL) {
            free((u32) "FREE array_entry[i].label", array_entry[i].label, 0);
            array_entry[i].label = NULL;
        }

        if (array_entry[i].value != NULL) {
            free((u32) "FREE array_entry[i].value", array_entry[i].value, 0);
            array_entry[i].value = NULL;
        }
    }
}
