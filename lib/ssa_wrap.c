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

#include "common.h"
#include "ssawrap.h"
#include "asa.h"

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <iconv.h>

/* might add zero-width break space (u200B) */
#define isbreak(c) ((c) == ' ' || (c) == '\t')

void ssaw_prepare(struct ssa_wrap_env *we, struct ssav_line *vl)
{
	we->cur_unit = NULL;
	we->vl = vl;
	we->pos = 0;
	/* beginning of line handled like implicit space */
	we->hit_space = 1;
}

static inline void ssaw_start(struct ssa_wrap_env *we)
{
	struct ssav_unit *u;

	u = xmalloc(sizeof(struct ssav_unit));
	u->next = NULL;
	u->type = SSAVU_TEXT;
	/* we->cur_unit == NULL: special case of leading whitespace.
	 * idxstart = 0 means we don't skip it.
	 */
	u->idxstart = we->cur_unit ? we->pos : 0;
	*(we->cur_unit ? &we->cur_unit->next : &we->vl->unit_first) = u;
	we->cur_unit = u;
	
	we->hit_space = 0;
}

void ssaw_put(struct ssa_wrap_env *we, struct ssa_node *n,
	struct ssav_node *vn, struct asa_font *fnt)
{
	unsigned *o;
	ssaout_t *c;

	if (vn->type == SSAVN_NEWLINE || vn->type == SSAVN_NEWLINEH) {
		if (we->vl->wrap != 2 && vn->type == SSAVN_NEWLINE)
			return;
		ssaw_start(we);
		we->cur_unit->type = SSAVU_NEWLINE;
		we->cur_unit->nl_node = vn;
		we->hit_space = 1;
		return;
	}

	if (vn->type != SSAVN_TEXT)
		return;
	o = vn->indici;
	c = n->v.text.s;
	while (c < n->v.text.e) {
		if (isbreak(*c))
			we->hit_space++;
		else if (we->hit_space)
			ssaw_start(we);

		*o++ = FT_Get_Char_Index(fnt->face, *c++);
		we->pos++;
	}
}

void ssaw_finish(struct ssa_wrap_env *we)
{
	we->vl->nchars = we->pos;

#if 0
	struct ssav_unit *u;
	struct ssa_node *n;
	unsigned uc = 0, pos = 0, endpos;
	ssaout_t *c;
	fprintf(stderr, "dumping line-break info for vline %d:\n", we->vl->input->no);
	u = we->vl->unit_first;
	n = we->vl->input->node_first;
	c = n->v.text.s;
	while (u) {
		fprintf(stderr, "> unit %d @%p: ", uc++, (void *)u);
		if (u->type != SSAVU_TEXT) {
			fprintf(stderr, "%s\n", u->type == SSAVU_NEWLINE ? "<\\n>" : "<?>");
			u = u->next;
			continue;
		}
		endpos = u->next ? u->next->idxstart : we->pos;
		while (pos != endpos) {
			if (c == n->v.text.e || n->type != SSAN_TEXT) {
				fprintf(stderr, "<nextnode>");
				do {
					n = n->next;
				} while (n && (n->type != SSAN_TEXT || n->v.text.s == n->v.text.e));
				if (!n)
					return;
				c = n->v.text.s;
			}
			fprintf(stderr, "[%c]", *c++);
			pos++;
		}
		fprintf(stderr, "\n");
		u = u->next;
	}
	fprintf(stderr, "<<end\n");
#endif
}

