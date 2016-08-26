/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-neighbor.h>
#include <sai-common.h>
#include <sai-api-class.h>
#include <sai-route.h>
#include <arpa/inet.h>
#include <netinet/ether.h>

VLOG_DEFINE_THIS_MODULE(sai_neighbor);

/*
 * Initializes neighbor.
 */
static void
__neighbor_init(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

/*
 *  This function adds a neighbour information.
 *
 * @param[in] is_ipv6_addr - is address IPv6 or not
 * @param[in] ip_addr      - neighbor IP address
 * @param[in] mac_addr     - neighbor MAC address
 * @param[in] rif          - router Interface ID
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__neighbor_create(bool           is_ipv6_addr,
                  const char     *ip_addr,
                  const char     *mac_addr,
                  const handle_t *rif,
                  int            *l3_id)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_status_t            status      = SAI_STATUS_SUCCESS;
    struct nh_entry         *p_nh_entry = NULL;
    sai_neighbor_entry_t    sai_neighbor;
    sai_attribute_t         attr[1];
    char                    buf[64];
    int                     retv;
    int                     rc;
    uint32_t                ip_prefix;
    handle_t                l3_egress_id;

    if (NULL == mac_addr || NULL == ip_addr) {
        return SAI_STATUS_FAILURE;
    }

    memset(&sai_neighbor, 0 , sizeof(sai_neighbor));
    memset(&ip_prefix, 0, sizeof(ip_prefix));
    memset(buf, 0, sizeof(buf));
    retv = inet_pton(AF_INET, ip_addr, buf);
    if (!retv) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&ip_prefix, buf, sizeof(ip_prefix));

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_NEIGHBOR_ATTR_DST_MAC_ADDRESS;

    struct ether_addr *ether_mac = ether_aton(mac_addr);
    memcpy(attr[0].value.mac, ether_mac, sizeof(attr[0].value.mac));
    sai_neighbor.rif_id = rif->data;
    if (!is_ipv6_addr)
    {
        sai_neighbor.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        sai_neighbor.ip_address.addr.ip4 = htonl(ip_prefix);
    }
    else
    {
        /* TO SUPPORT */
    }

    p_nh_entry = ops_sai_neighbor_get_nexthop(ip_addr);
    if (NULL == p_nh_entry) {
        rc = ops_sai_routing_nexthop_create(rif, ip_addr, &l3_egress_id);
        if (rc) {
            goto exit;
        }
    }
    else {
        l3_egress_id.data = p_nh_entry->handle.data;
    }

    ops_sai_neighbor_add_nexthop(ip_addr, &l3_egress_id);
    *l3_id = l3_egress_id.data;

    status = sai_api->neighbor_api->create_neighbor_entry(&sai_neighbor, 1, attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to create host entry");

exit:
    return SAI_STATUS_SUCCESS;
}

/*
 *  This function deletes a neighbour information.
 *
 * @param[in] is_ipv6_addr - is address IPv6 or not
 * @param[in] ip_addr      - neighbor IP address
 * @param[in] rif          - router Interface ID
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__neighbor_remove(bool is_ipv6_addr, const char *ip_addr, const handle_t *rif)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    struct nh_entry     *p_nh_entry = NULL;
    sai_neighbor_entry_t neighbor;
    char                buf[64];
    int                 retv    = 0;
    int                 rc      = 0;
    uint32_t            ip_prefix;
    handle_t            l3_egress_id;

    if (NULL == ip_addr) {
        return SAI_STATUS_FAILURE;
    }

    memset(&neighbor, 0, sizeof(neighbor));
    memset(&ip_prefix, 0, sizeof(ip_prefix));
    memset(buf, 0, sizeof(buf));
    retv = inet_pton(AF_INET, ip_addr, buf);
    if (!retv) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&ip_prefix, buf, sizeof(ip_prefix));

    neighbor.rif_id = rif->data;
    neighbor.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor.ip_address.addr.ip4 = htonl(ip_prefix);

    status = sai_api->neighbor_api->remove_neighbor_entry(&neighbor);
    SAI_ERROR_LOG_EXIT(status, "Failed to remove host entry");

    p_nh_entry = ops_sai_neighbor_get_nexthop(ip_addr);
    if (NULL != p_nh_entry) {
        l3_egress_id.data = p_nh_entry->handle.data;
        rc = ops_sai_neighbor_delete_nexthop(ip_addr);
        if (!rc) {
            rc = ops_sai_routing_nexthop_remove(&l3_egress_id);
        }
    }

exit:
    return status;
}

/*
 *  This function reads the neighbor's activity information.
 *
 * @param[in] is_ipv6_addr - is address IPv6 or not
 * @param[in] ip_addr      - neighbor IP address
 * @param[in] rif          - router Interface ID
 * @param[out] activity_p  - activity
 *
 * @return 0  if operation completed successfully.
 * @return -1 if operation failed.*/
static int
__neighbor_activity_get(bool           is_ipv6_addr,
                        const char     *ip_addr,
                        const handle_t *rif,
                        bool           *activity)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
    return 0;
}

/*
 * De-initializes neighbor.
 */
static void
__neighbor_deinit(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

DEFINE_GENERIC_CLASS(struct neighbor_class, neighbor) = {
    .init = __neighbor_init,
    .create = __neighbor_create,
    .remove = __neighbor_remove,
    .activity_get = __neighbor_activity_get,
    .deinit = __neighbor_deinit
};

DEFINE_GENERIC_CLASS_GETTER(struct neighbor_class, neighbor);
