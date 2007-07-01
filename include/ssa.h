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
/** @file ssa.h - SSA definitions */

#ifndef _SSA_H
#define _SSA_H

#include <stdlib.h>
#include "colour.h"

struct ssa_error;			/* defined in asaerror.h */

/** script version.
 * determines behaviour in many cases
 */
enum ssa_version {
	SSAVV_UNDEF = 0,		/**< not detected yet or no idea */
	SSAVV_4 = 1 << 0,
	SSAVV_4P = 1 << 1,
	SSAVV_4PP = 1 << 2,
};

/** ssa_line type.
 * determines which fields of ssa_line are valid
 */
enum ssa_linetype {
	SSAL_DIALOGUE = 1,
	SSAL_COMMENT,
	SSAL_PICTURE,
	SSAL_SOUND,
	SSAL_MOVIE,
	SSAL_COMMAND,
};

/** effect field */
enum ssa_effecttype {
	SSAE_NONE = 0,
	SSAE_KARAOKE,			/**< deprecated */
	SSAE_SCROLLUP,
	SSAE_SCROLLDOWN,
	SSAE_BANNER,
};

#define SSA_DESTCS "UCS-4LE"		/**< output charset.
					 * change this only if using the
					 * lexer standalone!
					 * @see ssaout_t
					 */
typedef char ssasrc_t;			/**< source byte */
typedef unsigned ssaout_t;		/**< output type */

/** SSA string.
 * ok, now for the explanations:
 *  - num of characters? e - s
 *  - length in bytes? (unsigned char *)e - (unsigned char *)s
 *  - nulltermination? always included
 *  - using wchar_t functions? priceless.
 * @var foo
 */
typedef struct ssa_string {
	ssaout_t *s, *e;
} ssa_string;

extern size_t ssa_utf8_len(ssa_string *s);
extern void ssa_utf8_conv(char *out, ssa_string *s);

struct ssa_node;

/** style section entry */
struct ssa_style {
	struct ssa_style *next, *base;

	ssa_string
		name,
		fontname;
	double
		fontsize;
	long int
		fontweight,
		italic,
		underline,
		strikeout;
	double
		scx,
		scy,
		sp,
		rot;		/**< frz */
	colour_t
		cprimary,	/**< text colour */
		csecondary,	/**< karaoke colour */
		coutline,	/**< outline for SSAV_4P,
				 * tertiary for SSAV_4 */
		cback;		/**< shadow for SSAV_4P,
				 * outline & shadow for SSAV_4 */
	long int
		bstyle;
	double
		border,
		shadow;
	long int
		align,
		marginl,
		marginr,
		margint,
		marginb,
		alpha,		/**< SSA doesn't implement this */
		encoding,	/**< line encoding */
		relative;	/**< coordinate base, 0 means screen */
	struct ssa_node
		*node_first,
		**node_last;
	void *vmptr;		/**< (opaque) pointer for VM */
};

/** all supported node types, as represented in the file.
 * - no tag: exists in SSA 4.0
 * - [ASS]:  exists in SSA 4.0+ / Advanced SSA
 * - [eq]:   TextSub 2.23-equinox
 *
 * append your tag if you add more
 *
 * - -: arg is unused
 * - D: arg is simple double
 * - I: arg is simple int
 * - S: arg is simple string
 * - C: arg is simple colour
 * - A: arg is simple alpha value
 * - *: arg is complex
 *
 * parser converts some of these (like a -> an)
 */

enum ssa_nodetype {
	SSAN_TEXT = 1,		/**< 1  - S normal text */
	SSAN_NEWLINE,		/**< 2  - - n */
	SSAN_NEWLINEH,		/**< 3  - - N */
	SSAN_BOLD,		/**< 4  - I b */
	SSAN_ITALICS,		/**< 5  - I i */
	SSAN_UNDERLINE,		/**< 6  - I u [ASS] */
	SSAN_STRIKEOUT,		/**< 7  - I s [ASS] */
	SSAN_BORDER,		/**< 8  - D bord [ASS] */
	SSAN_SHADOW,		/**< 9  - D shad [ASS] */
	SSAN_BLUREDGES,		/**< 10 - I be [ASS] */
	SSAN_FONT,		/**< 11 - S fn */
	SSAN_FONTSIZE,		/**< 12 - D fs */
/* ___ */
	SSAN_FSCX,		/**< 13 - D fscx [ASS] */
	SSAN_FSCY,		/**< 14 - D fscy [ASS] */
	SSAN_FRX,		/**< 15 - D frx [ASS] */
	SSAN_FRY,		/**< 16 - D fry [ASS] */
	SSAN_FRZ,		/**< 17 - D frz [ASS] */
	SSAN_FAX,		/**< 18 - D fax [eq] */
	SSAN_FAY,		/**< 19 - D fay [eq] */
/* ^^^ these have direct translation matrix equivalents */
	SSAN_FSP,		/**< 20 - D fsp [ASS] */
	SSAN_FE,		/**< 21 - I fe [ASS] */
	SSAN_COLOUR,		/**< 22 - C c - 1c in [ASS] */
	SSAN_COLOUR2,		/**< 23 - C 2c [ASS] */
	SSAN_COLOUR3,		/**< 24 - C 3c [ASS] */
	SSAN_COLOUR4,		/**< 25 - C 4c [ASS] */
	SSAN_ALPHA1,		/**< 26 - A 1a [ASS] */
	SSAN_ALPHA2,		/**< 27 - A 2a [ASS] */
	SSAN_ALPHA3,		/**< 28 - A 3a [ASS] */
	SSAN_ALPHA4,		/**< 29 - A 4a [ASS] */
	SSAN_ALPHA,		/**< 30 - A alpha */
	SSAN_ALIGN,		/**< 31 - I a */
	SSAN_ALIGNNUM,		/**< 32 - I an */
	SSAN_KARA,		/**< 33 - I k */
	SSAN_KARAF,		/**< 34 - I K or kf [ASS] */
	SSAN_KARAO,		/**< 35 - I ko [ASS] */
	SSAN_KARAT,		/**< 36 - I kt [ASS] */
	SSAN_WRAP,		/**< 37 - I q [ASS] */
	SSAN_RESET,		/**< 38 - * rSTYLE - STYLE only in [ASS] */

