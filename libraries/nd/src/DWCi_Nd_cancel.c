#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_os.h"

extern int DWCi_Ndi_nhttpid;

BOOL DWCi_NdCancelAsync(void)
{
    if (!DWCi_Ndi_prepareHttpCallback()) {
        DWCi_Ndi_lockGlobal();
        int id = DWCi_Ndi_nhttpid;
        DWCi_Ndi_unlockGlobal();
        BOOL rc = NHTTP_CancelRequestAsync(id);
        return rc;
    }

    DWCi_Ndi_unprepareHttpCallback();
    return FALSE;
}
