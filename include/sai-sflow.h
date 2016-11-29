/*
 * Copyright Mellanox Technologies, Ltd. 2001-2016.
 * This software product is licensed under Apache version 2, as detailed in
 * the COPYING file.
 */

#ifndef SAI_SFLOW_H
#define SAI_SFLOW_H 1

#include <sai.h>
#include <sai-common.h>
#include <sset.h>
#include <ofproto/ofproto.h>
#ifdef SAI_VENDOR
#include <sai-vendor-common.h>
#endif /* SAI_VENDOR */

struct sflow_class {
    /**
     * Initialize sflow.
     */
    void (*init)(void);

    /**
     * Create sflow.
     */
    int (*create)(handle_t *id, uint32_t rate);
    
    /**
     * Remove sflow.
     */
    int (*remove)(handle_t id);

    /**
     * Set Rate sflow.
     */
    int (*set_rate)(handle_t id, uint32_t rate);

    /**
     * De-initialize sflow.
     */
    void (*deinit)(void);
};

DECLARE_GENERIC_CLASS_GETTER(struct sflow_class, sflow);

#define ops_sai_sflow_class_generic() (CLASS_GENERIC_GETTER(sflow)())

#ifndef ops_sai_sflow_class
#define ops_sai_sflow_class           ops_sai_sflow_class_generic
#endif

static inline void ops_sai_sflow_init(void)
{
    ovs_assert(ops_sai_sflow_class()->init);
    ops_sai_sflow_class()->init();
}

static inline int ops_sai_sflow_create(handle_t *id, uint32_t rate)
{
    ovs_assert(ops_sai_sflow_class()->create);
    return ops_sai_sflow_class()->create(id,rate);
}

static inline int ops_sai_sflow_remove(handle_t id)
{
    ovs_assert(ops_sai_sflow_class()->remove);
    return ops_sai_sflow_class()->remove(id);
}

static inline int ops_sai_sflow_set_rate(handle_t id, uint32_t rate)
{
    ovs_assert(ops_sai_sflow_class()->set_rate);
    return ops_sai_sflow_class()->set_rate(id, rate);
}

static inline void ops_sai_sflow_deinit(void)
{
    ovs_assert(ops_sai_sflow_class()->deinit);
    ops_sai_sflow_class()->deinit();
}

#endif /* sai-sflow.h */
