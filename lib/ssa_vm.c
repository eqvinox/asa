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

#define _BSD_SOURCE /* strdup() */

#include "common.h"
#include "ssa.h"
#include "ssawrap.h"
#include "ssavm.h"
#include "ssarun.h"
#include "asa.h"
#include "asafont.h"
#include "asaerror.h"

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>
#include <math.h>

#define FONT_FALLBACK1	"Arial"
#define FONT_FALLBACK2	"sans-serif"

struct ssav_prepare_ctx {
	struct ssa *ssa;
	struct ssa_vm *vm;
	struct ssa_line *l;

	struct ssav_line *vl;
	struct ssav_node **nodenextp;

	struct ssav_params *pset;
	struct assp_frameref *ng;
	int ng_ref;

	struct ssa_wrap_env wrap;
};

static void ssav_anim_insert(struct ssav_prepare_ctx *ctx,
	struct ssav_controller *ctr);
static void ssav_anim_lineinsert(struct ssav_prepare_ctx *ctx,
	struct ssav_controller *ctr);

#define ifp struct ssav_prepare_ctx *ctx, struct ssa_node *n, ptrdiff_t param
#define afp struct ssav_prepare_ctx *ctx, struct ssa_node *n, \
	ptrdiff_t param, struct ssav_controller *ctr

typedef void ssav_ipfunc(ifp);
typedef void ssav_apfunc(afp);
static void ssav_ignore(ifp);

static void ssav_nl(ifp);
static void ssav_text(ifp);
static void ssav_fontsize(ifp);
static void ssav_reset(ifp);

static void ssav_double(ifp);
static void ssava_double(afp);
static void ssav_colour(ifp);
static void ssava_colour(afp);

static void ssav_align(ifp);
static void ssav_lineint(ifp);

static void ssav_pos(ifp);
static void ssava_pos(afp);
static void ssav_move(ifp);

static void ssav_clip(ifp);
static void ssava_clip(afp);

static void ssav_fade(ifp);
static void ssav_fad(ifp);

static void ssav_anim(ifp);

struct ssa_ipnode {
	ssav_ipfunc *func;
	ssav_apfunc *afunc;
	ptrdiff_t param;
};

#define apply_offset(x,y) (((char *)x) + y)		/**< apply e(x) */
#define e(x) ((ptrdiff_t) &((struct ssav_params *)0)->x)
#define l(x) ((ptrdiff_t) &((struct ssav_line *)0)->x)
static struct ssa_ipnode iplist[SSAN_MAX] = {
							/* S - static / nonanimatable,
							 * a - animatable
							 * * - special
							 *  l - local
							 *  G - line (global)
							 * ? - undecided
							 */
	{NULL,		NULL,		0},
	{ssav_text,	NULL,		0},		/* Sl SSAN_TEXT */
	{ssav_nl,	NULL,		0},		/* Sl SSAN_NEWLINE */
	{ssav_nl,	NULL,		0},		/* Sl SSAN_NEWLINEH */
	{NULL,		NULL,		0},		/* !l SSAN_BOLD */
	{NULL,		NULL,		0},		/* ?l SSAN_ITALICS */
	{NULL,		NULL,		0},		/* ?l SSAN_UNDERLINE */
	{NULL,		NULL,		0},		/* ?l SSAN_STRIKEOUT */
	{ssav_double,	ssava_double,	e(border)},	/* al SSAN_BORDER */
	{NULL,		NULL,		0},		/* al SSAN_SHADOW */
	{NULL,		NULL,		0},		/* ?? SSAN_BLUREDGES */
	{NULL,		NULL,		0},		/* ?l SSAN_FONT */
	{ssav_fontsize,	NULL,		0},		/* ?l SSAN_FONTSIZE */

