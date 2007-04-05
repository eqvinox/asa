/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
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
/** @file colour.h - colour typedefs */

#ifndef _COLOUR_H
#define _COLOUR_H

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else
#define __LITTLE_ENDIAN	1234
#define __BIG_ENDIAN	4321
#define __PDP_ENDIAN	3412
#define __BYTE_ORDER	__LITTLE_ENDIAN
#endif

typedef unsigned char alpha_t;		/**< alpha field */
typedef unsigned char colour_one_t;	/**< single colour channel */

/** colour storage */
typedef union {
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		colour_one_t b, g, r;
		alpha_t a;
#elif __BYTE_ORDER == __BIG_ENDIAN
		alpha_t a;
		colour_one_t r, g, b;
#elif __BYTE_ORDER == __PDP_ENDIAN
		colour_one_t r;
		alpha_t a;
		colour_one_t b, g;
#else
#error fix <endian.h>
#endif
	} c;
	unsigned int l;			/**< always is 0xAARRGGBB */
} colour_t;

#endif /* _COLOUR_H */
