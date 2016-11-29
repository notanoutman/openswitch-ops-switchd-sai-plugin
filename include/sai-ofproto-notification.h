#ifndef NOTIFICATION_BLOCKS_H
#define NOTIFICATION_BLOCKS_H 1

#include <sai.h>
#include <netdev-provider.h>

#define NO_PRIORITY  UINT_MAX

enum notification_id {
    BLK_NOTIFICATION_SWITCH_PACKET = 0,
    BLK_NOTIFICATION_FDB,
    
    MAX_NOTIFICATION_BLOCKS_NUM,
};

struct notification_params{
    struct packet_params{
        sai_hostif_trap_id_t    trap_id;
        struct netdev           *netdev_;
        sai_uint16_t            vlan_id;
        void                    *buffer;
        sai_size_t              buffer_size;
    }packet_params;
};

int execute_notification_block(struct notification_params *params, enum notification_id notification_id);
int register_notification_callback(void (*callback_handler)(struct notification_params*),
                                  enum notification_id notification_id, unsigned int priority);

#endif /* reconfigure-blocks.h */
