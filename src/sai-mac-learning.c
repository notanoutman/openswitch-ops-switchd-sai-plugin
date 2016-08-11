/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */
/* by xuwj */
#define __BOOL_DEFINED
/* end */
#include "hmap.h"
#include "ofproto/ofproto.h"
#include "packets.h"
#include "errno.h"
#include "sai-mac-learning.h"
#include "ovs-thread.h"
#include <sai-api-class.h>
#include <sai-netdev.h>
#include <sai-fdb.h>
#include <sai-handle.h>
#include <sai-ofproto-provider.h>
#include <netinet/ether.h>

VLOG_DEFINE_THIS_MODULE(sai_mac_learning);

static struct vlog_rate_limit mac_learning_rl = VLOG_RATE_LIMIT_INIT(5, 20);

/*
 * The buffers are defined as 2 because:
 *    To allow simultaneous read access to bridge.c and ops-mac-learning.c code
 *    threads will be: __init thread, switchd main thread and thread created for
 *    bcm callback without worrying about wait for acquiring the lock.
 */
#define MAX_BUFFERS   2

/*
 * mlearn_mutex is the mutex going to be used to access the all_macs_learnt and
 * what hmap is currently in use.
 */
struct ovs_mutex mlearn_mutex = OVS_MUTEX_INITIALIZER;

/*
 * all_macs_learnt:
 *      It is for storing the MACs learnt.
 *
 * Threads that can access this data structure: thread, timer thead,
 * thread created by ASIC for the traversal.
 */
struct mlearn_hmap all_macs_learnt[MAX_BUFFERS] OVS_GUARDED_BY(mlearn_mutex);

#define TIMER_THREAD_TIMEOUT 60
static pthread_t sai_timer_thread;

static int current_hmap_in_use = 0 OVS_GUARDED_BY(mlearn_mutex);

static struct mac_learning_plugin_interface *p_mlearn_plugin_interface = NULL;

static struct mac_learning_plugin_interface *
get_plugin_mac_learning_interface (void)
{
    struct plugin_extension_interface *p_extension = NULL;

    if (p_mlearn_plugin_interface) {
        return (p_mlearn_plugin_interface);
    }

    if (!find_plugin_extension(MAC_LEARNING_PLUGIN_INTERFACE_NAME,
                               MAC_LEARNING_PLUGIN_INTERFACE_MAJOR,
                               MAC_LEARNING_PLUGIN_INTERFACE_MINOR,
                               &p_extension)) {
        if (p_extension) {
            p_mlearn_plugin_interface = p_extension->plugin_interface;
            return (p_extension->plugin_interface);
        }
    }
    return NULL;
}

/*
 * Function: sai_mac_learning_table_hash_calc
 *
 * This function calculates the hash based on MAC address, vlan and hardware unit number.
 */
static uint32_t
sai_mac_learning_table_hash_calc(const struct eth_addr mac,
                                 const uint16_t  vlan,
                                 int             hw_unit)
{
    uint32_t hash =
        hash_2words(hash_uint64_basis(eth_addr_vlan_to_uint64(mac, vlan), 0),
                hw_unit);
    return (hash);
}

/*
 * Function: sai_mac_learning_table_is_full
 *
 * This function is used to find if the hmap has reached it's capacity or not.
 */
static bool
sai_mac_learning_table_is_full(const struct mlearn_hmap *mlearn_hmap)
{
    return ((mlearn_hmap->buffer).actual_size == (mlearn_hmap->buffer).size);
}

static void
sai_mac_learning_get_port_name_from_id(handle_t port_id, char* port_name)
{
    if(SAI_OBJECT_TYPE_LAG == sai_object_type_query(port_id.data))
    {
        ofbundle_get_port_name_by_handle_id(port_id, port_name);
    } else if (SAI_OBJECT_TYPE_PORT == sai_object_type_query(port_id.data)){
        netdev_sai_get_port_name_by_handle_id(port_id, port_name);
    }

    return ;
}

