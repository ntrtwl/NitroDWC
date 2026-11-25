#include <nhttp/NHTTP.h>
#include <nitro.h>
#include <nitroWiFi.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_DWC.h"
#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_filecount.h"
#include "DWCi_Nd_fileget.h"
#include "DWCi_Nd_filelist.h"
#include "DWCi_Nd_lib.h"
#include "DWCi_Nd_os.h"
#include "DWCi_Nd_stdlib.h"
#include "DWCi_Nd_string.h"

extern BOOL DWCi_Ndi_isExistApinfo;
extern char *DWCi_Ndi_authtoken;
extern DWCNdAlloc DWCi_Ndi_alloc;
extern char *DWCi_Ndi_serverURL;
extern DWCNdCallback DWCi_Ndi_dlcallback;
extern DWCNdError DWCi_Ndi_error;
extern DWCNdFree DWCi_Ndi_free;
extern int DWCi_Ndi_nhttpid;
extern char *DWCi_Ndi_dlattr[3];
extern char DWCi_Ndi_dwcgamecd[9];
extern char DWCi_Ndi_macaddr[17];
extern char DWCi_Ndi_strApinfo[21];
extern char DWCi_Ndi_password[25];
extern char DWCi_Ndi_userAgent[33];

extern SOCCaInfo dwci_nd_ca_nasself;

static char rhgamecode[9];
static char userid[21];

static SOCCaInfo *ca_array[] = {
    &dwci_nd_ca_nasself
};

static const char DWCiNd_action_list[] = "bGlzdA**";
static const char DWCiNd_action_count[] = "Y291bnQ*";
static const char DWCiNd_action_contents[] = "Y29udGVudHM*";

static char listnumRecvbuf[DWC_ND_RECVBUF_MIN];

static void req_callback(NHTTPError error, NHTTPRes *res, void *param)
{
    DWCi_Ndi_HTTPREQ *hrq = param;
    DWCNdCallbackReason reason = hrq->reason;
    DWCNdCallback callback = hrq->callback;
    THREADEVENTDESC *threadEvent = hrq->threadEvent;
    int httpStatus = 0;
    int serverError = -1;
    DWCNdError result;
    char *strHttpStatus;

    switch (error) {
    case NHTTP_ERROR_NONE:
        httpStatus = 500;
        if (NHTTP_GetHeaderField(res, "HTTPSTATUSCODE", &strHttpStatus) != 3) {
            result = DWC_ND_ERROR_HTTP;
        } else {
            httpStatus = DWCi_Ndi_strToInt(strHttpStatus, 3);
            if (httpStatus == 302) {
                char *v;
                char outstr[4];
                BOOL isCode = FALSE;

                int n = NHTTP_GetBodyAll(res, &v);
                if (n < 0) {
                    result = DWC_ND_ERROR_HTTP;
                } else {
                    for (int pos = 0; pos < n;) {
                        int bgn = pos;
                        while (v[pos] != '=' && pos < n) {
                            pos++;
                        }

                        int len = pos - bgn;
                        pos++;
                        if (len == 8) {
                            isCode = DWCi_Ndi_strncmp(&v[bgn], "returncd", 8) == 0;
                        }

                        bgn = pos;
                        while (v[pos] != '&' && v[pos] != '\r' && v[pos] != '\0' && pos < n) {
                            pos++;
                        }

                        len = pos - bgn;
                        pos++;
                        if (isCode) {
                            if (len == 4) {
                                len = DWCi_Ndi_decodeBASE64(outstr, &v[bgn], len);
                                outstr[len] = '\0';
                                serverError = DWCi_Ndi_strToInt(outstr, len);
                            }
                            break;
                        }
                    }

                    if (serverError < 0) {
                        result = DWC_ND_ERROR_HTTP;
                    } else {
                        result = DWC_ND_ERROR_DLSERVER;
                    }
                }
            } else if (httpStatus <= 0 || httpStatus != 200) {
                result = DWC_ND_ERROR_HTTP;
            } else {
                switch (reason) {
                case DWC_ND_CBREASON_GETFILELISTNUM:
                    if (DWCi_Ndi_response_count(res, param)) {
                        result = DWC_ND_ERROR_NONE;
                    } else {
                        result = DWC_ND_ERROR_HTTP;
                    }
                    break;
                case DWC_ND_CBREASON_GETFILELIST:
                    if (DWCi_Ndi_response_list(res, param)) {
                        result = DWC_ND_ERROR_NONE;
                    } else {
                        result = DWC_ND_ERROR_HTTP;
                    }
                    break;
                case DWC_ND_CBREASON_GETFILE:
                    if (DWCi_Ndi_response_contents(res, param)) {
                        result = DWC_ND_ERROR_NONE;
                    } else {
                        result = DWC_ND_ERROR_HTTP;
                    }
                    break;
                }
            }
        }
        break;
    case NHTTP_ERROR_CANCELED:
        result = DWC_ND_ERROR_CANCELED;
        break;
    case NHTTP_ERROR_BUFFULL:
        result = DWC_ND_ERROR_BUFFULL;
        break;
    default:
        result = DWC_ND_ERROR_HTTP;
        break;
    }

    switch (reason) {
    case DWC_ND_CBREASON_GETFILELISTNUM:
        break;
    case DWC_ND_CBREASON_GETFILELIST:
        if (hrq->param.list.num != NULL) {
            DWCi_Ndi_free(hrq->param.list.num);
        }
        if (hrq->param.list.offset != NULL) {
            DWCi_Ndi_free(hrq->param.list.offset);
        }
        DWCi_Ndi_free(hrq->param.list.recvbuf);
        break;
    case DWC_ND_CBREASON_GETFILE:
        DWCi_Ndi_free(hrq->param.file.filename);
        break;
    }

    DWCi_Ndi_lockGlobal();
    DWCi_Ndi_nhttpid = -1;
    DWCi_Ndi_unlockGlobal();

    NHTTP_DestroyResponse(res);
    DWCi_Ndi_free(param);
    DWCi_Ndi_unprepareHttpCallback();

    if (result != DWC_ND_ERROR_NONE) {
        DWCi_Ndi_error = result;
    }
    if (callback != NULL) {
        callback(reason, result, serverError);
    }
    if (threadEvent != NULL) {
        DWCi_Ndi_kickThread(threadEvent);
    }
}

