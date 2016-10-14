/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <errno.h>

#include <seq.h>
#include <coverage.h>
#include <hmap.h>
#include <vlan-bitmap.h>
#include <socket-util.h>
#include <ofproto/ofproto-provider.h>
#include <ofproto/bond.h>
#include <ofproto/tunnel.h>

#include <vswitch-idl.h>
#include <openswitch-idl.h>

#include <sai-vendor.h>
#include <sai-common.h>
#include <sai-netdev.h>
#include <sai-api-class.h>
#include <sai-log.h>
#include <sai-port.h>
#include <sai-vlan.h>
#include <sai-mirror.h>
#include <sai-netdev.h>
#include <sai-router.h>
#include <sai-host-intf.h>
#include <sai-router-intf.h>
#include <sai-route.h>
#include <sai-neighbor.h>
#include <sai-hash.h>
#include <sai-classifier.h>
#include <sai-ofproto-provider.h>
#include <sai-mac-learning.h>
#include <sai-stp.h>
#include <sai-lag.h>
#include <sai-fdb.h>
#include "asic-plugin.h"
#include <netinet/ether.h>
#include "bridge.h"

#define SAI_INTERFACE_TYPE_SYSTEM "system"
#define SAI_INTERFACE_TYPE_VRF "vrf"
#define SAI_DATAPATH_VERSION "0.0.1"

VLOG_DEFINE_THIS_MODULE(ofproto_sai);

handle_t	        invalid_handle    = HANDLE_INITIALIZAER;

/* All existing ofproto provider instances, indexed by ->up.name. */
static struct hmap all_ofproto_sai = HMAP_INITIALIZER(&all_ofproto_sai);

static const unsigned long empty_trunks[BITMAP_N_LONGS(VLAN_BITMAP_SIZE)];

static void __init(const struct shash *);
static void __enumerate_types(struct sset *);
static int __enumerate_names(const char *, struct sset *);
static int __del(const char *, const char *);
static const char *__port_open_type(const char *, const char *);
static struct ofproto *__alloc(void);
static int __construct(struct ofproto *);
static void __destruct(struct ofproto *);
static void __sai_dealloc(struct ofproto *);
static inline struct ofport_sai *__ofport_sai_cast(const struct ofport *);
static struct ofport *__port_alloc(void);
static int __port_construct(struct ofport *);
static void __port_destruct(struct ofport *);
static void __port_dealloc(struct ofport *);
static void __port_reconfigured(struct ofport *, enum ofputil_port_config);
static int __port_query_by_name(const struct ofproto *, const char *,
                                struct ofproto_port *);
static struct ofport_sai *__get_ofp_port(const struct ofproto_sai *,
                                         ofp_port_t);
static int __port_add(struct ofproto *, struct netdev *netdev);
static int __port_del(struct ofproto *, ofp_port_t);
static int __port_get_stats(const struct ofport *, struct netdev_stats *);
static int __port_dump_start(const struct ofproto *, void **);
static int __port_dump_next(const struct ofproto *, void *,
                            struct ofproto_port *);
static int __port_dump_done(const struct ofproto *, void *);
static struct rule *__rule_alloc(void);
static void __rule_dealloc(struct rule *);
static enum ofperr __rule_construct(struct rule *);
static void __rule_insert(struct rule *, struct rule *, bool);
static void __rule_delete(struct rule *);
static void __rule_destruct(struct rule *);
static void __rule_get_stats(struct rule *, uint64_t *, uint64_t *,
                             long long int *);
static enum ofperr __rule_execute(struct rule *, const struct flow *,
                                  struct dp_packet *);
static bool __set_frag_handling(struct ofproto *, enum ofp_config_flags);
static enum ofperr __packet_out(struct ofproto *, struct dp_packet *,
                                const struct flow *, const struct ofpact *,
                                size_t);
static int __ofbundle_port_add(struct ofbundle_sai *, struct ofport_sai *);
static int __ofbundle_port_del(struct ofport_sai *);
static void __trunks_realloc(struct ofbundle_sai *, const unsigned long *);
static int __native_tagged_vlan_set(int, uint32_t, bool);
static int __vlan_reconfigure(struct ofbundle_sai *,
                              const struct ofproto_bundle_settings *);
static int __ofbundle_ports_reconfigure(struct ofbundle_sai *,
                                        const struct
                                        ofproto_bundle_settings *);
static int __ofbundle_router_intf_reconfigure(struct ofbundle_sai *,
                                                const struct
                                                ofproto_bundle_settings *);
static int __ofbundle_router_intf_remove(struct ofbundle_sai *);
static int __ofbundle_ip_reconfigure(struct ofbundle_sai *,
                                       const struct ofproto_bundle_settings *);
static int __ofbundle_ip_remove(struct ofbundle_sai *);
static int __ofbundle_ip_secondary_reconfigure(struct ofbundle_sai *,
                                               const struct
                                               ofproto_bundle_settings *,
                                               bool);
static struct ip_address *__ofbundle_ip_secondary_find(struct
                                                       ofbundle_sai *,
                                                       const char *);
static int __ofproto_ip_add(struct ofproto *, const char *, bool);
static int __ofproto_ip_remove(struct ofproto *, const char *, bool);

static void __ofbundle_rename(struct ofbundle_sai *, const char *);
static struct ofbundle_sai *__ofbundle_create(struct ofproto_sai *, void *,
                                              const struct
                                              ofproto_bundle_settings *);
static void __ofbundle_destroy(struct ofbundle_sai *, bool);
static struct ofbundle_sai *__ofbundle_lookup(struct ofproto_sai *, void *);
static struct ofbundle_sai *__ofbundle_lookup_by_netdev_name(struct
                                                               ofproto_sai *,
                                                               const char *);

static int __bundle_set(struct ofproto *, void *,
                                  const struct ofproto_bundle_settings *);
static void __bundle_remove(struct ofport *);
static int __bundle_get(struct ofproto *, void *, int *);
static bool __is_bundle_active(struct ofproto *,
                               const struct ofbundle_sai *,
                               const struct ofproto_bundle_settings *);
static void __bundle_setting_copy(struct ofbundle_sai *,
                                  const struct ofproto_bundle_settings *);
static void __bundle_setting_free(struct ofbundle_sai *);
static void __bundle_cache_init(struct ofbundle_sai *);
static void __bundle_cache_free(struct ofbundle_sai *);
static int __set_vlan(struct ofproto *, int, bool);
static inline struct ofproto_sai_group *__ofproto_sai_group_cast(const struct
                                                                 ofgroup *);
static struct ofgroup *__group_alloc(void);
static enum ofperr __group_construct(struct ofgroup *);
static void __group_destruct(struct ofgroup *);
static void __group_dealloc(struct ofgroup *);
static enum ofperr __group_modify(struct ofgroup *);
static enum ofperr __group_get_stats(const struct ofgroup *,
                                     struct ofputil_group_stats *);
static const char *__get_datapath_version(const struct ofproto *);
static struct neigbor_entry* __neigh_entry_hash_find(const char *,
                                                     const struct
                                                     ofbundle_sai *);
static void __neigh_entry_hash_add(const char *, const char *,
                                   struct ofbundle_sai *);
static void __neigh_entry_hash_remove(const char *, struct ofbundle_sai *);

static int __add_l3_host_entry(const struct ofproto *, void *, bool, char *,
                               char *, int *);
static int __delete_l3_host_entry(const struct ofproto *, void *, bool, char *,
                                  int *);
static int __get_l3_host_hit_bit(const struct ofproto *, void *, bool, char *,
                                 bool *);
static int __l3_route_action(const struct ofproto *, enum ofproto_route_action,
                             struct ofproto_route *);
static void __l3_local_route_attach_to_bundle(struct ofbundle_sai *, char *);
static void __l3_local_route_dettach_from_bundle(struct ofbundle_sai *, char *);
static int __l3_ecmp_set(const struct ofproto *, bool);
static int __l3_ecmp_hash_set(const struct ofproto *, unsigned int, bool);
static int __run(struct ofproto *);
static void __wait(struct ofproto *);
static void __set_tables_version(struct ofproto *, cls_version_t);
static void __ofproto_bundle_settings_dump(const
                                           struct ofproto_bundle_settings *s);

static struct ofmirror_sai *__ofmirror_lookup(struct ofproto_sai *ofproto, void *aux);
static void __ofmirror_destroy(struct ofmirror_sai *mirror, int all);
static int __mirror_set(struct ofproto *ofproto_, void *aux,
        const struct ofproto_mirror_settings *s);
static int __mirror_get_stats(struct ofproto *ofproto, void *aux,
        uint64_t *packets, uint64_t *bytes);
static bool __is_mirror_output_bundle(const struct ofproto *ofproto, void *aux);
static void __mirror_get_bundle_port_info(struct ofbundle_sai *bundle, sai_mirror_porttype_t *pt, sai_mirror_portid_t *portid);

void __sai_register_stg_mac_learning_plugin_init();
static int __ofproto_stp_init();

static void __ofproto_lag_port_update (int lag_handle, struct ofport_sai *port, bool add);
static void __ofproto_lag_create(struct ofbundle_sai *bundle);
static void __ofproto_lag_destroy(struct ofbundle_sai *bundle);

const struct ofproto_class ofproto_sai_class = {
    PROVIDER_INIT_GENERIC(init,                  __init)
    PROVIDER_INIT_GENERIC(enumerate_types,       __enumerate_types)
    PROVIDER_INIT_GENERIC(enumerate_names,       __enumerate_names)
    PROVIDER_INIT_GENERIC(del,                   __del)
    PROVIDER_INIT_GENERIC(port_open_type,        __port_open_type)
    PROVIDER_INIT_GENERIC(type_run,              NULL)
    PROVIDER_INIT_GENERIC(type_wait,             NULL)
    PROVIDER_INIT_GENERIC(alloc,                 __alloc)
    PROVIDER_INIT_GENERIC(construct,             __construct)
    PROVIDER_INIT_GENERIC(destruct,              __destruct)
    PROVIDER_INIT_GENERIC(dealloc,               __sai_dealloc)
    PROVIDER_INIT_GENERIC(run,                   __run)
    PROVIDER_INIT_GENERIC(wait,                  __wait)
    PROVIDER_INIT_GENERIC(get_memory_usage,      NULL)
    PROVIDER_INIT_GENERIC(type_get_memory_usage, NULL)
    PROVIDER_INIT_GENERIC(flush,                 NULL)
    PROVIDER_INIT_GENERIC(query_tables,          NULL)
    PROVIDER_INIT_GENERIC(set_tables_version,    __set_tables_version)
    PROVIDER_INIT_GENERIC(port_alloc,            __port_alloc)
    PROVIDER_INIT_GENERIC(port_construct,        __port_construct)
    PROVIDER_INIT_GENERIC(port_destruct,         __port_destruct)
    PROVIDER_INIT_GENERIC(port_dealloc,          __port_dealloc)
    PROVIDER_INIT_GENERIC(port_modified,         NULL)
    PROVIDER_INIT_GENERIC(port_reconfigured,     __port_reconfigured)
    PROVIDER_INIT_GENERIC(port_query_by_name,    __port_query_by_name)
    PROVIDER_INIT_GENERIC(port_add,              __port_add)
    PROVIDER_INIT_GENERIC(port_del,              __port_del)
    PROVIDER_INIT_GENERIC(port_get_stats,        __port_get_stats)
    PROVIDER_INIT_GENERIC(port_dump_start,       __port_dump_start)
    PROVIDER_INIT_GENERIC(port_dump_next,        __port_dump_next)
    PROVIDER_INIT_GENERIC(port_dump_done,        __port_dump_done)
    PROVIDER_INIT_GENERIC(port_poll,             NULL)
    PROVIDER_INIT_GENERIC(port_poll_wait,        NULL)
    PROVIDER_INIT_GENERIC(port_is_lacp_current,  NULL)
    PROVIDER_INIT_GENERIC(port_get_lacp_stats,   NULL)
    PROVIDER_INIT_GENERIC(rule_construct,        NULL)
    PROVIDER_INIT_GENERIC(rule_alloc,            __rule_alloc)
    PROVIDER_INIT_GENERIC(rule_construct,        __rule_construct)
    PROVIDER_INIT_GENERIC(rule_insert,           __rule_insert)
    PROVIDER_INIT_GENERIC(rule_delete,           __rule_delete)
    PROVIDER_INIT_GENERIC(rule_destruct,         __rule_destruct)
    PROVIDER_INIT_GENERIC(rule_dealloc,          __rule_dealloc)
    PROVIDER_INIT_GENERIC(rule_get_stats,        __rule_get_stats)
    PROVIDER_INIT_GENERIC(rule_execute,          __rule_execute)
    PROVIDER_INIT_GENERIC(set_frag_handling,     __set_frag_handling)
    PROVIDER_INIT_GENERIC(packet_out,            __packet_out)
    PROVIDER_INIT_GENERIC(set_netflow,           NULL)
    PROVIDER_INIT_GENERIC(get_netflow_ids,       NULL)
    PROVIDER_INIT_GENERIC(set_sflow,             NULL)
    PROVIDER_INIT_GENERIC(set_ipfix,             NULL)
    PROVIDER_INIT_GENERIC(set_cfm,               NULL)
    PROVIDER_INIT_GENERIC(cfm_status_changed,    NULL)
    PROVIDER_INIT_GENERIC(get_cfm_status,        NULL)
    PROVIDER_INIT_GENERIC(set_bfd,               NULL)
    PROVIDER_INIT_GENERIC(bfd_status_changed,    NULL)
    PROVIDER_INIT_GENERIC(get_bfd_status,        NULL)
    PROVIDER_INIT_GENERIC(set_stp,               NULL)
    PROVIDER_INIT_GENERIC(get_stp_status,        NULL)
    PROVIDER_INIT_GENERIC(set_stp_port,          NULL)
    PROVIDER_INIT_GENERIC(get_stp_port_status,   NULL)
    PROVIDER_INIT_GENERIC(get_stp_port_stats,    NULL)
    PROVIDER_INIT_GENERIC(set_rstp,              NULL)
    PROVIDER_INIT_GENERIC(get_rstp_status,       NULL)
    PROVIDER_INIT_GENERIC(set_rstp_port,         NULL)
    PROVIDER_INIT_GENERIC(get_rstp_port_status,  NULL)
    PROVIDER_INIT_GENERIC(set_queues,            NULL)
    PROVIDER_INIT_GENERIC(bundle_set,            __bundle_set)
    PROVIDER_INIT_GENERIC(bundle_remove,         __bundle_remove)
    PROVIDER_INIT_OPS_SPECIFIC(bundle_get,       __bundle_get)
    PROVIDER_INIT_OPS_SPECIFIC(set_vlan,         __set_vlan)
    PROVIDER_INIT_GENERIC(mirror_set,            __mirror_set)
    PROVIDER_INIT_GENERIC(mirror_get_stats,      __mirror_get_stats)
    PROVIDER_INIT_GENERIC(set_flood_vlans,       NULL)
    PROVIDER_INIT_GENERIC(is_mirror_output_bundle, __is_mirror_output_bundle)
    PROVIDER_INIT_GENERIC(forward_bpdu_changed,  NULL)
    PROVIDER_INIT_GENERIC(set_mac_table_config,  NULL)
    PROVIDER_INIT_GENERIC(set_mcast_snooping,    NULL)
    PROVIDER_INIT_GENERIC(set_mcast_snooping_port, NULL)
    PROVIDER_INIT_GENERIC(set_realdev,           NULL)
    PROVIDER_INIT_GENERIC(meter_get_features,    NULL)
    PROVIDER_INIT_GENERIC(meter_set,             NULL)
    PROVIDER_INIT_GENERIC(meter_get,             NULL)
    PROVIDER_INIT_GENERIC(meter_del,             NULL)
    PROVIDER_INIT_GENERIC(group_alloc,           __group_alloc)
    PROVIDER_INIT_GENERIC(group_construct,       __group_construct)
    PROVIDER_INIT_GENERIC(group_destruct,        __group_destruct)
    PROVIDER_INIT_GENERIC(group_dealloc,         __group_dealloc)
    PROVIDER_INIT_GENERIC(group_modify,          __group_modify)
    PROVIDER_INIT_GENERIC(group_get_stats,       __group_get_stats)
    PROVIDER_INIT_GENERIC(get_datapath_version,  __get_datapath_version)
    PROVIDER_INIT_OPS_SPECIFIC(add_l3_host_entry, __add_l3_host_entry)
    PROVIDER_INIT_OPS_SPECIFIC(delete_l3_host_entry, __delete_l3_host_entry)
    PROVIDER_INIT_OPS_SPECIFIC(get_l3_host_hit,  __get_l3_host_hit_bit)
    PROVIDER_INIT_OPS_SPECIFIC(l3_route_action,  __l3_route_action)
    PROVIDER_INIT_OPS_SPECIFIC(l3_ecmp_set,      __l3_ecmp_set)
    PROVIDER_INIT_OPS_SPECIFIC(l3_ecmp_hash_set, __l3_ecmp_hash_set)
};


