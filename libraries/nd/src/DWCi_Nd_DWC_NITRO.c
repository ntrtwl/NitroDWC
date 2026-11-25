#include "DWCi_Nd_DWC.h"

#include <base/dwc_connectinet.h>
#include <bm/dwc_backup.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_stdlib.h"
#include "DWCi_Nd_string.h"
#include "dwc_version.h"

BOOL DWCi_Ndi_getApInfo(char *apinfo)
{
    static DWCApInfo ap;
    char strAp[13] = "00:0000000-00";

    if (DWC_GetApInfo(&ap)) {
        if (0 <= ap.aptype && ap.aptype < 10) {
            strAp[1] = '0' + ap.aptype;
        } else if (10 <= ap.aptype && ap.aptype < DWC_APINFO_TYPE_UNKNOWN + 1) {
            DWCi_Ndi_intToStr(strAp, ap.aptype);
        }

        if (ap.aptype == DWC_APINFO_TYPE_SHOP || ap.aptype == DWC_APINFO_TYPE_NINTENDOSPOT) {
            if (0 <= ap.area && ap.area < 10) {
                strAp[3] = '0' + ap.area;
            }
            DWCi_Ndi_memcpy(&strAp[4], ap.spotinfo, DWC_SIZE_SPOTINFO);
        }

        DWCi_Ndi_encodeBASE64(apinfo, strAp, sizeof(strAp));

        return TRUE;
    }

    return FALSE;
}

#define HW_ROM_HEADER_GAME_CODE (HW_ROM_HEADER_BUF + 0xC)

BOOL DWCi_Ndi_getDSdesc(char *gamecode, char *userid)
{
    DWCWiFiInfo wifiinfo;
    char strId[13];

    DWCi_Ndi_memclr(&wifiinfo, sizeof(DWCWiFiInfo));
    DWCi_BM_GetWiFiInfo(&wifiinfo);

    if (wifiinfo.attestedUserId == 0) {
        return FALSE;
    }
    if (DWCi_Ndi_intToStrFigure(strId, wifiinfo.attestedUserId, sizeof(strId)) < 0) {
        return FALSE;
    }

    DWCi_Ndi_encodeBASE64(userid, strId, sizeof(strId));
    DWCi_Ndi_encodeBASE64(gamecode, (char *)HW_ROM_HEADER_GAME_CODE, DWC_ND_LENGTH_GAMECODE);

    return TRUE;
}

void DWCi_Ndi_getUserAgent(char *userAgent)
{
    OS_SNPrintf(userAgent, 33, "Nitro WiFi SDK/%d.%d", DWC_VERSION_MAJOR, DWC_VERSION_MINOR);
}
