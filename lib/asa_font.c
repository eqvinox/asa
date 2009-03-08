/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
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
#include "asafont.h"
#include "subhelp.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H

#ifndef NO_FONTCONFIG
#if defined(_WIN32) && !defined(__GNUC__)
#pragma comment(lib, "freetype")
#pragma comment(lib, "libfontconfig")
#pragma comment(lib, "libexpat")
#endif
static FcConfig *fontconf;
static FcPattern *aux;
#else

#error this code is unmaintained and needs to be updated.

#ifndef _WIN32
#error fontconfig can be disabled only on win32
#endif
#include <windows.h>
#pragma comment(lib, "freetype")
#pragma comment(lib, "gdi32")
static HDC screenDC;
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

#ifndef NO_FONTCONFIG
	fontconf = FcInitLoadConfigAndFonts();
	if (!fontconf) {
		subhelp_log(CSRI_LOG_ERROR,
			"Fontconfig initialization failed");
		return;
	}
#else
	screenDC = CreateDCW(L"DISPLAY", NULL, NULL, NULL);
#endif
	if (FT_Init_FreeType(&asaf_ftlib)) {
		subhelp_log(CSRI_LOG_ERROR,
			"FreeType initialization failed");
		return;
	}
#ifndef NO_FONTCONFIG
	aux = FcPatternCreate();
#endif
}

void asaf_done()
{
	int i;
	for (i = 0; i < NHASH; i++)
		fonthashes[i] = NULL;

	FT_Done_FreeType(asaf_ftlib);
#ifndef NO_FONTCONFIG
	if (aux)
		FcPatternDestroy(aux);
#ifdef HAVE_FCFINI
	FcFini();
#endif
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
#ifndef NO_FONTCONFIG
	FcPattern *final, *tmp1, *tmp2;
	FcResult res;
	FcChar8 *filename;
	int fontindex;
	unsigned hash;

	tmp1 = FcPatternBuild(NULL,
		FC_FAMILY, FcTypeString, name,
		FC_SLANT, FcTypeInteger, slant,
		FC_WEIGHT, FcTypeInteger, asaf_lv2weight(weight),
		NULL);
	if (!tmp1) {
		subhelp_log(CSRI_LOG_WARNING,
			"failed to build fontconfig pattern for %s "
			"(weight %d, slant %d)", name, weight, slant);
		return NULL;
	}

	tmp2 = FcFontRenderPrepare(fontconf, tmp1, aux);
	FcPatternDestroy(tmp1);
	final = FcFontMatch(fontconf, tmp2, &res);
	FcPatternDestroy(tmp2);
	if (!final)
		return NULL;

	hash = FcPatternHash(final);

	if ((rv = hash_get(hash))) {
		FcPatternDestroy(final);
		return rv;
	}

	if (FcPatternGetString(final, FC_FILE, 0, &filename)
			!= FcResultMatch
		|| FcPatternGetInteger(final, FC_INDEX, 0, &fontindex)
			!= FcResultMatch) {
		subhelp_log(CSRI_LOG_WARNING,
			"error locating font %s (no filename/index)", name);
		FcPatternDestroy(final);
		return NULL;
	}

	if (FT_New_Face(asaf_ftlib, (char *)filename, fontindex, &face)) {
		subhelp_log(CSRI_LOG_WARNING,
			"error loading font %s (\"%s\")", name, filename);
		FcPatternDestroy(final);
		return NULL;
	} else
		subhelp_log(CSRI_LOG_INFO,
			"loaded font %s from \"%s\":%d", name, filename,
			fontindex);
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

	rv = xnew(struct asa_font);
	if (!rv) {
		FT_Done_Face(face);
		if (buffer)
			xfree(buffer);
		oom_msg();
		return NULL;
	}
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
	FT_Done_Face(af->face);
	if (af->data)
		xfree(af->data);
	xfree(af);
}

struct asa_fontinst *asaf_reqsize(struct asa_font *af, double size)
{
	struct asa_fontinst *rv = xnew(struct asa_fontinst);
	FT_Size_RequestRec req;
	TT_HoriHeader *hori;
	TT_OS2 *os2;
	float scale = 1.0;

	oom_return(!rv);

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
	xfree(as);
}

