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

#ifndef _SSA_REND_H
#define _SSA_REND_H

#include "ssa.h"

/** prepare for rendering.
 * ssa_rend neets to already be allocated and have the ssa member set */
extern int ssa_prepare(struct ssa_renderer *ssa_rend);

extern void ssa_render(struct ssa_renderer *ssa_rend, double timestamp,
	unsigned char *dst, unsigned w, unsigned h);

#endif /* _SSA_REND_H */
