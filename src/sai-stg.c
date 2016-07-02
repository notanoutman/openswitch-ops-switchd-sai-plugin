/*
 * Copyright (C) 2015-2016 Hewlett-Packard Development Company, L.P.
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
 * File: ops-stg.c
 *
 */
/* by xuwj */
#define __BOOL_DEFINED
/* end */
#include <stdio.h>
#include <stdlib.h>

#include <openvswitch/vlog.h>
#include <ofproto/ofproto.h>
//#include "netdev-bcmsdk.h"
//#include "ops-debug.h"
#include <sai-api-class.h>
#include "sai-port.h"
#include "sai-vlan.h"
#include "sai-stg.h"
#include "sai-log.h"
#include <sai-netdev.h>
//#include "platform-defines.h"

VLOG_DEFINE_THIS_MODULE(ops_stg);

#define OPS_VLAN_MIN        0
#define OPS_VLAN_MAX        4096
#define MAX_HW_PORTS        256
#define OPS_VLAN_VALID(v)  ((v)>OPS_VLAN_MIN && (v)<OPS_VLAN_MAX)

#define CTC_SAI_OBJECT_TYPE_SET(objtype,index)  					\
    (((sai_object_id_t)objtype << 32) | (sai_object_id_t)index)

#define CTC_SAI_OBJECT_TYPE_GET(objid)  							\
    ((objid >> 32) & 0xFF)

#define CTC_SAI_OBJECT_INDEX_GET(objid)  							\
    (objid & 0xFFFFFFFF)

unsigned int stp_enabled = 0;
unsigned int ops_stg_count = 0;
ops_stg_data_t *ops_stg[OPS_STG_COUNT+2] = { NULL };

int
ops_sai_api_sai_stp_id2stp_id(sai_object_id_t sai_stp_id)
{
    return CTC_SAI_OBJECT_INDEX_GET(sai_stp_id);
}
#if 0
sai_object_id_t
ops_sai_api_stp_id2sai_stp_id(int stp_id)
{
    sai_object_id_t temp_id;

    temp_id = CTC_SAI_OBJECT_TYPE_SET(SAI_OBJECT_TYPE_STP_INSTANCE,stp_id);

    return temp_id;
}
#endif
sai_object_id_t
ops_sai_api_stp_id2sai_stp_id(int stp_id,sai_object_id_t* sai_stp_id)
{
    sai_object_id_t temp_id;

    temp_id = CTC_SAI_OBJECT_TYPE_SET(SAI_OBJECT_TYPE_STP_INSTANCE,stp_id);

    *sai_stp_id = temp_id;

    return temp_id;
}

#if 0
////////////////////////////////// DEBUG ///////////////////////////////////
/*-----------------------------------------------------------------------------
| Function: show_stg_vlan_data
| Description:  displays ops_stg_data object  vlan details
| Parameters[in]: ops_stg_data_t object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/

static void
show_stg_vlan_data(struct ds *ds, ops_stg_data_t *pstg)
{
    ops_stg_vlan_t *p_stg_vlan = NULL, *p_next_stg_vlan = NULL;

    if (!ds || !pstg) {
       /* invalid param */
       VLOG_ERR("%s: invalid param", __FUNCTION__);
       return;
    }

    ds_put_format(ds, "Vlan Count %d:\n", pstg->n_vlans);
    ds_put_format(ds, "Vlan  id's: ");
    HMAP_FOR_EACH_SAFE (p_stg_vlan, p_next_stg_vlan, node, &pstg->vlans) {
        ds_put_format(ds, " %d", p_stg_vlan->vlan_id);
    }
    ds_put_format(ds, "\n");

}


////////////////////////////////// DEBUG ///////////////////////////////////
/*-----------------------------------------------------------------------------
| Function: show_stg_data
| Description:  displays ops_stg_data object details
| Parameters[in]: ops_stg_data_t object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/

static void
show_stg_data(struct ds *ds, ops_stg_data_t *pstg)
{
    int unit;
    char pfmt[_SHR_PBMP_FMT_LEN];

    if (!ds || !pstg) {
        /* invalid param */
        VLOG_ERR("%s: invalid param", __FUNCTION__);
        return;
    }

    ds_put_format(ds, "STG %d:\n", pstg->stg_id);
    show_stg_vlan_data(ds, pstg);
    for (unit = 0; unit <= MAX_SWITCH_UNIT_ID; unit++) {
        ds_put_format(ds, "  disabled ports=%s\n",
                      _SHR_PBMP_FMT(pstg->disabled_ports[unit], pfmt));
        ds_put_format(ds, "  blocked ports=%s\n",
                      _SHR_PBMP_FMT(pstg->blocked_ports[unit], pfmt));
        ds_put_format(ds, "  learning ports=%s\n",
                      _SHR_PBMP_FMT(pstg->learning_ports[unit], pfmt));
        ds_put_format(ds, "  forwarding ports=%s\n",
                      _SHR_PBMP_FMT(pstg->forwarding_ports[unit], pfmt));
        ds_put_format(ds, "\n");
    }
    ds_put_format(ds, "\n");

}
#endif

