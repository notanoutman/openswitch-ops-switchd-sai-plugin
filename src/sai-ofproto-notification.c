#include <stdlib.h>
#include <errno.h>
#include "sai-ofproto-notification.h"
#include "openvswitch/vlog.h"
#include "list.h"

VLOG_DEFINE_THIS_MODULE(sai_ofproto_notification);

/* Node for a registered callback handler in a reconfigure block list */
struct notification_list_node{
    void (*callback_handler)(struct notification_params*);
    unsigned int priority;
    struct ovs_list node;
};

static bool notification_init = false;
static struct ovs_list** notification_list = NULL;

static int init_notification_blocks(void);
static int insert_node_on_notification(struct notification_list_node *new_node,
                               struct ovs_list *func_list);


int
register_notification_callback(void (*callback_handler)(struct notification_params*),
                           enum notification_id notification_id, unsigned int priority)
{
    struct notification_list_node *new_node;

    /* Initialize reconfigure lists */
    if (!notification_init) {
        if(init_notification_blocks()) {
            VLOG_ERR("Cannot initialize blocks");
            goto error;
        }
    }

    if (callback_handler == NULL) {
        VLOG_ERR("NULL callback function");
        goto error;
    }

    if ((notification_id < 0) || (notification_id >= MAX_NOTIFICATION_BLOCKS_NUM)) {
        VLOG_ERR("Invalid notification_id passed as parameter");
        goto error;
    }

    new_node = (struct notification_list_node *) xmalloc (sizeof(struct notification_list_node));
    new_node->callback_handler = callback_handler;
    new_node->priority = priority;
    if (insert_node_on_notification(new_node, notification_list[notification_id])) {
        VLOG_ERR("Failed to add node in block");
        goto error;
    }
    return 0;

error:
    return EINVAL;
}

/* Insert a new block list node in the given reconfigure block list. Node is
 * ordered by priority
 */
static int
insert_node_on_notification(struct notification_list_node *new_node, struct ovs_list *func_list)
{
    struct notification_list_node *notification_node;
    struct ovs_list *last_node;

    if (!func_list){
        VLOG_ERR("Invalid list passed as parameter");
        goto error;
    }

    /* If list is empty, insert node at the top */
    if (list_is_empty(func_list)) {
        list_push_back(func_list, &new_node->node);
        return 0;
    }

    /* If priority is higher than bottom element, insert node at the bottom */
    last_node = list_back(func_list);
    if (!last_node) {
        VLOG_ERR("Cannot get bottom element of list");
        goto error;
    }
    notification_node = CONTAINER_OF(last_node, struct notification_list_node, node);
    if ((new_node->priority) >= (notification_node->priority)) {
        list_push_back(func_list, &new_node->node);
        return 0;
    }

    /* Walk the list and insert node in between nodes */
    LIST_FOR_EACH(notification_node, node, func_list) {
        if ((notification_node->priority) > (new_node->priority)) {
            list_insert(&notification_node->node, &new_node->node);
            return 0;
        }
    }

 error:
    return EINVAL;
}

/* Initialize the list of blocks */
static int
init_notification_blocks(void)
{
    int notification_counter;
    notification_list = (struct ovs_list**) xcalloc (MAX_NOTIFICATION_BLOCKS_NUM,
                                            sizeof(struct ovs_list*));

    /* Initialize each of the Blocks */
    for (notification_counter = 0; notification_counter < MAX_NOTIFICATION_BLOCKS_NUM; notification_counter++) {
        notification_list[notification_counter] = (struct ovs_list *) xmalloc (sizeof(struct ovs_list));
        list_init(notification_list[notification_counter]);
    }

    notification_init = true;
    return 0;
}

/* Execute all registered callbacks for a given Reconfigure Block ordered by
 * priority
*/
int
execute_notification_block(struct notification_params *params, enum notification_id notification_id)
{
    struct notification_list_node *actual_node;

    /* Initialize reconfigure lists */
    if (!notification_init) {
        if(init_notification_blocks()) {
            VLOG_ERR("Cannot initialize blocks");
            goto error;
        }
    }

    if (!params) {
        VLOG_ERR("Invalid NULL params structure");
        goto error;
    }

    if ((notification_id < 0) || (notification_id >= MAX_NOTIFICATION_BLOCKS_NUM)) {
        VLOG_ERR("Invalid notification_id passed as parameter");
        goto error;
    }

    VLOG_DBG("Executing block %d of notification", notification_id);

    LIST_FOR_EACH(actual_node, node, notification_list[notification_id]) {
        if (!actual_node->callback_handler) {
            VLOG_ERR("Invalid function callback_handler found");
            goto error;
        }
        actual_node->callback_handler(params);
    }

    return 0;

 error:
    return EINVAL;
}
