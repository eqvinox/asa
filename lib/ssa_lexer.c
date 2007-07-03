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

/** @file ssa_parse.c - SSA parser.
 *
 * 2004 04 09 - 0.1
 *  parses most SSA cruft except embedded fonts or graphics
 *
 * Notes:
 *  - all stuff in here is \0 safe, end pointer & memcpy used everywhere
 *  -        - " -      uses wchar_t (well, someone check toupper())
 *
 * (\0-safeness actually isn't worth a bit because we return wchar_t *
 *  without length ... *sigh* ...)
 *
 *   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   * this code is pure *magic* - THINK before even changing one bit! *
 *   * (almost) all functions are called from several distinct places! *
 *   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */

#include "common.h"
#include "ssa.h"
#include "asaerror.h"
#include "subhelp.h"

#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <iconv.h>
#include <errno.h>

#ifdef _MSC_VER
#pragma warning( disable : 4047)
#pragma warning( disable : 4100)
#pragma warning( disable : 4706)
#endif

#define ssa_isspace(x)	isspace((unsigned char)x)	/**< @see ssasrc_t */
#define ssa_isalpha(x)	isalpha((unsigned char)x)	/**< @see ssasrc_t */
#define ssa_isdigit(x)	isdigit((unsigned char)x)	/**< @see ssasrc_t */
#define ssa_isxdigit(x)	isxdigit((unsigned char)x)	/**< @see ssasrc_t */
#define ssa_toupper(x)	toupper((unsigned char)x)	/**< @see ssasrc_t */

#define ssatod		strtod		/**< @see ssasrc_t */
#define ssatol		strtol		/**< @see ssasrc_t */ 
#define ssatoul		strtoul		/**< @see ssasrc_t */ 

struct ssa_parselist;

/** SSA parsing context.
 * to warn about misplaced lines (and to somewhen support embedding)
 */
enum ssa_context {
	SSACTX_INIT = 1 << 0,		/**< nothing read yet */
	SSACTX_RAW = 1 << 1,		/**< no header, accept all */
	SSACTX_INFO = 1 << 2,		/**< script info */
	SSACTX_STYLE = 1 << 3,		/**< style */
	SSACTX_EVENTS = 1 << 4,		/**< events */
	SSACTX_PACKET = 1 << 5,		/**< MKV streaming */
};

/** lexer state / instance.
 * deallocated after lexing complete
 */
struct ssa_state {
	struct ssa *output;		/**< result */
	struct ssa_line *sline;		/**< current dialogue line */
	const ssasrc_t
		*line, *end,
		*param, *pend,
		*anisource;		/**< start of t block.
					 * only used for producing
					 * better error messages
					 */
	unsigned lineno;		/**< line number, 1 based */
	enum ssa_context ctx;		/**< current context */
	struct ssa_parselist *ctx_pl;	/**< parselist for context */
	
	unsigned unicode;		/**< is file unicode?.
					 * disables \fe */
	const char *ic_srccs;		/**< internal charset.
					 * everything typed ssasrc_t is
					 * in the charset specified by this.
					 * - always a single-byte charset.
					 * - only UTF-8 and MS-ANSI currently
					 *   used.
					 */
	iconv_t ic_srcout;		/**< iconv handle 
					 * ic_srccs -> SSA_DESTCS.
					 * @see SSA_DESTCS. */
	long int ic_tcv_enc;		/**< current text fe setting */
	iconv_t ic_tcv;			/**< iconv fe -> SSA_DESTCS.
					 * @see SSA_DESTCS */
	struct ssa_error **elast;	/**< last error added */
	unsigned nerr;			/**< number of errors found */
};

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#undef inline
#else
#define inline 
#endif

/** parameter to ssa_parsefunc.
 * stored in key lists, passed to parsefuncs
 * @see ssa_parsefunc
 */
typedef union {
	long lparam;
	ptrdiff_t offset;
	struct ssa_parselist *parselist;
} par_t;

/** ssa sub-parser function.
 * @param state parser state (everything valid).
 *   - state->param must be moved to first unparsed character
 * @param param parameters from key list (type depends on func)
 * @param elem "current" element (for param.offset)
 * @return nonzero to enable whitespace/completeness check.
 *   - if state->param is whitespace: syntax error
 *   - if state->param != state->pend: trailing garbage
 */
typedef unsigned ssa_parsefunc(struct ssa_state *state, par_t param,
	void *elem);

/** key list entry.
 * used to form lookup tables for parsing functions
 */
struct ssa_parsetext_ctx {
	const ssasrc_t *text;		/**< key. NULL = end of list */
	ssa_parsefunc *func;		/**< responsible parsing function */
	par_t param;			/**< 2nd parameter to func */
	enum ssa_context ctx;		/**< context restriction */
};

/** key list entry, tagged. */
struct ssa_parsetext_tag {
	const ssasrc_t *text;		/**< key. NULL = end of list */
	ssa_parsefunc *func;		/**< responsible parsing function */
	par_t param;			/**< 2nd parameter to func */
	unsigned type;			/**< tag to store result under */
};

/* foo X_X */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define pl(x)	{.parselist = x}
#else
#define pl(x)	{(struct ssa_parselist *) x}	/**< par_t from parselist */
#endif

/* func = NULL && param = 0 marks end [use NULL, 1 for skipping stuff]
 * ssa_version for version specific stuff (default SSAV_UNDEF)
 *             can be |'ed with SSAP_OPTIONAL for "omission point"
 *              (see \move for example)
 */
#define SSAP_OPTIONAL		0x8000
/* don't include ssa_parselist_fmt entry into default parselist.
 * (use for alternative field names)
 */
#define SSAP_NODEF		0x4000

#define SSAP_ALL		(SSAP_OPTIONAL | SSAP_NODEF)

/** all versions */
#define SSAV_UNDEF		SSAVV_4 | SSAVV_4P | SSAVV_4PP
#define SSAV_4P			SSAVV_4P | SSAVV_4PP	/**< plus versions */
#define SSAV_4			SSAVV_4			/**< original SSAv4 */
#define SSAV_4PP		SSAVV_4PP		/**< ASS2 / ++ */
#define SSAV_4nPP		SSAVV_4 | SSAVV_4P	/**< all except ++ */

struct ssa_parselist {
	ssa_parsefunc *func;
	par_t param;
	long flags;
};

struct ssa_parselist_fmt {
	ssa_parsefunc *func;
	par_t param;
	long ssa_version;
	const ssasrc_t *column;
};

