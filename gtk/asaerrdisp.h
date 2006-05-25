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

#ifndef _ASAERRDISP_H
#define _ASAERRDISP_H

struct asaerrdisp {
	GtkWidget *list, *src, *detl, *bprev, *bnext;

	GtkListStore *errstor;
	GtkTreeSelection *errsel;

	GtkTextBuffer *detlbuf, *srcbuf;
	GtkTextMark *errmark;

	struct ssa *ssa;
};

extern void aed_init();
extern void aed_create(struct asaerrdisp *disp, GladeXML *xml,
	const gchar *listname, const gchar *srcname, const gchar *detlname,
	const gchar *bnext, const gchar *bprev);
extern unsigned aed_open(struct asaerrdisp *disp, struct ssa *ssa,
	struct ssa_vm *vm);

#endif /* _ASAERRDISP_H */
