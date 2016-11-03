#include "sai-ofproto-sflow.h"
#include <inttypes.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include "ofproto/collectors.h"
#include "compiler.h"
#include "hash.h"
#include "hmap.h"
#include "netdev.h"
#include "netlink.h"
#include "ofpbuf.h"
#include "ofproto/ofproto.h"
#include "packets.h"
#include "poll-loop.h"
#include "ovs-router.h"
#include "route-table.h"
#include "sflow_api.h"
#include "socket-util.h"
#include "timeval.h"
#include "openvswitch/vlog.h"
#include "ofproto/ofproto-provider.h"
#include "sai-netdev.h"
#include "sai-sflow.h"
#include "sai-port.h"
#include "sai-log.h"
#include "sai-api-class.h"

#include "unixctl.h"

VLOG_DEFINE_THIS_MODULE(sai_ofproto_sflow);

static struct ovs_mutex mutex;

/* This global var is used to determine which sFlow
   sub-agent should send the datapath counters. */
#define SFLOW_GC_SUBID_UNCLAIMED (uint32_t)-1
static uint32_t sflow_global_counters_subid = SFLOW_GC_SUBID_UNCLAIMED;

struct sai_sflow_port {
    struct hmap_node hmap_node; /* In struct dpif_sflow's "ports" hmap. */
    SFLDataSource_instance dsi; /* sFlow library's notion of port number. */
    struct ofport *ofport;      /* To retrive port stats. */
    uint32_t hw_lane_id;
//    enum sai_sflow_tunnel_type tunnel_type;
};

struct sai_sflow {
    struct collectors *collectors;
    SFLAgent *sflow_agent;
    struct ofproto_sflow_options *options;
    time_t next_tick;
    size_t n_flood, n_all;
    struct hmap ports;          /* Contains "struct dpif_sflow_port"s. */
    uint32_t probability;
    struct ovs_refcount ref_cnt;
    handle_t         sflow_id;
};

static void sai_sflow_del_poller(struct sai_sflow *,
                                  struct sai_sflow_port *);

#define RECEIVER_INDEX 1

static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);

struct sai_sflow    *pgsflow = NULL;

static bool
nullable_string_is_equal(const char *a, const char *b)
{
    return a ? b && !strcmp(a, b) : !b;
}

static bool
ofproto_sflow_options_equal(const struct ofproto_sflow_options *a,
                         const struct ofproto_sflow_options *b)
{
    return (sset_equals(&a->targets, &b->targets)
            && a->sampling_rate == b->sampling_rate
            && a->polling_interval == b->polling_interval
            && a->header_len == b->header_len
//            && a->sub_id == b->sub_id
         && a->max_datagram == b->max_datagram
            && nullable_string_is_equal(a->agent_device, b->agent_device)
            && nullable_string_is_equal(a->control_ip, b->control_ip));
}

static struct ofproto_sflow_options *
ofproto_sflow_options_clone(const struct ofproto_sflow_options *old)
{
    struct ofproto_sflow_options *new = xmemdup(old, sizeof *old);
    sset_clone(&new->targets, &old->targets);
    new->agent_device = old->agent_device ? xstrdup(old->agent_device) : NULL;
    new->control_ip = old->control_ip ? xstrdup(old->control_ip) : NULL;
    return new;
}

static void
ofproto_sflow_options_destroy(struct ofproto_sflow_options *options)
{
    if (options) {
        sset_destroy(&options->targets);
        free(options->agent_device);
        free(options->control_ip);
        free(options);
    }
}

/* sFlow library callback to allocate memory. */
static void *
sflow_agent_alloc_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                     size_t bytes)
{
    return xzalloc(bytes);
}

/* sFlow library callback to free memory. */
static int
sflow_agent_free_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                    void *obj)
{
    free(obj);
    return 0;
}

/* sFlow library callback to report error. */
static void
sflow_agent_error_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                     char *msg)
{
    VLOG_WARN("sFlow agent error: %s", msg);
}

/* sFlow library callback to send datagram. */
static void
sflow_agent_send_packet_cb(void *ds_, SFLAgent *agent OVS_UNUSED,
                           SFLReceiver *receiver OVS_UNUSED, u_char *pkt,
                           uint32_t pktLen)
{
    struct sai_sflow *ds = ds_;
    collectors_send(ds->collectors, pkt, pktLen);
}