static void freeHttp(DWCi_Ndi_HTTPREQ *hrq, NHTTPReq *req)
{
    NHTTP_DestroyRequest(req);
    DWCi_Ndi_unprepareHttpCallback();
    DWCi_Ndi_free(hrq);
}

static BOOL setupHttp(DWCi_Ndi_HTTPREQ **phrq, NHTTPReq **preq, char *strAction, char *recvbuf, u32 recvbufLen)
{
    if (!DWCi_Ndi_getDSdesc(rhgamecode, userid)) {
        DWCi_Ndi_error = DWC_ND_ERROR_FATAL;
        return FALSE;
    }

    DWCi_Ndi_HTTPREQ *hrq = DWCi_Ndi_alloc(sizeof(DWCi_Ndi_HTTPREQ), 4);
    if (hrq == NULL) {
        DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
        return FALSE;
    }

    hrq->callback = DWCi_Ndi_dlcallback;
    hrq->threadEvent = NULL;

    if (!DWCi_Ndi_prepareHttpCallback()) {
        DWCi_Ndi_free(hrq);
        DWCi_Ndi_error = DWC_ND_ERROR_BUSY;
        return FALSE;
    }

    NHTTPReq *req = NHTTP_CreateRequest(DWCi_Ndi_serverURL, NHTTP_REQMETHOD_POST, recvbuf, recvbufLen, req_callback, hrq);
    if (req == NULL) {
        DWCi_Ndi_unprepareHttpCallback();
        DWCi_Ndi_free(hrq);
        DWCi_Ndi_error = DWC_ND_ERROR_HTTP;
        return FALSE;
    }

    *phrq = hrq;
    *preq = req;

    if (!NHTTP_SetCAChain(req, ca_array, 1)) {
        goto err;
    }
    if (!NHTTP_AddHeaderField(req, "User-Agent", DWCi_Ndi_userAgent)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "gamecd", DWCi_Ndi_dwcgamecd)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "rhgamecd", rhgamecode)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "passwd", DWCi_Ndi_password)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "token", DWCi_Ndi_authtoken)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "userid", userid)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "macadr", DWCi_Ndi_macaddr)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "action", strAction)) {
        goto err;
    }
    if (DWCi_Ndi_dlattr[0] != NULL) {
        if (!NHTTP_AddPostDataAscii(req, "attr1", DWCi_Ndi_dlattr[0])) {
            goto err;
        }
    }
    if (DWCi_Ndi_dlattr[1] != NULL) {
        if (!NHTTP_AddPostDataAscii(req, "attr2", DWCi_Ndi_dlattr[1])) {
            goto err;
        }
    }
    if (DWCi_Ndi_dlattr[2] != NULL) {
        if (!NHTTP_AddPostDataAscii(req, "attr3", DWCi_Ndi_dlattr[2])) {
            goto err;
        }
    }
    if (DWCi_Ndi_isExistApinfo) {
        if (!NHTTP_AddPostDataAscii(req, "apinfo", DWCi_Ndi_strApinfo)) {
            goto err;
        }
    }

    return TRUE;
err:
    freeHttp(hrq, req);
    DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
    return FALSE;
}

static BOOL runHttp(DWCi_Ndi_HTTPREQ *hrq, NHTTPReq *req)
{
    DWCi_Ndi_lockGlobal();
    hrq->id = NHTTP_SendRequestAsync(req);
    DWCi_Ndi_nhttpid = hrq->id;
    DWCi_Ndi_unlockGlobal();

    if (hrq->id < 0) {
        freeHttp(hrq, req);
        DWCi_Ndi_error = DWC_ND_ERROR_HTTP;
        return FALSE;
    } else {
        return TRUE;
    }
}

