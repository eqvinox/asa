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

#include "common.h"
#include "asaproc.h"
#include "asafont.h"
#include "ssavm.h"
#include "ssarun.h"

#include <math.h>
#include "freetype/ftstroke.h"

#define HACKING 1

#define clip(x,a,b) ((x) < (a) ? a : ((x) > (b) ? (b) : (x)))


/* Fixed to float.
   By ArchMage ZeratuL */
static inline float fixed_to_float(signed long n) {
	return ((float)(n))/64;
}

/* Float to fixed.
   By ArchMage ZeratuL */
static inline signed long float_to_fixed(float n) {
	return (int)(n * 64);
}

/* This function takes a 4-float point and multiplies it by a 4x4 matrix.
   Point is given in a float[4] = {x,y,z,w} format.
   By ArchMage ZeratuL */
static inline void ssar_3dmatrix_transform_point(float *p,float *m) {
	float t[4];

	/* Multiply */
	t[0] = p[0]*m[0] + p[1]*m[4] +  p[2]*m[8] + p[3]*m[12];
	t[1] = p[0]*m[1] + p[1]*m[5] +  p[2]*m[9] + p[3]*m[13];
	t[2] = p[0]*m[2] + p[1]*m[6] + p[2]*m[10] + p[3]*m[14];
	t[3] = p[0]*m[3] + p[1]*m[7] + p[2]*m[11] + p[3]*m[15];

	/* Limit w to 0.05f, to avoid a division by 0 */
	if (t[3] < 0) {
		t[0] = -t[0];
		t[1] = -t[1];
		t[2] = -t[2];
		t[3] = -t[3];
	}
	if (t[3] < 0.05f) t[3] = 0.05f;

	/* Copy back (I don't think that any other method would be faster here? memcpy, maybe?) */
	p[0] = t[0];
	p[1] = t[1];
	p[2] = t[2];
	p[3] = t[3];
}

/* This function takes a freetype glyph and multiplies it by a 4x4 float matrix.
   Matrix is given in a colmajor format, that is, first column is [0], [1], [2] and [3]
   By ArchMage ZeratuL */
static inline void ssar_3dmatrix_transform_glyph(FT_Glyph glyph,float *matrix) {
	FT_Outline *outline;
	float point[4];
	int i;

	/* Get outline */
	outline = &((FT_OutlineGlyph) glyph)->outline;

	/* Process points */
	for (i=0;i<outline->n_points;i++) {
		/* Convert to float */
		point[0] = fixed_to_float(outline->points[i].x);
		point[1] = fixed_to_float(outline->points[i].y);
		point[2] = 0.0f;
		point[3] = 1.0f;

		/* Multiply by the matrix */
		ssar_3dmatrix_transform_point(point,matrix);

		/* Convert back to fixed */
		outline->points[i].x = float_to_fixed(point[0]/point[3]);
		outline->points[i].y = float_to_fixed(point[1]/point[3]);
	}

	/* Process the glyph's advance, same as above */
	point[0] = fixed_to_float(glyph->advance.x);
	point[1] = fixed_to_float(glyph->advance.y);
	point[2] = 0.0f;
	point[3] = 1.0f;
	ssar_3dmatrix_transform_point(point,matrix);
	glyph->advance.x = float_to_fixed(point[0]/point[3]);
	glyph->advance.y = float_to_fixed(point[1]/point[3]);
}

extern void assp_spanfunc(int y, int count, const FT_Span *spans, void *user);
extern void assp_spanblur(int y, int count, const FT_Span *spans, void *user);