static struct sai_sflow_port *
sai_sflow_find_port(const struct sai_sflow *ds, uint32_t hw_lane_id)
    OVS_REQUIRES(mutex)
{
    struct sai_sflow_port *dsp;

    HMAP_FOR_EACH_IN_BUCKET (dsp, hmap_node, hash_int(hw_lane_id,0),
                             &ds->ports) {
        if (dsp->hw_lane_id == hw_lane_id) {
            return dsp;
        }
    }
    return NULL;
}

/* If there are multiple bridges defined then we need some
   minimal artibration to decide which one should send the
   global counters.  This function allows each sub-agent to
   ask if he should do it or not. */
static bool
sflow_global_counters_subid_test(uint32_t subid)
    OVS_REQUIRES(mutex)
{
    if (sflow_global_counters_subid == SFLOW_GC_SUBID_UNCLAIMED) {
        /* The role is up for grabs. */
        sflow_global_counters_subid = subid;
    }
    return (sflow_global_counters_subid == subid);
}

static void
sflow_global_counters_subid_clear(uint32_t subid)
    OVS_REQUIRES(mutex)
{
    if (sflow_global_counters_subid == subid) {
        /* The sub-agent that was sending global counters
           is going away, so reset to allow another
           to take over. */
        sflow_global_counters_subid = SFLOW_GC_SUBID_UNCLAIMED;
    }
}

static void
sflow_agent_get_global_counters(void *ds_, SFLPoller *poller,
                                SFL_COUNTERS_SAMPLE_TYPE *cs)
    OVS_REQUIRES(mutex)
{
//    struct dpif_sflow *ds = ds_;
    SFLCounters_sample_element res_elem;
    struct rusage usage;

    if (!sflow_global_counters_subid_test(poller->agent->subId)) {
        /* Another sub-agent is currently responsible for this. */
        return;
    }

    /* resource usage */
    getrusage(RUSAGE_SELF, &usage);
    res_elem.tag = SFLCOUNTERS_APP_RESOURCES;
    res_elem.counterBlock.appResources.user_time
        = timeval_to_msec(&usage.ru_utime);
    res_elem.counterBlock.appResources.system_time
        = timeval_to_msec(&usage.ru_stime);
    res_elem.counterBlock.appResources.mem_used = (usage.ru_maxrss * 1024);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.mem_max);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.fd_open);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.fd_max);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.conn_open);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.conn_max);

    SFLADD_ELEMENT(cs, &res_elem);
    sfl_poller_writeCountersSample(poller, cs);
}

/*
 * Get port statistics.
 *
 * @param[in] hw_id port label id.
 * @param[out] stats pointer to netdev statistics.
 *
 * @return 0, sai status converted to errno otherwise.
 */
