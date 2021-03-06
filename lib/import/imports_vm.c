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

#ifdef HAVE_CONFIG_H
#include "acconf.h"
#endif

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <subhelp.h>

#include "common.h"
#include "imports.h"

#define MAXDELTA	4	/**< nr of times kept for delta backref */
#define MAXGROUP	24	/**< maximum number of regex match groups */

/** state of a running import */
struct asa_import_state {
	const char *line;		/**< beginning of current line */
	size_t remain;			/**< remaining data at line */

	char **matches;			/**< active matchgroups */
	unsigned nmatches;		/**< number of matchgroups */
	char *selstr;			/**< buffer for currently selected */
	size_t sellen;			/**< strlen of selstr */
	char *out;			/**< output buffer (NULL if empty) */
	size_t outlen;			/**< length of output string w/o \0 */

	double fps_app,			/**< application-supplied 1/fps */
	       fps_rec,			/**< 1/fps value used for calcs */
		start,			/**< start time */
		end;			/**< end time */
	double delta[MAXDELTA];		/**< hist of last times for delta */

	asa_import_callback *cb;	/**< commit callback */
	void *cb_arg;			/**< callback argument */
};

#define iargs struct asa_import_state *state, struct asa_import_insn *insn
/** asa instruction function.
 * @param state import state
 * @param insn instruction to execute
 * @return status code.\n
 *   0: continue
 *   -1: restart from beginning
 *   >0: break level
 */
typedef int (*asa_import_func)(iargs);

static int asai_commit (iargs);
static int asai_discard(iargs);
static int asai_break  (iargs);
static int asai_select (iargs);
static int asai_sg     (iargs);
static int asai_sgu    (iargs);
static int asai_append (iargs);
static int asai_fps    (iargs);
static int asai_show   (iargs);
static int asai_hide   (iargs);
static int asai_child  (iargs);
#undef iargs

/** vm functions. KEEP IN SYNC WITH imports.h! */
static asa_import_func importfuncs[] = {
	asai_commit,
	asai_discard,
	asai_break,
	asai_select,
	asai_sg,
	asai_sgu,
	asai_append,
	asai_fps,
	asai_show,
	asai_hide,
	asai_child
};
#define ASAI_MAX (unsigned)(sizeof(importfuncs) / sizeof(importfuncs[0]))

struct asa_import_detect *asa_det_first = NULL,
	**asa_det_last = &asa_det_first;
struct asa_import_format *asa_fmt_first = NULL,
	**asa_fmt_last = &asa_fmt_first;

/** asa_imports_crosslink - resolve references in imports file.
 * updates asa_import_format.prevtgt, asa_import_format.nexttgt,
 * asa_import_detect.fmt
 */
static void asa_imports_crosslink()
{
	struct asa_import_format *fmt, *fmt2;
	struct asa_import_detect *det;

	for (fmt = asa_fmt_first; fmt; fmt = fmt->next) {
		for (fmt2 = fmt->next; fmt2; fmt2 = fmt2->next)
			if (!strcmp(fmt->name, fmt2->name)) {
				fmt->nexttgt = fmt2;
				fmt2->prevtgt = fmt;
				break;
			}
	}

	for (det = asa_det_first; det; det = det->next) {
		det->fmt = NULL;
		for (fmt = asa_fmt_first; fmt; fmt = fmt->next)
			if (!strcmp(det->name, fmt->name)) {
				det->fmt = fmt;
				break;
			}
	}
}

/** asa_imports_detect - autodetect subtitle format.
 * @param data pointer to subtitle byte data
 * @param datalen byte length of data
 * @return the detect structure that hit or NULL if detection failed
 */
struct asa_import_detect *asa_imports_detect(const void *data, size_t dlen)
{
	struct asa_import_detect *det;
	const char *d = (const char *)data;
	int v[64];

	if (dlen > 2048)
		dlen = 2048;
	for (det = asa_det_first; det; det = det->next)
		if (pcre_exec(det->re.pcregex, NULL, d, dlen, 0, 0, v, 64) >= 0)
			return det;
	return NULL;
}

/** asai_run_insns - execute a list of instructions.
 * @see asa_import_func */
static int asai_run_insns(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	struct asa_import_insn *inow = insn, preload;
	int rv, repeating = 0;

	preload.next = insn;
	for (; inow; inow = inow->next) {
		if (repeating && inow->insn != ASAI_CHILD)
			continue;
		if (inow->insn >= ASAI_MAX)
			continue;
		rv = importfuncs[inow->insn](state, inow);
		if (rv == -1) {
			inow = &preload;
			continue;
		}
		if (rv > 0)
			return rv - 1;
	}
	/* ran through everything, let's try another round */
	return -1;
}

/** asai_commit - commit a block to the user.
 * @see asa_import_func */
