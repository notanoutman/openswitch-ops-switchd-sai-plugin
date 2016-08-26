/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */
/* by xuwj */
#define __BOOL_DEFINED
/* end */
#include <config.h>
#include <errno.h>
#include <linux/ethtool.h>
#include <netinet/ether.h>

#include <vswitch-idl.h>
#include <netdev-provider.h>
#include <openflow/openflow.h>
#include <openswitch-idl.h>
#include <openswitch-dflt.h>
#include <sai-api-class.h>
#include <sai-log.h>
#include <sai-common.h>
#include <sai-port.h>
#include <sai-host-intf.h>
#include <sai-router-intf.h>

VLOG_DEFINE_THIS_MODULE(netdev_sai);

/* Protects 'sai_list'. */
static struct ovs_mutex sai_netdev_list_mutex = OVS_MUTEX_INITIALIZER;
static struct ovs_list sai_netdev_list OVS_GUARDED_BY(sai_netdev_list_mutex)
    = OVS_LIST_INITIALIZER(&sai_netdev_list);

struct netdev_sai {
    struct netdev up;
    struct ovs_list list_node OVS_GUARDED_BY(sai_netdev_list_mutex);
    struct ovs_mutex mutex OVS_ACQ_AFTER(sai_netdev_list_mutex);
    uint32_t hw_id;
    bool is_port_initialized;
    long long int carrier_resets;
    struct ops_sai_port_config default_config;
    struct ops_sai_port_config config;
    struct eth_addr mac_addr;
    bool netdev_internal_admin_state;
    const handle_t *rif_handle;
};

static inline bool __is_sai_class(const struct netdev_class *);
static inline struct netdev_sai *__netdev_sai_cast(const struct netdev *);
static struct netdev *__alloc(void);
static int __construct(struct netdev *);
static void __destruct(struct netdev *);
static void __dealloc(struct netdev *);
static int __set_hw_intf_info(struct netdev *, const struct smap *);
static int __set_hw_intf_info_internal(struct netdev *netdev_,
                                                const struct smap *args);
static bool __args_autoneg_get(const struct smap *, bool);
static bool __args_duplex_get(const struct smap *, bool);
static int __args_pause_get(const struct smap *, bool, bool);
static int __set_hw_intf_config(struct netdev *, const struct smap *);
static int __set_hw_intf_config_internal(struct netdev *,
                                                  const struct smap *);
static int __set_etheraddr_full(struct netdev *, const struct eth_addr mac);
static int __set_etheraddr(struct netdev *, const struct eth_addr mac);
static int __get_etheraddr(const struct netdev *, struct eth_addr *mac);
static int __get_mtu(const struct netdev *, int *);
static int __set_mtu(const struct netdev *, int);
static int __get_carrier(const struct netdev *, bool *);
static long long int __get_carrier_resets(const struct netdev *);
static int __get_stats(const struct netdev *, struct netdev_stats *);
static int __get_features(const struct netdev *, enum netdev_features *,
                          enum netdev_features *, enum netdev_features *,
                          enum netdev_features *);
static int __update_flags(struct netdev *, enum netdev_flags,
                          enum netdev_flags, enum netdev_flags *);
static int __update_flags_internal(struct netdev *,
                                   enum netdev_flags,
                                   enum netdev_flags,
                                   enum netdev_flags *);
static int __update_flags_loopback(struct netdev *,
                                   enum netdev_flags,
                                   enum netdev_flags,
                                   enum netdev_flags *);

