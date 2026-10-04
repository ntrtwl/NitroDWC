#ifndef DWC_AC_MAKELIST_H
#define DWC_AC_MAKELIST_H

#include <nitro.h>

enum SearchListType {
    SEARCH_LIST_TYPE_AROUND = 0,
    SEARCH_LIST_TYPE_DIFFER_CHANNEL,
    SEARCH_LIST_TYPE_STEALTH,
};

u8 DWCi_AC_MakeSearchList(int type);

#endif // DWC_AC_MAKELIST_H
