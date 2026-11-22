#ifndef NHTTP_STDLIB_H
#define NHTTP_STDLIB_H

#include "nhttp/NHTTP.h"

int NHTTPi_strcmp(const char *p1, const char *p2);
int NHTTPi_stricmp(const char *p1, const char *p2);
int NHTTPi_strncmp(const char *p1, const char *p2, int size);
int NHTTPi_strnicmp(const char *p1, const char *p2, int size);
int NHTTPi_strlen(const char *p);
void NHTTPi_memclr(void *p, int size);
void NHTTPi_memcpy(void *dst, const void *src, int size);

#endif // NHTTP_STDLIB_H
