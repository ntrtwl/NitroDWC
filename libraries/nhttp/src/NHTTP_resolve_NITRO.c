#include "NHTTP_resolve.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "nhttp/NHTTP.h"

u32 NHTTPi_resolveHostname(NHTTPReq *req)
{
    SOCInAddr socaddr;
    u32 addr;

    char savchr = req->pURL[req->nLenHost];
    req->pURL[req->nLenHost] = '\0';

    char *hostname = &req->pURL[req->isSSL ? 8 : 7];
    if (SOC_InetAtoN(hostname, &socaddr)) {
        addr = socaddr.addr;
    } else {
        u32 h = SOCL_Resolve(hostname);
        if (h == 0) {
            return 0;
        }
        addr = SOC_HtoNl(h);
    }

    req->pURL[req->nLenHost] = savchr;
    return addr;
}
