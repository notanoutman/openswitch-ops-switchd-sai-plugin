/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */
#ifndef __BOOL_DEFINED
#define __BOOL_DEFINED
#endif
#include <netdev-provider.h>

#include <sai-common.h>
#include <sai-log.h>
#include <sai-api-class.h>
#include <sai-port.h>
#include <sai-fdb.h>
#include <list.h>

VLOG_DEFINE_THIS_MODULE(sai_port);

struct ops_sai_port_transaction_callback {
    struct ovs_list list_node;
    port_transaction_clb_t callback;
    enum ops_sai_port_transaction type;
};

struct port_info {
    struct ops_sai_port_config config;
    uint32_t hw_lanes[SAI_MAX_LANES];
    uint32_t hw_lanes_count;
};

//static struct port_info port_info[SAI_PORTS_MAX * SAI_MAX_LANES];

static struct ovs_list callback_list = OVS_LIST_INITIALIZER(&callback_list);

static sai_status_t __set_hw_intf_config_full(uint32_t,
                                              const struct
                                              ops_sai_port_config *,
                                              struct ops_sai_port_config *);
static sai_port_flow_control_mode_t sai_port_pause(bool, bool);
static sai_status_t __port_get_lane_list(uint32_t, uint32_t *, uint32_t *);
static sai_status_t __create_port(uint32_t, uint32_t *, uint32_t, uint32_t);
static sai_status_t __remove_port(uint32_t);
static sai_status_t __port_unsplit(uint32_t, uint32_t, uint32_t, uint32_t *);
static sai_status_t __port_split_to_2(uint32_t, uint32_t, uint32_t, uint32_t *);
static sai_status_t __port_split_to_4(uint32_t, uint32_t, uint32_t, uint32_t *);

void
ops_sai_port_init(void)
{
    ovs_assert(ops_sai_port_class()->init);
    ops_sai_port_class()->init();
}

static int __hw_lane_cmp(const void *a, const void *b)
{
    return ((*(uint32_t *)a) - (*(uint32_t*)b));
}

/*
 * Initialize port functionality.
 */
void
__port_init(void)
{
    VLOG_INFO("Initializing port");
}

/*
 * De-initialize port functionality.
 */
void
ops_sai_port_deinit(void)
{
    ovs_assert(ops_sai_port_class()->deinit);
    ops_sai_port_class()->deinit();
}

/*
 * De-initialize port functionality.
 */
void
__port_deinit(void)
{
    VLOG_INFO("De-initializing port");
}


/*
 * Register port transaction callback.
 *
 * @param[in] clb port transaction callback.
 * @param[in] type transaction type.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int ops_sai_port_transaction_register_callback(port_transaction_clb_t clb,
                                               enum ops_sai_port_transaction type)
{
    struct ops_sai_port_transaction_callback *node = NULL;

    /* Coverity[leaked_storage] */
    node = xzalloc(sizeof(*node));
    node->callback = clb;
    node->type = type;

    list_push_back(&callback_list, &node->list_node);

    return 0;
}

/*
 * Un-register port transaction callback.
 *
 * @param[in] clb port transaction callback.
 * @return 0, sai status converted to errno otherwise.
 */
int ops_sai_port_transaction_unregister_callback(port_transaction_clb_t clb)
{
    struct ops_sai_port_transaction_callback *iter = NULL;
    struct ops_sai_port_transaction_callback *next = NULL;

    LIST_FOR_EACH_SAFE(iter, next, list_node, &callback_list) {
        if (iter->callback == clb) {
            list_remove(&iter->list_node);
            free(iter);
        }
    }

    return 0;
}

/*
 * Call callbacks registered with specified transaction type.
 *
 * @param[in] hw_id port label id.
 * @param[in] transaction transaction type.
 *
 * @return 0, sai status converted to errno otherwise.
 */
void ops_sai_port_transaction(uint32_t hw_id,
                              enum ops_sai_port_transaction transaction)
{
    struct ops_sai_port_transaction_callback *iter = NULL;

    LIST_FOR_EACH(iter, list_node, &callback_list) {
        if (iter->type == transaction) {
            iter->callback(hw_id);
        }
    }
}