static inline void ssar_one(struct ssa_vm *vm, FT_OutlineGlyph *g,
	struct ssav_unit *u, struct assp_param *p, FT_Stroker stroker,
	FT_Vector org, unsigned blur, double shad, FT_Pos bord,
	float *matrix3d, int tgt)
{
	FT_Glyph transformed;
	FT_Outline *o;
	FT_Raster_Params params;
	FT_Vector orgneg;
	FT_BBox cbox;
	FT_Vector shaddist;

	params.flags      	= ft_raster_flag_aa | ft_raster_flag_direct;
	params.black_spans	= assp_spanfunc;
	params.gray_spans	= assp_spanfunc;
	params.user		= p;
	params.target		= NULL;

	shaddist.x = (FT_Pos)(shad * 64);
	shaddist.y = (FT_Pos)(shad * 64);
	if (vm->scalebas)
		FT_Vector_Transform(&shaddist, &vm->fscale);

	org.x >>= 10;
	org.y >>= 10;
	orgneg.x = u->final.x - org.x;
	orgneg.y = u->final.y - org.y;
	FT_Glyph_Copy(*(FT_Glyph *)g, &transformed);
	FT_Glyph_Transform(transformed, NULL, &orgneg);

	/* \frz, \frx, \fry, \fax, \fay */
	ssar_3dmatrix_transform_glyph(transformed,matrix3d);

	/* move */
	FT_Glyph_Transform(transformed, NULL, &org);

	/* \fscx, \fscy and playres scaling are done directly after
	 * loading the glyph.
	 * - FT_Glyph_Transform(transformed, &vm->cscale, NULL); -
	 */

	bord = (bord + 63) >> 6;
	FT_Glyph_Get_CBox(transformed, FT_GLYPH_BBOX_PIXELS, &cbox);
	cbox.xMin -= bord - (shaddist.x < 0 ? shaddist.x : 0);
	cbox.xMax += bord + (shaddist.x > 0 ? shaddist.x : 0);
	cbox.yMin -= bord - (shaddist.y < 0 ? shaddist.y : 0);
	cbox.yMax += bord + (shaddist.y > 0 ? shaddist.y : 0);
	if (cbox.xMax < p->cx0 || cbox.xMin >= p->cx1
		|| cbox.yMax < p->cy0 || cbox.yMin >= p->cy1) {
		FT_Done_Glyph(transformed);
		return;
	}

	p->elem = tgt;
	o = &((FT_OutlineGlyph)transformed)->outline;
	FT_Outline_Render(asaf_ftlib, o, &params);

	params.black_spans	= blur ? assp_spanblur : assp_spanfunc;
	params.gray_spans	= blur ? assp_spanblur : assp_spanfunc;

	if (bord) {
		FT_Vector borderbugfix;
		borderbugfix.x = -cbox.xMin << 10;
		borderbugfix.y = -cbox.yMin << 10;
		FT_Glyph_Transform(transformed, NULL, &borderbugfix);

		FT_Glyph_StrokeBorder(&transformed, stroker, 0, 1);

		borderbugfix.x *= -1;
		borderbugfix.y *= -1;
		FT_Glyph_Transform(transformed, NULL, &borderbugfix);

		p->elem = 2;
		o = &((FT_OutlineGlyph)transformed)->outline;
		FT_Outline_Render(asaf_ftlib, o, &params);
	}

	if (shaddist.x || shaddist.y) {
		FT_Outline_Translate(o, shaddist.x, shaddist.y);

		p->elem = 3;
		FT_Outline_Render(asaf_ftlib, o, &params);
	}

	FT_Done_Glyph(transformed);
}

void ssar_line(struct ssa_vm *vm, struct ssav_line *l, struct assp_fgroup *fg,
	double ltime)
{
	struct ssav_node *n = l->node_first;
	struct ssav_unit *u = l->unit_first;
	struct assp_frameref *prevg = NULL;
	int ustop = u->next ? u->next->idxstart : l->nchars, idx = 0;
	int tgt;
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

		/* actually, active.clip is set in ssa_anim.c:90 */
		p.f = ng->frame;
		cpos.x = l->active.clip.xMin;
		cpos.y = l->active.clip.yMin;
		FT_Vector_Transform(&cpos, &vm->cscale);
		p.cx0 = clip(cpos.x >> 16, 0, (int)ng->frame->group->w);
		p.cy0 = clip(cpos.y >> 16, 0, (int)ng->frame->group->h);
		cpos.x = l->active.clip.xMax;
		cpos.y = l->active.clip.yMax;
		FT_Vector_Transform(&cpos, &vm->cscale);
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

		if (n->kara && ltime < n->kara->start) {
			tgt = 1;
			if (n->kara->type == SSAVK_BORD)
				bordersize = 0;
		} else
			tgt = 0;

		if (bordersize && vm->scalebas) {
			FT_Vector border = {bordersize, bordersize};
			FT_Vector_Transform(&border, &vm->fscale);
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
			ssar_one(vm, g, u, &p, stroker, l->active.org, np->blur,
				np->shadow, bordersize, n->matrix3d, tgt);
			g++, idx++;
		}
		n = n->next;
	}
	FT_Stroker_Done(stroker);
}

static void ssar_commit(struct ssav_line *l, struct csri_frame *dst)
{
	struct assp_frameref *prev = NULL;
	struct ssav_node *n = l->node_first;
	int c;

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
			for (c = 0; c < 4; c++) {
				unsigned a = prev->frame->colours[c].c.a;
				a = 256 - a;
				a *= 255 - p->r.fade.c.a;
				prev->frame->colours[c].c.a = (unsigned char)
					255 - (a >> 8);
			}
			asa_blit(prev->frame, dst);
		}
		n = n->next;
	}
}

void ssar_dispose(struct ssav_line *l)
{
	struct ssav_node *n;

	for (n = l->node_first; n; n = n->next)
		if (n->group)
			assp_framefree(n->group);
}

void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg, struct csri_frame *dst)
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
		if (fl & SSAR_REND || l->flags & SSAV_KARAOKE)
			ssar_line(vm, l, fg, ftime - l->start);
		ssar_commit(l, dst);
	}
	assa_end(vm);
}

void ssar_flush(struct ssa_vm *vm)
{
	assa_start(vm);
	assa_end(vm);
}