static int asai_commit(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	int rv = 0;

	if (!state->out)
		return 0;
	if (state->outlen > 0)
		rv = state->cb(state->cb_arg, state->start, state->end,
			state->out, state->outlen);
	xfree(state->out);
	state->out = NULL;
	state->outlen = 0;
	if (rv)
		return INT_MAX;
	return 0;
}

/** asai_discard - clear the buffer without committing.
 * @see asa_import_func */
static int asai_discard(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	if (!state->out)
		return 0;
	xfree(state->out);
	state->out = NULL;
	state->outlen = 0;
	return 0;
}

/** asai_break - jump out of child.
 * @see asa_import_func */
static int asai_break(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	return insn->v.break_depth;
}

/** asai_select - choose a match group to be the active one.
 * @see asa_import_func */
static int asai_select (struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	if (insn->v.select < 0
		|| (unsigned)insn->v.select >= state->nmatches) {
		subhelp_log(CSRI_LOG_ERROR, "import script trying to "
			"reference group %d, maximum is %d",
			insn->v.select, state->nmatches);
		return 0;
	}
	if (state->selstr)
		xfree(state->selstr);
	state->selstr = xstrdup(state->matches[insn->v.select]);
	state->sellen = strlen(state->selstr);
	return 0;
}

static ptrdiff_t asai_process_replace(struct asa_import_state *state,
	struct asa_import_insn *insn, int *v, int rv)
{
	struct asa_repl *r;
	char *newstr;
	ptrdiff_t newpos, firstold;
	size_t newstr_size;

	newstr_size = v[0] * 2;
	newstr = (char *)xmalloc(newstr_size);
	memcpy(newstr, state->selstr, v[0]);
	newpos = v[0];

	for (r = insn->v.sg.repl; r; r = r->next) {
		size_t avail = newstr_size - newpos, need;
		const char *src;

		if (r->group >= rv) {
			subhelp_log(CSRI_LOG_ERROR,
				"import script trying to replace by "
				"reference group %d, maximum is %d",
				r->group, rv);
			continue;
		}
		if (r->group >= 0) {
			need = v[r->group * 2 + 1] - v[r->group * 2];
			src = state->selstr + v[r->group * 2];
		} else {
			need = strlen(r->text);
			src = r->text;
		}
		if (need > avail) {
			newstr_size += need - avail + 256;
			newstr = (char *)xrealloc(newstr, newstr_size);
		}
		memcpy(newstr + newpos, src, need);
		newpos += need;
	}
	firstold = newpos;
	newstr_size = newpos + state->sellen - v[1];
	newstr = (char *)xrealloc(newstr, newstr_size + 1);
	memcpy(newstr + newpos, state->selstr + v[1],
		state->sellen - v[1] + 1);
	state->selstr = newstr;
	state->sellen = newstr_size;
	return firstold;
}

/** asai_sg - search and replace.
 * @see asa_import_func */
static int asai_sg(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	int rv, v[MAXGROUP * 2];
	char *oldstr;
	ptrdiff_t s = 0;

	if (!state->selstr)
		return 0;
	while ((unsigned)s < state->sellen &&
		(rv = pcre_exec(insn->v.sg.regex.pcregex, NULL, state->selstr,
			state->sellen, s, 0, v, MAXGROUP * 2)) >= 0) {
		oldstr = state->selstr;
		s = asai_process_replace(state, insn, v, rv);
		xfree(oldstr);
	}
	return 0;
}

/** asai_chunk_alloc - allocate composite chunk.
 * @see asa_import_func */
static inline char **asai_chunk_alloc(char **old, int *v, int rv)
{
	size_t s = rv * sizeof(char *);
	int i;
	for (i = 0; i < rv; i++)
		s += v[i * 2 + 1] - v[i * 2] + 1;
	return (char **)xrealloc(old, s);
}

/** asai_set_matches - load result from pcre_exec into matches */
static void asai_set_matches(struct asa_import_state *state,
	const char *src, int *v, int rv)
{
	unsigned i;
	char *dst;

	state->matches = asai_chunk_alloc(state->matches, v, rv);
	state->nmatches = rv;
	dst = (char *)(state->matches + rv);
	for (i = 0; i < state->nmatches; i++) {
		size_t len = v[2 * i + 1] - v[2 * i];
		state->matches[i] = dst;
		memcpy(dst, src + v[2 * i], len);
		dst[len] = '\0';
		dst += len + 1;
	}
	if (state->selstr)
		xfree(state->selstr);
	state->selstr = xstrdup(state->matches[0]);
	state->sellen = strlen(state->selstr);
}

/** asai_sgu - replace one time and update matches.
 * @see asa_import_func */
static int asai_sgu(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	int rv, v[MAXGROUP * 2];
	char *oldstr;

	if (!state->selstr)
		return 0;
	if ((rv = pcre_exec(insn->v.sg.regex.pcregex, NULL, state->selstr,
			state->sellen, 0, 0, v, MAXGROUP * 2)) >= 0) {
		oldstr = state->selstr;
		asai_process_replace(state, insn, v, rv);

		asai_set_matches(state, oldstr, v, rv);
		xfree(oldstr);
	}
	return 0;
}

