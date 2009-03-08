/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
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

extern void ssgl_prepare(struct ssa_vm *vm, struct ssav_line *l);
extern void ssgl_dispose(struct ssav_line *l);

extern void ssar_dispose(struct ssav_line *l);

extern void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg,
	struct csri_frame *dst);
extern void ssar_flush(struct ssa_vm *vm);

#endif /* _SSARUN_H */
