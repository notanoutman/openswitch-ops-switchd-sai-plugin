/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

//#include <sai-vendor-util.h>
#include <sai-log.h>

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

	mac[0] = 0x11;
	mac[1] = 0x22;
	mac[2] = 0x33;
	mac[3] = 0x44;
	mac[4] = 0x55;
	mac[5] = 0x66;

	return status;
}
