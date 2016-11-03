/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-api-class.h>
#include <sai-sflow.h>

VLOG_DEFINE_THIS_MODULE(sai_sflow);

/*
 * Initialize sflow.
 */
static void
__sflow_init(void)
{
    VLOG_INFO("Initializing sflow");
}

/*
 * De-initialize sflow.
 */
static void
__sflow_deinit(void)
{
    VLOG_INFO("De-initializing sflow");
}

static int
__sflow_create(handle_t *id, uint32_t rate)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr;
    sai_object_id_t oid;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    ovs_assert(sai_api->samplepacket_api != NULL);

    attr.id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;
    attr.value.u32 = rate;

    status = sai_api->samplepacket_api->create_samplepacket_session(&oid,1,&attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to create_samplepacket_session");

    id->data = oid;

    return 0;
exit:
    return -1;
}

static int
__sflow_remove(handle_t id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    sai_object_id_t oid;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    ovs_assert(sai_api->samplepacket_api != NULL);

    oid = id.data;
    status = sai_api->samplepacket_api->remove_samplepacket_session(oid);
    SAI_ERROR_LOG_EXIT(status, "Failed to remove_samplepacket_session");

    return 0;
exit:
    return -1;
}



static int
__sflow_set_rate(handle_t id, uint32_t rate)
{

    return 0;
}

DEFINE_GENERIC_CLASS(struct sflow_class, sflow) = {
        .init     = __sflow_init,
        .create   = __sflow_create,
        .remove   = __sflow_remove,
        .set_rate = __sflow_set_rate,
        .deinit   = __sflow_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct sflow_class, sflow);