	{ssav_double,	ssava_double,	e(m.fscx)},	/* al SSAN_FSCX */
	{ssav_double,	ssava_double,	e(m.fscy)},	/* al SSAN_FSCY */
	{ssav_double,	ssava_double,	e(m.frx)},	/* aG SSAN_FRX */
	{ssav_double,	ssava_double,	e(m.fry)},	/* aG SSAN_FRY */
	{ssav_double,	ssava_double,	e(m.frz)},	/* aG SSAN_FRZ */
	{ssav_double,	ssava_double,	e(m.fax)},	/* aG SSAN_FAX */
	{ssav_double,	ssava_double,	e(m.fay)},	/* aG SSAN_FAY */

	{ssav_double,	ssava_double,	e(m.fsp)},	/* ?l SSAN_FSP */
	{ssav_ignore,	NULL,		0},		/* -- SSAN_FE */
	{ssav_colour,	ssava_colour,	0x0},		/* al SSAN_COLOUR */
	{ssav_colour,	ssava_colour,	0x1},		/* al SSAN_COLOUR2 */
	{ssav_colour,	ssava_colour,	0x2},		/* al SSAN_COLOUR3 */
	{ssav_colour,	ssava_colour,	0x3},		/* al SSAN_COLOUR4 */
	{ssav_colour,	ssava_colour,	0x10},		/* al SSAN_ALPHA */
	{ssav_colour,	ssava_colour,	0x11},		/* al SSAN_ALPHA2 */
	{ssav_colour,	ssava_colour,	0x12},		/* al SSAN_ALPHA3 */
	{ssav_colour,	ssava_colour,	0x13},		/* al SSAN_ALPHA4 */
	{ssav_align,	NULL,		1},		/* SG SSAN_ALIGN */
	{ssav_align,	NULL,		0},		/* SG SSAN_ALIGNNUM */
	{NULL,		NULL,		0},		/* Sl SSAN_KARA */
	{NULL,		NULL,		0},		/* Sl SSAN_KARAF */
	{NULL,		NULL,		0},		/* Sl SSAN_KARAO */
	{NULL,		NULL,		0},		/* Sl SSAN_KARAT */
	{ssav_lineint,	NULL,		l(wrap)},	/* SG SSAN_WRAP */
	{ssav_reset,	NULL,		0},		/* *l SSAN_RESET */

	{ssav_anim,	NULL,		0},		/* S* SSAN_T */
	{ssav_move,	NULL,		0},		/* SG SSAN_MOVE */
	{ssav_pos,	ssava_pos,	l(base.pos)},	/* aG SSAN_POS */
	{ssav_pos,	ssava_pos,	l(base.org)},	/* SG SSAN_ORG */
	{ssav_fade,	NULL,		0},		/* S? SSAN_FADE */
	{ssav_fad,	NULL,		0},		/* S? SSAN_FAD */
	{ssav_clip,	ssava_clip,	0},		/* aG SSAN_CLIPRECT */
	{NULL,		NULL,		0},		/* SG SSAN_CLIPDRAW */
	{NULL,		NULL,		0},		/* Sl SSAN_PAINT */
	{NULL,		NULL,		0},		/* ?l SSAN_PBO */

};

/****************************************************************************/

static inline int ssa_strcmp(const ssa_string *a, const ssa_string *b)
{
	ptrdiff_t lena = a->e - a->s, lenb = b->e - b->s;
	if (lena != lenb)
		return 1;
	return memcmp(a->s, b->s, lena * sizeof(ssaout_t));
}

static struct ssav_error *ssav_add_error(struct ssa_vm *vm,
	enum ssav_errc code, char *ext_str)
{
	struct ssav_error *me;

	me = xmalloc(sizeof(struct ssav_error));
	memset(me, 0, sizeof(struct ssav_error));
	me->errorcode = code;
	*vm->errnext = me;
	vm->errnext = &me->next;
	if (ext_str)
		me->ext_str = xstrdup(ext_str);
	return me;
}

static void ssav_add_error_dlg(enum ssav_errc code, char *ext_str,
	struct ssav_prepare_ctx *ctx, struct ssa_node *n)
{
	struct ssav_error *me = ssav_add_error(ctx->vm, code, ext_str);
	me->context = SSAVECTX_DIALOGUE;
	me->i.dlg.lineno = ctx->l->no;
	me->i.dlg.lexline = ctx->l;
	me->i.dlg.line = ctx->vl;
	me->i.dlg.node = n;
	me->i.dlg.ntype = n->type;
}

