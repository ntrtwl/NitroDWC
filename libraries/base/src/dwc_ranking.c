#include <nitro.h>

#include <string.h>
#include <stdlib.h>

#include <base/dwc_account.h>
#include <base/dwc_ranking.h>
#include <base/dwc_error.h>
#include <base/dwc_memfunc.h>

#include <auth/dwc_http.h>

#include <ranking/dwc_ranksession.h>

typedef union {
    char data[84];
    struct {
        char secretkey[20];
        char randkey_1[8];
        char randkey_2[8];
        char randkey_3[8];
        char randkey_4[8];
        char gamename[32];
    } info;
} DWCiRankingInitData;

typedef struct {
    u32 size;
    DWCRnkGetMode mode;
    union {
        void * header;
        struct {
            u32 count;
            u32 total;
            DWCRnkData rankingdata;
        } * listheader;
        struct {
            u32 order;
            u32 total;
        } * orderheader;
    };
} DWCiRankingResponse;

extern DWCHttpParam DWCauthhttpparam;

struct {
    DWCRnkState state;
    s32 pid;
} g_rankinginfo = {
    DWC_RNK_STATE_NOTREADY,
};

DWCRnkError DWCi_RankingGetResponse(DWCiRankingResponse * out);

DWCRnkError DWCi_RankingGetResponse (DWCiRankingResponse * out)
{
    void * buf;
    u32 size;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if (g_rankinginfo.state != DWC_RNK_STATE_COMPLETED) {
        return DWC_RNK_ERROR_NOTCOMPLETED;
    }

    buf = DWCi_RankingSessionGetResponse(&size);

    if (size == 0) {
        return DWC_RNK_ERROR_EMPTY_RESPONSE;
    }

    out->size = size;
    out->mode = (DWCRnkGetMode)((u32 *)buf)[0];
    out->header = &((u32 *)buf)[1];

    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkInitialize (const char * initdata, const DWCUserData * userdata)
{

    u32 randkey_1, randkey_2, randkey_3, randkey_4;
    DWCiRankingInitData * ptr;
    char secretkey[21];
    char buf[9] = "";

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if (g_rankinginfo.state != DWC_RNK_STATE_NOTREADY) {
        return DWC_RNK_ERROR_INIT_ALREADYINITIALIZED;
    }

    if (!DWC_CheckUserData(userdata) || !DWC_CheckHasProfile(userdata)) {
        return DWC_RNK_ERROR_INIT_INVALID_USERDATA;
    }

    if (strlen(initdata) >= sizeof(DWCiRankingInitData)) {
        return DWC_RNK_ERROR_INIT_INVALID_INITDATASIZE;
    }

    ptr = (DWCiRankingInitData *)initdata;

    strncpy(secretkey, ptr->info.secretkey, 20);
    secretkey[20] = '\0';

    randkey_1 = strtoul(strncpy(buf, ptr->info.randkey_1, 8), NULL, 16);
    randkey_2 = strtoul(strncpy(buf, ptr->info.randkey_2, 8), NULL, 16);
    randkey_3 = strtoul(strncpy(buf, ptr->info.randkey_3, 8), NULL, 16);
    randkey_4 = strtoul(strncpy(buf, ptr->info.randkey_4, 8), NULL, 16);

    if (!DWCi_RankingValidateKey(ptr->info.gamename,
                                 secretkey,
                                 randkey_1,
                                 randkey_2,
                                 randkey_3,
                                 randkey_4)) {

        return DWC_RNK_ERROR_INIT_INVALID_INITDATA;

    }

    g_rankinginfo.pid = userdata->gs_profile_id;

    if (strcmp(DWCauthhttpparam.url, DWC_AUTH_NAS_URL) == 0) {
        DWCi_RankingSessionInitialize(TRUE);
    } else {
        DWCi_RankingSessionInitialize(FALSE);
    }

    g_rankinginfo.state = DWC_RNK_STATE_INITIALIZED;
    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkShutdown (void)
{
    DWCi_RankingSessionShutdown();
    g_rankinginfo.state = DWC_RNK_STATE_NOTREADY;

    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkPutScoreAsync (u32 category, DWCRnkRegion region, s32 score, void * data, u32 size)
{
    DWCiRankingSessionResult res;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if ((g_rankinginfo.state != DWC_RNK_STATE_INITIALIZED) && (g_rankinginfo.state != DWC_RNK_STATE_COMPLETED)) {
        return DWC_RNK_ERROR_PUT_NOTREADY;
    }

    if ((category > DWC_RNK_CATEGORY_MAX) || (size > DWC_RNK_DATA_MAX)) {
        return DWC_RNK_ERROR_INVALID_PARAMETER;
    }

    if ((data == NULL) && (size != 0)) {
        return DWC_RNK_ERROR_INVALID_PARAMETER;
    }

    res = DWCi_RankingSessionPutAsync(category,
                                      g_rankinginfo.pid,
                                      region,
                                      score,
                                      data,
                                      size);

    switch (res) {
    case DWCi_RANKING_SESSION_ERROR_INVALID_KEY:
        return DWC_RNK_ERROR_PUT_INVALID_KEY;
    case DWCi_RANKING_SESSION_ERROR_NOMEMORY:
        return DWC_RNK_ERROR_PUT_NOMEMORY;
    }

    g_rankinginfo.state = DWC_RNK_STATE_PUT_ASYNC;
    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkGetScoreAsync (DWCRnkGetMode mode,
                                  u32 category,
                                  DWCRnkRegion region,
                                  DWCRnkGetParam * param)
{

    DWCiRankingSessionResult res;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if ((g_rankinginfo.state != DWC_RNK_STATE_INITIALIZED) && (g_rankinginfo.state != DWC_RNK_STATE_COMPLETED)) {
        return DWC_RNK_ERROR_GET_NOTREADY;
    }

    if ((category > DWC_RNK_CATEGORY_MAX) || (param == NULL)) {
        return DWC_RNK_ERROR_INVALID_PARAMETER;
    }

    switch (mode) {
    case DWC_RNK_GET_MODE_ORDER:
        if (param->size != sizeof(param->order)) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        switch (param->order.sort) {
        case DWC_RNK_ORDER_ASC:
        case DWC_RNK_ORDER_DES:
            break;
        default:
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }
        break;
    case DWC_RNK_GET_MODE_TOPLIST:
        if (param->size != sizeof(param->toplist)) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        switch (param->toplist.sort) {
        case DWC_RNK_ORDER_ASC:
        case DWC_RNK_ORDER_DES:
            break;
        default:
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        if (param->toplist.limit > DWC_RNK_GET_MAX || param->toplist.limit == 0) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        break;
    case DWC_RNK_GET_MODE_NEAR:
    case DWC_RNK_GET_MODE_NEAR_HI:
    case DWC_RNK_GET_MODE_NEAR_LOW:
        if (param->size != sizeof(param->near)) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        switch (param->near.sort) {
        case DWC_RNK_ORDER_ASC:
        case DWC_RNK_ORDER_DES:
            break;
        default:
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        if (param->near.limit > DWC_RNK_GET_MAX || param->near.limit <= 1) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }
        break;
    case DWC_RNK_GET_MODE_FRIENDS:
        if (param->size != sizeof(param->friends)) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        switch (param->friends.sort) {
        case DWC_RNK_ORDER_ASC:
        case DWC_RNK_ORDER_DES:
            break;
        default:
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }

        if (param->friends.limit > (DWC_RNK_FRIENDS_MAX + 1) ||
            param->friends.limit <= 1) {
            return DWC_RNK_ERROR_INVALID_PARAMETER;
        }
        break;
    }

    res = DWCi_RankingSessionGetAsync(mode,
                                      category,
                                      g_rankinginfo.pid,
                                      region,
                                      param);

    switch (res) {

    case DWCi_RANKING_SESSION_ERROR_INVALID_KEY:
        return DWC_RNK_ERROR_GET_INVALID_KEY;

    case DWCi_RANKING_SESSION_ERROR_NOMEMORY:
        return DWC_RNK_ERROR_GET_NOMEMORY;
    }

    g_rankinginfo.state = DWC_RNK_STATE_GET_ASYNC;

    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkCancelProcess (void)
{
    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if ((g_rankinginfo.state != DWC_RNK_STATE_PUT_ASYNC) &&
        (g_rankinginfo.state != DWC_RNK_STATE_GET_ASYNC)) {
        return DWC_RNK_ERROR_CANCEL_NOTASK;
    }

    DWCi_RankingSessionCancel();

    g_rankinginfo.state = DWC_RNK_STATE_ERROR;
    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkProcess (void)
{
    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    if ((g_rankinginfo.state != DWC_RNK_STATE_PUT_ASYNC) &&
        (g_rankinginfo.state != DWC_RNK_STATE_GET_ASYNC)) {
        return DWC_RNK_PROCESS_NOTASK;
    }

    switch (DWCi_RankingSessionProcess()) {

    case DWCi_RANKING_SESSION_STATE_ERROR:
        g_rankinginfo.state = DWC_RNK_STATE_ERROR;
        break;
    case DWCi_RANKING_SESSION_STATE_CANCELED:
        g_rankinginfo.state = DWC_RNK_STATE_ERROR;
        break;
    case DWCi_RANKING_SESSION_STATE_INITIAL:
        break;
    case DWCi_RANKING_SESSION_STATE_REQUEST:
    case DWCi_RANKING_SESSION_STATE_GETTING_TOKEN:
    case DWCi_RANKING_SESSION_STATE_GOT_TOKEN:
    case DWCi_RANKING_SESSION_STATE_SENDING_DATA:
        break;
    case DWCi_RANKING_SESSION_STATE_COMPLETED:
        g_rankinginfo.state = DWC_RNK_STATE_COMPLETED;
        break;
    }

    return DWC_RNK_SUCCESS;
}

DWCRnkState DWC_RnkGetState (void)
{
    return g_rankinginfo.state;
}

DWCRnkError DWC_RnkResGetRow (DWCRnkData * out, u32 index)
{
    DWCRnkData * ptr;
    DWCiRankingResponse res;
    DWCRnkError err;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    err = DWCi_RankingGetResponse(&res);
    if (err != DWC_RNK_SUCCESS)
        return err;

    switch (res.mode) {
    case DWC_RNK_GET_MODE_ORDER:
        return DWC_RNK_ERROR_INVALID_MODE;
    case DWC_RNK_GET_MODE_TOPLIST:
    case DWC_RNK_GET_MODE_NEAR:
    case DWC_RNK_GET_MODE_FRIENDS:
    case DWC_RNK_GET_MODE_NEAR_HI:
    case DWC_RNK_GET_MODE_NEAR_LOW:
        break;
    }

    if (out == NULL)
        return DWC_RNK_ERROR_INVALID_PARAMETER;

    if (index >= res.listheader->count)
        return DWC_RNK_ERROR_INVALID_PARAMETER;

    ptr = &res.listheader->rankingdata;

    while (index-- > 0) {
        ptr = (DWCRnkData *)(((u8 *)&ptr->userdata) + ptr->size);
    }

    if ((u32) & ptr->userdata + ptr->size > (u32)res.header + res.size) {
        return DWC_RNK_ERROR_INVALID_PARAMETER;
    }

    *out = *ptr;
    out->userdata = &ptr->userdata;

    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkResGetRowCount (u32 * count)
{
    DWCiRankingResponse res;
    DWCRnkError err;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    err = DWCi_RankingGetResponse(&res);
    if (err != DWC_RNK_SUCCESS)
        return err;

    if (count == NULL)
        return DWC_RNK_ERROR_INVALID_PARAMETER;

    switch (res.mode) {
    case DWC_RNK_GET_MODE_ORDER:
        return DWC_RNK_ERROR_INVALID_MODE;
    case DWC_RNK_GET_MODE_TOPLIST:
    case DWC_RNK_GET_MODE_NEAR:
    case DWC_RNK_GET_MODE_FRIENDS:
    case DWC_RNK_GET_MODE_NEAR_HI:
    case DWC_RNK_GET_MODE_NEAR_LOW:
        break;
    }

    *count = res.listheader->count;
    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkResGetOrder (u32 * order)
{
    DWCiRankingResponse res;
    DWCRnkError err;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    err = DWCi_RankingGetResponse(&res);
    if (err != DWC_RNK_SUCCESS)
        return err;

    if (order == NULL)
        return DWC_RNK_ERROR_INVALID_PARAMETER;

    if (res.mode != DWC_RNK_GET_MODE_ORDER)
        return DWC_RNK_ERROR_INVALID_MODE;

    *order = res.orderheader->order;
    return DWC_RNK_SUCCESS;
}

DWCRnkError DWC_RnkResGetTotal (u32 * total)
{
    DWCiRankingResponse res;
    DWCRnkError err;

    if (DWCi_IsError()) return DWC_RNK_IN_ERROR;

    err = DWCi_RankingGetResponse(&res);
    if (err != DWC_RNK_SUCCESS)
        return err;

    if (total == NULL)
        return DWC_RNK_ERROR_INVALID_PARAMETER;

    *total = res.orderheader->total;

    return DWC_RNK_SUCCESS;
}