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
#include "ssavm.h"

#include <freetype/fttrigon.h>
#include <assert.h>

static void ssgl_rotmat(FT_Matrix *m, double dangle)
{
	FT_Angle angle = (int)(dangle * 65536);
	m->xx = FT_Cos(angle);
	m->xy = FT_Sin(angle);
	m->yx = -FT_Sin(angle);
	m->yy = FT_Cos(angle);
}

void ssgl_matrix(struct ssav_params *p, FT_Matrix *fx0, FT_Matrix *fx1)
{
	FT_Matrix tmp;

	fx0->xx = (int)(p->m.fscx * 655.36);
	fx0->xy = 0x00000L;
	fx0->yx = 0x00000L;
	fx0->yy = (int)(-p->m.fscy * 655.36);

	fx1->xx = 0x10000L;
	fx1->xy = (FT_Fixed)(p->m.fax * 65536);
	fx1->yx = (FT_Fixed)(p->m.fay * 65536);
	fx1->yy = 0x10000L;
	if (p->m.frz != 0.0) {
		ssgl_rotmat(&tmp, p->m.frz);
		FT_Matrix_Multiply(&tmp, fx1);
	}
}

static void ssgl_prep_glyphs(struct ssav_node *n)
{
	FT_OutlineGlyph *g, *end;
	if (n->glyphs) {
		g = n->glyphs;
		end = g + n->nchars;
		while (g < end) {
			if (*g)
				FT_Done_Glyph(&g[0]->root);
			g++;
		}
	} else
		g = n->glyphs = xmalloc(sizeof(FT_OutlineGlyph)
			* n->nchars);
}

static void ssgl_load_glyph(FT_Face fnt, unsigned idx,
	FT_Vector *pos, FT_Matrix *mat, double fsp, FT_OutlineGlyph *dst)
{
	FT_Glyph tmp;
	FT_Vector tmppos;

	FT_Load_Glyph(fnt, idx, FT_LOAD_DEFAULT);

	*dst = NULL;
	if (fnt->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
		fprintf(stderr,	"non-outline glyph format %d\n",
			fnt->glyph->format);
		return;
	}

	tmppos = *pos;
	FT_Get_Glyph(fnt->glyph, &tmp);
	tmppos.y += fnt->size->metrics.descender;
	FT_Glyph_Transform(tmp, mat, &tmppos);
	pos->x += (tmp->advance.x >> 10) + (int)(fsp * 64);
	pos->y += tmp->advance.y >> 10;

	*dst = (FT_OutlineGlyph)tmp;
}

void ssgl_prepare(struct ssav_line *l)
{
	FT_Vector pos, hv;
	FT_Matrix mat, fx1;
	FT_Face fnt;
	FT_OutlineGlyph *g, *end;
	unsigned idx, stop, *src;

	struct ssav_node *n = l->node_first;
	struct ssav_unit *u = l->unit_first;
	struct ssav_params *p;

	if (!u || !n)
		return;

	u->height = 0;
	idx = 0;
	pos.x = pos.y = 0;
	hv.x = 0;
	while (u && n) {
		p = n->params;
		if (p->finalized)
			p = p->finalized;

		fnt = asaf_sactivate(n->params->fsiz);
		ssgl_matrix(p, &mat, &fx1);
		n->fx1 = fx1;
		hv.y = fnt->size->metrics.height;
		FT_Vector_Transform(&hv, &mat);

		if (u->type == SSAVU_NEWLINE) {
			if (n != u->nl_node) {
				n = u->nl_node;
				continue;
			}
			u->height = -hv.y;

			u = u->next;
			n = n->next;
			pos.x = pos.y = 0;
			continue;
		}

		ssgl_prep_glyphs(n);
		g = n->glyphs;
		end = g + n->nchars;
		src = n->indici;

		stop = u->next ? u->next->idxstart : l->nchars;

		if (idx != stop && -hv.y > u->height)
			u->height = -hv.y;

		while (g < end) {
			ssgl_load_glyph(fnt, *src++, &pos, &mat, p->m.fsp, g++);
			idx++;
			if (idx == stop) {
				u->size = pos;
				u = u->next;
				if (!u)
					return;
				if (u->type == SSAVU_NEWLINE)
					break;

				stop = u->next ? u->next->idxstart
					: l->nchars;
				pos.x = pos.y = 0;
				u->height = -hv.y;
			}
		}

		n = n->next;
	}
	fprintf(stderr, "AIEEEEEEEE! ssgl_prepare: This code should never be reached!\n"
		"Report to equinox @ irc.chatsociety.net #aegisub!\n");
}

void ssgl_dispose(struct ssav_line *l)
{
	struct ssav_node *n = l->node_first;
	while (n) {
		FT_OutlineGlyph *g = n->glyphs, *end = g + n->nchars;
		if (g) {
			while (g < end)
				FT_Done_Glyph(&g++[0]->root);
			xfree(n->glyphs);
			n->glyphs = NULL;
		}
		n = n->next;
	}
}