static void ssav_add_error_style(enum ssav_errc code, char *ext_str,
	struct ssa_vm *vm, struct ssa_style *s)
{
	struct ssav_error *me = ssav_add_error(vm, code, ext_str);
	size_t namesz = ssa_utf8_len(&s->name);
	me->context = SSAVECTX_STYLE;
	me->i.style.style = s;
	me->i.style.stylename = xmalloc(namesz);
	ssa_utf8_conv(me->i.style.stylename, &s->name);
}

/****************************************************************************/

static struct ssav_params *ssav_addref(struct ssav_params *p)
{
	p->nref++;
	return p;
}

static void ssav_release(struct ssav_params *p)
{
	if (!--p->nref) {
		asaf_srelease(p->fsiz);
		asaf_frelease(p->font);
		xfree(p);
	}
}

static void ssav_hunt_ex(struct ssav_prepare_ctx *ctx,
	struct ssa_style *style)
{
	struct ssa_node *cn = style->node_first;

	if (style->base)
		ssav_hunt_ex(ctx, style->base);

	while (cn) {
		if (SSAN(cn->type) < SSAN_MAX) {
			struct ssa_ipnode *ip = &iplist[SSAN(cn->type)];
			if (ip->func)
				ip->func(ctx, cn, ip->param);
			else
				ssav_add_error_style(SSAVEC_NOTSUP, NULL,
					ctx->vm, style);
		}
		cn = cn->next;
	}
}

static struct ssav_params *ssav_get_style(struct ssav_prepare_ctx *ctx,
	struct ssa_style *style)
{
	struct ssav_params *rv, *temp;
	if (!style->vmptr)
		return NULL;
	temp = ctx->pset;
	ctx->pset = ssav_addref((struct ssav_params *)style->vmptr);
	ssav_hunt_ex(ctx, style);
	rv = ctx->pset;
	ctx->pset = temp;
	return rv;
}

static struct ssav_params *ssav_alloc_size(struct ssav_params *p,
	unsigned nctr)
{
	struct ssav_params *rv;
	unsigned ncopy = p->nctr < nctr ? p->nctr : nctr;

	rv = xmalloc(sizeof(struct ssav_params)
		+ sizeof(struct ssav_controller) * nctr);
	memcpy(rv, p, sizeof(*rv) + sizeof(struct ssav_controller) * ncopy);

	rv->nref = 1;
	asaf_faddref(rv->font);
	asaf_saddref(rv->fsiz);
	ssav_release(p);
	return rv;
}

static struct ssav_params *ssav_alloc_clone(struct ssav_params *p)
{
	if (p->nref == 1)
		return p;
	return ssav_alloc_size(p, p->nctr);
}

static struct ssav_params *ssav_alloc_clone_clear(struct ssav_params *p,
	ptrdiff_t offset, ptrdiff_t size)
{
	struct ssav_params *rv = p;
	unsigned c = 0;
	if (rv->nref != 1)
		rv = ssav_alloc_size(p, p->nctr);
	while (c < rv->nctr) {
		if (rv->ctrs[c].offset >= offset
			&& rv->ctrs[c].offset <= offset + size) {
			rv->nctr--;
			memmove(&rv->ctrs[c], &rv->ctrs[c + 1],
				rv->nctr - c);
		} else
			c++;
	}
	return xrealloc(rv, sizeof(struct ssav_params)
		+ sizeof(struct ssav_controller) * rv->nctr);
}

static enum ssav_errc ssav_set_font(struct ssav_params *pset,
	ssa_string *name, double size)
{
	if (pset->fsiz)
		asaf_srelease(pset->fsiz);
	if (pset->font)
		asaf_frelease(pset->font);
	
	if (name) {
		size_t namesz = ssa_utf8_len(name);
		if (pset->f.name)
			xfree(pset->f.name);
		pset->f.name = xmalloc(namesz);
		ssa_utf8_conv(pset->f.name, name);
	}

	pset->f.size = size;
	pset->font = asaf_request(pset->f.name, pset->f.italic,
		pset->f.weight);
	if (!pset->font) {
		pset->font = asaf_request(FONT_FALLBACK1,
			pset->f.italic, pset->f.weight);
		if (!pset->font) {
			pset->font = asaf_request(FONT_FALLBACK2,
				pset->f.italic, pset->f.weight);
			if (!pset->font)
				return SSAVEC_FONTNX;
		}
	}
	pset->fsiz = asaf_reqsize(pset->font, pset->f.size);
	return 0;
}

