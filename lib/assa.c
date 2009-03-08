/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005, 2006, 2007  David Lamparter
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

/** @file assa.c asa screen space allocator. */

#include "common.h"
#include "ssavm.h"
#include "ssarun.h"

#include <subhelp.h>

/** initialize incremental allocation.
 * @param vm the VM
 * @return scrap'n'redo flags
 */
enum ssar_redoflags assa_start(struct ssa_vm *vm)
{
	struct assa_layer *lay = vm->firstlayer;

	while (lay) {
		lay->curpos = &lay->allocs;
		lay = lay->next;
	}
	return vm->redoflags;
}

/** get or allocate layer.
 * @param vm the VM
 * @param layer layer number
 * @return the layer or NULL on OOM
 */
static struct assa_layer *assa_getlayer(struct ssa_vm *vm, long int layer)
{
	struct assa_layer **prev = &vm->firstlayer, *newl;

	while (*prev && (*prev)->layer < layer)
		prev = &(*prev)->next;
	if (*prev && (*prev)->layer == layer)
		return *prev;

	newl = xnew(struct assa_layer);
	oom_return(!newl);
	newl->layer = layer;
	newl->next = *prev;
	newl->allocs = NULL;
	newl->curpos = &newl->allocs;
	*prev = newl;
	return newl;
}

/** kill remaining allocations on a layer.
 * starts off current incremental position, kills everything until end.
 * @param lay the layer
 */
static void assa_trash(struct assa_layer *lay)
{
	struct assa_alloc *fptr, *fnext;

	fptr = *lay->curpos;
	*lay->curpos = NULL;
	while (fptr) {
		fnext = fptr->next;
		ssar_dispose(fptr->line);
		ssgl_dispose(fptr->line);
		xfree(fptr);
		fptr = fnext;
	}
}

struct assa_rect {
	FT_Vector pos, size;
	double xalign, yalign;
};

/** temporary line entry.
 * assa_fit_split builds a linked list of these;
 * horizontal alignment is then calculated afterwards.
 */
struct fitline {
	FT_Vector size;				/**< tot line w/h */
	struct ssav_unit
		*startat,			/**< first unit */
		*endat;				/**< first nonunit */
	int full_flag;
};

struct fitlines {
	unsigned alloc, used;
	struct fitline *fl;
};

/** fitting pass #1: split into lines and sum up their size.
 * @param fls (out) arranged lines
 * @param u first unit
 * @param width wrap width
 * @param h_total (out) total height of everything
 *  fls->fl == NULL on OOM
 */
static void assa_fit_q12(struct fitlines *fls, struct ssav_unit *u,
	FT_Pos width, FT_Pos *h_total, long wrap)
{
	FT_Pos w_remain = INT_MAX;
	struct fitline *fl = fls->fl;

	while (u) {
		if (fls->used == fls->alloc) {
			fls->fl = (struct fitline *)xrealloc_free(fls->fl,
				(fls->alloc += 5) * sizeof(struct fitline));
			if (!fls->fl)
				return;
		}
		fl = fls->fl + fls->used;
		fl->startat = u;
		fl->size.x = fl->size.y = 0;

		if (wrap == 1)
			w_remain = width;
		while (u && u->type == SSAVU_TEXT) {
			if (wrap == 1)
				w_remain -= u->size.x << 10;
			fl->size.x += u->size.x << 10;
			if ((u->height << 10) > fl->size.y)
				fl->size.y = u->height << 10;
			u = u->next;
		}
		if (u && u->type == SSAVU_NEWLINE) {
			if ((u->height << 10) > fl->size.y)
				fl->size.y = u->height << 10;
			u = u->next;
		}
		*h_total += fl->size.y;
		fl->endat = u;
		fls->used++;
	};
}

/** unit valid for q0/q3.
 * q0 and q3 are processed block-wise, split along \N
 */
#define uv (u && u->type == SSAVU_TEXT)

/** get neccessary lines.
 * counts how much lines are needed to fit u into width.
 * @param u first unit, function walks until NULL / non-TEXT
 * @param width wrapping width
 * @param totlen sum of all size.x values, computed along the way
 * @return number of lines needed
 */
