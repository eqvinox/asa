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
#include "asafont.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H

#ifdef WIN32
#include <windows.h>
#endif

#ifndef WIN32
FcConfig *fontconf;
FcPattern *aux;
#else
HDC screenDC;
#endif
FT_Library asaf_ftlib;

#define NHASH 64
#define HASH(x) ((x) & (NHASH - 1))
static struct asa_font *fonthashes[NHASH];

static void hash_insert(struct asa_font *af)
{
	af->next = fonthashes[HASH(af->hash)];
	fonthashes[HASH(af->hash)] = af;
}

static struct asa_font *hash_get(unsigned hash)
{
	struct asa_font *cur = fonthashes[HASH(hash)];
	while (cur && cur->hash != hash)
		cur = cur->next;
	return cur;
}

static void hash_remove(struct asa_font *af)
{
	struct asa_font **pnext, *cur;
	cur = *(pnext = &fonthashes[HASH(af->hash)]);
	while (cur && cur != af)
		cur = *(pnext = &cur->next);
	if (cur)
		*pnext = cur->next;
}

void asaf_init()
{
	int i;
	for (i = 0; i < NHASH; i++)
		fonthashes[i] = NULL;

#ifndef WIN32
	fontconf = FcInitLoadConfigAndFonts();
	if (!fontconf) {
		fprintf(stderr, "Fontconfig initialization failed\n");
		return;
	}
#else
	screenDC = CreateDCW(L"DISPLAY", NULL, NULL, NULL);
#endif
	if (FT_Init_FreeType(&asaf_ftlib)) {
		fprintf(stderr, "FreeType initialization failed\n");
		return;
	}
#ifndef WIN32
	aux = FcPatternCreate();
#endif
}

void asaf_done()
{
	int i;
	for (i = 0; i < NHASH; i++)
		fonthashes[i] = NULL;

	FT_Done_FreeType(asaf_ftlib);
#ifndef WIN32
	if (aux)
		FcPatternDestroy(aux);
	FcFini();
#else
	DeleteDC(screenDC);
#endif
}

static inline int asaf_lv2weight(long int lv)
{
	return (lv == 0) ? 100
		: (lv == 1 || lv == -1) ? 300
		: lv;
}

struct asa_font *asaf_request(const char *name, int slant, int weight)
{
	struct asa_font *rv;
	FT_Face face;
	void *buffer = NULL;
#ifndef WIN32
	FcPattern *final, *tmp1, *tmp2;
	FcResult res;
	FcChar8 *filename;
	int fontindex;
	unsigned hash;

	tmp1 = FcPatternBuild(NULL,
		FC_FAMILY, FcTypeString, name,
		FC_SLANT, FcTypeInteger, slant,
		FC_WEIGHT, FcTypeInteger, asaf_lv2weight(weight),
//		FC_SIZE, FcTypeDouble, size,
		NULL);
	if (!tmp1) {
		fprintf(stderr, "failed to build pattern for %s %d %d\n",
			name, slant, weight);
		return NULL;
	}
//	FcPatternPrint(rv->pattern);

	tmp2 = FcFontRenderPrepare(fontconf, tmp1, aux);
	FcPatternDestroy(tmp1);
	final = FcFontMatch(fontconf, tmp2, &res);
	FcPatternDestroy(tmp2);
	hash = FcPatternHash(final);

	if ((rv = hash_get(hash))) {
		FcPatternDestroy(final);
		fprintf(stderr, "Cached %08x %s\n", hash, name);
		return rv;
	}

	if (FcPatternGetString(final, FC_FILE, 0, &filename)
			!= FcResultMatch
		|| FcPatternGetInteger(final, FC_INDEX, 0, &fontindex)
			!= FcResultMatch) {
		fprintf(stderr, "error locating font\n");
		FcPatternDestroy(final);
		return NULL;
	}
	
	if (FT_New_Face(asaf_ftlib, (char *)filename, fontindex, &face)) {
		fprintf(stderr, "error opening %s\n", filename);
		FcPatternDestroy(final);
		return NULL;
	} else
		fprintf(stderr, "Loaded %08x %s => \"%s\":%d\n", hash, name, filename, fontindex);
/*	FcPatternGetDouble(af->pattern, FC_SIZE, 0, &size);
	FT_Set_Char_Size(af->face, size * 64, size * 64, 0, 0); */
	FcPatternDestroy(final);
#else
	HFONT font;
	DWORD size;
	unsigned hash = 0, *ptr, *end;