static int
__port_stats_get(uint32_t hw_id,  SFLIf_counters *counters)
{
    enum stats_indexes {
    STAT_IDX_IF_IN_OCTETS,
    STAT_IDX_IF_IN_PKTS,
    STAT_IDX_IF_IN_MULTICAST_PKTS,
    STAT_IDX_IF_IN_BROADCAST_PKTS,
    STAT_IDX_IF_IN_ERRORS,

    STAT_IDX_IF_OUT_OCTETS,
    STAT_IDX_IF_OUT_PKTS,
    STAT_IDX_IF_OUT_MULTICAST_PKTS,
    STAT_IDX_IF_OUT_BROADCAST_PKTS,
    STAT_IDX_IF_OUT_ERRORS,

        STAT_IDX_COUNT
    };
    static const sai_port_stat_counter_t counter_ids[] = {
    SAI_PORT_STAT_IF_IN_OCTETS,
    SAI_PORT_STAT_IF_IN_PKTS,
    SAI_PORT_STAT_IF_IN_MULTICAST_PKTS,
    SAI_PORT_STAT_IF_IN_BROADCAST_PKTS,
    SAI_PORT_STAT_IF_IN_ERRORS,

    SAI_PORT_STAT_IF_OUT_OCTETS,
    SAI_PORT_STAT_IF_OUT_PKTS,
    SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS,
    SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS,
    SAI_PORT_STAT_IF_OUT_ERRORS,

    };
    uint64_t port_counters[STAT_IDX_COUNT] = {};
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t port_oid = ops_sai_api_port_map_get_oid(hw_id);
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    ovs_assert(counters);

    status = sai_api->port_api->get_port_stats(port_oid, counter_ids,
                                               STAT_IDX_COUNT, port_counters);
    SAI_ERROR_LOG_EXIT(status, "Failed to get stats for port %d", hw_id);


    counters->ifInOctets = port_counters[STAT_IDX_IF_IN_OCTETS];
    counters->ifInUcastPkts = port_counters[SAI_PORT_STAT_IF_IN_PKTS];
    counters->ifInMulticastPkts = port_counters[STAT_IDX_IF_IN_MULTICAST_PKTS];
    counters->ifInBroadcastPkts = port_counters[STAT_IDX_IF_IN_BROADCAST_PKTS];
    counters->ifInDiscards = -1;
    counters->ifInErrors = port_counters[STAT_IDX_IF_IN_ERRORS];
    counters->ifInUnknownProtos = -1;
    counters->ifOutOctets = port_counters[STAT_IDX_IF_OUT_OCTETS];
    counters->ifOutUcastPkts = port_counters[SAI_PORT_STAT_IF_OUT_PKTS];
    counters->ifOutMulticastPkts = port_counters[STAT_IDX_IF_OUT_MULTICAST_PKTS];
    counters->ifOutBroadcastPkts = port_counters[STAT_IDX_IF_OUT_BROADCAST_PKTS];
    counters->ifOutDiscards = -1;
    counters->ifOutErrors = port_counters[STAT_IDX_IF_OUT_ERRORS];
    counters->ifPromiscuousMode = 0;

exit:
    return SAI_ERROR_2_ERRNO(status);
}

static void
sflow_agent_get_counters(void *ds_, SFLPoller *poller,
                         SFL_COUNTERS_SAMPLE_TYPE *cs)
    OVS_REQUIRES(mutex)
{
    struct sai_sflow *ds = ds_;
    SFLCounters_sample_element elem/*, of_elem, name_elem*/;
    enum netdev_features current;
    struct sai_sflow_port *dsp;
    SFLIf_counters *counters;
//    struct netdev_stats stats;
    enum netdev_flags flags;
//    const char *ifName;

    dsp = sai_sflow_find_port(ds, u32_to_odp(poller->bridgePort));
    if (!dsp) {
        return;
    }

    elem.tag = SFLCOUNTERS_GENERIC;
    counters = &elem.counterBlock.generic;
    counters->ifIndex = SFL_DS_INDEX(poller->dsi);
    counters->ifType = 6;
    if (!netdev_get_features(dsp->ofport->netdev, &current, NULL, NULL, NULL)) {
        /* The values of ifDirection come from MAU MIB (RFC 2668): 0 = unknown,
           1 = full-duplex, 2 = half-duplex, 3 = in, 4=out */
        counters->ifSpeed = netdev_features_to_bps(current, 0);
        counters->ifDirection = (netdev_features_is_full_duplex(current)
                                 ? 1 : 2);
    } else {
        counters->ifSpeed = 100000000;
        counters->ifDirection = 0;
    }
    if (!netdev_get_flags(dsp->ofport->netdev, &flags) && flags & NETDEV_UP) {
        counters->ifStatus = 1; /* ifAdminStatus up. */
        if (netdev_get_carrier(dsp->ofport->netdev)) {
            counters->ifStatus |= 2; /* ifOperStatus us. */
        }
    } else {
        counters->ifStatus = 0;  /* Down. */
    }

    /* XXX
       1. Is the multicast counter filled in?
       2. Does the multicast counter include broadcasts?
       3. Does the rx_packets counter include multicasts/broadcasts?
    */
    __port_stats_get(netdev_sai_hw_id_get(dsp->ofport->netdev), counters);

    SFLADD_ELEMENT(cs, &elem);
#if 0
    /* Include Port name. */
    if ((ifName = netdev_get_name(dsp->ofport->netdev)) != NULL) {
    memset(&name_elem, 0, sizeof name_elem);
    name_elem.tag = SFLCOUNTERS_PORTNAME;
    name_elem.counterBlock.portName.portName.str = (char *)ifName;
    name_elem.counterBlock.portName.portName.len = strlen(ifName);
    SFLADD_ELEMENT(cs, &name_elem);
    }

    /* Include OpenFlow DPID and openflow port number. */
    memset(&of_elem, 0, sizeof of_elem);
    of_elem.tag = SFLCOUNTERS_OPENFLOWPORT;
    of_elem.counterBlock.ofPort.datapath_id =
    ofproto_get_datapath_id(dsp->ofport->ofproto);
    of_elem.counterBlock.ofPort.port_no =
      (OVS_FORCE uint32_t)dsp->ofport->ofp_port;
    SFLADD_ELEMENT(cs, &of_elem);
#endif

    sfl_poller_writeCountersSample(poller, cs);
}

