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

extern enum ssar_redoflags ssar_eval(struct ssa_vm *vm, struct ssav_line *l,
	double time, enum ssar_redoflags fl);

struct assa_alloc {
	struct assa_alloc *next;
	struct ssav_line *line;
};

struct assa_layer {
	struct assa_layer *next;
	long int layer;

	struct assa_alloc *allocs, **curpos;
};

extern void assa_setup(struct ssa_vm *vm, unsigned width, unsigned height);
extern enum ssar_redoflags assa_start(struct ssa_vm *vm);
extern enum ssar_redoflags assa_realloc(struct ssa_vm *vm,
	struct ssav_line *l, enum ssar_redoflags prev);
extern void assa_end(struct ssa_vm *vm);

void ssgl_prepare(struct ssav_line *l);
void ssgl_dispose(struct ssav_line *l);

extern void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg);
extern void ssar_flush(struct ssa_vm *vm);

#endif /* _SSARUN_H */
