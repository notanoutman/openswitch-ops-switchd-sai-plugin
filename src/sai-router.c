/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-router.h>
#include <sai-api-class.h>

VLOG_DEFINE_THIS_MODULE(sai_router);

/*
 * Initializes virtual router.
 */
static void
__router_init(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

/*
 * Creates virtual router.
 *
 * @param[out] vrid - Virtual router id.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__router_create(handle_t *handle)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr[3];
    sai_object_id_t routerid = 55;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    memset(attr, 0, sizeof(attr));
    attr[0].id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE;
    attr[0].value.booldata = 1;
    attr[1].id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V6_STATE;
    attr[1].value.booldata = 0;
    attr[2].id = SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_TTL1_ACTION;
    attr[2].value.s32 = SAI_PACKET_ACTION_TRAP;
    status = sai_api->router_api->create_virtual_router(&routerid, 3, attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to create virtual router");
    handle->data = routerid;

exit:
    return SAI_ERROR_2_ERRNO(status);

}
/*
 * Removes virtual router.
 *
 * @param[out] vrid - Virtual router id.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__router_remove(const handle_t *handle)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
    return 0;
}

/*
 * De-initializes virtual router.
 */
static void
__router_deinit(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

DEFINE_GENERIC_CLASS(struct router_class, router) = {
    .init = __router_init,
    .create = __router_create,
    .remove = __router_remove,
    .deinit = __router_deinit
};

DEFINE_GENERIC_CLASS_GETTER(struct router_class, router);