/**
 * Regster ofproto provider.
 */
void
ofproto_sai_register(void)
{
    ofproto_class_register(&ofproto_sai_class);
}

/*
 * Restore bundle configuration.
 *
 * @param[in] netdev_name name of netdev that bundle represents.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
ofproto_sai_bundle_enable(const char *netdev_name)
{
    int status = 0;
    struct ofbundle_sai *bundle = NULL;
    struct ofproto_sai *ofproto = NULL;
    struct ip_address *addr = NULL;
    struct ip_address *next = NULL;
    bool found = false;

    SAI_API_TRACE_FN();

    VLOG_INFO("Enabling netdev configuration (netdev: %s)", netdev_name);

    HMAP_FOR_EACH(ofproto, all_ofproto_sai_node, &all_ofproto_sai) {
        HMAP_FOR_EACH(bundle, hmap_node, &ofproto->bundles) {
            if (STR_EQ(bundle->name, netdev_name)) {
                found = true;
                break;
            }
        }

        if (found) {
            break;
        }
    }

    if (!found) {
        VLOG_INFO("Bundle not found");
        status = 0;
        goto exit;
    }

    bundle->config_cache.cache_config = false;

    status = __bundle_set(&ofproto->up, bundle->aux, bundle->config_cache.config);
    ERRNO_LOG_EXIT(status, "Failed to restore bundle configuration "
                   "(bundle_name: %s)", bundle->config_cache.config->name);

     HMAP_FOR_EACH_SAFE (addr, next, addr_node, &bundle->local_routes) {
         status = ops_sai_route_local_add(&bundle->ofproto->vrid,
                                          addr->address,
                                          &bundle->router_intf.rifid);
         ERRNO_LOG_EXIT(status,
                        "Failed to restore bundle local route configuration "
                        "(bundle_name: %s)", bundle->config_cache.config->name);
     }

exit:
    return status;
}

/*
 * Disable and cache bundle configuration.
 *
 * @param[in] netdev_name name of netdev that bundle represents.
 *
 * @return 0, sai status converted to errno otherwise.
 */
int
ofproto_sai_bundle_disable(const char *netdev_name)
{
    int status = 0;
    struct ofbundle_sai *bundle = NULL;
    struct ofproto_sai *ofproto = NULL;

    SAI_API_TRACE_FN();

    VLOG_INFO("Disabling netdev configuration (netdev: %s)", netdev_name);

    HMAP_FOR_EACH(ofproto, all_ofproto_sai_node, &all_ofproto_sai) {
        bundle =__ofbundle_lookup_by_netdev_name(ofproto, netdev_name);
        if (bundle) {
            break;
        }
    }

    if (!bundle) {
        VLOG_INFO("Bundle not found");
        status = 0;
        goto exit;
    }


    /* Remove configuration only */
    __ofbundle_destroy(bundle, true);
    bundle->config_cache.cache_config = true;

exit:
    return status;
}

static void
__init(const struct shash *iface_hints)
{
    SAI_API_TRACE_FN();

    ops_sai_api_init();
    ops_sai_port_init();
    ops_sai_vlan_init();
    ops_sai_policer_init();
    ops_sai_router_init();
    ops_sai_host_intf_init();
    ops_sai_router_intf_init();
    ops_sai_neighbor_init();
    ops_sai_route_init();
    ops_sai_host_intf_traps_register();
    ops_sai_ecmp_hash_init();
    ops_sai_classifier_init();
    ops_sai_lag_init();
    ops_sai_stp_init();
    ops_sai_fdb_init();

    __ofproto_stp_init();
    sai_mac_learning_init();
    __sai_register_stg_mac_learning_plugin_init();
}

static void
__enumerate_types(struct sset *types)
{
    SAI_API_TRACE_FN();

    sset_add(types, SAI_INTERFACE_TYPE_VRF);
    sset_add(types, SAI_INTERFACE_TYPE_SYSTEM);
}

static int
__enumerate_names(const char *type, struct sset *names)
{
    struct ofproto_sai *ofproto;

    SAI_API_TRACE_FN();

    sset_clear(names);
    HMAP_FOR_EACH(ofproto, all_ofproto_sai_node, &all_ofproto_sai) {
        if (strcmp(type, ofproto->up.type)) {
            continue;
        }
        sset_add(names, ofproto->up.name);
    }

    return 0;
}

static int
__del(const char *type, const char *name)
{
    SAI_API_TRACE_FN();

    ops_sai_ecmp_hash_deinit();
    ops_sai_host_intf_traps_unregister();
    ops_sai_route_deinit();
    ops_sai_neighbor_deinit();
    ops_sai_router_intf_deinit();
    ops_sai_host_intf_deinit();
    ops_sai_router_deinit();
    ops_sai_policer_deinit();
    ops_sai_vlan_deinit();
    ops_sai_port_deinit();
    ops_sai_api_uninit();

    return 0;
}

static const char *
__port_open_type(const char *datapath_type, const char *port_type)
{
    SAI_API_TRACE_FN();

    VLOG_DBG("datapath_type: %s, port_type: %s", datapath_type, port_type);

    if ((STR_EQ(port_type, OVSREC_INTERFACE_TYPE_INTERNAL)) ||
        (STR_EQ(port_type, OVSREC_INTERFACE_TYPE_VLANSUBINT)) ||
        (STR_EQ(port_type, OVSREC_INTERFACE_TYPE_LOOPBACK))) {
        return port_type;
    } else {
        return SAI_INTERFACE_TYPE_SYSTEM;
    }
}

static struct ofproto *
__alloc(void)
{
    struct ofproto_sai *ofproto = xzalloc(sizeof *ofproto);

    SAI_API_TRACE_FN();

    return &ofproto->up;
}



static int
__construct(struct ofproto *ofproto_)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    int error = 0;

    SAI_API_TRACE_FN();

    VLOG_DBG("constructing ofproto - %s type - %s", ofproto->up.name,
             ofproto->up.type);

    sset_init(&ofproto->ports);
    sset_init(&ofproto->ghost_ports);
    /* Currently ACL is not supported. Implementation will be added in a
     * future. */
    ofproto_init_tables(ofproto_, 1);

    hmap_init(&ofproto->bundles);
    hmap_init(&ofproto->mirrors);

    if (STR_EQ(ofproto_->type, SAI_TYPE_IACL)) {
        VLOG_DBG("iACL container construct placeholder");
        goto exit;
    }
    hmap_insert(&all_ofproto_sai, &ofproto->all_ofproto_sai_node,
                hash_string(ofproto->up.name, 0));

    if (STR_EQ(ofproto_->type, SAI_INTERFACE_TYPE_VRF)) {
        error = ops_sai_router_create(&ofproto->vrid);
        ERRNO_EXIT(error);
    }

exit:
    return error;
}

static void
__destruct(struct ofproto *ofproto_ OVS_UNUSED)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);

    SAI_API_TRACE_FN();

    if (STR_EQ(ofproto_->type, SAI_INTERFACE_TYPE_VRF)) {
        ops_sai_router_remove(&ofproto->vrid);
    }

    sset_destroy(&ofproto->ghost_ports);
    sset_destroy(&ofproto->ports);

    if (STR_EQ(ofproto_->type, SAI_TYPE_IACL)) {
        VLOG_DBG("iACL container destruct placeholder");
        return;
    }

    hmap_remove(&all_ofproto_sai, &ofproto->all_ofproto_sai_node);
}

static void
__sai_dealloc(struct ofproto *ofproto_)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);

    SAI_API_TRACE_FN();

    free(ofproto);
}

static inline struct ofport_sai *
__ofport_sai_cast(const struct ofport *ofport)
{
    return ofport ? CONTAINER_OF(ofport, struct ofport_sai, up) : NULL;
}

static struct ofport *
__port_alloc(void)
{
    struct ofport_sai *port = xzalloc(sizeof *port);

    SAI_API_TRACE_FN();

    return &port->up;
}

static int
__port_construct(struct ofport *port_)
{
    SAI_API_TRACE_FN();

    return 0;
}

static void
__port_destruct(struct ofport *port_ OVS_UNUSED)
{
    SAI_API_TRACE_FN();
}

static void
__port_dealloc(struct ofport *port_)
{
    struct ofport_sai *port = __ofport_sai_cast(port_);

    SAI_API_TRACE_FN();

    free(port);
}

static void
__port_reconfigured(struct ofport *port_, enum ofputil_port_config old_config)
{
    SAI_API_TRACE_FN();

    VLOG_DBG("port_reconfigured %p %d", port_, old_config);
}

static int
__port_query_by_name(const struct ofproto *ofproto_, const char *devname,
                     struct ofproto_port *ofproto_port)
{
    SAI_API_TRACE_FN();

    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    const char *type = netdev_get_type_from_name(devname);

    if (type) {
        const struct ofport *ofport;

        ofport = shash_find_data(&ofproto->up.port_by_name, devname);
        ofproto_port->ofp_port = ofport ? ofport->ofp_port : OFPP_NONE;
        ofproto_port->name = xstrdup(devname);
        ofproto_port->type = xstrdup(type);
        return 0;
    }

    return ENODEV;
}

static struct ofport_sai *
__get_ofp_port(const struct ofproto_sai *ofproto, ofp_port_t ofp_port)
{
    struct ofport *ofport = ofproto_get_port(&ofproto->up, ofp_port);

    return ofport ? __ofport_sai_cast(ofport) : NULL;
}

static int
__port_add(struct ofproto *ofproto_, struct netdev *netdev)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);

    SAI_API_TRACE_FN();

    sset_add(&ofproto->ports, netdev->name);
    return 0;
}

static int
__port_del(struct ofproto *ofproto_, ofp_port_t ofp_port)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    struct ofport_sai *ofport = __get_ofp_port(ofproto, ofp_port);

    SAI_API_TRACE_FN();

    if (ofport) {
        sset_find_and_delete(&ofproto->ports,
                            netdev_get_name(ofport->up.netdev));
    } else {
        VLOG_WARN("Port could not be found (ofproto: %s, ofp_port: %u)",
                  ofproto_->name, ofp_port);
    }
    return 0;
}

static int
__port_get_stats(const struct ofport *ofport_, struct netdev_stats *stats)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static int
__port_dump_start(const struct ofproto *ofproto_ OVS_UNUSED,
                            void **statep)
{
    SAI_API_TRACE_FN();

    *statep = xzalloc(sizeof (struct port_dump_state));

    return 0;
}

static int
__port_dump_next(const struct ofproto *ofproto_, void *state_,
                 struct ofproto_port *port)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    struct port_dump_state *state = state_;
    struct sset_node *node;

    SAI_API_TRACE_FN();

    if (state->has_port) {
        ofproto_port_destroy(&state->port);
        state->has_port = false;
    }

    while ((node = sset_at_position(&ofproto->ports, &state->bucket,
                                    &state->offset))) {
        int error;

        error = __port_query_by_name(ofproto_, node->name, &state->port);
        if (!error) {
            *port = state->port;
            state->has_port = true;
            return 0;
        } else if (error != ENODEV) {
            return error;
        }
    }

    if (!state->ghost) {
        state->ghost = true;
        state->bucket = 0;
        state->offset = 0;
        return __port_dump_next(ofproto_, state_, port);
    }

    return EOF;
}

static int
__port_dump_done(const struct ofproto *ofproto_, void *state_)
{
    struct port_dump_state *state = state_;

    SAI_API_TRACE_FN();

    if (state->has_port) {
        ofproto_port_destroy(&state->port);
    }
    free(state);
    return 0;
}

static struct rule *
__rule_alloc(void)
{
    struct rule *rule = xzalloc(sizeof *rule);

    SAI_API_TRACE_FN();

    return rule;
}

static void
__rule_dealloc(struct rule *rule_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    free(rule_);
}

static enum ofperr
__rule_construct(struct rule *rule_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static void
__rule_insert(struct rule *rule_,
                        struct rule *old_rule,
                        bool forward_stats)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

static void
__rule_delete(struct rule *rule_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return;
}

static void
__rule_destruct(struct rule *rule_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return;
}

static void
__rule_get_stats(struct rule *rule_, uint64_t *packets,
                           uint64_t *bytes OVS_UNUSED,
                           long long int *used OVS_UNUSED)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return;
}

