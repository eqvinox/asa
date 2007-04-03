/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005  David Lamparter
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

#ifndef _COMMON_H
#define _COMMON_H

#define xmalloc malloc
#define xrealloc realloc
#define xfree free

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
#include <stdlib.h>
#include <string.h>
static inline char *xstrdup(const char *s)
{
	size_t len = strlen(s) + 1;
	char *ptr = (char *)malloc(len);
	memcpy(ptr, s, len);
	return ptr;
}
#endif

/*
 * okay. do NOT include this header from anything other than
 * files going into the DSO / DLL. you certainly do not want
 * invisible symbols in your main program ^_^
 */

#ifdef _WIN32
# define f_export 	__declspec(dllexport)
#else
# if 0
/*
 * now, i would really like to enable this, but that'll
 * require some work. TODO:
 *  - figure out why visibility-hidden screws -fstack-protector
 *  - make one library out of the, uh, 4 we have right now
 *  - decide what to export and what not to
 *  - check whether we can do #pragma GCC visib... here or whether
 *    we need to wait till after system headers
 *                                                -equinox, 20060418
 */
# ifdef __GNUC__
#  if __GNUC__ >= 3
#   define ASA_STARTIMPL	_Pragma("GCC visibility push(internal)")
#   define f_export		__attribute__((visibility ("default")))
#   define f_fptr		__attribute__((visibility ("hidden")))
#  endif
# endif
# endif
#endif

#ifndef f_export
# define f_export
#endif
#ifndef f_fptr
# define f_fptr
#endif

#ifdef HAVE_PNG_H
#include <png.h>
#endif

#define iconv_errno errno

#endif /* _COMMON_H */