/* Obtains an address to use for the local sFlow agent and stores it into
 * '*agent_addr'.  Returns true if successful, false on failure.
 *
 * The sFlow agent address should be a local IP address that is persistent and
 * reachable over the network, if possible.  The IP address associated with
 * 'agent_device' is used if it has one, and otherwise 'control_ip', the IP
 * address used to talk to the controller.  If the agent device is not
 * specified then it is figured out by taking a look at the routing table based
 * on 'targets'. */
static bool
sflow_choose_agent_address(const char *agent_ip, SFLAddress *agent_addr)
{
    int                 af;
    void                *addr       = NULL;
    struct in_addr      myIP;
    struct in6_addr     myIP6;

    memset(agent_addr, 0, sizeof *agent_addr);

    ovs_assert(agent_ip != NULL);

    if (strchr(agent_ip, ':'))  {
        memset(&myIP6, 0, sizeof myIP6);
        af = AF_INET6;
        agent_addr->type = SFLADDRESSTYPE_IP_V6;
        addr = &myIP6;
    } else {
        memset(&myIP, 0, sizeof myIP);
        af = AF_INET;
        agent_addr->type = SFLADDRESSTYPE_IP_V4;
        addr = &myIP;
    }

    if (inet_pton(af, agent_ip, addr) != 1) {
        /* This error condition should not happen. */
        VLOG_ERR("sFlow Agent device IP is malformed:%s", agent_ip);
        goto error;
    }

    if (agent_addr->type == SFLADDRESSTYPE_IP_V4) {
        agent_addr->address.ip_v4.addr = myIP.s_addr;
    } else {
        memcpy(agent_addr->address.ip_v6.addr, myIP6.s6_addr, 16);
    }

    return true;

error:
    VLOG_ERR("could not determine IP address for sFlow agent");
    return false;
}

static void
sai_sflow_clear__(struct sai_sflow *ds) OVS_REQUIRES(mutex)
{
    if (ds->sflow_agent) {
        sflow_global_counters_subid_clear(ds->sflow_agent->subId);
        sfl_agent_release(ds->sflow_agent);
        free(ds->sflow_agent);
        ds->sflow_agent = NULL;
        ops_sai_sflow_remove(ds->sflow_id);
     ds->sflow_id.data = 0;
    }
    collectors_destroy(ds->collectors);
    ds->collectors = NULL;
    ofproto_sflow_options_destroy(ds->options);
    ds->options = NULL;

    /* Turn off sampling to save CPU cycles. */
    ds->probability = 0;
}

void
sai_sflow_clear(struct sai_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    sai_sflow_clear__(ds);
    ovs_mutex_unlock(&mutex);
}

struct sai_sflow *
sai_sflow_create(void)
{
    static struct ovsthread_once once = OVSTHREAD_ONCE_INITIALIZER;
    struct sai_sflow *ds;

    if (ovsthread_once_start(&once)) {
        ovs_mutex_init_recursive(&mutex);
        ovsthread_once_done(&once);
    }

    if(pgsflow) return pgsflow;

    ds = xcalloc(1, sizeof *ds);
    ds->next_tick = time_now() + 1;
    hmap_init(&ds->ports);
    ds->probability = 0;
    ovs_refcount_init(&ds->ref_cnt);

    ds->sflow_id.data = SAI_NULL_OBJECT_ID;

    pgsflow = ds;

    return ds;
}

void
sai_sflow_destroy(struct sai_sflow *ds) OVS_EXCLUDED(mutex)
{
    if (ds->sflow_agent) {
        struct sai_sflow_port *dsp, *next;

        HMAP_FOR_EACH_SAFE (dsp, next, hmap_node, &ds->ports) {
            sai_sflow_del_poller(ds, dsp);
        }

        sai_sflow_clear(ds);
    }
}

