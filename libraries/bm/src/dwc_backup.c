#include "bm/dwc_backup.h"

#include <nitro.h>
#include <nitro/spi/ARM7/nvram.h>

enum NVRAMCommandState {
    COMM_STATE_READ = 1,
    COMM_STATE_WRITE_ENABLE,
    COMM_STATE_WRITE,
    COMM_STATE_READ_STATUS_WRITE_ENABLE,
    COMM_STATE_READ_STATUS_WRITE,
    COMM_STATE_SOFTWARE_RESET,
    COMM_STATE_WRITE_DISABLE,
};

static u8 Work[32] ATTRIBUTE_ALIGN(32);
static u8 Wifi[14];
static volatile BOOL nv_cb_occurred;
static vu16 nv_result;
static u32 Address;

static BOOL readNvram(u32 address, u32 size, void *buf);
static void writeNvram(u32 address, u16 size, const void *data);
static BOOL verify(const void *src, u32 address, u32 size, void *work);
static BOOL writeDisable(void);
static void Callback_NVRAM(PXIFifoTag tag, u32 data, BOOL err);

BOOL DWCi_BM_GetApInfo(DWCMemPage *buf)
{
    return readNvram(Address, sizeof(DWCMemPage) * 3, buf) != FALSE;
}

void DWCi_BM_GetWiFiInfo(DWCWiFiInfo *buf)
{
    MI_CpuCopy8(&Wifi, &buf->attestedUserId, 6);
    buf->attestedUserId &= 0x7FFFFFFFFFF;

    MI_CpuCopy8(&Wifi[5], &buf->notAttestedId, 6);
    buf->notAttestedId >>= 3;
    buf->notAttestedId &= 0x7FFFFFFFFFF;

    MI_CpuCopy8(&Wifi[10], &buf->pass, 2);
    buf->pass >>= 6;
    buf->pass &= 0x3FF;

    MI_CpuCopy8(&Wifi[12], &buf->randomHistory, 2);
}

BOOL DWCi_BM_SetWiFiInfo(const DWCWiFiInfo *info, void *work)
{
    int i;
    MEMSWI *wk = work;
    u32 addr = Address;

    DWCi_BACKUPlConvWifiInfo(info);
    MATH_CRC16InitTable(&wk->table);

    for (i = 0; i < 2; i++, addr += sizeof(DWCMemPage)) {
        if (!readNvram(addr, sizeof(DWCMemPage), &wk->mem)) {
            OS_Panic("DWCi_BM_SetWiFiInfo : readNvram failed\n");
            return FALSE;
        }

        MI_CpuCopy8(Wifi, wk->mem.wifi, sizeof(Wifi));
        wk->mem.crc = MATH_CalcCRC16(&wk->table, &wk->mem, sizeof(DWCMemPage) - sizeof(wk->mem.crc));

        do {
            writeNvram(addr, sizeof(DWCMemPage), &wk->mem);
        } while (!verify(&wk->mem, addr, sizeof(DWCMemPage), &wk->work));
    }

    return writeDisable() != FALSE;
}

BOOL DWCi_BACKUPlInit(void *work)
{
    u16 *wk = work;

    if (!readNvram(32, 32, wk)) {
        return FALSE;
    }

    Address = wk[0] * 8 - 0x400;
    return TRUE;
}

BOOL DWCi_BACKUPlRead(DWCMemMap *mem)
{
    return readNvram(Address, sizeof(DWCMemMap), mem) != FALSE;
}

BOOL DWCi_BACKUPlWritePage(const DWCMemMap *data, const BOOL *page, void *work)
{
    int i;
    u32 addr = Address;

    for (i = 0; i < 4; i++, addr += sizeof(DWCMemPage)) {
        if (page[i]) {
            do {
                writeNvram(addr, sizeof(DWCMemPage), &data->page[i]);
            } while (!verify(&data->page[i], addr, sizeof(DWCMemPage), work));
        }
    };

    return writeDisable() != FALSE;
}

BOOL DWCi_BACKUPlWriteAll(MEMINIT *work)
{
    int i;
    u32 addr = Address;

    for (i = 0; i < 4; i++, addr += sizeof(DWCMemPage)) {
        work->mm.page[i].crc = MATH_CalcCRC16(&work->table, &work->mm.page[i], sizeof(DWCMemPage) - sizeof(work->mm.page[i].crc));
        do {
            writeNvram(addr, sizeof(DWCMemPage), &work->mm.page[i]);
        } while (!verify(&work->mm.page[i], addr, sizeof(DWCMemPage), work->work));
    };

    return writeDisable() != FALSE;
}

