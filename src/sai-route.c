/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-route.h>
#include <sai-api-class.h>
#include <stdlib.h>

VLOG_DEFINE_THIS_MODULE(sai_route);

#undef malloc

#define SAI_NEXT_HOP_MAX    8

#define OPS_ROUTE_HASH_MAXSIZE 64

#define COPS_IPV4_ADDR_LEN_IN_BIT        32          /**< IPv4 address length in bit */

#define SAI_IPV4_LEN_TO_MASK(mask, len)  \
    {                           \
        (mask) = (len) ? ~((1 << (COPS_IPV4_ADDR_LEN_IN_BIT - (len))) - 1) : 0; \
    }

struct hmap all_nexthop     = HMAP_INITIALIZER(&all_nexthop);
struct hmap all_route       = HMAP_INITIALIZER(&all_route);
struct hmap all_if_addr     = HMAP_INITIALIZER(&all_if_addr);

struct hmap*
sai_nexthop_hmap_get_global()
{
    return &all_nexthop;
}

int
ops_sai_nh_refcnt_inc(struct nh_entry *p_nh_entry)
{
    if (NULL != p_nh_entry)
    {
        p_nh_entry->ref ++;
    }

    return 0;
}

int
ops_sai_nh_refcnt_dec(struct nh_entry *p_nh_entry)
{
    if (NULL != p_nh_entry)
    {
        p_nh_entry->ref --;
    }

    return 0;
}

struct nh_entry*
nexthop_hmap_find_by_id(struct hmap *nh_hmap, char* id)
{
    struct nh_entry *p_nh_entry = NULL;
    char            *hashstr    = NULL;

    hashstr = id;
    HMAP_FOR_EACH_WITH_HASH(p_nh_entry, nh_hmap_node, hash_string(hashstr, 0),
                            nh_hmap) {
        if ((strcmp(p_nh_entry->id, id) == 0)){
            return p_nh_entry;
        }
    }

    return NULL;
}

int
nexthop_hmap_add(struct hmap *nh_hmap, struct nh_entry *nh_entry)
{
    struct nh_entry *p_nh_entry = NULL;
    char            *hashstr    = NULL;

    if (!nh_entry) {
        return 1;
    }

    p_nh_entry = nexthop_hmap_find_by_id(nh_hmap, nh_entry->id);
    if (NULL == p_nh_entry) {
        nh_entry->ref = 1;
    }
    else {
        ops_sai_nh_refcnt_inc(p_nh_entry);
        return 0;
    }

    hashstr = nh_entry->id;
    hmap_insert(nh_hmap, &nh_entry->nh_hmap_node, hash_string(hashstr, 0));

    return 0;
}

int
nexthop_hmap_delete(struct hmap *nh_hmap, struct nh_entry *nh_entry)
{
    if (!nh_entry) {
        return 1;
    }

    if (nh_entry->ref > 1) {
        ops_sai_nh_refcnt_dec(nh_entry);
        return 0;
    }

    hmap_remove(nh_hmap, &nh_entry->nh_hmap_node);
    if (nh_entry->id) {
        free(nh_entry->id);
    }

    free(nh_entry);

    return 0;
}

struct if_addr*
if_addr_hmap_find_by_prefix(struct hmap *if_addr_hmap, char* prefix)
{
    struct if_addr *pst_if_addr = NULL;
    char            *hashstr    = NULL;

    hashstr = prefix;
    HMAP_FOR_EACH_WITH_HASH(pst_if_addr, if_addr_hmap_node, hash_string(hashstr, 0),
                            if_addr_hmap) {
        if ((strcmp(pst_if_addr->prefix, prefix) == 0)){
            return pst_if_addr;
        }
    }

    return NULL;
}

struct if_addr*
if_addr_hmap_find_by_ifname(struct hmap *if_addr_hmap, char* ifname)
{
    struct if_addr  *pst_if_addr    = NULL;
    struct if_addr  *next           = NULL;

    HMAP_FOR_EACH_SAFE (pst_if_addr, next, if_addr_hmap_node, &all_if_addr) {

        if ((strcmp(pst_if_addr->ifname, ifname) == 0)){
            return pst_if_addr;
        }
    }

    return NULL;
}

int
if_addr_hmap_add(struct hmap *if_addr_hmap, struct if_addr *p_if_addr)
{
    struct if_addr *pst_if_addr = NULL;
    char            *hashstr    = NULL;

    if (!p_if_addr) {
        return 1;
    }

    pst_if_addr = if_addr_hmap_find_by_prefix(if_addr_hmap, p_if_addr->prefix);
    if (NULL != pst_if_addr) {
        return 0;
    }

    hashstr = p_if_addr->prefix;
    hmap_insert(if_addr_hmap, &p_if_addr->if_addr_hmap_node, hash_string(hashstr, 0));

    return 0;
}

int
if_addr_hmap_delete(struct hmap *if_addr_hmap, struct if_addr *p_if_addr)
{
    if (!p_if_addr) {
        return 1;
    }

    hmap_remove(if_addr_hmap, &p_if_addr->if_addr_hmap_node);
    if (p_if_addr->prefix) {
        free(p_if_addr->prefix);
    }

    if (p_if_addr->ifname) {
        free(p_if_addr->ifname);
    }

    free(p_if_addr);

    return 0;
}

struct nh_entry*
ops_sai_neighbor_get_nexthop(const char* id)
{
    struct nh_entry *pst_nh = NULL;
    struct hmap     *nh_hmap= NULL;
    char   id_cp[64];

    memset(id_cp, 0x0, sizeof(id_cp));
    memcpy(id_cp, id, sizeof(id_cp));
    nh_hmap = sai_nexthop_hmap_get_global();

    if (NULL != nh_hmap) {
        pst_nh = nexthop_hmap_find_by_id(nh_hmap, id_cp);

        if (NULL != pst_nh) {
            return pst_nh;
        }
    }

    return NULL;
}

