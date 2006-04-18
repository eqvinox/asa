/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005, 2006  David Lamparter
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

void ssar_line(struct ssav_line *l, struct assp_fgroup *fg)
{
	struct ssav_node *n = l->node_first;
	struct ssav_unit *u = l->unit_first;
	int ustop = u->next ? u->next->idxstart : l->nchars, idx = 0;
	FT_Vector pos;

	pos.x = pos.y = 0;
	
while (n) {
	struct ssar_nodegroup *ng = n->group;
	struct assp_param p;
	FT_OutlineGlyph *g, *gend;
	FT_Stroker stroker;

	if (!ng->frame)
		ng->frame = assp_framenew(fg);

	p.f = ng->frame;
	p.cx0 = 0;
	p.cy0 = 0;
	p.cx1 = ng->frame->group->w;
	p.cy1 = ng->frame->group->h;
	p.xo = p.yo = 0;

	ng->frame->colours[0] = n->params->r.colours[0];
	ng->frame->colours[2] = n->params->r.colours[2];

	g = n->glyphs;
	gend = g + n->nchars;

	if (!g) {
		fprintf(stderr, "NO GLYPH\n");
		return;
	}

	FT_Stroker_New(n->params->font->face->memory, &stroker);

	/* XXX XXX XXX formula is incorrect!
	 * someone go figure a correct one! */
	FT_Stroker_Set(stroker, (int)(n->params->border * 64),
		FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

	while (g < gend) {
		FT_Glyph transformed;
		FT_Glyph stroked;
		FT_Outline *o;
		FT_Raster_Params params;

		if (idx == ustop) {
			pos.x += u->size.x;
			pos.y += u->size.y;
			u = u->next;
			ustop = u->next ? u->next->idxstart : l->nchars;
		}

		// FT_Glyph_Copy(*g, &copy); - XXX?
		transformed = *g;
		FT_Glyph_Transform(transformed, NULL, &u->final);
		o = &((FT_OutlineGlyph)transformed)->outline;

		p.elem = 0;

		params.flags      = ft_raster_flag_aa
				| ft_raster_flag_direct;
		params.black_spans	= assp_spanfunc;
		params.gray_spans	= assp_spanfunc;
		params.user	= &p;
		params.target	= NULL;
			   
		FT_Outline_Render(asaf_ftlib, o, &params);
		
		stroked = transformed; // XXX?
		FT_Glyph_Stroke(&stroked, stroker, 0);

		p.elem = 2;
		FT_Outline_Render(asaf_ftlib, &((FT_OutlineGlyph)stroked)->outline, &params);
		//FT_Done_Glyph(transformed); - XXX?
		//FT_Done_Glyph(stroked); - XXX?

		g++, idx++;
	}

	FT_Stroker_Done(stroker);
	n = n->next;
}
}

static void ssar_commit(struct ssav_line *l)
{
	struct ssar_nodegroup *prev = NULL;
	struct ssav_node *n = l->node_first;

	while (n) {
		if (n->group != prev && n->group->frame)
			asar_commit((prev = n->group)->frame);
		n = n->next;
	}
}

void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg)
{
	unsigned ln;
	enum ssar_redoflags fl;

	if (!vm->cache || vm->cache->start > ftime)
		vm->cache = vm->fragments;
	while (vm->cache->next && vm->cache->next->start <= ftime)
		vm->cache = vm->cache->next;

	assa_start(vm);
	for (ln = 0; ln < vm->cache->nrend; ln++) {
		struct ssav_line *l = vm->cache->lines[ln];

#if HACKING
		fl = 0;
#else
		fl = ssar_eval(l, ftime);
#endif
		fl = assa_realloc(vm, l, fl);
		if (fl & SSAR_REND)
			ssar_line(l, fg);
		ssar_commit(l);
	}
	assa_end(vm);
}

