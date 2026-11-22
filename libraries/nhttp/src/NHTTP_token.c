#include "NHTTP_token.h"

#include <nitro.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"

int NHTTPi_getUrlEncodedSize(char *p)
{
    int n;
    char c;

    for (n = 0, c = *p++; c != '\0'; c = *p++) {
        if (('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == ' ') {
            n += 1;
        } else {
            n += 3;
        }
    }

    return n;
}

int NHTTPi_encodeUrlChar(char *p, char c)
{
    int h, l;

    if (c == ' ') {
        p[0] = '+';
        return 1;
    }

    if (('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')) {
        p[0] = c;
        return 1;
    }

    h = (c >> 4) & 0xF;
    l = c & 0xF;
    p[0] = '%';
    p[1] = h < 10 ? h + '0' : h - 10 + 'A';
    p[2] = l < 10 ? l + '0' : l - 10 + 'A';

    return 3;
}

int NHTTPi_strToHex(char *p, int len)
{
    int i, n;
    BOOL e;

    if (len > 8) {
        return -1;
    }

    if (len == 8 && p[0] > '7') {
        return -1;
    }

    for (i = 0, n = 0, e = FALSE; i < len; i++) {
        char c = toLower(p[i]);
        if ('0' <= c && c <= '9') {
            n = (n << 4) + c - '0';
            e = TRUE;
        } else if ('a' <= c && c <= 'f') {
            n = (n << 4) + c - 'a' + 10;
            e = TRUE;
        } else if (e && (c == ' ' || c == '\0')) {
            break;
        } else if (!e && c == ' ') {
            continue;
        } else {
            return -1;
        }
    }

    return n;
}

int NHTTPi_strToInt(char *p, int len)
{
    int i, n;
    BOOL e;

    if (len > 10) {
        return -1;
    }

    for (i = 0, n = 0, e = FALSE; i < len; i++) {
        char c = p[i];
        if (e && (c == ' ' || c == '\0')) {
            break;
        } else if (!e && c == ' ') {
            continue;
        } else if (!(c >= '0' && c <= '9')) {
            return -1;
        }

        e = TRUE;
        int prev = n;
        n = 10 * n + c - '0';
        if (prev > n) {
            return -1;
        }
    }

    return n;
}

int NHTTPi_intToStr(char *p, u32 n)
{
    int i;
    BOOL b;
    u32 x;
    u32 rdx[9] = {
        1000000000,
        100000000,
        10000000,
        1000000,
        100000,
        10000,
        1000,
        100,
        10,
    };
    int len = 0;

    for (i = 0, b = FALSE; i < 9; i++) {
        if (n >= rdx[i]) {
            x = n / rdx[i];
            n -= x * rdx[i];
            b = TRUE;
            if (p != NULL) {
                p[len] = x + '0';
            }
            len++;
        } else if (b) {
            if (p != NULL) {
                p[len] = '0';
            }
            len++;
        }
    }

    if (p != NULL) {
        p[len] = n + '0';
    }
    len++;

    return len;
}

int NHTTPi_compareToken(char *p1, char *p2)
{
    for (; toLower(*p1) == toLower(*p2); p1++, p2++) {
        if (*p1 == '\0' || *p1 == ' ') {
            return 0;
        }
    }

    return -1;
}

int NHTTPi_strtonum(char *p, int size)
{
    int n, numFound;

    for (numFound = 0, n = 0; size--; p++) {
        if (*p == ' ') {
            continue;
        }

        if ('0' <= *p && *p <= '9') {
            n = n * 10 + *p - '0';
            if (++numFound > 9) {
                return -1;
            }
        }
    }

    return numFound ? n : -1;
}

int NHTTPi_memfind(char *p, int size, char *pattern, int patternsize)
{
    int i, j;
    char const *m;

    if (size < patternsize) {
        return -1;
    }

    for (i = 0; i < size - patternsize + 1; i++) {
        if (p[i] == pattern[0]) {
            m = &p[i];

            for (j = 1; j < patternsize; j++) {
                if (m[j] != pattern[j]) {
                    break;
                }
            }

            if (j == patternsize) {
                return 0;
            }
        }
    }

    return -1;
}