struct nh_entry*
ops_sai_neighbor_add_nexthop(const char* id, handle_t *l3_egress_id)
{

    struct nh_entry *p_nh_entry = NULL;
    struct hmap     *nh_hmap = NULL;

    nh_hmap = sai_nexthop_hmap_get_global();
    if (NULL == nh_hmap) {
        return NULL;
    }

    p_nh_entry = xzalloc(sizeof(*p_nh_entry));
    if (NULL  == p_nh_entry) {
        return NULL;
    }

    memset(p_nh_entry, 0, sizeof(*p_nh_entry));

    p_nh_entry->id = xstrdup(id);
    p_nh_entry->handle.data = l3_egress_id->data;

    nexthop_hmap_add(nh_hmap, p_nh_entry);

    return p_nh_entry;
}

int
ops_sai_neighbor_delete_nexthop(const char* id)
{

    struct nh_entry *p_nh_entry = NULL;
    struct hmap* nh_hmap = NULL;
    char   id_cp[64];

    memset(id_cp, 0x0, sizeof(id_cp));
    memcpy(id_cp, id, sizeof(id_cp));
    nh_hmap = sai_nexthop_hmap_get_global();
    if (NULL == nh_hmap) {
        return -1;
    }

    p_nh_entry = nexthop_hmap_find_by_id(nh_hmap, id_cp);
    if (NULL != p_nh_entry && p_nh_entry->ref > 1) {
        ops_sai_nh_refcnt_dec(p_nh_entry);
        return 1;
    }

    nexthop_hmap_delete(nh_hmap, p_nh_entry);

    return 0;
}

struct nh_entry*
ops_sai_route_get_nexthop(char* id)
{
    struct nh_entry *pst_nh = NULL;

    pst_nh = nexthop_hmap_find_by_id(&all_nexthop, id);

    return pst_nh;
}

struct nh_entry*
ops_sai_route_add_nexthop(char* id, handle_t *l3_egress_id)
{
    struct nh_entry *p_nh_entry = NULL;

    p_nh_entry = xzalloc(sizeof(*p_nh_entry));
    if (NULL  == p_nh_entry) {
        return NULL;
    }

    memset(p_nh_entry, 0, sizeof(*p_nh_entry));

    p_nh_entry->id = xstrdup(id);
    p_nh_entry->handle.data = l3_egress_id->data;

    nexthop_hmap_add(&all_nexthop, p_nh_entry);

    return p_nh_entry;
}

int
ops_sai_route_delete_nexthop(char* id)
{
    struct nh_entry *p_nh_entry = NULL;

    p_nh_entry = nexthop_hmap_find_by_id(&all_nexthop, id);
    if (NULL != p_nh_entry && p_nh_entry->ref > 1) {
        ops_sai_nh_refcnt_dec(p_nh_entry);
        return 1;
    }

    nexthop_hmap_delete(&all_nexthop, p_nh_entry);

    return 0;
}

/* Add nexthop into the route entry */
static void
ops_sai_nexthop_add(sai_ops_route_t *route, char *id)
{
    sai_ops_nexthop_t   *nh         = NULL;
    char                *hashstr    = NULL;

    if (!route || !id) {
        return;
    }

    nh = malloc(sizeof(*nh));
    if (!nh) {
        return;
    }

    /* NOTE: Either IP or Port, not both */
    nh->id = xstrdup(id);

    hashstr = id;
    hmap_insert(&route->nexthops, &nh->node, hash_string(hashstr, 0));
    route->n_nexthops++;

    VLOG_DBG("Add NH %s, for route %s", nh->id, route->prefix);
}

/* Delete nexthop into route entry */
static void
ops_sai_nexthop_delete(sai_ops_route_t *route, sai_ops_nexthop_t *nh)
{
    if (!route || !nh) {
        return;
    }

    VLOG_DBG("Delete NH %s in route %s", nh->id, route->prefix);

    hmap_remove(&route->nexthops, &nh->node);
    if (nh->id) {
        free(nh->id);
    }
    free(nh);
    route->n_nexthops--;
}

/* Find nexthop entry in the route's nexthops hash */
sai_ops_nexthop_t*
ops_sai_nexthop_lookup(sai_ops_route_t *route, char *id)
{
    sai_ops_nexthop_t   *nh         = NULL;
    char                *hashstr    = NULL;

    if (!route || !id) {
        return NULL;
    }

    hashstr = id;
    HMAP_FOR_EACH_WITH_HASH(nh, node, hash_string(hashstr, 0),
                            &route->nexthops) {
        if ((strcmp(nh->id, id) == 0)){
            return nh;
        }
    }
    return NULL;
}

static void
ops_sai_route_hash(int vrf, const char *prefix, char *hashstr, int hashlen)
{
    snprintf(hashstr, hashlen, "%d:%s", vrf, prefix);
}

/* Find a route entry matching the prefix */
sai_ops_route_t *
ops_sai_route_lookup(int vrf, const char *prefix)
{
    sai_ops_route_t *route = NULL;
    char            hashstr[OPS_ROUTE_HASH_MAXSIZE];

    ops_sai_route_hash(vrf, prefix, hashstr, sizeof(hashstr));
    HMAP_FOR_EACH_WITH_HASH(route, node, hash_string(hashstr, 0),
                            &all_route) {
        if ((strcmp(route->prefix, prefix) == 0) &&
            (route->vrf == vrf)) {
            return route;
        }
    }
    return NULL;
} /* ops_route_lookup */

