/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_ROUTE_H
#define SAI_ROUTE_H 1

#include <sai-common.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

#include <hmap.h>
#include <hash.h>
#include <netdev.h>

struct route_class {
    /**
    * Initializes route.
    */
    void (*init)(void);
    /**
     *  Function for adding IP to me route.
     *  Means tell hardware to trap packets with specified prefix to CPU.
     *  Used while assigning IP address(s) to routing interface.
     *
     * @param[in] vrid    - virtual router ID
     * @param[in] prefix  - IP prefix
     *
     * @return 0     if operation completed successfully.
     * @return errno if operation failed.*/
    int  (*ip_to_me_add)(const handle_t *vrid, const char *prefix);
    int  (*ip_to_me_delete)(const handle_t *vrid, const char *prefix);
    /**
     *  Function for adding local route.
     *  Means while creating new routing interface and assigning IP address to it
     *  'tells' the hardware that new subnet is accessible through specified
     *  router interface.
     *
     * @param[in] vrid    - virtual router ID
     * @param[in] prefix  - IP prefix
     * @param[in] rifid   - router interface ID
     *
     * @return 0     if operation completed successfully.
     * @return errno if operation failed.*/
    int  (*local_add)(const handle_t *vrid,
                      const char     *prefix,
                      const handle_t *rifid);
    /**
     *  Function for adding next hops(list of remote routes) which are accessible
     *  over specified IP prefix
     *
     * @param[in] vrid           - virtual router ID
     * @param[in] prefix         - IP prefix
     * @param[in] next_hop_count - count of next hops
     * @param[in] next_hops      - list of next hops
     *
     * @return 0     if operation completed successfully.
     * @return errno if operation failed.*/
    int  (*remote_add)(handle_t           vrid,
                       const char        *prefix,
                       uint32_t           next_hop_count,
                       char *const *const next_hops);
    /**
     *  Function for deleting next hops(list of remote routes) which now are not
     *  accessible over specified IP prefix
     *
     * @param[in] vrid           - virtual router ID
     * @param[in] prefix         - IP prefix
     * @param[in] next_hop_count - count of next hops
     * @param[in] next_hops      - list of next hops
     *
     * @return 0     if operation completed successfully.
     * @return errno if operation failed.*/
    int  (*remote_nh_remove)(handle_t            vrid,
                             const char         *prefix,
                             uint32_t            next_hop_count,
                             char *const *const  next_hops);
    /**
     *  Function for deleting remote route
     *
     * @param[in] vrid   - virtual router ID
     * @param[in] prefix - IP prefix over which next hops can be accessed
     *
     * @notes if next hops were already configured earlier for this route then
     *        delete all next-hops of a route as well as the route itself.
     *
     * @return 0     if operation completed successfully.
     * @return errno if operation failed.*/
    int  (*remove)(const handle_t *vrid, const char *prefix);

    int  (*if_addr_add)(const handle_t *vrid, const char *prefix, const char *ifname);

    int  (*if_addr_delete)(const handle_t *vrid, const char *prefix, const char *ifname);

    /**
     * De-initializes route.
     */
    void (*deinit)(void);
};

DECLARE_GENERIC_CLASS_GETTER(struct route_class, route);

#define ops_sai_route_class_generic() (CLASS_GENERIC_GETTER(route)())

#ifndef ops_sai_route_class
#define ops_sai_route_class ops_sai_route_class_generic
#endif

enum ops_route_state {
    OPS_ROUTE_STATE_NON_ECMP = 0,
    OPS_ROUTE_STATE_ECMP
};

struct nh_entry {
    struct hmap_node nh_hmap_node;
    uint32_t ref;
    bool is_ipv6_addr;
    char *id;
    handle_t handle;
};

typedef struct sai_ops_route_s
{
    struct      hmap_node node;
    int         vrf;
    bool        is_ipv6;                   /* IP V4/V6 */
    char        *prefix;
    struct      hmap nexthops;              /* list of selected next hops */
    uint8_t     n_nexthops;                 /* number of nexthops */
    uint8_t     refer_cnt;                 /* refer counts for route with nexthop of interface */
    enum        ops_route_state rstate;     /* state of route */
    handle_t    nh_ecmp;                   /* next hop index ecmp*/
}sai_ops_route_t;

typedef struct sai_ops_nexthop_s
{
    struct      hmap_node node;             /* route->nexthops */
    char        *id;                        /* IP address or Port name */
}sai_ops_nexthop_t;

struct if_addr {
    struct hmap_node if_addr_hmap_node;
    bool is_ipv6_addr;
    char *ifname;       /* interface name  */
    char *prefix;
    handle_t key;
};

static inline void
ops_sai_route_init(void)
{
    ovs_assert(ops_sai_route_class()->init);
    ops_sai_route_class()->init();
}

static inline int
ops_sai_route_ip_to_me_add(const handle_t *vrid, const char *prefix)
{
    ovs_assert(ops_sai_route_class()->ip_to_me_add);
    return ops_sai_route_class()->ip_to_me_add(vrid, prefix);
}

static inline int
ops_sai_route_ip_to_me_delete(const handle_t *vrid, const char *prefix)
{
    ovs_assert(ops_sai_route_class()->ip_to_me_delete);
    return ops_sai_route_class()->ip_to_me_delete(vrid, prefix);
}

static inline int
ops_sai_route_local_add(const handle_t *vrid,
                        const char     *prefix,
                        const handle_t *rifid)
{
    ovs_assert(ops_sai_route_class()->local_add);
    return ops_sai_route_class()->local_add(vrid, prefix, rifid);
}

static inline int
ops_sai_route_remote_add(handle_t           vrid,
                         const char        *prefix,
                         uint32_t           next_hop_count,
                         char *const *const next_hops)
{
    ovs_assert(ops_sai_route_class()->remote_add);
    return ops_sai_route_class()->remote_add(vrid, prefix, next_hop_count,
                                             next_hops);
}

static inline int
ops_sai_route_remote_nh_remove(handle_t           vrid,
                               const char        *prefix,
                               uint32_t           next_hop_count,
                               char *const *const next_hops)
{
    ovs_assert(ops_sai_route_class()->remote_nh_remove);
    return ops_sai_route_class()->remote_nh_remove(vrid, prefix,
                                                   next_hop_count,
                                                   next_hops);
}

static inline int
ops_sai_route_remove(const handle_t *vrid, const char     *prefix)
{
    ovs_assert(ops_sai_route_class()->remove);
    return ops_sai_route_class()->remove(vrid, prefix);
}

static inline void
ops_sai_route_deinit(void)
{
    ovs_assert(ops_sai_route_class()->deinit);
    ops_sai_route_class()->deinit();
}

struct nh_entry*
ops_sai_neighbor_get_nexthop(const char* id);

struct nh_entry*
ops_sai_neighbor_add_nexthop(const char* id, handle_t *l3_egress_id);

int
ops_sai_neighbor_delete_nexthop(const char* id);

int32_t
ops_sai_routing_nexthop_create(const handle_t *rif, const char* prefix, handle_t *l3_egress_id);

int32_t
ops_sai_routing_nexthop_remove(handle_t *l3_egress_id);

#endif /* sai-route.h */