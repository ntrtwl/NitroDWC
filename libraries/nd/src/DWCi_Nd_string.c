#include "DWCi_Nd_string.h"

#include <nhttp/NHTTP.h>
#include <nitro.h>

#include "nd/DWCi_Nd.h"

int DWCi_Ndi_strToInt(char *p, int len)
{
    return NHTTPi_strToInt(p, len);
}

int DWCi_Ndi_intToStr(char *p, u32 n)
{
    return NHTTPi_intToStr(p, n);
}

int DWCi_Ndi_intToStrFigure(char *p, u64 n, int figure)
{
    static const u64 rdx[16] = {
        10000000000000000,
        1000000000000000,
        100000000000000,
        10000000000000,
        1000000000000,
        100000000000,
        10000000000,
        1000000000,
        100000000,
        10000000,
        1000000,
        100000,
        10000,
        1000,
        100,
        10
    };
    int i;
    BOOL b;
    int len = 0;

    if (n >= 100000000000000000) {
        return -1;
    }

    for (i = 0, b = FALSE; i < 16; i++) {
        if (n >= rdx[i]) {
            u32 x = n / rdx[i];
            n -= x * rdx[i];
            b = TRUE;
            p[len++] = x + '0';
        } else if (b || 17 - i <= figure) {
            p[len++] = '0';
        }
    }

    p[len++] = n + '0';
    return len;
}

int DWCi_Ndi_hexToStrFigure(char *p, u32 n, int figure)
{
    int len = 0;

    if (figure > 8) {
        return -1;
    }

    for (int i = 1; i <= figure; i++) {
        u32 x = (n >> ((figure - i) * 4)) & 0xF;
        if (x < 10) {
            p[len++] = x + '0';
        } else {
            p[len++] = x - 10 + 'a';
        }
    }

    return len;
}
