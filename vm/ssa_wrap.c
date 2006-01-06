#include "common.h"
#include "ssawrap.h"
#include "asa.h"

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <alloca.h>
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

#ifdef SSA_DEBUG
	struct ssav_unit *u;
	struct ssa_node *n;
	unsigned uc = 0, pos = 0, endpos;
	ssaout_t *c;
	fprintf(stderr, "dumping line-break info for vline %d:\n", we->vl->input->no);
	u = we->vl->unit_first;
	n = we->vl->input->node_first;
	c = n->v.text.s;
	while (u) {
		fprintf(stderr, "> unit %d: ", uc);
		endpos = u->next ? u->next->idxstart : we->pos;
		while (pos != endpos) {
			if (c == n->v.text.e) {
				fprintf(stderr, "<nextnode>");
				do {
					n = n->next;
				} while (n && n->type != SSAN_TEXT);
				if (!n)
					return;
				c = n->v.text.s;
			}
			fprintf(stderr, "[%c]", *c++);
			pos++;
		}
		fprintf(stderr, "\n");
		u = u->next;
		uc++;
	}
	fprintf(stderr, "<<end\n");
#endif
}