/* Add new route and NHs */
sai_ops_route_t*
ops_sai_route_add(int vrf, const char *prefix, uint32_t nh_count, char *const *const next_hops)
{
    sai_ops_route_t *routep = NULL;
    char            hashstr[OPS_ROUTE_HASH_MAXSIZE];
    int             i = 0;

    if (!prefix) {
        return NULL;
    }

    routep = malloc(sizeof(*routep));
    if (!routep) {
        return NULL;
    }

    routep->vrf = vrf;
    routep->prefix = xstrdup(prefix);
    routep->n_nexthops = 0;
    routep->refer_cnt = 0;

    hmap_init(&routep->nexthops);

    for (i = 0; i < nh_count; i++) {
        ops_sai_nexthop_add(routep, next_hops[i]);
    }

    ops_sai_route_hash(vrf, prefix, hashstr, sizeof(hashstr));
    hmap_insert(&all_route, &routep->node, hash_string(hashstr, 0));
    return routep;
}

/* Update route nexthop: add, delete, resolve and unresolve nh */
static int
ops_sai_route_update(int vrf, sai_ops_route_t *routep,
                 uint32_t nh_count,
                 char *const *const next_hops,
                 bool is_delete_nh)
{
    sai_ops_nexthop_t* nh = NULL;
    int ret = 0;
    int i;

    for (i = 0; i < nh_count; i++) {
        nh = ops_sai_nexthop_lookup(routep, next_hops[i]);
        if (is_delete_nh) {
            ops_sai_nexthop_delete(routep, nh);
        } else {
            /* add or update */
            if (!nh) {
                ops_sai_nexthop_add(routep, next_hops[i]);
            } else {
                /* update is currently resolved on unreoslved */
                ret = 1;
                continue;
            }
        }
    }

    return ret;
}

/* Delete route in system*/
static void
ops_sai_route_del(sai_ops_route_t *routep)
{
    sai_ops_nexthop_t *nh = NULL;
    sai_ops_nexthop_t *next = NULL;

    if (!routep) {
        return;
    }

    VLOG_DBG("delete route %s", routep->prefix);

    hmap_remove(&all_route, &routep->node);

    HMAP_FOR_EACH_SAFE(nh, next, node, &routep->nexthops) {
        ops_sai_nexthop_delete(routep, nh);
    }

    if (routep->prefix) {
        free(routep->prefix);
    }
    free(routep);
}

int
ops_string_to_prefix(char *prefix, uint32_t *addr, uint8_t *mask_len)
{
    int     rc = 0;
    int     ret = 0;
    char    *ptr = NULL;
    char    buf[64];
    int     i = 0;
    uint8_t mask =0;

    memset(buf, 0, sizeof(buf));

    ptr = strtok(prefix, "/");
    while(NULL != ptr)
    {
        if (0 == i)
        {
            rc = inet_pton(AF_INET, ptr, buf);
            if (!rc)
            {
                ret = 1;
                return ret;
            }

            memcpy(addr, buf, sizeof(uint32_t));
        }
        else if (1 == i)
        {
            mask = atoi(ptr);
            *mask_len = mask;
        }
        else
        {
            return ret;
        }

        ptr = strtok(NULL, "/");
        i ++;
    }

    return ret;
}

/* Add nexthop into the route entry */
int32_t
ops_sai_routing_nexthop_create(const handle_t *rif, const char* prefix, handle_t *l3_egress_id)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t attr[4];

    if (NULL == prefix)
    {
        return SAI_STATUS_FAILURE;
    }

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_NEXT_HOP_ATTR_TYPE;
    attr[0].value.s32 = SAI_NEXT_HOP_IP;
    attr[1].id = SAI_NEXT_HOP_ATTR_IP;

    char                buf[64];
    int                 retv = 0;
    uint32_t            ip_prefix;
    sai_object_id_t     nhid = 0;

    memset(&ip_prefix, 0, sizeof(ip_prefix));
    memset(buf, 0, sizeof(buf));
    retv = inet_pton(AF_INET, prefix, buf);
    if (!retv) {
        VLOG_ERR("Invaild prefix %s", prefix);
        return status;
    }
    memcpy(&ip_prefix, buf, sizeof(ip_prefix));

    attr[1].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attr[1].value.ipaddr.addr.ip4 = htonl(ip_prefix);

    attr[2].id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
    if (NULL != rif) {
        attr[2].value.oid = rif->data;
    }
    else {
        attr[2].value.oid = 0;
    }

    status = sai_api->nexthop_api->create_next_hop(&nhid, 3, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to create nexthop");
    l3_egress_id->data = nhid;

exit:
    return status;
}

/* Del nexthop into the route entry */
int32_t
ops_sai_routing_nexthop_remove(handle_t *l3_egress_id)
{
    sai_status_t        status  = SAI_STATUS_SUCCESS;
    sai_object_id_t     nhid    = 0;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    if (NULL == l3_egress_id)
    {
        return SAI_STATUS_SUCCESS;
    }

    nhid = l3_egress_id->data;
    status = sai_api->nexthop_api->remove_next_hop(nhid);

    SAI_ERROR_LOG_EXIT(status, "Failed to remove nexthop");

exit:
    return status;
}

