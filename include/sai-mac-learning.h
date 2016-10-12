/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef __SAI_MAC_LEARNING_H__
#define __SAI_MAC_LEARNING_H__ 1

#include "openvswitch/vlog.h"
#include <mac-learning-plugin.h>
#include <plugin-extensions.h>

extern int sai_mac_learning_init(void);

extern int sai_mac_learning_get_hmap(struct mlearn_hmap **mhmap);

extern int sai_mac_learning_l2_addr_flush_handler(mac_flush_params_t *settings);

extern int sai_mac_learning_l2_addr_flush_by_port(const char *name);
extern int sai_mac_learning_l2_addr_flush_by_tid(int tid);
extern int sai_mac_learning_l2_addr_flush_by_vlan(int vid);

#endif /* __SAI_MAC_LEARNING_H__ */