static struct ssav_params *ssav_alloc_style(struct ssa *ssa,
	struct ssa_vm *vm, struct ssa_style *style)
{
	enum ssav_errc ec;
	struct ssav_params *rv = xmalloc(sizeof(struct ssav_params));
	memset(rv, 0, sizeof(*rv));

	ec = ssav_set_font(rv, &style->fontname, style->fontsize);
	if (ec) {
		ssav_add_error_style(ec, rv->f.name, vm, style);
		xfree(rv);
		return NULL;
	}
	rv->f.weight = style->fontweight;
	rv->f.italic = !!style->italic;

	rv->m.fscx = style->scx;
	rv->m.fscy = style->scy;
	rv->m.frz = style->rot;
	rv->m.frx = rv->m.fry = 0.0;
	rv->m.fax = rv->m.fay = 0.0;
	rv->m.fsp = style->sp;

	rv->r.colours[0] = style->cprimary;
	rv->r.colours[1] = style->csecondary;
	rv->r.colours[2] = ssa->version & SSAVV_4 ?
		style->cback : style->coutline;
	rv->r.colours[3] = style->cback;

	rv->nref = 1;
	return rv;
}

/****************************************************************************/

static void ssav_ng_invalidate(struct ssav_prepare_ctx *ctx)
{
	if (ctx->ng_ref) {
		ctx->ng = xmalloc(sizeof(struct assp_frameref));
		memset(ctx->ng, 0, sizeof(*ctx->ng));
		ctx->ng_ref = 0;
	}
}

static void ssav_assign_pset(struct ssav_prepare_ctx *ctx,
	struct ssav_params *newpset)
{
	unsigned c;

	if (memcmp(&ctx->pset->r, &newpset->r, sizeof(ctx->pset->r)))
		ssav_ng_invalidate(ctx);
	else
		for (c = 0; c < newpset->nctr; c++) {
			ptrdiff_t offset = newpset->ctrs[c].offset;
			if (offset < e(r.colours[0])
				|| offset >= e(r.colours[4]))
				continue;
			if (c >= ctx->pset->nctr
				|| memcmp(&ctx->pset->ctrs[c],
					&newpset->ctrs[c],
					sizeof(struct ssav_controller))) {
				ssav_ng_invalidate(ctx);
				break;
			}
		}
	ctx->pset = newpset;
}

/****************************************************************************/

static void ssav_ignore(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
}

/****************************************************************************/

static void ssav_double(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	ctx->pset = ssav_alloc_clone_clear(ctx->pset, param, sizeof(double));
	*(double *)apply_offset(ctx->pset, param) = n->v.dval;
}

static void ssava_double(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param, struct ssav_controller *ctr)
{
	ctr->type = SSAVC_MATRIX;
	ctr->offset = param;
	ctr->nextval.dval = n->v.dval;
	ssav_anim_insert(ctx, ctr);
}

static void ssav_fontsize(struct ssav_prepare_ctx *ctx,
				struct ssa_node *n, ptrdiff_t param)
{
	ctx->pset = ssav_alloc_clone(ctx->pset);
	asaf_srelease(ctx->pset->fsiz);
	ctx->pset->f.size = n->v.dval;
	ctx->pset->fsiz = asaf_reqsize(ctx->pset->font, ctx->pset->f.size);
}

static void ssav_reset(struct ssav_prepare_ctx *ctx,
				struct ssa_node *n, ptrdiff_t param)
{
	struct ssa_style *reset_to = n->v.style ? n->v.style : ctx->l->style;
	if (!reset_to->vmptr)
		return;
	ssav_release(ctx->pset);
	ssav_assign_pset(ctx, ssav_get_style(ctx, reset_to));
}