static unsigned assa_count_lines(struct ssav_unit *u, FT_Pos width,
	FT_Pos *totlen)
{
	FT_Pos w_remain;
	unsigned rv = 0;
	*totlen = 0;
	while (uv) {
		rv++;
		if ((u->size.x << 10) >= width) {
			u = u->next;
			*totlen += width;
			continue;
		}
		w_remain = width;
		while (uv && (u->size.x << 10) < w_remain) {
			w_remain -= (u->size.x << 10);
			*totlen += u->size.x << 10;
			u = u->next;
		}
	}
	return rv;
}

/** process one block of units for q0.
 * @param fls destination line storage
 * @param u first unit, function walks until NULL / non-TEXT
 * @param width wrapping width
 * @param h_total (out) total height of everything
 * @return first unit not processed (!u || u->type == SSAVU_NEWLINE)
 */
static struct ssav_unit *assa_fit_q0_i(struct fitlines *fls,
	struct ssav_unit *u, FT_Pos width, FT_Pos *h_total)
{
	FT_Pos totlen, avg;
	unsigned nlines;
	struct fitline *first, *fl, *end;

	nlines = assa_count_lines(u, width, &totlen);
	if (nlines == 0)
		return u;

	if (fls->alloc < nlines + fls->used) {
		fls->fl = (struct fitline *)xrealloc_free(fls->fl,
			(fls->alloc = nlines + fls->used)
			* sizeof(struct fitline));
		if (!fls->fl)
			return NULL;
	}
	first = fls->fl + fls->used;
	fls->used = nlines + fls->used;
	end = first + nlines;
	for (fl = first; fl < end; fl++)
		fl->size.x = fl->size.y = fl->full_flag = 0;

	avg = INT_MAX;
	for (fl = first; uv && fl < end; fl++) {
		if (avg > totlen / (end - fl))
			avg = totlen / (end - fl);
		fl->size.x = u->size.x << 10;
		totlen -= fl->size.x;
		fl->startat = u;
		u = fl->endat = u->next;
		while (uv && fl->size.x + (u->size.x << 10) <= avg) {
			fl->size.x += u->size.x << 10;
			totlen -= u->size.x << 10;
			u = u->next;
			fl->endat = u;
		}
		avg = fl->size.x;
	}

	fl = --end;
	while (uv) {
		FT_Pos thislen = fl->endat->size.x << 10,
			maxlen = (fl == first || (fl - 1)->full_flag)
				? width : (fl - 1)->size.x;
		if (fl->size.x + thislen <= maxlen) {
			fl->size.x += thislen;
			fl->endat = fl->endat->next;
			if (fl != end) {
				fl++;
				fl->size.x -= thislen;
				fl->startat = fl->startat->next;
			} else
				u = u->next;
		} else if (fl == first || (fl - 1)->full_flag) {
			fl->full_flag = 1;
			fl++;
		} else
			fl--;
	}
	for (fl = first; fl <= end; fl++) {
		for (u = fl->startat; u != fl->endat; u = u->next)
			if (fl->size.y < (u->height << 10))
				fl->size.y = u->height << 10;
		*h_total += fl->size.y;
	}
	return u;
}

/** perform block splitting and handle newlines in q0 and q3.
 * @param fls destination line storage
 * @param u first unit, function walks until NULL / non-TEXT
 * @param width wrapping width
 * @param h_total (out) total height of everything
 * @param wrap wrapping mode (0 or 3)
 *  fls->fl == NULL on OOM
 */
