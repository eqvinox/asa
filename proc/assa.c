/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005  David Lamparter
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

#include "common.h"
#include "ssavm.h"
#include "ssarun.h"

void assa_start(struct assa_env *ae)
{
	struct assa_layer *lay = ae->firstlayer;

	while (lay) {
		lay->curpos = &lay->allocs;
		lay = lay->next;
	}
}

static struct assa_layer *assa_getlayer(struct assa_env *ae, long int layer)
{
	struct assa_layer **prev = &ae->firstlayer, *newl;

	while (*prev && (*prev)->layer < layer)
		prev = &(*prev)->next;
	if (*prev && (*prev)->layer == layer)
		return *prev;

	newl = xmalloc(sizeof(*newl));
	newl->layer = layer;
	newl->next = *prev;
	newl->allocs = NULL;
	newl->curpos = &newl->allocs;
	*prev = newl;
	return newl;
}

static void assa_trash(struct assa_layer *lay)
{
	struct assa_alloc *fptr, *fnext;

	fptr = *lay->curpos;
	*lay->curpos = NULL;
	while (fptr) {
		fnext = fptr->next;
#if SSA_DEBUG
		fprintf(stderr, "disposing of line %d\n",
			fptr->line->input->no);
#endif
		ssgl_dispose(fptr->line);
		xfree(fptr);
		fptr = fnext;
	}
}

static void assa_wrap(struct assa_layer *lay, struct ssav_line *l)
{
	struct assa_alloc *newa;
	newa = xmalloc(sizeof(*newa));

#if SSA_DEBUG
	fprintf(stderr, "rewrapping line %d\n", l->input->no);
#endif
	ssgl_prepare(l);

	newa->next = NULL;
	newa->line = l;
	*lay->curpos = newa;
	lay->curpos = &newa->next;
}

enum ssar_redoflags assa_realloc(struct assa_env *ae,
	struct ssav_line *l, enum ssar_redoflags prev)
{
	struct assa_layer *lay = assa_getlayer(ae, l->ass_layer);

	if (*lay->curpos
		&& (*lay->curpos)->line == l
		&& !(prev & SSAR_WRAP)) 
		lay->curpos = &(*lay->curpos)->next;
	else {
		assa_trash(lay);
		prev |= SSAR_WRAP | SSAR_REND;
		assa_wrap(lay, l);
	}
		
	return prev;
}

void assa_end(struct assa_env *ae)
{
	struct assa_layer *lay = ae->firstlayer;

	while (lay) {
		assa_trash(lay);
		lay = lay->next;
	}
}

void assa_setup(struct ssa_vm *vm, unsigned width, unsigned height)
{
	/* initialize vm->ae here too */
	if (vm->playresx != 0.0 && vm->playresy != 0.0) {
		vm->res.x = (int)(vm->playresx * 1./65536.);
		vm->res.y = (int)(vm->playresy * 1./65536.);
		/* should probably create scaling matrix here */
	} else {
		vm->res.x = width << 16;
		vm->res.y = height << 16;
		/* should set scaling matrix to identity here */
	}
}