static unsigned ssa_genstr(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_genint(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_gencrd(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_twocrd(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_genfp (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_genbool(struct ssa_state *state, par_t param, void *elem);

static unsigned ssa_setctx(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_setver(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_notsup(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_style (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_styleex(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_std   (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_fmt   (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_sstyle(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_time  (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_colour(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_alpha (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_colalp(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_brace (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_move  (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_t     (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_effect(struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_clip  (struct ssa_state *state, par_t param, void *elem);
static unsigned ssa_fe    (struct ssa_state *state, par_t param, void *elem);


#define fmts	(SSACTX_STYLE | SSACTX_EVENTS)
#define dlg	(SSACTX_RAW | SSACTX_EVENTS | SSACTX_PACKET)
#define evnt	(SSACTX_RAW | SSACTX_EVENTS)
#define styl	(SSACTX_RAW | SSACTX_STYLE)
#define info	(SSACTX_RAW | SSACTX_INFO)
#define init	(SSACTX_INIT)
#define any	(~0UL)

/** index of ptkeys entry to use for SSACTX_PACKET */
#define PTKEYS_PACKET	0

#define apply_offset(x,y) (((char *)x) + y)		/**< apply e(x) */
#define e(x) ((ptrdiff_t) &((struct ssa *)0)->x)	/**< store offset */
/** global keyword table */
static struct ssa_parsetext_ctx ptkeys[] = {
	/* content lines - first because used the most */
	{"dialogue:",		ssa_std,	{SSAL_DIALOGUE},	dlg},
	{"style:",		ssa_style,	{0},			styl},
	{"styleex:",		ssa_styleex,	{0},			styl},
	{"comment:",		ssa_std,	{SSAL_COMMENT},		evnt},
	{"picture:",		ssa_std,	{SSAL_PICTURE},		evnt},
	{"sound:",		ssa_std,	{SSAL_SOUND},		evnt},
	{"movie:",		ssa_std,	{SSAL_MOVIE},		evnt},
	{"command:",		ssa_std,	{SSAL_COMMAND},		evnt},
	{"format:",		ssa_fmt,	{0},			fmts},
	/* sections */
	{"[script info]",	ssa_setctx,	{SSACTX_INFO},		init},
	{"[v4 styles]",		ssa_setver,	{SSAVV_4},		any},
	{"[v4+ styles]",	ssa_setver,	{SSAVV_4P},		any},
	{"[v4++ styles]",	ssa_setver,	{SSAVV_4PP},		any},
	{"[events]",		ssa_setctx,	{SSACTX_EVENTS},	any},
	{"[fonts]",		ssa_notsup,	{0},			any},
	{"[graphics]",		ssa_notsup,	{0},			any},
	/* info (those no one cares about) */
	{"title:",		ssa_genstr,	{e(title)},		info},
	{"original script:",	ssa_genstr,	{e(orig_script)},	info},
	{"original script checking:", ssa_genstr, {e(orig_script)},	info},
	{"original translation:", ssa_genstr,	{e(orig_transl)},	info},
	{"original editing:",	ssa_genstr,	{e(orig_edit)},		info},
	{"original timing:",	ssa_genstr,	{e(orig_timing)},	info},
	{"synch point:",	ssa_genstr,	{e(synch_point)},	info},
	{"script updated by:",	ssa_genstr,	{e(script_upd_by)},	info},
	{"update details:",	ssa_genstr,	{e(upd_details)},	info},
	/* info (the important ones) */
	{"scripttype:",		ssa_setver,	{0},			info},
	{"collisions:",		ssa_genstr,	{e(collisions)},	info},
	{"playresy:",		ssa_genfp,	{e(playresy)},		info},
	{"playresx:",		ssa_genfp,	{e(playresx)},		info},
	{"playdepth:",		ssa_genint,	{e(playdepth)},		info},
	{"timer:",		ssa_genfp,	{e(timer)},		info},
	/* actually WrapStyle only exists in ASS but we aren't picky */
	{"wrapstyle:",		ssa_genint,	{e(wrapstyle)},		info},
	{"scaledborderandshadow:", ssa_genbool,	{e(scalebas)},		info},
	{"lastwav:",		NULL,		{0},			info},
	{"wav:",		NULL,		{0},			info},
	{NULL,			NULL,		{0},			0}
};
#undef e

#undef fmts
#undef evnt
#undef styl
#undef info
#undef init
#undef any

#define e(x) ((ptrdiff_t) &((struct ssa_line *)0)->x)
/** CSV for Dialogue / etc.
 * we could build this from the Format: line, but some scripts are missing
 * Format: lines entirely...
 */
static struct ssa_parselist_fmt plcommon[] = {
	{ssa_genint,	{e(ass_layer)},		SSAV_4P,	"layer"},
	{ssa_genstr,	{e(ssa_marked)},	SSAV_4,		"marked"},
	{ssa_time,	{e(start)},		SSAV_UNDEF,	"start"},
	{ssa_time,	{e(end)},		SSAV_UNDEF,	"end"},
	{ssa_sstyle,	{e(style)},		SSAV_UNDEF,	"style"},
	{ssa_genstr,	{e(name)},		SSAV_UNDEF,	"name"},
	{ssa_gencrd,	{e(marginl)},		SSAV_UNDEF,	"marginl"},
	{ssa_gencrd,	{e(marginr)},		SSAV_UNDEF,	"marginr"},
	{ssa_twocrd,	{e(margint)},		SSAV_4nPP,	"marginv"},
	{ssa_gencrd,	{e(margint)},		SSAV_4PP,	"margint"},
	{ssa_gencrd,	{e(marginb)},		SSAV_4PP,	"marginb"},
	{ssa_effect,	{0},			SSAV_UNDEF,	"effect"},
	/* text isn't parsed by this (commas in it ;) */
	{NULL,		{0},			0,		NULL}
};

/** CSV for MKV-style streaming */
static struct ssa_parselist plpacket[] = {
	{ssa_genint,	{e(read_order)},	0},
	{ssa_genint,	{e(ass_layer)},		0},
	{ssa_sstyle,	{e(style)},		0},
	{ssa_genstr,	{e(name)},		0},
	{ssa_gencrd,	{e(marginl)},		0},
	{ssa_gencrd,	{e(marginr)},		0},
	{ssa_twocrd,	{e(margint)},		0},
	{ssa_effect,	{0},			0},
	{NULL,		{0},			0}
};
#undef e
#define e(x) ((ptrdiff_t) &((struct ssa_style *)0)->x)
#define ND SSAP_NODEF |
/** CSV for Style */
static struct ssa_parselist_fmt plstyle[] = {
	{ssa_genstr,	{e(name)},		SSAV_UNDEF,	"name"},
	{ssa_genstr,	{e(fontname)},		SSAV_UNDEF,	"fontname"},
	{ssa_genfp,	{e(fontsize)},		SSAV_UNDEF,	"fontsize"},
	{ssa_colour,	{e(cprimary)},		SSAV_4,  "primarycolour"},
	{ssa_colour,	{e(csecondary)},	SSAV_4,  "secondarycolour"},
	{ssa_colour,	{e(coutline)},		SSAV_4,  "tertiarycolour"},
	{ssa_colour,	{e(coutline)},	     ND	SSAV_4,  "outlinecolour"},
	{ssa_colour,	{e(cback)},		SSAV_4,  "backcolour"},
	{ssa_colalp,	{e(cprimary)},		SSAV_4P, "primarycolour"},
	{ssa_colalp,	{e(csecondary)},	SSAV_4P, "secondarycolour"},
	{ssa_colalp,	{e(coutline)},		SSAV_4P, "outlinecolour"},
	{ssa_colalp,	{e(cback)},		SSAV_4P, "backcolour"},
	{ssa_genint,	{e(fontweight)},	SSAV_UNDEF,	"bold"},
	{ssa_genint,	{e(italic)},		SSAV_UNDEF,	"italic"},
	{ssa_genint,	{e(underline)},		SSAV_4P,	"underline"},
	{ssa_genint,	{e(strikeout)},		SSAV_4P,	"strikeout"},
	{ssa_genfp,	{e(scx)},		SSAV_4P,	"scalex"},
	{ssa_genfp,	{e(scy)},		SSAV_4P,	"scaley"},
	{ssa_genfp,	{e(sp)},		SSAV_4P,	"spacing"},
	{ssa_genfp,	{e(rot)},		SSAV_4P,	"angle"},
	{ssa_genint,	{e(bstyle)},		SSAV_UNDEF, "borderstyle"},
	{ssa_genfp,	{e(border)},		SSAV_UNDEF,	"outline"},
	{ssa_genfp,	{e(shadow)},		SSAV_UNDEF,	"shadow"},
	{ssa_genint,	{e(align)},		SSAV_UNDEF,	"alignment"},
	{ssa_gencrd,	{e(marginl)},		SSAV_UNDEF,	"marginl"},
	{ssa_gencrd,	{e(marginr)},		SSAV_UNDEF,	"marginr"},
	{ssa_twocrd,	{e(margint)},		SSAV_4nPP,	"marginv"},
	{ssa_gencrd,	{e(margint)},		SSAV_4PP,	"margint"},
	{ssa_gencrd,	{e(marginb)},		SSAV_4PP,	"marginb"},
	{ssa_genint,	{e(alpha)},		SSAV_4,		"alphalevel"},
	{ssa_genint,	{e(encoding)},		SSAV_UNDEF,	"encoding"},
	{ssa_genint,	{e(relative)},		SSAV_4PP,	"relativeto"},
	{NULL,		{0},			0,		NULL}
};
#undef ND
#undef e
#define e(x) ((ptrdiff_t) &((struct ssa_node *)0)->v.x)
static struct ssa_parselist plpos[] = {
	{ssa_gencrd,		{e(pos.x)},		0},
	{ssa_gencrd,		{e(pos.y)},		0},
	{NULL,			{0},			0}
};
static struct ssa_parselist plorg[] = {
	{ssa_gencrd,		{e(org.x)},		0},
	{ssa_gencrd,		{e(org.y)},		0},
	{NULL,			{0},			0}
};
static struct ssa_parselist plfad[] = {
	{ssa_genint,		{e(fad.in)},		0},
	{ssa_genint,		{e(fad.out)},		0},
	{NULL,			{0},			0}
};
static struct ssa_parselist plfade[] = {
	{ssa_alpha,		{e(fade.a1)},		0},
	{ssa_alpha,		{e(fade.a2)},		0},
	{ssa_alpha,		{e(fade.a3)},		0},
	{ssa_genint,		{e(fade.start1)},	0},
	{ssa_genint,		{e(fade.end1)},		0},
	{ssa_genint,		{e(fade.start2)},	0},
	{ssa_genint,		{e(fade.end2)},		0},
	{NULL,			{0},			0}
};
static struct ssa_parselist plmove[] = {
	{ssa_gencrd,		{e(move.x1)},		0},
	{ssa_gencrd,		{e(move.y1)},		0},
	{ssa_gencrd,		{e(move.x2)},		0},
	{ssa_gencrd,		{e(move.y2)},		0},
	{ssa_genint,		{e(move.start)}, 	SSAP_OPTIONAL},
	{ssa_genint,		{e(move.end)},		0},
	{NULL,			{0},			0}
};
static struct ssa_parselist plclip[] = {
	{ssa_gencrd,		{e(clip.x1)},		0},
	{ssa_gencrd,		{e(clip.y1)},		0},
	{ssa_gencrd,		{e(clip.x2)},		0},
	{ssa_gencrd,		{e(clip.y2)},		0},
	{NULL,			{0},			0}
};
#undef e

#define e(x) ((ptrdiff_t) &((struct ssa_line *)0)->effp.x)
static struct ssa_parselist plscroll[] = {
	{ssa_gencrd,		{e(scroll.y1)},		0},
	{ssa_gencrd,		{e(scroll.y2)},		0},
	{ssa_genfp,		{e(scroll.delay)},	0},
	{ssa_genint,		{e(scroll.fadeawayh)},	SSAP_OPTIONAL},
	{NULL,			{0},			0}
};

static struct ssa_parselist plbanner[] = {
	{ssa_genfp,		{e(banner.delay)},	0},
	{ssa_genint,		{e(banner.ltr)},	SSAP_OPTIONAL},
	{ssa_genint,		{e(banner.fadeawayw)},	SSAP_OPTIONAL},
	{NULL,			{0},			0}
};
#undef e

/* no func - directly fed into parse_xsv by ssa_effect */
static struct ssa_parsetext_tag pteffect[] = {
	{"!effect",		NULL,	{0},		SSAE_NONE},
	{"karaoke",		NULL,	{0},		SSAE_KARAOKE},
	{"scroll up;",		NULL,	pl(plscroll),	SSAE_SCROLLUP},
	{"scroll down;",	NULL,	pl(plscroll),	SSAE_SCROLLDOWN},
	{"banner;",		NULL,	pl(plbanner),	SSAE_BANNER},
	{NULL,			NULL,	{0},		0}
};

#define e(x) ((ptrdiff_t) &((struct ssa_node *)0)->v.x)
#define e_alpha		ssa_alpha,	{e(alpha)}
#define e_colour	ssa_colour,	{e(colour)}
#define e_long		ssa_genint,	{e(lval)}
#define e_fp		ssa_genfp,	{e(dval)}
#define e_text		ssa_genstr,	{e(text)}
#define e_style		ssa_sstyle,	{e(style)}
#define e_brace(x)	ssa_brace,	pl(x)
/** core: keyword table for style overrides */
static struct ssa_parsetext_tag ptoverr[] = {
	/* longer matches MUST appear before shorter ones (see fad/fade) */
	{"1a",			e_alpha,	SSANR |	SSAN_ALPHA1},
	{"1c",			e_colour,	SSANR |	SSAN_COLOUR},
	{"2a",			e_alpha,	SSANR |	SSAN_ALPHA2},
	{"2c",			e_colour,	SSANR |	SSAN_COLOUR2},
	{"3a",			e_alpha,	SSANR |	SSAN_ALPHA3},
	{"3c",			e_colour,	SSANR |	SSAN_COLOUR3},
	{"4a",			e_alpha,	SSANR |	SSAN_ALPHA4},
	{"4c",			e_colour,	SSANR |	SSAN_COLOUR4},
	{"K",			e_long,			SSAN_KARAF},
	{"alpha",		e_alpha,	SSANR |	SSAN_ALPHA},
	{"an",			e_long,			SSAN_ALIGNNUM},
	{"a",			e_long,			SSAN_ALIGN},
	{"bord",		e_fp,		SSANR |	SSAN_BORDER},
	{"be",			e_long,			SSAN_BLUREDGES},
	{"b",			e_long,		SSANR |	SSAN_BOLD},
	{"clip",	ssa_clip,	{0},		SSAN_CLIPRECT},
	{"c",			e_colour,	SSANR |	SSAN_COLOUR},
	{"fade",		e_brace(plfade),	SSAN_FADE},
	/* XXX: write ssa_fad (represent \fad as \fade) */
	{"fad",			e_brace(plfad),		SSAN_FAD},
	{"fax",			e_fp,			SSAN_FAX},
	{"fay",			e_fp,			SSAN_FAY},
	{"fe",		ssa_fe,		{0},		SSAN_FE},
	{"fn",			e_text,		SSANR |	SSAN_FONT},
	{"frx",			e_fp,			SSAN_FRX},
	{"fry",			e_fp,			SSAN_FRY},
	{"frz",			e_fp,			SSAN_FRZ},
	{"fr",			e_fp,			SSAN_FRZ},
	{"fscx",		e_fp,		SSANR |	SSAN_FSCX},
	{"fscy",		e_fp,		SSANR |	SSAN_FSCY},
	{"fsp",			e_fp,		SSANR |	SSAN_FSP},
	{"fs",			e_fp,		SSANR |	SSAN_FONTSIZE},
	{"i",			e_long,		SSANR |	SSAN_ITALICS},
	{"kf",			e_long,			SSAN_KARAF},
	{"ko",			e_long,			SSAN_KARAO},
	{"kt",			e_long,			SSAN_KARAT},
	{"k",			e_long,			SSAN_KARA},
	{"move",	ssa_move,	{0},		SSAN_MOVE},
	{"org",			e_brace(plorg),		SSAN_ORG},
	{"pbo",			e_long,			SSAN_PBO},
	{"pos",			e_brace(plpos),		SSAN_POS},
	{"p",		ssa_notsup,	{1},		SSAN_PAINT},
	{"q",			e_long,			SSAN_WRAP},
	{"r",			e_style,		SSAN_RESET},
	{"shad",		e_fp,		SSANR |	SSAN_SHADOW},
	{"s",			e_long,		SSANR |	SSAN_STRIKEOUT},
	{"t",		ssa_t,		{0},		SSAN_T},
	{"u",			e_long,		SSANR |	SSAN_UNDERLINE},
	{NULL,		NULL,		{0},		0}
};
#undef e_alpha
#undef e_colour
#undef e_long
#undef e_fp
#undef e_text
#undef e_style
#undef e_brace

/** return nodetype name.
 * @param type type to get the name for.
 * @return a static string (ASCII), containing the override tag name
 *  (without leading \\)
 */
const char *ssa_typename(enum ssa_nodetype type)
{
	struct ssa_parsetext_tag *pt = ptoverr;
	for (pt = ptoverr; pt->text; pt++)
		if ((pt->type & ~SSANR) == type)
			return pt->text;
	return NULL;
}

/** push error on error list.
 * @param state parser state
 * @param location position where the error occured
 *  - location is used for determining column and MUST be somewhere in
 *    state->line ... state->end
 * @param origin position which probably has caused the error
 * @param ec error code
 */
static void ssa_add_error_ext(struct ssa_state *state,
	const ssasrc_t *location, const ssasrc_t *origin,
	enum ssa_errc ec)
{
	struct ssa_error *me;
	size_t textlen = state->end - state->line;
	char *severity =
		ssaec[ec].sev == 0 ? "information" :
		ssaec[ec].sev == 1 ? "hint" :
		ssaec[ec].sev == 2 ? "warning" :
		ssaec[ec].sev == 3 ? "error" :
		ssaec[ec].sev == 4 ? "fatal" :
		"?";


	if (state->ctx == SSACTX_PACKET)
		subhelp_log(CSRI_LOG_DEBUG, "parser: <stream>: %s: %s",
			severity, ssaec[ec].sh);
	else
		subhelp_log(CSRI_LOG_DEBUG, "parser: %d:%d: %s: %s",
			state->lineno, location - state->line,
			severity, ssaec[ec].sh);

	if (state->nerr++ >= state->output->maxerrs) {
		int ne = state->nerr < 1000 ? 100 : 1000;
		if (state->nerr % ne == 0)
			subhelp_log(CSRI_LOG_WARNING, "%d parser errors",
				state->nerr);
		return;
	}
	
	me = xnew(struct ssa_error);
	me->next = NULL;
	me->lineno = state->lineno;
	me->column = (unsigned)(location - state->line);
	me->origin = origin ? (unsigned)(origin - state->line)
		: SSA_ENOORIGIN;
/*	me->description = description; */
	me->errorcode = ec;
	me->textline = (ssasrc_t *)xmalloc((textlen + 1) * sizeof(ssasrc_t));
	memcpy(me->textline, state->line, textlen * sizeof(ssasrc_t));
	me->textline[textlen] = '\0';
	me->linelen = (unsigned)textlen;

	*state->elast = me;
	state->elast = &me->next;
}

/** push error on error list, without origin.
 * @see ssa_add_error_ext
 */
static void ssa_add_error(struct ssa_state *state, const ssasrc_t *location,
	enum ssa_errc ec)
{
	ssa_add_error_ext(state, location, NULL, ec);
}

/** skip whitespace.
 * @param now current position
 * @param end maximum position to advance to
 */
static inline void ssa_skipws(struct ssa_state *state,
	const ssasrc_t **now, const ssasrc_t *end)
{
	while (*now < end) {
		if (!ssa_isspace(**now)) {
			if (*now + 3 > end
				|| (unsigned char)(*now)[0] != 0xef
				|| (unsigned char)(*now)[1] != 0xbb
				|| (unsigned char)(*now)[2] != 0xbf)
				break;
			ssa_add_error(state, *now, SSAEC_STRAY_BOM);
			*now += 2;
		}
		(*now)++;
	}
}

/** ssa_compare - compare SSA content with given text.
 * @param now start of source text to check
 * @praam end end of source text
 * @param text text to compare against.
 *  - 0-terminated
 *  - a single space character matches any whitespace
 *  - lowercase matches both lowercase and uppercase
 * @param err (out) first nonmatching source character
 *  - may be NULL
 * @return if matched, pointer to fisrt char after end of match.
 *  whitespace at the end is skipped
 */

static const ssasrc_t *ssa_compare(struct ssa_state *state,
	const ssasrc_t *now, const ssasrc_t *end,
	const char *text, const ssasrc_t **err)
{
	while (now < end && *text) {
		if (*text == ' ') {
			if (!ssa_isspace(*now))
				break;
			ssa_skipws(state, &now, end);
			text++;
			continue;
		}
		if (*now != *text)
			if (!ssa_isalpha(*text) || *now != ssa_toupper(*text))
				break;
		now++, text++;
	}

	if (!*text) {
		ssa_skipws(state, &now, end);
		return now;
	}
	if (err)
		*err = now;
	return NULL;
}

/** compare source against output.
 * @param state parser state
 * @param now start of source text
 * @param end end of source text
 * @param text what to match against, no special meanings like ssa_compare
 * @param err error pointer
 * @return first character after match, with whitespace skipped
 * @see ssa_compare
 */
static const ssasrc_t *ssa_cmpout(struct ssa_state *state,
	const ssasrc_t *now, const ssasrc_t *end,
	ssa_string *text, const ssasrc_t **err)
{
	ssaout_t *onow = text->s, *oend = text->e;
	while (now < end && onow < oend) {
		if ((unsigned)*now != *onow)
			break;
		now++, onow++;
	}

	if (onow == oend) {
		ssa_skipws(state, &now, end);
		return now;
	}
	if (err)
		*err = now;
	return NULL;
}

/** ssa_chr - scan string for character.
 * memchr, basically */
static const ssasrc_t *ssa_chr(const ssasrc_t *now, const ssasrc_t *end,
	ssasrc_t chr)
{
	while (now < end) {
		if (*now == chr)
			return now;
		now++;
	}
	return NULL;
}

/** ssa_strdup - duplicate ssa_string.
 * @param dst destionation ssa_string.
 * @param src source
 * */
static void ssa_strdup(ssa_string *dst, ssa_string *src)
{
	size_t size = (char *)src->e - (char *)src->s + sizeof(ssaout_t);
	dst->s = (ssaout_t *)xmalloc(size);
	memcpy(dst->s, src->s, size);
	dst->e = dst->s + (src->e - src->s);
}

/** ssa_setctx - set context to lparam.
 * @param param new context
 * @param elem unused
 */
static unsigned ssa_setctx(struct ssa_state *state, par_t param, void *elem)
{
	if (state->ctx_pl)
		free(state->ctx_pl);
	state->ctx_pl = NULL;
	state->ctx = param.lparam;
	return 1;
}

/** ssa_setver - check & set ssa version (SSA / ASS).
 * @param param (lparam) zero to parse text, nonzero to set to lparam
 * @param elem unused
 *  this also enters style context if used with a version number
 */
static unsigned ssa_setver(struct ssa_state *state, par_t param, void *elem)
{
	enum ssa_version ver = param.lparam;
	if (!ver) {
		const ssasrc_t *after, *err;
		if ((after = ssa_compare(state, state->param, state->pend,
			"v4.00++", &err)))
			ver = SSAVV_4PP;
		else if ((after = ssa_compare(state, state->param, state->pend,
			"v4.00+", &err)))
			ver = SSAVV_4P;
		else if ((after = ssa_compare(state, state->param,
			state->pend, "v4.00", &err)))
			ver = SSAVV_4;
		else {
			ssa_add_error(state, err, SSAEC_UNKNOWNVER);
			return 0;
		}
		/* see semantics of ssa_parsefunc */
		state->param = after;
	} else {
		par_t p = { SSACTX_STYLE };
		ssa_setctx(state, p, elem);
	}
	if (state->output->version && state->output->version != ver) {
		ssa_add_error(state, state->line,
			SSAEC_AMBIGUOUS_SVER);
		/* we're going to continue parsing, so expect ASS stuff */
		state->output->version = SSAVV_4P;
		return 0;
	}
	state->output->version = ver;
	return 1;
}

/** something not supported.
 * @param param (lparam) nonzero to skip until next backslash,
 *   zero to skip all
 */
static unsigned ssa_notsup(struct ssa_state *state, par_t param, void *elem)
{
	const ssasrc_t *next;

	ssa_add_error(state, state->param, SSAEC_NOTSUP);
	if (param.lparam) {
		next = ssa_chr(state->param, state->pend, '\\');
		state->param = next ? next : state->pend;
	} else
		state->param = state->pend;
	return 0;
}

/** convert ssasrc_t string to ssaout_t string.
 * used for stuff in base encoding (means: font names, style names, etc.,
 *   everything except text lines).
 * entire func is CSDEP
 */
static void ssa_src2str(struct ssa_state *state,
	const ssasrc_t *now, const ssasrc_t *end, ssa_string *str)
{
	size_t outsize = 0, outleft = 0,
		inleft = end - now;
	/* iconv madness */
	ICONV_CONST char *innow = (ICONV_CONST char *)now;
	char *outbuf = NULL, *outnow = NULL;
	ptrdiff_t outnowd = 0;

	iconv(state->ic_srcout, NULL, NULL, NULL, NULL);

	do {
		outsize += (inleft + 1) * sizeof(wchar_t);
		outleft += (inleft + 1) * sizeof(wchar_t);
		outbuf = (char *)xrealloc(outbuf, outsize);
		outnow = outbuf + outnowd;

		iconv_errno = 0;
		iconv(state->ic_srcout, &innow, &inleft, &outnow, &outleft);

		outnowd = outnow - outbuf;
	} while (iconv_errno == E2BIG);
	if (iconv_errno || inleft) {
		free(outbuf);
		str->s = str->e = NULL;
		ssa_add_error_ext(state, (ssasrc_t *)innow, now,
			SSAEC_INVAL_ENCODING);
		return;
	}
	/* 4 zero bytes at end for safety */
	outbuf = (char *)xrealloc(outbuf, outnowd + 4);
	*(unsigned *)(outbuf + outnowd) = 0;

	str->s = (ssaout_t *)outbuf;
	str->e = (ssaout_t *)(outbuf + outnowd);
}

/** ssa_genstr - some string parameter into offset specified by param.
 * @param state parser state. non-NULL anisource causes an error
 * @param param (offset) element to store
 * @param elem base for param.offset
 */
static unsigned ssa_genstr(struct ssa_state *state, par_t param, void *elem)
{
	ssa_string *str;
	if (state->anisource)
		ssa_add_error_ext(state, state->param, state->anisource,
			SSAEC_INVAL_ANI);
	str = ((ssa_string *)apply_offset(elem, param.offset));
	ssa_src2str(state, state->param, state->pend, str);
	state->param = state->pend;
	return 1;
}

/** common code for ssa_genint & ssa_genfp.
 *  XXX: check wether iswdigit and wcsto{l,d} support the "other" unicode
 * digits (half-width and full-width etc.)
 */
static inline unsigned ssa_scannum(struct ssa_state *state, ssasrc_t *buf,
	ssasrc_t **bptr, unsigned allow_dot, unsigned allow_sign,
	unsigned maxlen)
{
	unsigned dot = allow_dot + 1;
	*bptr = buf;
	/* overrun neither line nor buf */
	while (state->param < state->pend && *bptr < buf + maxlen) {
		/* *bptr == buf => first char => sign; dot by allow_dot */
		if (ssa_isdigit(*state->param)
			|| (*bptr == buf && allow_sign
			 && (*state->param == '+' || *state->param == '-'))
			|| (*state->param == '.' && (dot++ == 2)))

			*(*bptr)++ = *state->param++;
		else
			break;
	}
	/* nothing in buf? */
	if (*bptr == buf) {
		ssa_add_error(state, state->param, SSAEC_EXC_NUMORSIGN);
		return 0;
	}
	/* catch . + - alone */
	if (*bptr == buf + 1 && !ssa_isdigit(buf[0])) {
		ssa_add_error(state, state->param, SSAEC_EXC_NUM);
		return 0;
	}
	if (*bptr == buf + maxlen) {
		ssa_add_error(state, state->param, SSAEC_NUM_2LONG);
		return 0;
	}
	**bptr = '\0';
	return dot;
}

/** long int into offset.
 * @see ssa_genstr
 */
static unsigned ssa_genint(struct ssa_state *state, par_t param, void *elem)
{
	long int result;
	ssasrc_t buf[12]; /* -2147483648\0 */
	ssasrc_t *bptr, *endptr;

	if (!ssa_scannum(state, buf, &bptr, 0, 1, 12))
		return 0;
	/* we *could* accept 0x... & such as well
	 * - but what about 012? (octal)
	 * btw, actually don't need endptr...
	 */
	result = ssatol(buf, &endptr, 10);
	if (bptr != endptr) {
		ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
		return 0;
	}
	*(long int *)apply_offset(elem, param.offset) = result;
	return 1;
}

/** core for ssa_genint and ssa_twoint.
 * @see ssa_genint
 */
static inline unsigned ssa_gencrdcore(struct ssa_state *state, par_t param,
	void *elem, coord_t *value)
{
	coord_t result;
	unsigned dot;
	ssasrc_t buf[15]; /* -32767.9999847\0 (well, two digits are safety)*/
	ssasrc_t *bptr, *endptr;

	dot = ssa_scannum(state, buf, &bptr, 1, 1, 15);
	if (!dot)
		return 0;
	if (dot == 2)	/* dot == 2, no dot found */
		result = ssatol(buf, &endptr, 10) << 16;
	else {		/* dot == 3, dot found */
		ssa_add_error(state, state->param, SSAEC_FP_COORD);
		result = (coord_t)(ssatod(buf, &endptr) * 65536.);
	}
	if (bptr != endptr) {
		ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
		return 0;
	}
	*value = result;

	return 1;
}

/** long int into offset.
 * @see ssa_genstr
 */
static unsigned ssa_gencrd(struct ssa_state *state, par_t param, void *elem)
{
	return ssa_gencrdcore(state, param, elem,
		(coord_t *)apply_offset(elem, param.offset));
}

/** long int into offset and offset + sizeof(long).
 * @see ssa_genstr
 */
static unsigned ssa_twocrd(struct ssa_state *state, par_t param, void *elem)
{
	coord_t value;
	if (!ssa_gencrdcore(state, param, elem, &value))
		return 0;
	((coord_t *)apply_offset(elem, param.offset))[0] = value;
	((coord_t *)apply_offset(elem, param.offset))[1] = value;
	return 1;
}

/** double into offset.
 * @see ssa_genstr
 */
static unsigned ssa_genfp (struct ssa_state *state, par_t param, void *elem)
{
	double result;
	ssasrc_t buf[32];
	ssasrc_t *bptr, *endptr;

	if (!ssa_scannum(state, buf, &bptr, 1, 1, 32))
		return 0;
	result = ssatod(buf, &endptr);
	if (bptr != endptr) {
		ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
		return 0;
	}
	*((double *)apply_offset(elem, param.offset)) = result;

	return 1;
}

/** bool (yes/no) into long at offset.
 * @see ssa_genstr
 */
static unsigned ssa_genbool(struct ssa_state *state, par_t param, void *elem)
{
	const ssasrc_t *after;
	if ((after = ssa_compare(state, state->param, state->pend,
		"yes", NULL)))
		*(long int *)apply_offset(elem, param.offset) = 1;
	else if ((after = ssa_compare(state, state->param, state->pend,
		"no", NULL)))
		*(long int *)apply_offset(elem, param.offset) = 0;
	else {
		ssa_add_error(state, state->param, SSAEC_EXC_BOOL);
		return 0;
	}
	state->param = after;
	return 1;
}

/** time into offset.
 * @see ssa_genstr
 */
static unsigned ssa_time(struct ssa_state *state, par_t param, void *elem)
{
	ssasrc_t buf[8], *bptr, *endptr;
	const ssasrc_t *next;
	unsigned c;
	long hm[2];
	double secs;

	for (c = 0; c < 2; c++) {
		if (!ssa_scannum(state, buf, &bptr, 0, 0, 8))
			return 0;
		hm[c] = ssatol(buf, &endptr, 10);
		if (bptr != endptr) {
			ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
			return 0;
		}
		next = ssa_chr(state->param, state->pend, ':');
		if (state->param != next) {
			ssa_add_error(state, state->param, SSAEC_EXC_DCOL);
			return 0;
		}
		state->param++;
	}

	if (!ssa_scannum(state, buf, &bptr, 1, 0, 8))
		return 0;
	secs = ssatod(buf, &endptr);
	if (bptr != endptr) {
		ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
		return 0;
	}

	secs += (hm[0] * 60 + hm[1]) * 60;
	*((double *)apply_offset(elem, param.offset)) = secs;
	return 1;
}

/** ssa_parse_xsv - parses X-separated values according to ssa_parselist.
 * @param p dispatch list
 * @param elem passed to dispatch parsefuncs
 * @param separator separator, usually , or ;
 *
 * s->param on call points to start for parsing, on return to first unparsed
 */
static inline unsigned ssa_parse_xsv(struct ssa_state *s,
	struct ssa_parselist *p, void *elem, ssasrc_t separator)
{
	const ssasrc_t *comma = s->param - 1, *save_pend = s->pend;
	unsigned ret;
	while (p->func || p->param.lparam || p->param.offset) {
		if (!comma) {
			if (p->flags & SSAP_OPTIONAL)
				return 1;
			ssa_add_error(s, s->pend, SSAEC_TRUNCLINE);
			return 0;
		}
		s->param = comma + 1;
		if ((comma = ssa_chr(s->param, s->pend, separator)))
			s->pend = comma;
		ret = 1;
		if (p->func) {
			ssa_skipws(s, &s->param, s->pend);
			ret = p->func(s, p->param, elem);
		}
		if (ret) {
			ssa_skipws(s, &s->param, s->pend);
			if (s->param != s->pend)
				ssa_add_error(s, s->param,
					SSAEC_TRAILGB_PARAM);
		}
		s->pend = save_pend;
		p++;
	}
	return 1;
}

/** ssa_default_fmt - turn ssa_parselist_fmt into ssa_parselist.
 * @param 
 */
static struct ssa_parselist *ssa_default_fmt(struct ssa_state *s,
	struct ssa_parselist_fmt *fmtpl)
{
	struct ssa_parselist *rv, *o;
	struct ssa_parselist_fmt *p = fmtpl;
	unsigned nalloc = 1;

	for (p = fmtpl; p->func || p->param.lparam || p->param.offset; p++)
		if (p->ssa_version & s->output->version
			&& !(p->ssa_version & SSAP_NODEF))
			nalloc++;
	o = rv = (struct ssa_parselist *)xmalloc(nalloc
		* sizeof(struct ssa_parselist));
	for (p = fmtpl; p->func || p->param.lparam || p->param.offset; p++)
		if (p->ssa_version & s->output->version
			&& !(p->ssa_version & SSAP_NODEF)) {
			o->func = p->func;
			o->param = p->param;
			o->flags = p->ssa_version & SSAP_ALL;
			o++;
		}
	memset(o, 0, sizeof(*o));
	return rv;
}

/** temporary bookkeeping for Format: parsing */
struct ssa_fmt_temp {
	struct ssa_parselist_fmt *pl,	/**< expected */
		*plpos;			/**< all */
	struct ssa_parselist *o;	/**< output */
	int opos, omax;
	int text, strange;
};

/** ssa_fmt_strange - emit strange-Format: warning once per Format:
 * @param s ssa state
 * @param t Format: parser state
 */
static inline void ssa_fmt_strange(struct ssa_state *s, struct ssa_fmt_temp *t)
{
	if (!t->strange) {
		ssa_add_error(s, s->param, SSAEC_NONSTANDARD_FMT);
		t->strange = 1;
	}
}

/** ssa_fmt_try - check whether ssa_parselist_fmt matches column name.
 * @param s ssa state
 * @param t Format: parser state
 * @param pl double pointer to entry to check.\n
 *   will be incremented if it matches, to update expected position
 * @return nonzero if match
 */
static int ssa_fmt_try(struct ssa_state *s, struct ssa_fmt_temp *t,
	struct ssa_parselist_fmt **pl)
{
	const ssasrc_t *after;
	after = ssa_compare(s, s->param, s->pend, (*pl)->column, NULL);
	if (!after)
		return 0;
	ssa_skipws(s, &after, s->pend);
	if (after != s->pend)
		return 0;
	t->o[t->opos].func = (*pl)->func;
	t->o[t->opos].param = (*pl)->param;
	t->o[t->opos].flags = (*pl)->ssa_version & SSAP_ALL;
	t->opos++;
	(*pl)++;
	s->param = s->pend;
	return 1;
}

/** ssa_fmt_one - process one Format: entry.
 * @param s ssa state
 * @param t Format: parser state, updated by this
 */
static void ssa_fmt_one(struct ssa_state *s, struct ssa_fmt_temp *t)
{
	struct ssa_parselist_fmt *plsearch;
	const ssasrc_t *save_start = s->param;

	if (!t->strange && t->plpos->column) {
		while (t->plpos->column
			&& (!(t->plpos->ssa_version & s->output->version)
				|| t->plpos->ssa_version & SSAP_NODEF))
			t->plpos++;
		if (ssa_fmt_try(s, t, &t->plpos))
			return;
	}
	if (s->ctx == SSACTX_EVENTS) {
		const ssasrc_t *after;
		after = ssa_compare(s, s->param, s->pend, "text", NULL);
		if (after) {
			t->text = 1;
			if (t->plpos->column)
				ssa_fmt_strange(s, t);
			s->param = after;
			return;
		}
	}
	ssa_fmt_strange(s, t);
	for (plsearch = t->pl; plsearch->column; plsearch++) {
		if (!(plsearch->ssa_version & s->output->version))
			continue;
		if (ssa_fmt_try(s, t, &plsearch))
			return;
	}
	for (plsearch = t->pl; plsearch->column; plsearch++)
		if (ssa_fmt_try(s, t, &plsearch)) {
			ssa_add_error(s, save_start, SSAEC_COLUMN_VERSION);
			return;
		}
	ssa_add_error(s, save_start, SSAEC_UNKNOWN_COLUMN);
}

/** ssa_fmt - parse Format: line according to active context.
 * @param s parser state
 * @param param unused
 * @param elem (points to output) unused
 * @return always 0 (ignore remaining line)
 *
 * updates ssa_state.ctx_pl accordingly.
 */
static unsigned ssa_fmt(struct ssa_state *s, par_t param, void *elem)
{
	struct ssa_fmt_temp t;
	const ssasrc_t *comma = s->param - 1, *save_pend = s->pend;

	if (s->ctx == SSACTX_STYLE)
		t.pl = plstyle;
	else if (s->ctx == SSACTX_EVENTS)
		t.pl = plcommon;
	else {
		ssa_add_error(s, s->line, SSAEC_INVALID_CTX);
		return 0;
	}
	if (s->ctx_pl) {
		ssa_add_error(s, s->line, SSAEC_REPEATED_FMT);
		free(s->ctx_pl);
	}

	t.text = t.strange = t.opos = 0;
	t.omax = 32;
	t.o = (struct ssa_parselist *)xmalloc(t.omax
		* sizeof(struct ssa_parselist));
	t.plpos = t.pl;
	while (comma) {
		if (t.text) {
			ssa_add_error(s, s->param, SSAEC_TEXT_NOT_LAST);
			break;
		}
		s->param = comma + 1;
		if ((comma = ssa_chr(s->param, s->pend, ',')))
			s->pend = comma;
		if (t.opos == t.omax) {
			t.omax += 32;
			t.o = (struct ssa_parselist *)xrealloc(t.o,
				t.omax * sizeof(struct ssa_parselist));
		}
		ssa_skipws(s, &s->param, s->pend);
		ssa_fmt_one(s, &t);
		s->pend = save_pend;
	}
	while (t.plpos->column
		&& (!(t.plpos->ssa_version & s->output->version)
			|| t.plpos->ssa_version & SSAP_NODEF))
		t.plpos++;
	if (!t.text && t.plpos->column)
		ssa_fmt_strange(s, &t);
	t.o = (struct ssa_parselist *)xrealloc(t.o,
		(t.opos + 1) * sizeof(struct ssa_parselist));
	memset(t.o + t.opos, 0, sizeof(t.o[0]));
	s->ctx_pl = t.o;
	return 0;
}

/** ssa_style - parse Style: line.
 * @param param unused
 * @param elem (points to output) unused
 */
static unsigned ssa_style(struct ssa_state *state, par_t param, void *elem)
{
	struct ssa_parselist *pl;
	struct ssa_style *style = xnewz(struct ssa_style);
	*state->output->style_last = style;
	state->output->style_last = &style->next;
	/* TODO: init with sane defaults */
	style->relative = 1;
	style->node_last = &style->node_first;

	pl = state->ctx_pl;
	if (!pl) {
		pl = ssa_default_fmt(state, plstyle);
		if (state->ctx == SSACTX_STYLE) {
			ssa_add_error(state, state->line,
				SSAEC_DEFAULT_STYLE_FMT);
			state->ctx_pl = pl;
		 }
	}
	if (!ssa_parse_xsv(state, pl, style, ',')) {
		if (!state->ctx_pl)
			free(pl);
		return 0;
	}
	if (!state->ctx_pl)
		free(pl);
	if (state->param == state->end)
		return 1;
	ssa_add_error(state, state->param, SSAEC_TRAILGB_LINE);
	return 0;
}

/** ssa_do_call_overr - calls parsefunc for one override.
 * has to find end of style (genstr will eat up everything otherwise)
 * return value determines wether parseoverr will spit out error msg
 * on garbage before next backslash
 */
static inline unsigned ssa_do_call_overr(struct ssa_parsetext_tag *p,
	struct ssa_state *s, struct ssa_node ***prev)
{
	struct ssa_node *node;
	unsigned ret = p->param.lparam;
	const ssasrc_t *save_pend, *slash, *bracket;

	if (p->func) {
		node = xnewz(struct ssa_node);
		node->type = p->type & ~SSANR;
		**prev = node;
		*prev = &node->next;

		/* hrm. a bit tricky ... we need pend for most "normal"
		 * parse funcs, but \t has "child" slashes ... let's do
		 * it the "logical" way :/
		 */
		save_pend = s->pend;

		slash = ssa_chr(s->param, s->pend, '\\');
		bracket = ssa_chr(s->param, slash, '(');
		if (slash && ((!bracket) || (bracket > slash)))
			s->pend = slash;
		if (p->type & SSANR && s->param == s->pend) {
			node->type |= SSANR;
			ret = 1;
		} else
			ret = p->func(s, p->param, node);
		s->pend = save_pend;

		ssa_skipws(s, &s->param, s->pend);
	}
	return ret;
}

/* the heart ;).
 * method for finding match isn't optimal yet, perhaps some sorted stuff?
 */
static unsigned ssa_parseoverr(struct ssa_state *s, struct ssa_node ***prev)
{
	unsigned lastret = 1;

	ssa_skipws(s, &s->param, s->pend);
	while (s->param < s->pend) {
		struct ssa_parsetext_tag *p = ptoverr;
		const ssasrc_t *slash, *best = NULL, *err, *now = NULL;

		/* hrm. will bail on garbage before ) */
		if (*s->param == ')')
			return 1;
		
		slash = ssa_chr(s->param, s->pend, '\\');
		if (slash != s->param && lastret)
			ssa_add_error(s, s->param, SSAEC_GB_STYLEOVER);
		if (!slash)
			return 1;
		s->param = slash + 1;
		
		lastret = 0;
		while (p->text) {
			now = ssa_compare(s, s->param, s->pend, p->text, &err);
			if (now) {
				s->param = now;
				lastret = ssa_do_call_overr(p, s, prev);
				break;
			}
			if (err > best)
				best = err;
			p++;
		}
		if (!now)
			ssa_add_error_ext(s, slash, best,
				SSAEC_STYLEOVER_UNRECOG);
	}
	return 1;
}

/** convert encoding number to iconv charset name */
static iconv_t ssa_enc2iconv(struct ssa_state *state, long int encoding)
{
	const char *cs = NULL;
	if (state->unicode)
		cs = state->ic_srccs;
	else switch (encoding) {
/* XXX
 *  BIG PROBLEM!
 *  UTF8 and UCS files usually have encoding = 0
 *   => would make them MS-ANSI, which is definitely not what we want.
 *   => wire 0 to source charset (UTF8 for Unicode, ANSI for other) and
 *         add 1 as alias for MS-ANSI (its our "DEFAULT" :)
 */
#if 0
	case 0:   cs = "MS-ANSI";	break; /* ANSI_CHARSET */
/*	case 1:   cs = "";		break;  - DEFAULT_CHARSET? */
/*	case 2:   cs = "";		break;  - SYMBOL_CHARSET? */
#endif
	case 0:   cs = state->ic_srccs;	break; /* ANSI_CHARSET */
	case 1:   cs = "MS-ANSI";	break; /* DEFAULT_CHARSET? */
/*	case 2:   cs = "";		break;  - SYMBOL_CHARSET? */

	case 77:  cs = "MAC";		break; /* MAC_CHARSET */
	case 128: cs = "CP932";		break; /* SHIFTJIS_CHARSET */
	case 129: cs = "CP949";		break; /* HANGEUL_CHARSET */
	case 130: cs = "JOHAB";		break; /* JOHAB_CHARSET */
	case 134: cs = "CP936";		break; /* GB2312_CHARSET */
	case 136: cs = "CP950";		break; /* CHINESEBIG5_CHARSET */
	case 161: cs = "MS-GREEK";	break; /* GREEK_CHARSET */
	case 162: cs = "MS-TURK";	break; /* TURKISH_CHARSET */
	case 163: cs = "CP1258";	break; /* VIETNAMESE_CHARSET? */
	case 177: cs = "MS-HEBR";	break; /* HEBREW_CHARSET */
	case 178: cs = "MS-ARAB";	break; /* ARABIC_CHARSET */
	case 186: cs = "CP1257";	break; /* BALTIC_CHARSET */
	case 204: cs = "MS-CYRL";	break; /* RUSSIAN_CHARSET */
	case 222: cs = "CP874";		break; /* THAI_CHARSET? */
	case 238: cs = "MS-EE";		break; /* EASTEUROPE_CHARSET */

	default:  cs = "MS-ANSI";
	}

	return iconv_open(SSA_DESTCS, cs);
}

/** text parsing state.
 * we can't directly copy text because of \\ and \{ ... we could make that
 * separate text nodes, but i kinda don't want that
 */
struct ssa_temp_text {
	struct ssa_node *node;		/**< node we're writing into */
	ptrdiff_t allocated;		/**< allocated bytes */
	ptrdiff_t pos;			/**< writing position, in bytes */
};

/** make sure tmp is valid and has space for at least 2 characters */
static void ssa_tmp_enchant(struct ssa_temp_text *tmp,
	struct ssa_node ***prev)
{
	if (!tmp->node) {
		tmp->node = xnew(struct ssa_node);
		tmp->node->next = NULL;
		tmp->node->type = SSAN_TEXT;
		tmp->node->v.text.s = NULL;
		**prev = tmp->node;
		*prev = &tmp->node->next;
	}
	if (tmp->pos + 2 * (int)sizeof(ssaout_t) >= tmp->allocated)
		tmp->node->v.text.s = (ssaout_t *)xrealloc(
			tmp->node->v.text.s,
			(tmp->allocated += 128) * sizeof(ssaout_t));
}

/** convert and store text lines into ssa_temp_text.
 * CSDEP
 */
static void ssa_tmp_iconv(struct ssa_temp_text *tmp,
	struct ssa_node ***prev, struct ssa_state *state,
	const ssasrc_t **now, const ssasrc_t *end)
{
	/* (iconv madness - should be const) */
	char *outnow;
	size_t outleft,
		inleft = end - *now;

	if (inleft == 0)
		return;

	iconv(state->ic_tcv, NULL, NULL, NULL, NULL);

	do {
		ssa_tmp_enchant(tmp, prev);
		
		outnow = (char *)tmp->node->v.text.s + tmp->pos;
		outleft = tmp->allocated - tmp->pos;

		iconv_errno = 0;
		iconv(state->ic_tcv, (ICONV_CONST char **)now, &inleft,
			&outnow, &outleft);
		tmp->pos = outnow - (char *)tmp->node->v.text.s;
	} while (iconv_errno == E2BIG);
	if (iconv_errno || inleft) {
		ssa_add_error(state, (ssasrc_t *)*now, SSAEC_INVAL_ENCODING);
	}
}

/** add single char to ssa_temp_text */
static void ssa_tmp_add(struct ssa_temp_text *tmp,
	struct ssa_node ***prev, ssaout_t ch)
{
	ssa_tmp_enchant(tmp, prev);
	tmp->node->v.text.s[tmp->pos / sizeof(ssaout_t)] = ch;
	tmp->pos += sizeof(ssaout_t);
}

/** finalize text node.
 * deattaches ssa_temp_text from node and minimizes node text size
 */
static inline void ssa_tmp_close(struct ssa_temp_text *tmp)
{
	if (!tmp->node)
		return;
	tmp->node->v.text.s = (ssaout_t *)xrealloc(tmp->node->v.text.s,
		tmp->pos + sizeof(ssaout_t));
	tmp->node->v.text.e = (ssaout_t *)((char *)tmp->node->v.text.s
		+ tmp->pos);
	*tmp->node->v.text.e = '\0';
	tmp->node = NULL;
	tmp->allocated = tmp->pos = 0;
}

/** insert newline node */
static inline void ssa_add_newline(unsigned code, struct ssa_node ***prev)
{
	struct ssa_node *node = xnew(struct ssa_node);
	node->next = NULL;
	node->type = code;
	**prev = node;
	*prev = &node->next;
}

/** parse Dialogue: text.
 *  - recognises \\ \{ \n \N
 *  - calls parseoverr on { with pend set to }
 */
static inline unsigned ssa_parsetext(struct ssa_state *state,
	struct ssa_line *line)
{
	const ssasrc_t *now = state->param, *brace, *slash, *lim;
	ssasrc_t tmps;
	struct ssa_state call;
	struct ssa_temp_text text;

	memcpy(&call, state, sizeof(struct ssa_state));

	line->node_last = &line->node_first;

	text.node = NULL;
	text.allocated = text.pos = 0;

	state->ic_tcv = ssa_enc2iconv(state, state->ic_tcv_enc
		= line->style->encoding);

	while (now < state->pend) {
		brace = ssa_chr(now, state->pend, '{');
		slash = ssa_chr(now, state->pend, '\\');
		if (brace && slash)
			lim = brace < slash ? brace : slash;
		else
			lim = brace ? brace : slash;
		if (!lim)
			lim = state->pend;

		ssa_tmp_iconv(&text, &line->node_last, state, &now, lim);
		if (now != lim)
			now = lim;

		if (now == state->pend)
			break;

		switch (*now) {
		case '\\':
			if (now + 1 == state->pend) {
				ssa_tmp_add(&text, &line->node_last, '\\');
				break;
			}
			tmps = *++now;
			switch (tmps) {
			case '{':
			case '\'':
			case '\"':
			case '\\':
				ssa_tmp_add(&text, &line->node_last, tmps);
				break;
			case 'n':
				ssa_tmp_close(&text);
				ssa_add_newline(SSAN_NEWLINE,
					&line->node_last);
				break;
			case 'N':
				ssa_tmp_close(&text);
				ssa_add_newline(SSAN_NEWLINEH,
					&line->node_last);
				break;
			default:
				ssa_tmp_add(&text, &line->node_last, '\\');
				ssa_tmp_add(&text, &line->node_last, tmps);
			}
			break;
		case '{':
			ssa_tmp_close(&text);
			call.param = now + 1;
			call.ic_tcv_enc = state->ic_tcv_enc;
			call.ic_tcv = state->ic_tcv;
			if (!(now = ssa_chr(++now, state->pend, '}'))) {
				now = state->pend - 1;
				call.pend = state->pend;
			} else
				call.pend = now;
			ssa_parseoverr(&call, &line->node_last);
			state->ic_tcv = call.ic_tcv;
			state->ic_tcv_enc = call.ic_tcv_enc;
			break;
		default:
			/* internal error? */
			ssa_tmp_add(&text, &line->node_last, *now);
		}
		now++;
	}
	state->param = now;
	ssa_tmp_close(&text);
	iconv_close(state->ic_tcv);
	state->ic_tcv = NULL;
	return 1;
}

/** update font encoding (ic_tcv) */
static unsigned ssa_fe(struct ssa_state *state, par_t param, void *elem)
{
	long int result;
	ssasrc_t buf[4];
	ssasrc_t *bptr, *endptr;

	if (state->anisource)
		ssa_add_error_ext(state, state->param, state->anisource,
			SSAEC_INVAL_ANI);

	if (!ssa_scannum(state, buf, &bptr, 0, 0, 4))
		return 0;
	result = ssatol(buf, &endptr, 10);
	if (bptr != endptr) {
		ssa_add_error(state, state->param, SSAEC_INVAL_ENC);
		return 0;
	}
	((struct ssa_node *)elem)->v.lval = result;
	if (result != state->ic_tcv_enc) {
		iconv_close(state->ic_tcv);
		state->ic_tcv = ssa_enc2iconv(state,
			state->ic_tcv_enc = result);
	}

	return 1;
}

/** parse effect field.
 * there is only 1 effect in the effect field...
 */
static unsigned ssa_effect(struct ssa_state *state, par_t param, void *elem)
{
	struct ssa_parsetext_tag *p = pteffect;
	struct ssa_line *line = (struct ssa_line *) elem;
	const ssasrc_t *best = NULL, *err, *now = NULL;
	unsigned ret = 1;

	if (state->param == state->pend)
		return 1;

	while (p->text) {
		now = ssa_compare(state, state->param, state->pend,
			p->text, &err);
		if (now) {
			state->param = now;
			line->effect = p->type;
			if (p->param.parselist)
				ret = ssa_parse_xsv(state,
					p->param.parselist, elem, ';');
			ssa_skipws(state, &state->param, state->pend);
			if (state->param != state->pend)
				ssa_add_error(state, best,
					SSAEC_TRAILGB_EFFECT);
			state->param = state->pend;
			return 1;
		}
		if (err > best)
			best = err;
		p++;
	}
	ssa_add_error(state, best, SSAEC_EFFECT_UNRECOG);
	return 0;
}

/** parse "common" lines (Dialogue, Comment, etc.).
 * @param param (lparam) line type
 *   @see ssa_linetype
 * @param elem (points to output) unused
 */
static unsigned ssa_std(struct ssa_state *state, par_t param, void *elem)
{
	struct ssa_line *line = xnewz(struct ssa_line);
	struct ssa_parselist *pl;
	par_t genstr_param;

	line->type = param.lparam;
	line->no = state->lineno;
	*state->output->line_last = line;
	state->output->line_last = &line->next;
	state->sline = line;
	/* TODO: init with sane defaults */

	pl = state->ctx_pl;
	if (!pl) {
		pl = ssa_default_fmt(state, plcommon);
		if (state->ctx == SSACTX_EVENTS) {
			ssa_add_error(state, state->line,
				SSAEC_DEFAULT_LINE_FMT);
			state->ctx_pl = pl;
		}
	}
	if (!ssa_parse_xsv(state, pl, line, ',')) {
		if (!state->ctx_pl)
			free(pl);
		return 0;
	}
	if (!state->ctx_pl)
		free(pl);
	if (state->param == state->end) {
		ssa_add_error(state, state->end, SSAEC_TRUNCLINE);
		return 0;
	}
	if (*state->param != ',') {
		ssa_add_error(state, state->param,
			SSAEC_TRAILGB_PARAM);
		return 0;
	}
	state->param++;
	state->pend = state->end;

	if (param.lparam == SSAL_DIALOGUE)
		return ssa_parsetext(state, line);

#define eoff(e) ((ptrdiff_t) &((struct ssa_line *)0)->e)
	genstr_param.offset = eoff(text);
#undef eoff
	return ssa_genstr(state, genstr_param, line);
}

/** find style with given name.
 * (for external use)
 * @return NULL = not found.
 */
struct ssa_style *ssa_findstyle(struct ssa *output, ssa_string *name)
{
	struct ssa_style *now = output->style_first;
	ptrdiff_t len = (char *)name->e - (char *)name->s;
	while (now) {
		ptrdiff_t nlen = (char *)now->name.e - (char *)now->name.s;
		if (len == nlen && !memcmp(name->s, now->name.s, len))
			return now;
		now = now->next;
	}
	return NULL;
}

/** find style referenced by input file.
 * @param state current state with pointers to style name in param / pend
 * @return found style or NULL
 */
static struct ssa_style *ssa_sstyle_int(struct ssa_state *state)
{
	const ssasrc_t *next, *bestp = NULL, *err;
	struct ssa_style *now = state->output->style_first, *best = NULL;

	if (state->param == state->pend)
		return NULL;

	if (state->param + 1 == state->pend && *state->param == '0') {
		ssa_add_error(state, state->param, SSAEC_R0);
		state->param++;
		return NULL;
	}

	if (*state->param == '*')
		state->param++;

	while (now) {
		err = NULL;
		next = ssa_cmpout(state, state->param, state->pend,
			&now->name, &err);
		if (err > next)
			next = err;
		if (next > bestp) {
			best = now;
			bestp = next;
		}
		now = now->next;
	}
	if (!bestp)
		bestp = state->param;
	ssa_skipws(state, &bestp, state->pend);
	if (bestp != state->pend) {
		ssa_add_error(state, state->param, SSAEC_UNKNSTYLE);
		bestp = state->pend;
	}
	state->param = bestp;
	return best;
}

/** lookup & put style in.
 * longest match counts
 */
static unsigned ssa_sstyle(struct ssa_state *state, par_t param, void *elem)
{
	if (state->anisource)
		ssa_add_error_ext(state, state->param, state->anisource,
			SSAEC_INVAL_ANI);

	*((struct ssa_style **)apply_offset(elem, param.offset)) =
		ssa_sstyle_int(state);
	return 1;
}

/** the brace core.
 * @param param (parselist) CSV content dispatch
 * @param elem passed to param.parselist functions
 * skips opening brace, but leaves closing brace be
 * (perhaps reusable for t and/or clip?)
 */
static inline unsigned ssa_do_brace(struct ssa_state *state, par_t param,
	void *elem)
{
	const ssasrc_t *out_brace, *save_pend;
	unsigned ret;

	/* whitespace should already have been skipped - hrm,
	 * actually we should put in a bit of strictness here
	 *
	 * ssa_do_brace can't be on beginning of line
	 */
	if (ssa_isspace(*(state->param - 1)))
		ssa_add_error(state, state->param, SSAEC_LEADWS_BRACE);
	if (*state->param != '(') {
		ssa_add_error(state, state->param, SSAEC_EXC_BRACE);
		return 0;
	}
	out_brace = ssa_chr(state->param, state->pend, ')');
	if (!out_brace)
		out_brace = state->pend;
	
	save_pend = state->pend;
	state->pend = out_brace;
	state->param++;
	
	if ((ret = ssa_parse_xsv(state, param.parselist, elem, ',')))
		ssa_skipws(state, &state->param, state->pend);

	state->pend = save_pend;
	return ret;
}

/** wrapper for ssa_do_brace */
static unsigned ssa_brace(struct ssa_state *state, par_t param, void *elem)
{
	const ssasrc_t *origin = state->param;
	unsigned ret = ssa_do_brace(state, param, elem);
	if (!ret)
		return 0;
	if (state->param == state->pend) {
		ssa_add_error_ext(state, state->param, origin,
			SSAEC_MISS_BRACE);
		return 1;
	}
	if (*state->param != ')') {
		ssa_add_error(state, state->param, SSAEC_TRAILGB_BRACE);
		return 0;
	}
	state->param++;
	return 1;
}

/** sane end time for moves, other stuff by ssa_brace */
static unsigned ssa_move(struct ssa_state *state, par_t param, void *elem)
{
	par_t brace_param;

	struct ssa_node *node = (struct ssa_node *) elem;
	node->v.move.end = (long)((state->sline->end -
		state->sline->start) * 1000);

	if (state->anisource)
		ssa_add_error_ext(state, state->param, state->anisource,
			SSAEC_INVAL_ANI);

	brace_param.parselist = plmove;
	return ssa_brace(state, brace_param, elem);
}

/** split up stuff that can't be done by ssa_parse_xsv.
 * @param maxcommas number of sections to parse.
 *   (if maxcommas = 4, ssa_do_commas parses 3 commas]
 * @param commas array to be filled with comma positions
 */
static inline unsigned ssa_do_commas(struct ssa_state *state,
	const ssasrc_t *end, const ssasrc_t **commas, unsigned maxcommas)
{
	const ssasrc_t *tmp;
	unsigned commac = 0;

	commas[0] = state->param;
	while (commac < maxcommas) {
		tmp = ssa_chr(commas[commac] + 1, end, ',');
		if (!tmp)
			break;
		commas[++commac] = tmp;
	}
	tmp = commas[commac] + 1;
	ssa_skipws(state, &tmp, end);
	if (tmp != end)
		ssa_add_error(state, tmp, SSAEC_TRAILGB_CSVBRACE);
	return commac;
}

/** ssa_styleex - parse StyleEx: line.
 * @param param unused
 * @param elem (points to output) unused
 */
static unsigned ssa_styleex(struct ssa_state *state, par_t param, void *elem)
{
	struct ssa_style *basestyle, *style;
	const ssasrc_t *commas[3], *end;
	unsigned ncommas;

	ssa_skipws(state, &state->param, state->end);
	end = ssa_chr(state->param, state->end, '\\');
	ncommas = ssa_do_commas(state, end ? end : state->end, commas, 4);
	if (!ncommas) {
		ssa_add_error(state, state->end, SSAEC_TRUNCLINE);
		return 0;
	}

	state->param = commas[1] + 1;
	state->pend = commas[2];
	basestyle = ssa_sstyle_int(state);
	if (!basestyle) {
		ssa_add_error(state, state->param, SSAEC_UNKNSTYLE);
		return 0;
	}
	style = xnew(struct ssa_style);
	memcpy(style, basestyle, sizeof(struct ssa_style));
	ssa_src2str(state, commas[0], commas[1], &style->name);
	ssa_strdup(&style->fontname, &basestyle->fontname);
	style->next = NULL;
	style->base = basestyle;
	style->node_first = NULL;
	style->node_last = &style->node_first;
	*state->output->style_last = style;
	state->output->style_last = &style->next;

	if (ncommas == 1) {
		ssa_add_error(state, state->end, SSAEC_TRUNCLINE);
		return 0;
	}
	state->param = commas[2] + 1;
	state->pend = state->end;
	if (!ssa_parseoverr(state, &style->node_last))
		return 0;
	if (state->param == state->end)
		return 1;
	ssa_add_error(state, state->param, SSAEC_TRAILGB_LINE);
	return 0;
}

/** read in start & end time for t */
static inline void ssa_t_do_times(struct ssa_state *state,
	const ssasrc_t **commas, struct ssa_node *node)
{
	unsigned c;
	struct ssa_state scannum_state;
	ssasrc_t buf[12]; /* -2147483648\0 */
	ssasrc_t *bptr, *endptr;
	memcpy(&scannum_state, state, sizeof(struct ssa_state));

	for (c = 0; c < 2; c++) {
		scannum_state.param = commas[c] + 1;
		scannum_state.pend = commas[c + 1];
		ssa_skipws(state, &scannum_state.param, scannum_state.pend);
		if (ssa_scannum(&scannum_state, buf, &bptr, 0, 1, 12)) {
			node->v.t.times[c] = ssatol(buf, &endptr, 10);
			if (bptr != endptr)
				ssa_add_error(state, scannum_state.param,
					SSAEC_INVAL_TIME);
			if (scannum_state.param != commas[c + 1])
				ssa_add_error(state, scannum_state.param,
					SSAEC_TRAILGB_TIME);
		}
	}
}

/** read in t acceleration */
static inline void ssa_t_do_accel(struct ssa_state *state,
	const ssasrc_t **commas, struct ssa_node *node)
{
	struct ssa_state scannum_state;
	ssasrc_t buf[32];
	ssasrc_t *bptr, *endptr;
	memcpy(&scannum_state, state, sizeof(struct ssa_state));

	scannum_state.param = commas[0] + 1;
	scannum_state.pend = commas[1];
	ssa_skipws(state, &scannum_state.param, scannum_state.pend);
	if (ssa_scannum(&scannum_state, buf, &bptr, 1, 1, 32)) {
		node->v.t.accel = ssatod(buf, &endptr);
		if (bptr != endptr)
			ssa_add_error(state, scannum_state.param,
				SSAEC_INVAL_ACCEL);
		if (scannum_state.param != commas[1])
			ssa_add_error(state, scannum_state.param,
				SSAEC_TRAILGB_ACCEL);
	}
}

/** some more heavy code, animation parsing.
 *  state->pend points to the end of the current {} block, not the closing
 *  brace
 */
static unsigned ssa_t(struct ssa_state *state, par_t param, void *elem)
{
	struct ssa_node *node = (struct ssa_node *) elem;
	const ssasrc_t *commas[4], *pstart, *cbrace, *origin = state->param;
	unsigned commac;

	if (state->anisource)
		ssa_add_error_ext(state, state->param, state->anisource,
			SSAEC_INVAL_ANI);

	node->v.t.node_last = &node->v.t.node_first;
	node->v.t.accel = 1.0;
	
	pstart = ssa_chr(state->param, state->pend, '\\');
	if (!pstart)
		pstart = state->pend;
	cbrace = ssa_chr(state->param, pstart, ')');
	if (cbrace)
		pstart = cbrace;
	commac = ssa_do_commas(state, pstart, commas, 4);
	state->param = pstart;
	node->v.t.flags = commac;
	if (commac & 2)
		ssa_t_do_times(state, commas, node);
	if (commac & 1)
		ssa_t_do_accel(state, commas + (commac & 2), node);

	state->anisource = origin;
	if (!ssa_parseoverr(state, &node->v.t.node_last)) {
		state->anisource = NULL;
		return 0;
	}
	state->anisource = NULL;

	/* whitespace already skipped */
	if (state->param == state->pend) {
		ssa_add_error_ext(state, state->param, origin,
			SSAEC_MISS_BRACE);
		return 1;
	}
	if (*state->param == ')') {
		state->param++;
		return 1;
	}
	ssa_add_error(state, state->param,
		SSAEC_TRAILGB_BRACE);
	state->param = ssa_chr(state->param, state->pend, ')');
	if (state->param)
		state->param++;
	else
		state->param = state->pend;
	return 0;
}

/** read clips, currently only rectangular ones though.
 */
static unsigned ssa_clip(struct ssa_state *state, par_t param, void *elem)
{
	const ssasrc_t *end, *c1, *c2;
	par_t brace_param;

	end = ssa_chr(state->param, state->pend, ')');
	if (!end) {
		ssa_add_error(state, state->param, SSAEC_MISS_BRACE);
		end = state->pend;
	}
	c1 = ssa_chr(state->param, end, ',');
	if (c1) {
		c2 = ssa_chr(c1 + 1, end, ',');
		if (c2) {
			brace_param.parselist = plclip;
			return ssa_brace(state, brace_param, elem);
		}
	}
	ssa_add_error(state, state->param, SSAEC_NOTSUP);
	if (end < state->pend)
		end++;
	state->param = end;
	return 0;
}

/** generic hex validator.
 * @param bptr bailout pointer
 * @param maxlen buf size
 */
static inline unsigned ssa_scanhex(struct ssa_state *state, ssasrc_t *buf,
	ssasrc_t **bptr, unsigned maxlen)
{
	int html = state->param[0] == '#';
	*bptr = buf;
	if (state->param == state->pend
		|| (state->param[0] != '&' && !html)) {
		ssa_add_error(state, state->param, SSAEC_EXC_AMPERSAND);
		return 0;
	}
	if (!html) {
		state->param++;
		if (state->param == state->pend
			|| ssa_toupper(state->param[0]) != 'H') {
			ssa_add_error(state, state->param, SSAEC_EXC_COLOUR);
			return 0;
		}
	}
	state->param++;
		
	/* overrun neither line nor buf */
	while (state->param < state->pend && *bptr < buf + maxlen) {
		if (!ssa_isxdigit(*state->param))
			break;
		*(*bptr)++ = *state->param++;
	}
	/* nothing in buf? */
	if (*bptr == buf) {
		ssa_add_error(state, state->param, SSAEC_EXC_NUM);
		if (*state->param == '&')
			state->param++;
		return 0;
	}
	if (*bptr == buf + maxlen) {
		ssa_add_error(state, state->param, SSAEC_NUM_2LONG);
		return 0;
	}
	**bptr = '\0';
	if (html)
		return 1;
	if (*state->param != '&')
		ssa_add_error(state, state->param,
			(*bptr - buf == (ptrdiff_t)maxlen - 1)
			|| (maxlen == 9 && *bptr - buf == 6)
			? SSAEC_EXC_AMPERSAND
			: SSAEC_TRAILGB_COLOUR);
	else
		state->param++;
	return 1;
}

#define VB_BEGONE do { onetmp = result.c.r; result.c.r = result.c.b; \
	result.c.b = onetmp; } while (0)
/** read (hex or dec) colour-like value */
static unsigned ssa_do_colour(struct ssa_state *state, colour_t *ret,
	int digits)
{
	colour_one_t onetmp;
	colour_t result;
	ssasrc_t buf[12]; /* -2147483648 */
	ssasrc_t *bptr, *endptr;
	const ssasrc_t *origin = state->param;

	if (state->param == state->pend) {
		ssa_add_error(state, state->param, SSAEC_EXC_COLOUR);
		return 0;
	}
	if (state->param[0] == '&') {
		if (!ssa_scanhex(state, buf, &bptr,
			digits == 6 ? 9 : digits + 1))
			return 0;
		if (bptr - buf != digits)
			ssa_add_error_ext(state, origin, state->param,
				(digits == 6) && (bptr - buf == 8)
				? SSAEC_COLOUR_ALPHALOST
				: SSAEC_COLOUR_STRANGE);
		result.l = ssatoul(buf, &endptr, 16);
		if (digits != 2)
			VB_BEGONE;
	} else if (state->param[0] == '#') {
		if (!ssa_scanhex(state, buf, &bptr,
			digits == 6 ? 9 : digits + 1))
			return 0;
		if (bptr - buf != digits)
			ssa_add_error_ext(state, origin, state->param,
				(digits == 6) && (bptr - buf == 8)
				? SSAEC_COLOUR_ALPHALOST
				: SSAEC_COLOUR_STRANGE);
		result.l = ssatoul(buf, &endptr, 16);
		if (digits == 8 || (digits == 6 && bptr - buf == 8))
			result.l = (result.l >> 8) | (result.l << 24);
	} else {
		if (!ssa_scannum(state, buf, &bptr, 0, 1, 12))
			return 0;
		result.l = ssatoul(buf, &endptr, 10);
		if (digits != 2)
			VB_BEGONE;
	}
		
	if (bptr != endptr || (digits == 2 && result.l >= 0x100)) {
		ssa_add_error(state, state->param, SSAEC_NUM_INVAL);
		return 0;
	}
	ret->l = result.l;

	return 1;
}

/** colour: ASS 8-digit style */
static unsigned ssa_colalp(struct ssa_state *state,
	par_t param, void *elem)
{
	return ssa_do_colour(state,
		((colour_t *)apply_offset(elem, param.offset)), 8);
}

/** colour: SSA 6-digit style */
static unsigned ssa_colour(struct ssa_state *state, par_t param, void *elem)
{
	return ssa_do_colour(state,
		((colour_t *)apply_offset(elem, param.offset)), 6);
}

/** alpha: 2 hex digits */
static unsigned ssa_alpha(struct ssa_state *state, par_t param, void *elem)
{
	colour_t c;
	if (!ssa_do_colour(state, &c, 2))
		return 0;
	*((alpha_t *)apply_offset(elem, param.offset)) = (alpha_t)c.l;
	return 1;
}

/** call ssa_parsefunc for output */
static inline void ssa_main_call(struct ssa_state *s,
	struct ssa_parsetext_ctx *p)
{
	unsigned ret = p->param.lparam;
	s->pend = s->end;
	if (!(s->ctx & p->ctx)) {
		if (s->ctx == SSACTX_INIT) {
			ssa_add_error(s, s->line, SSAEC_RAW_CTX);
			s->ctx = SSACTX_RAW;
		} else
			ssa_add_error(s, s->line, SSAEC_INVALID_CTX);
	}
	if (p->func)
		ret = p->func(s, p->param, s->output);
	if (ret) {
		ssa_skipws(s, &s->param, s->end);
		if (s->param != s->end)
			ssa_add_error(s, s->param, SSAEC_TRAILGB_LINE);
	}
}

/** main lexer function.
 * @param output result, must be allocated prior to calling, will be zeroed
 * @param data script input, possibly mmap'ed (is never written to)
 * @param datasize size of data
 */
int ssa_lex(struct ssa *output, const void *data, size_t datasize)
{
	struct ssa_state s;
	const char *srccs = NULL;
	const unsigned char *dc = (const unsigned char *)data;
	const char *csrc = NULL, *cend = NULL;
	char *freeme = NULL;
	size_t csrcsize = datasize;
	char *oldlocale_ctype, *oldlocale_numeric;

	if (datasize < 4)
		return 1;

	oldlocale_ctype = setlocale(LC_CTYPE, "C");
	oldlocale_numeric = setlocale(LC_NUMERIC, "C");

	s.output = output;
	s.elast = &output->errlist;
	s.nerr = 0;
	s.lineno = 0;
	s.anisource = NULL;
	s.ctx = SSACTX_INIT;
	s.ctx_pl = NULL;

	memset(output, 0, sizeof(struct ssa));
	output->line_last = &output->line_first;
	output->style_last = &output->style_first;

	s.ic_srccs = "UTF-8";
	s.unicode = 1;

	if (dc[0] == 0xff && dc[1] == 0xfe && dc[2] == 0x00 && dc[3] == 0x00)
		srccs = "UCS-4LE";
	else if (dc[0] == 0xff && dc[1] == 0xfe)
		srccs = "UCS-2LE";
	else if (dc[0] == 0x00 && dc[1] == 0x00 && dc[2] == 0xfe && dc[3] == 0xff)
		srccs = "UCS-4BE";
	else if (dc[0] == 0xfe && dc[1] == 0xff)
		srccs = "UCS-2BE";
	else if (dc[0] == 0xef && dc[1] == 0xbb && dc[2] == 0xbf) {
		csrc = (const char *)data;
		cend = csrc + datasize;
	} else {
		csrc = (const char *)data;
		cend = csrc + datasize;
		s.unicode = 0;
		s.ic_srccs = "MS-ANSI";	/* "DEFAULT" */
	}

	if (srccs) {
		size_t outleft = datasize, inleft = datasize;
		ptrdiff_t outnowd = 0;

		iconv_t preconv = iconv_open("UTF-8", srccs);
		ICONV_CONST char *curconv = (ICONV_CONST char *)data;
		char *endpos;

		do {
			csrcsize += 1024;
			outleft += 1024;

			freeme = (char *)xrealloc(freeme, csrcsize);
			endpos = freeme + outnowd;

			iconv_errno = 0;
			iconv(preconv, &curconv, &inleft, &endpos, &outleft);

			outnowd = cend - freeme;
		} while (iconv_errno == E2BIG);
		iconv_close(preconv);
		if (iconv_errno || inleft) {
			free(freeme);
			setlocale(LC_CTYPE, oldlocale_ctype);
			setlocale(LC_NUMERIC, oldlocale_numeric);
			return 2;
		}
		csrc = freeme;
		cend = endpos;
	}

	if ((unsigned char)csrc[0] == 0xef
		&& (unsigned char)csrc[1] == 0xbb
		&& (unsigned char)csrc[2] == 0xbf)

		csrc += 3;

	s.ic_srcout = iconv_open(SSA_DESTCS, "UTF-8");

	do {
		struct ssa_parsetext_ctx *pnow = ptkeys;
		const ssasrc_t *best_match = NULL, *now;
		
		const ssasrc_t *lend = ssa_chr(csrc, cend, '\xA');
		if (!lend)
			lend = cend;
		s.end = (*(lend - 1) == '\xD') ? lend - 1 : lend;
		s.lineno++;
		s.line = now = csrc;

		ssa_skipws(&s, &now, s.end);
		if (now == s.end || *now == ';' || *now == '!')
			goto cont;

		while (pnow->text) {
			const ssasrc_t *err = NULL;
			if ((s.param = ssa_compare(&s, now, s.end, pnow->text,
				&err))) {
				ssa_main_call(&s, pnow);
				break;
			}
			if (err > best_match)
				best_match = err;
			pnow++;
		};
		if (!pnow->text)
			ssa_add_error(&s, best_match, SSAEC_PARSEERROR);

cont:
		csrc = lend + 1;
	} while (csrc < cend);

	iconv_close(s.ic_srcout);
	if (freeme)
		free(freeme);
	if (s.ctx_pl)
		free(s.ctx_pl);
	setlocale(LC_CTYPE, oldlocale_ctype);
	setlocale(LC_NUMERIC, oldlocale_numeric);
	return 0;
}

/** stream packet lexer.
 * @param output result, must be allocated prior to calling, will be zeroed
 * @param data script input, possibly mmap'ed (is never written to)
 * @param datasize size of data
 *
 * WARNING: this resets output->line_first and output->line_last!
 */
void ssa_lex_packet(struct ssa *output, const void *data, size_t datasize)
{
	struct ssa_state s;
	const char *csrc = (const char *)data, *cend = csrc + datasize;
	char *oldlocale_ctype, *oldlocale_numeric;

	oldlocale_ctype = setlocale(LC_CTYPE, "C");
	oldlocale_numeric = setlocale(LC_NUMERIC, "C");

	s.output = output;
	s.output->line_first = NULL;
	s.output->line_last = &s.output->line_first;
	s.elast = NULL;
	s.nerr = 0;
	s.lineno = 0;
	s.anisource = NULL;
	s.ctx = SSACTX_PACKET;
	s.ctx_pl = plpacket;
	s.ic_srccs = "UTF-8";
	s.ic_srcout = iconv_open(SSA_DESTCS, "UTF-8");
	s.unicode = 1;

	do {
		const ssasrc_t *lend = ssa_chr(csrc, cend, '\xA'), *now;
		if (!lend)
			lend = cend;
		if (lend > csrc)
			s.end = (*(lend - 1) == '\xD') ? lend - 1 : lend;
		s.line = now = csrc;

		ssa_skipws(&s, &now, s.end);
		if (now != s.end && *now != ';' && *now != '!')
			ssa_main_call(&s, &ptkeys[PTKEYS_PACKET]);
		csrc = lend + 1;
	} while (csrc < cend);

	iconv_close(s.ic_srcout);
	setlocale(LC_CTYPE, oldlocale_ctype);
	setlocale(LC_NUMERIC, oldlocale_numeric);
	return;
}

/** free a ssa_string.
 * @param str the string to be freed.
 */
static void ssa_freestr(ssa_string *str)
{
	if (!str->s)
		return;
	xfree(str->s);
	str->s = str->e = NULL;
}

/** free a chain of ssa_nodes.
 * @param nodes first node to be freed. all linked nodes
 *  (next / v.t.node_first) are freed as well.
 */
static void ssa_freenodes(struct ssa_node *nodes)
{
	struct ssa_node *node, *nnext;
	for (node = nodes; node; node = nnext) {
		switch (node->type) {
		case SSAN_TEXT:
			ssa_freestr(&node->v.text);
			break;
		case SSAN_T:
			ssa_freenodes(node->v.t.node_first);
			break;
		default:
			;
		}
		nnext = node->next;
		xfree(node);
	}
}

/** release all resources attached to output.
 * @param output the lexer to be freed. does not free *output itself
 *  since ssa_lex didn't allocate it
 *  (so you can use a local variable / put it in a struct / ...)
 *  @see ssa_lex
 */
void ssa_free(struct ssa *output)
{
	struct ssa_style *style, *snext;
	struct ssa_line *line, *lnext;
	struct ssa_error *err, *errnext;
	struct ssa_parsetext_ctx *pt;

	for (line = output->line_first; line; line = lnext) {
		ssa_freenodes(line->node_first);
		ssa_freestr(&line->ssa_marked);
		ssa_freestr(&line->name);
		ssa_freestr(&line->text);
		lnext = line->next;
		xfree(line);
	}

	for (style = output->style_first; style; style = snext) {
		ssa_freestr(&style->name);
		ssa_freestr(&style->fontname);
		snext = style->next;
		xfree(style);
	}

	for (pt = ptkeys; pt->text; pt++)
		if (pt->func == ssa_genstr)
			ssa_freestr((ssa_string *)apply_offset(output,
				pt->param.offset));

	for (err = output->errlist; err; err = errnext) {
		xfree(err->textline);
		errnext = err->next;
		xfree(err);
	}
}

