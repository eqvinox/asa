#ifndef _SSAWRAP_H
#define _SSAWRAP_H

#include "ssavm.h"
#include "asafont.h"

struct ssa_wrap_env {
	struct ssav_unit *cur_unit;
	struct ssav_line *vl;
	unsigned pos;
	unsigned hit_space;
};

extern void ssaw_put(struct ssa_wrap_env *we, struct ssa_node *n,
	struct ssav_node *vn, struct asa_font *fnt);
extern void ssaw_prepare(struct ssa_wrap_env *we, struct ssav_line *vl);
extern void ssaw_finish(struct ssa_wrap_env *we);

#endif /* _SSAWRAP_H */
