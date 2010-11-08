/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007  David Lamparter
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

#ifndef _IMPORTS_H
#define _IMPORTS_H

#include <pcre.h>

/** parser reuse. imports-to-C uses char *, real VM uses pcre * */
typedef union {
	pcre *pcregex;
	char *str;
} asa_pcre;

/** pcre_compile wrapper.
 * used to allow loading differently for imports-to-C conversion.
 * @param out output (parsed pcre)
 * @param str regular expression string
 * @return 0 on success, any other on error
 */
extern int asa_pcre_compile(asa_pcre *out, const char *str);

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
			asa_pcre regex;		/**< search for */
			struct asa_repl *repl;	/**< replace by */
		} sg;
		/** time specification */
		struct {
			struct asa_tspec *tsp;	/**< sources to sum up */
			int delta_select;	/**< -1 or delta index */
		} tspec;
		/** child instructions */
		struct {
			asa_pcre regex;		/** <if this matches... */
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
		*prevtgt,			/**< previous target */
		*nexttgt;			/**< next target of this*/

	char *name;				/**< format name */

	enum asa_import_target target;		/**< target */
	struct asa_import_insn *insns;		/**< instructions */
};

/** format detection rules */
struct asa_import_detect {
	struct asa_import_detect *next;		/**< next rule */

	asa_pcre re;				/**< if this matches... */

	char *name;				/**< use this format */
	struct asa_import_format *fmt;		/**< through this pointer */

};

extern struct asa_import_detect *asa_det_first, **asa_det_last;
extern struct asa_import_format *asa_fmt_first, **asa_fmt_last;

typedef int (asa_import_callback)(void *arg, double start, double stop,
	const char *buffer, size_t buffer_length);

extern struct asa_import_detect *asa_imports_detect(const void *data,
	size_t dlen);
extern int asa_import(const void *data, size_t dlen, double fps,
	struct asa_import_detect *det, enum asa_import_target target,
	asa_import_callback *callback, void *arg);
extern void asa_init_import();

#endif
