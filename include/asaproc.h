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

#ifndef _ASAPROC_H
#define _ASAPROC_H

struct assp_frameref;

#include "ssa.h"
#include "asafont.h"

typedef union _cell {
	unsigned char e[4];
	unsigned int d;
} cell;

typedef struct _cellline {
	unsigned first;
	unsigned last;
	cell data[1];
} cellline;

struct assp_fgroup {
	struct asa_frame *active;

	unsigned w, h;
	cellline *unused;
	struct assp_frameref *issued;

	unsigned resvsize, ptr;
	cellline *reservoir[1];
};

struct assp_frame {
	struct assp_fgroup *group;
	colour_t colours[4];
	cellline *lines[1];
};

struct assp_param {
	struct assp_frame *f;
	int cx0, cx1, cy0, cy1;
	int xo, yo;
	int elem;
};

struct assp_frameref {
	struct assp_frameref *next;
	struct assp_frame *frame;
};

extern struct assp_fgroup *assp_fgroupnew(unsigned w, unsigned h);
extern void assp_fgroupfree(struct assp_fgroup *g);

extern void assp_framenew(struct assp_frameref *ng, struct assp_fgroup *g);
f_fptr extern void assp_spanfunc(int y, int count, const FT_Span *spans, void *user);
extern void assp_framefree(struct assp_frameref *ng);

extern void asar_commit(struct assp_frame *f);

#endif /* _ASAPROC_H */