int
ops_sai_port_config_get(uint32_t hw_id, struct ops_sai_port_config *conf)
{
    ovs_assert(ops_sai_port_class()->config_get);
    return ops_sai_port_class()->config_get(hw_id, conf);
}

/*
 * Reads port configuration.
 *
 * @param[in] hw_id port label id.
 * @param[out] conf pointer to port configuration.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_config_get(uint32_t hw_id, struct ops_sai_port_config *conf)
{
    enum port_attr_list {
        PORT_ATTR_HW_ENABLE = 0,
        PORT_ATTR_AUTONEG,
#ifndef MLNX_SAI
        PORT_ATTR_DUPLEX,
#endif
        PORT_ATTR_FLOW_CONTROL,
        PORT_ATTR_MTU,
        PORT_ATTR_SPEED,
        PORT_ATTR_COUNT
    };

    sai_attribute_t attr[PORT_ATTR_COUNT] = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_port_flow_control_mode_t pause = SAI_PORT_FLOW_CONTROL_DISABLE;

    NULL_PARAM_LOG_ABORT(conf);

    attr[PORT_ATTR_HW_ENABLE].id = SAI_PORT_ATTR_ADMIN_STATE;
    attr[PORT_ATTR_AUTONEG].id = SAI_PORT_ATTR_AUTO_NEG_MODE;
#ifndef MLNX_SAI
    attr[PORT_ATTR_DUPLEX].id = SAI_PORT_ATTR_FULL_DUPLEX_MODE;
#endif
    attr[PORT_ATTR_FLOW_CONTROL].id = SAI_PORT_ATTR_GLOBAL_FLOW_CONTROL;
    attr[PORT_ATTR_MTU].id = SAI_PORT_ATTR_MTU;
    attr[PORT_ATTR_SPEED].id = SAI_PORT_ATTR_SPEED;

    status = sai_api->port_api->get_port_attribute(port_oid, PORT_ATTR_COUNT,
                                                   attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get config on port %d", hw_id);

    conf->hw_enable = attr[PORT_ATTR_HW_ENABLE].value.booldata;
    conf->autoneg = attr[PORT_ATTR_AUTONEG].value.booldata;
#ifndef MLNX_SAI
    conf->full_duplex = attr[PORT_ATTR_DUPLEX].value.booldata;
#endif
    pause = attr[PORT_ATTR_FLOW_CONTROL].value.u32;
    conf->pause_tx = (pause == SAI_PORT_FLOW_CONTROL_TX_ONLY) ||
                     (pause == SAI_PORT_FLOW_CONTROL_BOTH_ENABLE);
    conf->pause_rx = (pause == SAI_PORT_FLOW_CONTROL_RX_ONLY) ||
                     (pause == SAI_PORT_FLOW_CONTROL_BOTH_ENABLE);
    conf->mtu = attr[PORT_ATTR_MTU].value.u32;
    conf->speed = attr[PORT_ATTR_SPEED].value.u32;

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_config_set(uint32_t hw_id, const struct ops_sai_port_config *new,
                        struct ops_sai_port_config *old)
{
    ovs_assert(ops_sai_port_class()->config_set);
    return ops_sai_port_class()->config_set(hw_id, new, old);
}

/*
 * Applies new port configuration.
 *
 * @param[in] hw_id port label id.
 * @param[in] new pointer to new port configuration.
 * @param[out] old pointer to current port configuration, will be updated with
 * values from new configuration.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_config_set(uint32_t hw_id, const struct ops_sai_port_config *new,
                  struct ops_sai_port_config *old)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_object_id_t port_id = ops_sai_api_port_map_get_oid(hw_id);

    NULL_PARAM_LOG_ABORT(old);
    NULL_PARAM_LOG_ABORT(new);

    status = __set_hw_intf_config_full(hw_id, new, old);
    SAI_ERROR_EXIT(status);

    if (old->hw_enable != new->hw_enable) {
        attr.id = SAI_PORT_ATTR_ADMIN_STATE;
        attr.value.booldata = new->hw_enable;
        status = sai_api->port_api->set_port_attribute(port_id, &attr);
        SAI_ERROR_LOG_EXIT(status, "Failed to set admin state %s for port %d",
                           new->hw_enable ? "UP" : "DOWN", hw_id);
    }

    memcpy(old, new, sizeof(struct ops_sai_port_config));

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_mtu_get(uint32_t hw_id, int *mtu)
{
    ovs_assert(ops_sai_port_class()->mtu_get);
    return ops_sai_port_class()->mtu_get(hw_id, mtu);
}

/*
 * Reads port mtu.
 *
 * @param[in] hw_id port label id.
 * @param[out] mtu pointer to mtu variable, will be set to current value.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_mtu_get(uint32_t hw_id, int *mtu)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(mtu);

    attr.id = SAI_PORT_ATTR_MTU;
    status = sai_api->port_api->get_port_attribute(ops_sai_api_port_map_get_oid(hw_id),
                                                   1, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get mtu for port %d", hw_id);

    *mtu = attr.value.u32 - 38;

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_mtu_set(uint32_t hw_id, int mtu)
{
    ovs_assert(ops_sai_port_class()->mtu_set);
    return ops_sai_port_class()->mtu_set(hw_id, mtu);
}

/*
 * Sets port mtu.
 *
 * @param[in] hw_id port label id.
 * @param[in] mtu value to be applied.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_mtu_set(uint32_t hw_id, int mtu)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_MTU;
    attr.value.u32 = mtu + 38;
    status = sai_api->port_api->set_port_attribute(ops_sai_api_port_map_get_oid(hw_id),
                                                   &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set %d mtu for port %d", mtu, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_carrier_get(uint32_t hw_id, bool *carrier)
{
    ovs_assert(ops_sai_port_class()->carrier_get);
    return ops_sai_port_class()->carrier_get(hw_id, carrier);
}

/*
 * Reads port operational state.
 *
 * @param[in] hw_id port label id.
 * @param[out] carrier pointer to boolean, set to true if port is in operational
 * state.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_carrier_get(uint32_t hw_id, bool *carrier)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(carrier);

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    status = sai_api->port_api->get_port_attribute(ops_sai_api_port_map_get_oid(hw_id),
                                                   1, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get carrier for port %d", hw_id);

    *carrier = (sai_port_oper_status_t) attr.value.u32 ==
        SAI_PORT_OPER_STATUS_UP;

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_flags_update(uint32_t hw_id, enum netdev_flags off,
                          enum netdev_flags on, enum netdev_flags *old_flagsp)
{
    ovs_assert(ops_sai_port_class()->flags_update);
    return ops_sai_port_class()->flags_update(hw_id, off, on, old_flagsp);
}

/*
 * Updates netdevice flags. Currently only NETDEV_UP.
 *
 * @param[in] hw_id port label id.
 * @param[in] off flags to be cleared.
 * @param[in] on flags to be set.
 * @param[out] old_flagsp set with current flags.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_flags_update(uint32_t hw_id, enum netdev_flags off,
                          enum netdev_flags on, enum netdev_flags *old_flagsp)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(old_flagsp);

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    status = sai_api->port_api->get_port_attribute(ops_sai_api_port_map_get_oid(hw_id),
                                                   1, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get admin state from port %d",
                       hw_id);

    if (attr.value.booldata) {
        *old_flagsp |= NETDEV_UP;
    }

    if (on & NETDEV_UP) {
        attr.value.booldata = true;
    } else if (off & NETDEV_UP) {
        attr.value.booldata = false;
    } else {
        goto exit;
    }

    status = sai_api->port_api->set_port_attribute(ops_sai_api_port_map_get_oid(hw_id),
                                                   &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set admin state on port %d", hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_pvid_get(uint32_t hw_id, sai_vlan_id_t *pvid)
{
    ovs_assert(ops_sai_port_class()->pvid_get);
    return ops_sai_port_class()->pvid_get(hw_id, pvid);
}

/*
 * Reads port VLAN ID.
 *
 * @param[in] hw_id port label id.
 * @param[out] pvid pointer to pvid variable, will be set to current pvid.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_pvid_get(uint32_t hw_id, sai_vlan_id_t *pvid)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(pvid);

    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
    status = sai_api->port_api->get_port_attribute(port_oid, 1, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get pvid for port %u", hw_id);

    *pvid = attr.value.u32;

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_pvid_set(uint32_t hw_id, sai_vlan_id_t pvid)
{
    ovs_assert(ops_sai_port_class()->pvid_set);
    return ops_sai_port_class()->pvid_set(hw_id, pvid);
}

/*
 * Sets port VLAN ID.
 *
 * @param[in] hw_id port label id.
 * @param[in] pvid new VLAN ID to be set.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_pvid_set(uint32_t hw_id, sai_vlan_id_t pvid)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_vlan_id_t old_vid = 0;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    status = __port_pvid_get(hw_id,&old_vid);
    SAI_ERROR_LOG_EXIT(status, "Failed to get pvid %d for port %u",
                       pvid, hw_id);

	if(pvid == old_vid) {
        return 0;
    }

    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
    attr.value.u32 = pvid;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set pvid %d for port %u",
                       pvid, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_port_stats_get(uint32_t hw_id, struct netdev_stats *stats)
{
    ovs_assert(ops_sai_port_class()->stats_get);
    return ops_sai_port_class()->stats_get(hw_id, stats);
}

int
ops_sai_port_split_info_get(uint32_t hw_id, enum ops_sai_port_split mode,
                           struct split_info *info)
{
    ovs_assert(ops_sai_port_class()->split_info_get);
    return ops_sai_port_class()->split_info_get(hw_id, mode, info);
}

int ops_sai_port_split(uint32_t hw_id, enum ops_sai_port_split mode, uint32_t speed,
                       uint32_t sub_intf_hw_id_cnt,
                       const uint32_t *sub_intf_hw_id)
{
    ovs_assert(ops_sai_port_class()->split);
    return ops_sai_port_class()->split(hw_id, mode, speed,
                                       sub_intf_hw_id_cnt, sub_intf_hw_id);
}

/*
 * Get port statistics.
 *
 * @param[in] hw_id port label id.
 * @param[out] stats pointer to netdev statistics.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
__port_stats_get(uint32_t hw_id, struct netdev_stats *stats)
{
    enum stats_indexes {
        STAT_IDX_IF_IN_UCAST_PKTS = 0,
        STAT_IDX_IF_IN_NON_UCAST_PKTS,
        STAT_IDX_IF_OUT_UCAST_PKTS,
        STAT_IDX_IF_OUT_NON_UCAST_PKTS,
        STAT_IDX_IF_IN_OCTETS,
        STAT_IDX_IF_OUT_OCTETS,
        STAT_IDX_IF_IN_ERRORS,
        STAT_IDX_IF_OUT_ERRORS,
        STAT_IDX_IF_IN_DISCARDS,
        STAT_IDX_IF_OUT_DISCARDS,
        STAT_IDX_ETHER_STATS_MULTICAST_PKTS,
        STAT_IDX_ETHER_STATS_COLLISIONS,
#ifndef MLNX_SAI
        STAT_IDX_ETHER_RX_OVERSIZE_PKTS,
#endif
        STAT_IDX_ETHER_STATS_CRC_ALIGN_ERRORS,

        /* Since SAI's defination about port stats are not so clear,
         * we have defined another set enumerations in sai/inc/saiport.h */
