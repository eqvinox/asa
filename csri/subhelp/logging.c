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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "visibility.h"
#include "subhelp.h"
#include <csri/logging.h>

static void *appdata = NULL;
static csri_logging_func *logfunc = NULL;

void setlogcallback(csri_logging_func *_logfunc, void *_appdata) hidden;

void setlogcallback(csri_logging_func *_logfunc, void *_appdata)
{
	logfunc = _logfunc;
	appdata = _appdata;
}

static struct csri_logging_ext logext = {
	setlogcallback
};

void subhelp_logging_pass(struct csri_logging_ext *nlogext)
{
	if (!nlogext || !nlogext->set_logcallback)
		return;
	nlogext->set_logcallback(logfunc, appdata);
}

void *subhelp_query_ext_logging(csri_ext_id extname)
{
	if (strcmp(extname, CSRI_EXT_LOGGING))
		return NULL;
	return &logext;
}

void subhelp_log(enum csri_logging_severity severity, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	subhelp_vlog(severity, msg, args);
	va_end(args);
}

void subhelp_vlog(enum csri_logging_severity severity,
	const char *msg, va_list args)
{
	char *buffer;
	const char *final;
	size_t size = 256;
	int n;

	buffer = (char *)malloc(256);
	while (buffer) {
		n = vsnprintf(buffer, size, msg, args);
		if (n >= 0 && (unsigned)n < size)
			break;
		size = n > 0 ? (unsigned)n + 1 : size * 2;
		buffer = (char *)realloc(buffer, size);
	}
	final = buffer ? buffer : "<out of memory in logging function>";
	subhelp_slog(severity, final);
	if (buffer)
		free(buffer);
}

void subhelp_slog(enum csri_logging_severity severity, const char *msg)
{
	if (logfunc)
		logfunc(appdata, severity, msg);
	else
		fprintf(stderr, "%s\n", msg);
}