	SSAN_T,			/**< 39 - * t [ASS] */
	SSAN_MOVE,		/**< 40 - * move [ASS] */
	SSAN_POS,		/**< 41 - * pos [ASS] */
	SSAN_ORG,		/**< 42 - * org [ASS] */
	SSAN_FADE,		/**< 43 - * fade [ASS] */
	SSAN_FAD,		/**< 44 - * fad [ASS] */
	SSAN_CLIPRECT,		/**< 45 - * clip(rect) [ASS] */
	SSAN_CLIPDRAW,		/**< 46 - * clip(draw) [ASS] */
	SSAN_PAINT,		/**< 47 - * p [ASS] */
	SSAN_PBO,		/**< 48 - I pbo [ASS] */

	SSAN_MAX,		/**< 49 */
};
#define SSANR		0x80	/**< 00 - empty parameter / revert */
#define SSAN(x)		((x) & ~SSANR)

/** describes which fields are set in a t node */
enum ssa_t_flags {
	SSA_T_HAVEACCEL = 1 << 0,	/**< acceleration was specified */
	SSA_T_HAVETIMES = 1 << 1	/**< times are valid */
};

struct ssa_node {
	struct ssa_node *next;

	enum ssa_nodetype type;

	union {
		ssa_string text;
		long lval;
		double dval;
		struct ssa_style *style;
		colour_t colour;
		alpha_t alpha;
		struct {
			long x, y;
		} pos, org;
		struct {
			long in, out;
		} fad;
		struct {
			alpha_t a1, a2, a3;
			long start1, end1, start2, end2;
		} fade;
		struct {
			long x1, y1, x2, y2;
		} clip;
		struct {
			long x1, y1, x2, y2, start, end;
		} move;
		struct {
			enum ssa_t_flags flags;
			/** animation start/end.
			 * fill with line unless flags has SSA_T_HAVETIMES.
			 */
			long times[2];
			/** animation acceleration.
			 * set to 1.0 if unspecified
			 */
			double accel;
			struct ssa_node *node_first, **node_last;
		} t;
		/* TODO: add stuff for complex overrides */
	} v;
};

/** line rendering switches */
enum ssa_rendflags {
	SSARF_NOCD = 1,			/**< no collision detection */
};

struct ssa_line {
	struct ssa_line *next;
	struct ssa_style *style;

	unsigned no;
	enum ssa_linetype type;

	long int
		ass_layer,
		marginl,
		marginr,
		margint,
		marginb;
	double
		start,		/**< seconds */
		end;
	ssa_string
		ssa_marked,
		name,
		text;		/**< for everything except Dialogue */
	enum ssa_effecttype effect;
	union {
		struct {
			long y1, y2;
			double delay;
			long fadeawayh;
		} scroll;
		struct {
			double delay;
			long ltr, fadeawayw;
		} banner;
	} effp;
	struct ssa_node
		*node_first,	/**< only for Dialogue */
		**node_last;
};

struct ssa {
	struct ssa_error *errlist;
	unsigned maxerrs;
	struct ssa_line *line_first, **line_last;
	struct ssa_style *style_first, **style_last;

	enum ssa_version version;

	long int
		wrapstyle,
		scalebas,
		playdepth;
	double
		playresx,
		playresy,
		timer;
	ssa_string
		collisions,
		title,
		orig_script,
		orig_transl,
		orig_edit,
		orig_timing,
		synch_point,
		script_upd_by,
		upd_details;
/*---*/
/*	enum asa_csp
		csp;
	unsigned long
		scwidth,
		scheight;
	FT_Matrix
		base; */
};

/*extern void *xrealloc(void *ptr, size_t size);
extern void *xmalloc(size_t size); */

extern int ssa_lex(struct ssa *output, const void *data, size_t datasize);
extern void ssa_free(struct ssa *output);
extern const char *ssa_typename(enum ssa_nodetype type);
extern const char *ssa_lexer_version;

#endif /* _SSA_H */
