#ifndef SAI_OFPROTO_SFLOW_H
#define SAI_OFPROTO_SFLOW_H 1

#include "openflow/openflow.h"
#include "util.h"

struct ofproto_sflow_options;
struct ofport;

struct sai_sflow *sai_sflow_create(void);
void sai_sflow_add_port(struct sai_sflow *ds, struct ofport *ofport, uint32_t hw_lane_id);
void sai_sflow_del_port(struct sai_sflow *, uint32_t hw_lane_id);
bool sai_sflow_port_in_list(struct sai_sflow *ds, uint32_t hw_lane_id);
void sai_sflow_destroy(struct sai_sflow *);
void sai_sflow_clear(struct sai_sflow *);
void sai_sflow_set_options(struct sai_sflow *, const struct ofproto_sflow_options *);
void sai_sflow_run(struct sai_sflow *);
void sai_sflow_wait(struct sai_sflow *);

void sai_sflow_init();

void
sai_sflow_received(struct sai_sflow *ds,void *buffer, size_t buffer_size, uint32_t hw_lane_id);

#endif