static void assa_fit_q0(struct fitlines *fls,
	struct ssav_unit *u, FT_Pos width, FT_Pos *h_total, long wrap)
{
	struct fitline *fl;
	while (u) {
		while (u->type == SSAVU_NEWLINE) {
			if (fls->used == fls->alloc) {
				fls->fl = (struct fitline *)xrealloc_free(
					fls->fl, (fls->alloc += 5)
					* sizeof(struct fitline));
				if (!fls->fl)
					return;
			}
			fl = fls->fl + fls->used++;
			fl->size.x = 0;
			*h_total += (fl->size.y = u->height << 10);
			u = fl->endat = (fl->startat = u)->next;
		}
		u = assa_fit_q0_i(fls, u, width, h_total);
		/* on OOM: u == fls->fl == NULL - returns safely here. */
		if (u && fls->used) {
			fl = fls->fl + fls->used - 1;
			if (fl->size.y < (u->height << 10)) {
				*h_total += (u->height << 10) - fl->size.y;
				fl->size.y = u->height << 10;
			}
			u = fl->endat = u->next;
		}
	}
}

/** fitting pass #2: fit a linked list of fitlines into a rectangle.
 * updates ssav_unit->final.{x,y} to be the correct on-screen position
 * @param fl the list of lines
 * @param r the rectangle to use (including alignment info)
 * @param h_total total height of all the lines
 */
static void assa_fit_arrange(struct ssav_line *l, struct fitlines *fls,
	struct assa_rect *r, FT_Pos h_total)
{
	FT_Pos y, x;
	FT_Pos w;
	struct fitline *fl, *end;

	l->y[0] = y = r->pos.y + (FT_Pos)((double)(r->size.y - h_total) * r->yalign);
	l->y[1] = l->y[0] + h_total;
	l->x[0] = w = 0;
	for (fl = fls->fl, end = fl + fls->used; fl < end; fl++) {
		struct ssav_unit *u = fl->startat;
		x = r->pos.x + (FT_Pos)((double)(r->size.x - fl->size.x) * r->xalign);
		if (fl->size.x > w)
			l->x[0] = x, w = fl->size.x;

		y += fl->size.y;
		while (u != fl->endat) {
			u->final.y = y >> 10;
			u->final.x = x >> 10;
			x += u->size.x << 10;
			u = u->next;
		}
	}
	l->x[1] = l->x[0] + w;
}

/** fitting parent: try to place line into a rectangle.
 * @param l the line to fit
 * @param r the rectangle to use (including alignment info)
 * @return 0 on success, -1 on OOM
 */
static int assa_fit(struct ssav_line *l, struct assa_rect *r)
{
	FT_Pos h_total = 0;
	struct fitlines fls;
	fls.alloc = 5;
	fls.used = 0;
	fls.fl = (struct fitline *)xmalloc(fls.alloc
		* sizeof(struct fitline));
	if (!fls.fl) {
		oom_msg();
		return -1;
	}

	if (l->wrap == 0 || l->wrap == 3)
		assa_fit_q0(&fls, l->unit_first, r->size.x, &h_total, l->wrap);
	else
		assa_fit_q12(&fls, l->unit_first, r->size.x, &h_total, l->wrap);
	if (!fls.fl) {
		oom_msg();
		return -1;
	}
	assa_fit_arrange(l, &fls, r, h_total);

	xfree(fls.fl);
	return 0;
}

/** move line until it doesn't collide with any other on-layer one.
 * @param l line to update
 * @param lay layer for collision information
 */
static void assa_collide(struct ssav_line *l, struct assa_layer *lay)
{
	struct ssav_unit *u;
	struct assa_alloc *a = lay->allocs;
	FT_Pos offsety = 0;
	int movedir = l->yalign >= 0.5, nmovedir = 1 - movedir;

	for (; a; a = a->next) {
		struct ssav_line *cl = a->line;

		if (l->x[0] >= cl->x[1]
			|| l->x[1] < cl->x[0])
			continue;
		if (l->y[0] + offsety >= cl->y[1]
			|| l->y[1] + offsety < cl->y[0])
			continue;

		offsety = cl->y[nmovedir] - l->y[movedir] - movedir;
		a = lay->allocs;
	}
	l->y[0] += offsety;
	l->y[1] += offsety;

	offsety >>= 10;
	for (u = l->unit_first; u; u = u->next)
		u->final.y += offsety;
}