static inline void ssav_splitcolour(struct ssa_node *n, ptrdiff_t *param,
	colour_t *mask, colour_t *value)
{
	value->l = mask->l = 0;
	if (*param & 0x10) {
		value->c.a = n->v.alpha;
		mask->c.a = 0xFF;
	} else {
		*value = n->v.colour;
		mask->c.r = mask->c.g = mask->c.b = 0xFF;
	}
	*param &= 3;
}

static void ssav_colour(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	colour_t mask, value;
	ssav_splitcolour(n, &param, &mask, &value);
	if ((ctx->pset->r.colours[param].l & mask.l) == value.l)
		return;
	ctx->pset = ssav_alloc_clone_clear(ctx->pset, e(r.colours[param]),
		sizeof(colour_t));
	ctx->pset->r.colours[param].l &= ~mask.l;
	ctx->pset->r.colours[param].l |= value.l;
	ssav_ng_invalidate(ctx);
}

static void ssava_colour(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param, struct ssav_controller *ctr)
{
	ssav_splitcolour(n, &param,
		&ctr->nextval.colour.mask,
		&ctr->nextval.colour.val);
	ctr->type = SSAVC_COLOUR;
	ctr->offset = e(r.colours[param]);
	ssav_anim_insert(ctx, ctr);
}

static void ssav_setfade(struct ssav_prepare_ctx *ctx, alpha_t alpha,
	double start, double end)
{
	struct ssav_controller ctr;
	int c;
	ctr.t1 = start;
	ctr.length_rez = 1. / (end - start);
	ctr.accel = 1.0;
	ctr.nextval.colour.mask.l = 0;
	ctr.nextval.colour.mask.c.a = 0xff;
	ctr.nextval.colour.val.l = 0;
	ctr.nextval.colour.val.c.a = alpha;
	for (c = 0; c < 4; c++) {
		ctr.type = SSAVC_COLOUR;
		ctr.offset = e(r.colours[c]);
		ssav_anim_insert(ctx, &ctr);
	}
}

static void ssav_fade(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	int c;
	ctx->pset = ssav_alloc_clone_clear(ctx->pset, e(r.colours[0]),
		4 * sizeof(colour_t));
	ssav_ng_invalidate(ctx);
	for (c = 0; c < 4; c++)
		ctx->pset->r.colours[param].c.a = n->v.fade.a1;
	ssav_setfade(ctx, n->v.fade.a2,
		n->v.fade.start1 * 0.001, n->v.fade.end1 * 0.001);
	ssav_setfade(ctx, n->v.fade.a3,
		n->v.fade.start2 * 0.001, n->v.fade.end2 * 0.001);
}

static void ssav_fad(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	int c;
	double dur = ctx->vl->end - ctx->vl->start;
	ctx->pset = ssav_alloc_clone_clear(ctx->pset, e(r.colours[0]),
		4 * sizeof(colour_t));
	ssav_ng_invalidate(ctx);
	for (c = 0; c < 4; c++)
		ctx->pset->r.colours[param].c.a = 0xff;
	ssav_setfade(ctx, 0x00, 0.0, n->v.fad.in * 0.001);
	ssav_setfade(ctx, 0xff, dur - (n->v.fad.out * 0.001), dur);
}

/****************************************************************************/

static inline long int ssa_a2an(long int v)
{
	return v < 4 ? v
		: v < 8 ? v - 2
		: v - 5;
}

static inline void ssav_setalign(struct ssav_line *vl, long int v,
	int oldstyle)
{
	if (oldstyle)
		v = ssa_a2an(v);
	v--;

	switch (v % 3) {
	case 0:	vl->xalign = 0.0; break;
	case 1:	vl->xalign = 0.5; break;
	case 2:	vl->xalign = 1.0; break;
	}
	switch (v / 3) {
	case 0:	vl->yalign = 1.0; break;
	case 1:	vl->yalign = 0.5; break;
	case 2:	vl->yalign = 0.0; break;
	}
}