BOOL DWCi_NdGetFileListNumAsync(int *entrynum)
{
    DWCi_Ndi_HTTPREQ *hrq;
    NHTTPReq *req;

    if (!setupHttp(&hrq, &req, DWCiNd_action_count, listnumRecvbuf, DWC_ND_RECVBUF_MIN)) {
        return FALSE;
    }

    hrq->reason = DWC_ND_CBREASON_GETFILELISTNUM;
    hrq->param.count.entrynum = entrynum;

    if (!runHttp(hrq, req)) {
        return FALSE;
    }

    return TRUE;
}

BOOL DWCi_NdGetFileListAsync(DWCNdFileInfo *filelist, int offset, int num)
{
    DWCi_Ndi_HTTPREQ *hrq;
    NHTTPReq *req;
    char strBuf[12];

    int lenBuf = num * DWC_ND_LISTBUFFER_PERNUM;
    if (lenBuf < DWC_ND_RECVBUF_MIN) {
        lenBuf = DWC_ND_RECVBUF_MIN;
    }

    char *pBuf = DWCi_Ndi_alloc(lenBuf, 4);
    if (pBuf == NULL) {
        DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
        return FALSE;
    }

    int lenOffset = DWCi_Ndi_intToStr(strBuf, offset);

    char *strOffset = DWCi_Ndi_alloc(DWCi_Ndi_getEncodeLenBASE64(lenOffset) + 1, 4);
    if (strOffset == NULL) {
        DWCi_Ndi_free(pBuf);
        DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
        return FALSE;
    }

    int n = DWCi_Ndi_encodeBASE64(strOffset, strBuf, lenOffset);
    strOffset[n] = '\0';

    int lenNum = DWCi_Ndi_intToStr(strBuf, num);

    char *strNum = DWCi_Ndi_alloc(DWCi_Ndi_getEncodeLenBASE64(lenNum) + 1, 4);
    if (strNum == NULL) {
        DWCi_Ndi_free(strOffset);
        DWCi_Ndi_free(pBuf);
        DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
        return FALSE;
    }

    n = DWCi_Ndi_encodeBASE64(strNum, strBuf, lenNum);
    strNum[n] = '\0';

    if (!setupHttp(&hrq, &req, DWCiNd_action_list, pBuf, lenBuf)) {
        DWCi_Ndi_free(strNum);
        DWCi_Ndi_free(strOffset);
        DWCi_Ndi_free(pBuf);
        return FALSE;
    }

    if (!NHTTP_AddPostDataAscii(req, "offset", strOffset)) {
        goto err;
    }
    if (!NHTTP_AddPostDataAscii(req, "num", strNum)) {
        goto err;
    }

    hrq->reason = DWC_ND_CBREASON_GETFILELIST;
    hrq->param.list.filelist = filelist;
    hrq->param.list.offset = strOffset;
    hrq->param.list.num = strNum;
    hrq->param.list.intnum = num;
    hrq->param.list.recvbuf = pBuf;

    if (!runHttp(hrq, req)) {
        DWCi_Ndi_free(strNum);
        DWCi_Ndi_free(strOffset);
        DWCi_Ndi_free(pBuf);
        return FALSE;
    }

    return TRUE;
err:
    DWCi_Ndi_free(strNum);
    DWCi_Ndi_free(strOffset);
    DWCi_Ndi_free(pBuf);
    freeHttp(hrq, req);
    DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
    return FALSE;
}

BOOL DWCi_NdGetFileAsync(DWCNdFileInfo *filelist, char *buf, u32 bufsize)
{
    DWCi_Ndi_HTTPREQ *hrq;
    NHTTPReq *req;

    if (bufsize < filelist->size) {
        DWCi_Ndi_error = DWC_ND_ERROR_PARAM;
        return FALSE;
    }

    int fnameLen = DWCi_Ndi_strlen(filelist->name);
    if (fnameLen > DWC_ND_FILENAME_LEN) {
        DWCi_Ndi_error = DWC_ND_ERROR_PARAM;
        return FALSE;
    }

    int n = fnameLen > 0 ? DWCi_Ndi_getEncodeLenBASE64(fnameLen) : 0;

    char *encName = DWCi_Ndi_alloc(n + 1, 4);
    if (encName == NULL) {
        DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
        return FALSE;
    }

    if (!setupHttp(&hrq, &req, DWCiNd_action_contents, buf, filelist->size)) {
        DWCi_Ndi_free(encName);
        return FALSE;
    }

    if (n > 0) {
        encName[DWCi_Ndi_encodeBASE64(encName, filelist->name, fnameLen)] = '\0';
        if (!NHTTP_AddPostDataAscii(req, "contents", encName)) {
            goto err;
        }
    }

    hrq->reason = DWC_ND_CBREASON_GETFILE;
    hrq->param.file.filename = encName;
    hrq->param.file.filesize = filelist->size;
    if (!runHttp(hrq, req)) {
        DWCi_Ndi_free(encName);
        return FALSE;
    }
    return TRUE;
err:
    freeHttp(hrq, req);
    DWCi_Ndi_free(encName);
    DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
    return FALSE;
}

BOOL DWCi_NdGetProgress(u32 *received, u32 *contentlen)
{
    return NHTTP_GetProgress(received, contentlen);
}