int32_t
ops_sai_routing_nh_group_add(sai_ops_route_t *pst_routep, handle_t *l3_egress_id)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    sai_ops_nexthop_t *pst_nh = NULL;
    struct nh_entry*  p_nexthop= NULL;

    sai_attribute_t             attr[2];
    sai_object_id_t             nhg_obj;
    sai_object_list_t           obj_list;
    sai_object_id_t             nh_obj[SAI_NEXT_HOP_MAX];
    uint32_t                    i = 0;

    memset(&obj_list, 0, sizeof(obj_list));
    memset(&nhg_obj, 0, sizeof(nhg_obj));
    memset(attr, 0, sizeof(attr));
    memset(nh_obj, 0, sizeof(nh_obj));

    /* Operation one: add the nexthop group */
    attr[0].id          = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
    attr[0].value.u32   = SAI_NEXT_HOP_GROUP_ECMP;

    /* update with single nexthop */
    HMAP_FOR_EACH(pst_nh, node, &pst_routep->nexthops)
    {
        if (!pst_routep->n_nexthops)
        {
            break;
        }

        p_nexthop = nexthop_hmap_find_by_id(&all_nexthop, pst_nh->id);
        if (NULL == p_nexthop)
        {
            i++;
            continue;
        }
        else
        {
            nh_obj[i] = p_nexthop->handle.data;
        }

        i++;
    }

    obj_list.list           = nh_obj;
    obj_list.count          = pst_routep->n_nexthops;
    attr[1].id              = SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST;
    memcpy(&attr[1].value.objlist, &obj_list, sizeof(obj_list));

    status = sai_api->nhg_api->create_next_hop_group(&nhg_obj, 2, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to create nexthop group");
    l3_egress_id->data = nhg_obj;

exit:
    return status;
}

int32_t
ops_sai_routing_nh_group_del(handle_t *l3_egress_id)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_object_id_t             nhg_obj;

    memset(&nhg_obj, 0, sizeof(nhg_obj));

    nhg_obj = l3_egress_id->data;

    status = sai_api->nhg_api->remove_next_hop_group(nhg_obj);
    SAI_ERROR_LOG_EXIT(status, "Failed to remove nexthop group");

exit:
    return status;
}

int32_t
ops_sai_routing_nhg_add_member(sai_ops_route_t *p_route, char *keyid)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct nh_entry     *p_nh_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    p_nh_entry = ops_sai_route_get_nexthop(keyid);

    status = sai_api->nhg_api->add_next_hop_to_group(p_route->nh_ecmp.data, 1, &p_nh_entry->handle.data);
    SAI_ERROR_LOG_EXIT(status, "Failed to add member to nexthop group");

exit:
    return status;
}

int32_t
ops_sai_routing_nhg_del_member(sai_ops_route_t *p_route, char *keyid)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct nh_entry     *p_nh_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    p_nh_entry = ops_sai_route_get_nexthop(keyid);

    if(!p_nh_entry)
		goto exit;

    status = sai_api->nhg_api->remove_next_hop_from_group(p_route->nh_ecmp.data, 1, &p_nh_entry->handle.data);
    SAI_ERROR_LOG_EXIT(status, "Failed to remove member from nexthop group");

exit:
    return status;
}


int32_t
ops_sai_routing_nexthop_id_update(sai_unicast_route_entry_t *p_route, handle_t *l3_egress_id)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_attribute_t             attr[1];

    memset(attr, 0, sizeof(attr));

    attr[0].id = SAI_ROUTE_ATTR_NEXT_HOP_ID;
    attr[0].value.oid = l3_egress_id->data;

    status = sai_api->route_api->set_route_attribute(p_route, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to update route nexthop id");

exit:
    return status;
}

static int
__ops_sai_route_local_add(const handle_t *vrid, const char *prefix)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    sai_unicast_route_entry_t route;
    sai_attribute_t attr[2];
    uint32_t    ip_prefix = 0;
    char        buf_preifx[256];
    int         rc = 0;
    uint8_t         prefix_len;
    sai_status_t    status = SAI_STATUS_SUCCESS;

    memset(&route, 0, sizeof(route));
    memset(attr, 0, sizeof(attr));
    memset(buf_preifx, 0, sizeof(buf_preifx));

    attr[0].id = SAI_ROUTE_ATTR_PACKET_ACTION;
    attr[0].value.s32 = SAI_PACKET_ACTION_TRAP;

    attr[1].id = SAI_ROUTE_ATTR_TRAP_PRIORITY;
    attr[1].value.u8 = 0;              // default to zero

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc)
    {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    route.vr_id = vrid->data;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    status = sai_api->route_api->create_route(&route, 2, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to add route entry");

exit:
    return rc;
}

static int
__ops_sai_route_local_delete(const handle_t *vrid, const char *prefix)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    sai_unicast_route_entry_t route;
    uint32_t    ip_prefix = 0;
    char        buf_preifx[256];
    int         rc = 0;
    uint8_t         prefix_len;
    sai_status_t    status = SAI_STATUS_SUCCESS;

    memset(&route, 0, sizeof(route));
    memset(buf_preifx, 0, sizeof(buf_preifx));

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc)
    {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    route.vr_id = vrid->data;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    status = sai_api->route_api->remove_route(&route);
    SAI_ERROR_LOG_EXIT(status, "Failed to remove route entry");

exit:
    return rc;
}

/*
 * Initializes route.
 */
