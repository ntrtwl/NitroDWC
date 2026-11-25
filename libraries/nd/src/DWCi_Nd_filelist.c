#include "DWCi_Nd_filelist.h"

#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_lib.h"
#include "DWCi_Nd_stdlib.h"
#include "DWCi_Nd_string.h"

BOOL DWCi_Ndi_response_list(NHTTPRes *res, DWCi_Ndi_HTTPREQ *hrq)
{
    char *body;
    BOOL isComplete = FALSE;
    char *contentlen;

    DWCi_Ndi_memclr(hrq->param.list.filelist, hrq->param.list.intnum * sizeof(DWCNdFileInfo));

    if (NHTTP_GetHeaderField(res, "Content-Length", &contentlen) > 0) {
        if (contentlen[0] == '0' && contentlen[1] == '\0') {
            return TRUE;
        }
    }

    int bodyLen = NHTTP_GetBodyAll(res, &body);
    if (bodyLen > 0) {
        for (int p = 0, n = 0; n < hrq->param.list.intnum; n++) {
            if (p >= bodyLen) {
                isComplete = TRUE;
                break;
            }

            int state = 0;
            int bgn = p;
            DWCNdFileInfo *pfi = &hrq->param.list.filelist[n];
            BOOL foundCR = FALSE;
            isComplete = FALSE;

            for (; p < bodyLen; p++) {
                if (body[p] == '\r') {
                    foundCR = TRUE;
                    continue;
                }

                if (foundCR) {
                    if (body[p] == '\n') {
                        if (state == 5) {
                            if (p - 1 - bgn > 8) {
                                goto end;
                            }
                            int size = DWCi_Ndi_strToInt(body + bgn, p - 1 - bgn);
                            if (size < 0) {
                                goto end;
                            }
                            pfi->size = size;
                        }

                        p++;
                        isComplete = state >= 5;
                        break;
                    }
                } else if (body[p] == '\t') {
                    switch (state) {
                    case 0:
                        if ((u32)(p - bgn) > DWC_ND_FILENAME_LEN) {
                            goto end;
                        }
                        DWCi_Ndi_memcpy(pfi->name, body + bgn, p - bgn);
                        break;
                    case 1:
                        if (DWCi_Ndi_getDecodeLenBASE64(p - bgn) > sizeof(pfi->explain)) {
                            goto end;
                        }
                        DWCi_Ndi_decodeBASE64(pfi->explain, body + bgn, p - bgn);
                        pfi->explain[DWC_ND_FILEEXPLAIN_LEN] = '\0';
                        break;
                    case 2:
                        if ((u32)(p - bgn) > DWC_ND_FILEATTR_LEN) {
                            goto end;
                        }
                        DWCi_Ndi_memcpy(pfi->param1, body + bgn, p - bgn);
                        break;
                    case 3:
                        if ((u32)(p - bgn) > DWC_ND_FILEATTR_LEN) {
                            goto end;
                        }
                        DWCi_Ndi_memcpy(pfi->param2, body + bgn, p - bgn);
                        break;
                    case 4:
                        if ((u32)(p - bgn) > DWC_ND_FILEATTR_LEN) {
                            goto end;
                        }
                        DWCi_Ndi_memcpy(pfi->param3, body + bgn, p - bgn);
                        break;
                    case 5:
                        if (p - bgn > 8) {
                            goto end;
                        }
                        int size = DWCi_Ndi_strToInt(body + bgn, p - bgn);
                        if (size < 0) {
                            goto end;
                        }
                        pfi->size = size;
                        break;
                    default:
                        break;
                    }

                    state++;
                    bgn = p + 1;
                }

                foundCR = FALSE;
            }
        }
    }
end:
    return isComplete;
}
