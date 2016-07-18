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
#include <sai-stp.h>

VLOG_DEFINE_THIS_MODULE(sai_stp);

typedef enum mstp_instance_port_state {
    MSTP_INST_PORT_STATE_DISABLED = 0,
    MSTP_INST_PORT_STATE_BLOCKED,
    MSTP_INST_PORT_STATE_LEARNING,
    MSTP_INST_PORT_STATE_FORWARDING,
    MSTP_INST_PORT_STATE_INVALID,
}mstp_instance_port_state_t;

struct hstp_entry {
    struct hmap_node    hmap_node;
    handle_t            handle;
    int                 stpid;
};

static struct hmap all_stp              = HMAP_INITIALIZER(&all_stp);
static int    default_stp_ins_id        = 0;
static unsigned long *bitmap_index      = NULL;

static void __stp_init(void);
static void __stp_deinit(void);
static  int __stp_create(int *);
static  int __stp_remove(int);
static  int __stp_add_vlan(int,int);
static  int __stp_remove_vlan(int,int);
static  int __stp_set_port_state(int,uint32_t,int);
static  int __stp_get_port_state(int,uint32_t,int*);
static  int __stp_get_default_instance(int*);

static int
__stp_idle_index_init(unsigned long **bitmap_index)
{
    *bitmap_index = bitmap_allocate1(INT_MAX);

    bitmap_set0(*bitmap_index, 0);           /* skip index 0 */

    return 0;
}

static int
__stp_idle_index_get(unsigned long *bitmap_index)
{
    int idle_index = 0;

    BITMAP_FOR_EACH_1 (idle_index, INT_MAX, bitmap_index) {
        bitmap_set0(bitmap_index, idle_index);
        return idle_index;
    }

    return -1;
}

static int
__stp_idle_index_put(unsigned long *bitmap_index, int index)
{
    bitmap_set1(bitmap_index, index);
    return -1;
}

static bool
__stp_get_port_state_2_hw_port_state(int port_state, sai_port_stp_port_state_t *hw_port_state)
{
    bool retval = false;

    if(!hw_port_state) {
        return retval;
    }

    switch (port_state) {
        case MSTP_INST_PORT_STATE_BLOCKED:
            *hw_port_state = SAI_PORT_STP_STATE_BLOCKING;
            retval = true;
            break;
        case MSTP_INST_PORT_STATE_DISABLED:
            *hw_port_state = SAI_PORT_STP_STATE_BLOCKING;
            retval = true;
            break;
        case MSTP_INST_PORT_STATE_LEARNING:
            *hw_port_state = SAI_PORT_STP_STATE_LEARNING;
            retval = true;
            break;
        case MSTP_INST_PORT_STATE_FORWARDING:
            *hw_port_state = SAI_PORT_STP_STATE_FORWARDING;
            retval = true;
            break;
        default:
            break;
    }

    return retval;
}

static bool
__stp_hw_port_state_2_port_state(sai_port_stp_port_state_t hw_port_state, int *port_state)
{
    bool retval = false;

    if(!port_state) {
        return retval;
    }

    switch (hw_port_state) {
        case SAI_PORT_STP_STATE_BLOCKING:
            *port_state = MSTP_INST_PORT_STATE_BLOCKED;
            retval = true;
            break;
        case SAI_PORT_STP_STATE_LEARNING:
            *port_state =  MSTP_INST_PORT_STATE_LEARNING;
            retval = true;
            break;
        case SAI_PORT_STP_STATE_FORWARDING:
            *port_state =  MSTP_INST_PORT_STATE_FORWARDING;
            retval = true;
            break;
        default:
            *port_state = MSTP_INST_PORT_STATE_INVALID;
            retval = false;
            break;
    }

    return retval;
}

static struct hstp_entry*
__stp_entry_hmap_find_create(struct hmap *hstp_hmap, int stpid, bool is_create)
{
    struct hstp_entry* hstp_entry  = NULL;
    int                _cur_stp_id = 0;

    NULL_PARAM_LOG_ABORT(hstp_hmap);

    HMAP_FOR_EACH(hstp_entry, hmap_node, hstp_hmap) {
        if (hstp_entry->stpid == stpid) {
            return hstp_entry;
        }
    }

    if (false == is_create) {
        return NULL;
    }

    _cur_stp_id = __stp_idle_index_get(bitmap_index);

    if(-1 == _cur_stp_id) {
        return NULL;
    }

    hstp_entry = xzalloc(sizeof(struct hstp_entry));
    if (!hstp_entry) {
        VLOG_ERR("Failed to allocate memory for stpid =%d",stpid);

        __stp_idle_index_put(bitmap_index, _cur_stp_id);
        return NULL;
    }

    hstp_entry->stpid = _cur_stp_id;

    hmap_insert(&all_stp, &hstp_entry->hmap_node, hash_int(hstp_entry->stpid, 0));

    return hstp_entry;
}

static void
__stp_entry_hmap_remove(struct hmap *hstp_hmap, struct hstp_entry *hstp_entry)
{
    hmap_remove(hstp_hmap,&hstp_entry->hmap_node);

    __stp_idle_index_put(bitmap_index, hstp_entry->stpid);

    free(hstp_entry);
}

/*
 * Initialize STPs.
 */
