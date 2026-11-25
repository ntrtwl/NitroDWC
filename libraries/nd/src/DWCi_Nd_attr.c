#include "DWCi_Nd_attr.h"

#include <nitro.h>

#include "nd/DWCi_Nd.h"

#include "DWCi_Nd_base64.h"
#include "DWCi_Nd_os.h"
#include "DWCi_Nd_stdlib.h"

extern DWCNdAlloc DWCi_Ndi_alloc;
extern DWCNdError DWCi_Ndi_error;
extern DWCNdFree DWCi_Ndi_free;
extern char *DWCi_Ndi_dlattr[3];

void DWCi_Ndi_freeAttr(void)
{
    for (int i = 0; i < 3; i++) {
        if (DWCi_Ndi_dlattr[i] != NULL) {
            DWCi_Ndi_free(DWCi_Ndi_dlattr[i]);
            DWCi_Ndi_dlattr[i] = NULL;
        }
    }
}

static BOOL setAttr(int id, char *attr)
{
    if (*attr != '\0') {
        int n = DWCi_Ndi_strlen(attr);
        if (n > DWC_ND_FILEATTR_LEN) {
            DWCi_Ndi_error = DWC_ND_ERROR_PARAM;
            return FALSE;
        }

        n = DWCi_Ndi_strlen(attr);
        if (n > DWC_ND_FILEATTR_LEN) {
            DWCi_Ndi_error = DWC_ND_ERROR_PARAM;
            return FALSE;
        }

        int encLen = DWCi_Ndi_getEncodeLenBASE64(n);
        DWCi_Ndi_dlattr[id] = DWCi_Ndi_alloc(encLen + 1, 4);
        if (DWCi_Ndi_dlattr[id] == NULL) {
            DWCi_Ndi_error = DWC_ND_ERROR_ALLOC;
            return FALSE;
        }

        n = DWCi_Ndi_encodeBASE64(DWCi_Ndi_dlattr[id], attr, n);
        DWCi_Ndi_dlattr[id][n] = '\0';
    }

    return TRUE;
}

BOOL DWCi_NdSetAttr(char *attr1, char *attr2, char *attr3)
{
    DWCi_Ndi_lockGlobal();
    DWCi_Ndi_freeAttr();
    if (setAttr(0, attr1) && setAttr(1, attr2) && setAttr(2, attr3)) {
        DWCi_Ndi_unlockGlobal();
        return TRUE;
    } else {
        DWCi_Ndi_freeAttr();
        DWCi_Ndi_unlockGlobal();
        return FALSE;
    }
}
