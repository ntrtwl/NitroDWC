#ifndef DWC_AC_SEARCH_H
#define DWC_AC_SEARCH_H

#include <nitro.h>

u8 DWCi_AC_SearchAP(void);
void DWCi_AC_SetStealthChannel(u16 channel);
s8 DWCi_AC_GetStealthChannel(u16 no);
void DWCi_AC_SearchRestart(u8 phase);

#endif // DWC_AC_SEARCH_H