#define NETDEV_SAI_CLASS(TYPE, CONSTRUCT, DESCRUCT, INTF_INFO, INTF_CONFIG, \
                         UPDATE_FLAGS, GET_MTU, SET_MTU) \
{ \
    PROVIDER_INIT_GENERIC(type,                 TYPE) \
    PROVIDER_INIT_GENERIC(init,                 NULL) \
    PROVIDER_INIT_GENERIC(run,                  NULL) \
    PROVIDER_INIT_GENERIC(wait,                 NULL) \
    PROVIDER_INIT_GENERIC(alloc,                __alloc) \
    PROVIDER_INIT_GENERIC(construct,            CONSTRUCT) \
    PROVIDER_INIT_GENERIC(destruct,             DESCRUCT) \
    PROVIDER_INIT_GENERIC(dealloc,              __dealloc) \
    PROVIDER_INIT_GENERIC(get_config,           NULL) \
    PROVIDER_INIT_GENERIC(set_config,           NULL) \
    PROVIDER_INIT_OPS_SPECIFIC(set_hw_intf_info, INTF_INFO) \
    PROVIDER_INIT_OPS_SPECIFIC(set_hw_intf_config, INTF_CONFIG) \
    PROVIDER_INIT_GENERIC(get_tunnel_config,    NULL) \
    PROVIDER_INIT_GENERIC(build_header,         NULL) \
    PROVIDER_INIT_GENERIC(push_header,          NULL) \
    PROVIDER_INIT_GENERIC(pop_header,           NULL) \
    PROVIDER_INIT_GENERIC(get_numa_id,          NULL) \
    PROVIDER_INIT_GENERIC(set_multiq,           NULL) \
    PROVIDER_INIT_GENERIC(send,                 NULL) \
    PROVIDER_INIT_GENERIC(send_wait,            NULL) \
    PROVIDER_INIT_GENERIC(set_etheraddr,        __set_etheraddr) \
    PROVIDER_INIT_GENERIC(get_etheraddr,        __get_etheraddr) \
    PROVIDER_INIT_GENERIC(get_mtu,              GET_MTU) \
    PROVIDER_INIT_GENERIC(set_mtu,              SET_MTU) \
    PROVIDER_INIT_GENERIC(get_ifindex,          NULL) \
    PROVIDER_INIT_GENERIC(get_carrier,          __get_carrier) \
    PROVIDER_INIT_GENERIC(get_carrier_resets,   __get_carrier_resets) \
    PROVIDER_INIT_GENERIC(set_miimon_interval,  NULL) \
    PROVIDER_INIT_GENERIC(get_stats,            __get_stats) \
    PROVIDER_INIT_GENERIC(get_features,         __get_features) \
    PROVIDER_INIT_GENERIC(set_advertisements,   NULL) \
    PROVIDER_INIT_GENERIC(set_policing,         NULL) \
    PROVIDER_INIT_GENERIC(get_qos_types,        NULL) \
    PROVIDER_INIT_GENERIC(get_qos_capabilities, NULL) \
    PROVIDER_INIT_GENERIC(get_qos,              NULL) \
    PROVIDER_INIT_GENERIC(set_qos,              NULL) \
    PROVIDER_INIT_GENERIC(get_queue,            NULL) \
    PROVIDER_INIT_GENERIC(set_queue,            NULL) \
    PROVIDER_INIT_GENERIC(delete_queue,         NULL) \
    PROVIDER_INIT_GENERIC(get_queue_stats,      NULL) \
    PROVIDER_INIT_GENERIC(queue_dump_start,     NULL) \
    PROVIDER_INIT_GENERIC(queue_dump_next,      NULL) \
    PROVIDER_INIT_GENERIC(queue_dump_done,      NULL) \
    PROVIDER_INIT_GENERIC(dump_queue_stats,     NULL) \
    PROVIDER_INIT_GENERIC(get_in4,              NULL) \
    PROVIDER_INIT_GENERIC(set_in4,              NULL) \
    PROVIDER_INIT_GENERIC(get_in6,              NULL) \
    PROVIDER_INIT_GENERIC(add_router,           NULL) \
    PROVIDER_INIT_GENERIC(get_next_hop,         NULL) \
    PROVIDER_INIT_GENERIC(get_status,           NULL) \
    PROVIDER_INIT_GENERIC(arp_lookup,           NULL) \
    PROVIDER_INIT_GENERIC(update_flags,         UPDATE_FLAGS) \
    PROVIDER_INIT_GENERIC(rxq_alloc,            NULL) \
    PROVIDER_INIT_GENERIC(rxq_construct,        NULL) \
    PROVIDER_INIT_GENERIC(rxq_destruct,         NULL) \
    PROVIDER_INIT_GENERIC(rxq_dealloc,          NULL) \
    PROVIDER_INIT_GENERIC(rxq_recv,             NULL) \
    PROVIDER_INIT_GENERIC(rxq_wait,             NULL) \
    PROVIDER_INIT_GENERIC(rxq_drain,            NULL) \
} \

