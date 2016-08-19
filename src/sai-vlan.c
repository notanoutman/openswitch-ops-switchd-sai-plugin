/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */
#ifndef __BOOL_DEFINED
#define __BOOL_DEFINED
#endif

#include <vlan-bitmap.h>
#include <ofproto/ofproto.h>

#include <sai-log.h>
#include <sai-api-class.h>
#include <sai-port.h>
#include <sai-vlan.h>

VLOG_DEFINE_THIS_MODULE(sai_vlan);

static struct hmap     all_vlan_member[VLAN_BITMAP_SIZE];

struct hvlan_mb_entry {
    struct hmap_node    hmap_node;
    uint32_t            hw_id;
    int32_t             tag_mode;
};

static int __vlan_port_set(sai_vlan_id_t, uint32_t, sai_vlan_tagging_mode_t,
                           bool);
static int __trunks_port_set(const unsigned long *, uint32_t, bool);

static struct hvlan_mb_entry*
__vlan_mb_entry_hmap_find_create(struct hmap *hmap, uint32_t  hw_id, bool is_create)
{
    struct hvlan_mb_entry* hnode_entry  = NULL;

    NULL_PARAM_LOG_ABORT(hmap);

    HMAP_FOR_EACH(hnode_entry, hmap_node, hmap) {
        if (hnode_entry->hw_id == hw_id) {
            return hnode_entry;
        }
    }

    if (false == is_create) {
        return NULL;
    }

    hnode_entry = xzalloc(sizeof(struct hvlan_mb_entry));
    if (!hnode_entry) {
        VLOG_ERR("Failed to allocate memory vlan_mb");

        return NULL;
    }

    hnode_entry->hw_id = hw_id;

    hmap_insert(hmap, &hnode_entry->hmap_node, hash_int(hnode_entry->hw_id, 0));

    return hnode_entry;
}

static void
__vlan_mb_entry_hmap_remove(struct hmap *hmap, struct hvlan_mb_entry *hnode_entry)
{
    hmap_remove(hmap,&hnode_entry->hmap_node);

    free(hnode_entry);
}

/*
 * Initialize VLANs.
 */
void
__vlan_init(void)
{
    int vlan_idx = 0;

    VLOG_INFO("Initializing VLANs");

    for(vlan_idx = 0; vlan_idx < VLAN_BITMAP_SIZE; vlan_idx++) {
        hmap_init(&all_vlan_member[vlan_idx]);
    }
}

int
ops_sai_vlan_intf_update(int vid, bool add)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct hvlan_mb_entry *hnode_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    if (add) {
        status = sai_api->vlan_api->create_vlan(vid);
        if (SAI_STATUS_ITEM_ALREADY_EXISTS == status) {
            return SAI_STATUS_SUCCESS;
        }

        SAI_ERROR_LOG_EXIT(status, "Failed to %s vlan vid %d", add ? "create" : "remove", vid);

        HMAP_FOR_EACH(hnode_entry, hmap_node, &all_vlan_member[vid]) {
            __vlan_port_set(vid, hnode_entry->hw_id, hnode_entry->tag_mode, true);
        }
    }

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * De-initialize VLANs.
 */
void
__vlan_deinit(void)
{
    VLOG_INFO("De-initializing VLANs");
}

/*
 * Adds port to access vlan.
 * @param[in] vid VLAN id.
 * @param[in] hw_id port label id.
 * @return 0, sai error converted to errno otherwise.
 */
int
__vlan_access_port_add(sai_vlan_id_t vid, uint32_t hw_id)
{
    int status = 0;
    status = __vlan_port_set(vid, hw_id, SAI_VLAN_PORT_UNTAGGED, true);
    ERRNO_LOG_EXIT(status, "Failed to __vlan port set");

    status = ops_sai_port_ingress_filter_set(hw_id, true);
    ERRNO_LOG_EXIT(status, "Failed to set ingress filter");

    status = ops_sai_port_drop_untagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop untagged");

    status = ops_sai_port_drop_tagged_set(hw_id, true);
    ERRNO_LOG_EXIT(status, "Failed to set drop tagged");

exit:
    return status;
}

/*
 * Removes port from access vlan and sets PVID to default.
 * @param[in] vid VLAN id.
 * @param[in] hw_id port label id.
 * @return 0, sai error converted to errno otherwise.
 */
int
__vlan_access_port_del(sai_vlan_id_t vid, uint32_t hw_id)
{
    int status = 0;

    /* Mode doesn't matter when port is removed from vlan. */
    status = __vlan_port_set(vid, hw_id, SAI_VLAN_PORT_UNTAGGED, false);
    ERRNO_EXIT(status);

    status = ops_sai_port_pvid_set(hw_id, OPS_SAI_PORT_DEFAULT_PVID);
    ERRNO_EXIT(status);

    status = ops_sai_port_pvid_untag_enable_set(hw_id, true);
    ERRNO_EXIT(status);

    status = ops_sai_port_ingress_filter_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set ingress filter");

    status = ops_sai_port_drop_untagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop untagged");

    status = ops_sai_port_drop_tagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop tagged");

exit:
    return status;
}

/*
 * Adds port to trunks.
 * @param[in] trunks vlan bitmap.
 * @param[in] hw_id port label id.
 * @return 0, sai error converted to errno otherwise.
 */
int
__vlan_trunks_port_add(const unsigned long *trunks, uint32_t hw_id)
{
    int status = 0;

    status = __trunks_port_set(trunks, hw_id, true);
    ERRNO_LOG_EXIT(status, "Failed to __trunks port set true");

    status = ops_sai_port_ingress_filter_set(hw_id, true);
    ERRNO_LOG_EXIT(status, "Failed to set ingress filter");

    status = ops_sai_port_drop_untagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop untagged");

    status = ops_sai_port_drop_tagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop tagged");

exit:
    return status;
}

