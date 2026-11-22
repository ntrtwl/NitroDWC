#include "NHTTP_socket.h"

#include <nitro.h>
#include <nitroWiFi.h>

#include "nhttp/NHTTP.h"

#include "NHTTP_lib.h"

static int SslAuthCallback(int result, SOCSslConnection *con, int level)
{
    if (result & SOC_CERT_OUTOFDATE) {
        result &= ~SOC_CERT_OUTOFDATE;
    }

    return result;
}

void NHTTPi_SetSSLSeed(void)
{
    static u32 ssl_seed[OS_LOW_ENTROPY_DATA_SIZE / sizeof(u32)];
    OS_GetLowEntropyData(ssl_seed);
    SOC_AddRandomSeed(ssl_seed, sizeof(ssl_seed));
}

int NHTTPi_SocOpen(NHTTPReq *req)
{
    int s = SOC_Socket(SOC_PF_INET, SOC_SOCK_STREAM, 0);

    if (s >= 0 && req->isSSL) {
        NHTTPi_SetSSLSeed();

        req->sslConn->ca_info = req->ca_info;
        req->sslConn->ca_builtins = req->n_ca;
        req->sslConn->auth_callback = SslAuthCallback;

        if (SOC_EnableSsl(s, req->sslConn) < 0) {
            SOC_Close(s);
            s = -1;
        }
    }

    return s;
}

int NHTTPi_SocClose(NHTTPReq *req, int s)
{
    int err = SOC_Close(s);
    return err;
}

int NHTTPi_SocCloseWait(NHTTPReq *req, int s)
{
    int err = SOC_Close(s);
    while (err == SOC_EINPROGRESS) {
        OS_Sleep(500);
        err = SOC_Close(s);
    }
    return err;
}

int NHTTPi_SocConnect(NHTTPReq *req, int s, u32 hostaddr, int port)
{
    SOCSockAddrIn scadr;
    scadr.len = sizeof(SOCSockAddrIn);
    scadr.family = SOC_PF_INET;
    scadr.port = SOC_HtoNs(port);
    scadr.addr.addr = hostaddr;

    int err = SOC_Connect(s, &scadr);
    if (err < 0) {
        return req->doCancel ? NHTTP_SOCKET_ERROR_CANCELED : NHTTP_SOCKET_ERROR_NETWORK;
    }
    return 0;
}

int NHTTPi_SocRecv(NHTTPReq *req, int s, char *buf, int len, int flags)
{
    int err = SOC_Recv(s, buf, len, flags);
    if (err < 0) {
        if (req->doCancel) {
            return NHTTP_SOCKET_ERROR_CANCELED;
        }

        if (err == SOC_ENOTCONN) {
            return 0;
        }

        return NHTTP_SOCKET_ERROR_NETWORK;
    }

    return err;
}

int NHTTPi_SocSend(NHTTPReq *req, int s, char *buf, int len, int flags)
{
    int err = SOC_Send(s, buf, len, flags);
    if (err < 0) {
        if (req->doCancel) {
            return NHTTP_SOCKET_ERROR_CANCELED;
        }

        if (err == SOC_ENOTCONN) {
            return 0;
        }

        return NHTTP_SOCKET_ERROR_NETWORK;
    }

    return err;
}

void NHTTPi_SocCancel(NHTTPReq *req, int s)
{
    if (s >= 0) {
        SOC_Shutdown(s, SOC_SHUT_RDWR);
    }
}
