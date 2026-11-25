#include "DWCi_Nd_filecount.h"

#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_lib.h"
#include "DWCi_Nd_string.h"

BOOL DWCi_Ndi_response_count(NHTTPRes *res, DWCi_Ndi_HTTPREQ *hrq)
{
    char *body;
    int bodyLen = NHTTP_GetBodyAll(res, &body);

    if (bodyLen >= 0) {
        *hrq->param.count.entrynum = DWCi_Ndi_strToInt(body, bodyLen);
        if (*hrq->param.count.entrynum < 0) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    return TRUE;
}
