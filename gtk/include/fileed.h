/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005  David Lamparter
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

#ifndef _FILEED_H
#define _FILEED_H

struct fileed {
	GtkWidget *lbl, *ed, *brws, *open;
	GtkWindow *fileparent;

	const gchar *title;
};

extern void fe_new(struct fileed *ed, GtkTable *tab, GtkWindow *selparent,
	signed row, const gchar *lbl, const gchar *btnlbl, const gchar *brwslbl);

#endif /* _FILEED_H */
