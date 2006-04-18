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

#include "common.h"
#include "ssa.h"
#include "ssawrap.h"
#include "ssavm.h"
#include "ssarun.h"
#include "asa.h"
#include "asafont.h"

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
	struct ssar_nodegroup *ng;
	int ng_ref;

	struct ssa_wrap_env wrap;
};

#define ifp struct ssav_prepare_ctx *ctx, struct ssa_node *n, ptrdiff_t param

typedef void ssav_ipfunc(ifp);

static void ssav_nl(ifp);
static void ssav_text(ifp);
static void ssav_fontsize(ifp);
static void ssav_reset(ifp);

static void ssav_double(ifp);
static void ssav_colour(ifp);

static void ssav_align(ifp);
static void ssav_lineint(ifp);
static void ssav_linenode(ifp);

struct ssa_ipnode {
	ssav_ipfunc *func;
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
	{NULL,		0},
	{ssav_text,	0},		/* Sl SSAN_TEXT */
	{ssav_nl,	0},		/* Sl SSAN_NEWLINE */
	{ssav_nl,	0},		/* Sl SSAN_NEWLINEH */
	{NULL,		0},		/* !l SSAN_BOLD */
	{NULL,		0},		/* ?l SSAN_ITALICS */
	{NULL,		0},		/* ?l SSAN_UNDERLINE */
	{NULL,		0},		/* ?l SSAN_STRIKEOUT */	
	{ssav_double,	e(border)},	/* al SSAN_BORDER */
	{NULL,		0},		/* al SSAN_SHADOW */
	{NULL,		0},		/* ?? SSAN_BLUREDGES */
	{NULL,		0},		/* ?l SSAN_FONT */
	{ssav_fontsize,	0},		/* ?l SSAN_FONTSIZE */

	{ssav_double,	e(m.fscx)},	/* al SSAN_FSCX */
	{ssav_double,	e(m.fscy)},	/* al SSAN_FSCY */
	{ssav_double,	e(m.frx)},	/* aG SSAN_FRX */
	{ssav_double,	e(m.fry)},	/* aG SSAN_FRY */
	{ssav_double,	e(m.frz)},	/* aG SSAN_FRZ */
	{ssav_double,	e(m.fax)},	/* aG SSAN_FAX */
	{ssav_double,	e(m.fay)},	/* aG SSAN_FAY */

	{ssav_double,	e(m.fsp)},	/* ?l SSAN_FSP */
	{NULL,		0},		/* -- SSAN_FE */
	{ssav_colour,	0x0},		/* al SSAN_COLOUR */
	{ssav_colour,	0x1},		/* al SSAN_COLOUR2 */
	{ssav_colour,	0x2},		/* al SSAN_COLOUR3 */
	{ssav_colour,	0x3},		/* al SSAN_COLOUR4 */
	{ssav_colour,	0x10},		/* al SSAN_ALPHA */
	{ssav_colour,	0x11},		/* al SSAN_ALPHA2 */
	{ssav_colour,	0x12},		/* al SSAN_ALPHA3 */
	{ssav_colour,	0x13},		/* al SSAN_ALPHA4 */
	{ssav_align,	1},		/* SG SSAN_ALIGN */
	{ssav_align,	0},		/* SG SSAN_ALIGNNUM */
	{NULL,		0},		/* Sl SSAN_KARA */
	{NULL,		0},		/* Sl SSAN_KARAF */
	{NULL,		0},		/* Sl SSAN_KARAO */
	{ssav_lineint,	l(wrap)},	/* SG SSAN_WRAP */
	{ssav_reset,	0},		/* *l SSAN_RESET */

	{NULL,		0},		/* S* SSAN_T */
	{NULL,		0},		/* SG SSAN_MOVE */
	{ssav_linenode,	l(pos)},	/* aG SSAN_POS */
	{NULL,		0},		/* SG SSAN_ORG */
	{NULL,		0},		/* S? SSAN_FADE */
	{NULL,		0},		/* S? SSAN_FAD */
	{NULL,		0},		/* aG SSAN_CLIPRECT */
	{NULL,		0},		/* SG SSAN_CLIPDRAW */
	{NULL,		0},		/* Sl SSAN_PAINT */
	{NULL,		0},		/* ?l SSAN_PBO */

};

static inline int ssa_strcmp(const ssa_string *a, const ssa_string *b)
{
	ptrdiff_t lena = a->e - a->s, lenb = b->e - b->s;
	if (lena != lenb)
		return 1;
	return memcmp(a->s, b->s, lena * sizeof(ssaout_t));
}

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

static struct ssav_params *ssav_get_style(struct ssa_style *style)
{
	return ssav_addref((struct ssav_params *)style->vmptr);
}

static struct ssav_params *ssav_alloc_clone(struct ssav_params *p)
{
	struct ssav_params *rv;

	if (p->nref == 1)
		return p;
	rv = xmalloc(sizeof(struct ssav_params));
	memcpy(rv, p, sizeof(*rv));
	
