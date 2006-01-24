#include "common.h"
#include "ssavm.h"

#include <freetype/fttrigon.h>
#include <assert.h>

void ssgl_matrix(struct ssav_node *n, FT_Matrix *rv)
{

	FT_Matrix out, tmp;

	out.xx = 0x10000L;
	out.xy = 0x00000L;
	out.yx = 0x00000L;
	out.yy = 0x10000L;

	if (n->params->m.fscx != 100.)
		out.xx = FT_MulFix(out.xx, (int)(n->params->m.fscx * 655.36));
	if (n->params->m.fscy != 100.)
		out.yy = FT_MulFix(out.yy, (int)(n->params->m.fscy * 655.36));
	/* frx & fry? */
	if (n->params->m.frz != 0.0) {
		FT_Angle angle = (int)(n->params->m.frz * 65536);
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
	FT_Glyph tmp;
	FT_OutlineGlyph *g, *end;
	unsigned idx, stop, *src;

	struct ssav_node *n = l->node_first;
	struct ssav_unit *u = l->unit_first;

	if (!u || !n)
		return;

	u->height = 0;
	idx = 0;
	stop = u->next ? u->next->idxstart : l->nchars;
	pos.x = pos.y = 0;
	while (u && n) {
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

		fnt = asaf_sactivate(n->params->fsiz);
		ssgl_matrix(n, &mat);
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
				FT_Get_Glyph(fnt->glyph, &tmp); 
				FT_Glyph_Transform(tmp, &mat, &pos);
				*g = tmp;
				pos.x += tmp->advance.x >> 10;
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