/*
 * Removes port from trunks.
 * @param[in] hw_id port label id.
 * @param[in] trunks vlan bitmap.
 * @return 0, sai error converted to errno otherwise.
 */
int
__vlan_trunks_port_del(const unsigned long *trunks, uint32_t hw_id)
{
    int status = 0;

    status = __trunks_port_set(trunks, hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to __trunks port set false");

    status = ops_sai_port_ingress_filter_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set ingress filter");

    status = ops_sai_port_drop_untagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop untagged");

    status = ops_sai_port_drop_tagged_set(hw_id, false);
    ERRNO_LOG_EXIT(status, "Failed to set drop tagged");

exit:
    return status;
}

/*
 * Creates or destroys vlan.
 * @param[in] vid VLAN id.
 * @param[in] add boolean which says if vlan should be added or removed.
 * @return 0, sai error converted to errno otherwise.
 */
int
__vlan_set(int vid, bool add)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct hvlan_mb_entry *hnode_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    if (add) {
        status = sai_api->vlan_api->create_vlan(vid);
        if (SAI_STATUS_ITEM_ALREADY_EXISTS == status) {
            status = SAI_STATUS_SUCCESS;
            goto out;
        }

    } else {
        status = sai_api->vlan_api->remove_vlan(vid);
    }

    SAI_ERROR_LOG_EXIT(status, "Failed to %s vlan vid %d", add ? "create" : "remove", vid);

out:
    if(add) {
        HMAP_FOR_EACH(hnode_entry, hmap_node, &all_vlan_member[vid]) {
            __vlan_port_set(vid, hnode_entry->hw_id, hnode_entry->tag_mode, true);
        }
    }

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Sets vlan to port.
 * @param[in] vid VLAN id.
 * @param[in] hw_id port label id.
 * @param[in] mode tagging mode: tagged/untagged.
 * @param[in] add boolean which says if port should be added or removed to/from
 * vlan.
 * @return 0, sai error converted to errno otherwise.
 */
static int
__vlan_port_set(sai_vlan_id_t vid, uint32_t hw_id,
                sai_vlan_tagging_mode_t mode, bool add)
{
    sai_vlan_port_t vlan_port;
    sai_status_t status = SAI_STATUS_SUCCESS;
    struct hvlan_mb_entry *hnode_entry = NULL;
    const struct ops_sai_api_class *sai_api = ops_sai_api_get_instance();

    vlan_port.port_id = ops_sai_api_hw_id2port_id(hw_id);
    vlan_port.tagging_mode = mode;
    if (add) {
        status = sai_api->vlan_api->add_ports_to_vlan(vid, 1, &vlan_port);
    } else {
        status = sai_api->vlan_api->remove_ports_from_vlan(vid, 1, &vlan_port);
    }
    SAI_ERROR_LOG_EXIT(status, "Failed to %s vlan %d on port %u",
                       add ? "add" : "remove", vid, hw_id);

    if(add) {
        hnode_entry = __vlan_mb_entry_hmap_find_create(&all_vlan_member[vid], hw_id, true);
        if(hnode_entry) {
            hnode_entry->tag_mode = mode;
        }
    } else {
        hnode_entry = __vlan_mb_entry_hmap_find_create(&all_vlan_member[vid], hw_id, false);
        if(hnode_entry) {
            __vlan_mb_entry_hmap_remove(&all_vlan_member[vid], hnode_entry);
        }
    }

    if (add && (SAI_VLAN_PORT_UNTAGGED != mode)) {
        goto exit;
    }

    if(0 == add) {
	vid = OPS_SAI_PORT_DEFAULT_PVID;
    }

    /* No need to convert return code - already errno value. */
    return ops_sai_port_pvid_set(hw_id, vid);

exit:
    return SAI_ERROR_2_ERRNO(status);
}

/*
 * Sets trunks to port.
 * @param[in] trunks vlan bitmap.
 * @param[in] hw_id port label id.
 * @param[in] add boolean which says if port should be added or removed to/from
 * vlan.
 * @return 0, sai error converted to errno otherwise.
 */
static int
__trunks_port_set(const unsigned long *trunks, uint32_t hw_id, bool add)
{
    int vid = 0;
    int status = 0;

    NULL_PARAM_LOG_ABORT(trunks);

    BITMAP_FOR_EACH_1(vid, VLAN_BITMAP_SIZE, trunks) {
        status = __vlan_port_set(vid, hw_id, SAI_VLAN_PORT_TAGGED, add);
        ERRNO_LOG_EXIT(status, "Failed to %s trunks", add ? "add" : "remove");

        if(!add) {
            status = ops_sai_port_pvid_set(hw_id, OPS_SAI_PORT_DEFAULT_PVID);
            ERRNO_EXIT(status);

            status = ops_sai_port_pvid_untag_enable_set(hw_id, true);
            ERRNO_EXIT(status);
        }
    }

exit:
    return status;
}

DEFINE_GENERIC_CLASS(struct vlan_class, vlan) = {
        .init = __vlan_init,
        .access_port_add = __vlan_access_port_add,
        .access_port_del = __vlan_access_port_del,
        .trunks_port_add = __vlan_trunks_port_add,
        .trunks_port_del = __vlan_trunks_port_del,
        .set = __vlan_set,
        .deinit = __vlan_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct vlan_class, vlan);