#if 1
        STAT_IDX_IF_IN_PKTS,
        STAT_IDX_IF_OUT_PKTS,
        STAT_IDX_IF_IN_CRC_ERR_PKTS,
#endif
        STAT_IDX_COUNT
    };
    static const sai_port_stat_counter_t counter_ids[] = {
        SAI_PORT_STAT_IF_IN_UCAST_PKTS,
        SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS,
        SAI_PORT_STAT_IF_OUT_UCAST_PKTS,
        SAI_PORT_STAT_IF_OUT_NON_UCAST_PKTS,
        SAI_PORT_STAT_IF_IN_OCTETS,
        SAI_PORT_STAT_IF_OUT_OCTETS,
        SAI_PORT_STAT_IF_IN_ERRORS,
        SAI_PORT_STAT_IF_OUT_ERRORS,
        SAI_PORT_STAT_IF_IN_DISCARDS,
        SAI_PORT_STAT_IF_OUT_DISCARDS,
        SAI_PORT_STAT_ETHER_STATS_MULTICAST_PKTS,
        SAI_PORT_STAT_ETHER_STATS_COLLISIONS,
#ifndef MLNX_SAI
        SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS,
#endif
        SAI_PORT_STAT_ETHER_STATS_CRC_ALIGN_ERRORS,

#if 1
        SAI_PORT_STAT_IF_IN_PKTS,
        SAI_PORT_STAT_IF_OUT_PKTS,
        SAI_PORT_STAT_IF_IN_CRC_ERR_PKTS,
#endif
    };
    uint64_t counters[STAT_IDX_COUNT] = {};
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(stats);

    status = sai_api->port_api->get_port_stats(port_oid, counter_ids,
                                               STAT_IDX_COUNT, counters);
    SAI_ERROR_LOG_EXIT(status, "Failed to get stats for port %d", hw_id);

    memset(stats, 0, sizeof *stats);

