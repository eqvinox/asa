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
#include "ssavm.h"
#include "subhelp.h"

#include <freetype/fttrigon.h>
#include <assert.h>
#include <math.h>

#if 0
static void ssgl_rotmat(FT_Matrix *m, double dangle)
{
	FT_Angle angle = (int)(dangle * 65536);
	m->xx = FT_Cos(angle);
	m->xy = FT_Sin(angle);
	m->yx = -FT_Sin(angle);
	m->yy = FT_Cos(angle);
}
#endif

void ssgl_matrix(struct ssa_vm *vm, struct ssav_params *p, FT_Matrix *fx0)
{
	fx0->xx = (int)(p->m.fscx * vm->fscale.xx * 0.01);
	fx0->xy = 0x00000L;
	fx0->yx = 0x00000L;
	fx0->yy = (int)(-p->m.fscy * vm->fscale.yy * 0.01);
}


/* Return identity matrix
   By ArchMage ZeratuL */
void ssgl_identity_matrix3d(float *matrix) {
	int i=0;
	for (i=0;i<16;i++) {
		if ((i % 5) == 0) matrix[i]=1.0f;
		else matrix[i]=0.0f;
	}
}

/* Multiply 3d matrices
   Returns result on matrix a
   Code by ArchMage ZeratuL */
void ssgl_multiply_matrix3d(float *_a,float *_b) {
	float *a,*b,*dst;
	float t[16];
	int i,x,y;
	a = _a;
	b = _b;
	dst = _a;
	memcpy(t,a,sizeof(float)*16);
	for (i=0;i<16;i++) {
		x = i/4;
		y = i-x*4;
		x *= 4;
		dst[i] = t[y]*b[x] + t[y+4]*b[x+1] + t[y+8]*b[x+2] + t[y+12]*b[x+3];
	}
}

/* Generate 3D matrix
   Code by ArchMage ZeratuL */
void ssgl_matrix3d(struct ssav_params *p, float *m) {
	float sax,cax,say,cay,saz,caz;
	float t = 0.017453292519943f;
	float mat[16];
	float matrix2[16] = { 2500, 0, 0,    0,
	                      0, 2500, 0,    0,
	                      0,    0, 1,    1,
	                      0,    0, 2500, 2500 };

	// Load identity
	ssgl_identity_matrix3d(m);

	// Perpsective matrix
	ssgl_multiply_matrix3d(m,matrix2);

	// frx & fry matrix
	if (p->m.frx != 0.0 || p->m.fry != 0.0) {
		sax = (float)sin(p->m.frx * t);
		say = (float)sin(p->m.fry * t);
		cax = (float)cos(p->m.frx * t);
		cay = (float)cos(p->m.fry * t);
		mat[0] = cay;      mat[4] = sax*say;       mat[8]  = -cax*say;     mat[12] = 0.0f;
		mat[1] = 0.0f;     mat[5] = cax;           mat[9]  = sax;          mat[13] = 0.0f;
		mat[2] = say*8.0f; mat[6] = -sax*cay*8.0f; mat[10] = cax*cay*8.0f; mat[14] = 0.0f;
		mat[3] = 0.0f;     mat[7] = 0.0f;          mat[11] = 0.0f;         mat[15] = 1.0f;
		ssgl_multiply_matrix3d(m,mat);
	}

	// frz matrix
	if (p->m.frz != 0.0) {
		saz = (float)sin(p->m.frz * t);
		caz = (float)cos(p->m.frz * t);
		ssgl_identity_matrix3d(mat);
		mat[0] = caz;  mat[4] = saz;
		mat[1] = -saz; mat[5] = caz;
		ssgl_multiply_matrix3d(m,mat);
	}

	// \fax / \fay matrix
	if (p->m.fax != 0.0 || p->m.fay != 0.0) {
		ssgl_identity_matrix3d(mat);
		mat[4] = (float)p->m.fax;
		mat[1] = (float)p->m.fay;
		ssgl_multiply_matrix3d(m,mat);
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
		g = n->glyphs = (FT_OutlineGlyph *)xmalloc(
			sizeof(FT_OutlineGlyph)	* n->nchars);
}

static void ssgl_load_glyph(FT_Face fnt, unsigned idx,
	FT_Vector *pos, FT_Matrix *mat, double fsp, FT_OutlineGlyph *dst)
{
	FT_Glyph tmp;
	FT_Vector tmppos;

	FT_Load_Glyph(fnt, idx, FT_LOAD_NO_HINTING);

	*dst = NULL;
	if (fnt->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
		subhelp_log(CSRI_LOG_ERROR, "%s: non-vector glyph format %x",
			fnt->family_name, fnt->glyph->format);
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

void ssgl_prepare(struct ssa_vm *vm, struct ssav_line *l)
{
	FT_Vector pos, hv;
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
	pos.x = pos.y = 0;
	hv.x = 0;
	while (u && n) {
		p = n->params;
		if (p->finalized)
			p = p->finalized;

		fnt = asaf_sactivate(n->params->fsiz);
		ssgl_matrix(vm, p, &mat);

		/* generate 3d matrix */
		ssgl_matrix3d(p, n->matrix3d);

		hv.y = (FT_Pos)(n->params->f.size * 64);
		FT_Vector_Transform(&hv, &mat);
		//hv.y = (FT_Pos)(n->params->f.size * 64 * p->m.fscy / 100.0);

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
	subhelp_log(CSRI_LOG_ERROR, "mismatching fragmentization - internal "
		"asa error. Please file a bug report.");
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


