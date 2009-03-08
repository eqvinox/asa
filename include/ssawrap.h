/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005  David Lamparter
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file in the documentation and/or
 *    other materials provided with the distribution.
 *
 ****************************************************************************/

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
