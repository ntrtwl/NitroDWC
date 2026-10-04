#include "shop_usb.h"

#include <nitro.h>

#include "ac/dwc_ac.h"

static const char *MASKkey0 = "gwi'6&fs=0Nf~";
static const char *MASKkey1 = "%(egEr)ag(s&m";

static const char *SHMASKkey0 = "38g6zxjk20gvmv]6^=j&%vY1";
static const char *SHMASKkey1 = "952uybjnpmu903bia@bk5m[-";

static const u8 CONV_4BIT_TABLE[] = { 10, 13, 14, 8, 9, 3, 6, 0, 12, 5, 2, 7, 11, 1, 15, 4 };
static const u8 CONV_BYTE_TABLE[] = { 5, 1, 12, 4, 2, 3, 10, 0, 11, 7, 9, 8, 6 };

typedef union U4B {
    u32 us32;
    u8 us8[4];
} U4B;

static void decodeSSID(const char *ssid, u8 buf[SSID_DECODE_LENGTH]);
static void makeShopWepKey(const u8 *seed, u8 *wepkey);
static void makeUsbWepKey(const u8 *seed, u8 *wepkey);
static int db64(const char *src, u8 *dest, u32 srcsize, u32 destsize);

BOOL DWCi_AC_CheckNintendoShopAP(const char *ssid)
{
    u8 buf[SSID_DECODE_LENGTH];
    decodeSSID(ssid, buf);
    return memcmp(buf, NINTENDO_SHOP_SSID, NINTENDO_SHOP_SSID_LENGTH) == 0;
}

void DWCi_AC_GetNintendoShopWepKey(const char *ssid, u8 wep[WEPKEY_LENGTH])
{
    u8 buf[SSID_DECODE_LENGTH];
    decodeSSID(ssid, buf);
    makeShopWepKey(buf, wep);
}

void DWCi_AC_GetPostalCode(const char *ssid, u8 pos[AP_ID_LENGTH])
{
    u8 buf[SSID_DECODE_LENGTH];
    decodeSSID(ssid, buf);
    if (memcmp(buf, NINTENDO_SHOP_SSID, NINTENDO_SHOP_SSID_LENGTH) == 0) {
        MI_CpuCopy8(&buf[8], pos, AP_ID_LENGTH);
    }
}

BOOL DWCi_AC_CheckNintendoUsbAP(const char *ssid)
{
    return memcmp(ssid, NINTENDO_USB_SSID, NINTENDO_USB_SSID_LENGTH) == 0;
}

BOOL DWCi_AC_GetNintendoUSBWepKey(const char *ssid, u8 wep[WEPKEY_LENGTH])
{
    ssid += 12;
    makeUsbWepKey(ssid, wep);
}

static void decodeSSID(const char *ssid, u8 buf[SSID_DECODE_LENGTH])
{
    int i;
    u8 ctable_inv[] = { 23, 20, 17, 13, 11, 6, 15, 14, 9, 21, 12, 4, 2, 1, 18, 16, 5, 3, 19, 10, 7, 8, 0, 22 };

    db64(ssid, buf, WM_SIZE_SSID, SSID_DECODE_LENGTH);

    for (i = 0; i < SSID_DECODE_LENGTH; i++) {
        buf[i] ^= SHMASKkey1[i];
    }

    for (i = 0; i < SSID_DECODE_LENGTH; i++) {
        u8 destnum = i;
        u8 dest;
        u8 srcnum = i;
        u8 src = buf[i];

        while (ctable_inv[srcnum] != 0xFF) {
            dest = buf[ctable_inv[srcnum]];
            buf[ctable_inv[destnum]] = src;
            destnum = ctable_inv[srcnum];
            ctable_inv[srcnum] = 0xFF;
            src = dest;
            srcnum = destnum;
        }
    }

    for (i = 0; i < SSID_DECODE_LENGTH; i++) {
        buf[i] ^= SHMASKkey0[i];
    }
}

