#ifndef DWC_AC_SHOP_USB_H
#define DWC_AC_SHOP_USB_H
#include <nitro.h>

#define NINTENDO_USB_SSID_LENGTH  8
#define NINTENDO_SHOP_SSID_LENGTH 8

#define NINTENDO_USB_SSID  "NWCUSBAP"
#define NINTENDO_SHOP_SSID "NDWCSHAP"

#define NINTENDO_USB_CONNECT_FLAG 9

#define WEPKEY_LENGTH      13
#define SSID_DECODE_LENGTH 24

BOOL DWCi_AC_CheckNintendoShopAP(const char *ssid);
void DWCi_AC_GetNintendoShopWepKey(const char *ssid, u8 wep[WEPKEY_LENGTH]);
void DWCi_AC_GetPostalCode(const char *ssid, u8 pos[10]);
BOOL DWCi_AC_CheckNintendoUsbAP(const char *ssid);
BOOL DWCi_AC_GetNintendoUSBWepKey(const char *ssid, u8 wep[WEPKEY_LENGTH]);

#endif // DWC_AC_SHOP_USB_H
