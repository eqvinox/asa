/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2005, 2006  David Lamparter
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
/** @file spanproc.c - FreeType2 span->cell processor */

#include "common.h"
#include "asaproc.h"
#include "asafont.h"

/** allocate a cellline, probably using reservoir ones.
 * \param g the framegroup to use the reservoir from
 * \return an unused, zeroed cellline
 */
static cellline *assp_cellalloc(struct assp_fgroup *g)
{
	cellline *rv;
	if (g->ptr)
		return g->reservoir[--g->ptr];
	rv = (cellline *)calloc(sizeof(cellline) - sizeof(cell)
		+ sizeof(cell) * g->w, 1);
	rv->first = g->w;
	return rv;
}

/** grab a sure hold of a cellline.
 * \param f frame used
 * \param row y coordinate of the line to grab
 * \return modificable line
 */
static inline cellline *assp_cellgrab(struct assp_frame *f, unsigned row)
{
	if (f->lines[row] == f->group->unused)
		f->lines[row] = assp_cellalloc(f->group);
	return f->lines[row];
}

/** free a cellline, probably zeroing it and putting it into the reservoir.
 * \param g the framegroup to use the reservoir from
 * \param c the no longer needed cellline
 */
static void assp_cellfree(struct assp_fgroup *g, cellline *c)
{
	unsigned d;

	if (c == g->unused)
		return;
	if (g->ptr >= g->resvsize) {
		free(c);
		return;
	}

	for (d = c->first; d < c->last; d++)
		c->data[d].d = 0;
	g->reservoir[g->ptr++] = c;
}

f_fptr void assp_spanfunc(int y, int count, const FT_Span *spans, void *user)
{
	struct assp_param *p = (struct assp_param *)user;
	const FT_Span *end = spans + count;
	int ry = y + p->yo;
	cellline *row;

	if (!count || ry < (int)p->cy0 || ry >= (int)p->cy1)
		return;
	row = assp_cellgrab(p->f, ry);

	while (spans < end) {
		const FT_Span *now = spans++;
		int rx = now->x + p->xo;
		int x0, x1;
		cell *cnow, *cend;

		x0 = rx < p->cx0 ? p->cx0 : rx;
		cnow = &row->data[x0];
		if (x0 < (int)row->first)
			row->first = x0;

		x1 = rx + now->len > p->cx1 ? p->cx1 :
			rx + now->len;
		cend = &row->data[x1];
		if (x1 > (int)row->last)
			row->last = x1;

		while (cnow < cend) {
			unsigned uv = cnow->e[p->elem] + now->coverage;
			cnow->e[p->elem] = uv > 255 ? 255 : uv;
			cnow++;
		}
	}
}

/** allocate a new frame, defaulting lines to unused.
 * \param g group to associate with
 */
void assp_framenew(struct assp_frameref *ng, struct assp_fgroup *g)
{
	struct assp_frame *rv;
	unsigned c;

	rv = malloc(sizeof(struct assp_frame) - sizeof(cellline *)
		+ g->h * sizeof(cellline *));
	rv->group = g;
	for (c = 0; c < g->h; c++)
		rv->lines[c] = g->unused;
	ng->next = g->issued;
	g->issued = ng;
	ng->frame = rv;
}

/** free an allocated frame, probably putting its lines into the reservoir.
 * probably called regularly
 * \param f frame to free
 */
void assp_framefree(struct assp_frameref *ng)
{
	struct assp_frame *f = ng->frame;
	struct assp_frameref **trail;
	unsigned c;

	if (!f)
		return;

	for (trail = &f->group->issued; *trail != ng; )
		trail = &(*trail)->next;
	*trail = ng->next;
	ng->next = NULL;
	ng->frame = NULL;

	for (c = 0; c < f->group->h; c++)
		assp_cellfree(f->group, f->lines[c]);
	free(f);
}

/** kill an allocated frame.
 * probably called only on resizing everything
 * (putting lines into the reservoir is kind of... useless in that case)
 * \param f frame to free
 */
static void assp_framekill(struct assp_frameref *ng)
{
	struct assp_frame *f = ng->frame;
	unsigned c;

	for (c = 0; c < f->group->h; c++)
		if (f->lines[c] != f->group->unused)
			free(f->lines[c]);
	free(f);

	ng->next = NULL;
	ng->frame = NULL;
}

struct assp_fgroup *assp_fgroupnew(unsigned w, unsigned h,
	enum csri_pixfmt pixfmt)
{
	struct assp_fgroup *rv;
	rv = malloc(sizeof(struct assp_fgroup) - sizeof(cellline *)
		+ sizeof(cellline *) * h * 2);
	rv->w = w;
	rv->h = h;
	if (asa_blit_set(rv, pixfmt)) {
		free(rv);
		return NULL;
	}
	rv->ptr = 0;
	rv->resvsize = h * 2;
	rv->unused = assp_cellalloc(rv);
	rv->issued = NULL;
	return rv;
}

void assp_fgroupfree(struct assp_fgroup *g)
{
	struct assp_frameref *ng = g->issued, *next;
	unsigned c;

	while (ng) {
		next = ng->next;
		assp_framekill(ng);
		ng = next;
	}
	free(g->unused);
	for (c = 0; c < g->ptr; c++)
		free(g->reservoir[c]);
	free(g);
}