	rv->nref = 1;
	asaf_faddref(rv->font);
	asaf_saddref(rv->fsiz);
	p->nref--;
	return rv;
}

static void ssav_set_font(struct ssav_params *pset,
	ssa_string *name, long int weight, long int italic, double size)
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
	pset->f.weight = weight;
	pset->f.italic = !!italic;
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
				abort();	/*XXX*/
		}
	}
	pset->fsiz = asaf_reqsize(pset->font, pset->f.size);
}

static struct ssav_params *ssav_alloc_style(struct ssa *ssa,
	struct ssa_style *style)
{
	struct ssav_params *rv = xmalloc(sizeof(struct ssav_params));
	memset(rv, 0, sizeof(*rv));

	ssav_set_font(rv, &style->fontname, style->fontweight,
		style->italic, style->fontsize);

	rv->m.fscx = style->scx;
	rv->m.fscy = style->scy;
	rv->m.frz = style->rot;
	rv->m.frx = rv->m.fry = 0.0;
	rv->m.fax = rv->m.fay = 0.0;
	rv->m.fsp = style->sp;

	rv->r.colours[0] = style->cprimary;
	rv->r.colours[1] = style->csecondary;
	rv->r.colours[2] = ssa->version == SSAV_4P ?
		style->coutline : style->cback;
	rv->r.colours[3] = style->cback;

	rv->nref = 1;
	return rv;
}

/****************************************************************************/

static void ssav_ng_invalidate(struct ssav_prepare_ctx *ctx)
{
	if (ctx->ng_ref) {
		ctx->ng = xmalloc(sizeof(struct ssar_nodegroup));
		memset(ctx->ng, 0, sizeof(*ctx->ng));
		ctx->ng_ref = 0;
	}
}

static void ssav_assign_pset(struct ssav_prepare_ctx *ctx,
	struct ssav_params *newpset)
{
	if (memcmp(&ctx->pset->r, &newpset->r, sizeof(ctx->pset->r)))
		ssav_ng_invalidate(ctx);
	ctx->pset = newpset;
}

/****************************************************************************/

static void ssav_double(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	ctx->pset = ssav_alloc_clone(ctx->pset);
	*(double *)apply_offset(ctx->pset, param) = n->v.dval;
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
	ssav_release(ctx->pset);
	ssav_assign_pset(ctx, ssav_get_style(reset_to));
}

static void ssav_colour(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	colour_t mask, value;
	value.l = mask.l = 0;
	if (param & 0x10) {
		value.c.a = n->v.alpha;
		mask.c.a = 0xFF;
	} else {
		value = n->v.colour;
		mask.c.r = mask.c.g = mask.c.b = 0xFF;
	}
	param &= 3;
	if ((ctx->pset->r.colours[param].l & mask.l) == value.l)
		return;
	ctx->pset = ssav_alloc_clone(ctx->pset);
	ctx->pset->r.colours[param].l &= ~mask.l;
	ctx->pset->r.colours[param].l |= value.l;
	ssav_ng_invalidate(ctx);
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

static void ssav_linenode(struct ssav_prepare_ctx *ctx, struct ssa_node *n,
	ptrdiff_t param)
{
	*(struct ssa_node **)apply_offset(ctx->vl, param) = n;
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
	struct ssav_line *vl = xmalloc(sizeof(struct ssav_line));
	struct ssav_prepare_ctx ctx;

	vl->start = l->start;
	vl->end = l->end;
	vl->ass_layer = l->ass_layer;
#if SSA_DEBUG
	vl->input = l;
#endif
	ssav_setalign(vl, l->style->align, ssa->version < SSAV_4P);
	override(marginl);
	override(marginr);
	override(marginv);
	vl->wrap = ssa->wrapstyle;
	vl->pos = NULL;
	vl->unit_first = NULL;

	ctx.ssa = ssa;
	ctx.vm = vm;
	ctx.l = l;
	ctx.vl = vl;
	ctx.nodenextp = &vl->node_first;
	ctx.ng_ref = 1;
	ssav_ng_invalidate(&ctx);

	ssaw_prepare(&ctx.wrap, vl);

	ctx.pset = ssav_get_style(l->style);
	while (cn) {
		if (SSAN(cn->type) < SSAN_MAX) {
			struct ssa_ipnode *ip = &iplist[SSAN(cn->type)];
			if (ip->func)
				ip->func(&ctx, cn, ip->param);
		}
		cn = cn->next;
	}
	ssav_release(ctx.pset);
	if (!ctx.ng_ref)
		xfree(ctx.ng);

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
	if ((vm->playresx == 0.0) ^ (vm->playresy == 0.0)) {
		if (vm->playresx == 0.0)
			vm->playresx = vm->playresy * (4./3.);
		else if (fabs(vm->playresx - 1280.0) < 0.1)
			vm->playresy = vm->playresx * (4./5.);
		else
			vm->playresy = vm->playresx * (3./4.);
	}

	while (s) {
		s->vmptr = ssav_alloc_style(ssa, s);
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

