#ifndef ND_BASE64
#define ND_BASE64

int DWCi_Ndi_getDecodeLenBASE64(int len);
int DWCi_Ndi_getEncodeLenBASE64(int len);
int DWCi_Ndi_encodeBASE64(char *encstr, char *srcbin, int length);
int DWCi_Ndi_decodeBASE64(char *decbin, char *str, int length);

#endif // ND_BASE64
