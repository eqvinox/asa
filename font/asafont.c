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

static struct avl_head *fontroot = NULL;

void asaf_init()
{
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
	struct avl_head *pcache;
#ifndef WIN32
	FcPattern *final, *tmp1, *tmp2;
	FcResult res;
	FcChar8 *filename;
	int fontindex;
	unsigned hash;
	struct avl_head *cent;

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

	if ((cent = avl_find_pcache(fontroot, hash, &pcache))) {
		FcPatternDestroy(final);
		fprintf(stderr, "Cached %08x %s\n", hash, name);
		return (struct asa_font *)cent;
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
	void *buffer;
	static unsigned hash = 0;

	font = CreateFont(16, 0, 0, 0, asaf_lv2weight(weight) * 2, slant, 0, 0, 0, 0, 0, 0, 0, name);
	SelectObject(screenDC, font);
	size = GetFontData(screenDC, 0, 0, NULL, 0);
	if (size == GDI_ERROR)
		return NULL;
	buffer = xmalloc(size);
	if (!buffer)
		return NULL;
	GetFontData(screenDC, 0, 0, buffer, size);
	if (FT_New_Memory_Face(asaf_ftlib, buffer, size, 0, &face))
		return NULL;
	hash++;
	avl_find_pcache(fontroot, hash, &pcache);
#endif

	rv = xmalloc(sizeof(struct asa_font));
	memset(&rv->avl, 0, sizeof(rv->avl));
	rv->avl.value = hash;
	rv->ref = 1;
	rv->face = face;

	avl_insert_pcache(&fontroot, &rv->avl, pcache);

	return rv;
}

void asaf_frelease(struct asa_font *af)
{
	if (--af->ref)
		return;
	avl_delete(&fontroot, &af->avl);
	FT_Done_Face(af->face);
	free(af);
}

struct asa_fontinst *asaf_reqsize(struct asa_font *af, double size)
{
	struct asa_fontinst *rv = xmalloc(sizeof(struct asa_fontinst));
	asaf_faddref(af);
	rv->ref = 1;
	rv->font = af;

	FT_New_Size(af->face, &rv->size);
	FT_Activate_Size(rv->size);
	FT_Set_Char_Size(af->face, (FT_F26Dot6)(size * 64),
			(FT_F26Dot6)(size * 64), 0, 0);

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

