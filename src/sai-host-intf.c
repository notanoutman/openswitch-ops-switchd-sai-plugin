/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <inttypes.h>
#include <string.h>
#include <util.h>
#include <hmap.h>
#include <hash.h>
#include <list.h>
#include <sai-log.h>
#include <sai-handle.h>

#include <sai-host-intf.h>
#include <sai-policer.h>
#include <sai-api-class.h>
#include <sai-port.h>
#include <sai-common.h>

#define SAI_TRAP_GROUP_ARP "sai_trap_group_arp"
#define SAI_TRAP_GROUP_BGP "sai_trap_group_bgp"
#define SAI_TRAP_GROUP_DHCP "sai_trap_group_dhcp"
#define SAI_TRAP_GROUP_DHCPV6 "sai_trap_group_dhcpv6"
#define SAI_TRAP_GROUP_LACP "sai_trap_group_lacp"
#define SAI_TRAP_GROUP_LLDP "sai_trap_group_lldp"
#define SAI_TRAP_GROUP_OSFP "sai_trap_group_osfp"
#define SAI_TRAP_GROUP_S_FLOW "sai_trap_group_s_flow"
#define SAI_TRAP_GROUP_STP "sai_trap_group_stp"
#define IFNAMSIZ            32

#define SAI_COMMAND_MAX_SIZE 512
#define SAI_DEFAULT_ETH_SWID 1

VLOG_DEFINE_THIS_MODULE(sai_host_intf);

struct hif_entry {
    struct hmap_node hmap_node;
    char name[IFNAMSIZ];
    enum host_intf_type type;
    handle_t handle;
    handle_t hostif_id;
    struct eth_addr mac;
};

static const struct ops_sai_trap_group_config trap_group_config_table[] = { {
        .name = SAI_TRAP_GROUP_ARP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_ARP_REQUEST,
            SAI_HOSTIF_TRAP_ID_ARP_RESPONSE,
            SAI_HOSTIF_TRAP_ID_IPV6_NEIGHBOR_DISCOVERY,
            -1
        },
        .priority = 2,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = true,
        .is_l3 = true,
    }, {
        .name = SAI_TRAP_GROUP_DHCP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_DHCP,
            -1
        },
        .priority = 3,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = true,
        .is_l3 = true,
    }, {
        .name = SAI_TRAP_GROUP_DHCPV6,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_DHCPV6,
            -1
        },
        .priority = 3,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = true,
        .is_l3 = true,
    }, {
        .name = SAI_TRAP_GROUP_LACP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_LACP,
            -1
        },
        .priority = 5,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = false,
        .is_l3 = false,
    }, {
        .name = SAI_TRAP_GROUP_LLDP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_LLDP,
            -1
        },
        .priority = 5,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = false,
        .is_l3 = false,
    }, {
        .name = SAI_TRAP_GROUP_OSFP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_OSPF,
            SAI_HOSTIF_TRAP_ID_OSPFV6,
            -1
        },
        .priority = 4,
        .policer_config = {
            .rate_max = 5000,
            .burst_max = 5000,
        },
        .is_log = false,
        .is_l3 = true,
    }, {
        .name = SAI_TRAP_GROUP_S_FLOW,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_SAMPLEPACKET,
            -1
        },
        .priority = 0,
        .policer_config = {
            .rate_max = 2000,
            .burst_max = 2000,
        },
        .is_log = false,
        .is_l3 = true,
    }, {
        .name = SAI_TRAP_GROUP_STP,
        .trap_ids = {
            SAI_HOSTIF_TRAP_ID_STP,
            -1
        },
        .priority = 5,
        .policer_config = {
            .rate_max = 1000,
            .burst_max = 1000,
        },
        .is_log = false,
        .is_l3 = false,
    },
};

static struct hmap sai_host_intf = HMAP_INITIALIZER(&sai_host_intf);

static struct ovs_list sai_trap_group_list
    = OVS_LIST_INITIALIZER(&sai_trap_group_list);

static void
__traps_bind(const int *, const handle_t *, bool, bool);

