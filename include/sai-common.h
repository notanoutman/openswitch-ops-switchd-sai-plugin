/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_COMMON_H
#define SAI_COMMON_H 1

#include <string.h>
#include <saitypes.h>
#include <net/ethernet.h>
#include <hmap.h>
#include <sai-handle.h>

#define PROVIDER_INIT_GENERIC(NAME, VALUE)       .NAME = VALUE,

#ifdef OPS
#define PROVIDER_INIT_OPS_SPECIFIC(NAME, VALUE)  .NAME = VALUE,
#else
#define PROVIDER_INIT_OPS_SPECIFIC(NAME, VALUE)
#endif

#define __DEFINE_CLASS(_type, _name, _sufix) static _type _name##_sufix
#define __CLASS(_name, _prefix)             _name##_prefix
#define __CLASS_GETTER(_name, _sufix)       _name##_getter##_sufix

#define __DECLARE_CLASS_GETTER(_type, _name, _sufix) \
    _type *_name##_getter##_sufix()

#define __DEFINE_CLASS_GETTER(_type, _name, _sufix) \
    __DECLARE_CLASS_GETTER(_type, _name, _sufix)    \
{                                                   \
    return &__CLASS(_name, _sufix);                 \
}

#define DEFINE_GENERIC_CLASS(_type, _name) \
        __DEFINE_CLASS(_type, _name, __generic)

#define DEFINE_VENDOR_CLASS(_type, _name) \
    __DEFINE_CLASS(_type, _name, __vendor)

#define CLASS_GENERIC(_name)    __CLASS(_name, __generic)
#define CLASS_VENDOR(_name)     __CLASS(_name, __vendor)

#define DECLARE_GENERIC_CLASS_GETTER(_type, _name) \
    __DECLARE_CLASS_GETTER(_type, _name, __generic)

#define DEFINE_GENERIC_CLASS_GETTER(_type, _name)  \
    __DEFINE_CLASS_GETTER(_type, _name, __generic)

#define DECLARE_VENDOR_CLASS_GETTER(_type, _name) \
    __DECLARE_CLASS_GETTER(_type, _name, __vendor)

#define DEFINE_VENDOR_CLASS_GETTER(_type, _name)  \
    __DEFINE_CLASS_GETTER(_type, _name, __vendor)

#define CLASS_GENERIC_GETTER(_name)  __CLASS_GETTER(_name, __generic)
#define CLASS_VENDOR_GETTER(_name)   __CLASS_GETTER(_name, __vendor)

#define MAC_STR_LEN 17
#define VLAN_INTF_PREFIX    "vlan"
#define VLAN_ID_MIN         1
#define VLAN_ID_MAX         4094

#define STR_EQ(str1, str2)      (strcmp(str1, str2) == 0)

#define OPS_SAI_SET_FLAG(VAL,FLAG)          (VAL) = (VAL) | (FLAG)
#define OPS_SAI_UNSET_FLAG(VAL,FLAG)        (VAL) = (VAL) & ~(FLAG)
#define OPS_SAI_FLAG_ISSET(VAL,FLAG)        (((VAL) & (FLAG)) == (FLAG))

#define OPS_SAI_IS_BIT_SET(flag, bit)   (((flag) & (1 << (bit))) ? 1: 0)
#define OPS_SAI_SET_BIT(flag, bit)      (flag) = (flag) | (1 << (bit))
#define OPS_SAI_CLEAR_BIT(flag, bit)    (flag) = (flag) & (~(1 << (bit)))

struct ip_address {
    struct hmap_node addr_node;
    char *address;
};

struct neigbor_entry {
    struct   hmap_node neigh_node;
    char     *mac_address;
    char     *ip_address;
};

typedef enum sai_mirror_porttype {
    SAI_MIRROR_PORT_PHYSICAL,
    SAI_MIRROR_PORT_LAG,
} sai_mirror_porttype_t;

typedef union sai_mirror_portid {
    int                 lag_id;
    uint32_t            hw_id;
} sai_mirror_portid_t;

int
ops_sai_vlan_intf_update(int vid, bool add);

#endif /* SAI_COMMON_H */