/*------------------------------------------------------------------------------
| Function:  get_hw_port_state_enum_from_port_state
| Description: get the hw port state equivalent enum from port state
| Parameters[in]: portstate
| Parameters[out]: hw_port_state:
| Return: True if valid port state else false.
-----------------------------------------------------------------------------*/
bool
get_hw_port_state_enum_from_port_state(int port_state, int *hw_port_state)
{
    bool retval = false;

    if(!hw_port_state) {
        return retval;
    }

    switch (port_state) {
        case OPS_STG_PORT_STATE_BLOCKED:
            *hw_port_state = SAI_PORT_STP_STATE_BLOCKING;
            retval = true;
            break;
        case OPS_STG_PORT_STATE_DISABLED:
            *hw_port_state = OPS_STG_PORT_STATE_BLOCKED; /* OPENNSL_STG_STP_DISABLE */;
            retval = true;
            break;
        case OPS_STG_PORT_STATE_LEARNING:
            *hw_port_state = SAI_PORT_STP_STATE_LEARNING;
            retval = true;
            break;
        case OPS_STG_PORT_STATE_FORWARDING:
            *hw_port_state = SAI_PORT_STP_STATE_FORWARDING;
            retval = true;
            break;
        default:
            break;
    }

    return retval;
}

/*------------------------------------------------------------------------------
| Function:  get_port_state_from_hw_port_state
| Description: get the port state equivalent from hw port state
| Parameters[in]: hw_portstate
| Parameters[out]: port_state:-
| Return: True if valid port state else false.
-----------------------------------------------------------------------------*/
bool
get_port_state_from_hw_port_state(int hw_port_state, int *port_state)
{
    bool retval = false;

    if(!port_state) {
        return retval;
    }

    switch (hw_port_state) {
        case SAI_PORT_STP_STATE_BLOCKING:
            *port_state = OPS_STG_PORT_STATE_BLOCKED;
            retval = true;
            break;
#if 0
        case OPENNSL_STG_STP_DISABLE:
            *port_state =  OPS_STG_PORT_STATE_DISABLED;
            retval = true;
            break;
#endif
        case SAI_PORT_STP_STATE_LEARNING:
            *port_state =  OPS_STG_PORT_STATE_LEARNING;
            retval = true;
            break;
        case SAI_PORT_STP_STATE_FORWARDING:
            *port_state =  OPS_STG_PORT_STATE_FORWARDING;
            retval = true;
            break;
        default:
            *port_state = OPS_STG_PORT_STATE_NOT_SET;
            retval = false;
            break;
    }

    return retval;
}
#if 0
/*-----------------------------------------------------------------------------
| Function: ops_stg_dump
| Description:  dumps all stg groups data
| Parameters[in]: stgid: spanning tree group id
|.Parameters[out]:  dynamic string object
| Return: None
-----------------------------------------------------------------------------*/