static enum ofperr
__rule_execute(struct rule *rule OVS_UNUSED, const struct flow *flow,
                         struct dp_packet *packet)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static bool
__set_frag_handling(struct ofproto *ofproto_,
                              enum ofp_config_flags frag_handling)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return false;
}

static enum ofperr
__packet_out(struct ofproto *ofproto_, struct dp_packet *packet,
                       const struct flow *flow,
                       const struct ofpact *ofpacts, size_t ofpacts_len)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

/*
 * Add port to bundle.
 */
static int
__ofbundle_port_add(struct ofbundle_sai *bundle, struct ofport_sai *port)
{
    int status = 0;
//    handle_t	portid = HANDLE_INITIALIZAER;
    uint32_t hw_id = netdev_sai_hw_id_get(port->up.netdev);

    if (NULL == port) {
        status = EINVAL;
        ERRNO_LOG_EXIT(status, "Got NULL port to add");
    }

    /* Port belongs to other bundle - remove. */
    if (NULL != port->bundle) {
        VLOG_WARN("Add port to bundle: removing port from old bundle");
        __bundle_remove(&port->up);
    }

    port->bundle = bundle;
    list_push_back(&bundle->ports, &port->bundle_node);

    if (STR_EQ(netdev_get_type(port->up.netdev), OVSREC_INTERFACE_TYPE_SYSTEM)) {
        if (-1 != bundle->vlan) {
            if(PORT_VLAN_NATIVE_TAGGED == bundle->vlan_mode) {
                status = __native_tagged_vlan_set(bundle->vlan, hw_id, true);

                ops_sai_port_pvid_untag_enable_set(hw_id, false);
            } else {
                status = ops_sai_vlan_access_port_add(bundle->vlan, hw_id);
                ERRNO_LOG_EXIT(status, "Failed to add port to bundle");

                ops_sai_port_pvid_untag_enable_set(hw_id, true);
            }
        }

        if (NULL != bundle->trunks) {
            status = ops_sai_vlan_trunks_port_add(bundle->trunks, hw_id);
            ERRNO_LOG_EXIT(status, "Failed to add port to bundle");
        }
    }

    if(bundle->lag_info.is_lag)
    {
        __ofproto_lag_port_update(bundle->lag_info.lag_id, port, true /*add*/);

//	 portid.data = netdev_sai_hw_id_get(port->up.netdev);
//       ops_sai_fdb_flush_entrys(1 /*L2MAC_FLUSH_BY_PORT*/, portid,0);
        sai_mac_learning_l2_addr_flush_by_port(netdev_get_name(port->up.netdev));

	 if(bundle->router_intf.created){
	     status = netdev_sai_set_router_intf_handle(port->up.netdev, &bundle->router_intf.rifid);
            ERRNO_EXIT(status);
	 }
    }

exit:
    return status;
}

/*
 * Remove port from bundle.
 */
static int
__ofbundle_port_del(struct ofport_sai *port)
{
    struct ofbundle_sai *bundle;
    int status = 0;
    uint32_t hw_id = netdev_sai_hw_id_get(port->up.netdev);
    sai_mirror_porttype_t   porttype;
    sai_mirror_portid_t     portid;

    if (NULL == port) {
        status = EINVAL;
        ERRNO_LOG_EXIT(status, "Got NULL port to remove");
    }

    bundle = port->bundle;

    if (STR_EQ(netdev_get_type(port->up.netdev), OVSREC_INTERFACE_TYPE_SYSTEM)) {
        if (-1 != bundle->vlan) {
            status = ops_sai_vlan_access_port_del(bundle->vlan, hw_id);
            ERRNO_LOG_EXIT(status, "Failed to remove port from bundle");
        }

        if (NULL != bundle->trunks) {
            status = ops_sai_vlan_trunks_port_del(bundle->trunks, hw_id);
            ERRNO_LOG_EXIT(status, "Failed to remove port from bundle");
        }
    }

    if(bundle->lag_info.is_lag)
    {
        portid.hw_id = hw_id;
        porttype = SAI_MIRROR_PORT_PHYSICAL;

        if (bundle->ingress_owner)
            ops_sai_mirror_src_del(bundle->ingress_owner->hid,
                    SAI_MIRROR_DATA_DIR_INGRESS, porttype, portid);

        if (bundle->egress_owner)
            ops_sai_mirror_src_del(bundle->egress_owner->hid,
                    SAI_MIRROR_DATA_DIR_EGRESS, porttype, portid);

        __ofproto_lag_port_update(bundle->lag_info.lag_id, port, false /*del*/);

	 if(bundle->router_intf.created){
	     status = netdev_sai_set_router_intf_handle(port->up.netdev, NULL);
            ERRNO_EXIT(status);
	 }
    }

exit:
    list_remove(&port->bundle_node);
    port->bundle = NULL;

    return status;
}
#if 0
/************************************ lag tx member **************************************/

/*
 * Add lag tx member port to bundle.
 */
static int
__ofbundle_lag_tx_port_add(struct ofbundle_sai *bundle, struct ofport_sai *port)
{
    int status = 0;

    if (NULL == port) {
        status = EINVAL;
        ERRNO_LOG_EXIT(status, "Got NULL port to add");
    }

    /* Port belongs to other bundle - remove. */
    if (NULL != port->tx_lag_bundle) {
        VLOG_WARN("Add lag tx member port to bundle: removing port from old bundle");
    }

    port->tx_lag_bundle = bundle;
    list_push_back(&bundle->tx_number_ports, &port->bundle_tx_lag_node);

    if (STR_EQ(netdev_get_type(port->up.netdev), OVSREC_INTERFACE_TYPE_SYSTEM)) {
        if(-1 != bundle->bond_hw_handle)
        {
            status = ops_sai_lag_set_tx_enable(bundle->bond_hw_handle,netdev_sai_hw_id_get(port->up.netdev),true);
            ERRNO_LOG_EXIT(status, "Failed to add lag member port to bundle");
        }
    }

exit:
    return status;
}

/*
 * Remove port from bundle.
 */
static int
__ofbundle_lag_tx_port_del(struct ofport_sai *port)
{
    struct ofbundle_sai *bundle;
    int status = 0;

    if (NULL == port) {
        status = EINVAL;
        ERRNO_LOG_EXIT(status, "Got NULL port to remove");
    }

    bundle = port->tx_lag_bundle;

    if (STR_EQ(netdev_get_type(port->up.netdev), OVSREC_INTERFACE_TYPE_SYSTEM)) {
        if(-1 != bundle->bond_hw_handle)
        {
            status = ops_sai_lag_set_tx_enable(bundle->bond_hw_handle,netdev_sai_hw_id_get(port->up.netdev),false);
            ERRNO_LOG_EXIT(status, "Failed to remove lag member port to bundle");
        }
    }

exit:
    list_remove(&port->bundle_tx_lag_node);
    port->tx_lag_bundle = NULL;

    return status;
}

static int
__lag_tx_member_reconfigure(struct ofbundle_sai *bundle,
                              const struct ofproto_bundle_settings *s, int tx_port_add)
{
    size_t i;
    bool port_found = false;
    int status = 0;
    struct ofport_sai *port = NULL, *next_port = NULL, *s_port = NULL;

    if(0 == tx_port_add) {
        /* Figure out which tx ports were removed. */
        LIST_FOR_EACH_SAFE(port, next_port, bundle_tx_lag_node, &bundle->tx_number_ports) {
            port_found = false;
            for (i = 0; i < s->n_slaves_tx_enable; i++) {
                s_port = __get_ofp_port(bundle->ofproto, s->slaves_tx_enable[i]);
                if (port == s_port) {
                    port_found = true;
                    break;
                }
            }
            if (!port_found) {
                status = __ofbundle_lag_tx_port_del(port);
                ERRNO_LOG_EXIT(status, "Failed to remove lag number port");
            }
        }
    } else {
        /* Figure out which tx ports were added. */
        for (i = 0; i < s->n_slaves_tx_enable; i++) {
            port_found = false;
            s_port = __get_ofp_port(bundle->ofproto, s->slaves_tx_enable[i]);

            LIST_FOR_EACH_SAFE(port, next_port, bundle_tx_lag_node, &bundle->tx_number_ports) {
                if (port == s_port) {
                    port_found = true;
                    break;
                }
            }

            if (!port_found) {
                status = __ofbundle_lag_tx_port_add(bundle,s_port);
            }
            ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");
        }
    }
exit:
    return status;
}

/************************************ lag tx member end **********************************/
#endif
/*
 * Reallocate bundle trunks.
 */
static void
__trunks_realloc(struct ofbundle_sai *bundle,
                            const unsigned long *trunks)
{
    /* Name didn't change. */
    if (vlan_bitmap_equal(bundle->trunks, trunks)) {
        return;
    }
    /* Free old name if there was any. */
    if (NULL != bundle->trunks) {
        free(bundle->trunks);
        bundle->trunks = NULL;
    }
    /* Copy new name if there is any. */
    if (NULL != trunks) {
        bundle->trunks = vlan_bitmap_clone(trunks);
    }
}

/*
 * Set native tagged vlan and corresponding pvid.
 */
static int __native_tagged_vlan_set(int vid, uint32_t hw_id, bool add)
{
    int status = 0;
    static unsigned long trunks[BITMAP_N_LONGS(VLAN_BITMAP_SIZE)];

    bitmap_and(trunks, empty_trunks, VLAN_BITMAP_SIZE);
    bitmap_set1(trunks, vid);

    status =  add ? ops_sai_vlan_trunks_port_add(trunks, hw_id) :
                    ops_sai_vlan_trunks_port_del(trunks, hw_id);
    ERRNO_EXIT(status);

    status = ops_sai_port_pvid_set(hw_id, add ? vid :
                                   OPS_SAI_PORT_DEFAULT_PVID);
    ERRNO_EXIT(status);

exit:
    return status;
}

/*
 * Reconfigure port to vlan settings. Remove ports from vlans that were in
 * bundle and add ports to vlan in new settings.
 */
static int
__vlan_reconfigure(struct ofbundle_sai *bundle,
                              const struct ofproto_bundle_settings *s)
{
    int status = 0;
    bool tag_changed = bundle->vlan != s->vlan;
    bool mod_changed = bundle->vlan_mode != s->vlan_mode;
    struct ofport_sai *port = NULL, *next_port = NULL;
    static unsigned long added_trunks[BITMAP_N_LONGS(VLAN_BITMAP_SIZE)];
    static unsigned long common_trunks[BITMAP_N_LONGS(VLAN_BITMAP_SIZE)];
    static unsigned long removed_trunks[BITMAP_N_LONGS(VLAN_BITMAP_SIZE)];

    /* Initialize all trunks as empty. */
    bitmap_and(added_trunks, empty_trunks, VLAN_BITMAP_SIZE);
    bitmap_and(common_trunks, empty_trunks, VLAN_BITMAP_SIZE);
    bitmap_and(removed_trunks, empty_trunks, VLAN_BITMAP_SIZE);
    /* Copy trunks from settings and bundle. */
    if (NULL != s->trunks) {
        bitmap_or(added_trunks, s->trunks, VLAN_BITMAP_SIZE);
    }
    if (NULL != bundle->trunks) {
        bitmap_or(removed_trunks, bundle->trunks, VLAN_BITMAP_SIZE);
    }
    bitmap_or(common_trunks, added_trunks, VLAN_BITMAP_SIZE);
    /* Figure out which trunks were added, removed, and which didn't change. */
    bitmap_and(common_trunks, removed_trunks, VLAN_BITMAP_SIZE);
    bitmap_not(removed_trunks, VLAN_BITMAP_SIZE);
    bitmap_or(removed_trunks, common_trunks, VLAN_BITMAP_SIZE);
    bitmap_not(removed_trunks, VLAN_BITMAP_SIZE);
    bitmap_not(added_trunks, VLAN_BITMAP_SIZE);
    bitmap_or(added_trunks, common_trunks, VLAN_BITMAP_SIZE);
    bitmap_not(added_trunks, VLAN_BITMAP_SIZE);
    /* Native vlan resembles trunks behavior, but takes higher priority.
     * If native vlan is the same as one of trunks, then we should handle
     * native vlan and not this trunk. */
    if (bundle->vlan_mode == PORT_VLAN_NATIVE_UNTAGGED ||
        bundle->vlan_mode == PORT_VLAN_NATIVE_TAGGED) {
        bitmap_set0(removed_trunks, bundle->vlan);
        if (s->trunks && bitmap_is_set(s->trunks, bundle->vlan)) {
            bitmap_set1(added_trunks, bundle->vlan);
        }
    }
    if (s->vlan_mode == PORT_VLAN_NATIVE_UNTAGGED ||
        s->vlan_mode == PORT_VLAN_NATIVE_TAGGED) {
        bitmap_set0(added_trunks, s->vlan);
    }

    /* Remove all ports from deleted vlans. */
    switch (bundle->vlan_mode) {
    case PORT_VLAN_ACCESS:
        if (tag_changed || mod_changed) {
            LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
                status = ops_sai_vlan_access_port_del(bundle->vlan,
                                                      netdev_sai_hw_id_get(port->up.netdev));
                ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
            }
        }
        break;

    case PORT_VLAN_TRUNK:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            status = ops_sai_vlan_trunks_port_del(removed_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
        }
        break;

    case PORT_VLAN_NATIVE_UNTAGGED:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (tag_changed || mod_changed) {
                status = ops_sai_vlan_access_port_del(bundle->vlan,
                                                      netdev_sai_hw_id_get(port->up.netdev));
                ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
            }
            status = ops_sai_vlan_trunks_port_del(removed_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
        }
        break;

    case PORT_VLAN_NATIVE_TAGGED:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (tag_changed || mod_changed) {
                status = __native_tagged_vlan_set(bundle->vlan,
                                                  netdev_sai_hw_id_get(port->up.netdev),
                                                  false);
                ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
            }
            status = ops_sai_vlan_trunks_port_del(removed_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to remove reconfigure vlans");
        }
        break;

    default:
        ovs_assert(false);
    }

    /* Add all ports to new vlans. */
    switch (s->vlan_mode) {
    case PORT_VLAN_ACCESS:
        if (tag_changed || mod_changed) {
            LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
                status = ops_sai_vlan_access_port_add(s->vlan,
                                                      netdev_sai_hw_id_get(port->up.netdev));
                ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");
            }
        }
        break;

    case PORT_VLAN_TRUNK:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            status = ops_sai_vlan_trunks_port_add(added_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");
        }
        break;

    case PORT_VLAN_NATIVE_UNTAGGED:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (tag_changed || mod_changed) {
                status = ops_sai_vlan_access_port_add(s->vlan,
                                                      netdev_sai_hw_id_get(port->up.netdev));
                ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");
            }
            status = ops_sai_vlan_trunks_port_add(added_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");
        }
        break;

    case PORT_VLAN_NATIVE_TAGGED:
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (tag_changed || mod_changed) {
                status = __native_tagged_vlan_set(s->vlan,
                                                  netdev_sai_hw_id_get(port->up.netdev),
                                                  true);
                ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");

                status = ops_sai_port_pvid_untag_enable_set(netdev_sai_hw_id_get(port->up.netdev), false);
                ERRNO_LOG_EXIT(status, "Failed to reconfigure untag enable");
            }
            status = ops_sai_vlan_trunks_port_add(added_trunks,
                                                  netdev_sai_hw_id_get(port->up.netdev));
            ERRNO_LOG_EXIT(status, "Failed to reconfigure vlans");
        }
        break;

    default:
        ovs_assert(false);
    }

    /* flush fdb entry */
    if(tag_changed || mod_changed) {
        if(bundle->lag_info.is_lag) {
            sai_mac_learning_l2_addr_flush_by_tid(bundle->lag_info.lag_id);
        }else{
            LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
                sai_mac_learning_l2_addr_flush_by_port(netdev_get_name(port->up.netdev));
            }
        }
    }else if(!bitmap_equal(removed_trunks, empty_trunks, VLAN_BITMAP_SIZE)){
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            sai_mac_learning_l2_addr_flush_by_port(netdev_get_name(port->up.netdev));
        }
    }

    bundle->vlan = s->vlan;
    bundle->vlan_mode = s->vlan_mode;
    __trunks_realloc(bundle, s->trunks);