#if 1
    stats->rx_packets = counters[STAT_IDX_IF_IN_PKTS];
    stats->tx_packets = counters[STAT_IDX_IF_OUT_PKTS];
#else
    stats->rx_packets = counters[STAT_IDX_IF_IN_UCAST_PKTS]
                      + counters[STAT_IDX_IF_IN_NON_UCAST_PKTS];
    stats->tx_packets = counters[STAT_IDX_IF_OUT_UCAST_PKTS]
                      + counters[STAT_IDX_IF_OUT_NON_UCAST_PKTS];
#endif
    stats->rx_bytes = counters[STAT_IDX_IF_IN_OCTETS];
    stats->tx_bytes = counters[STAT_IDX_IF_OUT_OCTETS];
    stats->rx_errors = counters[STAT_IDX_IF_IN_ERRORS];
    stats->tx_errors = counters[STAT_IDX_IF_OUT_ERRORS];
    stats->rx_dropped = counters[STAT_IDX_IF_IN_DISCARDS];
    stats->tx_dropped = counters[STAT_IDX_IF_OUT_DISCARDS];
    stats->multicast = counters[STAT_IDX_ETHER_STATS_MULTICAST_PKTS];
    stats->collisions = counters[STAT_IDX_ETHER_STATS_COLLISIONS];
