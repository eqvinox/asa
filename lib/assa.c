/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005,2006  David Lamparter
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

enum ssar_redoflags assa_start(struct ssa_vm *vm)
{
	struct assa_layer *lay = vm->firstlayer;

	while (lay) {
		lay->curpos = &lay->allocs;
		lay = lay->next;
	}
	return vm->redoflags;
}

static struct assa_layer *assa_getlayer(struct ssa_vm *vm, long int layer)
{
	struct assa_layer **prev = &vm->firstlayer, *newl;

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

struct assa_rect {
	FT_Vector pos, size;

	int wrapdir;
	double xalign, yalign;
};

struct fitline {
	struct fitline *next;
	FT_Vector size;
	struct ssav_unit *startat, *endat;
};

/* first pass: split into lines and sum up their size */
static struct fitline *assa_fit_split(struct ssav_unit *u, FT_Pos width,
	FT_Pos *h_total)
{
	FT_Pos w_remain = -1;
	struct fitline *fl = NULL,
		*ret = fl, **upd = &ret;

	while (u) {
		if (u->size.x > w_remain) {
			if (fl) {
				*h_total += fl->size.y;
				fl->endat = u;
			}

			fl = malloc(sizeof(struct fitline));
			fl->next = *upd;
			*upd = fl;

			fl->size.x = fl->size.y = 0;
			fl->startat = u;
			w_remain = width;
		}

		w_remain -= u->size.x << 10;
		fl->size.x += u->size.x << 10;
		if ((u->height << 10) > fl->size.y)
			fl->size.y = u->height << 10;

		u = u->next;
	}
	fl->endat = NULL;
	*h_total += fl->size.y;

	return ret;
}

static void assa_fit_arrange(struct fitline *fl,
	struct assa_rect *r, FT_Pos h_total)
{
	FT_Pos y, x;

	y = r->pos.y + (FT_Pos)((double)(r->size.y - h_total) * r->yalign);
	if (r->wrapdir < 0)
		y += h_total;

	while (fl) {
		struct ssav_unit *u = fl->startat;
		x = r->pos.x + (FT_Pos)((double)(r->size.x - fl->size.x) * r->xalign);

		if (r->wrapdir > 0)
			y += fl->size.y;

		while (u != fl->endat) {
			u->final.y = y >> 10;
			u->final.x = x >> 10;
			x += u->size.x << 10;
			u = u->next;
		}

		if (r->wrapdir < 0)
			y -= fl->size.y;
		fl = fl->next;
	}
}

static void assa_fit(struct ssav_line *l, struct assa_rect *r)
{
	FT_Pos h_total = 0;
	struct fitline *fl, *del;

	fl = assa_fit_split(l->unit_first, r->size.x, &h_total);
	assa_fit_arrange(fl, r, h_total);

	for (del = fl; del; del = fl)
		fl = del->next, free(del);
}

static void assa_wrap(struct ssa_vm *vm, struct assa_layer *lay,
	struct ssav_line *l)
{
	struct assa_alloc *newa;
	struct assa_rect r;

	newa = xmalloc(sizeof(*newa));

#if SSA_DEBUG
	fprintf(stderr, "rewrapping line %d\n", l->input->no);
#endif
	ssgl_prepare(l);

	newa->next = NULL;
	newa->line = l;
	*lay->curpos = newa;
	lay->curpos = &newa->next;

	r.wrapdir = l->yalign  < 0.25 ? 1 : -1;
	r.xalign = l->xalign;
	r.yalign = l->yalign;
	if (l->pos) {
		r.pos.x = l->pos->v.pos.x * (long)(l->xalign * 65536.);
		r.size.x = (FT_Pos)(
			(double)(l->pos->v.pos.x << 16) * (1. - l->xalign)
			+ (double)(vm->res.x - (l->pos->v.pos.x << 16))
				* l->xalign);
		r.pos.y = l->pos->v.pos.y * (long)(l->yalign * 65536.);
		r.size.y = (FT_Pos)(
			(double)(l->pos->v.pos.y << 16) * (1. - l->yalign)
			+ (double)(vm->res.y - (l->pos->v.pos.y << 16))
				* l->yalign);
	} else {
		r.pos.x = l->marginl << 16;
		r.size.x = vm->res.x - ((l->marginl + l->marginr) << 16);
		r.pos.y = l->margint << 16;
		r.size.y = vm->res.y - ((l->margint + l->marginb) << 16);
	}
	assa_fit(l, &r);
}

enum ssar_redoflags assa_realloc(struct ssa_vm *vm,
	struct ssav_line *l, enum ssar_redoflags prev)
{
	struct assa_layer *lay = assa_getlayer(vm, l->ass_layer);

	if (*lay->curpos
		&& (*lay->curpos)->line == l
		&& !(prev & SSAR_WRAP)) 
		lay->curpos = &(*lay->curpos)->next;
	else {
		assa_trash(lay);
		prev |= SSAR_WRAP | SSAR_REND;
		assa_wrap(vm, lay, l);
	}
		
	return prev;
}

void assa_end(struct ssa_vm *vm)
{
	struct assa_layer *lay = vm->firstlayer;

	while (lay) {
		assa_trash(lay);
		lay = lay->next;
	}
	vm->redoflags = 0;
}

void assa_setup(struct ssa_vm *vm, unsigned width, unsigned height)
{
	if (vm->playresx != 0.0 && vm->playresy != 0.0) {
		vm->res.x = (int)(vm->playresx * 65536.);
		vm->res.y = (int)(vm->playresy * 65536.);
		/* should probably create scaling matrix here */
	} else {
		vm->res.x = width << 16;
		vm->res.y = height << 16;
		/* should set scaling matrix to identity here */
	}
	vm->redoflags = SSAR_COLO | SSAR_REND | SSAR_WRAP;
}

