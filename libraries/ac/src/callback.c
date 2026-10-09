#include "callback.h"

#include <nitroWiFi.h>
#include <nitro_wl/ARM7/WlCmdLabel.h>

#include "ac.h"
#include "beacon.h"

void DWCi_AC_WCMCallback(WCMNotice *notice)
{
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    if (notice->notify == WCM_NOTIFY_CONNECT) {
        if (notice->result != WCM_RESULT_SUCCESS) {
            switch (notice->parameter[1].n) {
            case WL_CMDLABEL_STS_NOT_SUPPORT_AUTH_ALGORITHM:
                wk->connectResult = AC_CONNECT_FAILURE_AUTH;
                break;
            case WL_CMDLABEL_STS_CHALLENGE_FAILURE:
                wk->connectResult = AC_CONNECT_FAILURE_WEP;
                break;
            case WL_CMDLABEL_STS_ASS_UNABLE_HANDLE:
                wk->connectResult = AC_CONNECT_FAILURE_CAPACITY;
                break;
            default:
                wk->connectResult = AC_CONNECT_FAILURE;
                break;
            }
        }
    } else if (notice->notify == WCM_NOTIFY_FOUND_AP) {
        DWCi_AC_GetBeacon((WMBssDesc *)notice->parameter[0].p);
    }
}