exit:
    return status;
}

static int
__ofbundle_ports_reconfigure(struct ofbundle_sai *bundle,
                               const struct ofproto_bundle_settings *s)
{
    size_t i;
    bool port_found = false;
    int status = 0;
    struct ofport_sai *port = NULL, *next_port = NULL, *s_port = NULL;
#if 0
    if(-1 != bundle->bond_hw_handle)
    {
        status = __lag_tx_member_reconfigure(bundle, s, 0);
        ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");
    }
#endif
    /* Figure out which ports were removed. */
    LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
        port_found = false;
        for (i = 0; i < s->n_slaves; i++) {
            s_port = __get_ofp_port(bundle->ofproto, s->slaves[i]);
            if (port == s_port) {
                port_found = true;
                break;
            }
        }
        if (!port_found) {
            status = __ofbundle_port_del(port);
            ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");
        }
    }

    status = __vlan_reconfigure(bundle, s);
    ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");

    /* Figure out which ports were added. */
    port = NULL;
    next_port = NULL;
    for (i = 0; i < s->n_slaves; i++) {
        port_found = false;
        s_port = __get_ofp_port(bundle->ofproto, s->slaves[i]);

        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (port == s_port) {
                port_found = true;
                break;
            }
        }

        if (!port_found) {
            status = __ofbundle_port_add(bundle, s_port);
        }
        ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");
    }
#if 0
    if(-1 != bundle->bond_hw_handle)
    {
        status = __lag_tx_member_reconfigure(bundle, s, 1);
        ERRNO_LOG_EXIT(status, "Failed to reconfigure ports");
    }
#endif
exit:
    return status;
}

static int
__ofbundle_intf_mac_reconfigure(struct ofport_sai *port, bool is_create)
{
    int         status = 0;
    char        sys_cmd[256];
    char        *mac_addr = NULL;
    struct      eth_addr mac;
    const struct    ether_addr *p_mac = (void *)mac.ea;

    memset(sys_cmd, 0, sizeof(sys_cmd));
    if (!is_create) {
        netdev_sai_get_etheraddr(port->up.netdev, &mac);
    } else {
        ops_sai_vendor_base_mac_get(mac.ea);
    }

    mac_addr = ether_ntoa(p_mac);
    snprintf(sys_cmd, sizeof(sys_cmd), "ip netns exec swns ip link set address %s dev %s",
            mac_addr, port->up.netdev->name);

    system(sys_cmd);

    return status;
}

static int
__ofbundle_lag_intf_mac_reconfigure(const char* port_name)
{
    int         status = 0;
    char        sys_cmd[256];
    char        *mac_addr = NULL;
    struct      eth_addr mac;
    const struct    ether_addr *p_mac = (void *)mac.ea;

    memset(sys_cmd, 0, sizeof(sys_cmd));

    ops_sai_vendor_base_mac_get(mac.ea);

    mac_addr = ether_ntoa(p_mac);
    snprintf(sys_cmd, sizeof(sys_cmd), "ip netns exec swns ip link set address %s dev %s",
            mac_addr, port_name);

    system(sys_cmd);

    return status;
}

/*
 * Reconfigures router interface.
 *
 * @param[in] bundle - Current configuration set on HW
 * @param[in] s      - New configuration that should be applied
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__ofbundle_router_intf_reconfigure(struct ofbundle_sai *bundle,
                                   const struct ofproto_bundle_settings *s)
{
    int status = 0;
    handle_t handle = HANDLE_INITIALIZAER;
    enum router_intf_type rif_type;
    const char *netdev_type = NULL;
    struct ofport_sai *port = NULL;
    struct ofport_sai *next_port = NULL;
    struct ofproto_sai *ofproto = NULL;

    ovs_assert(bundle != NULL);

    ofproto = bundle->ofproto;

    ovs_assert(STR_EQ(ofproto->up.type, SAI_INTERFACE_TYPE_VRF));

    if(strncmp(s->name,"lag",3) == 0) {
	    netdev_type = OVSREC_INTERFACE_TYPE_SYSTEM;
    }else{
           port = __get_ofp_port(ofproto, s->slaves[0]);
	    if (!port) {
	        status = -1;
	        ERRNO_LOG_EXIT(status, "Failed to get port (slave: %u)",
	                       s->slaves[0]);
	    }

	    netdev_type = netdev_get_type(port->up.netdev);
    }

    ovs_assert(netdev_type != NULL);

    if (STR_EQ(netdev_type, OVSREC_INTERFACE_TYPE_INTERNAL)) {
        rif_type = ROUTER_INTF_TYPE_VLAN;
        handle.data = s->vlan;
    } else if (STR_EQ(netdev_type, OVSREC_INTERFACE_TYPE_LOOPBACK)){
        bundle->router_intf.is_loopback = true;
        goto exit;
    } else if (STR_EQ(netdev_type, OVSREC_INTERFACE_TYPE_VLANSUBINT)) {
        /* Currently subinterface are not supported. */
        goto exit;
    } else {
        rif_type = ROUTER_INTF_TYPE_PORT;

	if(bundle->lag_info.is_lag) {
	    handle = bundle->lag_info.lag_hw_handle;
       }else{
           handle.data = ops_sai_api_port_map_get_oid(netdev_sai_hw_id_get(port->up.netdev));
	}
    }

    if (bundle->router_intf.created &&
            !HANDLE_EQ(&bundle->router_intf.handle, &handle)) {
        status = __ofbundle_router_intf_remove(bundle);
        ERRNO_EXIT(status);
    }

    if (!bundle->router_intf.created) {
        status = ops_sai_router_intf_create(&ofproto->vrid, rif_type, &handle,
                                            NULL, 0,
                                            &bundle->router_intf.rifid);
        ERRNO_EXIT(status);
        bundle->router_intf.created = true;
        bundle->router_intf.handle = handle;
        bundle->router_intf.enabled = false;

        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            status = netdev_sai_set_router_intf_handle(port->up.netdev,
                                                       &bundle->router_intf.rifid);
            __ofbundle_intf_mac_reconfigure(port, true);
            ERRNO_EXIT(status);
        }

        if(strncmp(s->name,"lag",3) == 0) {
            __ofbundle_lag_intf_mac_reconfigure(s->name);
        }

	 if (STR_EQ(netdev_type, OVSREC_INTERFACE_TYPE_SYSTEM)) {
//		 ops_sai_fdb_flush_entrys(1 /*L2MAC_FLUSH_BY_PORT*/, handle,0);
		 if(bundle->lag_info.is_lag) {
			sai_mac_learning_l2_addr_flush_by_tid(bundle->lag_info.lag_id);
		 }else{
		       port = __get_ofp_port(ofproto, s->slaves[0]);
			sai_mac_learning_l2_addr_flush_by_port(netdev_get_name(port->up.netdev));
		 }
	 }
    }

    if (bundle->router_intf.created &&
            bundle->router_intf.enabled != s->enable) {
        status = ops_sai_router_intf_set_state(&bundle->router_intf.rifid,
                                               s->enable);
        ERRNO_EXIT(status);
        bundle->router_intf.enabled = s->enable;
    }

exit:
    return status;
}

/*
 * Removes router interface configuration.
 *
 * @param[in] bundle - Current configuration set on HW
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofbundle_router_intf_remove(struct ofbundle_sai *bundle)
{
    int status = 0;
    struct ip_address *addr = NULL;
    struct ip_address *next = NULL;
    struct ofproto_sai *ofproto = NULL;
    struct ofport_sai *port = NULL;
    struct ofport_sai *next_port = NULL;
    struct neigbor_entry *neigh = NULL;
    struct neigbor_entry *next_neigh = NULL;

    ovs_assert(bundle != NULL);
    ovs_assert(bundle->ofproto != NULL);

    ofproto = bundle->ofproto;

    /* Remove all existing local routes before interface deletion */
    HMAP_FOR_EACH_SAFE (addr, next, addr_node, &bundle->local_routes) {
        status = ops_sai_route_remove(&ofproto->vrid, addr->address);
        ERRNO_EXIT(status);

        hmap_remove(&bundle->local_routes, &addr->addr_node);
        free(addr->address);
        free(addr);
    }

    HMAP_FOR_EACH_SAFE(neigh, next_neigh, neigh_node, &bundle->neighbors) {
        if (0 != strnlen(neigh->mac_address, MAC_STR_LEN)) {
            status = ops_sai_neighbor_remove(addr_is_ipv6(neigh->ip_address),
                                             neigh->ip_address,
                                             &bundle->router_intf.rifid);
            ERRNO_EXIT(status);

            hmap_remove(&bundle->neighbors, &neigh->neigh_node);

            free(neigh->ip_address);
            free(neigh->mac_address);
            free(neigh);
        }
    }

    if (bundle->router_intf.created) {
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            __ofbundle_intf_mac_reconfigure(port, false);
            status = netdev_sai_set_router_intf_handle(port->up.netdev,
                                                       NULL);
            ERRNO_EXIT(status);
        }

        status = ops_sai_router_intf_remove(&bundle->router_intf.rifid);
        ERRNO_EXIT(status);

        memset(&bundle->router_intf, 0, sizeof(bundle->router_intf));		/* bundle->router_intf.created = False */
    }

exit:
    return status;
}

/*
 * Reconfigure interface IP addresses. Remove from HW deleted entries.
 * Add new entries.
 *
 * @param[in] bundle - Current configuration set on HW
 * @param[in] s      - New configuration that should be applied
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofbundle_ip_reconfigure(struct ofbundle_sai *bundle,
                                     const struct ofproto_bundle_settings *s)
{
    int status = 0;
    struct ofproto *ofproto = NULL;
    struct port         *port_      = NULL;

    ovs_assert(bundle != NULL);
    ovs_assert(bundle->ofproto != NULL);

    ofproto = &bundle->ofproto->up;

    port_ = (struct port *)bundle->aux;

    if(strncmp(bundle->name,"lag",3) == 0) {
        if(bundle->ipv4_primary == NULL && s->ip4_address == NULL) {
            if(port_->cfg->ip4_address) {

                /* Add new */
                status = __ofproto_ip_add(ofproto, port_->cfg->ip4_address, false);
                ERRNO_EXIT(status);

                bundle->ipv4_primary = xstrdup(port_->cfg->ip4_address);
            }
        }
    }

    /* If primary ipv4 got added/deleted/modified */
    if (s->ip_change & PORT_PRIMARY_IPv4_CHANGED) {
        if (!s->ip4_address ||
                (bundle->ipv4_primary &&
                        !STR_EQ(bundle->ipv4_primary, s->ip4_address))) {
            if (bundle->ipv4_primary) {
                status = __ofproto_ip_remove(ofproto,
                                               bundle->ipv4_primary,
                                               false);
                ERRNO_EXIT(status);

                free(bundle->ipv4_primary);
                bundle->ipv4_primary = NULL;
            }
        }

        if (s->ip4_address) {
            /* Add new */
            status = __ofproto_ip_add(ofproto, s->ip4_address, false);
            ERRNO_EXIT(status);

            bundle->ipv4_primary = xstrdup(s->ip4_address);
        }
    }

    /* If primary ipv6 got added/deleted/modified */
    if (s->ip_change & PORT_PRIMARY_IPv6_CHANGED) {
        if (!s->ip6_address ||
                (bundle->ipv6_primary &&
                        !STR_EQ(bundle->ipv6_primary, s->ip6_address))) {
            if (bundle->ipv6_primary) {
                status = __ofproto_ip_remove(ofproto,
                                               bundle->ipv6_primary,
                                               true);
                ERRNO_EXIT(status);

                free(bundle->ipv6_primary);
                bundle->ipv6_primary = NULL;
            }
        }

        if (s->ip6_address) {
            /* Add new */
            status = __ofproto_ip_add(ofproto, s->ip6_address, true);
            ERRNO_EXIT(status);

            bundle->ipv6_primary = xstrdup(s->ip6_address);
        }
    }

    if (s->ip_change & PORT_SECONDARY_IPv4_CHANGED) {
        status = __ofbundle_ip_secondary_reconfigure(bundle, s, false);
        ERRNO_EXIT(status);
    }

    if (s->ip_change & PORT_SECONDARY_IPv6_CHANGED) {
        status = __ofbundle_ip_secondary_reconfigure(bundle, s, true);
        ERRNO_EXIT(status);
    }

exit:
    return status;
}

