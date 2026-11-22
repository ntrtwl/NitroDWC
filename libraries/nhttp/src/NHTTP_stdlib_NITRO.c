#include "NHTTP_stdlib.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"


int NHTTPi_strcmp(const char *p1, const char *p2)
{
    return STD_StrCmp(p1, p2);
}

int NHTTPi_stricmp(const char *p1, const char *p2)
{
    return NHTTPi_strnicmp(p1, p2, -1);
}

int NHTTPi_strncmp(const char *p1, const char *p2, int size)
{
    return STD_StrNCmp(p1, p2, size);
}

int NHTTPi_strnicmp(const char *p1, const char *p2, int size)
{
    char c1, c2;

    for (; size > 0; size--) {
        c1 = *p1++;
        c2 = *p2++;

        if (c1 == '\0' || c2 == '\0') {
            if (c1 == '\0' && c2 == '\0') {
                size = 0;
            }
            break;
        }

        if (toLower(c1) != toLower(c2)) {
            break;
        }
    }

    return size;
}

int NHTTPi_strlen(const char *p)
{
    return STD_StrLen(p);
}

void NHTTPi_memclr(void *p, int size)
{
    MI_CpuClear8(p, size);
}

void NHTTPi_memcpy(void *dst, const void *src, int size)
{
    MI_CpuCopy8(src, dst, size);
}
