/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
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
