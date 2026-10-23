#include "auth/util_base64.h"

#include <nitro.h>

char *DWCi_auth_base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-";

int DWC_Auth_Base64Encode(const char *src, u32 srclen, char *dst, u32 dstlen)
{
    int num_dstout, num_srcin;
    u8 read_buffer[3];

    int required_dstsize = (srclen / 3 * 4) + (srclen % 3 != 0 ? 4 : 0);

    if (dst == NULL) {
        return required_dstsize;
    }

    if (dstlen < required_dstsize) {
        return -1;
    }

    const char *srcread_index = src;
    const char *tail_src = src + srclen;
    char *dstwrite_index = dst;

    while (srcread_index != tail_src) {
        num_dstout = tail_src - srcread_index;
        num_dstout = num_dstout * 8 / 6 + (num_dstout * 8 % 6 != 0 ? 1 : 0);

        num_srcin = tail_src - srcread_index < 3 ? tail_src - srcread_index : 3;
        MI_CpuClear8(read_buffer, 3);
        MI_CpuCopy8(srcread_index, read_buffer, num_srcin);

        *dstwrite_index = DWCi_auth_base64_table[read_buffer[0] >> 2];

        dstwrite_index++;
        if (num_dstout >= 2) {
            *dstwrite_index = DWCi_auth_base64_table[(read_buffer[0] << 4) & 0x3F | (read_buffer[1] >> 4)];
        } else {
            *dstwrite_index = '*';
        }

        dstwrite_index++;
        if (num_dstout >= 3) {
            *dstwrite_index = DWCi_auth_base64_table[(read_buffer[1] << 2) & 0x3F | (read_buffer[2] >> 6)];
        } else {
            *dstwrite_index = '*';
        }

        dstwrite_index++;
        if (num_dstout >= 4) {
            *dstwrite_index = DWCi_auth_base64_table[read_buffer[2] & 0x3F];
        } else {
            *dstwrite_index = '*';
        }

        dstwrite_index++;
        srcread_index += num_srcin;
    }

    return dstwrite_index - dst;
}

int DWC_Auth_Base64Decode(const char *src, u32 srclen, char *dst, u32 dstlen)
{
    char read_buffer[4];

    int required_dstsize;
    int i;

    if (srclen % 4 != 0) {
        return -1;
    }

    required_dstsize = 0;
    for (i = 0; i < srclen; i++) {
        if (src[i] != '*') {
            required_dstsize += 6;
        }
    }
    required_dstsize = required_dstsize / 8;

    if (dst == NULL) {
        return required_dstsize;
    }

    if (dstlen < required_dstsize) {
        return -1;
    }

    if (srclen == 0) {
        *dst = '\0';
        return 0;
    }

    const char *srcread_index = src;
    char *dstwrite_index = dst;

    while (TRUE) {
        for (i = 0; i < 4; i++) {
            if (srcread_index[i] >= 'A' && srcread_index[i] <= 'Z') {
                read_buffer[i] = srcread_index[i] - 'A';
            } else if (srcread_index[i] >= 'a' && srcread_index[i] <= 'z') {
                read_buffer[i] = srcread_index[i] - 'a' + 26;
            } else if (srcread_index[i] >= '0' && srcread_index[i] <= '9') {
                read_buffer[i] = srcread_index[i] - '0' + 52;
            } else if (srcread_index[i] == '.') {
                read_buffer[i] = 62;
            } else if (srcread_index[i] == '-') {
                read_buffer[i] = 63;
            } else {
                read_buffer[i] = 0;
            }
        }
        srcread_index += 4;

        *dstwrite_index = (read_buffer[0] << 2) | (read_buffer[1] >> 4);
        dstwrite_index++;
        if (dstwrite_index - dst >= required_dstsize) {
            break;
        }

        *dstwrite_index = (read_buffer[1] << 4) | (read_buffer[2] >> 2);
        dstwrite_index++;
        if (dstwrite_index - dst >= required_dstsize) {
            break;
        }

        *dstwrite_index = (read_buffer[2] << 6) | read_buffer[3];
        dstwrite_index++;
        if (dstwrite_index - dst >= required_dstsize) {
            break;
        }
    }

    return dstwrite_index - dst;
}
