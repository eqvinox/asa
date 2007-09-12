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

#include "common.h"
#include "ssa.h"
#include "ssavm.h"

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

static inline struct ssa_frag *ssap_frag_alloc(struct ssa_frag *frag,
	unsigned newtotalelems)
{
	return (struct ssa_frag *)xrealloc(frag, sizeof(struct ssa_frag)
		+ newtotalelems * sizeof(struct ssav_line *));
}

static inline struct ssa_frag *ssap_frag_clone(const struct ssa_frag *frag)
{
	struct ssa_frag *new;
	size_t size = sizeof(struct ssa_frag)
		+ frag->nrend * sizeof(struct ssav_line *);
	new = (struct ssa_frag *)xmalloc(size);
	memcpy(new, frag, size);
	return new;
}

/* prev and prevp only are hints passed for optimization.
 * pass v->fragments / &v->fragments if there are no hints */
static struct ssa_frag *ssap_frag_split(struct ssa_vm *v,
	struct ssa_frag *prev, double start,
	struct ssa_frag ***returnp)
{
	/* need to duplicate previous' lines, therefore seeking with
	 * prev one */
	struct ssa_frag *seek, *rv;
	if (start <= prev->start) {
		if (start <= v->fragments->start) {
			*returnp = &v->fragments;
			return v->fragments;
		}
		seek = v->fragments;
	} else
		seek = prev;

	while (seek->next && seek->next->start < start)
		seek = seek->next;
	/* XXX epsilon? */
	if (seek->next && seek->next->start == start) {
		*returnp = &seek->next;
		return seek->next;
	}
	
	rv = ssap_frag_clone(seek);
	seek->next = rv;
	rv->start = start;
	*returnp = &seek->next;
	return rv;
}

struct ssa_frag *ssap_frag_add(struct ssa_vm *v,
	struct ssa_frag *prev, struct ssav_line *l)
{
	struct ssa_frag *first, *last, *seek, **seekp, **discard;
	unsigned idx;
	if (l->end <= l->start)
		return NULL;
	v->cache = NULL;
	first = ssap_frag_split(v, prev, l->start, &seekp);
	last = ssap_frag_split(v, first, l->end, &discard);
	
	seek = first;
	do {
		*seekp = ssap_frag_alloc(seek, seek->nrend + 1);
		if (seek == first)
			first = *seekp;
		seek = *seekp;

		for (idx = 0; idx < seek->nrend; idx++)
			/*
			 * Note: this actually decides the whether the
			 * file line order is kept or reversed for
			 * same-layer lines (> or >= in next line)
			 */
			if (seek->lines[idx]->ass_layer > l->ass_layer) {
				memmove(seek->lines + idx + 1,
					seek->lines + idx,
					sizeof(struct ssav_line *)
					* (seek->nrend - idx));
				seek->lines[idx] = l; 
				break;
			}
		if (idx == seek->nrend)
			seek->lines[seek->nrend] = l;
		seek->nrend++;

		seek = *(seekp = &seek->next);
	} while (seek != last);
	return first;
}

void ssap_frag_expire(struct ssa_vm *vm, double ts)
{
	struct ssa_frag *prev = NULL, **pcur = &vm->fragments;
	struct ssav_line **r, **w;
	vm->cache = NULL;

	while (*pcur) {
		struct ssa_frag *cur = *pcur;

		/* fragments with visible lines always have a ->next */
		if (cur->nrend && cur->next->start <= ts) {
			double next = cur->next->start;

			for (r = w = &cur->lines[0];
				r < &cur->lines[cur->nrend];
				r++)
			{
				/* line not expired, keep it */
				if ((*r)->end > ts)
					*w++ = *r;
				/* last reference, free it */
				else if ((*r)->end == next)
					ssav_free(*r);
				/* else: not last reference, free'd later */
			}
			if (r != w) {
				cur->nrend = w - &cur->lines[0];
				cur = ssap_frag_alloc(cur, cur->nrend);
				*pcur = cur;
			}
			/* remove unneeded fragments */
			if (prev && prev->nrend == cur->nrend
				&& !memcmp(prev->lines, cur->lines, cur->nrend
					* sizeof(struct ssav_line *)))
			{
				prev->next = cur->next;
				xfree(cur);
				cur = prev;
			}
		}
		prev = cur;
		pcur = &cur->next;
	}
}

void ssap_frag_free(struct ssa_vm *vm)
{
	ssap_frag_expire(vm, HUGE_VAL);
}

struct ssa_frag *ssap_frag_init(struct ssa_vm *vm)
{
	struct ssa_frag *sentinel;

	sentinel = vm->fragments = ssap_frag_alloc(NULL, 0);
	sentinel->next = NULL;
	sentinel->start = 0.0;
	sentinel->nrend = 0;
	vm->cache = NULL;

	return sentinel;
}