/** wrap and position line.
 * @param vm the SSA vm to use (for frame size)
 * @param lay ASS layer (only same-layer lines get collision detection)
 * @param l the line to process
 * @return 0 on success, -1 on OOM
 */
static int assa_wrap(struct ssa_vm *vm, struct assa_layer *lay,
	struct ssav_line *l)
{
	struct assa_alloc *newa;
	struct assa_rect r;

	newa = xnew(struct assa_alloc);
	if (!newa)
		return -1;

	ssgl_prepare(vm, l);

	newa->next = NULL;
	newa->line = l;

	r.xalign = l->xalign;
	r.yalign = l->yalign;
	if (l->flags & SSAV_POS) {
		r.pos = l->active.pos;
		FT_Vector_Transform(&r.pos, &vm->cscale);
		r.pos.x -= (FT_Pos)(l->xalign * vm->outsize.x);
		r.pos.y -= (FT_Pos)(l->yalign * vm->outsize.y);
		r.size = vm->outsize;
		if (assa_fit(l, &r))
			goto out_oom;
	} else {
		FT_Vector tmp;
		r.pos.x = l->marginl;
		r.pos.y = l->margint;
		FT_Vector_Transform(&r.pos, &vm->cscale);
		tmp.x = l->marginr;
		tmp.y = l->marginb;
		FT_Vector_Transform(&tmp, &vm->cscale);
		r.size = vm->outsize;
		r.size.x -= r.pos.x + tmp.x;
		r.size.y -= r.pos.y + tmp.y;
		if (assa_fit(l, &r))
			goto out_oom;
		assa_collide(l, lay);
	}
	if ((l->flags & SSAV_ORG) == 0) {
		l->active.org.x = r.pos.x + (FT_Pos)(r.xalign * r.size.x);
		l->active.org.y = r.pos.y + (FT_Pos)(r.yalign * r.size.y);
	}
	*lay->curpos = newa;
	lay->curpos = &newa->next;
	return 0;

out_oom:
	xfree(newa);
	return -1;
}

enum ssar_redoflags assa_realloc(struct ssa_vm *vm,
	struct ssav_line *l, enum ssar_redoflags prev)
{
	struct assa_layer *lay = assa_getlayer(vm, l->ass_layer);
	struct assa_alloc **curpos;

	if (!lay)
		return prev;

	curpos = lay->curpos;
	if (!(prev & SSAR_WRAP)) {
		for (; *curpos; curpos = &(*curpos)->next)
			if ((*curpos)->line == l) {
				lay->curpos = &(*curpos)->next;
				return prev;
			}
	}
	assa_trash(lay);
	prev |= SSAR_WRAP | SSAR_REND;
	assa_wrap(vm, lay, l);
	return prev;
}

void assa_end(struct ssa_vm *vm)
{
	struct assa_layer **lay = &vm->firstlayer, *tofree;

	while (*lay) {
		assa_trash(*lay);
		if (!(*lay)->allocs) {
			tofree = *lay;
			*lay = tofree->next;
			xfree(tofree);
		} else
			lay = &(*lay)->next;
	}
	vm->redoflags = 0;
}

void assa_setup(struct ssa_vm *vm, unsigned width, unsigned height)
{
	vm->cscale.xy = 0x00000;
	vm->cscale.yx = 0x00000;
	vm->outsize.x = width << 16;
	vm->outsize.y = height << 16;
	if (vm->playresx != 0.0 && vm->playresy != 0.0) {
		vm->playres.x = (int)(vm->playresx * 65536.);
		vm->playres.y = (int)(vm->playresy * 65536.);
		vm->cscale.xx = (FT_Pos)(width * 65536. / vm->playresx);
		vm->cscale.yy = (FT_Pos)(height * 65536. / vm->playresy);
	} else {
		vm->playres = vm->outsize;
		vm->cscale.xx = 0x10000;
		vm->cscale.yy = 0x10000;
	}
	vm->fscale = vm->cscale;
	vm->fscale.xx = vm->fscale.yy;
	vm->redoflags = SSAR_COLO | SSAR_REND | SSAR_WRAP;
}