#ifndef MLNX_SAI
    stats->rx_over_errors = counters[STAT_IDX_ETHER_RX_OVERSIZE_PKTS];
#endif
#if 1
    stats->rx_crc_errors = counters[STAT_IDX_IF_IN_CRC_ERR_PKTS];
#else
    stats->rx_crc_errors = counters[STAT_IDX_ETHER_STATS_CRC_ALIGN_ERRORS];
#endif

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Get port split info.
 *
 * @param[in] hw_id port HW lane id.
 * @param[in] mode split mode for which info requested.
 * @param[out] info pointer to split info structure.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int __port_split_info_get(uint32_t hw_id, enum ops_sai_port_split mode,
                          struct split_info
                          *info)
{
    info->disable_neighbor = false;
    return 0;
}

/*
 * Split port.
 *
 * @param[in] hw_id parent port label id.
 * @param[in] mode port split mode.
 * @param[in] speed port speed.
 * @param[in] sub_intf_hw_id_cnt count of sub-interfaces HW IDs.
 * @param[in] sub_intf_hw_id list of sub-interfaces HW IDs.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int __port_split(uint32_t hw_id, enum ops_sai_port_split mode, uint32_t speed,
                 uint32_t sub_intf_hw_id_cnt, const uint32_t *sub_intf_hw_id)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t hw_id_list[SAI_MAX_LANES] = { };

    NULL_PARAM_LOG_ABORT(sub_intf_hw_id);
    ovs_assert(sub_intf_hw_id_cnt <= SAI_MAX_LANES);

    memcpy(hw_id_list, sub_intf_hw_id, sizeof(*hw_id_list) * sub_intf_hw_id_cnt);
    qsort(hw_id_list, sub_intf_hw_id_cnt, sizeof(*hw_id_list), __hw_lane_cmp);

    VLOG_INFO("Port split (hw_id: %u, mode: %d, sub-intf-cnt: %u)",
              hw_id, mode, sub_intf_hw_id_cnt);

    switch (mode) {
    case OPS_SAI_PORT_SPLIT_UNSPLIT:
            status = __port_unsplit(hw_id, speed, sub_intf_hw_id_cnt, hw_id_list);
            SAI_ERROR_EXIT(status);
            break;
    case OPS_SAI_PORT_SPLIT_TO_2:
            status = __port_split_to_2(hw_id, speed, sub_intf_hw_id_cnt, hw_id_list);
            SAI_ERROR_EXIT(status);
            break;
    case OPS_SAI_PORT_SPLIT_TO_4:
            status = __port_split_to_4(hw_id, speed, sub_intf_hw_id_cnt, hw_id_list);
            SAI_ERROR_EXIT(status);
            break;
    default:
        ovs_assert(false);
        break;
    }

exit:
    VLOG_INFO("Port split finished (status: %d), ",
              status);
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Applies all supported port configuration except from hw_enable.
 *
 * @param[in] hw_id port label id.
 * @param[in] new pointer to new port configuration.
 * @param[out] old pointer to current port configuration, will be updated with
 * values from new configuration.
 *
 * @return SAI_STATUS_SUCCESS, sai specific error otherwise.
 */
