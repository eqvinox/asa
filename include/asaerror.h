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
/** @file asaerror.h - asa error codes */

#ifndef _ASAERROR_H
#define _ASAERROR_H

#include "ssa.h"
#include "ssavm.h"

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
	SSAEC_EXC_BOOL,			/**< expecting boolean value */
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
	SSAEC_RAW_CTX,			/**< no script header */
	SSAEC_INVALID_CTX,		/**< invalid command for cur ctx */
	SSAEC_DEFAULT_STYLE_FMT,	/**< style Format: missing */
	SSAEC_DEFAULT_LINE_FMT,		/**< line Format: missing */
	SSAEC_NONSTANDARD_FMT,		/**< non-standard Format: */
	SSAEC_REPEATED_FMT,		/**< repeated/late Format: */
	SSAEC_UNKNOWN_COLUMN,		/**< unknown column in Format: */
	SSAEC_COLUMN_VERSION,		/**< wrong version Format: column */
	SSAEC_TEXT_NOT_LAST,		/**< event Format: Text not last */
	SSAEC_FP_COORD,			/**< floating point coordinates */

	SSAEC_MAX
};

enum ssav_errc {
	SSAVEC_NOTSUP = SSAEC_MAX,	/**< not supported by VM */
	SSAVEC_FONTNX,			/**< font not found */
	SSAVEC_NOANIM,			/**< non-animatable element in t */
	SSAVEC_MAX
};

#define ASAEC_MAX SSAVEC_MAX

/** error strings */
struct asaec_desc {
	int sev;			/**< severity */
	char *sh;			/**< short description */
	char *add;			/**< long (additional description) */
	char *warn;			/**< warning text */
};
extern struct asaec_desc ssaec[ASAEC_MAX];	/**< error text table */

#define SSA_ENOORIGIN (~0UL)
/** SSA lexer error message */
struct ssa_error {
	struct ssa_error *next;
	unsigned lineno;		/**< one-based line number */
	unsigned column;		/**< zero-based column number */
	ssasrc_t *textline;		/**< line. free() this! */
	unsigned linelen;
	unsigned origin;		/**< zero-based error origin */
	enum ssa_errc errorcode;
};

/** SSA VM error context */
enum ssav_ectx {
	SSAVECTX_UNKNOWN = 0,		/**< no known context */
	SSAVECTX_DIALOGUE,		/**< dialogue -
					 * lexline and lineno valid */
	SSAVECTX_STYLE			/**< style -
					 * style and stylename valid */
};

/** SSA VM error message.
 * make sure lexer hasn't been freed before accessing ssa_* stuff! */
struct ssav_error {
	struct ssav_error *next;
	enum ssav_errc errorcode;
	enum ssav_ectx context;
	union {
		struct {
			/** one-based line number */
			unsigned lineno;
			/** lexer line reference */
			struct ssa_line *lexline;
			struct ssa_node *node;
			enum ssa_nodetype ntype;
			struct ssav_line *line;
		} dlg;
		struct {
			/** lexer style reference */
			struct ssa_style *style;
			/** style name.
			 * style name, encoded in UTF-8; needs to be freed */
			char *stylename;
		} style;
	} i;
	char *ext_str;			/**< extended information.
					 * UTF-8, needs to be freed. */
};

#endif /* _ASAERROR_H*/