static void
__route_init(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

/*
 *  Function for adding IP to me route.
 *  Means tell hardware to trap packets with specified prefix to CPU.
 *  Used while assigning IP address(s) to routing interface.
 *
 * @param[in] vrid    - virtual router ID
 * @param[in] prefix  - IP prefix
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__route_ip_to_me_add(const handle_t *vrid, const char *prefix)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    sai_unicast_route_entry_t route;
    sai_attribute_t attr[2];
    uint32_t    ip_prefix = 0;
    char        buf_preifx[256];
    int         rc = 0;
    uint8_t         prefix_len;
    sai_status_t    status = SAI_STATUS_SUCCESS;

    memset(&route, 0, sizeof(route));
    memset(attr, 0, sizeof(attr));
    memset(buf_preifx, 0, sizeof(buf_preifx));

    attr[0].id = SAI_ROUTE_ATTR_PACKET_ACTION;
    attr[0].value.s32 = SAI_PACKET_ACTION_TRAP;

    attr[1].id = SAI_ROUTE_ATTR_TRAP_PRIORITY;
    attr[1].value.u8 = 0;              // default to zero

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc)
    {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    route.vr_id = vrid->data;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    status = sai_api->route_api->create_route(&route, 2, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to add route entry");

exit:
    return rc;
}

static int
__route_ip_to_me_delete(const handle_t *vrid, const char *prefix)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_unicast_route_entry_t route;
    uint32_t        ip_prefix = 0;
    char            buf_preifx[256];
    int             rc = 0;
    uint8_t         prefix_len;
    sai_status_t    status = SAI_STATUS_SUCCESS;

    memset(&route, 0, sizeof(route));
    memset(buf_preifx, 0, sizeof(buf_preifx));

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc)
    {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    route.vr_id = vrid->data;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    /* del the ipuc prefix */
    status = sai_api->route_api->remove_route(&route);
    SAI_ERROR_LOG_EXIT(status, "Failed to delete route entry");


exit:
    return rc;
}

static int
__sai_route_local_action(uint64_t          vrid,
                      const char            *prefix,
                      uint32_t              next_hop_count,
                      char *const *const    next_hops,
                      uint32_t              action)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    struct nh_entry     *p_nh_entry = NULL;
    sai_ops_route_t     *ops_routep = NULL;
    sai_ops_nexthop_t   *ops_nh = NULL;
    sai_unicast_route_entry_t route;
    sai_attribute_t     attr[3];
    char                buf_preifx[256];
    handle_t            l3_egress_id;
    handle_t            l3_nhg_id;
    handle_t            l3_id_cp;
    uint32_t            ip_prefix   = 0;
    uint32_t            index       = 0;
    uint8_t             prefix_len  = 0;
    int                 rc          = 0;
    int                 vrf_id      = 0;

    memset(buf_preifx, 0, sizeof(buf_preifx));
    memset(&l3_egress_id, 0, sizeof(l3_egress_id));
    memset(&l3_id_cp, 0, sizeof(l3_id_cp));

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc) {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    memset(&route, 0, sizeof(route));
    route.vr_id = vrid;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    if (action) {
        ops_routep = ops_sai_route_lookup(vrf_id, prefix);
        if (!ops_routep) {
            ops_routep = ops_sai_route_add(vrf_id, prefix, next_hop_count, next_hops);
            if (NULL != ops_routep) {
                if (1 == ops_routep->n_nexthops) {
                    p_nh_entry = ops_sai_route_get_nexthop(next_hops[0]);
                    if (NULL == p_nh_entry) {
                        rc = ops_sai_routing_nexthop_create(NULL, next_hops[0], &l3_egress_id);
                        if (rc) {
                            return status;
                        }
                    }else {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }

                    ops_sai_route_add_nexthop(next_hops[0], &l3_egress_id);
                    l3_id_cp.data = l3_egress_id.data;

                }

                /* next comes routes adding */
                attr[0].id = SAI_ROUTE_ATTR_PACKET_ACTION;
                attr[0].value.s32 = SAI_PACKET_ACTION_FORWARD;
                attr[1].id = SAI_ROUTE_ATTR_NEXT_HOP_ID;
                attr[1].value.oid = l3_id_cp.data;
                attr[2].id = SAI_ROUTE_ATTR_TRAP_PRIORITY;
                attr[2].value.u8 = 0;                       // default to zero

                status = sai_api->route_api->create_route(&route, 3, attr);
                SAI_ERROR_LOG_EXIT(status, "Failed to add route entry");

            }
        } else {
            goto exit;
        }
    } else {
        ops_routep = ops_sai_route_lookup(vrf_id, prefix);
        if (!ops_routep) {
            return status;
        }

        if (next_hops) {
           ops_sai_route_update(vrf_id, ops_routep, next_hop_count, next_hops, true);
            if (1 == ops_routep->n_nexthops) {
                HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops)
                {
                    p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                    if (NULL != p_nh_entry) {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }
                }

                /* update the nhid */
                ops_sai_routing_nexthop_id_update(&route, &l3_egress_id);

                /* delete the nhid after */
                if (OPS_ROUTE_STATE_ECMP == ops_routep->rstate) {
                    ops_sai_routing_nh_group_del(&ops_routep->nh_ecmp);
                }
            }
            else if (1 < ops_routep->n_nexthops) {
                /* create nexthop group */
                ops_sai_routing_nh_group_add(ops_routep, &l3_nhg_id);

                /* update the nhid */
                ops_sai_routing_nexthop_id_update(&route, &l3_nhg_id);

                /* delete the nhid after */
                if (OPS_ROUTE_STATE_ECMP == ops_routep->rstate) {
                    ops_sai_routing_nh_group_del(&ops_routep->nh_ecmp);
                }

                ops_routep->nh_ecmp.data = l3_nhg_id.data;
            }

            /* del the nexthop db or refer  */
            for(index = 0; index < next_hop_count; index++)
            {
                p_nh_entry = ops_sai_route_get_nexthop(next_hops[index]);
                if (NULL != p_nh_entry) {
                    l3_egress_id.data = p_nh_entry->handle.data;
                    rc = ops_sai_route_delete_nexthop(p_nh_entry->id);
                    if (!rc) {
                        rc = ops_sai_routing_nexthop_remove(&l3_egress_id);
                    }
                }
            }
        } else {
            /* del the ipuc prefix */
            status = sai_api->route_api->remove_route(&route);
            SAI_ERROR_LOG_EXIT(status, "Failed to delete route entry");

            if (OPS_ROUTE_STATE_ECMP == ops_routep->rstate) {
                ops_sai_routing_nh_group_del(&ops_routep->nh_ecmp);
            }

            /* del the nexthop db or refer  */
            HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops)
            {
                p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                if (NULL != p_nh_entry) {
                    l3_egress_id.data = p_nh_entry->handle.data;
                    rc = ops_sai_route_delete_nexthop(p_nh_entry->id);
                    if (!rc) {
                        rc = ops_sai_routing_nexthop_remove(&l3_egress_id);
                    }
                }
            }

            ops_sai_route_del(ops_routep);
            return status;
        }
    }

