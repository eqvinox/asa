#ifndef _SSARUN_H
#define _SSARUN_H

#include "asaproc.h"

struct ssav_line;

enum ssar_redoflags {
	SSAR_COLO = (1 << 0),
	SSAR_REND = (1 << 1),
	SSAR_WRAP = (1 << 2),
};

extern enum ssar_redoflags ssar_eval(struct ssav_line *l, double time);

struct assa_alloc {
	struct assa_alloc *next;
	struct ssav_line *line;
};

struct assa_layer {
	struct assa_layer *next;
	long int layer;

	struct assa_alloc *allocs, **curpos;
};

struct assa_env {
	struct assa_layer *firstlayer;
};

struct ssar_nodegroup {
	struct assp_frame *frame;
};

extern void assa_start(struct assa_env *ae);
extern enum ssar_redoflags assa_realloc(struct assa_env *ae,
	struct ssav_line *l, enum ssar_redoflags prev);
extern void assa_end(struct assa_env *ae);

void ssgl_prepare(struct ssav_line *l);
void ssgl_dispose(struct ssav_line *l);

#endif /* _SSARUN_H */
