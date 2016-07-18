/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */
#ifndef __BOOL_DEFINED
#define __BOOL_DEFINED
#endif

#include <hmap.h>
#include <hash.h>

#include <ofproto/ofproto.h>

#include <sai-log.h>
#include <sai-api-class.h>
#include <sai-lag.h>

VLOG_DEFINE_THIS_MODULE(sai_lag);

struct hlag_entry {
    struct hmap_node    hmap_node;
    handle_t            handle;
    int                 lagid;
    struct hmap         hmap_member_ports;
};

struct hlag_member_entry {
    struct hmap_node    hmap_node;
    handle_t            handle;
    uint32_t            hw_id;
};

static struct hmap all_lag = HMAP_INITIALIZER(&all_lag);

static unsigned long *bitmap_index = NULL;

void __lag_init(void);
void __lag_deinit(void);
int  __lag_create(int *lagid);
int  __lag_remove(int lagid);
int  __lag_member_port_add(int,uint32_t);
int  __lag_member_port_del(int,uint32_t);
int  __lag_set_tx_enable(int,uint32_t,bool);
int  __lag_set_balance_mode(int,int);

static int
__lag_idle_index_init(unsigned long **bitmap_index)
{
    *bitmap_index = bitmap_allocate1(INT_MAX);

    return 0;
}

static int
__lag_idle_index_get(unsigned long *bitmap_index)
{
    int idle_index = 0;

    BITMAP_FOR_EACH_1 (idle_index, INT_MAX, bitmap_index) {
        bitmap_set0(bitmap_index, idle_index);
        return idle_index;
    }

    return -1;
}

static int
__lag_idle_index_put(unsigned long *bitmap_index, int index)
{
    bitmap_set1(bitmap_index, index);
    return -1;
}

static struct hlag_entry*
__lag_entry_hmap_find_create(struct hmap *hlag_hmap, int lagid, bool is_create)
{
    struct hlag_entry* hlag_entry = NULL;
    int                _cur_lag_id = 0;;

    NULL_PARAM_LOG_ABORT(hlag_hmap);

    HMAP_FOR_EACH(hlag_entry, hmap_node, hlag_hmap) {
        if (hlag_entry->lagid == lagid) {
            return hlag_entry;
        }
    }

    if (false == is_create) {
        return NULL;
    }

    _cur_lag_id = __lag_idle_index_get(bitmap_index);

    if(-1 == _cur_lag_id) {
        return NULL;
    }

    hlag_entry = xzalloc(sizeof(struct hlag_entry));
    if (!hlag_entry) {
        VLOG_ERR("Failed to allocate memory for LAG id =%d",lagid);

        __lag_idle_index_put(bitmap_index, _cur_lag_id);
        return NULL;
    }

    hlag_entry->lagid = _cur_lag_id;

    hmap_init(&hlag_entry->hmap_member_ports);

    hmap_insert(&all_lag, &hlag_entry->hmap_node, hash_int(hlag_entry->lagid, 0));

    return hlag_entry;
}

static void
__lag_entry_hmap_remove(struct hmap *hlag_hmap, struct hlag_entry *hlag_entry)
{
    struct hlag_member_entry* hlag_member_entry = NULL;
    struct hlag_member_entry* hlag_member_entry_next = NULL;

    HMAP_FOR_EACH_SAFE(hlag_member_entry,hlag_member_entry_next,
                       hmap_node, &hlag_entry->hmap_member_ports) {
        hmap_remove(&hlag_entry->hmap_member_ports,&hlag_member_entry->hmap_node);
        free(hlag_member_entry);
        hlag_member_entry = NULL;
    }

    hmap_destroy(&hlag_entry->hmap_member_ports);
    hmap_remove(hlag_hmap,&hlag_entry->hmap_node);

    __lag_idle_index_put(bitmap_index, hlag_entry->lagid);

    free(hlag_entry);
}

static void
__lag_member_hmap_add(struct hmap *hlag_member_hmap,
                               const struct hlag_member_entry *hlag_member_entry)
{
    struct hlag_member_entry *hlag_member_entry_int = NULL;

    NULL_PARAM_LOG_ABORT(hlag_member_hmap);
    NULL_PARAM_LOG_ABORT(hlag_member_entry);

    hlag_member_entry_int = xzalloc(sizeof(*hlag_member_entry_int));
    memcpy(hlag_member_entry_int, hlag_member_entry, sizeof(*hlag_member_entry_int));

    hmap_insert(hlag_member_hmap, &hlag_member_entry_int->hmap_node,
                hash_int(hlag_member_entry_int->hw_id, 0));
}

static struct hlag_member_entry*
__lag_member_hmap_find(struct hmap *hlag_member_hmap,
                               uint32_t hw_id)
{
    struct hlag_member_entry *hlag_member_entry = NULL;

    HMAP_FOR_EACH(hlag_member_entry, hmap_node, hlag_member_hmap) {
        if (hlag_member_entry->hw_id == hw_id) {
            return hlag_member_entry;
        }
    }

    return NULL;
}

static void
__lag_member_hmap_del(struct hmap *hlag_member_hmap,
                              struct hlag_member_entry *hlag_member_entry)
{
    NULL_PARAM_LOG_ABORT(hlag_member_hmap);
    NULL_PARAM_LOG_ABORT(hlag_member_entry);

    hmap_remove(hlag_member_hmap,&hlag_member_entry->hmap_node);
    free(hlag_member_entry);
}

/*
 * Initialize Lags.
 */
void
__lag_init(void)
{
    VLOG_INFO("Initializing Lags");

    __lag_idle_index_init(&bitmap_index);
}

/*
 * De-initialize Lags.
 */
