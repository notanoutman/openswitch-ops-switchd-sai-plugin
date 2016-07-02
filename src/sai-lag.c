/*
 * Copyright Centec Networks Inc. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef __BOOL_DEFINED
#define __BOOL_DEFINED
#endif

#include <netdev-provider.h>
#include <sai-log.h>
#include <sai-vendor.h>
#include <sai-api-class.h>
#include <sai-lag.h>

VLOG_DEFINE_THIS_MODULE(sai_lag);

static struct hmap sai_lap_data = HMAP_INITIALIZER(&sai_lap_data);


#define CTC_SAI_OBJECT_TYPE_GET(objid)  							\
    ((objid >> 32) & 0xFF)

#define CTC_SAI_OBJECT_INDEX_GET(objid)  							\
    (objid & 0xFFFFFFFF)

int
ops_sai_api_sai_lag_id2lag_id(sai_object_id_t sai_lag_id)
{
    return CTC_SAI_OBJECT_INDEX_GET(sai_lag_id);
}

sai_object_id_t
ops_sai_api_lag_id2sai_lag_id(int lag_id)
{
	return (((sai_object_id_t)(SAI_OBJECT_TYPE_LAG)<<32 )| (sai_object_id_t)lag_id);
}

sai_object_id_t
ops_sai_api_hw_id2sai_lag_member_id(int hw_id)
{
	return (((sai_object_id_t)(SAI_OBJECT_TYPE_LAG_MEMBER)<<32 )| (sai_object_id_t)hw_id);
}

static ops_sai_lag_data_t *
__find_lag_data(int lag_id)
{
    ops_sai_lag_data_t *lagp = NULL;
    ops_sai_lag_data_t *tmp_lagp = NULL;

    HMAP_FOR_EACH(tmp_lagp, hmap_lag_data, &sai_lap_data) {
        if (tmp_lagp->lag_id != lag_id) {
            continue;
        }
        lagp = tmp_lagp;
        break;
    }

    return lagp;

}

static void
__free_lag_data(ops_sai_lag_data_t *lagp)
{
    bitmap_free(lagp->pbmp_ports);
    bitmap_free(lagp->pbmp_egr_en);

    hmap_remove(&sai_lap_data, &lagp->hmap_lag_data);
    free(lagp);
}

static ops_sai_lag_data_t *
__get_lag_data(int lag_id)
{
    ops_sai_lag_data_t *lagp = NULL;

    lagp = __find_lag_data(lag_id);
    if (lagp == NULL) {
        // LAG data hasn't been created yet.
        lagp = malloc(sizeof(ops_sai_lag_data_t));
        if (!lagp) {
            VLOG_ERR("Failed to allocate memory for LAG id=%d", lag_id);
            return NULL;
        }

        // lag_id is allocated by bcmsdk later.
        lagp->lag_id = -1;
        lagp->hw_created = 0;

        lagp->pbmp_ports  = bitmap_allocate(SAI_PORTS_MAX);
        lagp->pbmp_egr_en = bitmap_allocate(SAI_PORTS_MAX);

        hmap_insert(&sai_lap_data, &lagp->hmap_lag_data,
                hash_int(lag_id, 0));
    }

    return lagp;
}

int
__ops_sai_lag_attach_port(int lag_id, int hw_port)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_object_id_t                 sai_lag_number_id = 0;
    sai_attribute_t                 attr[2];

    VLOG_DBG("Trunk Attach: hw_port=%d, tid=%d",
               hw_port, lag_id);

    attr[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attr[0].value.oid = (((sai_object_id_t)(SAI_OBJECT_TYPE_LAG)<<32 )| (sai_object_id_t)lag_id);;

    attr[1].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attr[1].value.oid = ops_sai_api_hw_id2port_id(hw_port);

    status = sai_api->lag_api->create_lag_member(&sai_lag_number_id, sizeof(attr)/sizeof(attr[0]), attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to create lag");

    VLOG_DBG("Done.");

    return 0;
exit:
    return 0;
}

int
__ops_sai_lag_detach_port(int lag_id, int hw_port)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_object_id_t                 sai_lag_number_id = 0;

    VLOG_DBG("Trunk Detach: hw_port=%d, tid=%d",
               hw_port, lag_id);

    sai_lag_number_id = ops_sai_api_hw_id2sai_lag_member_id(hw_port);
    status = sai_api->lag_api->remove_lag_member(sai_lag_number_id);

    SAI_ERROR_LOG_EXIT(status, "Failed to detach lag");

    VLOG_DBG("Done.");

    return 0;
exit:
    return 0;
}

int
__ops_sai_lag_egress_enable_port(int lag_id, int hw_port, int enable)
{
    sai_status_t                    status      = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_object_id_t                 sai_lag_number_id = 0;
    sai_attribute_t                 attr = {0};

    VLOG_DBG("Trunk Detach: hw_port=%d, tid=%d",
               hw_port, lag_id);

    attr.id = SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE;

    if(enable)
    {
        attr.value.booldata = false;
    }else{
        attr.value.booldata = true;
    }

    sai_lag_number_id = ops_sai_api_hw_id2sai_lag_member_id(hw_port);

    status = sai_api->lag_api->set_lag_member_attribute(sai_lag_number_id,&attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to egress enable port to lag");

    VLOG_DBG("Done.");

    return 0;
exit:
    return 0;
}


//////////////////////////////// Public API //////////////////////////////
int
ops_sai_lag_create(int *lag_idp)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    sai_object_id_t     sai_lag_id  = 0;
    ops_sai_lag_data_t *lagp        = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    VLOG_DBG("entry: lag_id=%d", *lag_idp);

    lagp = __get_lag_data(*lag_idp);
    if (!lagp) {
        VLOG_ERR("Failed to get LAG data for LAGID %d", *lag_idp);
        return -1;
    }

    if (lagp->hw_created) {
        VLOG_WARN("Duplicated LAG creation request, LAGID=%d",
                  *lag_idp);
        return -1;
    }

    status = sai_api->lag_api->create_lag(&sai_lag_id, 0, NULL);

    SAI_ERROR_LOG_EXIT(status, "Failed to create lag");

    lagp->hw_created    = 1;
    lagp->lag_id        = ops_sai_api_sai_lag_id2lag_id(sai_lag_id);

    *lag_idp = ops_sai_api_sai_lag_id2lag_id(sai_lag_id);

    VLOG_DBG("Done");
    return SAI_ERROR_2_ERRNO(status);
exit:
    __free_lag_data(lagp);
    return SAI_ERROR_2_ERRNO(status);
}

int
ops_sai_lag_destroy(int lag_id)
{
    sai_status_t        status      = SAI_STATUS_SUCCESS;
    sai_object_id_t     sai_lag_id  = 0;
    ops_sai_lag_data_t  *lagp       = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    lagp = __find_lag_data(lag_id);
    if (lagp) {
        sai_lag_id = (((sai_object_id_t)(SAI_OBJECT_TYPE_LAG)<<32 )| (sai_object_id_t)lag_id);;
        status = sai_api->lag_api->remove_lag(sai_lag_id);

        __free_lag_data(lagp);
    } else {
        VLOG_WARN("Deleting non-existing LAG, LAG_ID=%d", lag_id);
    }

    VLOG_DBG("Done");

    return SAI_ERROR_2_ERRNO(status);
}
#if 0
void
ops_sai_lag_attach_ports(int lag_id, unsigned long *pbm)
{
    int                 index = 0;
    ops_sai_lag_data_t  *lagp;
    int                 hw_port;

    (void)hw_port;

    VLOG_DBG("entry: lag_id=%d", lag_id);

    lagp = __get_lag_data(lag_id);
    if (!lagp) {
        VLOG_ERR("Failed to get LAG data for LAGID %d", lag_id);
        return;
    }

    if (!lagp->hw_created) {
        VLOG_WARN("Error LAGID=%d not created in hardware", lag_id);
        return;
    }

    /* Detach removed member ports. */
    for(index = 0; index < SAI_PORTS_MAX; index++)
    {
        if(!bitmap_is_set(pbm,index) && bitmap_is_set(lagp->pbmp_ports,index))
        {
            __ops_sai_lag_detach_port(lagp->lag_id,index);
            bitmap_set0(lagp->pbmp_ports,index);
        }
    }

    /* Attach current list of member ports. */
    for(index = 0; index < SAI_PORTS_MAX; index++)
    {
        if(bitmap_is_set(pbm,index) && !bitmap_is_set(lagp->pbmp_ports,index))
        {
            __ops_sai_lag_attach_port(lagp->lag_id,index);
            bitmap_set1(lagp->pbmp_ports,index);
        }
    }

    VLOG_DBG("done");
} // ops_sai_lag_attach_ports

