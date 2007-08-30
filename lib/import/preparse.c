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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "imports.h"

struct asa_import_detect *asa_det_first = NULL,
	**asa_det_last = &asa_det_first;
struct asa_import_format *asa_fmt_first = NULL,
	**asa_fmt_last = &asa_fmt_first;

int asa_pcre_compile(asa_pcre *out, const char *str)
{
	const char *err;
	int ec, eo;
	pcre *tmp;

	tmp = pcre_compile2(str, 0, &ec, &err, &eo, NULL);
	if (!tmp) {
		fprintf(stderr, "/%s/: %s (%d)\n", str, err, eo);
		return 1;
	}
	pcre_refcount(tmp, -1);
	out->str = strdup(str);
	return 0;
}

void yyerror(char *s)
{
	fprintf(stderr, "failed to load imports file: %s", s);
}

extern int yyparse();
extern FILE *yyin;

#define BUFSIZES	2048

static void escape(char *out, const char *input)
{
	char *onow = out;
	const char *inow = input;

	while (*inow && onow < out + BUFSIZES - 5) {
		if (*inow < 0x20 || *inow >= 0x7f) {
			sprintf(onow, "\\x%02x", (unsigned char)*inow);
			onow += 4;
		} else {
			if (*inow == '\\' || *inow == '"')
				*onow++ = '\\';
			*onow++ = *inow;
		}
		inow++;
	}
	*onow = '\0';
}

static void dump_detects()
{
	char namebuf[BUFSIZES], rexbuf[BUFSIZES];

	struct asa_import_detect *d;
	for (d = asa_det_first; d; d = d->next) {
		escape(namebuf, d->name);
		escape(rexbuf, d->re.str);
		printf("\tdet(\"%s\",\"%s\")\n", namebuf, rexbuf);
	}
}

static void dump_insns(int level, struct asa_import_insn *i)
{
	char buf1[BUFSIZES];
	struct asa_repl *repl;
	struct asa_tspec *tsp;
	for (; i; i = i->next) {
		switch (i->insn) {
		case ASAI_CHILD:
			escape(buf1, i->v.child.regex.str);
			printf("\t\tinsn_b(i%d, i%d, ASAI_CHILD, \"%s\")\n",
				level - 1, level, buf1);
			dump_insns(level + 1, i->v.child.insns);
			printf("\t\tinsn_e()\n");
			break;
		case ASAI_COMMIT:
		case ASAI_DISCARD:
		case ASAI_APPEND:
			printf("\t\tinsn(i%d, %d);\n", level - 1, i->insn);
			break;
		case ASAI_BREAK:
			printf("\t\tinsn(i%d, ASAI_BREAK); ", level - 1);
			printf("i->v.break_depth = %d;\n", i->v.break_depth);
			break;
		case ASAI_SELECT:
			printf("\t\tinsn(i%d, ASAI_SELECT); ", level - 1);
			printf("i->v.select = %d;\n", i->v.select);
			break;
		case ASAI_FPS:
			printf("\t\tinsn(i%d, ASAI_FPS); ", level - 1);
			printf("i->v.fps_value = %lf;\n", i->v.fps_value);
			break;
		case ASAI_SG:
		case ASAI_SGU:
			escape(buf1, i->v.sg.regex.str);
			printf("\t\tinsn_sg(i%d, %d, \"%s\")\n",
				level - 1, i->insn, buf1);
			for (repl = i->v.sg.repl; repl; repl = repl->next)
				if (repl->group != -1)
					printf("\t\t\trepl(%d, NULL)\n",
						repl->group);
				else {
					escape(buf1, repl->text);
					printf("\t\t\trepl(-1, \"%s\")\n",
						buf1);
				}
			printf("\t\tinsn_sge()\n");
			break;
		case ASAI_SHOW:
		case ASAI_HIDE:
			printf("\t\tinsn_ts(i%d, %d, %d); ",
				level - 1, i->insn, i->v.tspec.delta_select);
			for (tsp = i->v.tspec.tsp; tsp; tsp = tsp->next)
				printf("\t\t\ttsp(%d, %lf, %lf)\n",
					tsp->group, tsp->mult, tsp->fps_mult);
			printf("\t\tinsn_tse()\n");
			break;
		default:
			fprintf(stderr, "Unknown instruction %d\n", i->insn);
			break;
		}
	}
}