void
__lag_deinit(void)
{
    VLOG_INFO("De-initializing Lags");
}

/*
 * Creates Lag.
 */
int
__lag_create(int *lagid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hlag_entry   *hlag_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_INFO("Creates Lags");

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, *lagid, true);

    if(!hlag_entry)
        return -1;

    status = sai_api->lag_api->create_lag(&hlag_entry->handle.data, 0, NULL);

    SAI_ERROR_LOG_EXIT(status, "Failed to create lag");

    *lagid = hlag_entry->lagid;
exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Removes Lag.
 */
int
__lag_remove(int lagid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hlag_entry   *hlag_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_INFO("Removes Lags");

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, lagid, false);

    if(!hlag_entry) {
        VLOG_WARN("Deleting non-existing LAG, LAG_ID=%d", lagid);
        return -1;
    }

    status = sai_api->lag_api->remove_lag(hlag_entry->handle.data);

    SAI_ERROR_LOG_EXIT(status, "Failed to remove LAG, LAG_ID=%d", lagid);

    __lag_entry_hmap_remove(&all_lag, hlag_entry);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Adds member port to lag.
 */
int
__lag_member_port_add(int        lagid,
                             uint32_t   hw_id)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    struct hlag_entry               *hlag_entry = NULL;
    struct hlag_member_entry        hlag_member_entry;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_attribute_t                 attr[2];

    VLOG_INFO("Adds member port to lag. hw_port=%d, tid=%d",
               hw_id, lagid);

    memset(&hlag_member_entry,0,sizeof(struct hlag_member_entry));
    memset(attr,0,sizeof(sai_attribute_t) * 2);

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, lagid, false);

    if(!hlag_entry) {
        VLOG_WARN("non-existing LAG, LAG_ID=%d", lagid);
        return -1;
    }

    attr[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attr[0].value.oid = hlag_entry->handle.data;

    attr[1].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attr[1].value.oid = ops_sai_api_hw_id2port_id(hw_id);

    status = sai_api->lag_api->create_lag_member(&hlag_member_entry.handle.data, sizeof(attr)/sizeof(attr[0]), attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to add port");

    hlag_member_entry.hw_id = hw_id;

    __lag_member_hmap_add(&hlag_entry->hmap_member_ports,&hlag_member_entry);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Removes member port from lag.
 */
int
__lag_member_port_del(int        lagid,
                            uint32_t   hw_id)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    struct hlag_entry               *hlag_entry = NULL;
    struct hlag_member_entry        *hlag_member_entry = NULL;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_attribute_t                 attr[2];

    VLOG_INFO("Removes member port from lag. hw_port=%d, tid=%d",
               hw_id, lagid);

    memset(attr,0,sizeof(sai_attribute_t) * 2);

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, lagid, false);

    if(!hlag_entry) {
        VLOG_WARN("non-existing LAG, LAG_ID=%d", lagid);
        return -1;
    }

    hlag_member_entry = __lag_member_hmap_find(&hlag_entry->hmap_member_ports,hw_id);
    if(!hlag_member_entry) {
        return 0;
    }

    status = sai_api->lag_api->remove_lag_member(hlag_member_entry->handle.data);

    SAI_ERROR_LOG_EXIT(status, "Failed to del port");

    __lag_member_hmap_del(&hlag_entry->hmap_member_ports,hlag_member_entry);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Set traffic distribution to this port as part of LAG.
 */
int
__lag_set_tx_enable(int        lagid,
                        uint32_t   hw_id,
                        bool       enable)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    struct hlag_entry               *hlag_entry = NULL;
    struct hlag_member_entry        *hlag_member_entry = NULL;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_attribute_t                 attr;

    VLOG_INFO("Set traffic distribution to this port as part of LAG. hw_port=%d, tid=%d, enable = %d",
               hw_id, lagid, enable);

    memset(&attr,0,sizeof(sai_attribute_t));

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, lagid, false);

    if(!hlag_entry) {
        VLOG_WARN("non-existing LAG, LAG_ID=%d", lagid);
        return -1;
    }

    hlag_member_entry = __lag_member_hmap_find(&hlag_entry->hmap_member_ports,hw_id);
    if(!hlag_member_entry) {
        return -1;
    }

    attr.id = SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE;

    if(enable)
    {
        attr.value.booldata = false;
    }else{
        attr.value.booldata = true;
    }

    status = sai_api->lag_api->set_lag_member_attribute(hlag_member_entry->handle.data, &attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to egress enable port to lag");

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Set balance mode from lag.
 */
int
__lag_set_balance_mode(int   lagid,
                              int   balance_mode)
{
    (void)lagid;
    (void)balance_mode;
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
    return 0;
}

int
__lag_get_handle_id(int         lagid,
                         handle_t   *handle_id)
{
    struct hlag_entry               *hlag_entry = NULL;

    VLOG_INFO("ops_sai_api_lagid2handle_id. tid=%d",lagid);

    hlag_entry = __lag_entry_hmap_find_create(&all_lag, lagid, false);

    if(!hlag_entry) {
        VLOG_WARN("non-existing LAG, LAG_ID=%d", lagid);
        return -1;
    }

    *handle_id = hlag_entry->handle;

    return 0;
}

DEFINE_GENERIC_CLASS(struct lag_class, lag) = {
        .init            = __lag_init,
        .create          = __lag_create,
        .remove          = __lag_remove,
        .member_port_add = __lag_member_port_add,
        .member_port_del = __lag_member_port_del,
        .set_tx_enable   = __lag_set_tx_enable,
        .set_balance_mode= __lag_set_balance_mode,
        .get_handle_id   = __lag_get_handle_id,
        .deinit          = __lag_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct lag_class, lag);
