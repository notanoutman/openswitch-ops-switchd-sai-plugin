/*
 * * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-log.h>
#include <sai-common.h>
#include <sai-hash.h>
#include <sai-api-class.h>
#include "ofproto/ofproto.h"


VLOG_DEFINE_THIS_MODULE(sai_hash);

static sai_object_id_t hash_field = 0;

static uint32_t
__ecmp_hash_type_ofproto_to_sai(uint32_t ofproto_type)
{
    uint32_t sai_type = 0;
    switch (ofproto_type)
    {
    case OFPROTO_ECMP_HASH_SRCPORT:
        sai_type = SAI_NATIVE_HASH_FIELD_SRC_IP;
        break;

    case OFPROTO_ECMP_HASH_DSTPORT:
        sai_type = SAI_NATIVE_HASH_FIELD_L4_DST_PORT;
        break;

    case OFPROTO_ECMP_HASH_SRCIP:
        sai_type = SAI_NATIVE_HASH_FIELD_SRC_IP;
        break;

    case OFPROTO_ECMP_HASH_DSTIP:
        sai_type = SAI_NATIVE_HASH_FIELD_DST_IP;
        break;

    case OFPROTO_ECMP_HASH_RESILIENT:
        sai_type = SAI_NATIVE_HASH_FIELD_SRC_IP;
        break;
    default:
        break;
    }

    return sai_type;
}

/*
 * Initialize hashing. Set default hash fields.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static void
__ecmp_hash_init(void)
{
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_status_t                status = SAI_STATUS_SUCCESS;
    sai_object_id_t             hash_type = 0;
    sai_attribute_t             attr[1];

    memset(attr, 0, sizeof(attr));

    OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_SRC_IP);
    OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_DST_IP);
    OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_L4_SRC_PORT);
    OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_L4_DST_PORT);

    attr[0].id = SAI_SWITCH_ATTR_ECMP_HASH;
    attr[0].value.oid = hash_type;
    status = sai_api->switch_api->set_switch_attribute(attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to set ecmp hash fields");

    OPS_SAI_SET_FLAG(hash_field, OFPROTO_ECMP_HASH_SRCPORT);
    OPS_SAI_SET_FLAG(hash_field, OFPROTO_ECMP_HASH_DSTPORT);
    OPS_SAI_SET_FLAG(hash_field, OFPROTO_ECMP_HASH_SRCIP);
    OPS_SAI_SET_FLAG(hash_field, OFPROTO_ECMP_HASH_DSTIP);

exit:
    return;
}

/*
 * Set hash fields.
 *
 * @param[in] fields_to_set - bitmap of hash fields.
 * @param[in] enable        - specifies if hash fields should be enabled or disabled.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static int
__ecmp_hash_set(uint64_t fields_to_set, bool enable)
{
    sai_status_t        status = SAI_STATUS_SUCCESS;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();
    sai_object_id_t     hash_type = 0;
    bool                need_update = false;
    sai_attribute_t     attr[1];

    memset(attr, 0, sizeof(attr));

    hash_type = __ecmp_hash_type_ofproto_to_sai(fields_to_set);
    if (enable) {
        if (!OPS_SAI_FLAG_ISSET(hash_field, fields_to_set)) {
            OPS_SAI_SET_FLAG(hash_field, fields_to_set);
            need_update = true;
        }
    } else {
        if (OPS_SAI_FLAG_ISSET(hash_field, fields_to_set)) {
            OPS_SAI_UNSET_FLAG(hash_field, fields_to_set);
            need_update = true;
        }
    }

    if (!need_update) {
        goto exit;
    }

    if (OPS_SAI_FLAG_ISSET(hash_field, OFPROTO_ECMP_HASH_SRCPORT)) {
        OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_L4_SRC_PORT);
    }

    if (OPS_SAI_FLAG_ISSET(hash_field, OFPROTO_ECMP_HASH_DSTPORT)) {
        OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_L4_DST_PORT);
    }

    if (OPS_SAI_FLAG_ISSET(hash_field, OFPROTO_ECMP_HASH_SRCIP)) {
        OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_SRC_IP);
    }

    if (OPS_SAI_FLAG_ISSET(hash_field, OFPROTO_ECMP_HASH_DSTIP)) {
        OPS_SAI_SET_BIT(hash_type, SAI_NATIVE_HASH_FIELD_DST_IP);
    }

    if (OPS_SAI_FLAG_ISSET(hash_field, OFPROTO_ECMP_HASH_RESILIENT)) {
        //need_update = true;
    }

    attr[0].id = SAI_SWITCH_ATTR_ECMP_HASH;
    attr[0].value.oid = hash_type;

    status = sai_api->switch_api->set_switch_attribute(attr);

    SAI_ERROR_LOG_EXIT(status, "Failed to set ecmp hash fields");

exit:
    return 0;
}

/*
 * De-initialize hashing.
 *
 * @return 0 operation completed successfully
 * @return errno operation failed
 */
static void
__ecmp_hash_deinit(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}

DEFINE_GENERIC_CLASS(struct hash_class, hash) = {
    .init = __ecmp_hash_init,
    .ecmp_hash_set = __ecmp_hash_set,
    .deinit = __ecmp_hash_deinit
};

DEFINE_GENERIC_CLASS_GETTER(struct hash_class, hash);
