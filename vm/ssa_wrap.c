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

void ssaw_prepare(struct ssa_wrap_env *we, struct ssav_line *vl)
{
	we->cur_unit = &vl->unit_first;
}

static inline void ssaw_commit(struct ssa_wrap_env *we)
{
}

void ssaw_finish(struct ssa_wrap_env *we)
{
}

void ssaw_put(struct ssa_wrap_env *we, struct ssa_node *n,
	struct ssav_node *vn, struct asa_font *fnt)
{
	unsigned *o, f;
	ssaout_t *c;

	o = vn->indici;
	c = n->v.text.s;
	while (c < n->v.text.e) {
		if (*c == ' ') {
			ssaw_commit(we);
		}

		f = FT_Get_Char_Index(fnt->face, *c);
		if (f) {
			*o++ = f;
		} else
			vn->nchars--;
		c++;
	}
}