static int
sai_mac_learning_get_id_from_port_name(char* port_name, handle_t *hand_id)
{
    uint32_t hw_id = 0;

    if(true == netdev_sai_get_hw_id_by_name(port_name, &hw_id))  {
	 VLOG_INFO("%s: hw_id = 0x%x ", __FUNCTION__, hw_id);
        hand_id->data = ops_sai_api_hw_id2port_id(hw_id);
        return 0;
    } else if(true == ofbundle_get_handle_id_by_port_name(port_name,hand_id)) {
        VLOG_INFO("%s: port_id: %lx ", __FUNCTION__,hand_id->data);
        return 0;
    }

    return -1;
}

/*
 * Function: sai_mac_learning_entry_add
 *
 * This function is used to add the entries in the all_macs_learnt hmap.
 *
 * If the entry is already present, it is modified or else it's created.
 */
static void
sai_mac_learning_entry_add(	struct mlearn_hmap 	*hmap_entry,
							const uint8_t 		mac[ETH_ADDR_LEN],
							const int16_t 		vlan,
							sai_attribute_t 	*attr,
							uint32_t 			attr_count,
							const mac_event 	event)
{
    struct mlearn_hmap_node *entry         = NULL;
    struct eth_addr 		mac_eth;
    uint32_t 				hash 		= 0;
    int 					actual_size = 0;
    char 					port_name[PORT_NAME_SIZE] = "";
    bool 					found 		= false;
    handle_t 		        port_id     = {0};
    int32_t 				attr_idx 	= 0;

    memcpy(mac_eth.ea, mac, sizeof(mac_eth.ea));
    hash = sai_mac_learning_table_hash_calc(mac_eth, vlan, 0);
    actual_size = (hmap_entry->buffer).actual_size;
    memset((void*)port_name, 0, sizeof(port_name));

    for(attr_idx = 0; attr_idx < attr_count; attr_idx++) {
        if(attr[attr_idx].id == SAI_FDB_ENTRY_ATTR_PORT_ID) {
            port_id.data = attr[attr_idx].value.oid;
        }
    }

	/* get port_name from sai_object_id_t, eg "lag1", "1" */
	sai_mac_learning_get_port_name_from_id(port_id, port_name);

    if (!strlen(port_name)) {
        VLOG_ERR("%s: not able to find port name for port_id: %lx ", __FUNCTION__, port_id.data);
        return;
    }

    HMAP_FOR_EACH_WITH_HASH (entry, hmap_node, hash,
                                 &(hmap_entry->table)) {
        VLOG_DBG("%s: cur_tables, port: %s, oper: %d, vlan: %d, MAC: %s",
                     __FUNCTION__, entry->port_name, entry->oper, entry->vlan,
                     ether_ntoa((struct ether_addr *)entry->mac.ea));

        if ((entry->vlan == vlan) && eth_addr_equals(entry->mac, mac_eth)) {
            if ((event == MLEARN_ADD) && (entry->oper == MLEARN_DEL)) {
                if (0 == strcmp(port_name,entry->port_name) /* port_id == entry->port */) {
                    /*
                     * remove this entry from hmap
                     */
                    entry->oper = MLEARN_UNDEFINED;
		      entry->vlan = 4096;
                    VLOG_DBG("%s: move event, entry found removing,add->undefined, vlan: %d, MAC: %s", __FUNCTION__,
                                vlan, ether_ntoa((struct ether_addr *)entry->mac.ea));
                    found = true;
                }
            } else if ((event == MLEARN_DEL) && (entry->oper == MLEARN_ADD)) {
                /*
                 * remove this entry from hmap
                 */
                if (0 == strcmp(port_name,entry->port_name) /* port_id == entry->port */) {
                    /*
                     * remove this entry from hmap
                     */
                    entry->oper = MLEARN_UNDEFINED;
		      entry->vlan = 4096;
                    VLOG_DBG("%s: move event, entry found removing,del->undefined, vlan: %d, MAC: %s", __FUNCTION__,
                                vlan, ether_ntoa((struct ether_addr *)entry->mac.ea));
                    found = true;
                }
            } else if ((event == MLEARN_DEL) && (entry->oper == MLEARN_DEL)) {
                /*
                 * update this entry from hmap
                 */
                if (0 == strcmp(port_name,entry->port_name) /* port_id == entry->port */) {
                    /*
                     * update this entry from hmap
                     */
                    VLOG_DBG("%s: update event, entry found removing to removing", __FUNCTION__);
                    found = true;
                }
            } else if ((event == MLEARN_ADD) && (entry->oper == MLEARN_ADD)) {
                /*
                 * update this entry from hmap
                 */
                if (0 == strcmp(port_name,entry->port_name) /* port_id == entry->port */) {
                    /*
                     * update this entry from hmap
                     */
                    VLOG_DBG("%s: update event, entry found adding to adding", __FUNCTION__);
                    found = true;
                }
            }
        }
    }

    if (!found) {
        if (actual_size < (hmap_entry->buffer).size) {
            struct mlearn_hmap_node *mlearn_node =
                                    &((hmap_entry->buffer).nodes[actual_size]);
            VLOG_DBG("%s: move_event, port: %lx, oper: %d, vlan: %d, MAC: %s",
                     __FUNCTION__, port_id.data, event, vlan,
                     ether_ntoa((struct ether_addr *)mac));

            memcpy(&mlearn_node->mac, &mac_eth, sizeof(mac_eth));
            mlearn_node->vlan 		= vlan;
            mlearn_node->hw_unit 	= 0;
            mlearn_node->oper 		= event;
            strncpy(mlearn_node->port_name, port_name, PORT_NAME_SIZE);
            hmap_insert(&hmap_entry->table,
                        &(mlearn_node->hmap_node),
                        hash);
            (hmap_entry->buffer).actual_size++;
        } else {
            VLOG_ERR("Error, not able to insert elements in hmap, size is: %u\n",
                     hmap_entry->buffer.actual_size);
        }
    }
}