static const struct netdev_class netdev_sai_class = NETDEV_SAI_CLASS(
        "system",
        __construct,
        __destruct,
        __set_hw_intf_info,
        __set_hw_intf_config,
        __update_flags,
        __get_mtu,
        __set_mtu);

static const struct netdev_class netdev_sai_internal_class = NETDEV_SAI_CLASS(
        "internal",
        __construct,
        __destruct,
        __set_hw_intf_info_internal,
        __set_hw_intf_config_internal,
        __update_flags_internal,
        NULL,
        NULL);

static const struct netdev_class netdev_sai_vlansubint_class = NETDEV_SAI_CLASS(
        "vlansubint",
        __construct,
        __destruct,
        NULL,
        NULL,
        __update_flags_internal,
        NULL,
        NULL);

static const struct netdev_class netdev_sai_loopback_class = NETDEV_SAI_CLASS(
        "loopback",
        __construct,
        __destruct,
        NULL,
        NULL,
        __update_flags_loopback,
        NULL,
        NULL);

/**
 * Register netdev classes - system and internal.
 */
void
netdev_sai_register(void)
{
    netdev_register_provider(&netdev_sai_class);
    netdev_register_provider(&netdev_sai_internal_class);
    netdev_register_provider(&netdev_sai_vlansubint_class);
    netdev_register_provider(&netdev_sai_loopback_class);
}

/**
 * Get port label ID from netdev.
 * @param[in] netdev_ - pointer to netdev.
 * @return uint32_t value of port label ID.
 */
uint32_t
netdev_sai_hw_id_get(struct netdev *netdev_)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    return netdev->hw_id;
}

int
netdev_sai_get_etheraddr(const struct netdev *netdev,
                         struct eth_addr *mac)
{
    struct netdev_sai *dev = __netdev_sai_cast(netdev);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&dev->mutex);
    if (!dev->is_port_initialized) {
        goto exit;
    }

    memcpy(mac, &dev->mac_addr, sizeof (*mac));

exit:
    ovs_mutex_unlock(&dev->mutex);
    return 0;
}
/**
 * Notifies openswitch when port state chenges.
 * @param[in] oid - port object id.
 * @param[in] link_status - port operational state.
 */
void
netdev_sai_port_oper_state_changed(sai_object_id_t oid, int link_status)
{
    struct netdev_sai *dev = NULL, *next_dev = NULL;

    LIST_FOR_EACH_SAFE(dev, next_dev, list_node, &sai_netdev_list) {
        if (dev->is_port_initialized
	     && (0 != strcmp(netdev_get_name(&dev->up),DEFAULT_BRIDGE_NAME))
            && ops_sai_api_hw_id2port_id(dev->hw_id) == oid) {
            break;
        }
    }

    if (NULL == dev) {
        return;
    }

    if (link_status) {
        dev->carrier_resets++;
    }

    netdev_change_seq_changed(&(dev->up));
    seq_change(connectivity_seq_get());
}

/**
 * Attach/detach router interface handle to netdev.
 *
 * @param[in] netdev_     - Pointer to netdev object.
 * @param[in] rif_handle  - Router interface handle.
 *
 * @note If NULL value is specified for rif_handle this means that
 *       router interface handle should be detached from netdev
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
int
netdev_sai_set_router_intf_handle(struct netdev *netdev_,
                                  const handle_t *rif_handle)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    VLOG_INFO("Set rif handle for netdev (netdev: %s, rif_handle: %p)",
              netdev_get_name(netdev_), rif_handle);

    ovs_mutex_lock(&netdev->mutex);

    ovs_assert(netdev->is_port_initialized);

    netdev->rif_handle = rif_handle;

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

/*
 * Check if netdev is of type sai_netdev.
 */
static inline bool
__is_sai_class(const struct netdev_class *class)
{
    return class->construct == __construct;
}

/*
 * Cast openswitch netdev to sai_netdev.
 */
static inline struct netdev_sai *
__netdev_sai_cast(const struct netdev *netdev)
{
    NULL_PARAM_LOG_ABORT(netdev);

    ovs_assert(__is_sai_class(netdev_get_class(netdev)));
    return CONTAINER_OF(netdev, struct netdev_sai, up);
}

