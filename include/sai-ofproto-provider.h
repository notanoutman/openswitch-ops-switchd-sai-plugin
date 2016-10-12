/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef OFPROTO_SAI_PROVIDER_H
#define OFPROTO_SAI_PROVIDER_H 1

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
#include <ofproto/ofproto-provider.h>

const struct ofproto_class ofproto_sai_class;

#define SAI_TYPE_IACL           "iACL"
#define SAI_TYPE_EACL           "eACL"

struct ofproto_sai {
    struct ofproto up;
    struct hmap_node all_ofproto_sai_node;      /* In 'all_ofproto_dpifs'. */
    struct hmap bundles;        /* Contains "struct ofbundle"s. */
    struct sset ports;          /* Set of standard port names. */
    struct sset ghost_ports;    /* Ports with no datapath port. */
    handle_t vrid;
    struct hmap mirrors;        /* type of struct ofmirror_sai */
};

struct ofport_sai {
    struct ofport up;
    struct ofbundle_sai *bundle;        /* Bundle that contains this port */
    struct ovs_list bundle_node;        /* In struct ofbundle's "ports" list. */

    struct ofbundle_sai *tx_lag_bundle;        /* Bundle that contains this port */
    struct ovs_list bundle_tx_lag_node;        /* In struct ofbundle's "tx_number_ports" list. */
};

struct ofbundle_sai {
    struct hmap_node hmap_node; /* In struct ofproto's "bundles" hmap. */
    struct ofproto_sai *ofproto;        /* Owning ofproto. */
    void *aux;                  /* Key supplied by ofproto's client. */
    char *name;                 /* Identifier for log messages. */

    /* Configuration. */
    struct ovs_list ports;      /* Contains "struct ofport"s. */
    enum port_vlan_mode vlan_mode;      /* VLAN mode */
    int vlan;                   /* -1=trunk port, else a 12-bit VLAN ID. */
    unsigned long *trunks;      /* Bitmap of trunked VLANs, if 'vlan' == -1.
                                 * NULL if all VLANs are trunked. */

    /* L3 interface */
    struct {
        bool created;
        bool enabled;
        handle_t handle; /* VLAN or port ID */
        handle_t rifid;
        bool is_loopback;
    } router_intf;

    /* L3 IP addresses */
    char *ipv4_primary;
    char *ipv6_primary;
    struct hmap ipv4_secondary;
    struct hmap ipv6_secondary;

    /* Local routes entries */
    struct hmap local_routes;
    /* Neighbor entries */
    struct hmap neighbors;

    struct {
        bool cache_config; /* Specifies if config should be cached */
        struct ofproto_bundle_settings *config;
        struct hmap local_routes;
    } config_cache;

    /* LAG Info */
    struct {
        int                      is_lag;
	 int 			     lag_id;
        handle_t             lag_hw_handle;
        struct ovs_list    tx_number_ports;        /* Contains "struct ofport"s. */
    }lag_info;

    /* Mirror info */
    struct ovs_list     ingress_node;
    struct ovs_list     egress_node;
    struct ofmirror_sai *ingress_owner;    /* owning ingress ofmirror_sai */
    struct ofmirror_sai *egress_owner;    /* owning egress ofmirror_sai */
};

struct ofmirror_sai {
    struct hmap_node        hmap_node;

    /* Owning ofproto. */
    struct ofproto_sai      *ofproto;

    /* the created session handle, returned from ctc api */
    handle_t                hid;

    /*
     * name of the mirror session, obtained from higher level
     * this field should be unique between different mirror sessions
     */
    char                    name[32];

    /*
     * 'higher' level mirror object, i.e. struct mirror in vswitchd layer
     * another field which you can treated as an 'unique key'
     */
    void                    *aux;

    /*
     * destination type
     *
     * 0, physical port
     * 1, link agg
     */
    sai_mirror_porttype_t   out_type;
    /* source port/lag */
    sai_mirror_portid_t     out_portid;
    /**/
    struct ofbundle_sai            *out;

    /* ingress source ports, type of struct ofbundle_sai */
    struct ovs_list         ingress_srcs;
    /* egress source ports, type of struct ofbundle_sai */
    struct ovs_list         egress_srcs;

    /* base numbers when stats begin */
    uint64_t tx_base_packets, tx_base_bytes;
};

struct ofproto_sai_group {
    struct ofgroup up;
};

struct port_dump_state {
    uint32_t bucket;
    uint32_t offset;
    bool ghost;
    struct ofproto_port port;
    bool has_port;
};

/*
 * Cast netdev ofproto to ofproto_sai.
 */
inline struct ofproto_sai *
ofproto_sai_cast(const struct ofproto *ofproto)
{
    ovs_assert(ofproto);
    ovs_assert(ofproto->ofproto_class == &ofproto_sai_class);
    return CONTAINER_OF(ofproto, struct ofproto_sai, up);
}

void ofproto_sai_register(void);
int ofproto_sai_bundle_enable(const char *);
int ofproto_sai_bundle_disable(const char *);

int ofbundle_get_port_name_by_handle_id(handle_t, char*);
int ofbundle_get_handle_id_by_tid(const int tid, handle_t *);

#endif /* sai-ofproto-provider.h */