static void
sai_sflow_add_poller(struct sai_sflow *ds, struct sai_sflow_port *dsp)
    OVS_REQUIRES(mutex)
{
    SFLPoller *poller = sfl_agent_addPoller(ds->sflow_agent, &dsp->dsi, ds,
                                            sflow_agent_get_counters);
    sfl_poller_set_sFlowCpInterval(poller, ds->options->polling_interval);
    sfl_poller_set_sFlowCpReceiver(poller, RECEIVER_INDEX);
    sfl_poller_set_bridgePort(poller, odp_to_u32(dsp->hw_lane_id));

    ops_sai_port_sample_packet_set(SFL_DS_INDEX(dsp->dsi) ,ds->sflow_id);
}

void
sai_sflow_add_port(struct sai_sflow *ds, struct ofport *ofport,
                    uint32_t hw_lane_id) OVS_EXCLUDED(mutex)
{
    struct sai_sflow_port *dsp;
    uint32_t    ifindex;

    ovs_mutex_lock(&mutex);
    sai_sflow_del_port(ds, hw_lane_id);

    ifindex = hw_lane_id;

    if (ifindex <= 0) {
        /* Not an ifindex port, and not a tunnel port either
         * so do not add a cross-reference to it here.
         */
        goto out;
    }

    /* Add to table of ports. */
    dsp = xmalloc(sizeof *dsp);
    dsp->ofport = ofport;
    dsp->hw_lane_id = hw_lane_id;
    hmap_insert(&ds->ports, &dsp->hmap_node, hash_int(hw_lane_id,0));

    if (ifindex > 0) {
        /* Add poller for ports that have ifindex. */
        SFL_DS_SET(dsp->dsi, SFL_DSCLASS_IFINDEX, ifindex, 0);
        if (ds->sflow_agent) {
            sai_sflow_add_poller(ds, dsp);
        }
    } else {
        /* Record "ifindex unknown" for the others */
        SFL_DS_SET(dsp->dsi, SFL_DSCLASS_IFINDEX, 0, 0);
    }

out:
    ovs_mutex_unlock(&mutex);
}

static void
sai_sflow_del_poller(struct sai_sflow *ds, struct sai_sflow_port *dsp)
    OVS_REQUIRES(mutex)
{
    handle_t               id = HANDLE_INITIALIZAER;    /* SAI_NULL_OBJECT_ID */

    sfl_agent_removePoller(ds->sflow_agent, &dsp->dsi);
    sfl_agent_removeSampler(ds->sflow_agent, &dsp->dsi);
    ops_sai_port_sample_packet_set(SFL_DS_INDEX(dsp->dsi) ,id);
}

void
sai_sflow_del_port(struct sai_sflow *ds, uint32_t hw_lane_id)
    OVS_EXCLUDED(mutex)
{
    struct sai_sflow_port *dsp;

    ovs_mutex_lock(&mutex);
    dsp = sai_sflow_find_port(ds, hw_lane_id);
    if (dsp) {
        if(ds->sflow_agent && SFL_DS_INDEX(dsp->dsi)) {
            sai_sflow_del_poller(ds, dsp);
        }

        hmap_remove(&ds->ports, &dsp->hmap_node);
        free(dsp);
    }
    ovs_mutex_unlock(&mutex);
}

bool sai_sflow_port_in_list(struct sai_sflow *ds, uint32_t hw_lane_id)
    OVS_EXCLUDED(mutex)
{
    struct sai_sflow_port *dsp;

    ovs_mutex_lock(&mutex);
    dsp = sai_sflow_find_port(ds, hw_lane_id);
    if (dsp) {
        ovs_mutex_unlock(&mutex);
        return 0;
    }
    ovs_mutex_unlock(&mutex);

    return -1;
}

