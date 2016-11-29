/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <malloc.h>
#include <string.h>
#include <sai-api-class.h>
#include <sai-log.h>
#include <sai-netdev.h>
#include <util.h>
#include <sai-vendor.h>
#include <sai-common.h>
#include <ofproto/ofproto-provider.h>
#include "sai-ofproto-notification.h"
#include <sai-ofproto-provider.h>

VLOG_DEFINE_THIS_MODULE(sai_api_class);

static sai_fdb_event_notification_fn sai_fdb_event_callback = NULL;

static struct ops_sai_api_class sai_api;
static sai_object_id_t hw_lane_id_to_oid_map[SAI_PORTS_MAX * SAI_MAX_LANES];
static struct eth_addr sai_api_mac;
static char sai_api_mac_str[MAC_STR_LEN + 1];
static char sai_config_file_path[PATH_MAX] = { };

static const char *__profile_get_value(sai_switch_profile_id_t, const char *);
static int __profile_get_next_value(sai_switch_profile_id_t, const char **,
                                    const char **);
static void __event_switch_state_changed(sai_switch_oper_status_t);
static void __event_fdb(uint32_t, sai_fdb_event_notification_data_t *);
static void __event_port_state(uint32_t,
                               sai_port_oper_status_notification_t *);
static void __event_port(uint32_t, sai_port_event_notification_t *);
static void __event_switch_shutdown(void);
static void __event_rx_packet(const void *, sai_size_t, uint32_t,
                                const sai_attribute_t *);
static sai_status_t __get_port_hw_lane_id(sai_object_id_t, uint32_t *);
static sai_status_t __init_ports(void);

/**
 * Initialize SAI api. Register callbacks, query APIs.
 */
