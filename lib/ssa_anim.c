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

#include <math.h>

#include "common.h"
#include "ssavm.h"
#include "ssarun.h"

#define accel_epsilon 0.0001

union aux {
	double dval;
	colour_t colour;
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
	f1 = 1.0 - f0;

	switch (ctr->type) {
	case SSAVC_MATRIX:
		deste->dval = f1 * deste->dval + f0 * ctr->nextval.dval;
		break;
	case SSAVC_COLOUR:
		fl0 = (long)(f0 * 256);
		fl1 = 256 - fl0;
#define apply(field) temp.c.field = (deste->colour.c.field * fl1 + \
			ctr->nextval.colour.val.c.field * fl0) >> 8;
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

enum ssar_redoflags ssar_eval(struct ssav_line *l, double ftime)
{
	double in = ftime - l->start;
	struct ssav_node *n = l->node_first;
	struct ssav_params *p = NULL;
	enum ssar_redoflags fl = 0;

	while (n) {
		if (n->params != p) {
			p = n->params;
			if (p->finalized) {
				unsigned c;
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

