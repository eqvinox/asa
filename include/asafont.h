/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

FT_Library asaf_ftlib;

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

struct asa_font *asaf_request(const char *name, int slant, int weight);
static inline void asaf_faddref(struct asa_font *af)
{	af->ref++;	}
void asaf_frelease(struct asa_font *af);

struct asa_fontinst *asaf_reqsize(struct asa_font *af, double size);
static inline void asaf_saddref(struct asa_fontinst *as)
{	as->ref++;	}
void asaf_srelease(struct asa_fontinst *as);

FT_Face asaf_sactivate(struct asa_fontinst *af);

void asaf_init();
void asaf_done();

#endif /* _ASAFONT_H */
