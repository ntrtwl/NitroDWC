#ifndef ND_STRING
#define ND_STRING

#include "nd/DWCi_Nd.h"

int DWCi_Ndi_strToInt(char *p, int len);
int DWCi_Ndi_intToStr(char *p, u32 n);
int DWCi_Ndi_intToStrFigure(char *p, u64 n, int figure);
int DWCi_Ndi_hexToStrFigure(char *p, u32 n, int figure);

#endif // ND_STRING