/*
 * Function: sai_mac_learning_clear_hmap
 *
 * This function clears the hmap and the buffer for storing the hmap nodes.
 */
static void
sai_mac_learning_clear_hmap (struct mlearn_hmap *mhmap)
{
    if (mhmap) {
        memset(&(mhmap->buffer), 0, sizeof(mhmap->buffer));
        mhmap->buffer.size = BUFFER_SIZE;
        hmap_clear(&(mhmap->table));
    }
}

/*
 * Function: sai_mac_learning_run
 *
 * This function will be invoked when either of the two conditions
 * are satisfied:
 * 1. current in use hmap for storing all macs learnt is full
 * 2. timer thread times out
 *
 * This function will check if there is any new MACs learnt, if yes,
 * then it triggers callback from bridge.
 * Also it changes the current hmap in use.
 *
 * current_hmap_in_use = current_hmap_in_use ^ 1 is used to toggle
 * the current hmap in use as the buffers are 2.
 */
static int
sai_mac_learning_run (void)
{
    struct mac_learning_plugin_interface *p_mlearn_interface = NULL;
    p_mlearn_interface = get_plugin_mac_learning_interface();
    if (p_mlearn_interface) {
        ovs_mutex_lock(&mlearn_mutex);
        if (hmap_count(&(all_macs_learnt[current_hmap_in_use].table))) {
            p_mlearn_interface->mac_learning_trigger_callback();
            current_hmap_in_use = current_hmap_in_use ^ 1;
            sai_mac_learning_clear_hmap(&all_macs_learnt[current_hmap_in_use]);
        }
        ovs_mutex_unlock(&mlearn_mutex);
    } else {
        VLOG_ERR("%s: Unable to find mac learning plugin interface",
                 __FUNCTION__);
    }

    return (0);
}

/*
 * This function is for getting callback from ASIC
 * for MAC learning.
 */
static void
sai_mac_learning_fdb_event_cb(  uint32_t count,
								         sai_fdb_event_notification_data_t *data)
{
    uint32_t    fdb_index = 0;

    if(count <= 0 || NULL == data){
        return;
    }

    for(fdb_index = 0; fdb_index < count; fdb_index++)
    {
        switch(data[fdb_index].event_type)
        {
            case SAI_FDB_EVENT_LEARNED:
                ovs_mutex_lock(&mlearn_mutex);
                sai_mac_learning_entry_add(&all_macs_learnt[current_hmap_in_use],
                              data[fdb_index].fdb_entry.mac_address,
                              data[fdb_index].fdb_entry.vlan_id,
                              data[fdb_index].attr,
                              data[fdb_index].attr_count,
                              MLEARN_ADD);
                ovs_mutex_unlock(&mlearn_mutex);
                break;

            case SAI_FDB_EVENT_AGED:
                ovs_mutex_lock(&mlearn_mutex);
                sai_mac_learning_entry_add(&all_macs_learnt[current_hmap_in_use],
                              data[fdb_index].fdb_entry.mac_address,
                              data[fdb_index].fdb_entry.vlan_id,
                              data[fdb_index].attr,
                              data[fdb_index].attr_count,
                              MLEARN_DEL);
                ovs_mutex_unlock(&mlearn_mutex);
                break;

            case SAI_FDB_EVENT_FLUSHED:
            default:
                break;
        }

        /*
         * notify vswitchd
         */
        if (sai_mac_learning_table_is_full(&all_macs_learnt[current_hmap_in_use])) {
            sai_mac_learning_run();
        }
    }
}


