/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#include <sai-netdev.h>
#include <sai-ofproto-provider.h>
#include <sai-log.h>
//#include "asic-plugin.h"

/* by xuwj: use plugins_register to load function */
#if 0
#define init libovs_sai_plugin_LTX_init
#define run libovs_sai_plugin_LTX_run
#define wait libovs_sai_plugin_LTX_wait
#define destroy libovs_sai_plugin_LTX_destroy
#define netdev_register libovs_sai_plugin_LTX_netdev_register
#define ofproto_register libovs_sai_plugin_LTX_ofproto_register
#define bufmon_register libovs_bcm_plugin_LTX_bufmon_register
#endif
/* end */

VLOG_DEFINE_THIS_MODULE(sai_plugin);

void
libovs_sai_plugin_LTX_init(void)
{
//    struct plugin_extension_interface sai_extension;

    SAI_API_TRACE_FN();
}

void
libovs_sai_plugin_LTX_run(void)
{
    SAI_API_TRACE_FN();
}

void
libovs_sai_plugin_LTX_wait(void)
{
    SAI_API_TRACE_FN();
}

void
libovs_sai_plugin_LTX_destroy(void)
{
    SAI_API_TRACE_FN();
}

void
libovs_sai_plugin_LTX_netdev_register(void)
{
    SAI_API_TRACE_FN();

    netdev_sai_register();
}

void
libovs_sai_plugin_LTX_ofproto_register(void)
{
    SAI_API_TRACE_FN();

    ofproto_sai_register();
}

void
libovs_bcm_plugin_LTX_bufmon_register(void)
{
    SAI_API_TRACE_NOT_IMPLEMENTED_FN();
}