/**
 * Returns host interface type string representation.
 *
 * @param[in] type - host interface type.
 *
 * @return pointer to type string representation
 */
const char *
ops_sai_host_intf_type_to_str(enum host_intf_type type)
{
    const char *str = NULL;
    switch(type) {
    case HOST_INTF_TYPE_L2_PORT_NETDEV:
        str = "L2 port netdev";
        break;
    case HOST_INTF_TYPE_L3_PORT_NETDEV:
        str = "L3 port netdev";
        break;
    case HOST_INTF_TYPE_L3_VLAN_NETDEV:
        str = "L3 VLAN netdev";
        break;
    default:
        str = "unknown";
        break;
    }

    return str;
}

/*
 * Initialize host interface.
 */
static void
__host_intf_init(void)
{
    VLOG_INFO("Initializing host interface");
}

/*
 * De-initialize host interface.
 */
static void
__host_intf_deinit(void)
{
    VLOG_INFO("De-initializing host interface");
}


/*
 * Find host interface entry in hash map.
 *
 * @param[in] hif_hmap      - Hash map.
 * @param[in] hif_name      - Host interface name used as map key.
 *
 * @return pointer to host interface entry if entry found.
 * @return NULL if entry not found.
 */
static struct hif_entry*
__sai_host_intf_entry_hmap_find(struct hmap *hif_hmap, const char *hif_name)
{
    struct hif_entry* hif_entry = NULL;

    NULL_PARAM_LOG_ABORT(hif_hmap);
    NULL_PARAM_LOG_ABORT(hif_name);

    HMAP_FOR_EACH_WITH_HASH(hif_entry, hmap_node,
                            hash_string(hif_name, 0), hif_hmap) {
        if (strcmp(hif_entry->name, hif_name) == 0) {
            return hif_entry;
        }
    }

    return NULL;
}

/*
 * Add host interface entry to hash map.
 *
 * @param[in] hif_hmap        - Hash map.
 * @param[in] hif_entry       - Host interface entry.
 */
static void
__sai_host_intf_entry_hmap_add(struct hmap *hif_hmap,
                           const struct hif_entry* hif_entry)
{
    struct hif_entry *hif_entry_int = NULL;

    NULL_PARAM_LOG_ABORT(hif_hmap);
    NULL_PARAM_LOG_ABORT(hif_entry);

    ovs_assert(!__sai_host_intf_entry_hmap_find(hif_hmap, hif_entry->name));

    hif_entry_int = xzalloc(sizeof(*hif_entry_int));
    memcpy(hif_entry_int, hif_entry, sizeof(*hif_entry_int));

    hmap_insert(hif_hmap, &hif_entry_int->hmap_node,
                hash_string(hif_entry->name, 0));
}

/*
 * Delete host interface entry from hash map.
 *
 * @param[in] hif_hmap        - Hash map.
 * @param[in] name            - Host interface name used as map key.
 */
static void
__sai_host_intf_entry_hmap_del(struct hmap *hif_hmap, const char *name)
{
    struct hif_entry* hif_entry = __sai_host_intf_entry_hmap_find(hif_hmap,
                                                              name);
    if (hif_entry) {
        hmap_remove(hif_hmap, &hif_entry->hmap_node);
        free(hif_entry);
    }
}

/*
 * Remove L2 port netdev.
 *
 * @param[in] name    - netdev name.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__sai_remove_netdev(const char *name )
{
    int err = 0;
    char command[SAI_COMMAND_MAX_SIZE] = { };

    NULL_PARAM_LOG_ABORT(name);

    VLOG_INFO("Removing host interface (name: %s, type: L2 port)",
              name);

    snprintf(command, sizeof(command), "ip link del dev %s", name);

    VLOG_DBG("Executing command (command: %s)", command);
    /* Error code is returned in the least significant byte */
    err = 0xff & system(command);
    ERRNO_LOG_EXIT(err , "Failed to execute command (command: %s)",
                   command);

exit:
    return err;
}

