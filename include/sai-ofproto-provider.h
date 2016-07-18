/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef OFPROTO_SIM_PROVIDER_H
#define OFPROTO_SIM_PROVIDER_H 1

#include <ofproto/ofproto-provider.h>
#include <sai-handle.h>

void ofproto_sai_register(void);

int ofbundle_get_port_name_by_handle_id(handle_t, char*);
int ofbundle_get_handle_id_by_port_name(const char *, handle_t *);

#endif /* sai-ofproto-provider.h */
