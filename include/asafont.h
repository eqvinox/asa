/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

#ifndef _ASAFONT_H
#define _ASAFONT_H

#ifndef NO_FONTCONFIG
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SIZES_H

extern FT_Library asaf_ftlib;

struct asa_font {
	struct asa_font *next;
	unsigned hash;

	unsigned ref;
	FT_Face face;
	void *data;
};

struct asa_fontinst {
	unsigned ref;
	struct asa_font *font;
	FT_Size size;
};

extern struct asa_font *asaf_request(const char *name, int slant, int weight);
static inline void asaf_faddref(struct asa_font *af)
{	af->ref++;	}
extern void asaf_frelease(struct asa_font *af);

extern struct asa_fontinst *asaf_reqsize(struct asa_font *af, double size);
static inline void asaf_saddref(struct asa_fontinst *as)
{	as->ref++;	}
extern void asaf_srelease(struct asa_fontinst *as);

extern FT_Face asaf_sactivate(struct asa_fontinst *af);

extern void asaf_init();
extern void asaf_done();

#endif /* _ASAFONT_H */
