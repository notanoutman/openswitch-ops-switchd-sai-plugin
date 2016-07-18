/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_LAG_H
#define SAI_LAG_H 1

#include <sai.h>
#include <sai-common.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

struct lag_class {
    /**
     * Initialize Lags.
     */
    void (*init)(void);
    /**
     * Creates Lag.
     *
     * @param[out] lagid   - Lag ID.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*create)(int *lagid);
    /**
     * Removes Lag.
     *
     * @param[in] lagid   - Lag ID.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*remove)(int lagid);
    /**
     * Adds member port to lag.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] hw_id   - port label id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*member_port_add)(int        lagid,
                           uint32_t   hw_id);
    /**
     * Removes member port from lag.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] hw_id   - port label id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*member_port_del)(int        lagid,
                           uint32_t   hw_id);

    /**
     * Set traffic distribution to this port as part of LAG.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] hw_id   - port label id.
     * @param[in] enable  - enable set traffic distribution.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*set_tx_enable)(int        lagid,
                         uint32_t   hw_id,
                         bool       enable);

    /**
     * Set balance mode from lag.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] balance_mode   - balance mode <enum bond_mode>.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*set_balance_mode)(int   lagid,
                            int   balance_mode);

    /**
     * Get handle_id from lagid.
     *
     * @param[in]  lagid        - Lag ID.
     * @param[out] handle_id    - handle_id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*get_handle_id)(int        lagid,
                         handle_t   *handle_id);

    /**
     * De-initialize Lags.
     */
    void (*deinit)(void);
};

DECLARE_GENERIC_CLASS_GETTER(struct lag_class, lag);

#define ops_sai_lag_class_generic() (CLASS_GENERIC_GETTER(lag)())

#ifndef ops_sai_lag_class
#define ops_sai_lag_class           ops_sai_lag_class_generic
#endif

static inline void ops_sai_lag_init(void)
{
    ovs_assert(ops_sai_lag_class()->init);
    ops_sai_lag_class()->init();
}

static inline int ops_sai_lag_create(int *lagid)
{
    ovs_assert(ops_sai_lag_class()->create);
    return ops_sai_lag_class()->create(lagid);
}

static inline int ops_sai_lag_remove(int lagid)
{
    ovs_assert(ops_sai_lag_class()->remove);
    return ops_sai_lag_class()->remove(lagid);
}

static inline int ops_sai_lag_member_port_add(int        lagid,
                                                       uint32_t   hw_id)
{
    ovs_assert(ops_sai_lag_class()->member_port_add);
    return ops_sai_lag_class()->member_port_add(lagid, hw_id);
}

static inline int ops_sai_lag_member_port_del(int        lagid,
                                                      uint32_t   hw_id)
{
    ovs_assert(ops_sai_lag_class()->member_port_del);
    return ops_sai_lag_class()->member_port_del(lagid, hw_id);
}

static inline int ops_sai_lag_set_tx_enable(int        lagid,
                                                  uint32_t   hw_id,
                                                  bool       enable)
{
    ovs_assert(ops_sai_lag_class()->set_tx_enable);
    return ops_sai_lag_class()->set_tx_enable(lagid, hw_id, enable);
}

static inline int ops_sai_lag_set_balance_mode(int   lagid,
                                                       int   balance_mode)
{
    ovs_assert(ops_sai_lag_class()->set_balance_mode);
    return ops_sai_lag_class()->set_balance_mode(lagid, balance_mode);
}

static inline int ops_sai_lag_get_handle_id(int        lagid,
                                                  handle_t   *handle_id)
{
    ovs_assert(ops_sai_lag_class()->get_handle_id);
    return ops_sai_lag_class()->get_handle_id(lagid, handle_id);
}

static inline void ops_sai_lag_deinit(void)
{
    ovs_assert(ops_sai_lag_class()->deinit);
    ops_sai_lag_class()->deinit();
}

#endif /* sai-lag.h */
