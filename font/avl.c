/*****
 * avl.c 
 *****
 * Adelson-Velski Landis Tree implementation
 * (C) 2004 David Lamparter
 *
 * no warranty. i wrote this stuff when i was 18 and i never
 * had a course on trees or anything.
 *****
 * ChangeLog
 *   20040628 - it works! :)
 *****/

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "avl.h"

/* single rotate for insert; overloaded (0 or 1) is the side
 * with too much stuff in it 
 */
static inline struct avl_head *avl_rotate(struct avl_head *now,
	unsigned overloaded)
{
	unsigned other = 1 - overloaded;
	struct avl_head *neo;

	neo = now->side[overloaded];

	neo->parent = now->parent;

	now->balance = 0;			/* hmm. */
	neo->balance = 0;

	now->side[overloaded] = neo->side[other];
	if (now->side[overloaded])
		now->side[overloaded]->parent = now;

	neo->side[other] = now;
	now->parent = neo;
	return neo;
}

/* double rotate for insert & delete; overloaded (0,1)
 */
static inline struct avl_head *avl_doublerotate(struct avl_head *now,
	unsigned overloaded)
{
	unsigned other = 1 - overloaded;
	int balance = -1 + (overloaded << 1);	/* side2balance */
	struct avl_head *mid, *neo;

	mid = now->side[overloaded];
	neo = mid->side[other];

	now->balance = (neo->balance == balance) ? -balance : 0;
	mid->balance = (neo->balance == -balance) ? balance : 0;
	neo->balance = 0;

	if ((mid->side[other] = neo->side[overloaded]))
		mid->side[other]->parent = mid;

	(neo->side[overloaded] = mid)->parent = neo;

	if ((now->side[overloaded] = neo->side[other]))
		now->side[overloaded]->parent = now;

	neo->side[other] = now;
	neo->parent = now->parent;
	now->parent = neo;

	return neo;
}

/* single rotate for delete; different balance rules
 * XXX: check wether this really is different from insert rotate,
 *   didn't really try...
 */
static inline struct avl_head *avl_rotate_zero(struct avl_head *now,
	unsigned overloaded)
{
	unsigned other = 1 - overloaded;
	int balance = -1 + (overloaded << 1);	/* side2balance */
	struct avl_head *neo;

	neo = now->side[overloaded];

	now->balance = neo->balance ? 0 :  balance;
	neo->balance = neo->balance ? 0 : -balance;

	now->side[overloaded] = neo->side[other];
	if (now->side[overloaded])
		now->side[overloaded]->parent = now;

	neo->side[other] = now;
	neo->parent = now->parent;
	now->parent = neo;

	return neo;
}

/* insert to known parent node - bottom-up balancing.
 * in most cases, we know the parent node because we previously
 * searched for it. top-down balancing wouldn't be faster...
 *
 * btw, no safety checks...
 */
void avl_insert_pcache(struct avl_head **root, struct avl_head *item,
	struct avl_head *pcache)
{
	struct avl_head *now, *set, *parent;
	avl_value value = item->value;
	unsigned from;

	item->parent = pcache;
	now = item->parent;
	if (now)
		now->side[value > now->value] = item;
	else {
		*root = item;
		return;
	}

	while (now) {
		from = value > now->value;

		parent = now->parent;		/* save over balancing */
		if (!now->balance) {
			now->balance = from ? 1 : -1;
		} else if ((now->balance == 1) != from) {
			now->balance = 0;
			return;
		} else {
			if ((value > now->side[from]->value) == from)
				set = avl_rotate(now, from);
			else
				set = avl_doublerotate(now, from);
			if (parent)
				parent->side[value > parent->value] = set;
			else
				*root = set;
			return;
		}
		now = parent;
	}
}

/* insert without known parent... is this even used anywhere? */
void avl_insert(struct avl_head **root, struct avl_head *item)
{
	struct avl_head *it, *pcache;
	it = avl_find_pcache(*root, item->value, &pcache);
	if (it) {
/*		LW("trying to insert existing value %d into AVL tree %p",
			item->value, *root); */
		return;
	}
	avl_insert_pcache(root, item, pcache);
}

/* find without returning parent node */
struct avl_head *avl_find(struct avl_head *root, avl_value value)
{
	struct avl_head *now;
	now = root;

	while (now) {
		if (now->value == value)
			return now;
		now = now->side[value > now->value];
	};
	return NULL;
}

/* find with returning parent node; NULL as retval = not found, NULL as
 * pcache = tree is empty
 */
struct avl_head *avl_find_pcache(struct avl_head *root, avl_value value,
	struct avl_head **pcache)
{
	struct avl_head *now;
	*pcache = NULL;
	now = root;

	while (now) {
		if (now->value == value)
			return now;
		*pcache = now;
		now = now->side[value > now->value];
	};
	return NULL;
}

/* guard around realloc to update references to the node in case the
 * addr changes
 */
struct avl_head *avl_realloc(struct avl_head **root, struct avl_head *node,
	size_t size)
{
	node = xrealloc(node, size);
	if (node->parent)
		node->parent->side[node->value > node->parent->value] =
			node;
	else
		*root = node;
	if (node->side[0])
		node->side[0]->parent = node;
	if (node->side[1])
		node->side[1]->parent = node;
	return node;
}

/* delete a leaf node (no childs) */
static void avl_delete_leaf(struct avl_head **root, struct avl_head *item,
	struct avl_head *set)
{
	struct avl_head *now, *parent;
	avl_value value = item->value;
	unsigned from, done = 0;

	now = item->parent;
	if (now)
		now->side[value > now->value] = set;
	else {
		*root = set;
		return;
	}

	while (now) {
		from = value > now->value;

		parent = now->parent;
		if (!now->balance) {
			now->balance = from ? -1 : 1;
			return;
		} else if ((now->balance == 1) == from) {
			now->balance = 0;
			set = now;
		} else {
			unsigned other = 1 - from;
			done = !now->side[other]->balance;
			if (now->side[other]->balance == -now->balance)
				set = avl_doublerotate(now, other);
			else
				set = avl_rotate_zero(now, other);
			if (parent)
				parent->side[value > parent->value] = set;
			else
				*root = set;
			if (done)
				return;
		}
		now = parent;
	}
}

/* delete some node. worst case is implemented by deleting innermost node
 * at more heavy tree
 */
void avl_delete(struct avl_head **root, struct avl_head *item)
{
	if (!item->side[0]) {
		if (item->side[1])
			item->side[1]->parent = item->parent;
		avl_delete_leaf(root, item, item->side[1]);
		return;
	}
	if (!item->side[1]) {
		item->side[0]->parent = item->parent;
		avl_delete_leaf(root, item, item->side[0]);
		return;
	}

	unsigned direction = (item->balance == 1), other = 1 - direction;
	struct avl_head *now = item->side[direction];

	while (now->side[other])
		now = now->side[other];
	if (now->side[direction])
		now->side[direction]->parent = now->parent;
	avl_delete_leaf(root, now, now->side[direction]);

	memcpy(now, item, sizeof(struct avl_headless));
	if (now->side[0])
		now->side[0]->parent = now;
	if (now->side[1])
		now->side[1]->parent = now;
	*(now->parent ? &now->parent->side[item->value >
			now->parent->value] : root) = now;
}