static struct netdev *
__alloc(void)
{
    struct netdev_sai *netdev = xzalloc(sizeof *netdev);

    SAI_API_TRACE_FN();

    return &(netdev->up);
}

static int
__construct(struct netdev *netdev_)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_init(&netdev->mutex);
    ovs_mutex_lock(&sai_netdev_list_mutex);
    list_push_back(&sai_netdev_list, &netdev->list_node);
    ovs_mutex_unlock(&sai_netdev_list_mutex);

    return 0;
}

static void
__destruct(struct netdev *netdev_)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&sai_netdev_list_mutex);

    if (netdev->is_port_initialized) {
        ops_sai_host_intf_netdev_remove(netdev_get_name(netdev_));
    }

    list_remove(&netdev->list_node);
    ovs_mutex_unlock(&sai_netdev_list_mutex);
    ovs_mutex_destroy(&netdev->mutex);
}

static void
__dealloc(struct netdev *netdev_)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    free(netdev);
}

static int
__set_hw_intf_info(struct netdev *netdev_, const struct smap *args)
{
    int status = 0;
    struct eth_addr mac = { };
    handle_t hw_id_handle;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);
    const int hw_id =
        smap_get_int(args, INTERFACE_HW_INTF_INFO_MAP_SWITCH_INTF_ID, -1);

    SAI_API_TRACE_FN();

    NULL_PARAM_LOG_ABORT(args);

    ovs_mutex_lock(&netdev->mutex);

    if (netdev->is_port_initialized
        || !strcmp(netdev_->name, DEFAULT_BRIDGE_NAME)) {
        goto exit;
    }

    netdev->hw_id = hw_id;

    status = ops_sai_api_base_mac_get(&mac);
    ERRNO_EXIT(status);

    memcpy(&netdev->mac_addr, &mac, sizeof(netdev->mac_addr));

    hw_id_handle.data = hw_id;
    status = ops_sai_host_intf_netdev_create(netdev_get_name(netdev_),
                                             HOST_INTF_TYPE_L2_PORT_NETDEV,
                                             &hw_id_handle, &mac);
    ERRNO_LOG_EXIT(status,
                   "Failed to create port interface (name: %s)",
                   netdev_get_name(netdev_));

    status = ops_sai_port_config_get(hw_id, &netdev->default_config);
    ERRNO_LOG_EXIT(status, "Failed to read default config on port: %d", hw_id);

    status = __set_etheraddr_full(netdev_, mac);
    ERRNO_EXIT(status);

    netdev->is_port_initialized = true;

exit:
    ovs_mutex_unlock(&netdev->mutex);
    return status;
}

static int
__set_hw_intf_info_internal(struct netdev *netdev_,
                                     const struct smap *args)
{
    int status = 0;
    int vlanid = 0;
    handle_t handle;
    struct eth_addr mac = { };
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);
    bool is_bridge_intf = smap_get_bool(args,
                                        INTERFACE_HW_INTF_INFO_MAP_BRIDGE,
                                        DFLT_INTERFACE_HW_INTF_INFO_MAP_BRIDGE);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);

    if (netdev->is_port_initialized) {
        goto exit;
    }

    if (!is_bridge_intf) {
        vlanid = strtol(netdev_get_name(netdev_) + strlen(VLAN_INTF_PREFIX),
                        NULL, 0);
        ovs_assert(vlanid >= VLAN_ID_MIN && vlanid <= VLAN_ID_MAX);

        handle.data = vlanid;

        status = ops_sai_api_base_mac_get(&mac);
        ERRNO_EXIT(status);

        memcpy(&netdev->mac_addr, &mac, sizeof(netdev->mac_addr));

        status = ops_sai_host_intf_netdev_create(netdev_get_name(netdev_),
                                                 HOST_INTF_TYPE_L3_VLAN_NETDEV,
                                                 &handle, &mac);
        ERRNO_LOG_EXIT(status,
                       "Failed to create port interface (name: %s)",
                       netdev_get_name(netdev_));
    }

    netdev->is_port_initialized = true;

exit:
    ovs_mutex_unlock(&netdev->mutex);
    return status;
}


/*
 * Read autoneg value from smap arguments.
 */