void
ops_sai_api_init(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;

    static const service_method_table_t sai_services = {
        __profile_get_value,
        __profile_get_next_value,
    };
    static sai_switch_notification_t sai_events = {
        __event_switch_state_changed,
        __event_fdb,
        __event_port_state,
        __event_port,
        __event_switch_shutdown,
        __event_rx_packet,
    };

    SAI_API_TRACE_FN();

    if (sai_api.initialized) {
        status = SAI_STATUS_FAILURE;
        SAI_ERROR_LOG_EXIT(status, "SAI api already initialized");
    }

    status = ops_sai_vendor_base_mac_get(sai_api_mac.ea);
    SAI_ERROR_LOG_EXIT(status, "Failed to get base MAC address");
    sprintf(sai_api_mac_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
            sai_api_mac.ea[0], sai_api_mac.ea[1],
            sai_api_mac.ea[2], sai_api_mac.ea[3],
            sai_api_mac.ea[4], sai_api_mac.ea[5]);

    status = ops_sai_vendor_config_path_get(sai_config_file_path,
                                            sizeof(sai_config_file_path));
    SAI_ERROR_LOG_EXIT(status, "Failed to get config file path");

    status = sai_api_initialize(0, &sai_services);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI api");

    status = sai_api_query(SAI_API_SWITCH, (void **) &sai_api.switch_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI switch api");


    status = sai_api.switch_api->initialize_switch(0, "SX", "/etc/spec/spec.txt", &sai_events);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize switch");

    status = sai_api_query(SAI_API_PORT, (void **) &sai_api.port_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI port api");

    status = sai_api_query(SAI_API_VLAN, (void **) &sai_api.vlan_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI vlan api");

    status = sai_api_query(SAI_API_HOST_INTERFACE,
                           (void **) &sai_api.host_interface_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI host interface api");

    status = sai_api_query(SAI_API_POLICER,
                           (void **) &sai_api.policer_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI policer api");

    status = sai_api_query(SAI_API_HASH,
                           (void **) &sai_api.hash_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI hash api");

    status = sai_api_query(SAI_API_MIRROR, (void **) &sai_api.mirror_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI Mirror api");

    status = sai_api_query(SAI_API_STP,
                           (void **) &sai_api.stp_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI Stp api");

    status = sai_api_query(SAI_API_LAG,
                           (void **) &sai_api.lag_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI LAG api");

    status = sai_api_query(SAI_API_FDB,
                           (void **) &sai_api.fdb_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI FDB api");

    status = sai_api_query(SAI_API_ROUTER_INTERFACE,
                           (void **) &sai_api.rif_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI router interface api");

    status = sai_api_query(SAI_API_ROUTE,
                           (void **) &sai_api.route_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI route api");

    status = sai_api_query(SAI_API_NEIGHBOR,
                           (void **) &sai_api.neighbor_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI neighbor api");

    status = sai_api_query(SAI_API_NEXT_HOP,
                           (void **) &sai_api.nexthop_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI nexthop api");

    status = sai_api_query(SAI_API_NEXT_HOP_GROUP,
                           (void **) &sai_api.nhg_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI nexthop group api");

    status = sai_api_query(SAI_API_VIRTUAL_ROUTER,
                           (void **) &sai_api.router_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI virtual router api");

    status = sai_api_query(SAI_API_SAMPLEPACKET,
                           (void **) &sai_api.samplepacket_api);
    SAI_ERROR_LOG_EXIT(status, "Failed to initialize SAI virtual router api");

    status = __init_ports();
    SAI_ERROR_LOG_EXIT(status, "Failed to create interfaces");

    sai_api.initialized = true;

exit:
    if (SAI_ERROR_2_ERRNO(status)) {
        ovs_assert(false);
    }
}

/**
 * Uninitialie SAI api.
 */
int
ops_sai_api_uninit(void)
{
    sai_status_t status = SAI_STATUS_SUCCESS;

    SAI_API_TRACE_FN();

    sai_api.initialized = false;
    status = sai_api_uninitialize();
    SAI_ERROR_LOG_EXIT(status, "Failed to uninitialize SAI api");

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/**
 * Get SAI api class. API has to be alreadyu initialize with sai_api_init().
 * @return pointer to sai_api class.
 */
const struct ops_sai_api_class *
ops_sai_api_get_instance(void)
{
    ovs_assert(sai_api.initialized);
    return &sai_api;
}

/**
 * Convert port label ID to sai_object_id_t.
 *
 * @param[in] hw_id port HW lane id.
 *
 * @return sai_object_id_t of requested port.
 */
sai_object_id_t
ops_sai_api_port_map_get_oid(uint32_t hw_id)
{
    ovs_assert(hw_id <= ARRAY_SIZE(hw_lane_id_to_oid_map));
    return hw_lane_id_to_oid_map[hw_id];
}

/**
 * Delete port label ID from map.
 *
 * @param[in] hw_id port HW lane id.
 *
 * @return sai_object_id_t of requested port.
 */
void ops_sai_api_port_map_delete(uint32_t hw_id)
{
    ovs_assert(hw_id <= ARRAY_SIZE(hw_lane_id_to_oid_map));
    hw_lane_id_to_oid_map[hw_id] = SAI_NULL_OBJECT_ID;
}

/**
 * Add port label ID to map.
 *
 * @param[in] hw_id port HW lane id.
 * @param[in] oid SAI port object ID.
 *
 * @return sai_object_id_t of requested port.
 */
void ops_sai_api_port_map_add(uint32_t hw_id, sai_object_id_t oid)
{
    ovs_assert(hw_id <= ARRAY_SIZE(hw_lane_id_to_oid_map));
    hw_lane_id_to_oid_map[hw_id] = oid;
}

/**
 * Read device base MAC address.
 * @param[out] mac pointer to MAC buffer.
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
int
ops_sai_api_base_mac_get(struct eth_addr *mac)
{
    memcpy(mac, &sai_api_mac, sizeof(*mac));

    return 0;
}

/*
 * Return value requested by SAI using string key.
 */
static const char *
__profile_get_value(sai_switch_profile_id_t profile_id, const char *variable)
{
    SAI_API_TRACE_FN();

    NULL_PARAM_LOG_ABORT(variable);

    if (!strcmp(variable, "BOARD_CONFIG_FILE_PATH")) {
        return sai_config_file_path;
    } else if (!strcmp(variable, "DEVICE_MAC_ADDRESS")) {
        return sai_api_mac_str;
    } else if (!strcmp(variable, "INITIAL_FAN_SPEED")) {
        return "50";
    }

    return NULL;
}

/*
 * Return next value requested by SAI using string key.
 */
static int
__profile_get_next_value(sai_switch_profile_id_t profile_id,
                           const char **variable, const char **value)
{
    SAI_API_TRACE_FN();

    return -1;
}

/*
 * Function will be called by SAI when switch state changes.
 */
static void
__event_switch_state_changed(sai_switch_oper_status_t switch_oper_status)
{
    SAI_API_TRACE_FN();
}

/*
 * Function will be called by SAI on fdb event.
 */
static void
__event_fdb(uint32_t count, sai_fdb_event_notification_data_t * data)
{
    SAI_API_TRACE_FN();

    if(sai_fdb_event_callback)
        sai_fdb_event_callback(count,data);
}

/*
 * Function will be called by SAI when port state changes.
 */
static void
__event_port_state(uint32_t count,
                   sai_port_oper_status_notification_t * data)
{
    uint32_t i = 0;

    SAI_API_TRACE_FN();

    NULL_PARAM_LOG_ABORT(data);

    for (i = 0; i < count; i++) {
        netdev_sai_port_oper_state_changed(data[i].port_id,
                                           SAI_PORT_OPER_STATUS_UP ==
                                           data[i].port_state);
    }
}

/*
 * Function will be called by SAI on port event.
 */
static void
__event_port(uint32_t count, sai_port_event_notification_t * data)
{
    SAI_API_TRACE_FN();
}

/*
 * Function will be called by SAI on switch shutdown.
 */
static void
__event_switch_shutdown(void)
{
    SAI_API_TRACE_FN();
}

const sai_attribute_t *
find_attrib_in_list(uint32_t attr_count, const sai_attribute_t * attr_list, sai_attr_id_t attrib_id)
{
    int ii = 0;

    for (ii = 0; ii < attr_count; ii++) {
         if (attr_list[ii].id == attrib_id) {
             return &attr_list[ii];
         }
     }

    return 0;
}

/*
 * Function will be called by SAI on rx packet.
 */
static void
__event_rx_packet(const void *buffer, sai_size_t buffer_size,
                  uint32_t attr_count, const sai_attribute_t * attr_list)
{
    struct notification_params  params;
    const sai_attribute_t             *trap_id;
    const sai_attribute_t             *ingress_oid;
    const sai_attribute_t             *vlan_id;
    struct netdev               *netdev;
    handle_t                          handle;
    static char 				*packets = NULL;

    SAI_API_TRACE_FN();

    if(packets == NULL){
	packets = xmalloc(9600 * sizeof(char));
	if(packets == NULL){
		return ;
	}
    }

    memcpy(packets, buffer, buffer_size);

    params.packet_params.buffer = packets;
    params.packet_params.buffer_size = buffer_size;

    trap_id = find_attrib_in_list(attr_count,attr_list,SAI_HOSTIF_PACKET_TRAP_ID);
    ingress_oid = find_attrib_in_list(attr_count,attr_list,SAI_HOSTIF_PACKET_INGRESS_PORT);
    vlan_id = find_attrib_in_list(attr_count,attr_list,SAI_HOSTIF_PACKET_VLAN_ID);

    ovs_assert(trap_id != NULL);
    ovs_assert(ingress_oid != NULL);
    ovs_assert(vlan_id != NULL);

    params.packet_params.trap_id = trap_id->value.s32;
    params.packet_params.vlan_id = vlan_id->value.u16;

    handle.data = ingress_oid->value.oid;
    netdev = netdev_get_by_hand_id(handle);

    ovs_assert(netdev != NULL);

    params.packet_params.netdev_ = netdev;

    execute_notification_block(&params, BLK_NOTIFICATION_SWITCH_PACKET);

    return ;
}

/*
 * Get port label ID bi sai_object_id_t.
 */
static sai_status_t
__get_port_hw_lane_id(sai_object_id_t oid, uint32_t *hw_lane_id)
{
    sai_attribute_t attr;
    uint32_t hw_lanes[SAI_MAX_LANES];
    sai_status_t status = SAI_STATUS_SUCCESS;

    NULL_PARAM_LOG_ABORT(hw_lane_id);

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = SAI_MAX_LANES;
    attr.value.u32list.list = hw_lanes;

    status = sai_api.port_api->get_port_attribute(oid, 1, &attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to get port HW lane list (port: %lu)",
                       oid);

    if (attr.value.u32list.count < 1) {
        status = SAI_STATUS_FAILURE;
        goto exit;
    }

    *hw_lane_id = hw_lanes[0];
    VLOG_DBG("Port HW lane ID to SAI OID mapping (hw_lane: %u, oid: 0x%lx",
             *hw_lane_id, oid);

exit:
    return status;
}

/*
 * Initialize physical ports list.
 */
static sai_status_t
__init_ports(void)
{
    uint32_t hw_lane_id = 0;
    sai_uint32_t port_number = 0;
    sai_attribute_t switch_attrib = { };
    sai_status_t status = SAI_STATUS_SUCCESS;
    static sai_object_id_t sai_oids[SAI_PORTS_MAX];

    switch_attrib.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = sai_api.switch_api->get_switch_attribute(1, &switch_attrib);
    SAI_ERROR_LOG_EXIT(status, "Failed to get switch port number");

    port_number = switch_attrib.value.u32;
    switch_attrib.id = SAI_SWITCH_ATTR_PORT_LIST;
    switch_attrib.value.objlist.count = port_number;
    switch_attrib.value.objlist.list = sai_oids;
    status = sai_api.switch_api->get_switch_attribute(1, &switch_attrib);
    SAI_ERROR_LOG_EXIT(status, "Failed to get switch port list");

    for (int i = 0; i < port_number; ++i) {
        status = __get_port_hw_lane_id(sai_oids[i], &hw_lane_id);
        SAI_ERROR_LOG_EXIT(status, "Failed to get switch port list");

        ops_sai_api_port_map_add(hw_lane_id, sai_oids[i]);
    }

exit:
    return status;
}

void
ops_sai_fdb_event_register(sai_fdb_event_notification_fn fn_cb)
{
    sai_fdb_event_callback = fn_cb;
}
