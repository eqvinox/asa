/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004  David Lamparter
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

#ifndef _AVL_H
#define _AVL_H

#include <stdlib.h>

/* XXX IMPORTANT:
	avl_head must be zeroed before being passed to avl_insert*!
 */

typedef unsigned int avl_value;

struct avl_head {
	struct avl_head *side[2];
	struct avl_head *parent;
	int balance;
	avl_value value;
};

struct avl_headless {
	struct avl_head *side[2];
	struct avl_head *parent;
	int balance;
};

extern void avl_insert(struct avl_head **root, struct avl_head *item);
extern void avl_insert_pcache(struct avl_head **root,
	struct avl_head *item, struct avl_head *pcache);
extern struct avl_head *avl_find(struct avl_head *root, avl_value value);
extern struct avl_head *avl_find_pcache(struct avl_head *root,
	avl_value value, struct avl_head **pcache);
extern void avl_delete(struct avl_head **root, struct avl_head *item);
extern struct avl_head *avl_realloc(struct avl_head **root,
	struct avl_head *node, size_t size);

#endif /* _AVL_H */