/*
 * Creates Linux netdev for specified interface.
 *
 * @param[in] name   - netdev name.
 * @param[in] type   - netdev type.
 * @param[in] handle - if netdev is L2 specifies port label id else VLAN id.
 * @param[in] mac    - netdev mac address.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__host_intf_netdev_create(const char *name,
                          enum host_intf_type type,
                          const handle_t *handle,
                          const struct eth_addr *mac)
{
    struct hif_entry hif = { };
    struct hif_entry *p_hif = NULL;
    char    ifname[64];
    char    *vlanid = NULL;
    int     vlan_id = 0;

    p_hif = __sai_host_intf_entry_hmap_find(&sai_host_intf, name);
    if (NULL !=  p_hif) {
        return 0;
    }

    switch (type) {
    case HOST_INTF_TYPE_L2_PORT_NETDEV:
        break;
    case HOST_INTF_TYPE_L3_PORT_NETDEV:
        break;
    case HOST_INTF_TYPE_L3_VLAN_NETDEV:
        break;
    default:
        ovs_assert(false);
        break;
    }

    sai_attribute_t hostif_attrib[3] = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t hif_id_port = SAI_NULL_OBJECT_ID;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(name);

    memset(ifname, 0, sizeof(ifname));
    memcpy(ifname, name, sizeof(ifname));
    if (0 == strncmp(name, "vlan", 4)) {
        vlanid = strtok(ifname, "vlan");
        vlan_id = atoi(vlanid);
        ops_sai_vlan_intf_update(vlan_id, true);
    }

    hostif_attrib[0].id = SAI_HOSTIF_ATTR_TYPE;
    hostif_attrib[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;
    hostif_attrib[1].id = SAI_HOSTIF_ATTR_NAME;
    strcpy(hostif_attrib[1].value.chardata, name);
    hostif_attrib[2].id = SAI_HOSTIF_ATTR_RIF_OR_PORT_ID;
    if (0 != strncmp(name, "vlan", 4)) {
        hostif_attrib[2].value.oid = ops_sai_api_port_map_get_oid(handle->data);
    } else {
        hostif_attrib[2].value.oid = vlan_id;
    }

    status = sai_api->host_interface_api->create_hostif(&hif_id_port,
                                                        ARRAY_SIZE(hostif_attrib),
                                                        hostif_attrib);

    SAI_ERROR_LOG_EXIT(status, "Failed to create host interface");

    strncpy(hif.name, name, sizeof(hif.name));
    hif.type = type;
    hif.hostif_id.data = hif_id_port;
    memcpy(&hif.handle, handle, sizeof(hif.handle));
    memcpy(&hif.mac, mac, sizeof(hif.mac));

    __sai_host_intf_entry_hmap_add(&sai_host_intf, &hif);

exit:
    return status;
}

/*
 * Removes Linux netdev for specified interface.
 *
 * @param[in] name - netdev name
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__host_intf_netdev_remove(const char *name)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
     __sai_host_intf_entry_hmap_del(&sai_host_intf, name);

    return 0;
}

/*
 * Registers traps for packets.
 */
static void
__host_intf_traps_register(void)
{
    int i = 0;
    sai_attribute_t attr[2] = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct ops_sai_trap_group_entry *group_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_INFO("Registering traps");

    for (i = 0; i < ARRAY_SIZE(trap_group_config_table); ++i) {
        /* create entry */
        group_entry = xzalloc(sizeof *group_entry);

        /* create policer */
        status = ops_sai_policer_create(&group_entry->policer,
                                        &trap_group_config_table[i].policer_config)
                                        ? SAI_STATUS_FAILURE
                                        : SAI_STATUS_SUCCESS;
        SAI_ERROR_LOG_ABORT(status, "Failed to register traps");

        /* create group */
        attr[0].id = SAI_HOSTIF_TRAP_GROUP_ATTR_PRIO;
        attr[0].value.u32 = trap_group_config_table[i].priority;
        attr[1].id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
        attr[1].value.oid = (sai_object_id_t)group_entry->policer.data;

        status = sai_api->host_interface_api->create_hostif_trap_group(&group_entry->trap_group.data,
                                                                       ARRAY_SIZE(attr),
                                                                       attr);
        SAI_ERROR_LOG_ABORT(status, "Failed to create group %s",
                            trap_group_config_table[i].name);

        /* register traps */
        __traps_bind(trap_group_config_table[i].trap_ids,
                     &group_entry->trap_group,
                     trap_group_config_table[i].is_l3,
                     trap_group_config_table[i].is_log);

        strncpy(group_entry->name, trap_group_config_table[i].name,
                sizeof(group_entry->name));
        group_entry->name[strnlen(group_entry->name,
                                  SAI_TRAP_GROUP_MAX_NAME_LEN - 1)] = '\0';
        list_push_back(&sai_trap_group_list, &group_entry->list_node);
    }
}