	font = CreateFont(16, 0, 0, 0, asaf_lv2weight(weight) * 2, slant, 0, 0, 0, 0, 0, 0, 0, name);
	if (!font)
		return NULL;

	SelectObject(screenDC, font);
	size = GetFontData(screenDC, 0, 0, NULL, 0);
	if (size == GDI_ERROR || !(buffer = xmalloc(size))) {
		DeleteObject(font);
		return NULL;
	}
	GetFontData(screenDC, 0, 0, buffer, size);

	ptr = (unsigned *)buffer;	/* poor man's hash */
	end = ptr + (size >> 2);
	for (; ptr < end; ptr++)
		hash ^= *ptr;

	if ((rv = hash_get(hash))) {
		xfree(buffer);
		DeleteObject(font);
		return rv;
	}
	if (FT_New_Memory_Face(asaf_ftlib, buffer, size, 0, &face)) {
		DeleteObject(font);
		xfree(buffer);
		return NULL;
	}
	DeleteObject(font);
#endif

	rv = xmalloc(sizeof(struct asa_font));
	rv->hash = hash;
	rv->ref = 1;
	rv->face = face;
	rv->data = buffer;

	hash_insert(rv);

	return rv;
}

void asaf_frelease(struct asa_font *af)
{
	if (--af->ref)
		return;
	hash_remove(af);
	if (af->data)
		xfree(af->data);
	FT_Done_Face(af->face);
	free(af);
}

struct asa_fontinst *asaf_reqsize(struct asa_font *af, double size)
{
	struct asa_fontinst *rv = xmalloc(sizeof(struct asa_fontinst));
	FT_Size_RequestRec req;
	TT_HoriHeader *hori;
	TT_OS2 *os2;
	float scale = 1.0;
#ifdef FONTSIZE_DEBUG
	FT_Size_Metrics *m = &af->face->size->metrics;

	fprintf(stderr, "%08x metric dump:\n", af->hash);
#endif

	asaf_faddref(af);
	rv->ref = 1;
	rv->font = af;

	FT_New_Size(af->face, &rv->size);
	FT_Activate_Size(rv->size);

	os2 = (TT_OS2 *)FT_Get_Sfnt_Table(af->face, ft_sfnt_os2);
	hori = (TT_HoriHeader *)FT_Get_Sfnt_Table(af->face, ft_sfnt_hhea);
	if (os2 && hori) {
		int horisum = hori->Ascender - hori->Descender;
		unsigned winsum = os2->usWinAscent + os2->usWinDescent;
		float mscale = (float)horisum / (float)winsum;
#ifdef FONTSIZE_DEBUG
		int typosum = os2->sTypoAscender - os2->sTypoDescender
			+ os2->sTypoLineGap;

		fprintf(stderr,
			"HHEA values:\n"
			"\tascender %4d descender %4d [linegap %4d] = %d\n"
			"OS2  values:\n"
			" Typo:\tascender %4d descender %4d "
				"linegap %4d = [%d]\n"
			" Win:\tascender %4u descender %4u = %u\n",
			hori->Ascender, hori->Descender,
				hori->Line_Gap, horisum,
			os2->sTypoAscender, os2->sTypoDescender,
				os2->sTypoLineGap, typosum,
			os2->usWinAscent, os2->usWinDescent, winsum);
		fprintf(stderr, "  --->\tscaling by %4.4f\n", mscale);
#endif
		scale = mscale;
	}

	req.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
	req.width = 0;
	req.height = (FT_F26Dot6)(size * 64 * scale);
	req.horiResolution = 0;
	req.vertResolution = 0;
	FT_Request_Size(af->face, &req);

#ifdef FONTSIZE_DEBUG
	fprintf(stderr, "Size_Metrics:\n\tppem\t%dx%d\n\tscale\t%4.2fx%4.2f\n"
		"\tascender %4.2f\n\tdescender %4.2f\n\theight\t%4.2f\n\tmax_advance %4.2f\n",
		m->x_ppem, m->y_ppem,
		m->x_scale / 64., m->y_scale / 64.,
		m->ascender / 64., m->descender / 64.,
		m->height / 64., m->max_advance / 64.);
#endif

	return rv;
}

FT_Face asaf_sactivate(struct asa_fontinst *as)
{
	FT_Activate_Size(as->size);
	return as->font->face;
}

void asaf_srelease(struct asa_fontinst *as)
{
	if (--as->ref)
		return;
	FT_Done_Size(as->size);
	asaf_frelease(as->font);
	free(as);
}

