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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else
#define __LITTLE_ENDIAN	1234
#define __BIG_ENDIAN	4321
#define __PDP_ENDIAN	3412
#define __BYTE_ORDER	__LITTLE_ENDIAN
#endif

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

/** error codes */
enum ssa_errc {
	SSAEC_NOTSUP = 1,		/**< command/option not supported */
	SSAEC_UNKNOWNVER,		/**< unknown script version */
	SSAEC_AMBIGUOUS_SVER,		/**< ambiguous script version */
	SSAEC_INVAL_ENCODING,		/**< invalid input encoding */
	SSAEC_PARSEERROR,		/**< parse error */
	SSAEC_EXC_NUMORSIGN,		/**< expecting number or sign */
	SSAEC_EXC_NUM,			/**< expecting number */
	SSAEC_EXC_COLOUR,		/**< expecting colour code */
	SSAEC_EXC_DCOL,			/**< expected ':' */
	SSAEC_EXC_AMPERSAND,		/**< expected '&' */
	SSAEC_EXC_BRACE,		/**< expected '(' */
	SSAEC_MISS_BRACE,		/**< missing closing brace */
	SSAEC_LEADWS_BRACE,		/**< leading whitespace before '(' */
	SSAEC_NUM_2LONG,		/**< number too long */
	SSAEC_NUM_INVAL,		/**< invalid number */
	SSAEC_TRUNCLINE,		/**< truncated line */
	SSAEC_TRAILGB_PARAM,		/**< trailing garbage in parameter */
	SSAEC_TRAILGB_LINE,		/**< trailing garbage on line */
	SSAEC_TRAILGB_EFFECT,		/**< trailing garbage in effect */
	SSAEC_TRAILGB_BRACE,		/**< trailing garbage in brace */
	SSAEC_TRAILGB_CSVBRACE,		/**< trailing garbage in csv brace */
	SSAEC_TRAILGB_TIME,		/**< trailing garbage after time */
	SSAEC_TRAILGB_ACCEL,		/**< trailing garbage after time */
	SSAEC_TRAILGB_COLOUR,		/**< trailing garbage in colour */
	SSAEC_GB_STYLEOVER,		/**< garbage in style override */
	SSAEC_STYLEOVER_UNRECOG,	/**< unrecognized style override */
	SSAEC_EFFECT_UNRECOG,		/**< unrecognized effect */
	SSAEC_UNKNSTYLE,		/**< unknown style */
	SSAEC_R0,			/**< problematic use of '0' style */
	SSAEC_INVAL_TIME,		/**< invalid time */
	SSAEC_INVAL_ACCEL,		/**< invalid acceleration */
	SSAEC_INVAL_ANI,		/**< invalid animation */
	SSAEC_INVAL_ENC,		/**< invalid encoding value */
	SSAEC_COLOUR_STRANGE,		/**< unusual colour length */
	SSAEC_COLOUR_ALPHALOST,		/**< alpha value lost */
	SSAEC_STRAY_BOM,		/**< stray BOM */

	SSAEC_MAX
};

/** error strings */
struct ssaec_desc {
	int sev;			/**< severity */
	char *sh;			/**< short description */
	char *add;			/**< long (additional description) */
	char *warn;			/**< warning text */
};
extern struct ssaec_desc ssaec[SSAEC_MAX];	/**< error text table */

#define SSA_ENOORIGIN (~0UL)
/** error message */
struct ssa_error {
	struct ssa_error *next;
	unsigned lineno;		/**< one-based line number */
	unsigned column;		/**< zero-based column number */
	ssasrc_t *textline;		/**< line. free() this! */
	unsigned linelen;
	unsigned origin;		/**< zero-based error origin */
	enum ssa_errc errorcode;
};

typedef unsigned char alpha_t;
typedef unsigned char colour_one_t;

/** colour storage */
typedef union {
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		colour_one_t b, g, r;
		alpha_t a;
#elif __BYTE_ORDER == __BIG_ENDIAN
		alpha_t a;
		colour_one_t r, g, b;
#elif __BYTE_ORDER == __PDP_ENDIAN
		colour_one_t r;
		alpha_t a;
		colour_one_t b, g;
#else
#error fix <endian.h>
#endif
	} c;
	unsigned int l;			/**< always is 0xAARRGGBB */
} colour_t;

/** style section entry */
struct ssa_style {
	struct ssa_style *next;

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
	SSAN_ALPHA,		/**< 26 - A alpha - 1a[ASS] */
	SSAN_ALPHA2,		/**< 27 - A 2a [ASS] */
	SSAN_ALPHA3,		/**< 28 - A 3a [ASS] */
	SSAN_ALPHA4,		/**< 29 - A 4a [ASS] */
	SSAN_ALIGN,		/**< 30 - I a */
	SSAN_ALIGNNUM,		/**< 31 - I an */
	SSAN_KARA,		/**< 32 - I k */
	SSAN_KARAF,		/**< 33 - I K or kf [ASS] */
	SSAN_KARAO,		/**< 34 - I ko [ASS] */
	SSAN_KARAT,		/**< 35 - I kt [ASS] */
	SSAN_WRAP,		/**< 36 - I q [ASS] */
	SSAN_RESET,		/**< 37 - * rSTYLE - STYLE only in [ASS] */

	SSAN_T,			/**< 38 - * t [ASS] */
	SSAN_MOVE,		/**< 39 - * move [ASS] */
	SSAN_POS,		/**< 40 - * pos [ASS] */
	SSAN_ORG,		/**< 41 - * org [ASS] */
	SSAN_FADE,		/**< 42 - * fade [ASS] */
	SSAN_FAD,		/**< 43 - * fad [ASS] */
	SSAN_CLIPRECT,		/**< 44 - * clip(rect) [ASS] */
	SSAN_CLIPDRAW,		/**< 45 - * clip(draw) [ASS] */
	SSAN_PAINT,		/**< 46 - * p [ASS] */
	SSAN_PBO,		/**< 47 - I pbo [ASS] */

	SSAN_MAX,		/**< 48 */
};
#define SSANR		0x80	/**< 00 - empty parameter / revert */
#define SSAN(x)		((x) & ~SSANR)

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
			long times[2];
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
	struct ssa_line *line_first, **line_last;
	struct ssa_style *style_first, **style_last;

	int ignoreenc;

	enum ssa_version version;

	long int
		wrapstyle,
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
extern const char *ssa_lexer_version;

#ifdef __cplusplus
}
#endif

#endif /* _SSA_H */
