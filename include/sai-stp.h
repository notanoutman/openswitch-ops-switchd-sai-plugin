/*
 * Copyright centec Networks Inc., Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_STP_H
#define SAI_STP_H 1

#include <sai.h>
#include <sai-common.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

struct stp_class {
    /**
     * Initialize STPs.
     */
    void (*init)(void);
    /**
     * Creates STP.
     *
     * @param[out] stpid   - stp ID.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*create)(int *stpid);
    /**
     * Removes STP.
     *
     * @param[in] stpid   - stp ID.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*remove)(int stpid);
    /**
     * Add vlan to stp.
     *
     * @param[in] stpid - stp ID.
     * @param[in] vid   - vlan id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*add_vlan)(int   stpid,
                    int   vid);
    /**
     * Remove vlan from stp.
     *
     * @param[in] stpid - stp ID.
     * @param[in] vid   - vlan id.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*remove_vlan)(int   stpid,
                       int   vid);

    /**
     * Set port state in stp.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] hw_id   - port label id.
     * @param[in] port_state  - port state <enum mstp_instance_port_state>.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*set_port_state)(int        lagid,
                          uint32_t   hw_id,
                          int        port_state);

    /**
     * Get port state in stp.
     *
     * @param[in] lagid   - Lag ID.
     * @param[in] hw_id   - port label id.
     * @param[out] port_state  - port state <enum mstp_instance_port_state>.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*get_port_state)(int        lagid,
                          uint32_t   hw_id,
                          int        *port_state);

    /**
     * Get default STP.
     *
     * @param[out] stpid   - stp ID.
     *
     * @return 0, sai error converted to errno otherwise.
     */
    int (*get_default_instance)(int *stpid);

    /**
     * De-initialize STPs.
     */
    void (*deinit)(void);
};

DECLARE_GENERIC_CLASS_GETTER(struct stp_class, stp);

#define ops_sai_stp_class_generic() (CLASS_GENERIC_GETTER(stp)())

#ifndef ops_sai_stp_class
#define ops_sai_stp_class           ops_sai_stp_class_generic
#endif

static inline void ops_sai_stp_init(void)
{
    ovs_assert(ops_sai_stp_class()->init);
    ops_sai_stp_class()->init();
}

static inline int ops_sai_stp_create(int *stpid)
{
    ovs_assert(ops_sai_stp_class()->create);
    return ops_sai_stp_class()->create(stpid);
}

static inline int ops_sai_stp_remove(int stpid)
{
    ovs_assert(ops_sai_stp_class()->remove);
    return ops_sai_stp_class()->remove(stpid);
}

static inline int ops_sai_stp_add_vlan(int   stpid,
                                              int   vid)
{
    ovs_assert(ops_sai_stp_class()->add_vlan);
    return ops_sai_stp_class()->add_vlan(stpid, vid);
}

static inline int ops_sai_stp_remove_vlan(int  stpid,
                                                int   vid)
{
    ovs_assert(ops_sai_stp_class()->remove_vlan);
    return ops_sai_stp_class()->remove_vlan(stpid, vid);
}

static inline int ops_sai_stp_set_port_state(int        lagid,
                                                  uint32_t   hw_id,
                                                  int        port_state)
{
    ovs_assert(ops_sai_stp_class()->set_port_state);
    return ops_sai_stp_class()->set_port_state(lagid, hw_id, port_state);
}

static inline int ops_sai_stp_get_port_state(int        lagid,
                                                  uint32_t   hw_id,
                                                  int        *port_state)
{
    ovs_assert(ops_sai_stp_class()->get_port_state);
    return ops_sai_stp_class()->get_port_state(lagid, hw_id, port_state);
}

static inline int ops_sai_stp_get_default_instance(int *stpid)
{
    ovs_assert(ops_sai_stp_class()->get_default_instance);
    return ops_sai_stp_class()->get_default_instance(stpid);
}

static inline void ops_sai_stp_deinit(void)
{
    ovs_assert(ops_sai_stp_class()->deinit);
    ops_sai_stp_class()->deinit();
}

#endif /* sai-lag.h */
