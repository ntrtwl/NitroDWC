#include "DWCi_Nd_stdlib.h"

#include <nitro.h>

#include "nd/DWCi_Nd.h"

int DWCi_Ndi_strlen(const char *p)
{
    return STD_StrLen(p);
}

int DWCi_Ndi_strcmp(const char *p1, const char *p2)
{
    return STD_StrCmp(p1, p2);
}

int DWCi_Ndi_strncmp(const char *p1, const char *p2, int size)
{
    return STD_StrNCmp(p1, p2, size);
}

void DWCi_Ndi_memclr(void *p, int size)
{
    MI_CpuClear8(p, size);
}

void DWCi_Ndi_memcpy(void *dst, const void *src, int size)
{
    MI_CpuCopy8(src, dst, size);
}
