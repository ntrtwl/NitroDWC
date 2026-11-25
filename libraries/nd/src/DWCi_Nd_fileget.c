#include "DWCi_Nd_fileget.h"

#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_lib.h"

BOOL DWCi_Ndi_response_contents(NHTTPRes *res, DWCi_Ndi_HTTPREQ *hrq)
{
    char *body;
    int bodyLen = NHTTP_GetBodyAll(res, &body);
    return bodyLen == hrq->param.file.filesize;
}
