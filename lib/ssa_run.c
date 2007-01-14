/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005, 2006, 2007  David Lamparter
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
#include "asa.h"
#include "asaproc.h"
#include "asafont.h"
#include "ssavm.h"
#include "ssarun.h"

#include "freetype/ftstroke.h"

#define HACKING 1

#define clip(x,a,b) ((x) < (a) ? a : ((x) > (b) ? (b) : (x)))

static inline void ssar_one(struct ssa_vm *vm, FT_OutlineGlyph *g,
	struct ssav_unit *u, struct assp_param *p, FT_Stroker stroker,
	FT_Vector org, double shad)
{
	FT_Glyph transformed;
	FT_Glyph stroked;
	FT_Outline *o;
	FT_Raster_Params params;
	FT_Vector orgneg;
	FT_BBox cbox;

	params.flags      	= ft_raster_flag_aa | ft_raster_flag_direct;
	params.black_spans	= assp_spanfunc;
	params.gray_spans	= assp_spanfunc;
	params.user		= p;
	params.target		= NULL;

	org.x >>= 10;
	org.y >>= 10;
	orgneg.x = u->final.x - org.x;
	orgneg.y = u->final.y - org.y;
	FT_Glyph_Copy(*(FT_Glyph *)g, &transformed);
	FT_Glyph_Transform(transformed, NULL, &orgneg);
	FT_Glyph_Transform(transformed, &u->fx1, &org);
	FT_Glyph_Transform(transformed, &vm->scale, NULL);

	/* XXX XXX XXX: if border or shadow are in-view, but char isn't,
	 * this BREAKS */
	FT_Glyph_Get_CBox(transformed, FT_GLYPH_BBOX_PIXELS, &cbox);
	if (cbox.xMax < p->cx0 || cbox.xMin >= p->cx1
		|| cbox.yMax < p->cy0 || cbox.yMin >= p->cy1) {
		FT_Done_Glyph(transformed);
		return;
	}

	p->elem = 0;
	o = &((FT_OutlineGlyph)transformed)->outline;
	FT_Outline_Render(asaf_ftlib, o, &params);

	stroked = transformed;
	FT_Glyph_Stroke(&stroked, stroker, 0);

	p->elem = 2;
	o = &((FT_OutlineGlyph)stroked)->outline;
	FT_Outline_Render(asaf_ftlib, o, &params);

	if (fabs(shad) > 0.01) {
		FT_Vector shaddist;

		shaddist.x = (FT_Pos)(shad * 65536);
		shaddist.y = (FT_Pos)(shad * 65536);
		if (vm->scalebas)
			FT_Vector_Transform(&shaddist, &vm->scale);
		FT_Glyph_Transform(transformed, NULL, &shaddist);

		p->elem = 3;
		o = &((FT_OutlineGlyph)transformed)->outline;
		FT_Outline_Render(asaf_ftlib, o, &params);
	}

	FT_Done_Glyph(transformed);
	FT_Done_Glyph(stroked);
}

void ssar_line(struct ssa_vm *vm, struct ssav_line *l, struct assp_fgroup *fg)
{
	struct ssav_node *n = l->node_first;
	struct ssav_unit *u = l->unit_first;
	struct assp_frameref *prevg = NULL;
	int ustop = u->next ? u->next->idxstart : l->nchars, idx = 0;
	FT_Stroker stroker;

	FT_Stroker_New(asaf_ftlib, &stroker);
	while (n) {
		struct assp_frameref *ng = n->group;
		struct assp_param p;
		FT_OutlineGlyph *g, *gend;
		struct ssav_params *np = n->params;
		FT_Pos bordersize;
		FT_Vector cpos;

		if (np->finalized)
			np = np->finalized;

		if (n->type != SSAVN_TEXT) {
			n = n->next;
			continue;
		}

		if (ng != prevg) {
			assp_framefree(ng);
			assp_framenew(ng, fg);
			prevg = ng;
		}

		p.f = ng->frame;
		cpos.x = l->active.clip.xMin;
		cpos.y = l->active.clip.yMin;
		FT_Vector_Transform(&cpos, &vm->scale);
		p.cx0 = clip(cpos.x >> 16, 0, (int)ng->frame->group->w);
		p.cy0 = clip(cpos.y >> 16, 0, (int)ng->frame->group->h);
		cpos.x = l->active.clip.xMax;
		cpos.y = l->active.clip.yMax;
		FT_Vector_Transform(&cpos, &vm->scale);
		p.cx1 = clip(cpos.x >> 16, 0, (int)ng->frame->group->w);
		p.cy1 = clip(cpos.y >> 16, 0, (int)ng->frame->group->h);
		p.xo = p.yo = 0;

		g = n->glyphs;
		gend = g + n->nchars;

		if (!g) {
			fprintf(stderr, "NO GLYPH\n");
			return;
		}

		bordersize = (FT_Pos)(np->border * 64);
		if (vm->scalebas) {
			FT_Vector border = {bordersize, bordersize};
			FT_Vector_Transform(&border, &vm->scale);
			/* this is the vsfilter way, don't bug me >_> */
			bordersize = (border.x + border.y) >> 1;
		}
		FT_Stroker_Set(stroker, bordersize, FT_STROKER_LINECAP_ROUND,
			FT_STROKER_LINEJOIN_ROUND, 0);
		while (g < gend) {
			while (idx == ustop) {
				u = u->next;
				ustop = u->next ? u->next->idxstart : l->nchars;
			}
			ssar_one(vm, g, u, &p, stroker, l->active.org, np->shadow);
			g++, idx++;
		}
		n = n->next;
	}
	FT_Stroker_Done(stroker);
}

static void ssar_commit(struct ssav_line *l)
{
	struct assp_frameref *prev = NULL;
	struct ssav_node *n = l->node_first;

	while (n) {
		if (n->type == SSAVN_TEXT && n->group != prev
			&& n->group && n->group->frame) {
			struct ssav_params *p = n->params;
			if (p->finalized)
				p = p->finalized;

			prev = n->group;
			prev->frame->colours[0] = p->r.colours[0];
			prev->frame->colours[1] = p->r.colours[1];
			prev->frame->colours[2] = p->r.colours[2];
			prev->frame->colours[3] = p->r.colours[3];
			asar_commit(prev->frame);
		}
		n = n->next;
	}
}

void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg)
{
	unsigned ln;
	enum ssar_redoflags fl, basefl;

	if (!vm->cache || vm->cache->start > ftime)
		vm->cache = vm->fragments;
	while (vm->cache->next && vm->cache->next->start <= ftime)
		vm->cache = vm->cache->next;

	basefl = assa_start(vm);
	for (ln = 0; ln < vm->cache->nrend; ln++) {
		struct ssav_line *l = vm->cache->lines[ln];
		if (!l->unit_first)
			continue;

		fl = ssar_eval(vm, l, ftime, basefl);
		fl = assa_realloc(vm, l, fl);
		if (fl & SSAR_REND)
			ssar_line(vm, l, fg);
		ssar_commit(l);
	}
	assa_end(vm);
}