BOOL DWCi_BACKUPlWriteChild(DWCMemMap *data, void *work)
{
    MEMWRITEC *wk = work;
    int i;
    u32 addr = Address;

    if (!Address) {
        return FALSE;
    }

    MATH_CRC16InitTable(&wk->table);

    for (i = 0; i < 4; i++, addr += sizeof(DWCMemPage)) {
        data->page[i].crc = MATH_CalcCRC16(&wk->table, &data->page[i], sizeof(DWCMemPage) - sizeof(data->page[i].crc));
        do {
            writeNvram(addr, sizeof(DWCMemPage), &data->page[i]);
        } while (!verify(&data->page[i], addr, sizeof(DWCMemPage), wk->work));
    }

    return writeDisable() != FALSE;
}

void DWCi_BACKUPlSetWiFi(const u8 *wifi)
{
    MI_CpuCopy8(wifi, Wifi, sizeof(Wifi));
}

u8 DWCi_BACKUPlConvMaskCidr(const u8 *mask)
{
    int snm;
    int i;

    for (i = 0, snm = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            if ((mask[i] >> j) & 1) {
                snm++;
            }
        }
    };

    return snm;
}

void DWCi_BACKUPlConvMaskAddr(int mask, u8 *buf)
{
    u32 snm = (0xFFFFFFFF >> mask) ^ 0xFFFFFFFF;

    for (int i = 0; i < 4; i++) {
        buf[i] = snm >> (24 - i * 8);
    }
}

