/* #include(LICENSE) */
/* state: clean, commented, in-devel */
#ifndef _SSA_REND_H
#define _SSA_REND_H

#include "ssa.h"

/** prepare for rendering.
 * ssa_rend neets to already be allocated and have the ssa member set */
extern int ssa_prepare(struct ssa_renderer *ssa_rend);

extern void ssa_render(struct ssa_renderer *ssa_rend, double timestamp,
	unsigned char *dst, unsigned w, unsigned h);

#endif /* _SSA_REND_H */
