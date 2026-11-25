#ifndef ND_STDLIB_H
#define ND_STDLIB_H

int DWCi_Ndi_strlen(const char *p);
int DWCi_Ndi_strcmp(const char *p1, const char *p2);
int DWCi_Ndi_strncmp(const char *p1, const char *p2, int size);
void DWCi_Ndi_memclr(void *p, int size);
void DWCi_Ndi_memcpy(void *dst, const void *src, int size);

#endif // ND_STDLIB_H
