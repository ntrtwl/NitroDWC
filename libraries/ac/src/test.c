#include "test.h"

#include <auth/dwc_netcheck.h>
#include <nitro.h>
#include <nitroWiFi.h>

#include "ac.h"

static const SOCConfig DWC_AC_SOC_CONFIG = {
    .vendor = SOC_VENDOR_NINTENDO,
    .version = SOC_VERSION,
    .alloc = NULL,
    .free = NULL,
    .flag = SOC_FLAG_DHCP,
    .addr = SOC_HtoNl(SOC_INADDR_ANY),
    .netmask = SOC_HtoNl(SOC_INADDR_ANY),
    .router = SOC_HtoNl(SOC_INADDR_ANY),
    .dns1 = SOC_HtoNl(SOC_INADDR_ANY),
    .dns2 = SOC_HtoNl(SOC_INADDR_ANY),
    .timeWaitBuffer = 4096,
    .reassemblyBuffer = 4096,
    .mtu = 0,
    .rwin = 0,
    .r2 = 0,
    .peerid = NULL,
    .passwd = NULL,
    .serviceName = NULL,
    .hostName = "NINTENDO-DS",
    .rdhcp = 4,
    .udpSendBuff = 0,
    .udpRecvBuff = 0
};

static u8 ConnectTestStart(ACWORK *wk);
static u8 GetIPAddress(ACWORK *wk);
static u8 ConnectTestCreate(void);
static u8 ConnectTestProcess(ACWORK *wk);
static u8 ConnectTestEnd(ACWORK *wk);
static u8 ConnectTestRetry(void);
static void MakeSOCConfig(ACHOLD *hd, ACWORK *wk, SOCConfig *sc);
static u32 ConvAddress(u8 *add);
static u32 ConvNetMask(u8 netmask);
static void GetConnectAPType(ACWORK *wk, ACHOLD *hd);
static void CheckSetDNS(ACWORK *wk);

u8 DWCi_AC_ConnectTest(void)
{
    u8 phase = DWCi_AC_GetPhase();
    ACWORK *wk = DWCi_AC_GetMemPtr(AC_NAME_WORK);
    if (WCM_GetPhase() == WCM_PHASE_DCF) {
        switch (phase) {
        case AC_PHASE_CONNECT_TEST_START:
            phase = ConnectTestStart(wk);
            break;
        case AC_PHASE_CONNECT_TEST_GET_IP:
            phase = GetIPAddress(wk);
            break;
        case AC_PHASE_CONNECT_TEST_CREATE:
            phase = ConnectTestCreate();
            break;
        case AC_PHASE_CONNECT_TEST_PROCESS:
            phase = ConnectTestProcess(wk);
            break;
        case AC_PHASE_CONNECT_TEST_END:
            phase = ConnectTestEnd(wk);
            break;
        case AC_PHASE_CONNECT_TEST_RETRY:
            phase = ConnectTestRetry();
            break;
        default:
            break;
        }
    } else {
        switch (phase) {
        case AC_PHASE_CONNECT_TEST_END:
            phase = ConnectTestEnd(wk);
            break;
        case AC_PHASE_CONNECT_TEST_RETRY:
            phase = ConnectTestRetry();
            break;
        case AC_PHASE_CONNECT_TEST_PROCESS:
            DWC_Netcheck_Abort();
            DWC_Netcheck_Destroy();
        default:
            wk->findList[wk->connectNo].state = AP_DISCONNECTED;
            phase = AC_PHASE_CONNECT_TEST_RETRY;
            break;
        }
    }

    return phase;
}

static u8 ConnectTestStart(ACWORK *wk)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    SOCConfig *sc = DWCi_AC_GetMemPtr(AC_NAME_SOC_CONFIG);

    MakeSOCConfig(hd, wk, sc);
    SOCL_SetYieldWait(4);
    if (SOC_Startup(sc) != 0) {
        DWCi_AC_SetError(AC_ERROR_SOC_STARTUP);
        return AC_PHASE_ERROR;
    }

    return AC_PHASE_CONNECT_TEST_GET_IP;
}

static u8 GetIPAddress(ACWORK *wk)
{
    if (SOC_GetHostID() != SOC_HtoNl(SOC_INADDR_ANY)) {
        CheckSetDNS(wk);
        if (wk->skipNetCheck == TRUE) {
            return AC_PHASE_CONNECT_TEST_END;
        } else {
            return AC_PHASE_CONNECT_TEST_CREATE;
        }
    }

    u64 seconds = OS_TicksToSeconds(OS_GetTick() - wk->timeOut);
    if (seconds >= 10) {
        wk->findList[wk->connectNo].state = AP_CANT_CONNECT;
        return AC_PHASE_CONNECT_TEST_RETRY;
    }

    return AC_PHASE_CONNECT_TEST_GET_IP;
}

