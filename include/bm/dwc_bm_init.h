#ifndef DWC_BM_INIT_H_
#define DWC_BM_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DWC_BM_INIT_WORK_SIZE 0x700
#define DWC_INIT_WORK_SIZE    0x700

enum DWCBMInitResult {
    DWC_BM_INIT_OK = 0,
    DWC_BM_INIT_WRITE_ERROR = -10000,
    DWC_BM_INIT_READ_ERROR = -10001,
    DWC_BM_INIT_OK_CLEAR = -10002,
    DWC_BM_INIT_OK_INIT = -10003,
};

int DWC_BM_Init(void *work);

#ifdef __cplusplus
}
#endif

#endif
