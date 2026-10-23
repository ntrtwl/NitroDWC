#ifndef DWC_UTIL_ALLOC_H_
#define DWC_UTIL_ALLOC_H_

#include <nitro.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*DWCAuthAlloc)(u32 name, s32 size);
typedef void (*DWCAuthFree)(u32 name, void *ptr, s32 size);

#ifdef __cplusplus
}
#endif

#endif
