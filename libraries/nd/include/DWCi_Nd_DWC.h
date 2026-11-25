#ifndef ND_DWC
#define ND_DWC

#include "nd/DWCi_Nd.h"

BOOL DWCi_Ndi_getApInfo(char *apinfo);
BOOL DWCi_Ndi_getDSdesc(char *gamecode, char *userid);
void DWCi_Ndi_getUserAgent(char *userAgent);

#endif // ND_DWC