void
ops_stg_dump(struct ds *ds, int stgid)
{
    if (OPS_STG_VALID(stgid)) {
        if (ops_stg[stgid] != NULL) {
            show_stg_data(ds, ops_stg[stgid]);
        } else {
            ds_put_format(ds, "STG %d does not exist.\n", stgid);
        }
    } else {
        int stgid, count;
        ds_put_format(ds, "Dumping all STGs (count=%d)...\n", ops_stg_count);
        for (stgid=0, count=0; stgid<=OPS_STG_COUNT; stgid++) {
            if (ops_stg[stgid] != NULL) {
                count++;
                show_stg_data(ds, ops_stg[stgid]);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_hw_dump
| Description:  dumps all stg groups data from hw
| Parameters[in]: stgid: spanning tree group id
|.Parameters[out]:  dynamic string object
| Return: None
-----------------------------------------------------------------------------*/

void
ops_stg_hw_dump(struct ds *ds, int stgid)
{
    int unit = 0;
    char pfmt[_SHR_PBMP_FMT_LEN];
    opennsl_vlan_t *vlan_list = NULL;
    int stg_vlan_count = 0;
    opennsl_error_t rc = OPENNSL_E_NONE;
    opennsl_pbmp_t disabled_ports[MAX_SWITCH_UNITS];
    opennsl_pbmp_t blocked_ports[MAX_SWITCH_UNITS];
    opennsl_pbmp_t learning_ports[MAX_SWITCH_UNITS];
    opennsl_pbmp_t forwarding_ports[MAX_SWITCH_UNITS];
    opennsl_port_t portid;
    opennsl_stg_t default_stg;

    int port_state = -1, hw_port_state = -1;


    if (!ds) {
        /* invalid param */
        VLOG_ERR("%s: invalid param", __FUNCTION__);
        return;
    }

    /* check range of hw stg id */
    if ((stgid < 0) || (stgid > 511)) {
        ds_put_format(ds, "Invalid hw stg id %d", stgid);
        return;
    }

    ds_put_format(ds, "STG %d:\n", stgid);
    rc = opennsl_stg_vlan_list(unit, stgid, &vlan_list, &stg_vlan_count);
    if (OPENNSL_FAILURE(rc)) {
        ds_put_format(ds, "Unit %d, stg get vlan error, rc=%d (%s)\n",
                 unit, rc, opennsl_errmsg(rc));
        return ;
    }

    ds_put_format(ds, "Vlan count %d:\n", stg_vlan_count);
    if (stg_vlan_count) {
        ds_put_format(ds, "Vlan  id's: ");
        for(int i =0; i<stg_vlan_count ; i++) {
            ds_put_format(ds, " %d", vlan_list[i]);
        }
        rc = OPENNSL_E_NONE;
        rc = opennsl_stg_vlan_list_destroy(unit, vlan_list, stg_vlan_count);
        if (OPENNSL_FAILURE(rc)) {
            ds_put_format(ds, "Unit %d, stg destroy vlan list error, rc=%d (%s)\n",
                     unit, rc, opennsl_errmsg(rc));
            return ;
        }
        ds_put_format(ds, "\n");
    }
    for (unit = 0; unit <= MAX_SWITCH_UNIT_ID; unit++) {

        OPENNSL_PBMP_CLEAR(disabled_ports[unit]);
        OPENNSL_PBMP_CLEAR(blocked_ports[unit]);
        OPENNSL_PBMP_CLEAR(learning_ports[unit]);
        OPENNSL_PBMP_CLEAR(forwarding_ports[unit]);

        for (portid =0; portid <=OPS_STG_ENTRY_PORT_WIDTH; portid++) {
            hw_port_state = -1;
            port_state = -1;
            opennsl_stg_stp_get(unit, stgid, portid, &hw_port_state);
            get_port_state_from_hw_port_state(hw_port_state, &port_state);
            switch (port_state) {
                case OPS_STG_PORT_STATE_BLOCKED:
                    OPENNSL_PBMP_PORT_ADD(blocked_ports[unit],
                                          portid);
                    break;
                case OPS_STG_PORT_STATE_DISABLED:
                    OPENNSL_PBMP_PORT_ADD(disabled_ports[unit],
                                          portid);
                    break;
                case OPS_STG_PORT_STATE_LEARNING:
                    OPENNSL_PBMP_PORT_ADD(learning_ports[unit],
                                          portid);
                    break;
                case OPS_STG_PORT_STATE_FORWARDING:
                    OPENNSL_PBMP_PORT_ADD(forwarding_ports[unit],
                                          portid);
                    break;
                default:
                    break;
            }
        }

        ds_put_format(ds, "  disabled ports=%s\n",
                      _SHR_PBMP_FMT(disabled_ports[unit], pfmt));
        ds_put_format(ds, "  blocked ports=%s\n",
                      _SHR_PBMP_FMT(blocked_ports[unit], pfmt));
        ds_put_format(ds, "  learning ports=%s\n",
                      _SHR_PBMP_FMT(learning_ports[unit], pfmt));
        ds_put_format(ds, "  forwarding ports=%s\n",
                      _SHR_PBMP_FMT(forwarding_ports[unit], pfmt));
        ds_put_format(ds, "\n");
    }

    ops_stg_default_get(&default_stg);
    ds_put_format(ds, "Default STG %d\n", default_stg);
    ds_put_format(ds, "\n");

}
#endif
/*-----------------------------------------------------------------------------
| Function: stg_data_find_create
| Description: lookup stg data for given stgid, if not found create stg data for given stgid
| Parameters[in]: stgid: spanning tree group id
| Parameters[in]:bool : true to create stg data, else do only lookup
| Parameters[out]:
| Return: ops_stg_data object
-----------------------------------------------------------------------------*/
static ops_stg_data_t *
stg_data_find_create(int stgid, bool create)
{
    ops_stg_data_t *p_stg_data = NULL;

    if (ops_stg[stgid] != NULL) {
        return ops_stg[stgid];
    }

    if (false == create) {
        return NULL;
    }

    // STG Entry data hasn't been created yet.
    p_stg_data = xzalloc(sizeof(ops_stg_data_t));
    if (!p_stg_data) {
        VLOG_ERR("Failed to allocate memory for STG id =%d",
                 stgid);
        return NULL;
    }

    p_stg_data->stg_id = stgid;

    ops_stg[stgid] = p_stg_data;
    ops_stg_count++;

    p_stg_data->n_vlans = 0;
    /* initialize stg entry vlan hash map */
    hmap_init(&p_stg_data->vlans);

    // Initialize member port bitmaps
    p_stg_data->pbmp_disabled_ports      = bitmap_allocate(MAX_HW_PORTS);
    p_stg_data->pbmp_blocked_ports       = bitmap_allocate(MAX_HW_PORTS);
    p_stg_data->pbmp_learning_ports      = bitmap_allocate(MAX_HW_PORTS);
    p_stg_data->pbmp_forwarding_ports    = bitmap_allocate(MAX_HW_PORTS);

    return p_stg_data;

} // stg_data_get

/*-----------------------------------------------------------------------------
| Function: stg_data_free
| Description: free stg group data
| Parameters[in]: stgid: spanning tree group id
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
static void
stg_data_free(int stgid)
{
    ops_stg_data_t *p_stg_data = NULL;
    ops_stg_vlan_t *p_stg_vlan, *p_next_stg_vlan;

    p_stg_data = ops_stg[stgid];
    if (!p_stg_data) {
        VLOG_ERR("Trying to free non-existent STG data (stgid=%d)!",
                 stgid);
        return;
    }

    HMAP_FOR_EACH_SAFE(p_stg_vlan, p_next_stg_vlan, node, &p_stg_data->vlans) {
        hmap_remove(&p_stg_data->vlans, &p_stg_vlan->node);
        free(p_stg_vlan);
        p_stg_data->n_vlans--;
    }

    bitmap_free(p_stg_data->pbmp_disabled_ports);
    bitmap_free(p_stg_data->pbmp_blocked_ports);
    bitmap_free(p_stg_data->pbmp_learning_ports);
    bitmap_free(p_stg_data->pbmp_forwarding_ports);

    free(p_stg_data);

    ops_stg[stgid] = NULL;
    ops_stg_count--;


}

/*-----------------------------------------------------------------------------
| Function: stg_data_add_vlan
| Description: add vlan to stg data
| Parameters[in]:  stgid: spanning tree group id
| Parameters[out]: vlanid:
| Return: None
-----------------------------------------------------------------------------*/
static void
stg_data_add_vlan(int stgid, int vlanid)
{
    ops_stg_data_t *p_stg_data = NULL;
    ops_stg_vlan_t *p_stg_vlan = NULL;

    p_stg_data = ops_stg[stgid];
    if (!p_stg_data) {
        VLOG_ERR("Trying to add vlan to  non-existent STG data (stgid=%d)!",
                 stgid);
        return;
    }

    HMAP_FOR_EACH_WITH_HASH(p_stg_vlan, node, hash_int(vlanid, 0),
                            &p_stg_data->vlans) {
        if (vlanid == p_stg_vlan->vlan_id){
            break;
        }
    }

    if(p_stg_vlan) {
        VLOG_DBG("vlan id %d found in stg id %d", p_stg_vlan->vlan_id,
                  p_stg_data->stg_id);
        return;
    }

    p_stg_vlan = xzalloc(sizeof(*p_stg_vlan));
    if (!p_stg_vlan) {
        VLOG_ERR("Failed to allocate memory for vlan id =%d, in STG %d",
                 vlanid, stgid);
        return;
    }
    p_stg_vlan->vlan_id = vlanid;

    hmap_insert(&p_stg_data->vlans, &p_stg_vlan->node, hash_int(vlanid, 0));
    p_stg_data->n_vlans++;
    VLOG_DBG("vlan id %d added to stg id %d", p_stg_vlan->vlan_id,
             p_stg_data->stg_id);
}

/*-----------------------------------------------------------------------------
| Function: stg_data_remove_vlan
| Description: removes vlan fron stg group
| Parameters[in]: stgid: spanning tree grouop id
| Parameters[in]: vlanid
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/

static void
stg_data_remove_vlan(int stgid, int vlanid)
{
    ops_stg_data_t *p_stg_data = NULL;
    ops_stg_vlan_t *p_stg_vlan = NULL;

    p_stg_data = ops_stg[stgid];
    if (!p_stg_data) {
        VLOG_ERR("Trying to remove vlan from non-existent STG data (stgid=%d)",
                 stgid);
        return;
    }

    HMAP_FOR_EACH_WITH_HASH(p_stg_vlan, node, hash_int(vlanid, 0),
                            &p_stg_data->vlans) {
        if (vlanid == p_stg_vlan->vlan_id){
            break;
        }
    }

    if(!p_stg_vlan) {
        VLOG_DBG("vlan id %d not found in stg id %d", vlanid, p_stg_data->stg_id);
        return;
    }

    VLOG_DBG("Delete vlan id %d from stg id %d", p_stg_vlan->vlan_id,
              p_stg_data->stg_id);
    hmap_remove(&p_stg_data->vlans, &p_stg_vlan->node);
    free(p_stg_vlan);
    p_stg_data->n_vlans--;
}

/*-----------------------------------------------------------------------------
| Function: stg_data_set_port_state
| Description: set port state in spanning tree group
| Parameters[in]:stgid: spanning tree group id
| Parameters[in]:portid
| Parameters[in]: port_state.
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
static void
stg_data_set_port_state(int stgid, int portid,
                           ops_stg_port_state_t port_state)
{
    ops_stg_data_t *p_stg_data = NULL;
//    int unit = 0;
    int hw_port;

    p_stg_data = ops_stg[stgid];
    if (!p_stg_data) {
        VLOG_ERR("Trying to set port sate for portid %d"
                 "to non-existent STG data (stgid=%d)!",
                 portid, stgid);
        return;
    }

    /* remove port from all port map list. */
    BITMAP_FOR_EACH_1 (hw_port, MAX_HW_PORTS, p_stg_data->pbmp_blocked_ports)
    {
        if (portid == hw_port) {
            bitmap_set0(p_stg_data->pbmp_blocked_ports,hw_port);

            VLOG_DBG("port id %d removed from blocked port"
                         "list of stg id %d",
                         portid, stgid);
        }
    }

    BITMAP_FOR_EACH_1 (hw_port, MAX_HW_PORTS, p_stg_data->pbmp_disabled_ports)
    {
        if (portid == hw_port) {
            bitmap_set0(p_stg_data->pbmp_disabled_ports,hw_port);

            VLOG_DBG("port id %d removed from disabled port"
                         "list of stg id %d",
                        portid, stgid);
        }
    }

    BITMAP_FOR_EACH_1 (hw_port, MAX_HW_PORTS, p_stg_data->pbmp_learning_ports)
    {
        if (portid == hw_port) {
            bitmap_set0(p_stg_data->pbmp_learning_ports,hw_port);

            VLOG_DBG("port id %d removed from learning port"
                         "list of stg id %d",
                        portid, stgid);
        }
    }

    BITMAP_FOR_EACH_1 (hw_port, MAX_HW_PORTS, p_stg_data->pbmp_forwarding_ports)
    {
        if (portid == hw_port) {
            bitmap_set0(p_stg_data->pbmp_forwarding_ports,hw_port);

            VLOG_DBG("port id %d removed from forwarding port"
                         "list of stg id %d",
                        portid, stgid);
        }
    }

    /* add port to respective ste port map list */
    switch (port_state) {
        case OPS_STG_PORT_STATE_BLOCKED:
            bitmap_set1(p_stg_data->pbmp_blocked_ports,portid);
            VLOG_DBG("port id %d set to blocked port list of stg id %d",
                portid, stgid);
            break;
            case OPS_STG_PORT_STATE_DISABLED:
                bitmap_set1(p_stg_data->pbmp_disabled_ports,portid);
                VLOG_DBG("port id %d set to disabled port"
                         "list of stg id %d",
                    portid, stgid);
                break;
            case OPS_STG_PORT_STATE_LEARNING:
                bitmap_set1(p_stg_data->pbmp_learning_ports,portid);
                VLOG_DBG("port id %d set to learning port"
                         "list of stg id %d",
                         portid, stgid);
                break;
            case OPS_STG_PORT_STATE_FORWARDING:
                bitmap_set1(p_stg_data->pbmp_forwarding_ports,portid);
                VLOG_DBG("port id %d set to forwarding port"
                         "list of stg id %d",
                         portid, stgid);
                break;
            default:
                VLOG_DBG("invalid port state");
                break;
    }


}

/*-----------------------------------------------------------------------------
| Function: ops_stg_default_get
| Description: get default stg group
| Parameters[in]: None
| Parameters[out]: opennsl_stg_t object
| Return: error  value
-----------------------------------------------------------------------------*/
int ops_stg_default_get(int *p_stg)
{
    if (!p_stg) {
        VLOG_ERR("Invalid stg ptr param");
        return -1;
    }

    *p_stg  = OPS_STG_DEFAULT;

    return 0;
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_vlan_add
| Description:  Add vlan to spanning tree group
| Parameters[in]: opennsl_stg_t : spanning tree group id
| Parameters[in]: opennsl_vlan_t: vlan id
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int ops_stg_vlan_add(int stgid, int vid)
{
    sai_status_t        rc          = SAI_STATUS_SUCCESS;
    int                 unit        = 0;
    ops_stg_data_t      *p_stg_data = NULL;
    bool                vlan_exists_in_stg_hw   = false;
    sai_attribute_t     vlan_list               = {0};
    sai_attribute_t     attr                    = {0};
    sai_object_id_t     sai_stp_id              = 0;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    if (!OPS_STG_VALID(stgid)) {
        VLOG_ERR("Invalid stgid param");
        return -1;
    }

    if (!OPS_VLAN_VALID(vid)) {
        VLOG_ERR("Invalid vid param");
        return -1;
    }

    p_stg_data = stg_data_find_create(stgid, false);
    if (NULL == p_stg_data) {
        VLOG_ERR("vlan add to non existing stg data for stg id %d",
                 stgid);
        return -1;
    }

    ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);
//    sai_stp_id = ops_sai_api_stp_id2sai_stp_id(stgid);
    vlan_list.id = SAI_STP_ATTR_VLAN_LIST;
    vlan_list.value.vlanlist.vlan_list = xzalloc(sizeof(sizeof(sai_vlan_id_t) * (OPS_VLAN_MAX + 1)));
    vlan_list.value.vlanlist.vlan_count= (OPS_VLAN_MAX + 1);
    rc = sai_api->stp_api->get_stp_attribute(sai_stp_id, 1, &vlan_list);
    if (SAI_STATUS_SUCCESS != rc) {
        VLOG_ERR("Unit %d, stg get vlan error, rc=%d\n",
                 unit, rc);
        return -1;
    }

    if (vlan_list.value.vlanlist.vlan_count) {
        for(int i =0; i < vlan_list.value.vlanlist.vlan_count ; i++) {
            if (vid == vlan_list.value.vlanlist.vlan_list[i]) {
                vlan_exists_in_stg_hw = true;
                break;
            }
        }
        free(vlan_list.value.vlanlist.vlan_list);
    }

    if (false == vlan_exists_in_stg_hw) {

        VLOG_DBG("opennsl_stg_vlan_add called with stgid %d vid %d", stgid,
                 vid);

        ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);

        attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
        attr.value.oid = sai_stp_id;
        rc = sai_api->vlan_api->set_vlan_attribute(vid, &attr);
        if (SAI_STATUS_SUCCESS != rc) {
            VLOG_ERR("Unit %d, stg vlan %d add error, rc=%d",
                     unit, vid, rc);
            return -1;
        }
    } else {
        VLOG_DBG("ops_stg_vlan_add: vlan id %d exists in stg id %d", vid, stgid);
    }

    stg_data_add_vlan(stgid, vid);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_vlan_remove
| Description:  Remove vlan from spanning tree group
| Parameters[in]: opennsl_stg_t : spanning tree group id
| Parameters[in]: opennsl_vlan_t: vlan id
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int ops_stg_vlan_remove(int stgid, int vid)
{
    sai_status_t        rc          = SAI_STATUS_SUCCESS;
    int                 unit = 0;
    sai_attribute_t     vlan_list               = {0};
    sai_attribute_t     attr                    = {0};
    sai_object_id_t     sai_stp_id              = 0;
    bool                vlan_exists_in_stg_hw   = false;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    ops_stg_data_t      *p_stg_data             = NULL;

    if (!OPS_STG_VALID(stgid)) {
        VLOG_ERR("Invalid stgid param");
        return -1;
    }

    if (!OPS_VLAN_VALID(vid)) {
        VLOG_ERR("Invalid vid param");
        return -1;
    }

    p_stg_data = stg_data_find_create(stgid, false);
    if (NULL == p_stg_data) {
        VLOG_ERR("vlan remove from non existing stg data for stg id %d",
                 stgid);
        return -1;
    }

    ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);
//    sai_stp_id = ops_sai_api_stp_id2sai_stp_id(stgid);
    vlan_list.id = SAI_STP_ATTR_VLAN_LIST;
    vlan_list.value.vlanlist.vlan_list = xzalloc(sizeof(sizeof(sai_vlan_id_t) * (OPS_VLAN_MAX + 1)));
    vlan_list.value.vlanlist.vlan_count= (OPS_VLAN_MAX + 1);
    rc = sai_api->stp_api->get_stp_attribute(sai_stp_id, 1, &vlan_list);
    if (SAI_STATUS_SUCCESS != rc) {
        VLOG_ERR("Unit %d, stg get vlan error, rc=%d\n",
                 unit, rc);
        return -1;
    }

    if (vlan_list.value.vlanlist.vlan_count) {
        for(int i =0; i < vlan_list.value.vlanlist.vlan_count ; i++) {
            if (vid == vlan_list.value.vlanlist.vlan_list[i]) {
                vlan_exists_in_stg_hw = true;
                break;
            }
        }
        free(vlan_list.value.vlanlist.vlan_list);
    }

    if (vlan_exists_in_stg_hw) {
        VLOG_DBG("opennsl_stg_vlan_remove called with stgid %d, vid %d", stgid,
                vid);

        ops_sai_api_stp_id2sai_stp_id(OPS_STG_RESERVED,&sai_stp_id);

        attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
        attr.value.oid = sai_stp_id;
        rc = sai_api->vlan_api->set_vlan_attribute(vid, &attr);
        if (SAI_STATUS_SUCCESS != rc) {
            VLOG_ERR("Unit %d, stg vlan %d remove error, rc=%d",
                    unit, vid, rc);
            return -1;
        }
    } else {
        VLOG_DBG("ops_stg_vlan_remove: vlan id %d doesn't exists in stg id %d", vid, stgid);
    }

    stg_data_remove_vlan(stgid, vid);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_create
| Description: create spanning tree group
| Parameters[in]: None
| Parameters[out]:opennsl_stg_t object
| Return: error value
-----------------------------------------------------------------------------*/
int ops_stg_create(int *pstgid)
{
    sai_status_t        rc          = SAI_STATUS_SUCCESS;
    int                 unit        = 0;
    sai_object_id_t     sai_stp_id  = 0;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_DBG("ops_stg_create_stg called");
    if (!pstgid) {
        VLOG_ERR("Invalid pstgid param");
        return -1;
    }

    if (ops_stg_count == OPS_STG_MAX) {
        VLOG_DBG("Max instances reached");
        return -1;
    }

    VLOG_DBG("create_stp called");
    rc = sai_api->stp_api->create_stp(&sai_stp_id,0,NULL);
    if (SAI_STATUS_SUCCESS != rc) {
        VLOG_ERR("Unit %d, create stg error, rc=%d",
                 unit, rc);
        return -1;
    }

    *pstgid = ops_sai_api_sai_stp_id2stp_id(sai_stp_id);

    VLOG_DBG("opennsl_stg_create returned stg %d", *pstgid);
    if (NULL != stg_data_find_create(*pstgid, true)) {
        VLOG_DBG("stg data entry created for stg id %d",
                 *pstgid);
    }
    else {
        VLOG_DBG("stg data entry creation failed for stg id %d",
                 *pstgid);
    }

    return SAI_ERROR_2_ERRNO(rc);
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_delete
| Description: delete stg from spanning tree group
| Parameters[in]: opennsl_stg_t: spanning tree group id
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int ops_stg_delete(int stgid)
{
    sai_status_t        rc      = SAI_STATUS_SUCCESS;
    int                 unit    = 0;
    sai_object_id_t     sai_stp_id  = 0;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    if (!OPS_STG_VALID(stgid)) {
        VLOG_ERR("Invalid stgid param");
        return -1;
    }

    if (OPS_STG_DEFAULT == stgid) {
        VLOG_ERR("default stg id %d shouldn't be deleted",
                 OPS_STG_DEFAULT);
        return -1;
    }

    if (NULL == stg_data_find_create(stgid, false)) {
        VLOG_ERR("stg data not found for given stgid %d",
                 stgid);
        return -1;
    }
    VLOG_DBG("remove_stp called with stgid %d", stgid);

//    sai_stp_id = ops_sai_api_stp_id2sai_stp_id(stgid);
    ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);
    rc = sai_api->stp_api->remove_stp(sai_stp_id);
    if (SAI_STATUS_SUCCESS != rc) {
        VLOG_ERR("Unit %d, delete stg error, rc=%d",
                 unit, rc);
        return -1;
    }

    stg_data_free(stgid);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function:  ops_stg_stp_set
| Description:  set port state in spanning tree group
| Parameters[in]: opennsl_stg_t : spanning tree group id
|.Parameters[in]: opennsl_port_t object
|.Parameters[in]: stp_state: port stp state
|.Parameters[in]:port_stp_set:
|                        True: if port first timetransitioning to forward state  or
                                   port blockec in all instances or
                                   single instance
                          else False.
| Parameters[out]: None
| Return:error value
-----------------------------------------------------------------------------*/
int ops_stg_stp_set(int stgid, int port, int stp_state,
                       bool port_stp_set)
{
    sai_status_t        rc              = SAI_STATUS_SUCCESS;
    int                 unit            = 0;
    ops_stg_data_t      *p_stg_data     = NULL;
    sai_object_id_t     sai_stp_id      = 0;
    sai_port_stp_port_state_t       hw_stp_state    = 0;
    const struct ops_sai_api_class  *sai_api = ops_sai_api_get_instance();

    if (!OPS_STG_VALID(stgid)) {
        VLOG_ERR("Invalid stgid param");
        return -1;
    }

    p_stg_data = stg_data_find_create(stgid, false);
    if (NULL == p_stg_data) {
        VLOG_ERR("port state set to non existing stg data for stg id %d",
                 stgid);
        return -1;
    }

    get_hw_port_state_enum_from_port_state(stp_state, (int*)&hw_stp_state);

    if (ops_stg_count > 1 ) {
        VLOG_DBG("set_stp_port_state called with stg %d port %d stp_state %d hw_stp_state %d",
                 stgid, port, stp_state, hw_stp_state);

        ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);

        rc = sai_api->stp_api->set_stp_port_state(sai_stp_id,
                                                  ops_sai_api_hw_id2port_id(port),
                                                  hw_stp_state);
        if (SAI_STATUS_SUCCESS != rc) {
            VLOG_ERR("Unit %d, stg %d port %d state %d set error, rc=%d",
                     unit, stgid, port, hw_stp_state, rc);
            return -1;
        }
    } else if ((ops_stg_count == 1) || port_stp_set) {
        VLOG_DBG("set_stp_port_state called with port %d stp_state %d hw stp state %d",
                 port, stp_state, hw_stp_state);

        ops_sai_api_stp_id2sai_stp_id(OPS_STG_DEFAULT,&sai_stp_id);
        rc = sai_api->stp_api->set_stp_port_state(sai_stp_id,
                                                  ops_sai_api_hw_id2port_id(port),
                                                  hw_stp_state);
        if (SAI_STATUS_SUCCESS != rc) {
            VLOG_ERR("Unit %d, port %d state %d set error, rc=%d",
                     unit, port, hw_stp_state, rc);
            return -1;
        }
    }

    stg_data_set_port_state(stgid, port, stp_state);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function: ops_stg_stp_get
| Description: get port state in spanning tree group
| Parameters[in]:opennsl_stg_t : spanning tree group id
| Parameters[in]: opennsl_port_t object
| Parameters[out]: p_stp_state: port stp state
| Return: error value
-----------------------------------------------------------------------------*/
int ops_stg_stp_get(int stgid, int port, int *p_stp_state)
{
    sai_status_t        rc              = SAI_STATUS_SUCCESS;
    int                 unit            = 0;
    sai_object_id_t     sai_stp_id      = 0;
    sai_port_stp_port_state_t       hw_stp_state    = 0;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();


    if (!stgid) {
        VLOG_ERR("Invalid stgid param");
        return -1;
    }

    ops_sai_api_stp_id2sai_stp_id(stgid,&sai_stp_id);

    rc = sai_api->stp_api->get_stp_port_state(sai_stp_id,
                                                  ops_sai_api_hw_id2port_id(port),
                                                  &hw_stp_state);
    if (SAI_STATUS_SUCCESS != rc) {
        VLOG_ERR("Unit %d, stg port state get error, rc=%d",
                 unit, rc);
        return -1;
    }

    get_port_state_from_hw_port_state((int)hw_stp_state, p_stp_state);

    return 0;
}

///////////////////////////////// INIT /////////////////////////////////
/*-----------------------------------------------------------------------------
| Function: ops_stg_init
| Description:initialization routine
| Parameters[in]:None
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/

int
ops_stg_init(int hw_unit)
{
    int pstg = 0;
    (void)hw_unit;

    ops_stg_create(&pstg);

    if (NULL == stg_data_find_create(OPS_STG_DEFAULT, false)) {
        VLOG_ERR("stg data create error for default stgid %d",
                 OPS_STG_DEFAULT);
        return -1;
    }
    VLOG_INFO("STG data created for stg-id %d", OPS_STG_DEFAULT);

    return 0;
}

///////////////////////////Plugin extension routines /////////////////////////////*/
/*-----------------------------------------------------------------------------
| Function: create_stg
| Description:plugin extension routine
| Parameters[in]:None
| Parameters[out]:  stg object
| Return: error value
-----------------------------------------------------------------------------*/
int
create_stg(int *p_stg)
{
    int stgid = 0;

    VLOG_DBG("%s: create stg entry called", __FUNCTION__);
    /* Create  stg and associate valn to stg. */
    ops_stg_create(&stgid);
    if (0 != stgid) {
        *p_stg = stgid;
        VLOG_DBG("%s: create stg entry, val=%d", __FUNCTION__, stgid);
        return 0;
    }
    else {
        VLOG_ERR("%s: create stg entry failed", __FUNCTION__);
        return -1;
    }
}

/*-----------------------------------------------------------------------------
| Function: delete_stg
| Description:plugin extension routine to delete STG
| Parameters[in]: stg id
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int
delete_stg(int stg)
{
    VLOG_DBG("%s: entry, stg=%d", __FUNCTION__, stg);
    /* Delete  stg. */
    ops_stg_delete(stg);

    return 0;
}

/*-----------------------------------------------------------------------------
| Function: add_stg_vlan
| Description: plugin extension routine to add vlan to stg
| Parameters[in]: stg id
| Parameters[in]: vlan id
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int
add_stg_vlan(int stg, int vid)
{
    ops_stg_vlan_add(stg, vid);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function: remove_stg_vlan
| Description:plugin extension routine  to remove vlan from STG
| Parameters[in]:None
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int
remove_stg_vlan(int stg, int vid)
{
    ops_stg_vlan_remove(stg, vid);
    return 0;
}

/*-----------------------------------------------------------------------------
| Function: set_stg_port_state
| Description:plugin extension routine  to set port state in STG
| Parameters[in]:port name
| Parameters[in]:stg id
| Parameters[in]:port state
| Parameters[in]:set global port state
| Parameters[out]: None
| Return: error value
-----------------------------------------------------------------------------*/
int
set_stg_port_state(char *port_name, int stg,
                     int port_state, bool port_stp_set)
{
    int hw_id = 0;

    VLOG_DBG("%s: called", __FUNCTION__);
    if (false == netdev_hw_id_from_name(port_name, &hw_id)) {
        VLOG_ERR("%s: unable to find netdev for port %s", __FUNCTION__,
                 port_name);
        return -1;
    }

    VLOG_DBG("%s: stg=%d, port=%d, port_state=%d", __FUNCTION__,
             stg, hw_id, port_state);
    /* set stg port state. */
    ops_stg_stp_set(stg, hw_id, port_state, port_stp_set);

    return 0;
}

/*-----------------------------------------------------------------------------
| Function:  get_stg_port_state
| Description:plugin extension routine  to get port state in STG
| Parameters[in]:port name
| Parameters[in]:stg id
| Parameters[out]: port state object
| Return: error value
-----------------------------------------------------------------------------*/
int
get_stg_port_state(char *port_name, int stg, int *p_port_state)
{
    int hw_id = 0;

    VLOG_DBG("%s: called", __FUNCTION__);
    if (false == netdev_hw_id_from_name(port_name, &hw_id)) {
        VLOG_ERR("%s: unable to find netdev for port %s", __FUNCTION__, port_name);
        return -1;
    }

    VLOG_DBG("%s: stg=%d, port=%d", __FUNCTION__, stg, hw_id);
    /* Get stg port state. */
    ops_stg_stp_get(stg, hw_id, p_port_state);

    return 0;
}

/*-----------------------------------------------------------------------------
| Function: get_stg_default
| Description: plugin extension routine to get default STG.
| Parameters[in]:None
| Parameters[out]: stg object
| Return: error value
-----------------------------------------------------------------------------*/
int
get_stg_default(int *p_stg)
{
   /* Get default stg */
   ops_stg_default_get(p_stg);
   return 0;
}
