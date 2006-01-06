#include "common.h"
#include "ssavm.h"

void ssgl_matrix(struct ssav_node *n, FT_Matrix *rv)
{
	FT_Matrix tm;

#if 0
	ssgl_get_basemat(n, &tm);
#else
	tm.xx = 0x10000L;
	tm.xy = 0x00000L;
	tm.yx = 0x00000L;
	tm.yy = 0x10000L;
#endif

	*rv = tm;
}

void ssgl_prep_one(struct ssav_node *n)
{
	FT_Vector pos;
	FT_Matrix mat;
	FT_Face fnt;
	FT_Glyph tmp;
	FT_OutlineGlyph *dst;
	unsigned c;

	pos.x = 0;
	pos.y = 0;
	ssgl_matrix(n, &mat);
	fnt = asaf_sactivate(n->params->fsiz);

	n->glyphs = dst = xmalloc(sizeof(FT_OutlineGlyph) * n->nchars);

	for (c = 0; c < n->nchars; c++) {
		FT_Load_Glyph(fnt, n->indici[c], FT_LOAD_DEFAULT);
		if (fnt->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
			dst[c] = NULL;
			fprintf(stderr, "glyph format %d != FT_GLYPH_FORMAT_OUTLINE (%d)\n",
			fnt->glyph->format, FT_GLYPH_FORMAT_OUTLINE);
		} else {
			FT_Get_Glyph(fnt->glyph, &tmp); 
			FT_Glyph_Transform(tmp, &mat, &pos);
			dst[c] = (FT_OutlineGlyph)tmp;
		}
	}
}

#define ssgl_(name,op) \
void ssgl_##name(struct ssav_line *l) \
{ \
	struct ssav_node *n = l->node_first; \
	while (n) { \
		FT_OutlineGlyph *g = n->glyphs, *end = g + n->nchars; \
		if (g) { \
			while (g < end) \
				FT_Done_Glyph(&g++[0]->root); \
			xfree(n->glyphs); \
			n->glyphs = NULL; \
		} \
		op \
		n = n->next; \
	} \
}

ssgl_(prepare, ssgl_prep_one(n);)
ssgl_(dispose, )

