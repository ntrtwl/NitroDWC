#ifndef ND_BASE64_H
#define ND_BASE64_H

int DWCi_Ndi_getDecodeLenBASE64(int len);
int DWCi_Ndi_getEncodeLenBASE64(int len);
int DWCi_Ndi_encodeBASE64(char *encstr, char *srcbin, int length);
int DWCi_Ndi_decodeBASE64(char *decbin, char *str, int length);

#endif // ND_BASE64_H
