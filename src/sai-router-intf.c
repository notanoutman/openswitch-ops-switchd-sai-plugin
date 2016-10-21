/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-api-class.h>
#include <sai-router-intf.h>
#include <sai-port.h>
#include <netdev.h>

VLOG_DEFINE_THIS_MODULE(sai_router_intf);


struct hmap l3if_hash = HMAP_INITIALIZER(&l3if_hash);

struct rif_entry {
    struct hmap_node rif_hmap_node;
    sai_object_id_t rif_id;
    enum router_intf_type type;
    handle_t handle;
};

/*
 * Find router interface entry in hash map.
 *
 * @param[in] rif_hmap        - Hash map.
 * @param[in] rif_handle      - Router interface handle used as map key.
 */
static struct rif_entry*
__sai_router_intf_entry_hmap_find(struct hmap *rif_hmap, const handle_t *rif_handle)
{
    struct rif_entry* p_rif_entry = NULL;

    ovs_assert(rif_handle);

    HMAP_FOR_EACH_WITH_HASH(p_rif_entry, rif_hmap_node,
                            hash_uint64(rif_handle->data), rif_hmap) {
        if (p_rif_entry->rif_id == rif_handle->data) {
            return p_rif_entry;
        }
    }

    return NULL;
}

struct rif_entry*
ops_sai_router_intf_get_by_rifid(const handle_t *rif_handle)
{
    return __sai_router_intf_entry_hmap_find(&l3if_hash, rif_handle);
}

struct rif_entry*
ops_sai_router_intf_update_ipaddr(const char *prefix)
{
    return NULL;
}

/*
 * Add router interface entry to hash map.
 *
 * @param[in] rif_hmap        - Hash map.
 * @param[in] rif_handle      - Router interface handle used as map key.
 * @param[in] rif_entry       - Router interface entry.
 */
static void
__sai_router_intf_entry_hmap_add(struct hmap *rif_hmap,
                             const handle_t *rif_handle,
                             const struct rif_entry* rif_entry)
{
    struct rif_entry *rif_entry_int = NULL;

    ovs_assert(!__sai_router_intf_entry_hmap_find(rif_hmap, rif_handle));

    rif_entry_int = xzalloc(sizeof(*rif_entry_int));
    memcpy(rif_entry_int, rif_entry, sizeof(*rif_entry_int));

    hmap_insert(rif_hmap, &rif_entry_int->rif_hmap_node,
                hash_uint64(rif_handle->data));
}

/*
 * Delete router interface entry from hash map.
 *
 * @param[in] rif_hmap        - Hash map.
 * @param[in] rif_handle      - Router interface handle used as map key.
 */
static void
__sai_router_intf_entry_hmap_del(struct hmap *rif_hmap, const handle_t *rif_handle)
{
    struct rif_entry* rif_entry = __sai_router_intf_entry_hmap_find(rif_hmap,
                                                                rif_handle);
    if (rif_entry) {
        hmap_remove(rif_hmap, &rif_entry->rif_hmap_node);
        free(rif_entry);
    }
}

/*
 * Returns string representation of router interface type.
 *
 * @param[in] type - Router interface type.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
const char *ops_sai_router_intf_type_to_str(enum router_intf_type type)
{
    const char *str = NULL;

    switch(type) {
    case ROUTER_INTF_TYPE_PORT:
        str = "port";
        break;
    case ROUTER_INTF_TYPE_VLAN:
        str = "vlan";
        break;
    default:
        str = "unknown";
        break;
    }

    return str;
}

/*
 * Initializes router interface.
 */
static void __router_intf_init(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

/*
 * Creates router interface.
 *
 * @param[in] vrid_handle - Virtual router id.
 * @param[in] type        - Router interface type.
 * @param[in] handle      - Router interface handle (Port lable ID or VLAN ID).
 * @param[in] addr        - Router interface MAC address. If not specified
 *                          default value will be used.
 * @param[in] mtu         - Router interface MTU. If not specified default
 *                          value will be used
 * @param[out] rif_handle - Router interface handle.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __router_intf_create(const handle_t *vr_handle,
                               enum router_intf_type type,
                               const handle_t *handle,
                               const struct ether_addr *addr,
                               uint16_t mtu, handle_t *rif_handle)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    struct rif_entry router_intf;
    sai_attribute_t attr[4];
    sai_object_id_t rif_id = 0;

    memset(&router_intf, 0, sizeof(router_intf));
    memset(attr, 0, sizeof(attr));

    attr[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attr[0].value.oid = 0;

    if (ROUTER_INTF_TYPE_PORT == type) {
        attr[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        attr[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
        attr[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
        attr[2].value.oid = handle->data;						/* handle_t == sai_object_id_t */
    } else {
        attr[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        attr[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
        attr[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
        attr[2].value.u16 = handle->data;
    }

    attr[3].id = SAI_ROUTER_INTERFACE_ATTR_MTU;
    attr[3].value.u32 = 1514;

    status = sai_api->rif_api->create_router_interface(&rif_id, 4, attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to create router interface");
    rif_handle->data = rif_id;

    router_intf.rif_id = rif_id;
    router_intf.type = type;
    memcpy(&router_intf.handle, handle, sizeof(router_intf.handle));

    __sai_router_intf_entry_hmap_add(&l3if_hash, rif_handle, &router_intf);

exit:
    return 0;
}

/*
 * Removes router interface.
 *
 * @param[in] rif_handle - Router interface handle.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __router_intf_remove(handle_t *rifid_handle)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    status = sai_api->rif_api->remove_router_interface(rifid_handle->data);

    SAI_ERROR_LOG_EXIT(status, "Failed to remove router interface");

    __sai_router_intf_entry_hmap_del(&l3if_hash, rifid_handle);

exit:
    return status;
}

/*
 * Set router interface admin state.
 *
 * @param[in] rif_handle - Router interface handle.
 * @param[in] state      - Router interface state.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __router_intf_set_state(const handle_t *rif_handle, bool state)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_status_t            status = SAI_STATUS_SUCCESS;
    sai_attribute_t         attr[1];

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE;
    attr[0].value.booldata = state;
    status = sai_api->rif_api->set_router_interface_attribute(rif_handle->data, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set router interface state");
exit:
    return status;
}

/*
 * Get router interface statistics.
 *
 * @param[in] rif_handle - Router interface handle.
 * @param[out] stats     - Router interface statisctics.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __router_intf_get_stats(const handle_t *rif_handle,
                                  struct netdev_stats *stats)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
    return 0;
}

/*
 * De-initializes router interface.
 */
static void __router_intf_deinit(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

DEFINE_GENERIC_CLASS(struct router_intf_class, router_intf) = {
        .init = __router_intf_init,
        .create = __router_intf_create,
        .remove = __router_intf_remove,
        .set_state = __router_intf_set_state,
        .get_stats = __router_intf_get_stats,
        .deinit = __router_intf_deinit
};

DEFINE_GENERIC_CLASS_GETTER(struct router_intf_class, router_intf);