void
ops_sai_lag_egress_enable_ports(int lag_id, unsigned long *pbm)
{
    int                 index = 0;
    ops_sai_lag_data_t  *lagp;
    int                 hw_port;

    VLOG_DBG("entry: lag_id=%d", lag_id);

    lagp = __get_lag_data(lag_id);
    if (!lagp) {
        VLOG_ERR("Failed to get LAG data for LAGID %d", lag_id);
        return;
    }

    if (!lagp->hw_created) {
        VLOG_WARN("Error LAGID=%d not created in hardware", lag_id);
        return;
    }

    /* Disable egress for removed member ports. */
    for(index = 0; index < SAI_PORTS_MAX; index++)
    {
        if(!bitmap_is_set(pbm,index) && bitmap_is_set(lagp->pbmp_egr_en,index))
        {
            __ops_sai_lag_egress_enable_port(lagp->lag_id,index,false);
            bitmap_set0(lagp->pbmp_egr_en,index);
        }
    }

    /* Enable egress for the current list of member ports. */
    for(index = 0; index < SAI_PORTS_MAX; index++)
    {
        if(bitmap_is_set(pbm,index) && !bitmap_is_set(lagp->pbmp_egr_en,index))
        {
            __ops_sai_lag_egress_enable_port(lagp->lag_id,index,true);
            bitmap_set1(lagp->pbmp_egr_en,index);
        }
    }

    VLOG_DBG("done");
} // ops_sai_lag_egress_enable_ports

void
ops_sai_lag_set_balance_mode(int lag_id, int lag_mode)
{
    int                 index = 0;
    ops_sai_lag_data_t  *lagp;
    int                 hw_port;

    VLOG_DBG("entry: lag_id=%d", lag_id);

    lagp = __get_lag_data(lag_id);
    if (!lagp) {
        VLOG_ERR("Failed to get LAG data for LAGID %d", lag_id);
        return;
    }

    if (!lagp->hw_created) {
        VLOG_WARN("Error LAGID=%d not created in hardware", lag_id);
        return;
    }

    if (lagp->lag_mode != lag_mode) {
        //hw_set_lag_balance_mode(unit, lag_id, lag_mode);
        lagp->lag_mode = lag_mode;
    }

    VLOG_DBG("done");
} // ops_sai_lag_set_balance_mode
#endif
