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
 * File: sai-mirror.c
 *
 * Purpose: This file has code to manage mirrors/span sessions for
 *          CTC hardware.  It uses the sai interface for all
 *          hw related operations.
 */

#ifndef __BOOL_DEFINED
#define __BOOL_DEFINED
#endif

#include <netdev-provider.h>
#include <vlan-bitmap.h>
#include <ofproto/ofproto.h>

#include <sai-log.h>
#include <sai-lag.h>
#include <sai-api-class.h>
#include <sai-port.h>
#include <sai-mirror.h>

VLOG_DEFINE_THIS_MODULE(sai_mirror);

#define INTERNAL_ERROR          EFAULT      /* an internal inconsistency */
#define EXTERNAL_ERROR          ENXIO       /* wrong parameters passed in */
#define RESOURCE_ERROR          ENOMEM      /* out of required resources */

static int
__get_oid(sai_mirror_portid_t id, sai_mirror_porttype_t t, sai_object_id_t *oid)
{
    handle_t lag_hid;

    if (t == SAI_MIRROR_PORT_PHYSICAL)
        *oid = ops_sai_api_port_map_get_oid(id.hw_id);
    else if (!ops_sai_lag_get_handle_id(id.lag_id, &lag_hid))
        *oid = lag_hid.data;
    else
        return -1;

    return 0;

}

void __mirror_init(void)
{
}

/*
 * A mirror object can be destroyed in one of following ways:
 *
 * - mirror object itself is directly specified OR
 * - its name is specified OR
 * - the 'aux' is supplied OR
 * - the corresponding mirror endpoint 'mtp' is supplied
 *
 * The precedence is as listed above.  At least one parameter
 * is needed and should not be NULL.
 */
static void
__mirror_destroy(handle_t hid)
{
    const struct
        ops_sai_api_class   *sai_api;

    sai_api  = ops_sai_api_get_instance();
    sai_api->mirror_api->remove_mirror_session(hid.data);
}

static int
__mirror_create(handle_t *hid,
        sai_mirror_porttype_t dtype, sai_mirror_portid_t out_id)
{
    sai_attribute_t             mirror_attr[2];
    sai_object_id_t             oid;
    const struct
        ops_sai_api_class       *sai_api;

    sai_api = ops_sai_api_get_instance();

    mirror_attr[0].id = SAI_MIRROR_SESSION_ATTR_TYPE;
    mirror_attr[0].value.s32 = SAI_MIRROR_TYPE_LOCAL;
    mirror_attr[1].id = SAI_MIRROR_SESSION_ATTR_MONITOR_PORT;
    __get_oid(out_id, dtype, &oid);
    mirror_attr[1].value.oid = oid;

    return
        sai_api->mirror_api->create_mirror_session(&hid->data, 2, mirror_attr);
}

static int
__mirror_src_del(handle_t hid, int flux,
        sai_mirror_porttype_t stype, sai_mirror_portid_t portid)
{
    sai_attribute_t         port_attr;
    sai_object_id_t         oid;
    const struct
        ops_sai_api_class   *sai_api;

    sai_api = ops_sai_api_get_instance();

    port_attr.value.objlist.count = 0;
    port_attr.value.objlist.list = NULL;

    __get_oid(portid, stype, &oid);

    if (flux == SAI_MIRROR_DATA_DIR_INGRESS)
        port_attr.id = SAI_PORT_ATTR_INGRESS_MIRROR_SESSION;
    else
        port_attr.id = SAI_PORT_ATTR_EGRESS_MIRROR_SESSION;

    return sai_api->port_api->set_port_attribute(oid, &port_attr);
}

static int
__mirror_src_add(handle_t hid, int flux,
        sai_mirror_porttype_t stype, sai_mirror_portid_t portid)
{
    sai_attribute_t             port_attr;
    sai_object_id_t             oid;
    const struct
        ops_sai_api_class       *sai_api;

    sai_api = ops_sai_api_get_instance();

    /* enable/disable ingress/egress according to flux */
    port_attr.value.objlist.count = 1;
    port_attr.value.objlist.list = &hid.data;

    __get_oid(portid, stype, &oid);

    if (flux == SAI_MIRROR_DATA_DIR_INGRESS)
        port_attr.id = SAI_PORT_ATTR_INGRESS_MIRROR_SESSION;
    else
        port_attr.id = SAI_PORT_ATTR_EGRESS_MIRROR_SESSION;

    return sai_api->port_api->set_port_attribute(oid, &port_attr);
}

/*******************************************
  All codes below added according to
  sai plugin format
 *******************************************/
DEFINE_GENERIC_CLASS(struct mirror_class, mirror) = {
    .init = __mirror_init,
    .create = __mirror_create,
    .src_add = __mirror_src_add,
    .src_del = __mirror_src_del,
    .destroy = __mirror_destroy,
};

DEFINE_GENERIC_CLASS_GETTER(struct mirror_class, mirror);