BOOL DWC_BACKUPlCheckSsid(const u8 *ssid)
{
    for (int i = 0; i < 32; i++) {
        if (ssid[i] != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL DWC_BACKUPlCheckIp(const u8 *ip, const u8 *mask)
{
    u32 ipa;
    u32 snm;

    if (!DWC_BACKUPlCheckAddress(ip)) {
        return FALSE;
    }

    MI_CpuCopy8(ip, &ipa, sizeof(u32));
    MI_CpuCopy8(mask, &snm, sizeof(u32));

    if ((ipa | snm) == ~1) {
        return FALSE;
    }

    if (ipa & ~snm) {
        return TRUE;
    }

    return FALSE;
}

BOOL DWC_BACKUPlCheckAddress(const u8 *address)
{
    if (address[0] == 127) {
        return FALSE;
    }

    if (address[0] < 1) {
        return FALSE;
    }

    if (address[0] > 223) {
        return FALSE;
    }

    return TRUE;
}

static BOOL NVRAMm_ExecuteCommand(int nv_state, u32 addr, u16 size, u8 *srcp)
{
    OSTick start;
    BOOL nv_sending = FALSE;

    u8 *nvram_srp = Work;

    while (TRUE) {
        if (!nv_sending) {
            nv_cb_occurred = FALSE;

            switch (nv_state) {
            case COMM_STATE_READ:
                nv_sending = SPI_NvramReadDataBytes(addr, size, srcp);
                break;
            case COMM_STATE_WRITE_ENABLE:
                nv_sending = SPI_NvramWriteEnable();
                break;
            case COMM_STATE_WRITE:
                nv_sending = SPI_NvramPageWrite(addr, size, srcp);
                start = OS_GetTick();
                break;
            case COMM_STATE_READ_STATUS_WRITE_ENABLE:
            case COMM_STATE_READ_STATUS_WRITE:
                nv_sending = SPI_NvramReadStatusRegister(nvram_srp);
                break;
            case COMM_STATE_SOFTWARE_RESET:
                nv_sending = SPI_NvramSoftwareReset();
                break;
            case COMM_STATE_WRITE_DISABLE:
                nv_sending = SPI_NvramWriteDisable();
                break;
            }
        } else {
            if (nv_cb_occurred == TRUE) {
                nv_sending = FALSE;

                if (nv_result == SPI_PXI_RESULT_SUCCESS) {
                    switch (nv_state) {
                    case COMM_STATE_READ:
                        return TRUE;
                    case COMM_STATE_WRITE_ENABLE:
                        nv_state = COMM_STATE_READ_STATUS_WRITE_ENABLE;
                        break;
                    case COMM_STATE_WRITE:
                        nv_state = COMM_STATE_READ_STATUS_WRITE;
                        break;
                    case COMM_STATE_READ_STATUS_WRITE_ENABLE:
                    case COMM_STATE_READ_STATUS_WRITE:
                        DC_InvalidateRange(nvram_srp, sizeof(u8));
                        if (nv_state == COMM_STATE_READ_STATUS_WRITE_ENABLE) {
                            if (*nvram_srp & NVRAM_STATUS_REGISTER_WEL) {
                                nv_state = COMM_STATE_WRITE;
                            } else {
                                return FALSE;
                            }
                        } else {
                            if (!(*nvram_srp & NVRAM_STATUS_REGISTER_WIP)) {
                                return TRUE;
                            } else {
                                if (*nvram_srp & NVRAM_STATUS_REGISTER_ERSER || OS_TicksToMilliSeconds(OS_GetTick() - start) > 4000) {
                                    nv_state = COMM_STATE_SOFTWARE_RESET;
                                } else {
                                    SVC_WaitByLoop(0x4000);
                                }
                            }
                        }
                        break;
                    case COMM_STATE_SOFTWARE_RESET:
                        return FALSE;
                    case COMM_STATE_WRITE_DISABLE:
                        return TRUE;
                    }
                } else {
                    return FALSE;
                }
            }
        }
    }
}

static BOOL readNvram(u32 address, u32 size, void *buf)
{
    DC_InvalidateRange(buf, size);

    while (!PXI_IsCallbackReady(PXI_FIFO_TAG_NVRAM, PXI_PROC_ARM7)) {
    }
    PXI_SetFifoRecvCallback(PXI_FIFO_TAG_NVRAM, Callback_NVRAM);

    while (TRUE) {
        if (NVRAMm_ExecuteCommand(COMM_STATE_READ, address, size, buf) == TRUE) {
            break;
        }

        SVC_WaitByLoop(0x40000);
    }

    DC_InvalidateRange(buf, size);
    return TRUE;
}

static void writeNvram(u32 address, u16 size, const void *data)
{
    while (!PXI_IsCallbackReady(PXI_FIFO_TAG_NVRAM, PXI_PROC_ARM7)) {
    }
    PXI_SetFifoRecvCallback(PXI_FIFO_TAG_NVRAM, Callback_NVRAM);

    DC_StoreRange(data, size);

    while (TRUE) {
        if (NVRAMm_ExecuteCommand(COMM_STATE_WRITE_ENABLE, address, size, data) == TRUE) {
            break;
        }

        SVC_WaitByLoop(0x40000);
    }
}

static BOOL verify(const void *src, u32 address, u32 size, void *work)
{
    if (!readNvram(address, size, work)) {
        return FALSE;
    }

    if (memcmp(src, work, size) == 0) {
        return TRUE;
    }

    return FALSE;
}

static BOOL writeDisable(void)
{
    while (!PXI_IsCallbackReady(PXI_FIFO_TAG_NVRAM, PXI_PROC_ARM7)) {
    }
    PXI_SetFifoRecvCallback(PXI_FIFO_TAG_NVRAM, Callback_NVRAM);

    while (TRUE) {
        if (NVRAMm_ExecuteCommand(COMM_STATE_WRITE_DISABLE, 0, 0, NULL) == TRUE) {
            break;
        }

        SVC_WaitByLoop(0x40000);
    }

    return TRUE;
}

static void Callback_NVRAM(PXIFifoTag tag, u32 data, BOOL err)
{
    nv_result = data & SPI_PXI_RESULT_DATA_MASK;
    nv_cb_occurred = TRUE;

    if (err) {
        nv_result = 0xFF;
    }

    if (nv_result != SPI_PXI_RESULT_SUCCESS) {
    }
}

const u8 *DWCi_BACKUPlConvWifiInfo(const DWCWiFiInfo *info)
{
    u64 tmp = info->notAttestedId;

    MI_CpuCopy8(info, &Wifi, 5);
    Wifi[5] = ((info->attestedUserId >> 40) & 7) | ((tmp & 0x1F) << 3);

    tmp >>= 5;
    MI_CpuCopy8(&tmp, &Wifi[6], 4);

    Wifi[10] = (tmp >> 32 & 0x3F) | ((info->pass & 3) << 6);

    Wifi[11] = info->pass >> 2;

    MI_CpuCopy8(&info->randomHistory, &Wifi[12], 2);
    return Wifi;
}

const void *DWCi_BACKUPlGetWifi(void)
{
    return Wifi;
}
