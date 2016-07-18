/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_FDB_H
#define SAI_FDB_H 1

#include <sai.h>
#include <sai-common.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

struct fdb_class {
    /**
     * Initialize FDBs.
     */
    void (*init)(void);

	/**
     * Set dynamic FDB entry aging time.
     *
	 * @param[in] sec   - aging time in sec.
     *
     * @return 0, sai error converted to errno otherwise.
     */
	int  (*set_aging_time)(int sec);

	/**
     * Register FDB event function.
     *
	 * @param[in] fdb_event_cb   - fdb event callback function.
     *
     * @return 0, sai error converted to errno otherwise.
     */
	int  (*register_fdb_event_callback)(sai_fdb_event_notification_fn fdb_event_cb);

	/**
     * Flush dynamic FDB entrys.
     *
	 * @param[in] options   - mac_flush_params options <enum mac_flush_options>.
	 * @param[in] id        - port handle id or lag handle id.
	 * @param[in] vid       - vlan id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
	int (*flush_entrys)(int options, handle_t id, int vid);

    /**
     * De-initialize FDBs.
     */
    void (*deinit)(void);
};

DECLARE_GENERIC_CLASS_GETTER(struct fdb_class, fdb);

#define ops_sai_fdb_class_generic() (CLASS_GENERIC_GETTER(fdb)())

#ifndef ops_sai_fdb_class
#define ops_sai_fdb_class           ops_sai_fdb_class_generic
#endif

static inline void ops_sai_fdb_init(void)
{
    ovs_assert(ops_sai_fdb_class()->init);
    ops_sai_fdb_class()->init();
}

static inline int ops_sai_fdb_set_aging_time(int sec)
{
    ovs_assert(ops_sai_fdb_class()->set_aging_time);
    return ops_sai_fdb_class()->set_aging_time(sec);
}

static inline int ops_sai_fdb_register_fdb_event_callback(sai_fdb_event_notification_fn fdb_event_cb)
{
    ovs_assert(ops_sai_fdb_class()->register_fdb_event_callback);
    return ops_sai_fdb_class()->register_fdb_event_callback(fdb_event_cb);
}

static inline int ops_sai_fdb_flush_entrys(int options, handle_t id, int vid)
{
    ovs_assert(ops_sai_fdb_class()->flush_entrys);
    return ops_sai_fdb_class()->flush_entrys(options, id, vid);
}

static inline void ops_sai_fdb_deinit(void)
{
    ovs_assert(ops_sai_fdb_class()->deinit);
    ops_sai_fdb_class()->deinit();
}

#endif /* sai-fdb.h */