static void dump_formats()
{
	char namebuf[BUFSIZES];
	int level = 1;

	struct asa_import_format *f;
	for (f = asa_fmt_first; f; f = f->next) {
		escape(namebuf, f->name);
		printf("\tfmt_b(\"%s\", %d)\n", namebuf, f->target);
		dump_insns(level, f->insns);
		printf("\tfmt_e()\n");
	}
}

int main(int argc, char **argv)
{
	const char *src;
	char datebuf[256];
	time_t now;
	struct tm *now_tm;

	switch (argc) {
	case 0:
	case 1:
		src = "<stdin>";
		yyin = stdin;
		break;
	case 2:
		src = argv[1];
		yyin = fopen(src, "r");
		if (!yyin) {
			fprintf(stderr, "Error opening \"%s\": %s (%d)\n",
				src, strerror(errno), errno);
			return 1;
		}
		break;
	default:
		fprintf(stderr, "Syntax: %s [<filename>]\n", argv[0]);
		return 2;
	}
	yyparse();

	time(&now);
	now_tm = gmtime(&now);
	strftime(datebuf, sizeof(datebuf), "%Y-%m-%dT%H:%M:%S+00:00", now_tm);
	printf(	"/* AUTOGENERATED FILE, DO NOT EDIT */\n"
		"/* generated from \"%s\" on %s */\n\n", src, datebuf);

	printf(	"#include <stdlib.h>\n"
		"#include <common.h>\n"
		"#include <imports.h>\n\n");

	printf(	"void preparse_add()\n{\n"
		"#define insn_init { NULL, NULL, 0, { 0 } }\n"
		"#define det(n,r) { static struct asa_import_detect d = { NULL }; \\\n"
			"\t\td.name = n; \\\n"
			"\t\tif (!asa_pcre_compile(&d.re, r)) \\\n"
			"\t\t\tasa_det_last = &(*asa_det_last = &d)->next; }\n"
		"#define fmt_b(n,t) { static struct asa_import_format f = { NULL }; \\\n"
			"\t\tstruct asa_import_insn *i, **i0 = NULL; \\\n"
			"\t\tf.name = n; f.target = t;\n"
		"#define fmt_e() \\\n"
			"\t\t}\n"
		"#define insn(n,t) { static struct asa_import_insn ii = insn_init; \\\n"
			"\t\ti = &ii; *n = i; n = &i->next; i->insn = t; }\n"
		"#define insn_b(n, m, t, r) { struct asa_import_insn **m;\\\n"
			"\t\t{ static struct asa_import_insn ii = insn_init; \\\n"
			"\t\t\ti = &ii; ii.insn = t; \\\n"
			"\t\t\tm = &ii.v.child.insns; \\\n"
			"\t\t}\\\n"
			"\t\tif (!asa_pcre_compile(&i->v.child.regex, r)) { \\\n"
			"\t\t\t*n = i; n = &i->next;\n"
		"#define insn_e() } }\n"
		"#define insn_sg(n, t, r) { struct asa_repl **repl;\\\n"
			"\t\t{ static struct asa_import_insn ii = insn_init; \\\n"
			"\t\t\ti = &ii; ii.insn = t; \\\n"
			"\t\t\trepl = &ii.v.sg.repl; \\\n"
			"\t\t}\\\n"
			"\t\tif (!asa_pcre_compile(&i->v.sg.regex, r)) { \\\n"
			"\t\t\t*n = i; n = &i->next;\n"
		"#define insn_sge() } }\n"
		"#define repl(g, t) { static struct asa_repl r = { NULL, g, t }; \\\n"
			"\t\t*repl = &r; repl = &r.next; }\n"
		"#define insn_ts(n, t, d) { struct asa_tspec **tsp;\\\n"
			"\t\t{ static struct asa_import_insn ii = insn_init; \\\n"
			"\t\t\ti = &ii; ii.insn = t; ii.v.tspec.delta_select = d; \\\n"
			"\t\t\ttsp = &ii.v.tspec.tsp; \\\n"
			"\t\t}\\\n"
			"\t\t*n = i; n = &i->next;\n"
		"#define insn_tse() }\n"
		"#define tsp(g, m, f) { static struct asa_tspec t = { NULL, g, m, f }; \\\n"
			"\t\t*tsp = &t; tsp = &t.next; }\n"
		"\n");
	dump_detects();
	dump_formats();
	printf(	"\n}\n");
}
