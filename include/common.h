/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005  David Lamparter
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

#ifndef _COMMON_H
#define _COMMON_H

#include <stdlib.h>

#define xmalloc malloc
#define xcalloc calloc
#define xnew(type) (type *)xmalloc(sizeof(type))
#define xnewz(type) (type *)xcalloc(sizeof(type), 1)
#define xrealloc realloc
#define xfree free

static inline void *xrealloc_free(void *old, size_t newsize)
{
	void *rv = realloc(old, newsize);
	if (!rv)
		free(old);
	return rv;
}

#define oom_return(x) do { if (x) { \
	subhelp_log(CSRI_LOG_ERROR, "Out of memory in %s:%d", \
		__FILE__, __LINE__); return NULL; } } while (0)
#define oom_msg() \
	subhelp_log(CSRI_LOG_ERROR, "Out of memory in %s:%d", \
		__FILE__, __LINE__)

#ifndef HAVE_CONFIG_H
#include "acconf_win32.h"
#else
#include "acconf.h"
#endif

#ifdef HAVE__STRDUP
#if !HAVE_DECL__STRDUP
#if defined(_WIN32)
extern char *__cdecl _strdup(const char*);
#else
extern char *_strdup(const char*);
#endif
#endif
#define xstrdup _strdup

#elif defined(HAVE_STRDUP)
#if !HAVE_DECL_STRDUP
#if defined(_WIN32)
extern char *__cdecl strdup(const char*);
#else
extern char *_strdup(const char*);
#endif
#endif
#define xstrdup strdup

#else
#include <string.h>
static inline char *xstrdup(const char *s)
{
	size_t len = strlen(s) + 1;
	char *ptr = (char *)malloc(len);
	memcpy(ptr, s, len);
	return ptr;
}
#endif

#ifdef _WIN32
# define f_export 	__declspec(dllexport)
#else
# ifdef HAVE_GCC_VISIBILITY
#  define f_export	__attribute__((visibility ("default")))
#  define f_fptr	__attribute__((visibility ("hidden")))
# endif
#endif

#ifndef f_export
# define f_export
#endif
#ifndef f_fptr
# define f_fptr
#endif

#define iconv_errno errno

#endif /* _COMMON_H */
