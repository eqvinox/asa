/* #include(LICENSE) */
/* state: partly clean, uncommented, in-devel, hacks, random code */
#include "common.h"
#include "ssa.h"
#include "ssarend.h"
#include "asa.h"
#include "asafont.h"

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <alloca.h>
#include <iconv.h>
#include <errno.h>
#include <math.h>

#if 0
/* doesn't belong here */
/* just wrote this sometime. textsub compatible. */
#define accel_epsilon 0.0001

void ssap_act(struct ssa *ssa, struct ssa_line *l, struct ssap_act *act,
	double time)
{
	struct ssap_actvalue *v = act->vals, *ve = v + act->count;
	double in = time - l->start - act->start,
		f0, f1;
	long fl0, fl1;
	int idx = -1;

	if (in < 0)
		idx = 0;
	else if (in > act->length)
		idx = 1;
	if (idx != -1) {
		while (v < ve) {
			switch (v->type) {
			case SSAACT_LONG:
				*(v->v.l.d) = v->v.l.n[idx];
				break;
			case SSAACT_DOUBLE:
				*(v->v.d.d) = v->v.d.n[idx];
				break;
			case SSAACT_COLOUR:
				v->v.c.d->l = v->v.c.n[idx].l;
				break;
			};
			v++;
		}
		return;
	}
	if (act->accel > (1.0 - accel_epsilon)
		&& act->accel < (1.0 + accel_epsilon))
		f0 = in * act->lengthrec;
	else
		f0 = pow(in * act->lengthrec, act->accel);
	f1 = 1.0 - f0;
	while (v < ve) {
		switch (v->type) {
		case SSAACT_LONG:
			*(v->v.l.d) = lround(f0 * (double)v->v.l.n[0]
				+ f1 * (double)v->v.l.n[1]);
			break;
		case SSAACT_DOUBLE:
			*(v->v.d.d) = f0 * v->v.d.n[0] + f1 * v->v.d.n[1];
			break;
		case SSAACT_COLOUR:
			fl0 = (long)(f0 * 256);
			fl1 = 256 - fl0;
			v->v.c.d->c.r = (v->v.c.n[0].c.r * fl0 +
				v->v.c.n[1].c.r * fl1) >> 8;
			v->v.c.d->c.g = (v->v.c.n[0].c.g * fl0 +
				v->v.c.n[1].c.g * fl1) >> 8;
			v->v.c.d->c.b = (v->v.c.n[0].c.b * fl0 +
				v->v.c.n[1].c.b * fl1) >> 8;
			v->v.c.d->c.a = (v->v.c.n[0].c.a * fl0 +
				v->v.c.n[1].c.a * fl1) >> 8;
			break;
		};
		v++;
	}
}
#endif 

/* this really doesn't belong here. its just to make the viewer show
 * something */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#define min(a,b) ((a) > (b) ? (b) : (a))
#define max(a,b) ((a) < (b) ? (b) : (a))

struct asa_spanparams {
	unsigned char *dst;
	int x, y, w, h;
	int g;
};

static void asa_span(int y, int count, FT_Span *spans, void *user)
{
	struct asa_spanparams *s = (struct asa_spanparams *)user;
	FT_Span *n = spans;
	unsigned realy;

	if (s->y - y < 0 || s->y - y > (signed)s->h)
		return;
	realy = s->y - y;
	while (count--) {
		unsigned start = max(s->x + n->x, 0),
			end = min(s->x + n->x + n->len, s->w);
		if (start < end) {
			unsigned char *sr = s->dst + realy * s->w + start;
			unsigned char *er = s->dst + realy * s->w + end;
			while (sr < er) {
				unsigned tmp = *sr;
				tmp = ((tmp * (0x100 - n->coverage)) +
					(s->g * (n->coverage + 1))) >> 8;
				*sr = tmp;
				sr++;
			}
		}
		n++;
	}
}

static void asa_one(struct ssa_line *l, unsigned char *dst,
	unsigned w, unsigned h);

void ssa_render(struct ssa_renderer *ssa_rend, double timestamp,
	unsigned char *dst, unsigned w, unsigned h)
{
	unsigned c;
	struct ssa_frag *f = ssa_rend->fragments;
	if (ssa_rend->hint && ssa_rend->hint->start <= timestamp)
		f = ssa_rend->hint;
	while (f->next && f->next->start <= timestamp)
		f = f->next;

	for (c = 0; c < f->nrend; c++)
		asa_one(f->lines[c], dst, w, h);
	ssa_rend->hint = f;
}

static void asa_one(struct ssa_line *l, unsigned char *dst,
	unsigned w, unsigned h)
{
	struct ssa_node *n = l->node_first;
	unsigned xstart = 16 << 6;

		struct asa_spanparams sp;
		sp.dst = dst;
		sp.y = h - 80;
		sp.h = h;
		sp.w = w;
		sp.g = 0xFF;


	while (n) {
		unsigned previdx = 0, cch, *now;
		struct asa_font *fnb;
		struct asa_fontinst *fni;
		FT_Face fnt;

		if (SSAN(n->type) != SSAN_TEXT) {
			n = n->next;
			continue;
		}
		
		cch = n->d.text.numch;
		fnb = n->d.text.font;
		now = n->d.text.indices;

		fni = asaf_reqsize(fnb, 16.0);
		fnt = asaf_sactivate(fni);

//		unsigned ybfb = (fnt->size->metrics.height
//			- fnt->size->metrics.ascender + 0x3F) >> 6;
		while (cch--) {
			FT_Glyph g;
			FT_Outline *o;
			FT_Raster_Params params;
			FT_Vector kerning;

			if (!*now) {
				now++;
				continue;
			}

			if (previdx) {
				FT_Get_Kerning(fnt, previdx, *now,
					ft_kerning_default, &kerning);
				xstart += kerning.x;
			}
			previdx = *now;

			FT_Load_Glyph(fnt, *now, FT_LOAD_DEFAULT);
			FT_Get_Glyph(fnt->glyph, &g);
			
			FT_Vector tmp;
			FT_Matrix tm;

			tmp.x = xstart;
			tmp.y = 0;
			tm.xx = 0x10000L;
			tm.xy = 0x00000L;
			tm.yx = 0x00000L;
			tm.yy = 0x10000L;

			FT_Glyph_Transform(g, &tm, &tmp);
			o = &((FT_OutlineGlyph)g)->outline;

			params.flags      = ft_raster_flag_aa
				| ft_raster_flag_direct;
			params.black_spans = asa_span;
			params.gray_spans = asa_span;
			params.user       = &sp;
			params.target = NULL;
			   
			sp.x = 0; //xstart >> 6;
			FT_Outline_Render(asaf_ftlib, o,
				&params);

			xstart += g->advance.x >> 10; //) + 48;
			FT_Done_Glyph(g);
			now++;
		}
		asaf_srelease(fni);

		n = n->next;
	}
}