static void ssav_align(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	ssav_setalign(ctx->vl, n->v.lval, (int)param);
}

static void ssav_lineint(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	*(long int *)apply_offset(ctx->vl, param) = n->v.lval;
}

static void ssav_clip(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	ctx->vl->base.clip.xMin = n->v.clip.x1 << 16;
	ctx->vl->base.clip.xMax = n->v.clip.x2 << 16;
	ctx->vl->base.clip.yMin = n->v.clip.y1 << 16;
	ctx->vl->base.clip.yMax = n->v.clip.y2 << 16;
}

static void ssava_clip(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param, struct ssav_controller *ctr)
{
#define clipctr(dst, src) \
	ctr->type = SSAVC_FTPOS; \
	ctr->offset = l(active.clip.dst); \
	ctr->nextval.pos = n->v.clip.src << 16; \
	ssav_anim_lineinsert(ctx, ctr);
	clipctr(xMin, x1)
	clipctr(xMax, x2)
	clipctr(yMin, y1)
	clipctr(yMax, y2)
#undef clipctr
}

/****************************************************************************/

static void ssav_pos(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	ctx->vl->flags |= (param == l(base.pos)) ? SSAV_POS : SSAV_ORG;
	((FT_Vector *)apply_offset(ctx->vl, param))->x = n->v.pos.x << 16;
	((FT_Vector *)apply_offset(ctx->vl, param))->y = n->v.pos.y << 16;
}

static void ssava_pos(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param, struct ssav_controller *ctr)
{
	ctx->vl->flags |= (param == l(base.pos)) ? SSAV_POSANIM : SSAV_ORGANIM;

	ctr->type = SSAVC_FTPOS;
	ctr->offset = param - l(base.pos) + l(active.pos.x);
	ctr->nextval.pos = n->v.pos.x << 16;
	ssav_anim_lineinsert(ctx, ctr);

	ctr->type = SSAVC_FTPOS;
	ctr->offset = param - l(base.pos) + l(active.pos.y);
	ctr->nextval.pos = n->v.pos.y << 16;
	ssav_anim_lineinsert(ctx, ctr);
}

static void ssav_move(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	struct ssav_controller ctr;

	ctx->vl->flags |= SSAV_POS | SSAV_POSANIM;
	ctx->vl->base.pos.x = n->v.move.x1 << 16;
	ctx->vl->base.pos.y = n->v.move.y1 << 16;

	ctr.t1 = n->v.move.start * 0.001;
	ctr.length_rez = 1. / ((n->v.move.end - n->v.move.start) * 0.001);
	ctr.accel = 1.0;

	ctr.type = SSAVC_FTPOS;
	ctr.offset = l(active.pos.x);
	ctr.nextval.pos = n->v.move.x2 << 16;
	ssav_anim_lineinsert(ctx, &ctr);

	ctr.type = SSAVC_FTPOS;
	ctr.offset = l(active.pos.y);
	ctr.nextval.pos = n->v.move.y2 << 16;
	ssav_anim_lineinsert(ctx, &ctr);
}

/****************************************************************************/

static void ssav_anim_insert(struct ssav_prepare_ctx *ctx,
	struct ssav_controller *ctr)
{
	if (ctr->type == SSAVC_NONE)
		return;

	ctx->pset = ssav_alloc_size(ctx->pset, ctx->pset->nctr + 1);
	memcpy(&ctx->pset->ctrs[ctx->pset->nctr++], ctr, sizeof(*ctr));

	ctr->type = SSAVC_NONE;
}

static void ssav_anim_lineinsert(struct ssav_prepare_ctx *ctx,
	struct ssav_controller *ctr)
{
	int reset_nodep = (ctx->nodenextp == &ctx->vl->node_first);

	if (ctr->type == SSAVC_NONE)
		return;

	ctx->vl = xrealloc(ctx->vl, sizeof(struct ssav_line) +
		sizeof(struct ssav_controller) * (ctx->vl->nctr + 1));
	memcpy(&ctx->vl->ctrs[ctx->vl->nctr++], ctr, sizeof(*ctr));
	if (reset_nodep)
		ctx->nodenextp = &ctx->vl->node_first;
	ctx->wrap.vl = ctx->vl;

