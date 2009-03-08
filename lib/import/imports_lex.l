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

#include <subhelp.h>

#include "common.h"
#include "imports.h"
#include "imports_par.h"
%}

%option noyywrap

%x comment
%x regex
%x regex_r
%x regex_rr

%s format
%s token
%s regex_s
%s outtype

%{
#define MAX_STRLEN	8192
#define tok_n(ret)	token_ret = ret; BEGIN(token);
#define rex_n(ret)	regex_ret = ret; BEGIN(regex_s);

#define ensure(len) \
	if ((ssize_t)(strbuf + MAX_STRLEN - strbuf_ptr - 1) < len) \
		yy_fatal_error("Regular expression too long.");
%}
%%
			static char strbuf[MAX_STRLEN], *strbuf_ptr;
			static int token_ret, comment_ret,
				regex_ret, regex_r_ret;
			if (0) yyunput(0, "");

 /* eat up space */
[[:space:]]*		/**/

 /* token input */
<token>[a-zA-Z0-9_]+	{
				BEGIN(token_ret); yylval.s = strdup(yytext);
				return TOKEN;
			}

 /* initial keywords */
<INITIAL>{
	"detect"	regex_ret = INITIAL; tok_n(regex_s); return DETECT;
	"format"	tok_n(outtype); return FORMAT;
}
 /* output keywords */
<outtype>{
	"ssa"		BEGIN(INITIAL); return SSA;
	"text"		BEGIN(INITIAL); return TEXT;
}

 /* formatspec keywords */
<format>{
	"fps"		return FPS;
	"show"		return SHOW;
	"hide"		return HIDE;
	"commit"	return COMMIT;
	"discard"	return DISCARD;
	"append"	return APPEND;
	"delta"		return DELTA;
	"select"	return SELECT;
	"break"		return BREAK;
	"sgu"		{
				regex_ret = regex_r;
				regex_r_ret = format;
				BEGIN(regex_s);
				return SGU;
			}
	"sg"		{
				regex_ret = regex_r;
				regex_r_ret = format;
				BEGIN(regex_s);
				return SG;
			}
	":"		regex_ret = format; BEGIN(regex_s); return ':';
	";"		return ';';
	"}"		BEGIN(INITIAL); return '}';
}

 /* format items */
"{"			BEGIN(format); return '{';

 /* comments */
"#".*
"/*"			comment_ret = YY_START; BEGIN(comment);
<comment>{
	[^*]*
	"*"+[^*/]*
	"*"+"/"		BEGIN(comment_ret);
}

0x[0-9a-fA-F]+		yylval.n = strtoul(yytext + 2, NULL, 16); return NUM;
[0-9]+\.[0-9]*		yylval.f = strtod(yytext, NULL); return FLOAT;
[0-9]+			yylval.n = strtoul(yytext, NULL, 10); return NUM;

 /* regex start */
<regex_s>"/"[^*]	{
				strbuf_ptr = strbuf;
				yyless(1);
				BEGIN(regex);
			}
<regex>{
	"/"		{
				*strbuf_ptr = '\0';
				yylval.s = strdup(strbuf);
				strbuf_ptr = strbuf;
				BEGIN(regex_ret);
				return REGEX;
			}
	\\.		{
				ensure(2);
				*strbuf_ptr++ = yytext[0];
				*strbuf_ptr++ = yytext[1];
			}
	[^\\/]+		{
				ssize_t len = strlen(yytext);
				ensure(len);
				memcpy(strbuf_ptr, yytext, len);
				strbuf_ptr += len;
			}
}
<regex_rr>{
	[0-9]+		{
				BEGIN(regex_r);
				yylval.n = atoi(yytext);
				return REGEX_RR;
			}
}
<regex_r>{
	"/"		{
				*strbuf_ptr = '\0';
				yylval.s = strdup(strbuf);
				strbuf_ptr = strbuf;
				BEGIN(regex_r_ret);
				return REGEX_R;
			}
	\\[0-9]		{
				yyless(1);
				BEGIN(regex_rr);
				*strbuf_ptr = '\0';
				yylval.s = strdup(strbuf);
				strbuf_ptr = strbuf;
				return REGEX_R;
			}
	\\-		/* skip over */
	\\n		ensure(1); *strbuf_ptr++ = '\n';
	\\r		ensure(1); *strbuf_ptr++ = '\r';
	\\t		ensure(1); *strbuf_ptr++ = '\t';
	\\[^0-9ntr-]	ensure(1); *strbuf_ptr++ = yytext[1];
	[^\\/]+		{
				ssize_t len = strlen(yytext);
				ensure(len);
				memcpy(strbuf_ptr, yytext, len);
				strbuf_ptr += len;
			}
}

.			return yytext[0];