static bool
__args_autoneg_get(const struct smap *args, bool def)
{
    const char *autoneg = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_AUTONEG);

    if (autoneg == NULL) {
        return def;
    }

    return !strcmp(autoneg, INTERFACE_USER_CONFIG_MAP_AUTONEG_ON);
}

/*
 * Read duplex value from smap arguments.
 */
static bool
__args_duplex_get(const struct smap *args, bool def)
{
    const char *duplex = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_DUPLEX);

    if (duplex == NULL) {
        return def;
    }

    return !strcmp(duplex, INTERFACE_HW_INTF_CONFIG_MAP_DUPLEX_FULL);
}

/*
 * Read pause value from smap arguments.
 */
static int
__args_pause_get(const struct smap *args, bool is_tx, bool def)
{
    const char *pause = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE);
    const char *requested_pause = is_tx ? INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_TX
        : INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_RX;

    if (pause == NULL) {
        return def;
    }

    return !strcmp(pause, requested_pause) ||
           !strcmp(pause, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_RXTX);
}

static int
__set_hw_intf_config(struct netdev *netdev_, const struct smap *args)
{
    int status = 0;
    struct ops_sai_port_config config = { };
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);
    const struct ops_sai_port_config *def = &netdev->default_config;
    /* Max speed must be always present (in yaml config file). */
    config.hw_enable = smap_get_bool(args, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE,
                                     def->hw_enable);
    config.autoneg = __args_autoneg_get(args, def->autoneg);
    config.mtu = smap_get_int(args, INTERFACE_HW_INTF_CONFIG_MAP_MTU,
                              def->mtu);
    config.speed = smap_get_int(args, INTERFACE_HW_INTF_CONFIG_MAP_SPEEDS,
                                def->speed);
    config.full_duplex = __args_duplex_get(args, def->full_duplex);
    config.pause_tx = __args_pause_get(args, true, def->pause_tx);
    config.pause_rx = __args_pause_get(args, false, def->pause_rx);

    ovs_mutex_lock(&netdev->mutex);

    status = ops_sai_port_config_set(netdev->hw_id, &config, &netdev->config);
    ERRNO_LOG_EXIT(status, "Failed to set hw interface config");

exit:
    ovs_mutex_unlock(&netdev->mutex);
    return status;
}

static int
__set_hw_intf_config_internal(struct netdev *netdev_, const struct smap *args)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);
    const char *enable = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE);

    SAI_API_TRACE_FN();

    if (enable) {
        netdev->netdev_internal_admin_state =
                STR_EQ(enable, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE_TRUE);
    }

    return 0;
}


/*
 * Cache MAC address.
 */
static int
__set_etheraddr_full(struct netdev *netdev,
                           const struct eth_addr mac)
{
    struct netdev_sai *dev = __netdev_sai_cast(netdev);

    if (!dev->is_port_initialized) {
        goto exit;
    }

    /* Not supported by SAI. */

    memcpy(&dev->mac_addr, &mac, sizeof (dev->mac_addr));

exit:
    return 0;
}

static int
__set_etheraddr(struct netdev *netdev,
                         const struct eth_addr mac)
{
    int status = 0;
    struct netdev_sai *dev = __netdev_sai_cast(netdev);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&dev->mutex);
    status = __set_etheraddr_full(netdev, mac);
    ovs_mutex_unlock(&dev->mutex);

    return status;
}

static int
__get_etheraddr(const struct netdev *netdev,
                         struct eth_addr *mac)
{
    struct netdev_sai *dev = __netdev_sai_cast(netdev);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&dev->mutex);
    if (!dev->is_port_initialized) {
        goto exit;
    }

    memcpy(mac, &dev->mac_addr, sizeof (*mac));

exit:
    ovs_mutex_unlock(&dev->mutex);
    return 0;
}