void
sai_sflow_set_options(struct sai_sflow *ds,
                       const struct ofproto_sflow_options *options)
    OVS_EXCLUDED(mutex)
{
    struct sai_sflow_port *dsp;
    bool options_changed;
    SFLReceiver *receiver;
    SFLAddress agentIP;
    time_t now;
    SFLDataSource_instance dsi;
    uint32_t dsIndex;
    uint32_t datagram;
    SFLSampler *sampler;
//    SFLPoller *poller;

    ovs_mutex_lock(&mutex);
    if (sset_is_empty(&options->targets) || !options->sampling_rate) {
        /* No point in doing any work if there are no targets or nothing to
         * sample. */
        sai_sflow_clear__(ds);
        goto out;
    }

    options_changed = (!ds->options
                       || !ofproto_sflow_options_equal(options, ds->options));

    /* Configure collectors if options have changed or if we're shortchanged in
     * collectors (which indicates that opening one or more of the configured
     * collectors failed, so that we should retry). */
    if (options_changed
        || collectors_count(ds->collectors) < sset_count(&options->targets)) {
        collectors_destroy(ds->collectors);
        collectors_create(&options->targets, SFL_DEFAULT_COLLECTOR_PORT,
                          &ds->collectors);
        if (ds->collectors == NULL) {
            VLOG_WARN_RL(&rl, "no collectors could be initialized, "
                         "sFlow disabled");
            sai_sflow_clear__(ds);
            goto out;
        }
    }

    /* Choose agent IP address and agent device (if not yet setup) */
    if (!sflow_choose_agent_address(options->agent_ip, &agentIP)) {
        sai_sflow_clear__(ds);
        goto out;
    }

    /* Avoid reconfiguring if options didn't change. */
    if (!options_changed) {
        goto out;
    }
    ofproto_sflow_options_destroy(ds->options);
    ds->options = ofproto_sflow_options_clone(options);

    /* Create agent. */
    VLOG_INFO("creating sFlow agent %d", options->sub_id);
    if (ds->sflow_agent) {
        sflow_global_counters_subid_clear(ds->sflow_agent->subId);
        sfl_agent_release(ds->sflow_agent);
     ops_sai_sflow_remove(ds->sflow_id);
     ds->sflow_id.data = 0;
    }
    ds->sflow_agent = xcalloc(1, sizeof *ds->sflow_agent);
    now = time_wall();
    sfl_agent_init(ds->sflow_agent,
                   &agentIP,
                   options->sub_id,
                   now,         /* Boot time. */
                   now,         /* Current time. */
                   ds,          /* Pointer supplied to callbacks. */
                   sflow_agent_alloc_cb,
                   sflow_agent_free_cb,
                   sflow_agent_error_cb,
                   sflow_agent_send_packet_cb);

    if (ds->options->max_datagram) {
        datagram = ds->options->max_datagram;
    } else {
        datagram = SFL_DEFAULT_DATAGRAM_SIZE;
    }

    receiver = sfl_agent_addReceiver(ds->sflow_agent);
    sfl_receiver_set_sFlowRcvrOwner(receiver, "OpenSwitch sFlow");
    sfl_receiver_set_sFlowRcvrTimeout(receiver, 0xffffffff);
    sfl_receiver_set_sFlowRcvrMaximumDatagramSize(receiver, datagram);

    /* Set the sampling_rate down in the datapath. */
    ds->probability = MAX(1, UINT32_MAX / ds->options->sampling_rate);

    ops_sai_sflow_create(&ds->sflow_id, ds->options->sampling_rate);

    /* Add a single sampler for the bridge. This appears as a PHYSICAL_ENTITY
       because it is associated with the hypervisor, and interacts with the server
       hardware directly.  The sub_id is used to distinguish this sampler from
       others on other bridges within the same agent. */
    dsIndex = 1000 + options->sub_id;
    SFL_DS_SET(dsi, SFL_DSCLASS_PHYSICAL_ENTITY, dsIndex, 0);
    sampler = sfl_agent_addSampler(ds->sflow_agent, &dsi);
    sfl_sampler_set_sFlowFsPacketSamplingRate(sampler, ds->options->sampling_rate);
    sfl_sampler_set_sFlowFsMaximumHeaderSize(sampler, ds->options->header_len);
    sfl_sampler_set_sFlowFsReceiver(sampler, RECEIVER_INDEX);
#if 0
    /* Add a counter poller for the bridge so we can use it to send
       global counters such as datapath cache hit/miss stats. */
    poller = sfl_agent_addPoller(ds->sflow_agent, &dsi, ds,
                                 sflow_agent_get_global_counters);
    sfl_poller_set_sFlowCpInterval(poller, ds->options->polling_interval);
    sfl_poller_set_sFlowCpReceiver(poller, RECEIVER_INDEX);
#endif
    /* Add pollers for the currently known ifindex-ports */
    HMAP_FOR_EACH (dsp, hmap_node, &ds->ports) {
        if (SFL_DS_INDEX(dsp->dsi)) {
            sai_sflow_add_poller(ds, dsp);
        }
    }


out:
    ovs_mutex_unlock(&mutex);
}

