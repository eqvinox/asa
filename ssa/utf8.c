/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2006  David Lamparter
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