/*
 * Unregisters traps for packets.
 */
static void
__hostint_traps_unregister(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct ops_sai_trap_group_entry *entry = NULL, *next_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    LIST_FOR_EACH_SAFE(entry, next_entry, list_node, &sai_trap_group_list) {
        list_remove(&entry->list_node);

        status = ops_sai_policer_remove(&entry->policer) ? SAI_STATUS_SUCCESS
                                                         : SAI_STATUS_FAILURE;
        SAI_ERROR_LOG_ABORT(status, "Failed to uninitialize traps %s",
                            entry->name);

        status = sai_api->host_interface_api->remove_hostif_trap_group(entry->trap_group.data);
        SAI_ERROR_LOG_ABORT(status, "Failed to remove trap group %s",
                            entry->name);

        free(entry);
    }
}

/*
 * Binds trap ids to trap groups.
 *
 * @param[in] trap_ids - list of trap ids, -1 terminated.
 * @param[in] group    - pointer to trap group handle.
 * @param[in] is_l3    - boolean indicating if trap channel is L3 netdev.
 * @param[in] is_log   - boolean indicating if packet should be forwarded.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static void
__traps_bind(const int *trap_ids, const handle_t *group, bool is_l3,
             bool is_log)
{
    int i = 0;
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(group);
    NULL_PARAM_LOG_ABORT(trap_ids);

    for (i = 0; trap_ids[i] != -1; i++) {
#if 0
        attr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attr.value.u32 = is_log ? SAI_PACKET_ACTION_LOG
                                : SAI_PACKET_ACTION_TRAP;
        status = sai_api->host_interface_api->set_trap_attribute(trap_ids[i],
                                                                 &attr);
        SAI_ERROR_LOG_ABORT(status, "Failed to set trap packet action, id %d",
                            trap_ids[i]);

        attr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL;
        attr.value.u32 = is_l3 ? SAI_HOSTIF_TRAP_CHANNEL_NETDEV
#ifdef MLNX_SAI
                               : SAI_HOSTIF_TRAP_CHANNEL_L2_NETDEV;
#else
                               : SAI_HOSTIF_TRAP_CHANNEL_NETDEV;
#endif
        status = sai_api->host_interface_api->set_trap_attribute(trap_ids[i],
                                                                 &attr);
        SAI_ERROR_LOG_ABORT(status, "Failed to set trap channel, id %d",
                            trap_ids[i]);
#endif
        attr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attr.value.oid = (sai_object_id_t)group->data;
        status = sai_api->host_interface_api->set_trap_attribute(trap_ids[i],
                                                                 &attr);
        SAI_ERROR_LOG_ABORT(status, "Failed to bind trap to group, id %d",
                            trap_ids[i]);
    }
}

DEFINE_GENERIC_CLASS(struct host_intf_class, host_intf) = {
        .init = __host_intf_init,
        .create = __host_intf_netdev_create,
        .remove = __host_intf_netdev_remove,
        .traps_register = __host_intf_traps_register,
        .traps_unregister = __hostint_traps_unregister,
        .deinit = __host_intf_deinit
};

DEFINE_GENERIC_CLASS_GETTER(struct host_intf_class, host_intf);
