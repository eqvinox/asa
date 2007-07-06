/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007  David Lamparter
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

#ifndef _IMPORTS_H
#define _IMPORTS_H

#include <pcre.h>

/** import instruction code */
enum asa_import_insn_type {
	ASAI_COMMIT = 0,	/**< put current buffer as line */
	ASAI_DISCARD,		/**< discard current buffer */
	ASAI_BREAK,		/**< leave child group */

	ASAI_SELECT,		/**< select source matchgroup */
	ASAI_SG,		/**< execute search & replace on selected */
	ASAI_SGU,		/**< like ASAI_SG, but update matchgroups */
	ASAI_APPEND,		/**< append selected source to buffer */

	ASAI_FPS,		/**< set fixed fps */
	ASAI_SHOW,		/**< set start time */
	ASAI_HIDE,		/**< set end time */

	ASAI_CHILD		/**< child regex match */
};

/** replacement element for ASAI_SG / ASAI_SGU */
struct asa_repl {
	struct asa_repl *next;	/**< next element */

	int group;		/**< -1 if fixed string, else source group */
	char *text;		/**< fixed string for group == -1 */
};

/** time specification element */
struct asa_tspec {
	struct asa_tspec *next;	/**< next element */

	int group;		/**< source matchgroup */
	double	mult,		/**< ts multiplier. 1.0 = 1 second */
		fps_mult;	/**< fps multiplier. probably 0.0 or 1.0 */
};

/** single import instruction */
struct asa_import_insn {
	struct asa_import_insn
		*parent,	/**< owner insn, NULL if format child */
		*next;		/**< next instruction */

	enum asa_import_insn_type insn;	/**< instruction code */
	/** instruction parameters */
	union {
		int break_depth;		/**< depth to break out */
		int select;			/**< matchgroup to select */
		/** search-replace parameters */
		struct {
			pcre *regex;		/**< search for */
			struct asa_repl *repl;	/**< replace by */
		} sg;
		/** time specification */
		struct {
			struct asa_tspec *tsp;	/**< sources to sum up */
			int delta_select;	/**< -1 or delta index */
		} tspec;
		/** child instructions */
		struct {
			pcre *regex;		/** <if this matches... */
			struct asa_import_insn *insns;	/**< do this. */
		} child;
		double fps_value;		/**< fixed fps value */
	} v;
};

/** destination format of import rules */
enum asa_import_target {
	ASAI_TARGET_TEXT = (1 << 0),		/**< plain text packets */
	ASAI_TARGET_SSA = (1 << 1)		/**< MKV ASS packets */
};

/** importing instructions for format */
struct asa_import_format {
	struct asa_import_format *next,		/**< next format */
		*nexttgt;			/**< next target of this*/

	char *name;				/**< format name */

	enum asa_import_target target;		/**< target */
	struct asa_import_insn *insns;		/**< instructions */
};

/** format detection rules */
struct asa_import_detect {
	struct asa_import_detect *next;		/**< next rule */

	pcre *re;				/**< if this matches... */

	char *name;				/**< use this format */
	struct asa_import_format *fmt;		/**< through this pointer */

};

#define MAXDELTA	4	/**< nr of times kept for delta backref */
#define MAXGROUP	24	/**< maximum number of regex match groups */

/** state of a running import */
struct asa_import_state {
	char *line;			/**< beginning of current line */

	char **matches;			/**< active matchgroups */
	int	nmatches,		/**< number of matchgroups */
		selected;		/**< currently active matchgroup */
	char *out;			/**< output buffer (NULL if empty) */

	double start,			/**< start time */
		end;			/**< end time */
	double delta[MAXDELTA];		/**< hist of last times for delta */
};

extern struct asa_import_detect *asa_det_first, **asa_det_last;
extern struct asa_import_format *asa_fmt_first, **asa_fmt_last;

#endif
