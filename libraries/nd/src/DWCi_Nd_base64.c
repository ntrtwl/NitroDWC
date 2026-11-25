#include "DWCi_Nd_base64.h"

#include <nitro.h>

#include "nd/DWCi_Nd.h"

static const char szBASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-";

static int getNchar(u8 *dst, char *p, int n)
{
    int i;
    int len = 0;

    for (i = 0; i < n; i++) {
        if (*p != '\0') {
            len++;
            *dst++ = *p++;
        } else {
            *dst++ = '\0';
        }
    }

    return len;
}

static int decodeChar(u8 c)
{
    if ('A' <= c && c <= 'Z') {
        return c - 'A';
    } else if ('a' <= c && c <= 'z') {
        return c - 'a' + 26;
    } else if ('0' <= c && c <= '9') {
        return c - '0' + 52;
    } else if (c == '.') {
        return 62;
    } else if (c == '-') {
        return 63;
    } else {
        return -1;
    }
}

int DWCi_Ndi_getDecodeLenBASE64(int len)
{
    return (len >> 2) * 3;
}

int DWCi_Ndi_getEncodeLenBASE64(int len)
{
    return (len + 2) / 3 * 4;
}

int DWCi_Ndi_encodeBASE64(char *encstr, char *srcbin, int length)
{
    u8 c[3];

    int len = 0;
    int n = getNchar(c, srcbin, length > 3 ? 3 : length);
    while (n > 0) {
        if (length < 3) {
            c[2] = 0;
        }
        if (length < 2) {
            c[1] = 0;
        }
        u8 base0 = c[0] >> 2;
        u8 base1 = ((c[0] << 4) | (c[1] >> 4)) & 63;
        u8 base2 = ((c[1] << 2) | (c[2] >> 6)) & 63;
        u8 base3 = c[2] & 63;
        *encstr++ = szBASE64[base0];
        *encstr++ = szBASE64[base1];
        *encstr++ = n > 1 ? szBASE64[base2] : '*';
        *encstr++ = n > 2 ? szBASE64[base3] : '*';

        srcbin += n;
        len += 4;
        length -= n;
        if (length == 0) {
            break;
        }

        n = getNchar(c, srcbin, length > 3 ? 3 : length);
    }

    *encstr = '\0';
    return len;
}

int DWCi_Ndi_decodeBASE64(char *decbin, char *str, int length)
{
    u8 c[4];
    char dec[3];
    int d;
    int n;

    int len = 0;
    n = getNchar(c, str, 4);
    d = 0;

    while (n > 0 && d >= 0) {
        dec[0] = 0;
        dec[1] = 0;
        dec[2] = 0;

        d = decodeChar(c[0]);
        if (d >= 0) {
            dec[0] = d << 2;
            d = decodeChar(c[1]);
            if (d >= 0) {
                dec[0] |= d >> 4;
                dec[1] = d << 4;
                d = decodeChar(c[2]);
                if (d >= 0) {
                    dec[1] |= d >> 2;
                    dec[2] = d << 6;
                    d = decodeChar(c[3]);
                    if (d >= 0) {
                        dec[2] |= d;
                    }
                }
            }
        }

        for (int i = 0; i < n - 1; i++) {
            *decbin++ = dec[i];
        }

        str += n;
        len += n - 1;
        length -= 4;
        if (length <= 0) {
            break;
        }

        n = getNchar(c, str, 4);
    }

    return len;
}