exit:
    if (ops_routep->n_nexthops > 1) {
        ops_routep->rstate = OPS_ROUTE_STATE_ECMP;
    } else {
        ops_routep->rstate = OPS_ROUTE_STATE_NON_ECMP;
        ops_routep->nh_ecmp.data = 0;
    }

    return status;
}


static int
__sai_route_remote_action(uint64_t          vrid,
                      const char            *prefix,
                      uint32_t              next_hop_count,
                      char *const *const    next_hops,
                      uint32_t              action)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    struct nh_entry     *p_nh_entry = NULL;
    sai_ops_route_t     *ops_routep = NULL;
    sai_ops_nexthop_t   *ops_nh = NULL;
    sai_unicast_route_entry_t route;
    sai_attribute_t     attr[3];
    char                buf_preifx[256];
    handle_t            l3_egress_id;
    handle_t            l3_nhg_id;
    handle_t            l3_id_cp;
    uint32_t            ip_prefix   = 0;
    uint32_t            index       = 0;
    uint8_t             prefix_len  = 0;
    int                 rc          = 0;
    int                 vrf_id      = 0;
    handle_t            vrfid;

    memset(buf_preifx, 0, sizeof(buf_preifx));
    memset(&l3_egress_id, 0, sizeof(l3_egress_id));
    memset(&l3_id_cp, 0, sizeof(l3_id_cp));

    memcpy(buf_preifx, prefix, sizeof(buf_preifx));
    rc = ops_string_to_prefix(buf_preifx, &ip_prefix, &prefix_len);
    if (rc) {
        VLOG_ERR("Invalid IPv4/Prefix");
        return rc; /* Return error */
    }

    vrfid.data = vrid;
    memset(&route, 0, sizeof(route));
    route.vr_id = vrid;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(ip_prefix);
    SAI_IPV4_LEN_TO_MASK(route.destination.mask.ip4, prefix_len);

    if (action) {
        ops_routep = ops_sai_route_lookup(vrf_id, prefix);
        if (!ops_routep) {
            ops_routep = ops_sai_route_add(vrf_id, prefix, next_hop_count, next_hops);
            if (NULL != ops_routep) {
                if (1 == ops_routep->n_nexthops) {
                    p_nh_entry = ops_sai_route_get_nexthop(next_hops[0]);
                    if (NULL == p_nh_entry) {
                        rc = ops_sai_routing_nexthop_create(NULL, next_hops[0], &l3_egress_id);
                        if (rc) {
                            return status;
                        }
                    }else {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }

                    ops_sai_route_add_nexthop(next_hops[0], &l3_egress_id);
                    l3_id_cp.data = l3_egress_id.data;

                } else if (1 <= ops_routep->n_nexthops) {
                    HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops) {
                        /* need to add nexthop db or update refer cnt */
                        p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                        if (NULL == p_nh_entry) {
                            rc = ops_sai_routing_nexthop_create(NULL, ops_nh->id, &l3_egress_id);
                            if (rc) {
                                continue;
                                /* continue to add the rest of nexthops */
                            }
                        }else {
                            l3_egress_id.data = p_nh_entry->handle.data;
                        }

                        ops_sai_route_add_nexthop(ops_nh->id, &l3_egress_id);
                    }

                    /* create nexthop group */
                    ops_sai_routing_nh_group_add(ops_routep, &l3_nhg_id);
                    ops_routep->nh_ecmp.data = l3_nhg_id.data;
                    l3_id_cp.data = l3_nhg_id.data;
                }

                /* next comes routes adding */
                attr[0].id = SAI_ROUTE_ATTR_PACKET_ACTION;
                attr[0].value.s32 = SAI_PACKET_ACTION_FORWARD;
                attr[1].id = SAI_ROUTE_ATTR_NEXT_HOP_ID;
                attr[1].value.oid = l3_id_cp.data;
                attr[2].id = SAI_ROUTE_ATTR_TRAP_PRIORITY;
                attr[2].value.u8 = 0;                       // default to zero

                status = sai_api->route_api->create_route(&route, 3, attr);
                SAI_ERROR_LOG_EXIT(status, "Failed to add route entry");
            }
        } else {
            /* for support the route with intf nexthop */

            if (0 == ops_routep->n_nexthops && ops_routep->refer_cnt)
            {
                //ops_routep = ops_sai_route_add(vrf_id, prefix, next_hop_count, next_hops);
                rc = ops_sai_route_update(vrf_id, ops_routep, next_hop_count, next_hops, false);
                if (rc) {
                    goto exit;
                }

                if (1 == ops_routep->n_nexthops) {
                    p_nh_entry = ops_sai_route_get_nexthop(next_hops[0]);
                    if (NULL == p_nh_entry) {
                        rc = ops_sai_routing_nexthop_create(NULL, next_hops[0], &l3_egress_id);
                        if (rc) {
                            return status;
                        }
                    }else {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }

                    ops_sai_route_add_nexthop(next_hops[0], &l3_egress_id);
                    l3_id_cp.data = l3_egress_id.data;

                } else if (1 <= ops_routep->n_nexthops) {
                    HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops) {
                        /* need to add nexthop db or update refer cnt */
                        p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                        if (NULL == p_nh_entry) {
                            rc = ops_sai_routing_nexthop_create(NULL, ops_nh->id, &l3_egress_id);
                            if (rc) {
                                continue;
                                /* continue to add the rest of nexthops */
                            }
                        }else {
                            l3_egress_id.data = p_nh_entry->handle.data;
                        }

                        ops_sai_route_add_nexthop(ops_nh->id, &l3_egress_id);
                    }

                    /* create nexthop group */
                    ops_sai_routing_nh_group_add(ops_routep, &l3_nhg_id);
                    ops_routep->nh_ecmp.data = l3_nhg_id.data;
                    l3_id_cp.data = l3_nhg_id.data;
                }

                /* next comes routes adding */
                attr[0].id = SAI_ROUTE_ATTR_PACKET_ACTION;
                attr[0].value.s32 = SAI_PACKET_ACTION_FORWARD;
                attr[1].id = SAI_ROUTE_ATTR_NEXT_HOP_ID;
                attr[1].value.oid = l3_id_cp.data;
                attr[2].id = SAI_ROUTE_ATTR_TRAP_PRIORITY;
                attr[2].value.u8 = 0;                       // default to zero

                status = sai_api->route_api->create_route(&route, 3, attr);
                SAI_ERROR_LOG_EXIT(status, "Failed to add route entry");
            }
            else if (ops_routep->n_nexthops)
            {
                /* update to ecmp  */
                rc = ops_sai_route_update(vrf_id, ops_routep, next_hop_count, next_hops, false);
                if (rc) {
                    goto exit;
                }

                HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops)
                {
                    /* need to add nexthop db or update refer cnt */
                    p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                    if (NULL == p_nh_entry) {
                        rc = ops_sai_routing_nexthop_create(NULL, ops_nh->id, &l3_egress_id);
                        if (rc) {
                            continue;
                        }
                    } else {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }

                    /* add the nexthop db or refer  */
                    for(index = 0; index < next_hop_count; index++)
                    {
                        if (0 == strcmp(ops_nh->id, next_hops[index])) {
                            ops_sai_route_add_nexthop(ops_nh->id, &l3_egress_id);
                        }
                    }
                }
		   if (OPS_ROUTE_STATE_ECMP != ops_routep->rstate) {
	                /* create nexthop group */
	                ops_sai_routing_nh_group_add(ops_routep, &l3_nhg_id);

	                ops_sai_routing_nexthop_id_update(&route, &l3_nhg_id);
			  ops_routep->nh_ecmp.data = l3_nhg_id.data;

			  goto exit;
		  }

		  /* add nexthop member */
		  for(index = 0; index < next_hop_count; index++){
			ops_sai_routing_nhg_add_member(ops_routep,next_hops[index]);
		  }

            }
        }
    } else {
        ops_routep = ops_sai_route_lookup(vrf_id, prefix);
        if (!ops_routep) {
            return status;
        }

        if (next_hops) {
            ops_sai_route_update(vrf_id, ops_routep, next_hop_count, next_hops, true);
            if (1 == ops_routep->n_nexthops) {
                HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops)
                {
                    p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                    if (NULL != p_nh_entry) {
                        l3_egress_id.data = p_nh_entry->handle.data;
                    }
                }

                /* update the nhid */
                ops_sai_routing_nexthop_id_update(&route, &l3_egress_id);

                /* delete the nhid after */
                if (OPS_ROUTE_STATE_ECMP == ops_routep->rstate) {
                    ops_sai_routing_nh_group_del(&ops_routep->nh_ecmp);
                }
            }
            else if (1 < ops_routep->n_nexthops) {

		  /* remove nexthop member */
		  for(index = 0; index < next_hop_count; index++){
			ops_sai_routing_nhg_del_member(ops_routep,next_hops[index]);
		  }

            } else if (ops_routep->refer_cnt) {
                /* del the ipuc prefix */
                status = sai_api->route_api->remove_route(&route);
                __ops_sai_route_local_add(&vrfid, prefix);
            }

            /* del the nexthop db or refer  */
            for(index = 0; index < next_hop_count; index++)
            {
                p_nh_entry = ops_sai_route_get_nexthop(next_hops[index]);
                if (NULL != p_nh_entry) {
                    l3_egress_id.data = p_nh_entry->handle.data;
                    rc = ops_sai_route_delete_nexthop(p_nh_entry->id);
                    if (!rc) {
                        rc = ops_sai_routing_nexthop_remove(&l3_egress_id);
                    }
                }
            }
        } else {
            /* del the ipuc prefix */
            status = sai_api->route_api->remove_route(&route);
            SAI_ERROR_LOG_EXIT(status, "Failed to delete route entry");

            if (OPS_ROUTE_STATE_ECMP == ops_routep->rstate) {
                ops_sai_routing_nh_group_del(&ops_routep->nh_ecmp);
            }

            /* del the nexthop db or refer  */
            HMAP_FOR_EACH(ops_nh, node, &ops_routep->nexthops)
            {
                p_nh_entry = ops_sai_route_get_nexthop(ops_nh->id);
                if (NULL != p_nh_entry) {
                    l3_egress_id.data = p_nh_entry->handle.data;
                    rc = ops_sai_route_delete_nexthop(p_nh_entry->id);
                    if (!rc) {
                        rc = ops_sai_routing_nexthop_remove(&l3_egress_id);
                    }
                }
            }

            ops_sai_route_del(ops_routep);
            return status;
        }
    }