	ctr->type = SSAVC_NONE;
}

static void ssav_anim(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	struct ssa_node *cn = n->v.t.node_first;
	struct ssav_controller ctr;
	if (n->v.t.flags & SSA_T_HAVETIMES) {
		ctr.t1 = n->v.t.times[0] * 0.001;
		ctr.length_rez = 1. / ((n->v.t.times[1] - n->v.t.times[0]) * 0.001);
	} else {
		ctr.t1 = 0.0;
		ctr.length_rez = 1. / (ctx->vl->end - ctx->vl->start);
	}
	ctr.accel = n->v.t.accel;
	ctr.type = SSAVC_NONE;
	while (cn) {
		if (SSAN(cn->type) < SSAN_MAX) {
			struct ssa_ipnode *ip = &iplist[SSAN(cn->type)];
			if (ip->afunc)
				ip->afunc(ctx, cn, ip->param, &ctr);
			else
				ssav_add_error_dlg(SSAVEC_NOANIM, NULL,
					ctx, cn);
		}
		cn = cn->next;
	}
}

static void ssav_finalizeds(struct ssav_node *vn)
{
	while (vn) {
		if (vn->params->nctr && !vn->params->finalized) {
			vn->params->finalized = malloc(
				sizeof(struct ssav_params));
			memcpy(vn->params->finalized, vn->params,
				sizeof(*vn->params->finalized));
		}
		vn = vn->next;
	}
}

/****************************************************************************/

static void ssav_nl(struct ssav_prepare_ctx *ctx,
				struct ssa_node *n, ptrdiff_t param)
{
	struct ssav_node *vn = xmalloc(sizeof(struct ssav_node));
	memset(vn, 0, sizeof(*vn));
	vn->type = n->type == SSAN_NEWLINEH ? SSAVN_NEWLINEH : SSAVN_NEWLINE;
	vn->params = ssav_addref(ctx->pset);
	*ctx->nodenextp = vn;
	ctx->nodenextp = &vn->next;
}

static void ssav_text(struct ssav_prepare_ctx *ctx, struct ssa_node *n, ptrdiff_t param)
{
	struct ssav_node *vn;

	if (!ctx->pset->font || (n->v.text.s == n->v.text.e))
		return;

	vn = xmalloc(sizeof(struct ssav_node));
	vn->next = NULL;
	vn->type = SSAVN_TEXT;
	vn->params = ssav_addref(ctx->pset);
	vn->group = ctx->ng;
	ctx->ng_ref++;
	vn->nchars = (unsigned)(n->v.text.e - n->v.text.s);
	vn->indici = xmalloc(sizeof(unsigned) * (n->v.text.e - n->v.text.s));
	vn->glyphs = NULL;
	*ctx->nodenextp = vn;
	ctx->nodenextp = &vn->next;

	ssaw_put(&ctx->wrap, n, vn, ctx->pset->font);
}

/* XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX *\
|* XXX  D   E   P   R   E   C   A   T   E   D  XXX *|
\* XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX#XXX */

#if 0
static void ssap_fixa(struct ssav_prepare_ctx *ctx, struct ssa_node *n)
{
	n->type = SSAN_ALIGNNUM;
	n->v.lval = ssa_a2an(n->v.lval);
}

static void ssap_font(struct ssav_prepare_ctx *ctx, struct ssa_node *n, ptrdiff_t param)
{
	struct ssa_style *reset_to;

	switch (SSAN(n->type)) {
	case SSAN_BOLD:
		if (ssa_lv2weight(n->v.lval) == ctx->fontweight)
			return;
		ctx->fontweight = ssa_lv2weight(n->v.lval);
		break;
	case SSAN_ITALICS:
		if (n->v.lval == ctx->italic)
			return;
		ctx->italic = n->v.lval;
		break;
	case SSAN_FONT:
		if (!ssa_strcmp(&n->v.text, ctx->fontname))
			return;
		ctx->fontname = &n->v.text;
		break;
	case SSAN_RESET:
		reset_to = n->v.style ? n->v.style : ctx->l->style;
		ctx->fontname = &reset_to->fontname;
		ctx->italic = reset_to->italic;
		ctx->fontweight = ssa_lv2weight(reset_to->fontweight);
	}
	asaf_frelease(ctx->font);
	ctx->font = ssa_font(ctx->fontname, ctx->italic, ctx->fontweight);
	if (!ctx->font)
		fprintf(stderr, "WARNING: node font load failed\n");
	return;
}

