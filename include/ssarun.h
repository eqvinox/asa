/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifndef _SSARUN_H
#define _SSARUN_H

#include "asaproc.h"

struct ssav_line;
struct ssa_vm;

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

extern void assa_setup(struct ssa_vm *v, unsigned width, unsigned height);
extern void assa_start(struct assa_env *ae);
extern enum ssar_redoflags assa_realloc(struct assa_env *ae,
	struct ssav_line *l, enum ssar_redoflags prev);
extern void assa_end(struct assa_env *ae);

void ssgl_prepare(struct ssav_line *l);
void ssgl_dispose(struct ssav_line *l);

#endif /* _SSARUN_H */