void
sai_sflow_received(struct sai_sflow *ds,void *buffer, size_t buffer_size, uint32_t hw_lane_id)
    OVS_EXCLUDED(mutex)
{
    SFL_FLOW_SAMPLE_TYPE fs;
    SFLFlow_sample_element hdrElem;
    SFLSampled_header *header;
    SFLSampler *sampler;
    struct sai_sflow_port *in_dsp;

    ovs_mutex_lock(&mutex);
    sampler = ds->sflow_agent ? ds->sflow_agent->samplers : NULL;
    if (!sampler) {
        goto out;
    }

    /* Build a flow sample. */
    memset(&fs, 0, sizeof fs);

    /* Look up the input ifIndex if this port has one. Otherwise just
     * leave it as 0 (meaning 'unknown') and continue. */
    in_dsp = sai_sflow_find_port(ds, hw_lane_id);
    if (in_dsp) {
        fs.input = SFL_DS_INDEX(in_dsp->dsi);
    }

    /* Make the assumption that the random number generator in the datapath converges
     * to the configured mean, and just increment the samplePool by the configured
     * sampling rate every time. */
    sampler->samplePool += sfl_sampler_get_sFlowFsPacketSamplingRate(sampler);

    /* Sampled header. */
    memset(&hdrElem, 0, sizeof hdrElem);
    hdrElem.tag = SFLFLOW_HEADER;
    header = &hdrElem.flowType.header;
    header->header_protocol = SFLHEADER_ETHERNET_ISO8023;
    /* The frame_length should include the Ethernet FCS (4 bytes),
     * but it has already been stripped,  so we need to add 4 here. */
    header->frame_length = buffer_size + 4;
    /* Ethernet FCS stripped off. */
    header->stripped = 4;
    header->header_length = MIN(buffer_size,
                                sampler->sFlowFsMaximumHeaderSize);
    header->header_bytes = buffer;

    /* Submit the flow sample to be encoded into the next datagram. */
    SFLADD_ELEMENT(&fs, &hdrElem);
    sfl_sampler_writeFlowSample(sampler, &fs);

out:
    ovs_mutex_unlock(&mutex);
}

void
sai_sflow_run(struct sai_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    if (ds->collectors != NULL) {
        time_t now = time_now();
        if (now >= ds->next_tick) {
            sfl_agent_tick(ds->sflow_agent, time_wall());
            ds->next_tick = now + 1;
        }
    }
    ovs_mutex_unlock(&mutex);
}

void
sai_sflow_wait(struct sai_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    if (ds->collectors != NULL) {
        poll_timer_wait_until(ds->next_tick * 1000LL);
    }
    ovs_mutex_unlock(&mutex);
}

static void
__sflow_plugin_dump_data(struct ds *ds, int argc, const char *argv[])
{
    if(pgsflow == NULL){
        ds_put_format(ds, "sflow is not config\n");
     return ;
    }

    ds_put_format(ds, "sflow config:\n");
    if(pgsflow->sflow_agent){
        ds_put_format(ds, "sampling rate :%d\n", pgsflow->options->sampling_rate);
     ds_put_format(ds, "polling interval:%d\n", pgsflow->options->polling_interval);
     ds_put_format(ds, "header_len     :%d\n", pgsflow->options->header_len);
     ds_put_format(ds, "agent_device  :%s\n", pgsflow->options->agent_device);
     ds_put_format(ds, "agent_ip         :%s\n", pgsflow->options->agent_ip);
    }else{
        ds_put_format(ds, "config error\n");
    }

}


static void
__sflow_unixctl_show(struct unixctl_conn *conn, int argc,
                  const char *argv[], void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    __sflow_plugin_dump_data(&ds, argc, argv);
    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
}

void
sai_sflow_init()
{
    ops_sai_sflow_init();

    unixctl_command_register("sai/sflow/show", NULL, 0, 0,
                             __sflow_unixctl_show, NULL);
}
