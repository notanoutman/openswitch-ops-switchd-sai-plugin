/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

//#include <sai-vendor-util.h>
#include <sai-log.h>
#include <stdlib.h>
#include <string.h>
//#include <packets.h>
//#include <socket-util.h>
//#include <netinet/in.h>

//#include <util.h>

VLOG_DEFINE_THIS_MODULE(centec_sai_util);

/**
 * Read base MAC address from EEPROM.
 * @param[out] mac pointer to MAC buffer.
 * @return sai_status_t.
 */
sai_status_t
ops_sai_vendor_base_mac_get(sai_mac_t mac)
{
    sai_status_t status = SAI_STATUS_SUCCESS;
    static sai_mac_t    _mac_addr = {0};

    if(0x0 == _mac_addr[1])
    {
        _mac_addr[0] = 0x00;
        _mac_addr[1] = 0x01;
        _mac_addr[2] = 0x02;
        _mac_addr[3] = rand() % 0xFF;
        _mac_addr[4] = rand() % 0xFF;
        _mac_addr[5] = rand() % 0xFF;
    }

    memcpy(mac,_mac_addr,sizeof(_mac_addr));

    return status;
}