#endif

#define override(attr) vl->attr = l->attr ? l->attr : l->style->attr

static void ssav_prep_dialogue(struct ssa *ssa, struct ssa_vm *vm,
	struct ssa_line *l, struct ssa_frag **hint)
{
	struct ssa_node *cn = l->node_first;
	struct ssav_line *vl;
	struct ssav_prepare_ctx ctx;

	if (!l->style->vmptr)
		return;

	vl = xmalloc(sizeof(struct ssav_line));
	vl->start = l->start;
	vl->end = l->end;
	vl->ass_layer = l->ass_layer;
#if SSA_DEBUG
	vl->input = l;
#endif
	ssav_setalign(vl, l->style->align, ssa->version & SSAVV_4);
	override(marginl);
	override(marginr);
	override(margint);
	override(marginb);
	vl->wrap = ssa->wrapstyle;
	vl->unit_first = NULL;
	vl->node_first = NULL;
	vl->base.clip.xMin = 0;
	vl->base.clip.xMax = -1;
	vl->base.clip.yMin = 0;
	vl->base.clip.yMax = -1;
	vl->nctr = 0;
	vl->flags = 0;

	ctx.ssa = ssa;
	ctx.vm = vm;
	ctx.l = l;
	ctx.vl = vl;
	ctx.nodenextp = &vl->node_first;
	ctx.ng_ref = 1;
	ssav_ng_invalidate(&ctx);

	ssaw_prepare(&ctx.wrap, vl);

	ctx.pset = NULL;
	ctx.pset = ssav_get_style(&ctx, l->style);
	while (cn) {
		if (SSAN(cn->type) < SSAN_MAX) {
			struct ssa_ipnode *ip = &iplist[SSAN(cn->type)];
			if (ip->func)
				ip->func(&ctx, cn, ip->param);
			else
				ssav_add_error_dlg(SSAVEC_NOTSUP, NULL,
					&ctx, cn);
		}
		cn = cn->next;
	}
	ssav_release(ctx.pset);
	vl = ctx.vl;
	if (!ctx.ng_ref)
		xfree(ctx.ng);
	ssav_finalizeds(vl->node_first);

	ssaw_finish(&ctx.wrap);

	if (vl->node_first) {
		struct ssa_frag *rv = ssap_frag_add(vm, *hint, vl);
		if (rv)
			*hint = rv;
	} else
		xfree(vl);
}

void ssav_create(struct ssa_vm *vm, struct ssa *ssa)
{
	struct ssa_style *s = ssa->style_first;
	struct ssa_line *l = ssa->line_first;
	struct ssa_frag *hint;

	vm->playresx = ssa->playresx;
	vm->playresy = ssa->playresy;
	vm->errlist = NULL;
	vm->errnext = &vm->errlist;
	if ((vm->playresx == 0.0) ^ (vm->playresy == 0.0)) {
		if (vm->playresx == 0.0)
			vm->playresx = vm->playresy * (4./3.);
		else if (fabs(vm->playresx - 1280.0) < 0.1)
			vm->playresy = vm->playresx * (4./5.);
		else
			vm->playresy = vm->playresx * (3./4.);
	}
	vm->firstlayer = NULL;

	while (s) {
		s->vmptr = ssav_alloc_style(ssa, vm, s);
		s = s->next;
	}

	hint = ssap_frag_init(vm);

	while (l) {
		if (l->type == SSAL_DIALOGUE)
			ssav_prep_dialogue(ssa, vm, l, &hint);
		l = l->next;
	}
	return;
}

