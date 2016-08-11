/*
 * Copyright (C) 2016 Hewlett-Packard Enterprise Company, L.P.
 * All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 *
 * File: sai-mirror.h
 *
 * Purpose: This file has code to manage mirrors/span sessions for
 *          CTC hardware.  It uses the sai interface for all
 *          hw related operations.
 */

#ifndef SAI_MIRRORS_H
#define SAI_MIRRORS_H 1

#include <inttypes.h>
#include <errno.h>
#include <sai.h>
#include <sai-common.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

#define SAI_MIRROR_DATA_DIR_INGRESS     0x01
#define SAI_MIRROR_DATA_DIR_EGRESS      0x02
#define SAI_MIRROR_DATA_DIR_BOTH        0x03

typedef enum sai_mirror_porttype {
    SAI_MIRROR_PORT_PHYSICAL,
    SAI_MIRROR_PORT_LAG,
} sai_mirror_porttype_t;

typedef union sai_mirror_portid {
    int                 lag_id;
    uint32_t            hw_id;
} sai_mirror_portid_t;

struct mirror_class {
    /**
     * Initialize sai mirror module.
     */
    void (*init)(void);

    /*
     * detroy the specified mirror session by aux
     */
    void (*destroy)(handle_t hid);

    /*
     * create a new mirror session ocuppying specified name, aux and destination

     * \param aux, key of mirror session, it is actually an `struct mirror *`
     *                    object from bridge->mirrors
     * \param name, mirror session name
     * \param desttype, destination port type, its value is of type `sai_mirror_desttype_t`
     * \param hw_id, this can be physical port id or lag id
     */
    int (*create)(handle_t *hid,
            sai_mirror_porttype_t desttype, sai_mirror_portid_t out_id);

    /*
     * add a source to the specified mirror session by aux
     *
     * \param aux, key of mirror session you passed to `create` method, it is actually
     *                    an `struct mirror *` object from bridge->mirrors
     * \param portkey, key fo the source port which would be used to find the specified port
     *                         in a mirror session
     * \param st, source port type, its value is of type `sai_mirror_srctype_t`
     * \param hw_id, this can be physical port id or lag id
     */
    int (*src_add)(handle_t hid, int flux,
            sai_mirror_porttype_t st, sai_mirror_portid_t portid);

    int (*src_del)(handle_t hid, int flux,
            sai_mirror_porttype_t st, sai_mirror_portid_t portid);
};

DECLARE_GENERIC_CLASS_GETTER(struct mirror_class, mirror);

#define ops_sai_mirror_class_generic() (CLASS_GENERIC_GETTER(mirror)())

#ifndef ops_sai_mirror_class
#define ops_sai_mirror_class           ops_sai_mirror_class_generic
#endif

static inline void ops_sai_mirror_init(void)
{
    ovs_assert(ops_sai_mirror_class()->init);
    ops_sai_mirror_class()->init();
}

static inline int ops_sai_mirror_src_add(handle_t hid, int flux,
            sai_mirror_porttype_t st, sai_mirror_portid_t portid)
{
    ovs_assert(ops_sai_mirror_class()->src_add);
    return ops_sai_mirror_class()->src_add(hid, flux, st, portid);
}

static inline int ops_sai_mirror_src_del(handle_t hid, int flux,
            sai_mirror_porttype_t st, sai_mirror_portid_t portid)
{
    ovs_assert(ops_sai_mirror_class()->src_del);
    return ops_sai_mirror_class()->src_del(hid, flux, st, portid);
}

static inline void ops_sai_mirror_destroy(handle_t hid)
{
    ovs_assert(ops_sai_mirror_class()->destroy);
    ops_sai_mirror_class()->destroy(hid);
}

static inline int ops_sai_mirror_create(handle_t *hid,
            sai_mirror_porttype_t desttype, sai_mirror_portid_t out_id)
{
    ovs_assert(ops_sai_mirror_class()->create);
    return ops_sai_mirror_class()->create(hid, desttype, out_id);
}

#endif /* SAI_MIRRORS_H */