/*
 * Remove interface IP addresses.
 *
 * @param[in] bundle - Current configuration set on HW
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofbundle_ip_remove(struct ofbundle_sai *bundle)
{
    int status = 0;
    struct ofproto *ofproto;
    struct ip_address *addr = NULL;
    struct ip_address *next = NULL;

    ofproto = &bundle->ofproto->up;

    /* Unconfigure primary ipv4 address and free */
    if (bundle->ipv4_primary) {
        status = __ofproto_ip_remove(ofproto, bundle->ipv4_primary, false);
        ERRNO_EXIT(status);

        free(bundle->ipv4_primary);
    }

    /* Unconfigure primary ipv6 address and free */
    if (bundle->ipv6_primary) {
        status = __ofproto_ip_remove(ofproto, bundle->ipv6_primary, true);
        ERRNO_EXIT(status);

        free(bundle->ipv6_primary);
    }

    /* Unconfigure secondary ipv4 address and free the hash */
    HMAP_FOR_EACH_SAFE (addr, next, addr_node, &bundle->ipv4_secondary) {
        status = __ofproto_ip_remove(ofproto, addr->address, false);
        ERRNO_EXIT(status);

        hmap_remove(&bundle->ipv4_secondary, &addr->addr_node);
        free(addr->address);
        free(addr);
    }

    /* Unconfigure secondary ipv6 address and free the hash */
    HMAP_FOR_EACH_SAFE (addr, next, addr_node, &bundle->ipv6_secondary) {
        status = __ofproto_ip_remove(ofproto, addr->address, true);
        ERRNO_EXIT(status);

        hmap_remove(&bundle->ipv6_secondary, &addr->addr_node);
        free(addr->address);
        free(addr);
    }

exit:
    return status;
}

/*
 * Reconfigure interface secondary IP addresses. Remove from HW all deleted
 * entries. Add new entries.
 *
 * @param[in] bundle  - Current configuration set on HW
 * @param[in] s       - New configuration that should be applied
 * @param[in] is_ipv6 - Indicates whether IPv4 or IPv6 addresses should
 *                      be reconfigured
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofbundle_ip_secondary_reconfigure(struct ofbundle_sai *bundle,
                                               const struct
                                               ofproto_bundle_settings *s,
                                               bool is_ipv6)
{
    int status = 0;
    struct ofproto *ofproto = NULL;
    struct shash new_ip_hash_map;
    struct ip_address *addr = NULL;
    struct ip_address *next = NULL;
    struct shash_node *addr_node = NULL;
    const char *address = NULL;
    int i = 0;
    struct hmap *bundle_ip_addresses = NULL;
    size_t new_ip_num = 0;
    char **new_ip_list = NULL;

    ovs_assert(bundle != NULL);
    ovs_assert(bundle->ofproto != NULL);

    ofproto = &bundle->ofproto->up;

    if (is_ipv6) {
        bundle_ip_addresses = &bundle->ipv6_secondary;
        new_ip_num = s->n_ip6_address_secondary;
        new_ip_list = s->ip6_address_secondary;
    } else {
        bundle_ip_addresses = &bundle->ipv4_secondary;
        new_ip_num = s->n_ip4_address_secondary;
        new_ip_list = s->ip4_address_secondary;
    }

    shash_init(&new_ip_hash_map);

    /* Create hash of the current secondary ip's */
    for (i = 0; i < new_ip_num; i++) {
       if(!shash_add_once(&new_ip_hash_map, new_ip_list[i],
               new_ip_list[i])) {
            VLOG_WARN("Duplicate address in secondary list %s\n",
                      new_ip_list[i]);
        }
    }

    /* Delete all removed */
    HMAP_FOR_EACH_SAFE (addr, next, addr_node, bundle_ip_addresses) {
        if (!shash_find_data(&new_ip_hash_map, addr->address)) {
            status = __ofproto_ip_remove(ofproto, addr->address, false);
            ERRNO_EXIT(status);

            hmap_remove(bundle_ip_addresses, &addr->addr_node);
            free(addr->address);
            free(addr);
        }
    }

    /* Add the newly added addresses to the list */
    SHASH_FOR_EACH (addr_node, &new_ip_hash_map) {
        address = addr_node->data;
        if (!__ofbundle_ip_secondary_find(bundle, address)) {
            status = __ofproto_ip_add(ofproto, address, false);
            ERRNO_EXIT(status);

            addr = xzalloc(sizeof *addr);
            addr->address = xstrdup(address);
            hmap_insert(bundle_ip_addresses, &addr->addr_node,
                        hash_string(addr->address, 0));
        }
    }

exit:
    return status;
}

/*
 * Check if IP address exists in cache.
 *
 * @param[in] bundle  - Current configuration set on HW
 * @param[in] address - Searching IP address
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static struct ip_address *__ofbundle_ip_secondary_find(struct
                                                       ofbundle_sai *bundle,
                                                       const char *address)
{
    struct ip_address *addr = NULL;
    struct hmap *bundle_ip_addresses = NULL;

    if (addr_is_ipv6(address)) {
        bundle_ip_addresses = &bundle->ipv6_secondary;
    } else {
        bundle_ip_addresses = &bundle->ipv4_secondary;
    }

    HMAP_FOR_EACH_WITH_HASH (addr, addr_node, hash_string(address, 0),
                             bundle_ip_addresses) {
        if (STR_EQ(addr->address, address)) {
            return addr;
        }
    }

    return NULL;
}

/*
 * Add IP address to interface.
 *
 * @param[in] ofproto_  - Pointer to ofproto structure.
 * @param[in] ip        - IP address
 * @param[in] is_ipv6   - Indicates if address is IPv4 or IPv6
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofproto_ip_add(struct ofproto *ofproto_,
                            const char *ip,
                            bool is_ipv6)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    char *ptr = NULL;
    /* IP address length + strlen(/128) */
    char prefix[INET6_ADDRSTRLEN + 5] = { };

    VLOG_INFO("Adding IP address %s", ip);

    strcpy(prefix, ip);
    ptr = strchr(prefix, '/');
    ovs_assert(ptr);

    sprintf(ptr, "/%d", is_ipv6 ? 128 : 32);

    return ops_sai_route_ip_to_me_add(&ofproto->vrid, prefix);
}

/*
 * Remove IP address from interface.
 *
 * @param[in] ofproto_  - Pointer to ofproto structure.
 * @param[in] ip        - IP address
 * @param[in] is_ipv6   - Indicates if address is IPv4 or IPv6
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int __ofproto_ip_remove(struct ofproto *ofproto_,
                                 const char *ip,
                                 bool is_ipv6)
{
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    char *ptr = NULL;
    char prefix[INET6_ADDRSTRLEN + 5]; /* IP address length + strlen(/128) */

    VLOG_INFO("Removing IP address %s", ip);

    strcpy(prefix, ip);
    ptr = strchr(prefix, '/');
    ovs_assert(ptr);

    sprintf(ptr, "/%d", is_ipv6 ? 128 : 32);

    return ops_sai_route_ip_to_me_delete(&ofproto->vrid, prefix);
}

/*
 * Rename bundle or delete name if new name is NULL.
 */
static void
__ofbundle_rename(struct ofbundle_sai *bundle, const char *name)
{
    /* Name didn't change. */
    if ((NULL != bundle->name) && (NULL != name)
        && !strcmp(name, bundle->name)) {
        return;
    }
    /* Free old name if there was any. */
    if (NULL != bundle->name) {
        free(bundle->name);
        bundle->name = NULL;
    }
    /* Copy new name if there is any. */
    if (NULL != name) {
        bundle->name = xstrdup(name);
    }
}

/*
 * Create new bundle and perform basic initialization.
 */
static struct ofbundle_sai *
__ofbundle_create(struct ofproto_sai *ofproto, void *aux,
                    const struct ofproto_bundle_settings *s)
{
    struct ofbundle_sai *bundle = xzalloc(sizeof (struct ofbundle_sai));

    hmap_insert(&ofproto->bundles, &bundle->hmap_node, hash_pointer(aux, 0));
    list_init(&bundle->ports);
    __ofbundle_rename(bundle, s->name);
    __trunks_realloc(bundle, s->trunks);

    bundle->ofproto = ofproto;
    bundle->aux = aux;

    bundle->ipv4_primary = NULL;
    bundle->ipv6_primary = NULL;
    hmap_init(&bundle->ipv4_secondary);
    hmap_init(&bundle->ipv6_secondary);
    hmap_init(&bundle->local_routes);
    hmap_init(&bundle->neighbors);

    __bundle_cache_init(bundle);

    bundle->lag_info.is_lag = false;
    bundle->lag_info.lag_id = -1;
    bundle->lag_info.lag_hw_handle = invalid_handle;
    list_init(&bundle->lag_info.tx_number_ports);

    list_init(&bundle->ingress_node);
    list_init(&bundle->egress_node);

    return bundle;
}

/*
 * Destroy bundle and remove ports from it.
 */
static void
__ofbundle_destroy(struct ofbundle_sai *bundle, bool delete_config_only)
{
    int status = 0;
    struct ofport_sai *port = NULL, *next_port = NULL;
    struct ofmirror_sai	*mir = NULL, *n_mir = NULL;
    struct ofproto_sai  *ofproto_sai = NULL;
    sai_mirror_porttype_t   porttype;
    sai_mirror_portid_t     portid;

    if (NULL == bundle) {
        return;
    }

    if (!bundle->config_cache.cache_config) {
        status = __ofbundle_ip_remove(bundle);
        ERRNO_LOG(status,
                  "Failed to remove bundle IP addresses (bundle: %s)",
                   bundle->name);

        status = __ofbundle_router_intf_remove(bundle);
        ERRNO_LOG(status,
                  "Failed to remove router interface configuration (bundle: %s)",
                  bundle->name);

		if (bundle->ingress_owner) {
		    __mirror_get_bundle_port_info(bundle, &porttype, &portid);
		    if (list_size(&bundle->ports)) {
		        ops_sai_mirror_src_del(bundle->ingress_owner->hid,
		                SAI_MIRROR_DATA_DIR_INGRESS, porttype, portid);
		    }

		    bundle->ingress_owner = NULL;
		    list_remove(&bundle->ingress_node);
		    list_init(&bundle->ingress_node);
		}

		if (bundle->egress_owner) {
		     __mirror_get_bundle_port_info(bundle, &porttype, &portid);
		    if (list_size(&bundle->ports)) {
		        ops_sai_mirror_src_del(bundle->egress_owner->hid,
		                SAI_MIRROR_DATA_DIR_EGRESS, porttype, portid);
		    }

		    bundle->egress_owner = NULL;
		    list_remove(&bundle->egress_node);
		    list_init(&bundle->egress_node);
		}

		HMAP_FOR_EACH(ofproto_sai, all_ofproto_sai_node, &all_ofproto_sai) {
		    HMAP_FOR_EACH_SAFE(mir, n_mir, hmap_node, &ofproto_sai->mirrors) {
		        if (mir->out->aux == bundle->aux) {
		            __ofmirror_destroy(mir, 1);
		        }
		    }
		}
#if 0
		if (bundle->bond_hw_handle != -1) {
		 LIST_FOR_EACH_SAFE(port, next_port, bundle_tx_lag_node, &bundle->tx_number_ports) {
		        status = __ofbundle_lag_tx_port_del(port);
		        ERRNO_LOG(status,
		                  "Failed to remove bundle tx port configuration (bundle: %s)",
		                  bundle->name);
		    }
		}
#endif
		LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
		    status = __ofbundle_port_del(port);
		    ERRNO_LOG(status,
		              "Failed to remove bundle port configuration (bundle: %s)",
		              bundle->name);
		}

		if (bundle->lag_info.is_lag) {
		    sai_mac_learning_l2_addr_flush_by_tid(bundle->lag_info.lag_id);
//		    ops_sai_fdb_flush_entrys(1 /*L2MAC_FLUSH_BY_PORT*/, bundle->lag_info.lag_hw_handle,bundle->vlan);
		    __ofproto_lag_destroy(bundle);
		}
	}

    if (!delete_config_only) {
        __ofbundle_rename(bundle, NULL);
        __trunks_realloc(bundle, NULL);
        hmap_destroy(&bundle->ipv4_secondary);
        hmap_destroy(&bundle->ipv6_secondary);
        hmap_destroy(&bundle->local_routes);
        hmap_destroy(&bundle->neighbors);
        hmap_remove(&bundle->ofproto->bundles, &bundle->hmap_node);

        __bundle_cache_free(bundle);

        free(bundle);
    }
}

/*
 * Find bundle by aux.
 */
static struct ofbundle_sai *
__ofbundle_lookup(struct ofproto_sai *ofproto, void *aux)
{
    struct ofbundle_sai *bundle = NULL;

    ovs_assert(ofproto);
    ovs_assert(aux);

    HMAP_FOR_EACH_IN_BUCKET(bundle, hmap_node, hash_pointer(aux, 0),
                            &ofproto->bundles) {
        if (bundle->aux == aux) {
            return bundle;
        }
    }

    return NULL;
}

static struct ofbundle_sai *
__ofbundle_lookup_by_netdev_name(struct ofproto_sai *ofproto,
                                   const char *name)
{
    struct ofport_sai *port = NULL;
    struct ofbundle_sai *bundle = NULL;

    ovs_assert(ofproto);
    ovs_assert(name);

    if(strncmp(name, "lag", 3) == 0) {
        HMAP_FOR_EACH(bundle, hmap_node, &ofproto->bundles) {
            if(STR_EQ(bundle->name, name)) {
                 return bundle;
             }
         }
    } else {
        HMAP_FOR_EACH(bundle, hmap_node, &ofproto->bundles) {
            LIST_FOR_EACH(port, bundle_node, &bundle->ports) {
                if (STR_EQ(netdev_get_name(port->up.netdev), name)) {
                    return bundle;
                }
            }
        }
    }

    return NULL;
}

static void
__ofproto_lag_port_update (int lag_id, struct ofport_sai *port, bool add)
{
    int rc = 0;

    VLOG_INFO("__ofproto_lag_port_update: lag %d port %s add %d",
                    lag_id, netdev_get_name(port->up.netdev), add);

    if (add) {
	 rc = ops_sai_lag_member_port_add(lag_id, netdev_sai_hw_id_get(port->up.netdev));
    } else {
        rc = ops_sai_lag_member_port_del(lag_id, netdev_sai_hw_id_get(port->up.netdev));
    }

    if (rc) {
        VLOG_ERR("__ofproto_lag_port_update failed for lag %d port %s add %d : rc %d",
                    lag_id, netdev_get_name(port->up.netdev), add, rc);
    }
}

