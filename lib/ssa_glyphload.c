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
#include "ssavm.h"

#include <freetype/fttrigon.h>
#include <assert.h>

void ssgl_matrix(struct ssav_params *p, FT_Matrix *rv)
{
	FT_Matrix out, tmp;

	out.xx = 0x10000L;
	out.xy = 0x00000L;
	out.yx = 0x00000L;
	out.yy = -0x10000L;

	if (p->m.fscx != 100.)
		out.xx = FT_MulFix(out.xx, (int)(p->m.fscx * 655.36));
	if (p->m.fscy != 100.)
		out.yy = FT_MulFix(out.yy, (int)(p->m.fscy * 655.36));
	/* frx & fry? */
	if (p->m.frz != 0.0) {
		FT_Angle angle = (int)(p->m.frz * 65536);
		tmp.xx = FT_Cos(angle);
		tmp.xy = FT_Sin(angle);
		tmp.yx = -FT_Sin(angle);
		tmp.yy = FT_Cos(angle);
		FT_Matrix_Multiply(&tmp, &out);
	}

	*rv = out;
}

void ssgl_prepare(struct ssav_line *l)
{
	FT_Vector pos;
	FT_Matrix mat;
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
	stop = u->next ? u->next->idxstart : l->nchars;
	pos.x = pos.y = 0;
	while (u && n) {
		p = n->params;
		if (p->finalized)
			p = p->finalized;
		if (n->glyphs) {
			g = n->glyphs;
			end = g + n->nchars;
			while (g < end) {
				if (*g)
					FT_Done_Glyph(&g[0]->root);
				g++;
			}
		} else {
			g = n->glyphs = xmalloc(sizeof(FT_OutlineGlyph)
				* n->nchars);
			end = g + n->nchars;
		}

		fnt = asaf_sactivate(n->params->fsiz);	/* XXX: animation */
		ssgl_matrix(p, &mat);
		src = n->indici;

		if (idx != stop && fnt->size->metrics.height > u->height)
			u->height = fnt->size->metrics.height;
		
		while (g < end) {
			if (idx == stop) {
				u->size = pos;
				u = u->next;
				assert(u);
				stop = u->next ? u->next->idxstart
					: l->nchars;
				pos.x = pos.y = 0;
				u->height = fnt->size->metrics.height;
			}

			FT_Load_Glyph(fnt, *src++, FT_LOAD_DEFAULT);
			if (fnt->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
				*g = NULL;
				fprintf(stderr,
					"non-outline glyph format %d\n",
					fnt->glyph->format);
			} else {
				FT_Glyph tmp;
				FT_Vector tmppos = pos;
				FT_Get_Glyph(fnt->glyph, &tmp);
				tmppos.y += fnt->size->metrics.descender;
				FT_Glyph_Transform(tmp, &mat, &tmppos);
				*g = (FT_OutlineGlyph)tmp;
				pos.x += (tmp->advance.x >> 10) +
					(int)(p->m.fsp * 64);
				pos.y += tmp->advance.y >> 10;
			}
			g++, idx++;
		}

		n = n->next;
	}
	u->size = pos;
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