static u8 ConnectTestCreate(void)
{
    DWCNetcheckParam *nc = DWCi_AC_GetMemPtr(AC_NAME_NETCHECK);
    if (DWC_Netcheck_Create(nc) != DWCNETCHECK_E_NOERR) {
        DWCi_AC_SetError(AC_ERROR_NETCHECK_CREATE);
        return AC_PHASE_ERROR;
    }

    return AC_PHASE_CONNECT_TEST_PROCESS;
}

static u8 ConnectTestProcess(ACWORK *wk)
{
    ACHOLD *hd = DWCi_AC_GetMemPtr(AC_NAME_HOLD);
    int result = DWC_Netcheck_GetError();
    if (result != DWCNETCHECK_E_NOERR) {
        if (hd->endType == DWCi_ConvConnectAPType(wk->connectApType)) {
            hd->errorTest = DWC_Netcheck_GetReturnCode();
        }

        DWC_Netcheck_Destroy();
        if (result != DWCNETCHECK_E_NETAVAIL) {
            wk->findList[wk->connectNo].state = AP_CANT_CONNECT;
            return AC_PHASE_CONNECT_TEST_RETRY;
        }

        return AC_PHASE_CONNECT_TEST_END;
    }

    return AC_PHASE_CONNECT_TEST_PROCESS;
}

static u8 ConnectTestEnd(ACWORK *wk)
{
    DWCi_AC_SetApType(wk->connectApType);
    return AC_PHASE_COMPLETE;
}

static u8 ConnectTestRetry(void)
{
    if (SOCL_CalmDown() != 0) {
        return AC_PHASE_CONNECT_TEST_RETRY;
    }

    int result = SOC_Cleanup();
    if (result == 0 || result == SOC_ENETRESET) {
        return AC_PHASE_CONNECT_RETRY;
    } else {
        return AC_PHASE_CONNECT_TEST_RETRY;
    }
}

static void MakeSOCConfig(ACHOLD *hd, ACWORK *wk, SOCConfig *sc)
{
    MI_CpuCopy8(&DWC_AC_SOC_CONFIG, sc, sizeof(SOCConfig));
    sc->alloc = hd->alloc;
    sc->free = hd->free;

    DWCMemPage *info;
    if (wk->connectApType < AP_TYPE_AOSS_USER3 + 1) {
        info = &wk->userInfo[DWCi_ConvConnectAPType(wk->connectApType)];
    } else {
        return;
    }

    if (info->ap.ip[0] != 0) {
        sc->flag = 0;
        sc->addr.addr = ConvAddress(info->ap.ip);
        sc->netmask.addr = ConvNetMask(info->ap.netmask);
        sc->router.addr = ConvAddress(info->ap.gateway);
        sc->dns1.addr = ConvAddress(info->ap.dns[0]);
        sc->dns2.addr = ConvAddress(info->ap.dns[1]);
    } else {
        sc->flag = SOC_FLAG_DHCP;
        sc->addr.addr = SOC_HtoNl(SOC_INADDR_ANY);
        sc->netmask.addr = SOC_HtoNl(SOC_INADDR_ANY);
        sc->router.addr = SOC_HtoNl(SOC_INADDR_ANY);
        sc->dns1.addr = SOC_HtoNl(SOC_INADDR_ANY);
        sc->dns2.addr = SOC_HtoNl(SOC_INADDR_ANY);
    }
}

static u32 ConvAddress(u8 *addr)
{
    u32 address = 0;
    address |= (*addr++ << 24);
    address |= (*addr++ << 16);
    address |= (*addr++ << 8);
    address |= *addr;

    return SOC_HtoNl(address);
}

static u32 ConvNetMask(u8 netmask)
{
    int i;
    int n = 32 - netmask;
    u32 netmask32 = 0xFFFFFFFF;

    for (i = 0; i < n; i++) {
        netmask32 <<= 1;
    }

    return SOC_HtoNl(netmask32);
}

static void CheckSetDNS(ACWORK *wk)
{
    DWCMemPage *info;
    if (wk->connectApType < AP_TYPE_AOSS_USER3 + 1) {
        info = &wk->userInfo[DWCi_ConvConnectAPType(wk->connectApType)];
    } else {
        return;
    }

    int dns = info->ap.dns[0][0] + info->ap.dns[0][1] + info->ap.dns[0][2] + info->ap.dns[0][3];
    if (info->ap.ip[0] == 0 && dns != 0) {
        SOCInAddr dns1, dns2;
        dns1.addr = ConvAddress(info->ap.dns[0]);
        dns2.addr = ConvAddress(info->ap.dns[1]);
        SOC_SetResolver(&dns1, &dns2);
    }
}