exit:
    if (ops_routep->n_nexthops > 1) {
        ops_routep->rstate = OPS_ROUTE_STATE_ECMP;
    } else {
        ops_routep->rstate = OPS_ROUTE_STATE_NON_ECMP;
        ops_routep->nh_ecmp.data = 0;
    }

    return status;
}

/*
 *  Function for adding local route.
 *  Means while creating new routing interface and assigning IP address to it
 *  'tells' the hardware that new subnet is accessible through specified
 *  router interface.
 *
 * @param[in] vrid    - virtual router ID
 * @param[in] prefix  - IP prefix
 * @param[in] rifid   - router interface ID
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__route_local_add(const handle_t *vrid,
                  const char     *prefix,
                  const handle_t *rifid)
{
    sai_status_t    status      = SAI_STATUS_SUCCESS;
    int                 vrf_id      = 0;
    sai_ops_route_t     *ops_routep = NULL;

    ops_routep = ops_sai_route_lookup(vrf_id, prefix);
    if (NULL == ops_routep) {
        ops_routep = ops_sai_route_add(vrf_id, prefix, 0, NULL);
        if (NULL != ops_routep) {
            ops_routep->refer_cnt = 1;
        } else {
            return status;
        }

        __ops_sai_route_local_add(vrid, prefix);
        /* need to check when mask is 32 */

    } else {
        ops_routep->refer_cnt++;
    }

    return status;
}

