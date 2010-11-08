%{
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
#include <unistd.h>
#include <ctype.h>

#include "common.h"
#include "imports.h"

extern int yylex();
extern void yyerror(char *s);

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
	asa_pcre pcre;
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
						xnewz(struct asa_import_detect);
					d->name = $2;
					if (!asa_pcre_compile(&d->re, $3))
						asa_det_last = &(*asa_det_last = d)->next;
					else
						xfree(d);
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
					if (asa_pcre_compile(&$$, $1))
						if (!current_fail)
							current_fail = 1;
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

