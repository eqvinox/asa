/* #include(LICENSE) */
#ifndef _ASAFONT_H
#define _ASAFONT_H

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include "avl.h"

FT_Library asaf_ftlib;

struct asa_font {
	struct avl_head avl;

	unsigned ref;
	FT_Face face;
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

#endif /* _ASAFONT_H */
