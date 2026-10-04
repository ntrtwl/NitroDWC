#include "start.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "ac.h"
#include "callback.h"
#include "makelist.h"

u8 DWCi_AC_Start(void)
{
    int phase = WCM_GetPhase();
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);

    if (phase == WCM_PHASE_WAIT) {
        WCMConfig config;
        config.dmano = wk->dmaNo;
        config.pbdbuffer = NULL;
        config.nbdbuffer = 0;
        config.nbdmode = WCM_APLIST_MODE_IGNORE;

        DWCi_AC_MakeSearchList(SEARCH_LIST_TYPE_AROUND);
        int result = WCM_StartupAsync(&config, DWCi_AC_WCMCallback);
        if (result == WCM_RESULT_FAILURE || result >= WCM_RESULT_REJECT) {
            DWCi_AC_SetError(AC_ERROR_WCM_STARTUP);
            return AC_PHASE_ERROR;
        }
    } else {
        return AC_PHASE_START;
    }

    return AC_PHASE_SEARCH_START;
}
