/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2006  David Lamparter
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
/** @file ssa.h - SSA definitions */

#include "common.h"
#include "ssa.h"

/** ssa_utf8_len - get UTF-8 byte length of a ssa_string.
 * @param s input ssa_string
 * @return number of bytes to allocate for converting the given ssa_string
 *  to UTF-8. includes +1 for the terminating null byte.
 */
size_t ssa_utf8_len(ssa_string *s)
{
	size_t result = 1;
	ssaout_t *c = s->s;

	for (; c < s->e; c++)
		result += *c < 0x80 ? 1 :
			(*c < 0x800 ? 2 :
			(*c < 0x10000 ? 3 : 4));
	return result;
}

/** ssa_utf8_conv - convert ssa_string to UTF-8.
 * @param out output buffer, utf8_len(s) bytes.
 *  will hold zero-terminated UTF-8 of s
 * @param s input ssa_string
 * @see utf8_len
 */
void ssa_utf8_conv(char *out, ssa_string *s)
{
	ssaout_t *c = s->s;
	for (; c < s->e; c++) {
		if (*c < 0x80)
			*out++ = *c;
		else if (*c < 0x800) {
			*out++ = 0xc0 | (*c >> 6);
			*out++ = 0x80 | (0x3f & *c);
		} else if (*c < 0x10000) {
			*out++ = 0xe0 | (*c >> 12);
			*out++ = 0x80 | (0x3f & (*c >> 6));
			*out++ = 0x80 | (0x3f & *c);
		} else if (*c < 0x10000) {
			*out++ = 0xf0 | (*c >> 18);
			*out++ = 0x80 | (0x3f & (*c >> 12));
			*out++ = 0x80 | (0x3f & (*c >> 6));
			*out++ = 0x80 | (0x3f & *c);
		}
	}
	*out = '\0';
}