static void __ofproto_lag_create(struct ofbundle_sai *bundle)
{
    struct port         *port_              = NULL;
    struct ofport_sai *port = NULL, *next_port = NULL;

    ops_sai_lag_create(&bundle->lag_info.lag_id);

    if(bundle->lag_info.lag_id == -1){
        VLOG_ERR("__ofproto_lag_create: create lag error; name = %s",bundle->name);
	 return ;
    }

    bundle->lag_info.is_lag = true;
    ops_sai_lag_get_handle_id(bundle->lag_info.lag_id, &bundle->lag_info.lag_hw_handle);

    port_ = (struct port *)bundle->aux;
    port_->bond_hw_handle = bundle->lag_info.lag_id;

    LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
        __ofproto_lag_port_update(bundle->lag_info.lag_id, port, true /*add*/);
    }
}

static void __ofproto_lag_destroy(struct ofbundle_sai *bundle)
{
    struct port         *port_              = NULL;
    struct ofport_sai *port = NULL, *next_port = NULL;

    LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
        __ofproto_lag_port_update(bundle->lag_info.lag_id, port, false /*del*/);
    }

    ops_sai_lag_remove(bundle->lag_info.lag_id);

    bundle->lag_info.is_lag = false;
    bundle->lag_info.lag_id = 1;

    port_ = (struct port *)bundle->aux;
    port_->bond_hw_handle = -1;
}


static int
__ofbundle_lag_reconfigure(struct ofbundle_sai *bundle,
                               const struct ofproto_bundle_settings *s)
{
    int                 status      = 0;

    /* lag interface always hw_bond_should_exist = true */
    if(s->hw_bond_should_exist && bundle->lag_info.lag_id == -1){
        __ofproto_lag_create(bundle);
    }

    return status;
}

/*
 * Apply bundle settings.
 */
static int
__bundle_set(struct ofproto *ofproto_, void *aux,
             const struct ofproto_bundle_settings *s)
{
    struct ofbundle_sai *bundle = NULL;
    int status = 0;
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);

    SAI_API_TRACE_FN();

    if (STR_EQ(ofproto_->type, SAI_TYPE_IACL)) {
        VLOG_DBG("iACL container bundle set placeholder (%s bundle)",s == NULL ? "destroy":"create");
        goto exit;
    }

    if ((s && STR_EQ(s->name, DEFAULT_BRIDGE_NAME))) {
        goto exit;
    }

    bundle = __ofbundle_lookup(ofproto, aux);
    if (NULL == s) {
        __ofbundle_destroy(bundle, false);
        goto exit;
    }

    if (NULL == bundle) {
        bundle = __ofbundle_create(ofproto, aux, s);
        if (!__is_bundle_active(ofproto_, NULL, s)) {
            bundle->config_cache.cache_config = true;
        }
    }

#if 0
    if (s->n_slaves > 1) {
        status = -1;
        ERRNO_LOG_EXIT(status, "LAGs are not implemented");
    }
#endif

    if (bundle->config_cache.cache_config) {
        status = 0;
        goto exit;
    }

    status = __ofbundle_lag_reconfigure(bundle, s);
    ERRNO_LOG_EXIT(status, "Failed to set bundle lag");

    status = __ofbundle_ports_reconfigure(bundle, s);
    ERRNO_LOG_EXIT(status, "Failed to reconfigure ports (bundle_name: %s)",
                   s->name);

    if (STR_EQ(ofproto_->type, SAI_INTERFACE_TYPE_VRF)) {
        status = __ofbundle_router_intf_reconfigure(bundle, s);
        ERRNO_LOG_EXIT(status, "Failed to reconfigure router interfaces "
                       "(bundle_name: %s)",
                       s->name);
        status = __ofbundle_ip_reconfigure(bundle, s);
        ERRNO_LOG_EXIT(status, "Failed to reconfigure ip addresses "
                       "(bundle_name: %s)",
                       s->name);
    }

exit:
    if (bundle && s) {
        __bundle_setting_free(bundle);
        __bundle_setting_copy(bundle, s);
    }
    return status;
}

static void
__bundle_remove(struct ofport *port_)
{
    int status = 0;
    struct ofport_sai *port = __ofport_sai_cast(port_);
    struct ofbundle_sai *bundle = port->bundle;

    SAI_API_TRACE_FN();

    if (NULL == bundle) {
        return;
    }
#if 0
    if (bundle->bond_hw_handle != -1) {
        status = __ofbundle_lag_tx_port_del(port);
        ERRNO_LOG(status,"Failed to remove tx bundle");
    }
#endif
    status = __ofbundle_port_del(port);
    ERRNO_LOG_EXIT(status, "Failed to remove bundle");

exit:
    if (list_is_empty(&bundle->ports)) {
        __ofbundle_destroy(bundle, false);
    }
}

static int
__bundle_get(struct ofproto *ofproto_, void *aux, int *bundle_handle)
{
    SAI_API_TRACE_FN();

    return 0;
}

/**
 * Verify if bundle HW lane is active.
 *
 * @param[in] bundle bundle.
 * @param[in] s bundle configuration.
 *
 * @return true if bundle is active else false.
 */
static bool
__is_bundle_active(struct ofproto *ofproto_,
                   const struct ofbundle_sai *bundle,
                   const struct ofproto_bundle_settings *s)
{
    bool lane_state = false;
    struct ofport_sai *port = NULL, *next_port = NULL;
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);

    if (bundle) {
        if (list_is_empty(&bundle->ports)) {
            return false;
        }

        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            if (netdev_sai_get_lane_state(port->up.netdev, &lane_state) ||
                    !lane_state) {
                return false;
            }

        }

        return lane_state;
    } else if (s) {
        /* fix lag check member port */
        if(STRN_EQ(s->name,"lag",strlen("lag"))){
            return true;
        }

        port = __get_ofp_port(ofproto, *s->slaves);

        if (!port) {
            return false;
        }

        if (netdev_sai_get_lane_state(port->up.netdev, &lane_state)) {
            return false;
        }

        return lane_state;
    }

    return false;
}

/**
 * Make a deep copy of bundle settings.
 *
 * @param[in] bundle bundle into which configuration to be copied.
 * @param[in] s configuration that should be copied.
 *
 * @return 0, sai status converted to errno otherwise.
 */
static void
__bundle_setting_copy(struct ofbundle_sai *bundle,
                      const struct ofproto_bundle_settings *s)
{
    int i = 0;
    struct ofproto_bundle_settings *copy = NULL;

    NULL_PARAM_LOG_ABORT(bundle);
    NULL_PARAM_LOG_ABORT(s);
    ovs_assert(!bundle->config_cache.config);

    copy = xzalloc(sizeof(*s));

    copy->name = xstrdup(s->name);

    if (s->slaves) {
        copy->slaves = xzalloc(sizeof(*s->slaves) * s->n_slaves);
        memcpy(copy->slaves, s->slaves, sizeof(*s->slaves) * s->n_slaves);
    }
    copy->n_slaves = s->n_slaves;

    copy->vlan_mode = s->vlan_mode;
    copy->vlan = s->vlan;
    copy->trunks = vlan_bitmap_clone(s->trunks);
    copy->use_priority_tags = s->use_priority_tags;

    if (s->bond) {
        copy->bond = xzalloc(sizeof(*s->bond));
        memcpy(copy->bond, s->bond, sizeof(*s->bond));
    }

    if (s->lacp) {
        copy->lacp = xzalloc(sizeof(*s->lacp));
        memcpy(copy->lacp, s->lacp, sizeof(*s->lacp));
    }

    if (s->lacp_slaves) {
        copy->lacp_slaves = xzalloc(sizeof(*s->lacp_slaves) * s->n_slaves);
        memcpy(copy->lacp_slaves, s->lacp_slaves,
               sizeof(*s->lacp_slaves) * s->n_slaves);
    }

    copy->realdev_ofp_port = s->realdev_ofp_port;

    for (i = 0; i < PORT_OPT_MAX; ++i) {
        /* It is safe to copy pointers they will be actual
         * during bundle whole life circle */
        copy->port_options[i] = s->port_options[i];
    }

    copy->hw_bond_should_exist = s->hw_bond_should_exist;

    copy->bond_handle_alloc_only = s->bond_handle_alloc_only;

    if (s->slaves_tx_enable) {
        copy->slaves_tx_enable = xzalloc(sizeof(*s->slaves_tx_enable) * s->n_slaves_tx_enable);
        memcpy(copy->slaves_tx_enable,
               s->slaves_tx_enable,
               sizeof(*s->slaves_tx_enable) * s->n_slaves_tx_enable);
    }
    copy->n_slaves_tx_enable = s->n_slaves_tx_enable;

    copy->slaves_entered = s->slaves_entered;
    copy->ip_change = s->ip_change;

    if (s->ip4_address) {
        copy->ip4_address = xstrdup(s->ip4_address);
    }

    if (s->ip6_address) {
        copy->ip6_address = xstrdup(s->ip6_address);
    }

    if (s->n_ip4_address_secondary) {
        copy->ip4_address_secondary =xzalloc(sizeof(*s->ip4_address_secondary) * s->n_ip4_address_secondary);
        for (i = 0; i < s->n_ip4_address_secondary; ++i) {
            copy->ip4_address_secondary[i] = xstrdup(s->ip4_address_secondary[i]);
        }
    }
    copy->n_ip4_address_secondary = s->n_ip4_address_secondary;

    if (s->n_ip6_address_secondary) {
        copy->ip6_address_secondary = xzalloc(sizeof(*s->ip6_address_secondary) * s->n_ip6_address_secondary);
        for (i = 0; i < s->n_ip6_address_secondary; ++i) {
            copy->ip6_address_secondary[i] = xstrdup(s->ip6_address_secondary[i]);
        }
    }
    copy->n_ip6_address_secondary = s->n_ip6_address_secondary;

    copy->enable = s->enable;

    bundle->config_cache.config = copy;
}

/**
 * Free cached bundle settings resources.
 *
 * @param[in] bundle bundle which stores settings resources
 *
 * @return 0, sai status converted to errno otherwise.
 */
static void
__bundle_setting_free(struct ofbundle_sai *bundle)
{
    int i = 0;
    struct ofproto_bundle_settings *s = NULL;

    if (!bundle || ! bundle->config_cache.config) {
        return;
    }

    s = bundle->config_cache.config;

    free(s->name);
    free(s->slaves);
    bitmap_free(s->trunks);
    free(s->bond);
    free(s->lacp);
    free(s->lacp_slaves);

    free(s->slaves_tx_enable);
    free(s->ip4_address);
    free(s->ip6_address);

    if (s->n_ip4_address_secondary) {
        for (i = 0; i < s->n_ip4_address_secondary; ++i) {
            free(s->ip4_address_secondary[i]);
        }
    }

    if (s->n_ip6_address_secondary) {
        for (i = 0; i < s->n_ip6_address_secondary; ++i) {
            free(s->ip6_address_secondary[i]);
        }
    }

    free(s);
    bundle->config_cache.config = NULL;
}

static void __bundle_cache_init(struct ofbundle_sai *bundle)
{
    hmap_init(&bundle->config_cache.local_routes);
}

static void __bundle_cache_free(struct ofbundle_sai *bundle)
{
    struct ip_address *addr = NULL;
    struct ip_address *next = NULL;

    ovs_assert(bundle);

    __bundle_setting_free(bundle);

    HMAP_FOR_EACH_SAFE (addr, next, addr_node, &bundle->local_routes) {
        hmap_remove(&bundle->local_routes, &addr->addr_node);
        free(addr->address);
        free(addr);
    }

    hmap_destroy(&bundle->config_cache.local_routes);
}

static int
__set_vlan(struct ofproto *ofproto, int vid, bool add)
{
    SAI_API_TRACE_FN();

    return ops_sai_vlan_set(vid, add);
}

static inline struct ofproto_sai_group *
__ofproto_sai_group_cast(const struct ofgroup *group)
{
    return group ? CONTAINER_OF(group, struct ofproto_sai_group, up) : NULL;
}

static struct ofgroup *
__group_alloc(void)
{
    struct ofproto_sai_group *group = xzalloc(sizeof *group);

    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return &group->up;
}

static enum ofperr
__group_construct(struct ofgroup *group_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static void
__group_destruct(struct ofgroup *group_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return;
}

static void
__group_dealloc(struct ofgroup *group_)
{
    struct ofproto_sai_group *group = __ofproto_sai_group_cast(group_);

    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    free(group);
}

static enum ofperr
__group_modify(struct ofgroup *group_)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static enum ofperr
__group_get_stats(const struct ofgroup *group_,
                  struct ofputil_group_stats *ogs)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();

    return 0;
}

static const char *
__get_datapath_version(const struct ofproto *ofproto_ OVS_UNUSED)
{
    SAI_API_TRACE_FN();

    return strdup(SAI_DATAPATH_VERSION);
}

static struct neigbor_entry*
__neigh_entry_hash_find(const char *ip_addr,
                        const struct ofbundle_sai *bundle)
{
    struct neigbor_entry* neigh_entry = NULL;

    ovs_assert(ip_addr);
    ovs_assert(bundle);

    HMAP_FOR_EACH_WITH_HASH(neigh_entry, neigh_node, hash_string(ip_addr, 0),
                            &bundle->neighbors) {
        if (STR_EQ(neigh_entry->ip_address, ip_addr)) {
            return neigh_entry;
        }
    }

    return NULL;
}

static void
__neigh_entry_hash_add(const char *mac_address,
                       const char *ip_addr,
                       struct ofbundle_sai *bundle)
{
    struct neigbor_entry* neigh_entry = NULL;

    ovs_assert(mac_address);
    ovs_assert(ip_addr);
    ovs_assert(bundle);

    if (NULL == __neigh_entry_hash_find(ip_addr, bundle)) {
        neigh_entry = xzalloc(sizeof *neigh_entry);
        neigh_entry->mac_address = xstrdup(mac_address);
        neigh_entry->ip_address  = xstrdup(ip_addr);

        hmap_insert(&bundle->neighbors, &neigh_entry->neigh_node,
                    hash_string(neigh_entry->ip_address, 0));
    }
}

static void
__neigh_entry_hash_remove(const char *ip_addr,
                          struct ofbundle_sai *bundle)
{
    struct neigbor_entry* neigh_entry = NULL;

    ovs_assert(ip_addr);
    ovs_assert(bundle);

    neigh_entry = __neigh_entry_hash_find(ip_addr, bundle);
    if (NULL != neigh_entry) {
        hmap_remove(&bundle->neighbors, &neigh_entry->neigh_node);
        free(neigh_entry->ip_address);
        free(neigh_entry->mac_address);
        free(neigh_entry);
    }
}

