/*
 * Copyright Centec Networks Inc. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_LAG_H
#define SAI_LAG_H 1


typedef struct ops_sai_lag_data {
    struct hmap_node    hmap_lag_data;

    int                 lag_id;
    int                 lag_mode;                       // Centec LAG hash mode.
    int                 hw_created;                     // Boolean indicating if this
                                                        // LAG has been created in in h/w.
    unsigned long       *pbmp_ports;                    // Attached ports
    unsigned long       *pbmp_egr_en;                   // Ports with egress enabled.
} ops_sai_lag_data_t;


int ops_sai_lag_create(int *lag_idp);
int ops_sai_lag_destroy(int lag_idp);

#if 0
void ops_sai_lag_attach_ports(int lag_id, unsigned long *pbm);
void ops_sai_lag_egress_enable_ports(int lag_id, unsigned long *pbm);
void ops_sai_lag_set_balance_mode(int lag_id, int lag_mode);
#endif

int __ops_sai_lag_attach_port(int lag_id, int hw_port);
int __ops_sai_lag_detach_port(int lag_id, int hw_port);
int __ops_sai_lag_egress_enable_port(int lag_id, int hw_port, int enable);


#endif /* sai-lag.h */
