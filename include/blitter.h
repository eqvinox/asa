/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

#ifndef _BLITTER_H
#define _BLITTER_H

#include "asaproc.h"

typedef void asa_rgbcommitf(struct assp_fgroup *g, cellline **lines,
		unsigned char colours[4][4]);
extern struct asa_optfuncs {
	asa_rgbcommitf *rgbx, *xrgb, *rgba, *argb;
} asa_optfuncs;

extern void asa_opt_init();

#endif