static int
__get_mtu(const struct netdev *netdev_, int *mtup)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (netdev->is_port_initialized) {
        status = ops_sai_port_mtu_get(netdev->hw_id, mtup);
    }
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static int
__set_mtu(const struct netdev *netdev_, int mtu)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (netdev->is_port_initialized) {
        status = ops_sai_port_mtu_set(netdev->hw_id, mtu);
    }
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static int
__get_carrier(const struct netdev *netdev_, bool * carrier)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (netdev->is_port_initialized) {
        if (STR_EQ(netdev_get_type(netdev_), OVSREC_INTERFACE_TYPE_SYSTEM)) {
            status = ops_sai_port_carrier_get(netdev->hw_id, carrier);
        } else {
            /* TODO: Waiting for implementation via netlink */
            *carrier = true;
        }
    }
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static long long int
__get_carrier_resets(const struct netdev *netdev_)
{
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    return netdev->carrier_resets;
}

static int
__get_stats(const struct netdev *netdev_, struct netdev_stats *stats)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (!netdev->is_port_initialized) {
        goto exit;
    }

    if (STR_EQ(netdev_get_type(netdev_), OVSREC_INTERFACE_TYPE_SYSTEM)) {
        status = ops_sai_port_stats_get(netdev->hw_id, stats);
        ERRNO_EXIT(status);
    }

    if (netdev->rif_handle) {
        status = ops_sai_router_intf_get_stats(netdev->rif_handle, stats);
        ERRNO_EXIT(status);
    }

exit:
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static int
__get_features(const struct netdev *netdev_, enum netdev_features *current,
               enum netdev_features *advertised,
               enum netdev_features *supported, enum netdev_features *peer)
{
#if 0
    NETDEV_F_10MB_HD =    1 << 0,  /* 10 Mb half-duplex rate support. */
    NETDEV_F_10MB_FD =    1 << 1,  /* 10 Mb full-duplex rate support. */
    NETDEV_F_100MB_HD =   1 << 2,  /* 100 Mb half-duplex rate support. */
    NETDEV_F_100MB_FD =   1 << 3,  /* 100 Mb full-duplex rate support. */
    NETDEV_F_1GB_HD =     1 << 4,  /* 1 Gb half-duplex rate support. */
    NETDEV_F_1GB_FD =     1 << 5,  /* 1 Gb full-duplex rate support. */
    NETDEV_F_10GB_FD =    1 << 6,  /* 10 Gb full-duplex rate support. */
    NETDEV_F_40GB_FD =    1 << 7,  /* 40 Gb full-duplex rate support. */
    NETDEV_F_100GB_FD =   1 << 8,  /* 100 Gb full-duplex rate support. */
    NETDEV_F_1TB_FD =     1 << 9,  /* 1 Tb full-duplex rate support. */
    NETDEV_F_OTHER =      1 << 10, /* Other rate, not in the list. */
    NETDEV_F_COPPER =     1 << 11, /* Copper medium. */
    NETDEV_F_FIBER =      1 << 12, /* Fiber medium. */
    NETDEV_F_AUTONEG =    1 << 13, /* Auto-negotiation. */
    NETDEV_F_PAUSE =      1 << 14, /* Pause. */
    NETDEV_F_PAUSE_ASYM = 1 << 15, /* Asymmetric pause. */
#endif
    enum {
        _SAI_NET_DEV_SPEED_10M  = 10,
        _SAI_NET_DEV_SPEED_100M = 100,
        _SAI_NET_DEV_SPEED_1G   = 1000,
        _SAI_NET_DEV_SPEED_10G  = 10000,
        _SAI_NET_DEV_SPEED_40G  = 40000,
        _SAI_NET_DEV_SPEED_100G = 100000,
        _SAI_NET_DEV_SPEED_1TB  = 1000000,
    };

    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (!netdev->is_port_initialized) {
        goto exit;
    }

    memset(current, 0, sizeof *current);

    if(netdev->config.autoneg) {
        *current |= NETDEV_F_AUTONEG;
    }

    if(netdev->config.pause_tx && netdev->config.pause_rx) {
        *current |= NETDEV_F_PAUSE;
    } else if(netdev->config.pause_tx) {
        *current |= NETDEV_F_PAUSE_ASYM;
    }