/** asai_append - append selected string to output buffer.
 * @see asa_import_func */
static int asai_append (struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	state->out = (char *)xrealloc(state->out,
		state->outlen + state->sellen + 1);
	memcpy(state->out + state->outlen, state->selstr, state->sellen);
	state->outlen += state->sellen;
	state->out[state->outlen] = '\0';
	return 0;
}

/** asai_fps - override fps.
 * @see asa_import_func */
static int asai_fps(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	state->fps_rec = insn->v.fps_value == 0 ? state->fps_app
		: (1. / insn->v.fps_value);
	return 0;
}

/** asai_gettime - walk asa_tspec and sum up the time.
 * @see asa_import_func
 * @return the calculated time, delta[0] on error
 * also updates the delta history.
 */
static double asai_gettime(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	struct asa_tspec *tsp;
	double t = 0.0;
	if (insn->v.tspec.delta_select != -1) {
		if (insn->v.tspec.delta_select < MAXDELTA)
			t += state->delta[insn->v.tspec.delta_select];
		else
			subhelp_log(CSRI_LOG_ERROR, "imports: tspec "
				"delta %d exceeds compiled-in maximum of %d",
				insn->v.tspec.delta_select, MAXDELTA);
	}
	for (tsp = insn->v.tspec.tsp; tsp; tsp = tsp->next) {
		char *errptr;
		double src;

		if ((unsigned)tsp->group >= state->nmatches) {
			subhelp_log(CSRI_LOG_ERROR, "imports: tspec "
				"tries to access group %d, but only "
				"%d groups exist",
				tsp->group, state->nmatches);
			continue;
		}
		if (!*state->matches[tsp->group])
			continue;
		src = strtod(state->matches[tsp->group], &errptr);
		if (*errptr)
			subhelp_log(CSRI_LOG_WARNING, "imports: "
				"invalid tspec '%s'",
				state->matches[tsp->group]);
		t += tsp->mult * src + tsp->fps_mult * state->fps_rec * src;
	}
	memmove(state->delta + 1, state->delta,
		sizeof(state->delta[0]) * (MAXDELTA - 1));
	state->delta[0] = t;
	return t;
}

/** asai_show - set start time.
 * @see asa_import_func */
static int asai_show(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	state->start = asai_gettime(state, insn);
	return 0;
}

/** asai_hide - set end time.
 * @see asa_import_func */
static int asai_hide(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	state->end = asai_gettime(state, insn);
	return 0;
}

/** asai_child - execute childs if we match.
 * @see asa_import_func */
static int asai_child(struct asa_import_state *state,
	struct asa_import_insn *insn)
{
	int rv, v[MAXGROUP * 2];
	if ((rv = pcre_exec(insn->v.child.regex.pcregex, NULL, state->line,
			state->remain, 0, 0, v, MAXGROUP * 2)) >= 0) {
		asai_set_matches(state, state->line, v, rv);
		state->line += v[1];
		state->remain -= v[1];
		rv = asai_run_insns(state, insn->v.child.insns);
		return rv;
	}
	return 0;
}

int asa_import(const void *data, size_t dlen, double fps,
	struct asa_import_detect *det, enum asa_import_target target,
	asa_import_callback *callback, void *arg)
{
	struct asa_import_format *fmt = det->fmt;
	struct asa_import_state state;
	int rv;

	memset(&state, 0, sizeof(state));
	state.fps_rec = state.fps_app = 1./fps;
	state.line = (const char *)data;
	state.remain = dlen;
	state.cb = callback;
	state.cb_arg = arg;

	rv = asai_run_insns(&state, fmt->insns);
	if (state.matches)
		xfree(state.matches);
	if (state.out) {
		callback(arg, state.start, state.end,
			state.out, state.outlen);
		xfree(state.out);
	}
	if (state.selstr)
		xfree(state.selstr);
	return rv;
}

int asa_pcre_compile(asa_pcre *out, const char *str)
{
	const char *err;
	int ec, eo;

	out->pcregex = pcre_compile2(str, 0, &ec, &err, &eo, NULL);
	if (out->pcregex)
		return 0;
	fprintf(stderr, "/%s/: %s (%d)\n", str, err, eo);
	return 1;
}

#ifdef IMPORTS
void yyerror(char *s)
{
	subhelp_log(CSRI_LOG_WARNING, "failed to load imports file: %s", s);
}

extern int yyparse();
extern FILE *yyin;
#endif
#ifdef IMPORTS_PREP
#include "imports_prep.h"
#endif

void asa_init_import()
{
#ifdef IMPORTS
	yyin = fopen(IMPORTS, "r");
	yyparse();
#endif
#ifdef IMPORTS_PREP
	preparse_add();
#endif
	asa_imports_crosslink();
}
