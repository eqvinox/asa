%{
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

#ifdef HAVE_CONFIG_H
#include "acconf.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include <subhelp.h>

#include "common.h"
#include "imports.h"

void yyerror(char *s)
{
	subhelp_log(CSRI_LOG_WARNING, "failed to load imports file: %s", s);
}

%}

%union {
	int n;
	char *s;
	float f;

	enum asa_import_target target;
	struct asa_repl *repl;
	struct {
		double m, f;
	} mult;
	struct asa_tspec *tspec;
	struct asa_import_insn *insn;
	pcre *pcre;
}

%token <s> TOKEN
%token <n> REGEX_RR
%token <s> REGEX_R
%token <s> REGEX
%token <n> NUM
%token <f> FLOAT

%token DETECT
%token FORMAT SSA TEXT

%token COMMIT DISCARD BREAK
%token SELECT SG SGU APPEND
%token FPS SHOW HIDE DELTA

%type <target> target
%type <repl> repl reple
%type <n> delta
%type <mult> tmul
%type <tspec> tspecs
%type <insn> formatspecs
%type <pcre> regex

%{
static struct asa_import_insn *current_insn;
static int current_fail = 0;			/* do we need to abort building the insn? */
%}
%%

/* the entire imports file */
input:	
	| detect input
	| format input
	;

/* format detection rule */
detect:	DETECT TOKEN REGEX	{
					struct asa_import_detect *d =
						xnew(struct asa_import_detect);
					const char *err;
					int ec, eo;
					d->name = $2;
					d->re = pcre_compile2($3, 0, &ec, &err, &eo, NULL);
					if (d->re)
						asa_det_last = &(*asa_det_last = d)->next;
					else {
						fprintf(stderr, "/%s/: %s (%d)\n", $3, err, eo);
						xfree(d);
					}
				}
	;

/* simple rules */
target:	SSA			{ $$ = ASAI_TARGET_SSA; }	;
target:	TEXT			{ $$ = ASAI_TARGET_TEXT; }	;

/* sg replacements */
reple:	REGEX_R			{
					$$ = xnew(struct asa_repl);
					$$->group = -1;
					$$->text = $1;
				}
	;
reple:	REGEX_RR		{
					$$ = xnew(struct asa_repl);
					$$->group = $1;
				}
	;
repl:				{ $$ = NULL; }
	| reple repl		{ $1->next = $2; $$ = $1; }
	;

/* time specification */
delta:				{ $$ = -1; }
	| DELTA NUM		{ $$ = $2; }
	;
tmul:	'h'			{ $$.f = 0.; $$.m = 3600.; }
	| 'm'			{ $$.f = 0.; $$.m = 60.; }
	| 's'			{ $$.f = 0.; $$.m = 1.; }
	| 'c'			{ $$.f = 0.; $$.m = 0.01; }
	| 'u'			{ $$.f = 0.; $$.m = 0.001; }
	| 'f'			{ $$.f = 1.; $$.m = 0.; }
	;
tspecs:				{ $$ = NULL; }
	| NUM tmul tspecs	{
					$$ = xnew(struct asa_tspec);
					$$->next = $3;
					$$->mult = $2.m;
					$$->fps_mult = $2.f;
					$$->group = $1;
				}
	;

/* helper for loading the regex */
regex:	REGEX			{
					const char *err;
					int ec, eo;
					$$ = pcre_compile2($1, 0, &ec, &err, &eo, NULL);
					if (!$$) {
						fprintf(stderr, "/%s/: %s (%d)\n", $1, err, eo);
						/* ignore nested errors */
						if (!current_fail)
							current_fail = 1;
					}
				}
	;
/* actual instructions */
formatspec:
	COMMIT			{ current_insn->insn = ASAI_COMMIT; }
	| DISCARD		{ current_insn->insn = ASAI_DISCARD; }
	| BREAK NUM		{
					current_insn->insn = ASAI_BREAK;
					current_insn->v.break_depth = $2;
				}
	| SELECT NUM		{
					current_insn->insn = ASAI_SELECT;
					current_insn->v.select = $2;
				}
	| SG regex repl		{
					current_insn->insn = ASAI_SG;
					current_insn->v.sg.regex = $2;
					current_insn->v.sg.repl = $3;
				}
	| SGU regex repl		{
					current_insn->insn = ASAI_SGU;
					current_insn->v.sg.regex = $2;
					current_insn->v.sg.repl = $3;
				}
	| APPEND		{ current_insn->insn = ASAI_APPEND; }
	| FPS FLOAT		{
					current_insn->insn = ASAI_FPS;
					current_insn->v.fps_value = $2;
				}
	| SHOW tspecs delta	{
					current_insn->insn = ASAI_SHOW;
					current_insn->v.tspec.tsp = $2;
					current_insn->v.tspec.delta_select = $3;
				}
	| HIDE tspecs delta	{
					current_insn->insn = ASAI_HIDE;
					current_insn->v.tspec.tsp = $2;
					current_insn->v.tspec.delta_select = $3;
				}
	;
formatspec:
	':' regex		{
					current_insn->insn = ASAI_CHILD;
					current_insn->v.child.regex = $2;
					$<insn>$ = current_insn;
				}
	formatspecs ';'		{
					current_insn = $<insn>3;
					current_insn->v.child.insns = $4;
				}
	;

formatspecs:			{ $$ = NULL; }
	|			{
					/* our parent failed, but we can't just
					 * drop parsing. continue and kill ourselves afterwards.
					 */
					if (current_fail)
						current_fail++;
					current_insn = xnew(struct asa_import_insn);
					$<insn>$ = current_insn;
				}
	formatspec
	formatspecs		{	
					if (current_fail) {
						xfree(current_insn);
						current_fail--;
						$$ = $3;
					} else {
						$<insn>1->next = $3;
						$$ = $<insn>1;
					}
				}
	;

format:	FORMAT TOKEN target
	'{' formatspecs '}'	{
					struct asa_import_format *f =
						xnewz(struct asa_import_format);
					f->name = $2;
					f->target = $3;
					f->insns = $5;
					asa_fmt_last = &(*asa_fmt_last = f)->next;
				}
	;

