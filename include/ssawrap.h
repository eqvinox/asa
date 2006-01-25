/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005  David Lamparter
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

#ifndef _SSAWRAP_H
#define _SSAWRAP_H

#include "ssavm.h"
#include "asafont.h"

struct ssa_wrap_env {
	struct ssav_unit *cur_unit;
	struct ssav_line *vl;
	unsigned pos;
	unsigned hit_space;
};

extern void ssaw_put(struct ssa_wrap_env *we, struct ssa_node *n,
	struct ssav_node *vn, struct asa_font *fnt);
extern void ssaw_prepare(struct ssa_wrap_env *we, struct ssav_line *vl);
extern void ssaw_finish(struct ssa_wrap_env *we);

#endif /* _SSAWRAP_H */