static sai_status_t
__set_hw_intf_config_full(uint32_t hw_id,
                          const struct ops_sai_port_config *new,
                          struct ops_sai_port_config *old)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_id = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    NULL_PARAM_LOG_ABORT(old);
    NULL_PARAM_LOG_ABORT(new);

    if (old->autoneg != new->autoneg) {
        attr.id = SAI_PORT_ATTR_AUTO_NEG_MODE;
        attr.value.booldata = new->autoneg;
        status = sai_api->port_api->set_port_attribute(port_id, &attr);
        SAI_ERROR_LOG_EXIT(status, "Failed to set autoneg %d for port %d",
                           new->autoneg, hw_id);
    }

    if (old->speed != new->speed) {
        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = new->speed;
        status = sai_api->port_api->set_port_attribute(port_id, &attr);
        SAI_ERROR_LOG_EXIT(status, "Failed to set speed %d for port %d",
                           new->speed, hw_id);
    }

    if (old->mtu != new->mtu) {
        status = ops_sai_port_mtu_set(hw_id, new->mtu);
        ERRNO_EXIT(status);
    }

    if ((old->pause_tx != new->pause_tx) || (old->pause_rx != new->pause_rx)) {
        attr.id = SAI_PORT_ATTR_GLOBAL_FLOW_CONTROL;
        attr.value.u32 = sai_port_pause(new->pause_tx, new->pause_rx);
        status = sai_api->port_api->set_port_attribute(port_id, &attr);
        SAI_ERROR_LOG_EXIT(status, "Failed to set pause %d for port %d",
                           new->speed, hw_id);
    }

    /* TODO: Duplex - integrate with SAI. */

exit:
    return status;
}

/*
 * Get SAI flow control mode based on tx and rx pause values.
 *
 * @param[in] pause_tx enable flow control for tx.
 * @param[in] pause_rx enable flow control for rx.
 *
 * @return sai flow control mode - either both, rx only, tx only or none.
 */
static sai_port_flow_control_mode_t
sai_port_pause(bool pause_tx, bool pause_rx)
{
    if (pause_tx && pause_rx) {
        return SAI_PORT_FLOW_CONTROL_BOTH_ENABLE;
    }
    if (pause_tx) {
        return SAI_PORT_FLOW_CONTROL_TX_ONLY;
    }
    if (pause_rx) {
        return SAI_PORT_FLOW_CONTROL_RX_ONLY;
    }

    return SAI_PORT_FLOW_CONTROL_DISABLE;
}
static sai_status_t
__port_unsplit(uint32_t hw_id, uint32_t speed,
               uint32_t sub_intf_hw_id_cnt, uint32_t *hw_id_list)
{
    return SAI_STATUS_NOT_SUPPORTED;
}

