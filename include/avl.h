/* #include(LICENSE), from rewomuro, Copyright (c) 2004 David Lamparter */
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