static int
__add_l3_host_entry(const struct ofproto *ofproto_, void *aux,
                    bool is_ipv6_addr, char *ip_addr,
                    char *next_hop_mac_addr, int *l3_egress_id)
{
    int status = 0;
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    struct ofbundle_sai *bundle = __ofbundle_lookup(ofproto, aux);
    struct neigbor_entry *neigh = NULL;

    SAI_API_TRACE_FN();

    ovs_assert(bundle);
    ovs_assert(ip_addr);
    ovs_assert(next_hop_mac_addr);

    if (bundle->config_cache.cache_config) {
        status = 0;
        goto exit;
    }

    ovs_assert(bundle->router_intf.created);

    neigh = __neigh_entry_hash_find(ip_addr, bundle);

    if (NULL != neigh && STR_EQ(neigh->mac_address, next_hop_mac_addr)) {
        VLOG_WARN("Not adding neighbor entry as it was already added"
                  "(ip address: %s, MAC: %s rifid: %lu)",
                  ip_addr, next_hop_mac_addr,
                  bundle->router_intf.rifid.data);
    } else {
        if (0 == strnlen(next_hop_mac_addr, MAC_STR_LEN)) {
            VLOG_WARN("Received neighbor entry with empty MAC address."
                      "(ip address: %s, rifid: %lu). Don't passing it to asic",
                      ip_addr, bundle->router_intf.rifid.data);
        } else {
            if (NULL != neigh &&
                0 == strnlen(neigh->mac_address,
                             MAC_STR_LEN)) {
                __neigh_entry_hash_remove(ip_addr, bundle);
            }
            status = ops_sai_neighbor_create(is_ipv6_addr,
                                             ip_addr,
                                             next_hop_mac_addr,
                                             &bundle->router_intf.rifid);
            ERRNO_EXIT(status);
        }
        __neigh_entry_hash_add(next_hop_mac_addr, ip_addr, bundle);
        *l3_egress_id = 1;
    }

    exit:
    SAI_API_TRACE_EXIT_FN();
    return status;
}

static int
__delete_l3_host_entry(const struct ofproto *ofproto_, void *aux,
                       bool is_ipv6_addr, char *ip_addr,
                       int *l3_egress_id)
{
    int status = 0;
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    struct ofbundle_sai *bundle = __ofbundle_lookup(ofproto, aux);
    struct neigbor_entry *neigh = NULL;

    SAI_API_TRACE_FN();

    ovs_assert(bundle);
    ovs_assert(ip_addr);

    if (bundle->config_cache.cache_config) {
        status = 0;
        goto exit;
    }

    ovs_assert(bundle->router_intf.created);

    neigh = __neigh_entry_hash_find(ip_addr, bundle);

    if (NULL != neigh){
        if (0 != strnlen(neigh->mac_address, MAC_STR_LEN)) {
            status = ops_sai_neighbor_remove(is_ipv6_addr,
                                             ip_addr,
                                             &bundle->router_intf.rifid);
            ERRNO_EXIT(status);
        }
        __neigh_entry_hash_remove(ip_addr, bundle);
        *l3_egress_id = -1;
    }

exit:
    SAI_API_TRACE_EXIT_FN();
    return status;
}

static int
__get_l3_host_hit_bit(const struct ofproto *ofproto_, void *aux,
                                bool is_ipv6_addr, char *ip_addr,
                                bool *hit_bit)
{
    int status = 0;
    struct ofproto_sai *ofproto = ofproto_sai_cast(ofproto_);
    struct ofbundle_sai *bundle = __ofbundle_lookup(ofproto, aux);
    struct neigbor_entry *neigh = NULL;

    SAI_API_TRACE_FN();

    ovs_assert(ip_addr);
    ovs_assert(hit_bit);

     neigh = __neigh_entry_hash_find(ip_addr, bundle);

    if (NULL != neigh) {
        if (0 == strnlen(neigh->mac_address, MAC_STR_LEN)) {
            VLOG_INFO("Not getting neighbor activity for entry with "
                      "empty MAC address(ip address: %s, rif: %lu)",
                      ip_addr, bundle->router_intf.rifid.data);
            *hit_bit = false;
        } else {
            status = ops_sai_neighbor_activity_get(is_ipv6_addr,
                                                   ip_addr,
                                                   &bundle->router_intf.rifid,
                                                   hit_bit);
            ERRNO_EXIT(status);
            }
        } else {
            *hit_bit = false;
            VLOG_WARN("Not getting neighbor activity for non-existing entry"
                      "(ip address: %s, rif: %lu)",
                      ip_addr, bundle->router_intf.rifid.data);
            }

exit:
    SAI_API_TRACE_EXIT_FN();
    return status;
}

static int
__l3_route_action(const struct ofproto *ofprotop,
                            enum ofproto_route_action action,
                            struct ofproto_route *routep)
{
    int          status     = 0;
    uint32_t     rnh_count  = 0;
    struct ofproto_sai *sai_ofproto = ofproto_sai_cast(ofprotop);
    char *next_hops[routep->n_nexthops];
    uint32_t lnh_count = 0;
    char *egress_intf[routep->n_nexthops];
    struct ofbundle_sai *bundle = NULL;
    struct ip_address *addr = NULL;

    SAI_API_TRACE_FN();

    for (uint32_t index=0; index < routep->n_nexthops; index ++) {
        struct ofproto_route_nexthop *nh = &(routep->nexthops[index]);

        switch (nh->type) {
        case OFPROTO_NH_IPADDR:
            next_hops[rnh_count++]   = nh->id;
            break;
        case OFPROTO_NH_PORT:
            egress_intf[lnh_count++] = nh->id;
            break;
        default:
            status = -1;
            ERRNO_LOG_EXIT(status, "Unknown ofproto next hope type: %d",
                                   routep->nexthops[index].type);
        }
    }

    if (rnh_count) {
        ovs_assert(lnh_count == 0);

        switch (action) {
        case OFPROTO_ROUTE_ADD:
            status = ops_sai_route_remote_add(sai_ofproto->vrid,
                                              routep->prefix,
                                              rnh_count,
                                              next_hops);
            break;
        case OFPROTO_ROUTE_DELETE_NH:
            status = ops_sai_route_remote_nh_remove(sai_ofproto->vrid,
                                                    routep->prefix,
                                                    rnh_count,
                                                    next_hops);
            break;
        case OFPROTO_ROUTE_DELETE:
            status = ops_sai_route_remove(&sai_ofproto->vrid,
                                          routep->prefix);
            break;
        default:
            status = -1;
            ERRNO_LOG_EXIT(status, "Unknown ofproto action %d", action);
        }
    } else if (lnh_count) {
        ovs_assert(rnh_count == 0);
        ovs_assert(lnh_count == 1);

        bundle = __ofbundle_lookup_by_netdev_name(sai_ofproto,
                                                  egress_intf[0]);
        if (bundle && bundle->router_intf.is_loopback) {
            goto exit;
        }

        switch (action) {
        case OFPROTO_ROUTE_ADD:
            ovs_assert(bundle);
            ovs_assert(bundle->router_intf.created);

            if (!bundle->config_cache.cache_config) {
                status = ops_sai_route_local_add(&sai_ofproto->vrid,
                                                 routep->prefix,
                                                 &bundle->router_intf.rifid);
            }

            __l3_local_route_attach_to_bundle(bundle, routep->prefix);

            break;
        case OFPROTO_ROUTE_DELETE:
        case OFPROTO_ROUTE_DELETE_NH:
            /* Bundle is already removed and all local routes are cleared */
            if (!bundle) {
                break;
            }

            if (!bundle->config_cache.cache_config) {
                HMAP_FOR_EACH_WITH_HASH (addr, addr_node,
                        hash_string(routep->prefix, 0),
                        &bundle->local_routes) {
                    if (STR_EQ(addr->address, routep->prefix)) {
                        status = ops_sai_route_remove(&sai_ofproto->vrid,
                                                      routep->prefix);
                        ERRNO_EXIT(status);
                        break;
                    }
                }
            }

            __l3_local_route_dettach_from_bundle(bundle, routep->prefix);

            break;
        default:
            status = -1;
            ERRNO_LOG_EXIT(status, "Unknown ofproto action %d", action);
        }
    }

exit:
    SAI_API_TRACE_EXIT_FN();
    return status;
}

static void
__l3_local_route_attach_to_bundle(struct ofbundle_sai *bundle, char *prefix)
{
    struct ip_address *addr = NULL;

    if (!bundle->config_cache.cache_config) {
        /* Add local route to bundle */
        addr = xzalloc(sizeof *addr);
        addr->address = xstrdup(prefix);
        hmap_insert(&bundle->local_routes, &addr->addr_node,
                    hash_string(addr->address, 0));
    }

    /* Add local route to bundle config cache */
    addr = xzalloc(sizeof *addr);
    addr->address = xstrdup(prefix);
    hmap_insert(&bundle->config_cache.local_routes, &addr->addr_node,
                    hash_string(addr->address, 0));
}

static void
__l3_local_route_dettach_from_bundle(struct ofbundle_sai *bundle, char * prefix)
{
    struct ip_address *addr = NULL;

    if (!bundle->config_cache.cache_config) {
        HMAP_FOR_EACH_WITH_HASH (addr, addr_node,
                                 hash_string(prefix, 0),
                                 &bundle->local_routes) {
            if (STR_EQ(addr->address, prefix)) {
                hmap_remove(&bundle->local_routes, &addr->addr_node);
                free(addr->address);
                free(addr);
                break;
            }
        }
    }

    HMAP_FOR_EACH_WITH_HASH (addr, addr_node,
                             hash_string(prefix, 0),
                             &bundle->config_cache.local_routes) {
        if (STR_EQ(addr->address, prefix)) {
            hmap_remove(&bundle->config_cache.local_routes, &addr->addr_node);
            free(addr->address);
            free(addr);
            break;
        }
    }
}

static int
__l3_ecmp_set(const struct ofproto *ofprotop, bool enable)
{
    int error = 0;

    if (!enable) {
        VLOG_ERR("Disabling ECMP is not supported");
        return EOPNOTSUPP;
    }

    return error;
}

static int
__l3_ecmp_hash_set(const struct ofproto *ofprotop, unsigned int hash,
                             bool enable)
{
    return ops_sai_ecmp_hash_set(hash, enable);
}

static int
__run(struct ofproto *ofproto)
{
    SAI_API_TRACE_FN();

    return 0;
}

static void
__wait(struct ofproto *ofproto)
{
    SAI_API_TRACE_FN();
}

static void
__set_tables_version(struct ofproto *ofproto, cls_version_t version)
{
    SAI_API_TRACE_FN();

    return;
}

static void
OVS_UNUSED __ofproto_bundle_settings_dump(const struct ofproto_bundle_settings *s)
{
    int i = 0;
    int n = 0;
    char buff[VLAN_BITMAP_SIZE * 4];
    char *buff_ptr = buff;

    if (!s) {
        VLOG_INFO("Bundle settings: NULL");
        return;
    }

    buff[0] = '\0';
    if (s->trunks) {
        for(i = 0; i < VLAN_BITMAP_SIZE; ++i) {
            if (bitmap_is_set(s->trunks, i)) {
                n = sprintf(buff_ptr, "%d,:%s", i, (i % 10) ? "" : "\n");
                buff_ptr += n;
            }
        }
    }

    VLOG_INFO("Bundle settings:\n"
              "\tname: %s\n"
              "\tstate: %d\n"
              "\tVLAN mode: %d\n"
              "\tVLAN: %d\n"
              "\tTrunks: %s\n"
#ifdef OPS
              "\tIP change: %d\n"
              "\tIPv4 address: %s\n"
              "\tIPv6 address: %s\n"
#endif
            "",
              s->name, s->enable, s->vlan_mode, s->vlan, buff, s->ip_change,
              s->ip_change & PORT_PRIMARY_IPv4_CHANGED ? s->ip4_address : "",
              s->ip_change & PORT_PRIMARY_IPv6_CHANGED ? s->ip6_address: "");

#ifdef OPS
    if (s->ip_change & PORT_SECONDARY_IPv4_CHANGED) {
        for (i = 0; i < s->n_ip4_address_secondary; ++i) {
            VLOG_INFO("\tIPv4 secondary address: %s",
                      s->ip4_address_secondary[i]);
        }
    }

    if (s->ip_change & PORT_SECONDARY_IPv6_CHANGED) {
        for (i = 0; i < s->n_ip6_address_secondary; ++i) {
            VLOG_INFO("\tIPv6 secondary address: %s",
                      s->ip6_address_secondary[i]);
        }
    }
#endif
}

int
ofbundle_get_port_name_by_handle_id(handle_t    port_id,
                                                 char        *str)
{
    struct ofproto_sai  *ofproto_sai  = NULL;
    struct ofbundle_sai *ofbundle_sai = NULL;

    HMAP_FOR_EACH(ofproto_sai, all_ofproto_sai_node, &all_ofproto_sai) {
        if (STR_EQ(ofproto_sai->up.type,SAI_INTERFACE_TYPE_SYSTEM))
        {
            HMAP_FOR_EACH(ofbundle_sai, hmap_node, &ofproto_sai->bundles) {
                if(!ofbundle_sai->lag_info.is_lag)
                    continue;

                if(HANDLE_EQ(&ofbundle_sai->lag_info.lag_hw_handle,&port_id))
                {
                    strcpy(str,ofbundle_sai->name);
                    return 0;
                }

            }
        }
    }

    return -1;
}

int
ofbundle_get_handle_id_by_tid(const int tid, handle_t *handle_id)
{
    struct ofproto_sai  *ofproto_sai  = NULL;
    struct ofbundle_sai *ofbundle_sai = NULL;

    ovs_assert(handle_id);

    HMAP_FOR_EACH(ofproto_sai, all_ofproto_sai_node, &all_ofproto_sai) {
        if (STR_EQ(ofproto_sai->up.type,SAI_INTERFACE_TYPE_SYSTEM))
        {
            HMAP_FOR_EACH(ofbundle_sai, hmap_node, &ofproto_sai->bundles) {
                if(!ofbundle_sai->lag_info.is_lag)
                    continue;

                if(ofbundle_sai->lag_info.lag_id == tid)
                {
		      *handle_id = ofbundle_sai->lag_info.lag_hw_handle;
                    return true;
                }
            }
        }
    }

    return false;
}

/*
 * returns true if the specified bundle is a lag/bond/trunk/ether channel
 */
static bool
bundle_is_a_lag (struct ofbundle_sai *bundle)
{
    return bundle->lag_info.is_lag;
}

/*
 * obtains the hw_unit & hw_port numbers of a bundle.  If the bundle is
 * a lag/trunk, then sets the hw_unit to 0 and hw_port to trunk_id.
 */