static sai_status_t
__port_split_to_2(uint32_t hw_id, uint32_t speed,
                  uint32_t sub_intf_hw_id_cnt, uint32_t *hw_id_list)
{
    return SAI_STATUS_NOT_SUPPORTED;
}

static sai_status_t
__port_split_to_4(uint32_t hw_id, uint32_t speed,
                  uint32_t sub_intf_hw_id_cnt, uint32_t *hw_id_list)
{
    return SAI_STATUS_NOT_SUPPORTED;
}

int ops_sai_port_ingress_filter_set(uint32_t hw_id, bool enable)
{
    ovs_assert(ops_sai_port_class()->ingress_filter_set);
    return ops_sai_port_class()->ingress_filter_set(hw_id, enable);
}

int __port_ingress_filter_set(uint32_t hw_id, bool enable)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_INGRESS_FILTERING;
    attr.value.booldata = enable;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set ingress filter %d for port %u",
                       enable, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int ops_sai_port_drop_tagged_set(uint32_t hw_id, bool enable)
{
    ovs_assert(ops_sai_port_class()->drop_tagged_set);
    return ops_sai_port_class()->drop_tagged_set(hw_id, enable);
}

int __port_drop_tagged_set(uint32_t hw_id, bool enable)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_DROP_TAGGED;
    attr.value.booldata = enable;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set tagged %d for port %u",
                       enable, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int ops_sai_port_drop_untagged_set(uint32_t hw_id, bool enable)
{
    ovs_assert(ops_sai_port_class()->drop_untagged_set);
    return ops_sai_port_class()->drop_untagged_set(hw_id, enable);
}

int __port_drop_untagged_set(uint32_t hw_id, bool enable)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_DROP_UNTAGGED;
    attr.value.booldata = enable;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set untagged %d for port %u",
                       enable, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int ops_sai_port_pvid_untag_enable_set(uint32_t hw_id, bool enable)
{
    ovs_assert(ops_sai_port_class()->pvid_untag_enable_set);
    return ops_sai_port_class()->pvid_untag_enable_set(hw_id, enable);
}

int __port_pvid_untag_enable_set(uint32_t hw_id, bool enable)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_CUSTOM_UNTAG_DEFAULT_VLAN;
    attr.value.u32 = (uint32_t)enable;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set __port_pvid_untag_enable_set %d for port %u",
                       enable, hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

int ops_sai_port_sample_packet_set(uint32_t hw_id, handle_t id)
{
    ovs_assert(ops_sai_port_class()->sample_packet_set);
    return ops_sai_port_class()->sample_packet_set(hw_id, id);
}

int __port_sample_packet_set(uint32_t hw_id, handle_t id)
{
    sai_attribute_t attr = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    attr.id = SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE;
    attr.value.oid = id.data;
    status = sai_api->port_api->set_port_attribute(port_oid, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to set __port_sample_packet_set %lx for port %u",id.data,hw_id);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

DEFINE_GENERIC_CLASS(struct port_class, port) = {
        .init = __port_init,
        .config_get = __port_config_get,
        .config_set = __port_config_set,
        .mtu_get = __port_mtu_get,
        .mtu_set = __port_mtu_set,
        .carrier_get = __port_carrier_get,
        .flags_update = __port_flags_update,
        .pvid_get = __port_pvid_get,
        .pvid_set = __port_pvid_set,
        .stats_get = __port_stats_get,
        .split_info_get = __port_split_info_get,
        .split = __port_split,
        .ingress_filter_set = __port_ingress_filter_set,
        .drop_tagged_set    = __port_drop_tagged_set,
        .drop_untagged_set  = __port_drop_untagged_set,
        .pvid_untag_enable_set = __port_pvid_untag_enable_set,
	.sample_packet_set = __port_sample_packet_set,
        .deinit = __port_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct port_class, port);