static void *
sai_mac_learning_timer_main (void * args OVS_UNUSED)
{
    while (true) {
        xsleep(TIMER_THREAD_TIMEOUT); 		/* in seconds */
        sai_mac_learning_run();
    }

    return (NULL);
}

/*
 * Function: sai_mac_learning_get_hmap
 *
 * This function will be invoked by the mac learning plugin code,
 * so that the switchd main thread can get the new MACs learnt/deleted
 * and can update the MAC table in the OVSDB accordingly.
 */
int sai_mac_learning_get_hmap(struct mlearn_hmap **mhmap)
{
    if (!mhmap) {
        VLOG_ERR("%s: Invalid argument", __FUNCTION__);
        return (EINVAL);
    }

    ovs_mutex_lock(&mlearn_mutex);
    if (hmap_count(&(all_macs_learnt[current_hmap_in_use ^ 1].table))) {
        *mhmap = &all_macs_learnt[current_hmap_in_use ^ 1];
    } else {
        *mhmap = NULL;
    }
    ovs_mutex_unlock(&mlearn_mutex);

    return (0);
}

/*
 * Function: sai_mac_learning_init
 *
 * This function is invoked in the ofproto __init.
 *
 * It initializes the hmaps, reserves the buffer capacity of the hmap
 * to avoid the time spent in the malloc and free.
 *
 * It also registers for the initial traversal of the MACs already
 * learnt in the ASIC for all hw_units.
 */
int
sai_mac_learning_init(void)
{
    int         idx     = 0;
    handle_t    id      = {0};

	/* init hmap */
    for (; idx < MAX_BUFFERS; idx++) {
        hmap_init(&(all_macs_learnt[idx].table));
        all_macs_learnt[idx].buffer.actual_size = 0;
        all_macs_learnt[idx].buffer.size = BUFFER_SIZE;
        hmap_reserve(&(all_macs_learnt[idx].table), BUFFER_SIZE);
    }

	/* register fdb event callback function */
    ops_sai_fdb_register_fdb_event_callback(sai_mac_learning_fdb_event_cb);

	/* use to run function sai_mac_learning_run() */
    sai_timer_thread = ovs_thread_create("ovs-sai-mac-learning-timer",
										 sai_mac_learning_timer_main,
										 NULL);

	/* flush all fdb entry */
	ops_sai_fdb_flush_entrys(L2MAC_FLUSH_ALL,id,0);

    return 0;
}

/*
 * Function: sai_mac_learning_l2_addr_flush_handler
 *
 * This function is invoked to flush MAC table entries on VLAN/PORT
 *
 */
int
sai_mac_learning_l2_addr_flush_handler(mac_flush_params_t *settings)
{
    int                 rc  = 0;
    handle_t            id  = HANDLE_INITIALIZAER;

    /* Get Harware Port */
    if (settings->options == L2MAC_FLUSH_BY_PORT
        || settings->options == L2MAC_FLUSH_BY_PORT_VLAN
        || settings->options == L2MAC_FLUSH_BY_TRUNK
        || settings->options == L2MAC_FLUSH_BY_TRUNK_VLAN) {
        rc = sai_mac_learning_get_id_from_port_name(settings->port_name, &id);
        if (rc) {
            VLOG_ERR_RL(&mac_learning_rl, "%s: %s name not found flags %u mode %d",
                        __FUNCTION__, settings->port_name,
                        settings->flags,
                        settings->options);

            return -1; /* Return error */
        }
    }

    return ops_sai_fdb_flush_entrys(settings->options, id, settings->vlan);
}
