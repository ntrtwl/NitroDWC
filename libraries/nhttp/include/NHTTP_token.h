#ifndef NHTTP_TOKEN_H
#define NHTTP_TOKEN_H

#include <nitro.h>

#include "nhttp/NHTTP.h"

int NHTTPi_getUrlEncodedSize(char *p);
int NHTTPi_encodeUrlChar(char *p, char c);
int NHTTPi_strToHex(char *p, int len);
int NHTTPi_strToInt(char *p, int len);
int NHTTPi_intToStr(char *p, u32 n);
int NHTTPi_compareToken(char *p1, char *p2);
int NHTTPi_strtonum(char *p, int size);
int NHTTPi_memfind(char *p, int size, char *pattern, int patternsize);

#endif // NHTTP_TOKEN_H
