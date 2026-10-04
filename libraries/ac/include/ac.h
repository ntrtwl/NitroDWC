#ifndef DWC_AC_AC_H
#define DWC_AC_AC_H

#include <bm/dwc_backup.h>
#include <nitro.h>
#include <nitroWiFi.h>

#include "ac/dwc_ac.h"

#define AC_FIND_LIST_MAX     10
#define AC_SEARCH_LIST_COUNT 9

typedef struct ACSearchList {
    u8 find : 4;
    u8 duplicate : 4;
    u8 type;
    u8 channel;
    u8 length;
    u8 ssid[WM_SIZE_SSID];
} ACSearchList;

typedef struct ACFindList {
    u8 state;
    u8 type;
    u8 rssi;
    u8 channel : 7;
    u8 find : 1;
} ACFindList;

typedef struct ACWORK {
    DWCMemPage userInfo[3];
    ACSearchList searchList[AC_SEARCH_LIST_COUNT];
    ACFindList findList[AC_FIND_LIST_MAX + 1];
    WMBssDesc bssDesc[AC_FIND_LIST_MAX + 1];
    OSTick timeOut;
    WCMWepDesc wepKey;
    u8 dmaNo;
    u8 powerMode : 2;
    u8 authMode : 2;
    u8 aroundCount : 4;
    u8 connectType : 4;
    u8 skipNetCheck : 2;
    u8 duplicateFlag : 2;
    u8 connectApType;
    u8 phaseBak;
    u8 searchListNo;
    u8 searchListNum;
    s8 searchChannel;
    u8 findListNum;
    u8 connectNo;
    u8 connectResult;
    u8 count;
    u16 stealthChannel;
} ACWORK;

typedef struct ACHOLD {
    void *(*alloc)(u32 name, s32 size);
    void (*free)(u32 name, void *ptr, s32 size);
    u8 allocState;
    u8 phase;
    u8 phaseError;
    u8 findAP;
    int error;
    int errorTest;
    u8 endState;
    u8 endType;
    u8 endSequence;
    u8 type;
    char apSpotInfo[10];
    u8 overrideType;
} ACHOLD;

enum ACPhase {
    AC_PHASE_NONE = 0,
    AC_PHASE_START,
    AC_PHASE_SEARCH_START,
    AC_PHASE_SEARCH_AROUND,
    AC_PHASE_SEARCH_DIFFER_CHANNEL,
    AC_PHASE_SEARCH_STEALTH,
    AC_PHASE_SEARCH_END,
    AC_PHASE_CONNECT_START,
    AC_PHASE_CONNECT_AP,
    AC_PHASE_CONNECT_RETRY,
    AC_PHASE_CONNECT_TEST_START,
    AC_PHASE_CONNECT_TEST_RETRY,
    AC_PHASE_CONNECT_TEST_GET_IP,
    AC_PHASE_CONNECT_TEST_CREATE,
    AC_PHASE_CONNECT_TEST_PROCESS,
    AC_PHASE_CONNECT_TEST_END,
    AC_PHASE_COMPLETE,
    AC_PHASE_ERROR,
    AC_PHASE_FATAL_ERROR,
};

enum APType {
    AP_TYPE_USER1 = 0,
    AP_TYPE_USER2,
    AP_TYPE_USER3,
    AP_TYPE_AOSS_USER1,
    AP_TYPE_AOSS_USER2,
    AP_TYPE_AOSS_USER3,
    AP_TYPE_USB,
    AP_TYPE_SHOP,
    AP_TYPE_FREESPOT,
    AP_TYPE_WAYPORT,
    AP_TYPE_NINTENDOWFC,
};

enum ACConnectResult {
    AC_CONNECT_START = 0,
    AC_CONNECT_FAILURE_AUTH,
    AC_CONNECT_FAILURE_WEP,
    AC_CONNECT_FAILURE_CAPACITY,
    AC_CONNECT_FAILURE,
};

enum APState {
    AP_CAN_CONNECT = 0,
    AP_CANT_CONNECT,
    AP_DISCONNECTED,
    AP_WEP_FAILURE,
    AP_OVER_CAPACITY,
};

enum ACError {
    AC_ERROR_WCM_FATAL = 0,
    AC_ERROR_WCM_STARTUP,
    AC_ERROR_SOC_STARTUP,
    AC_ERROR_NETCHECK_CREATE,
    AC_ERROR_WCM_IRREGULAR,
    AC_ERROR_AP_NOT_FOUND,
    AC_ERROR_INET_NOT_FOUND,
};

#define AC_NAME_HOLD       (1 << 0)
#define AC_NAME_WCM_BUFFER (1 << 1)
#define AC_NAME_SOC_CONFIG (1 << 2)
#define AC_NAME_NETCHECK   (1 << 3)
#define AC_NAME_WORK       (1 << 4)

void DWCi_AC_InsertApInfo(int no, DWCBMApInfo *info);
void *DWCi_AC_Alloc(u32 name, s32 size);
void DWCi_AC_Free(u32 name, void *ptr, s32 size);
void DWCi_AC_FreeAll(void);
void *DWCi_AC_GetMemPtr(u32 name);
void DWCi_AC_SetPhase(u8 phase);
u8 DWCi_AC_GetPhase(void);
void DWCi_AC_SetError(int error);
int DWCi_AC_GetError(void);
void DWCi_AC_SetApType(u8 type);
u8 DWCi_ConvConnectAPType(u8 type);

#endif