static void
__stp_init(void)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    struct hstp_entry               *hstp_entry = NULL;
    sai_attribute_t                 attr;

    VLOG_INFO("Initializing STPs");

    memset(&attr,0,sizeof(sai_attribute_t));

    __stp_idle_index_init(&bitmap_index);

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, -1, true);

    if(!hstp_entry)
        return;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID;

    status = sai_api->switch_api->get_switch_attribute(1,&attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to __stp_init lag");

    hstp_entry->handle.data = attr.value.oid;

    default_stp_ins_id = hstp_entry->stpid;

exit:
    return ;
}

/*
 * De-initialize STPs.
 */
static void
__stp_deinit(void)
{
    VLOG_INFO("De-initializing STPs");
}

/*
 * Creates STP.
 */
static int
__stp_create(int *stpid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, *stpid, true);

    if(!hstp_entry)
        return -1;

    status = sai_api->stp_api->create_stp(&hstp_entry->handle.data,0,NULL);
    SAI_ERROR_LOG_EXIT(status, "Failed to create lag");

    *stpid = hstp_entry->stpid;
exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Removes STP.
 */
static int
__stp_remove(int stpid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_INFO("Removes Lags");

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, stpid, false);

    if(!hstp_entry) {
        VLOG_WARN("Deleting non-existing STP, stpid=%d", stpid);
        return -1;
    }

    status = sai_api->stp_api->remove_stp(hstp_entry->handle.data);

    SAI_ERROR_LOG_EXIT(status, "Failed to remove STP, stpid=%d", stpid);

    __stp_entry_hmap_remove(&all_stp, hstp_entry);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Add vlan to stp.
 */
static int
__stp_add_vlan(int stpid, int vid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t                 attr;

    VLOG_INFO("__stp_add_vlan");

    memset(&attr,0,sizeof(sai_attribute_t));

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, stpid, false);

    if(!hstp_entry) {
        VLOG_WARN("non-existing STP, stpid=%d", stpid);
        return -1;
    }

    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = hstp_entry->handle.data;
    status = sai_api->vlan_api->set_vlan_attribute(vid, &attr);

    SAI_ERROR_LOG_EXIT(status, "stp vlan %d add error, stpid=%d",vid,stpid);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Remove vlan from stp.
 */
static int
__stp_remove_vlan(int stpid, int vid)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t                 attr;

    VLOG_INFO("__stp_remove_vlan");

    memset(&attr,0,sizeof(sai_attribute_t));

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, stpid, false);

    if(!hstp_entry) {
        VLOG_WARN("non-existing STP, stpid=%d", stpid);
        return -1;
    }

    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid =
        __stp_entry_hmap_find_create(&all_stp,default_stp_ins_id,false)->handle.data;
    status = sai_api->vlan_api->set_vlan_attribute(vid, &attr);

    SAI_ERROR_LOG_EXIT(status, "stp vlan %d add error, stpid=%d",vid,stpid);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Set port state in stp.
 */
static int
__stp_set_port_state(int  stpid,
                         uint32_t  hw_id,
                         int  port_state)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t                 attr;
    sai_port_stp_port_state_t       hw_stp_state = 0;

    VLOG_INFO("__stp_set_port_state");

    memset(&attr,0,sizeof(sai_attribute_t));

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, stpid, false);

    if(!hstp_entry) {
        VLOG_WARN("non-existing STP, stpid=%d", stpid);
        return -1;
    }

    __stp_get_port_state_2_hw_port_state(port_state, &hw_stp_state);

    status = sai_api->stp_api->set_stp_port_state(hstp_entry->handle.data,
                                                  ops_sai_api_hw_id2port_id(hw_id),
                                                  hw_stp_state);

    SAI_ERROR_LOG_EXIT(status, "stp %d port %d state %d set error",stpid, hw_id, port_state);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Get port state in stp.
 */
static int
__stp_get_port_state(int  stpid,
                         uint32_t  hw_id,
                         int  *port_state)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct hstp_entry   *hstp_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t                 attr;
    sai_port_stp_port_state_t       hw_stp_state = 0;

    VLOG_INFO("__stp_get_port_state");

    memset(&attr,0,sizeof(sai_attribute_t));

    hstp_entry = __stp_entry_hmap_find_create(&all_stp, stpid, false);

    if(!hstp_entry) {
        VLOG_WARN("non-existing STP, stpid=%d", stpid);
        return -1;
    }

    status = sai_api->stp_api->get_stp_port_state(hstp_entry->handle.data,
                                                  ops_sai_api_hw_id2port_id(hw_id),
                                                  &hw_stp_state);

    SAI_ERROR_LOG_EXIT(status, "stp %d port %d state get error",stpid, hw_id);

    __stp_hw_port_state_2_port_state(hw_stp_state, port_state);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Get default STP.
 */
static int
__stp_get_default_instance(int *stpid)
{
    *stpid = default_stp_ins_id;
    return 0;
}

DEFINE_GENERIC_CLASS(struct stp_class, stp) = {
        .init            = __stp_init,
        .create          = __stp_create,
        .remove          = __stp_remove,
        .add_vlan        = __stp_add_vlan,
        .remove_vlan     = __stp_remove_vlan,
        .set_port_state  = __stp_set_port_state,
        .get_port_state  = __stp_get_port_state,
        .get_default_instance = __stp_get_default_instance,
        .deinit          = __stp_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct stp_class, stp);