    switch(netdev->config.speed) {
    case _SAI_NET_DEV_SPEED_10M:
        if(netdev->config.full_duplex) {
            *current |= NETDEV_F_10MB_FD;
        }else{
            *current |= NETDEV_F_10MB_HD;
        }
        break;
    case _SAI_NET_DEV_SPEED_100M:
        if(netdev->config.full_duplex) {
            *current |= NETDEV_F_100MB_FD;
        }else{
            *current |= NETDEV_F_100MB_HD;
        }
        break;
    case _SAI_NET_DEV_SPEED_1G:
        if(netdev->config.full_duplex) {
            *current |= NETDEV_F_1GB_FD;
        }else{
            *current |= NETDEV_F_1GB_HD;
        }
        break;
    case _SAI_NET_DEV_SPEED_10G:
        *current |= NETDEV_F_10GB_FD;
        break;
    case _SAI_NET_DEV_SPEED_40G:
        *current |= NETDEV_F_40GB_FD;
        break;
    case _SAI_NET_DEV_SPEED_100G:
        *current |= NETDEV_F_100GB_FD;
        break;
    case _SAI_NET_DEV_SPEED_1TB:
        *current |= NETDEV_F_1TB_FD;
        break;
    default:
        *current |= NETDEV_F_OTHER;
        break;
    }

exit:
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static int
__update_flags(struct netdev *netdev_, enum netdev_flags off,
               enum netdev_flags on, enum netdev_flags *old_flagsp)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);
    if (netdev->is_port_initialized) {
        status = ops_sai_port_flags_update(netdev->hw_id, off, on, old_flagsp);
    } else {
        *old_flagsp = 0;
    }
    ovs_mutex_unlock(&netdev->mutex);

    return status;
}


static int
__update_flags_internal(struct netdev *netdev_,
                        enum netdev_flags off,
                        enum netdev_flags on, enum netdev_flags *old_flagsp)
{
    int status = 0;
    struct netdev_sai *netdev = __netdev_sai_cast(netdev_);

    SAI_API_TRACE_FN();

    ovs_mutex_lock(&netdev->mutex);

    if (netdev->is_port_initialized) {
        if (netdev->netdev_internal_admin_state) {
            *old_flagsp = NETDEV_UP;
        }

        if (on & NETDEV_UP) {
            netdev->netdev_internal_admin_state = true;
        } else if (off & NETDEV_UP) {
            netdev->netdev_internal_admin_state = false;
        }
    } else {
        *old_flagsp = 0;
    }

    ovs_mutex_unlock(&netdev->mutex);

    return status;
}

static int
__update_flags_loopback(struct netdev *netdev_,
                        enum netdev_flags off,
                        enum netdev_flags on, enum netdev_flags *old_flagsp)
{
    SAI_API_TRACE_FN();

    if ((off | on) & ~NETDEV_UP) {
        return EOPNOTSUPP;
    }

    *old_flagsp = NETDEV_UP | NETDEV_LOOPBACK;

    return 0;
}

static struct netdev_sai *
__netdev_sai_get_netdev_by_name(const char *name)
{
    struct netdev_sai *netdev = NULL;
    bool found = false;

    ovs_mutex_lock(&sai_netdev_list_mutex);
    LIST_FOR_EACH(netdev, list_node, &sai_netdev_list) {
        if (strcmp(netdev->up.name, name) == 0) {
            found = true;
            break;
        }
    }
    ovs_mutex_unlock(&sai_netdev_list_mutex);

    return (found == true) ? netdev : NULL;
}

int
netdev_sai_get_port_name_by_handle_id(handle_t    port_id,
                                                char        *str)
{
    struct netdev_sai *dev = NULL, *next_dev = NULL;
    int32_t     found = 0;

    if (!str) {
        return -1;
    }

    LIST_FOR_EACH_SAFE(dev, next_dev, list_node, &sai_netdev_list) {
        if (dev->is_port_initialized
	     && (0 != strcmp(netdev_get_name(&dev->up),DEFAULT_BRIDGE_NAME))
            && ops_sai_api_hw_id2port_id(dev->hw_id) == port_id.data) {
            found = 1;
            break;
        }
    }

    if (dev && found) {
        strcpy(str, dev->up.name);
        return 0;
    }

    return -1;
}

int
netdev_sai_get_hw_id_by_name(const char *name, uint32_t *hw_id)
{
    struct netdev_sai *netdev = NULL;

    if (!name || !hw_id) {
        return false;
    }

    netdev = __netdev_sai_get_netdev_by_name(name);

    if (netdev) {
        *hw_id = netdev->hw_id;
        return true;
    }
    else {
        return false;
    }
}
