/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
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
#include <sai-fdb.h>

VLOG_DEFINE_THIS_MODULE(sai_fdb);

enum mac_flush_options {
    L2MAC_FLUSH_BY_VLAN,
    L2MAC_FLUSH_BY_PORT,
    L2MAC_FLUSH_BY_PORT_VLAN,
    L2MAC_FLUSH_BY_TRUNK,
    L2MAC_FLUSH_BY_TRUNK_VLAN,
    L2MAC_FLUSH_ALL
};

static void __fdb_init(void);
static void __fdb_deinit(void);
static int  __fdb_set_aging_time(int);
static int  __fdb_register_fdb_event_callback(sai_fdb_event_notification_fn);
static int  __fdb_flush_entrys(int, handle_t, int);

/*
 * Initialize FDBs.
 */
static void
__fdb_init(void)
{
    VLOG_INFO("Initializing FDBs");

	__fdb_set_aging_time(300); 	/* ops-default aging time 300sec; */
}

/*
 * De-initialize FDBs.
 */
static void
__fdb_deinit(void)
{
    VLOG_INFO("De-initializing FDBs");
}

/*
 * Set dynamic FDB entry aging time.
 */
static int
__fdb_set_aging_time(int sec)
{
	sai_status_t                    status      = SAI_STATUS_SUCCESS;
	const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
    sai_attribute_t                 attr;

	VLOG_INFO("__fdb_set_aging_time sec:%d", sec);

    attr.id 		= SAI_SWITCH_ATTR_FDB_AGING_TIME;
    attr.value.u32  = sec;

	status = sai_api->switch_api->set_switch_attribute(&attr);
    SAI_ERROR_LOG_EXIT(status, "Failed to __fdb_set_aging_time");

exit:
	return SAI_ERROR_2_ERRNO(status);
}

/*
 * Register FDB event function.
 */
static int
__fdb_register_fdb_event_callback(sai_fdb_event_notification_fn fdb_event_cb)
{
	ops_sai_fdb_event_register(fdb_event_cb);
    return 0;
}

/*
 * Flush dynamic FDB entrys.
 */
static int
__fdb_flush_entrys(int options, handle_t id, int vid)
{
	sai_status_t                    status      = SAI_STATUS_SUCCESS;
	const struct ops_sai_api_class  *sai_api    = ops_sai_api_get_instance();
	sai_attribute_t    				attr[3];
	int 							attr_count  = 0;

	memset(attr,0,sizeof(sai_attribute_t) * 3);

	switch(options)
	{
	case L2MAC_FLUSH_BY_VLAN:
		attr[0].id 			= SAI_FDB_FLUSH_ATTR_VLAN_ID;
		attr[0].value.u16 	= vid;
		attr_count 			= 1;
		break;

	case L2MAC_FLUSH_BY_TRUNK:
	case L2MAC_FLUSH_BY_PORT:
		attr[0].id 			= SAI_FDB_FLUSH_ATTR_PORT_ID;
		attr[0].value.oid 	= id.data;
		attr_count 			= 1;
		break;

	case L2MAC_FLUSH_BY_TRUNK_VLAN:
	case L2MAC_FLUSH_BY_PORT_VLAN:
		attr[0].id 			= SAI_FDB_FLUSH_ATTR_PORT_ID;
		attr[0].value.oid 	= id.data;
		attr[1].id 			= SAI_FDB_FLUSH_ATTR_VLAN_ID;
		attr[1].value.u16 	= vid;
		attr_count 			= 2;
		break;

	case L2MAC_FLUSH_ALL:
		attr_count 			= 0;
		break;

	default:
		status = SAI_STATUS_INVALID_PARAMETER;
		goto exit;
	}

	status = sai_api->fdb_api->flush_fdb_entries(attr_count,attr);

exit:
    return SAI_ERROR_2_ERRNO(status);
}


DEFINE_GENERIC_CLASS(struct fdb_class, fdb) = {
        .init 				= __fdb_init,
        .set_aging_time 	= __fdb_set_aging_time,
        .register_fdb_event_callback = __fdb_register_fdb_event_callback,
        .flush_entrys 		= __fdb_flush_entrys,
        .deinit 			= __fdb_deinit,
};

DEFINE_GENERIC_CLASS_GETTER(struct fdb_class, fdb);