static void
__mirror_get_bundle_port_info(struct ofbundle_sai *bundle,
        sai_mirror_porttype_t *pt, sai_mirror_portid_t *portid)
{
    const struct ofport_sai *port, *next_port;

    VLOG_DBG("bundle_get_hw_info called for bundle %s (0x%p)",
            bundle->name, bundle);

    /* port is a lag */
    if (bundle_is_a_lag(bundle)) {
        if (pt) *pt = SAI_MIRROR_PORT_LAG;
        (*portid).lag_id = bundle->lag_info.lag_id;
        VLOG_DBG("bundle %s (0x%p) *IS* a lag (lagid %d)",
                bundle->name, bundle, bundle->lag_info.lag_id);
        return;
    }

    if (pt) *pt = SAI_MIRROR_PORT_PHYSICAL;
    /* it should have ONE and ONLY ONE entry */
    if (list_size(&bundle->ports) == 1) {
        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            (*portid).hw_id = netdev_sai_hw_id_get(port->up.netdev);
            break;
        }
    } else {
        VLOG_ERR("port list size is %d for bundle %s",
                (int) list_size(&bundle->ports), bundle->name);
        (*portid).hw_id = -1;
    }
}

/*
 * Create new mirror and perform basic initialization.
 */
static struct ofmirror_sai *
__ofmirror_create(struct ofproto_sai *ofproto, void *aux,
                    const struct ofproto_mirror_settings *s)
{
    struct ofproto_mirror_bundle {
        struct ofproto *ofproto;
        void *aux;
    } *mout;
    struct ofmirror_sai *mirror;

    mirror = xzalloc(sizeof (struct ofmirror_sai));
    if (!mirror)
        return NULL;

    hmap_insert(&ofproto->mirrors, &mirror->hmap_node, hash_pointer(aux, 0));
    mirror->ofproto = ofproto;
    mirror->aux = aux;

    list_init(&mirror->ingress_srcs);
    list_init(&mirror->egress_srcs);
    memset(mirror->name, 0, sizeof mirror->name);
    strncpy(mirror->name, s->name, sizeof mirror->name - 1);

    /* out_bundle is a pointer to a buffer containing a *ofproto,*aux tuple */
    mout = (struct ofproto_mirror_bundle *)(s->out_bundle);
    mirror->out = __ofbundle_lookup(
            ofproto_sai_cast(mout->ofproto), mout->aux);

    VLOG_INFO("n_srcs %d, n_dsts %d, out_vlan %u",
            (int) s->n_srcs, (int) s->n_dsts, s->out_vlan);

    __mirror_get_bundle_port_info(mirror->out,
            &mirror->out_type, &mirror->out_portid);

    /* Initialize port stats info */
    __mirror_get_stats(&ofproto->up, aux,
            &mirror->tx_base_packets, &mirror->tx_base_bytes);

    ops_sai_mirror_create(&mirror->hid, mirror->out_type, mirror->out_portid);

    return mirror;
}

/*
 * Destroy mirror and remove bundles from it.
 */
static void
__ofmirror_destroy(struct ofmirror_sai *mirror, int all)
{
    struct ofbundle_sai *bd = NULL, *next_bd = NULL;
    sai_mirror_porttype_t   porttype;
    sai_mirror_portid_t     portid;

    if (NULL == mirror)
        return;

    LIST_FOR_EACH_SAFE(bd, next_bd, ingress_node, &mirror->ingress_srcs) {
        __mirror_get_bundle_port_info(bd, &porttype, &portid);

        /*
         * This condition actually applies for bond bundle only.
         *
         * When all member ports of a lag be removed, the lag is actually
         * destroyed in chip layer. Such that using the "valid" bond id
         * will get an invalid lag object id, which caused error in sai layer
         *
         * other hand, when you removed all member ports from a lag,
         * then all the ports' mirror attribute had been cleared, so there
         * is no sense to call mirror_src_del here
         */
        if (list_size(&bd->ports)) {
            ops_sai_mirror_src_del(mirror->hid,
                    SAI_MIRROR_DATA_DIR_INGRESS, porttype, portid);
        }

        bd->ingress_owner = NULL;
        list_remove(&bd->ingress_node);
        list_init(&bd->ingress_node);
    }

    LIST_FOR_EACH_SAFE(bd, next_bd, egress_node, &mirror->egress_srcs) {
        __mirror_get_bundle_port_info(bd, &porttype, &portid);

        if (list_size(&bd->ports)) {
            ops_sai_mirror_src_del(mirror->hid,
                    SAI_MIRROR_DATA_DIR_EGRESS, porttype, portid);
        }

        bd->egress_owner = NULL;
        list_remove(&bd->egress_node);
        list_init(&bd->egress_node);
    }

    if (all) {
        ops_sai_mirror_destroy(mirror->hid);
        hmap_remove(&mirror->ofproto->mirrors, &mirror->hmap_node);
        free(mirror);
    }
}

/*
 * Find mirror by aux.
 */
static struct ofmirror_sai *
__ofmirror_lookup(struct ofproto_sai *ofproto, void *aux)
{
    struct ofmirror_sai *mirror = NULL;

    ovs_assert(ofproto);
    ovs_assert(aux);

    HMAP_FOR_EACH_IN_BUCKET(mirror, hmap_node, hash_pointer(aux, 0),
                            &ofproto->mirrors) {
        if (mirror->aux == aux) {
            return mirror;
        }
    }

    return NULL;
}

static int
__mirror_set(struct ofproto *ofproto_,
        void *aux,
        const struct ofproto_mirror_settings *s)
{
    /*
     * To allow bundles to be in any instance of a bridge/VRF,
     * both the ofproto pointer and aux values are given
     */
    struct ofproto_mirror_bundle {
        struct ofproto *ofproto;
        void *aux;
    } *msrcs, *mdsts, *mout;

    struct ofproto_sai *ofproto;
    struct ofmirror_sai *mirror;
    struct ofbundle_sai *t;
    sai_mirror_porttype_t   porttype;
    sai_mirror_portid_t     portid;
    int i;

    VLOG_INFO("__mirror_set called, aux = %p, s = %p", aux, s);

    ofproto = ofproto_sai_cast(ofproto_);
    mirror = __ofmirror_lookup(ofproto, aux);
    if (!s) {
        VLOG_INFO("mirror destroy all");
        __ofmirror_destroy(mirror, 1);
        return 0;
    }

    if (!mirror) {
        VLOG_INFO("mirror not exist, create it");
        mirror = __ofmirror_create(ofproto, aux, s);
    } else {
        mout = (struct ofproto_mirror_bundle *)(s->out_bundle);
        t = __ofbundle_lookup(ofproto_sai_cast(mout->ofproto), mout->aux);
        if (mirror->out != t) {
            __ofmirror_destroy(mirror, 1);
            mirror = __ofmirror_create(ofproto, aux, s);
        } else
            __ofmirror_destroy(mirror, 0);
    }

    /* srcs is a pointer to an array of N *ofproto,*aux tuples */
    msrcs = (struct ofproto_mirror_bundle *)(s->srcs);
    for (i = 0 ; i < s->n_srcs; i++) {
        ofproto = ofproto_sai_cast(msrcs[i].ofproto);
        t = __ofbundle_lookup(ofproto, msrcs[i].aux);

        if (list_is_empty(&t->ingress_node)) {
            list_insert(&mirror->ingress_srcs, &t->ingress_node);
            t->ingress_owner = mirror;
        } else if (t->ingress_owner != mirror)
            return EBUSY;

        __mirror_get_bundle_port_info(t, &porttype, &portid);
        VLOG_INFO("ingress: bundle %p, porttype: %d, portid: %d",
                t, porttype, portid.hw_id);
        ops_sai_mirror_src_add(mirror->hid,
                SAI_MIRROR_DATA_DIR_INGRESS, porttype, portid);
    }

    /* dsts is a pointer to an array of N *ofproto,*aux tuples */
    mdsts = (struct ofproto_mirror_bundle *)(s->dsts);
    for (i = 0; i < s->n_dsts ; i++) {
        ofproto = ofproto_sai_cast(mdsts[i].ofproto);
        t = __ofbundle_lookup(ofproto, mdsts[i].aux);

        if (list_is_empty(&t->egress_node)) {
            list_insert(&mirror->egress_srcs, &t->egress_node);
            t->egress_owner = mirror;
        } else if (t->egress_owner != mirror)
            return EBUSY;

        __mirror_get_bundle_port_info(t, &porttype, &portid);
        VLOG_INFO("egress: bundle %p, porttype: %d, portid: %d",
                t, porttype, portid.hw_id);
        ops_sai_mirror_src_add(mirror->hid,
                SAI_MIRROR_DATA_DIR_EGRESS, porttype, portid);

    }

    return 0;
}

static int
__mirror_get_stats(struct ofproto *ofproto,
        void *aux,
        uint64_t *packets,
        uint64_t *bytes)
{
    SAI_API_TRACE_FN();

    struct ofmirror_sai *mirror;
    struct netdev_stats stats;
    const struct ofport_sai *port;
    uint32_t		hw_id;

    mirror = __ofmirror_lookup(ofproto_sai_cast(ofproto), aux);
    if (!mirror) goto err;

    /*
     * Since sai-port->stats_get() function accepts only physical port
     * id (not accept lag id), we should extract physical port's id from
     * lag manually, we just need to get all member ports' stats
     */
    *packets = *bytes = 0;
    if (bundle_is_a_lag(mirror->out)) {
        LIST_FOR_EACH(port, bundle_node, &mirror->out->ports) {
            hw_id = netdev_sai_hw_id_get(port->up.netdev);
            if (ops_sai_port_stats_get(hw_id, &stats))
                goto err;

            *packets += stats.tx_packets;
            *bytes += stats.tx_bytes;
        }
    } else {
        hw_id = mirror->out_portid.hw_id;
        if (ops_sai_port_stats_get(hw_id, &stats))
            goto err;

        *packets = stats.tx_packets;
        *bytes = stats.tx_bytes;
    }

    *packets -= mirror->tx_base_packets;
    *bytes -= mirror->tx_base_bytes;

    return 0;

err:
    *packets = *bytes = -1;
    return -1;
}

static bool
__is_mirror_output_bundle(const struct ofproto *ofproto, void *aux)
{
    VLOG_DBG("is_mirror_output_bundle called");
    struct ofmirror_sai     *mirror;
    struct ofproto_sai      *ofproto_;

    ofproto_ = ofproto_sai_cast(ofproto);
    HMAP_FOR_EACH(mirror, hmap_node, &ofproto_->mirrors) {
        if (mirror->out->aux == aux)
            return true;
    }
    return false;
}

static int stp_default_id = -1;
static int
__ofproto_stp_init()
{
    ops_sai_stp_get_default_instance(&stp_default_id);

    if (-1 == stp_default_id) {
        VLOG_ERR("stp create error for default stgid");
        return -1;
    }

    VLOG_INFO("stp created for default stg-id %d", stp_default_id);

    return 0;
}

static int
__ofproto_stp_create(int *stpid)
{
    VLOG_DBG("%s: create stg entry called", __FUNCTION__);

    return ops_sai_stp_create(stpid);
}

static int
__ofproto_stp_remove(int stgid)
{
    VLOG_DBG("%s: entry, stg=%d", __FUNCTION__, stgid);

    return ops_sai_stp_remove(stgid);
}

static int
__ofproto_stp_add_vlan(int stgid, int vid)
{
    VLOG_DBG("%s: entry, stg=%d, vid=%d", __FUNCTION__, stgid,vid);

    return ops_sai_stp_add_vlan(stgid, vid);
}

static int
__ofproto_stp_remove_vlan(int stgid, int vid)
{
    VLOG_DBG("%s: entry, stg=%d, vid=%d", __FUNCTION__, stgid,vid);

    return ops_sai_stp_remove_vlan(stgid, vid);
}

static int
__ofproto_stp_set_port_state(char *port_name,
                         int  stgid,
                         int  port_state,
                         bool port_stp_set)
{
    uint32_t hw_id = 0;
    int stp_id = stgid;

    VLOG_DBG("%s: entry, stg=%d, port_state=%d, port_stp_set=%d", __FUNCTION__, stgid,port_state,port_stp_set);

    if (false == netdev_sai_get_hw_id_by_name(port_name, &hw_id)) {
        VLOG_ERR("%s: unable to find netdev for port %s", __FUNCTION__,
                 port_name);
        return -1;
    }
#if 0
    if(port_stp_set)
    {
        stp_id = stp_default_id;
    }
#endif
    return ops_sai_stp_set_port_state(stp_id,hw_id,port_state);
}

static int
__ofproto_stp_get_port_state(char *port_name,
                         int  stgid,
                         int  *port_state)
{
    uint32_t hw_id = 0;
    int stp_id = stgid;

    VLOG_DBG("%s: entry, stg=%d", __FUNCTION__, stgid);

    if (false == netdev_sai_get_hw_id_by_name(port_name, &hw_id)) {
        VLOG_ERR("%s: unable to find netdev for port %s", __FUNCTION__,
                 port_name);
        return -1;
    }

    return ops_sai_stp_get_port_state(stp_id,hw_id,port_state);
}

static int
__ofproto_stp_get_default(int *stgid)
{
    return ops_sai_stp_get_default_instance(stgid);
}


static struct asic_plugin_interface __sai_interface = {
    .create_stg         = __ofproto_stp_create,
    .delete_stg         = __ofproto_stp_remove,
    .add_stg_vlan       = __ofproto_stp_add_vlan,
    .remove_stg_vlan    = __ofproto_stp_remove_vlan,
    .set_stg_port_state = __ofproto_stp_set_port_state,
    .get_stg_port_state = __ofproto_stp_get_port_state,
    .get_stg_default    = __ofproto_stp_get_default,
    .get_mac_learning_hmap = sai_mac_learning_get_hmap,
    .l2_addr_flush         = sai_mac_learning_l2_addr_flush_handler,
};

void __sai_register_stg_mac_learning_plugin_init()
{
    struct plugin_extension_interface sai_extension;

    SAI_API_TRACE_FN();

    sai_extension.plugin_name = ASIC_PLUGIN_INTERFACE_NAME;
    sai_extension.major = ASIC_PLUGIN_INTERFACE_MAJOR;
    sai_extension.minor = ASIC_PLUGIN_INTERFACE_MINOR;
    sai_extension.plugin_interface = (void *)&__sai_interface;

    register_plugin_extension(&sai_extension);
    VLOG_INFO("The %s asic plugin interface was registered", ASIC_PLUGIN_INTERFACE_NAME);
}
