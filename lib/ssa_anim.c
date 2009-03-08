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

#include "common.h"

#include <math.h>

#include "ssavm.h"
#include "ssarun.h"

#define accel_epsilon 0.0001

union aux {
	double dval;
	colour_t colour;
	FT_Pos pos;
};

static void ssar_apply(char *dest, struct ssav_controller *ctr,
	double time)
{
	double f0, f1;
	long fl0, fl1;
	colour_t temp;
	double in = time - ctr->t1;

	union aux *deste = (union aux *)(dest + ctr->offset);

	if (ctr->accel > (1.0 - accel_epsilon)
		&& ctr->accel < (1.0 + accel_epsilon))
		f0 = in * ctr->length_rez;
	else
		f0 = pow(in * ctr->length_rez, ctr->accel);
	if (f0 < 0.0)
		f0 = 0.0;
	if (f0 > 1.0)
		f0 = 1.0;
	f1 = 1.0 - f0;

	switch (ctr->type) {
	case SSAVC_MATRIX:
		deste->dval = f1 * deste->dval + f0 * ctr->nextval.dval;
		break;
	case SSAVC_FTPOS:
		deste->pos = (FT_Pos)(f1 * deste->pos + f0 * ctr->nextval.pos);
		break;
	case SSAVC_COLOUR:
		fl0 = (long)(f0 * 256);
		fl1 = 256 - fl0;
#define apply(field) temp.c.field = (colour_one_t)( \
			(deste->colour.c.field * fl1 + ctr->nextval.colour.val.c.field * fl0) \
				>> 8);
		apply(r);
		apply(g);
		apply(b);
		apply(a);
		deste->colour.l &= ~ctr->nextval.colour.mask.l;
		deste->colour.l |= temp.l;
		break;
	case SSAVC_NONE:
	case SSAVC_COUNT:
		;
	};
}

enum ssar_redoflags ssar_eval(struct ssa_vm *vm, struct ssav_line *l,
	double ftime, enum ssar_redoflags fl)
{
	double in = ftime - l->start;
	struct ssav_node *n = l->node_first;
	struct ssav_params *p = NULL;
	unsigned c;

	memcpy(&l->active, &l->base, sizeof(struct ssav_lineparams));
	if (l->active.clip.xMax == -1)
		l->active.clip.xMax = vm->playres.x;
	if (l->active.clip.yMax == -1)
		l->active.clip.yMax = vm->playres.y;
	if (l->nctr) {
		fl |= SSAR_REND | SSAR_WRAP;
		for (c = 0; c < l->nctr; c++)
			ssar_apply((char *)l, &l->ctrs[c], in);
	}

	while (n) {
		if (n->params != p) {
			p = n->params;
			if (p->finalized) {
				memcpy(p->finalized, p, sizeof(*p->finalized));
				for (c = 0; c < p->nctr; c++) {
					ssar_apply((char *)p->finalized,
						&p->ctrs[c], in);
					if (p->ctrs[c].type == SSAVC_MATRIX)
						fl |= SSAR_REND | SSAR_WRAP;
				}
			}
		}
		n = n->next;
	}
	return fl;
}