static void makeShopWepKey(const u8 *seed, u8 *wepkey)
{
    u8 temp[17];
    u8 md5src[3] = { 0x61, 0x61, 0x61 }; // unused

    MATHMD5Context context;
    MATH_MD5Init(&context);
    MATH_MD5Update(&context, seed, SSID_DECODE_LENGTH);
    MATH_MD5GetHash(&context, temp);

    MI_CpuCopy8(&temp[3], wepkey, WEPKEY_LENGTH);
}

static void makeUsbWepKey(const u8 *seed, u8 *wepkey)
{
    int i;
    u8 keytemp[WEPKEY_LENGTH];

    for (i = 0; i < WEPKEY_LENGTH; i++) {
        wepkey[i] = seed[i] ^ seed[WEPKEY_LENGTH + i % 7];
    }

    for (i = 0; i < 7; i++) {
        wepkey[3 + i] ^= seed[WEPKEY_LENGTH + i];
    }

    for (i = 0; i < WEPKEY_LENGTH; i++) {
        wepkey[i] ^= MASKkey0[i];
    }

    MI_CpuCopy8(wepkey, keytemp, WEPKEY_LENGTH);

    for (i = 0; i < WEPKEY_LENGTH; i++) {
        wepkey[CONV_BYTE_TABLE[i]] = keytemp[i];
    }

    for (i = 0; i < WEPKEY_LENGTH; i++) {
        wepkey[i] ^= MASKkey1[i];
    }

    for (i = 0; i < WEPKEY_LENGTH; i++) {
        wepkey[i] = CONV_4BIT_TABLE[(wepkey[i] >> 4) & 0xF] << 4 | CONV_4BIT_TABLE[wepkey[i] & 0xF];
    }

    for (i = 0; i < 3; i++) {
        wepkey[i] ^= wepkey[i + 6];
        wepkey[i + 3] ^= wepkey[i + 9];
        wepkey[i + 6] ^= wepkey[i + 3];
        wepkey[i + 9] ^= wepkey[i];
        wepkey[12] ^= wepkey[i];
    }
}

static s32 codetovalue(u8 c)
{
    if ('A' <= c && c <= 'Z') {
        return c - 'A';
    } else if ('a' <= c && c <= 'z') {
        return (s32)(c - 'a') + 26;
    } else if ('0' <= c && c <= '9') {
        return (s32)(c - '0') + 52;
    } else if (c == '+') {
        return 62;
    } else if (c == '/') {
        return 63;
    } else if (c == '=') {
        return 0;
    } else {
        return -1;
    }
}

static int db64(const char *src, u8 *dest, u32 srcsize, u32 destsize)
{
    U4B u4b;
    u32 base64 = 0;
    int srcmod = 0;
    int srcmax = 0;
    int i, j, k;

    if (destsize >= (srcsize * 3) / 4) {
        srcmod = srcsize % 4;
        srcmax = srcsize - srcmod;
    } else {
        return -1;
    }

    for (i = 0; i < srcmax; i += 4) {
        base64 = 0;

        for (j = 0; j < 4; j++) {
            u32 x = codetovalue(src[i + j]);
            base64 |= x << (3 - j) * 6;
        }

        u4b.us32 = base64;
        for (k = 0; k < 3; k++) {
            dest[(i / 4) * 3 + k] = u4b.us8[2 - k];
        }
    }

    if (srcmod) {
        u4b.us32 = 0;
        base64 = 0;
        for (i = 0; i < srcmod; i++) {
            u32 x = codetovalue(src[srcmax + i]);
            base64 |= x << (3 - i) * 6;
            u4b.us32 = u4b.us32 | base64;
        }
        for (j = 0; j < srcmod; j++) {
            dest[(srcmax * 3) / 4 + j] = u4b.us8[2 - j];
        }
    }

    return (srcsize * 3) / 4;
}
