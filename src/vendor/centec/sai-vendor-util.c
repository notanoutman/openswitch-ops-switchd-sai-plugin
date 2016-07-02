/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-vendor-util.h>
#include <sai-log.h>

#include <packets.h>
#include <socket-util.h>
#include <netinet/in.h>

#include <util.h>

VLOG_DEFINE_THIS_MODULE(mlnx_sai_util);

/*
 * Converts string representation of IP prefix into SX SDK format.
 *
 * @param[in] prefix     - IPv4/IPv6 prefix in string representation.
 * @param[out] sx_prefix - IPv4/IPv6 prefix in format used by SX SDK.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
#if 0
int ops_sai_common_ip_prefix_to_sx_ip_prefix(const char *prefix,
                                             sx_ip_prefix_t *sx_prefix)
{
    int                i = 0;
    int                error = 0;
    uint32_t           *addr_chunk = NULL;
    uint32_t           *mask_chunk = NULL;
    char               *error_msg = NULL;

    memset(sx_prefix, 0, sizeof(*sx_prefix));

    if (addr_is_ipv6(prefix)) {
        sx_prefix->version = SX_IP_VERSION_IPV6;
        error_msg = ipv6_parse_masked(prefix,
                                      (struct in6_addr*)
                                      &sx_prefix->prefix.ipv6.addr,
                                      (struct in6_addr*)
                                      &sx_prefix->prefix.ipv6.mask);
        if (!error_msg) {
            /* SDK IPv6 is 4*uint32. Each uint32 is in host order.
             * Between uint32s there is network byte order */
            addr_chunk = sx_prefix->prefix.ipv6.addr.s6_addr32;
            mask_chunk = sx_prefix->prefix.ipv6.mask.s6_addr32;

            for (i = 0; i < 4; ++i) {
                addr_chunk[i] = ntohl(addr_chunk[i]);
                mask_chunk[i] = ntohl(mask_chunk[i]);
            }
        }
    } else {
        sx_prefix->version = SX_IP_VERSION_IPV4;
        error_msg = ip_parse_masked(prefix,
                                    (ovs_be32*)
                                    &sx_prefix->prefix.ipv4.addr.s_addr,
                                    (ovs_be32*)
                                    &sx_prefix->prefix.ipv4.mask.s_addr);
        if (!error_msg) {
            /* SDK IPv4 is in host order*/
            sx_prefix->prefix.ipv4.addr.s_addr =
                    ntohl(sx_prefix->prefix.ipv4.addr.s_addr);
            sx_prefix->prefix.ipv4.mask.s_addr =
                    ntohl(sx_prefix->prefix.ipv4.mask.s_addr);
        }
    }

    if (NULL != error_msg) {
        error = -1;
        ERRNO_LOG_EXIT(error, "%s", error_msg);
    }

exit:
    if (error_msg) {
        free(error_msg);
    }

    return error;
}


/*
 * Converts string representation of IP address into SX SDK format.
 *
 * @param[in] ip     - IPv4/IPv6 address in string representation.
 * @param[out] sx_ip - IPv4/IPv6 address in format used by SX SDK.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
int ops_sai_common_ip_to_sx_ip(const char *ip, sx_ip_addr_t *sx_ip)
{
    int                i = 0;
    int                error = 0;
    uint8_t            family = AF_INET;
    uint32_t           *addr_chunk = NULL;

    if (addr_is_ipv6(ip)){
        family = AF_INET6;
        sx_ip->version = SX_IP_VERSION_IPV6;
    } else {
        sx_ip->version = SX_IP_VERSION_IPV4;
    }

    /* inet_pton return 1 on success */
    if (1 != inet_pton(family, ip, (void*)&sx_ip->addr)) {
        error = -1;
        ERRNO_LOG_EXIT(error, "Invalid IP address: %s", ip);
    }

    if (sx_ip->version == SX_IP_VERSION_IPV6) {
        /* SDK IPv6 is 4*uint32. Each uint32 is in host order.
         * Between uint32s there is network byte order */
        addr_chunk = sx_ip->addr.ipv6.s6_addr32;

        for (i = 0; i < 4; ++i) {
            addr_chunk[i] = ntohl(addr_chunk[i]);
        }
    } else {
        /* SDK IPv4 is in host order*/
        sx_ip->addr.ipv4.s_addr = ntohl(sx_ip->addr.ipv4.s_addr);
    }

exit:
    return error;
}
#endif
/**
 * Read base MAC address from EEPROM.
 * @param[out] mac pointer to MAC buffer.
 * @return sai_status_t.
 */
sai_status_t
ops_sai_vendor_base_mac_get(sai_mac_t mac)
{
	sai_status_t status = SAI_STATUS_SUCCESS;

	mac[0] = 0x11;
	mac[1] = 0x22;
	mac[2] = 0x33;
	mac[3] = 0x44;
	mac[4] = 0x55;
	mac[5] = 0x66;

	return status;
}