/*
 *  Function for adding next hops(list of remote routes) which are accessible
 *  over specified IP prefix
 *
 * @param[in] vrid           - virtual router ID
 * @param[in] prefix         - IP prefix
 * @param[in] next_hop_count - count of next hops
 * @param[in] next_hops      - list of next hops
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__route_remote_add(handle_t           vrid,
                   const char        *prefix,
                   uint32_t           next_hop_count,
                   char *const *const next_hops)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint32_t        cmd_add = true;

    VLOG_INFO("Adding next hop(s) for remote route"
              "(prefix: %s, next hop count %u)", prefix, next_hop_count);

    ovs_assert(prefix);
    ovs_assert(next_hops);
    ovs_assert(next_hop_count);
    if (SAI_NEXT_HOP_MAX <= next_hop_count) {
        VLOG_ERR("The num of nexthops is overflow");
        goto exit;
    }

    status = __sai_route_remote_action(vrid.data, prefix, next_hop_count,
                                   next_hops, cmd_add);

    SAI_ERROR_LOG_EXIT(status, "Failed to add remote route"
                      "(prefix: %s, next hop count %u)",
                      prefix, next_hop_count);

exit:
    return status;
}

/*
 *  Function for deleting next hops(list of remote routes) which now are not
 *  accessible over specified IP prefix
 *
 * @param[in] vrid           - virtual router ID
 * @param[in] prefix         - IP prefix
 * @param[in] next_hop_count - count of next hops
 * @param[in] next_hops      - list of next hops
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__route_remote_nh_remove(handle_t           vrid,
                         const char        *prefix,
                         uint32_t           next_hop_count,
                         char *const *const next_hops)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint32_t        cmd_add = false;

    status = __sai_route_remote_action(vrid.data, prefix, next_hop_count,
                                   next_hops, cmd_add);

    return status;
}

/*
 *  Function for deleting remote route
 *
 * @param[in] vrid           - virtual router ID
 * @param[in] prefix         - IP prefix over which next hops can be accessed
 *
 * @notes if next hops were already configured earlier for this route then
 *        delete all next-hops of a route as well as the route itself.
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__route_remove(const handle_t *vrid, const char     *prefix)
{
    sai_status_t        status          = SAI_STATUS_SUCCESS;
    uint32_t            cmd_add         = false;
    int                 vrf_id          = 0;
    sai_ops_route_t     *ops_routep     = NULL;

    if (NULL == vrid || NULL == prefix)
    {
        return SAI_STATUS_FAILURE;
    }

    ops_routep = ops_sai_route_lookup(vrf_id, prefix);
    if (NULL != ops_routep) {
        if (ops_routep->refer_cnt) {
            if (ops_routep->n_nexthops) {
                if (1 <= ops_routep->refer_cnt) {
                    ops_routep->refer_cnt --;
                }
            } else {
                /* For route without nexthop of intf, and local multipath */
                if (1 < ops_routep->refer_cnt) {
                    ops_routep->refer_cnt --;
                } else if (1 == ops_routep->refer_cnt) {
                    /* for delete the local route */
                    __ops_sai_route_local_delete(vrid, prefix);
                    ops_sai_route_del(ops_routep);
                }
            }
        } else {
            if (ops_routep->n_nexthops) {
                /* for delete the remote route and nexthop  */
                status = __sai_route_remote_action(vrid->data, prefix, 0, NULL, cmd_add);
            }
        }
    }

    return status;
}

/*
 * De-initializes route.
 */
static void
__route_deinit(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

DEFINE_GENERIC_CLASS(struct route_class, route) = {
    .init = __route_init,
    .ip_to_me_add = __route_ip_to_me_add,
    .ip_to_me_delete = __route_ip_to_me_delete,
    .local_add = __route_local_add,
    .remote_add = __route_remote_add,
    .remote_nh_remove = __route_remote_nh_remove,
    .remove = __route_remove,
    .deinit = __route_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct route_class, route);
