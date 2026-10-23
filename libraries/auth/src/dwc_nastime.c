#include "dwc_nastime.h"

#include <errno.h>
#include <nitro.h>
#include <stdlib.h>
#include <string.h>

// dumb hack to force the compiler to order the data correctly
static char *unused[] = {
    "Jun",
    "Jul",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Oct",
    "Nov"
};

static char *monthname[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

BOOL DWCnastimediffvalid;
s64 DWCnastimediff;
s64 DWCnastimediffbase;

BOOL DWC_Auth_SetNasTimeDiff(char *datestr)
{
    RTCDate rtcdate;
    RTCTime rtctime;
    s64 serversec;
    s64 localsec;
    char datestr2[30];

    if (strlen(datestr) != strlen("Fri, 03 Mar 2006 01:28:13 GMT")
        || (datestr[7] != ' ')
        || (datestr[11] != ' ')
        || (datestr[16] != ' ')
        || (datestr[19] != ':')
        || (datestr[22] != ':')
        || (datestr[25] != ' ')) {
        goto err;
    }

    if (RTC_GetDateTime(&rtcdate, &rtctime) != RTC_RESULT_SUCCESS) {
        goto err;
    }
    localsec = RTC_ConvertDateTimeToSecond(&rtcdate, &rtctime);
    if (localsec == -1) {
        goto err;
    }

    strcpy(datestr2, datestr);
    datestr2[7] = 0;
    datestr2[11] = 0;
    datestr2[16] = 0;
    datestr2[19] = 0;
    datestr2[22] = 0;
    datestr2[25] = 0;

    errno = ENOERR;
    rtcdate.year = atoi(&datestr2[12]);
    if (errno == ERANGE) {
        goto err;
    }
    rtcdate.year -= 2000;

    rtcdate.month = 13;
    for (int i = 0; i < 12; i++) {
        if (strcmp(monthname[i], &datestr2[8]) == 0) {
            rtcdate.month = i + 1;
            break;
        }
    }
    if (rtcdate.month > 12) {
        goto err;
    }

    errno = ENOERR;
    rtcdate.day = atoi(&datestr2[5]);
    if (errno == ERANGE) {
        goto err;
    }

    errno = ENOERR;
    rtctime.hour = atoi(&datestr2[17]);
    if (errno == ERANGE) {
        goto err;
    }

    errno = ENOERR;
    rtctime.minute = atoi(&datestr2[20]);
    if (errno == ERANGE) {
        goto err;
    }

    errno = ENOERR;
    rtctime.second = atoi(&datestr2[23]);
    if (errno == ERANGE) {
        goto err;
    }

    serversec = RTC_ConvertDateTimeToSecond(&rtcdate, &rtctime);
    if (serversec == -1) {
        goto err;
    }

    DWCnastimediff = localsec - serversec;
    DWCnastimediffbase = localsec;
    DWCnastimediffvalid = TRUE;
    return TRUE;
err:
    return FALSE;
}
