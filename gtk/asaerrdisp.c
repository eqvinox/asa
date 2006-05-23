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

#include "common.h"
#include "asaerror.h"

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "asaerrdisp.h"

#define DISPWRAP
#define NEAT_SPACE

enum {
	COL_LINE = 0,
	COL_COLUMN,
	COL_ERROR,
	COL_PTR,
	COL_PIXBUF,
	COL_VM,
	NUM_COLS
};

#include "inline_pngs.h"
GdkPixbuf *pixs[5];
char *severity[5] = {"oddness", "normal", "warning", "error", "critical"};

static void aed_next(GtkWidget *widget, gpointer data)
{
	struct asaerrdisp *disp = (struct asaerrdisp *) data;
	GtkTreeIter iter, olditer;
	GtkTreePath *path;
	gboolean havesel;

	havesel = gtk_tree_selection_get_selected(disp->errsel, NULL, &iter);
	if (havesel) {
		olditer = iter;
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(disp->errstor), &iter))
			gtk_tree_selection_unselect_iter(disp->errsel, &olditer);
		else
			return;
	} else
		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(disp->errstor), &iter);
	gtk_tree_selection_select_iter(disp->errsel, &iter);
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(disp->errstor), &iter);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(disp->list),
		path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
}

static void aed_prev(GtkWidget *widget, gpointer data)
{
	struct asaerrdisp *disp = (struct asaerrdisp *) data;
	GtkTreeIter iter, olditer;
	GtkTreePath *path;
	gboolean havesel;

	havesel = gtk_tree_selection_get_selected(disp->errsel, NULL, &iter);
	if (havesel) {
		olditer = iter;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(disp->errstor), &iter);
		if (gtk_tree_path_prev(path)) {
			gtk_tree_selection_unselect_iter(disp->errsel, &olditer);
			gtk_tree_model_get_iter(GTK_TREE_MODEL(disp->errstor), &iter, path);
		} else
			return;
	} else {
		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(disp->errstor), &iter);
		olditer = iter;
		while (gtk_tree_model_iter_next(GTK_TREE_MODEL(disp->errstor), &iter))
			olditer = iter;
		iter = olditer;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(disp->errstor), &iter);
	}
	gtk_tree_selection_select_iter(disp->errsel, &iter);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(disp->list),
		path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
}

static void aed_enablebtns(struct asaerrdisp *disp, GtkTreeIter *iter)
{
	GtkTreeIter niter = *iter;
	GtkTreePath *path;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(disp->errstor), iter);
	gtk_widget_set_sensitive(disp->bprev,
		gtk_tree_path_prev(path));
	gtk_tree_path_free(path);
	gtk_widget_set_sensitive(disp->bnext,
		gtk_tree_model_iter_next(GTK_TREE_MODEL(disp->errstor), &niter));
}

static GdkColor srcols[4] = {
	{0, 0xFFFF, 0xCCCC, 0xAAAA},
	{0, 0xAAAA, 0x0000, 0x0000},
	{0, 0xFFFF, 0xFFFF, 0xBBBB},
	{0, 0x6666, 0x4444, 0x0000}};

static void aed_filliter(struct asaerrdisp *disp, struct ssa_error *e,
	GtkTreeIter *iter)
{
	char lineno[256], column[256];
	snprintf(lineno, sizeof(lineno), "%d", e->lineno);
	snprintf(column, sizeof(column), "%d", e->column + 1);
	gtk_list_store_set(disp->errstor, iter,
		COL_LINE, lineno,
		COL_COLUMN, column,
		COL_ERROR, ssaec[e->errorcode].sh,
		COL_PTR, e,
		COL_PIXBUF, pixs[ssaec[e->errorcode].sev],
		COL_VM, 0,
		-1);
}

static void aed_filliter_vm(struct asaerrdisp *disp, struct ssav_error *e,
	GtkTreeIter *iter)
{
	char lineno[256] = "";
	if (e->context == SSAVECTX_DIALOGUE)
		snprintf(lineno, sizeof(lineno), "%d", e->i.dlg.lineno);
	gtk_list_store_set(disp->errstor, iter,
		COL_LINE, lineno,
		COL_COLUMN, "",
		COL_ERROR, ssaec[e->errorcode].sh,
		COL_PTR, e,
		COL_PIXBUF, pixs[ssaec[e->errorcode].sev],
		COL_VM, 1,
		-1);
}

unsigned aed_open(struct asaerrdisp *disp, struct ssa *ssa, struct ssa_vm *vm)
{
	GtkTreeIter iter;
	struct ssa_error *e;
	struct ssav_error *ve;
	unsigned errc;
	
	disp->ssa = ssa;
	
	gtk_list_store_clear(disp->errstor);
	errc = 0;
	e = ssa->errlist;
	while (e) {
		gtk_list_store_append(disp->errstor, &iter);
		aed_filliter(disp, e, &iter);
		e = e->next;
		errc++;
	}
	ve = vm->errlist;
	while (ve) {
		gtk_list_store_append(disp->errstor, &iter);
		aed_filliter_vm(disp, ve, &iter);
		ve = ve->next;
		errc++;
	}

	gtk_widget_set_sensitive(disp->bnext, errc != 0);
	gtk_widget_set_sensitive(disp->bprev, errc != 0);

	return errc;
}

static int aed_update_sel_lex(struct asaerrdisp *disp, GtkTreeModel *mod,
	GtkTreeIter *iter)
{
	struct ssa_error *e;
	char *tagname;
	GtkTextIter ti1, ti2;
	unsigned pos = 0, rem;
	unsigned char *src;

	gtk_tree_model_get(mod, iter, COL_PTR, &e, -1);
	src = (unsigned char *)e->textline;
	rem = e->linelen + 1;
	
	gtk_widget_show(disp->src);
	if (disp->errmark)
		gtk_text_buffer_delete_mark(disp->srcbuf, disp->errmark);
	disp->errmark = NULL;
	
	gtk_text_buffer_get_start_iter(disp->srcbuf, &ti1);
	gtk_text_buffer_get_end_iter(disp->srcbuf, &ti2);
	gtk_text_buffer_delete(disp->srcbuf, &ti1, &ti2);

	do {
		unsigned char tba[15];
		int tbl = 1;
		if (rem > 1) {
			/*
			 * WARNING: 0x2423 actually managed to
			 * crash my gtk+ 2.6.7, sending it into a
			 * dead loop somewhere in the font rendering
			 * code. USE AT YOUR OWN RISK.
			 */
#ifdef NEAT_SPACE
			if (*src > 0x20 && *src < 0x80)
#else
			if (*src >= 0x20 && *src < 0x80)
#endif
				tba[0] = *src;
			else if (*src < 0x20) {
				tba[0] = 224 +  (0x2400 >> 12);
				tba[1] = 128 + ((0x2400 >> 6) & 0x3F);
				tba[2] = 128 + *src;
				tbl = 3;
			} else if (*src > 0x80) {
				snprintf((char *)tba, 14,
					"‹%02X›%n",
					(unsigned)*src, &tbl);
#ifdef NEAT_SPACE
			} else if (*src == 0x20) {
				tba[0] = 224 +  (0x2423 >> 12);
				tba[1] = 128 + ((0x2423 >> 6) & 0x3F);
				tba[2] = 128 + ((0x2423 >> 0) & 0x3F);
				tbl = 3;
#endif
			} else if (*src == 0x80) {
				tba[0] = 224 +  (0x2421 >> 12);
				tba[1] = 128 + ((0x2421 >> 6) & 0x3F);
				tba[2] = 128 + ((0x2421 >> 0) & 0x3F);
				tbl = 3;
			}
		} else {
			tba[0] = 224 +  (0x2424 >> 12);
			tba[1] = 128 + ((0x2424 >> 6) & 0x3F);
			tba[2] = 128 + ((0x2424 >> 0) & 0x3F);
			tba[3] = '\n';
			tbl = 3;
		}
		
		tagname = "base";
		if (pos == e->column) {
			tagname = "err";
			disp->errmark = gtk_text_buffer_create_mark(disp->srcbuf,
				NULL, &ti1, TRUE);
		} else if (pos == e->origin)
			tagname = "origin";
		gtk_text_buffer_insert_with_tags_by_name(disp->srcbuf, &ti1,
			(char *)tba, tbl, tagname, NULL);
		src++, rem--, pos++;
	} while (rem);
	if (disp->errmark)
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(disp->src),
			disp->errmark,
			0.0, TRUE, 0.5, 0.5);
	gtk_text_buffer_insert(disp->srcbuf, &ti1, " ", 1);
	return e->errorcode;
}

static int aed_update_sel_vm(struct asaerrdisp *disp, GtkTreeModel *mod,
	GtkTreeIter *iter)
{
	struct ssav_error *ve;
	GtkTextIter ti1, ti2;
	char buffer[256];

	gtk_tree_model_get(mod, iter, COL_PTR, &ve, -1);

	gtk_text_buffer_get_start_iter(disp->srcbuf, &ti1);
	gtk_text_buffer_get_end_iter(disp->srcbuf, &ti2);
	gtk_text_buffer_delete(disp->srcbuf, &ti1, &ti2);

	gtk_text_buffer_insert_with_tags_by_name(disp->srcbuf, &ti1,
		"(source text not available for VM errors)\n\n", -1,
		"vminfo", NULL);
	if (ve->context == SSAVECTX_STYLE) {
		snprintf(buffer, sizeof(buffer), "Style: %s\n",
			ve->i.style.stylename);
		gtk_text_buffer_insert(disp->srcbuf, &ti1,
			buffer, -1);
	} else if (ve->context == SSAVECTX_DIALOGUE) {
		gtk_text_buffer_insert(disp->srcbuf, &ti1, "Node type: ", -1);
		snprintf(buffer, sizeof(buffer), "\\%s\n",
			ssa_typename(ve->i.dlg.ntype));
		gtk_text_buffer_insert_with_tags_by_name(disp->srcbuf, &ti1,
			buffer, -1, "vmtype", NULL);
	}
	return ve->errorcode;
}

static void aed_update_sel_int(struct asaerrdisp *disp, GtkTreeModel *mod,
	GtkTreeIter *iter)
{
	GtkTextIter ti1, ti2;
	int is_vm, errorcode;
	gtk_tree_model_get(mod, iter, COL_VM, &is_vm, -1);

	if (is_vm)
		errorcode = aed_update_sel_vm(disp, mod, iter);
	else
		errorcode = aed_update_sel_lex(disp, mod, iter);

	gtk_text_buffer_get_start_iter(disp->detlbuf, &ti1);
	gtk_text_buffer_get_end_iter(disp->detlbuf, &ti2);
	gtk_text_buffer_delete(disp->detlbuf, &ti1, &ti2);

	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		ssaec[errorcode].sh, -1, "bold", NULL);

	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		"\n(severity: ", 12, "sev", NULL);
	gtk_text_buffer_insert_pixbuf(disp->detlbuf, &ti1,
		pixs[ssaec[errorcode].sev]);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		" ", 1, "sev", NULL);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		severity[ssaec[errorcode].sev], -1, "sev",
		(ssaec[errorcode].sev > 1 ? "bold" : NULL), NULL);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		")\n\n", 3, "sev", NULL);
	if (ssaec[errorcode].add)
		gtk_text_buffer_insert(disp->detlbuf, &ti1,
			ssaec[errorcode].add, -1);
	else {
		gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
			"no additional information availiable", -1, "gray",
			NULL);
	}
	if (ssaec[errorcode].warn) {
		gtk_text_buffer_insert(disp->detlbuf, &ti1, "\n\n", 2);
		gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
			ssaec[errorcode].warn, -1, "bold", "red", NULL);
	}
	gtk_text_buffer_insert(disp->detlbuf, &ti1, "\n", 1);
}

static gboolean aed_selfunc(GtkTreeSelection *sel,
	GtkTreeModel *mod, GtkTreePath *path,
	gboolean currently_sel, gpointer data)
{
	struct asaerrdisp *disp = (struct asaerrdisp *) data;
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(mod, &iter, path)) {
		if (currently_sel)
			return TRUE;

		aed_enablebtns(disp, &iter);
		aed_update_sel_int(disp, mod, &iter);
	}
	return TRUE;
}

void aed_create(struct asaerrdisp *disp, GladeXML *xml,
	const gchar *listname, const gchar *srcname, const gchar *detlname,
	const gchar *bnext, const gchar *bprev)
{
	GtkCellRenderer *rend;
	PangoFontDescription *font_desc;
	GtkTextView *tv;
	GtkTreeViewColumn *col;
	GdkColormap *colormap;
	gboolean coloralloc[4];

	memset(disp, 0, sizeof(*disp));

/* error list storage */
	disp->errstor = gtk_list_store_new(NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING,
		G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_PIXBUF,
		G_TYPE_UINT);

/* error list itself */
	disp->list = glade_xml_get_widget(xml, listname);
//	gtk_widget_set_size_request(disp->list, 400, 100);

/* error list: 1st column - Line */
	rend = gtk_cell_renderer_text_new();
	g_object_set(rend, "xalign", (gfloat)1.0f, NULL);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(disp->list), -1,
		"Line", rend,
		"text", COL_LINE,
		NULL);
	col = gtk_tree_view_get_column(GTK_TREE_VIEW(disp->list), 0);
	gtk_tree_view_column_set_min_width(col, 64);
	
/* error list: 2nd column - Column */
	rend = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(disp->list), -1,
		"Column", rend,
		"text", COL_COLUMN,
		NULL);
	col = gtk_tree_view_get_column(GTK_TREE_VIEW(disp->list), 1);
	gtk_tree_view_column_set_min_width(col, 64);

/* error list: 3rd column - Error (2 renderers, pixmap + text)*/
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Error");
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, rend, FALSE);
	gtk_tree_view_column_add_attribute(col, rend,
		"pixbuf", COL_PIXBUF);
	rend = gtk_cell_renderer_text_new();
	g_object_set(rend, "text", " ", NULL);
	gtk_tree_view_column_pack_start(col, rend, FALSE);
	rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, rend, TRUE);
	gtk_tree_view_column_add_attribute(col, rend,
		"text", COL_ERROR);
	gtk_tree_view_column_set_min_width(col, 16);
	gtk_tree_view_insert_column(GTK_TREE_VIEW(disp->list), col, -1);

/* error list: selection */
	disp->errsel = gtk_tree_view_get_selection(GTK_TREE_VIEW(disp->list));
	gtk_tree_selection_set_mode(disp->errsel, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(disp->errsel, aed_selfunc,
		disp, NULL);

/* error list: add storage */
	gtk_tree_view_set_model(GTK_TREE_VIEW(disp->list),
		GTK_TREE_MODEL(disp->errstor));

/* prev/next buttons */
	disp->bnext = glade_xml_get_widget(xml, bnext);
	g_signal_connect(G_OBJECT(disp->bnext), "clicked",
		G_CALLBACK(aed_next), disp);
	disp->bprev = glade_xml_get_widget(xml, bprev);
	g_signal_connect(G_OBJECT(disp->bprev), "clicked",
		G_CALLBACK(aed_prev), disp);

/* source view */
	disp->src = glade_xml_get_widget(xml, srcname);
	tv = GTK_TEXT_VIEW(disp->src);
	disp->srcbuf = gtk_text_view_get_buffer(tv);

/* source view: attributes */
	gtk_text_view_set_editable(tv, 0);
	gtk_text_view_set_right_margin(tv, 4);
	gtk_text_view_set_left_margin(tv, 4);
#ifdef DISPWRAP
	gtk_text_view_set_wrap_mode(tv, GTK_WRAP_CHAR);
	gtk_widget_set_size_request(disp->src, 400, 45);
#else
	gtk_text_view_set_wrap_mode(tv, GTK_WRAP_NONE);
	gtk_widget_set_size_request(disp->src, 400, 30);
#endif

/* detail infos */
	disp->detl = glade_xml_get_widget(xml, detlname);
	tv = GTK_TEXT_VIEW(disp->detl);
	disp->detlbuf = gtk_text_view_get_buffer(tv);

/* detail infos: attributes */
	gtk_text_view_set_editable(tv, 0);
	gtk_text_view_set_right_margin(tv, 4);
	gtk_text_view_set_left_margin(tv, 4);
	gtk_text_view_set_wrap_mode(tv, GTK_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(disp->detl, 400, 140);

/* highlighting tags, detail infos */
	gtk_text_buffer_create_tag(disp->detlbuf, "sev",
		"size-points", (double)9.0,
		NULL);
	gtk_text_buffer_create_tag(disp->detlbuf, "bold",
		"weight", 700,
		"weight-set", 1,
		NULL);
	gtk_text_buffer_create_tag(disp->detlbuf, "gray",
		"foreground", "gray",
		NULL);
	gtk_text_buffer_create_tag(disp->detlbuf, "red",
		"foreground", "red",
		NULL);
/* highlighting tags, source view */
	colormap = gtk_widget_get_colormap(disp->src);
	gdk_colormap_alloc_colors(colormap, srcols,
		4, FALSE, TRUE, coloralloc);

/* source view: font */
/* XXX make font configurable? */
	gtk_text_buffer_create_tag(disp->srcbuf, "base",
		"font", "Andale Mono",
		NULL);
	gtk_text_buffer_create_tag(disp->srcbuf, "err",
		"font", "Andale Mono",
		"background-gdk", &srcols[0],
		"foreground-gdk", &srcols[1],
		NULL);
	gtk_text_buffer_create_tag(disp->srcbuf, "origin",
		"font", "Andale Mono",
		"background-gdk", &srcols[2],
		"foreground-gdk", &srcols[3],
		NULL);
	font_desc = pango_font_description_new();
	pango_font_description_set_style(font_desc, PANGO_STYLE_ITALIC);
	gtk_text_buffer_create_tag(disp->srcbuf, "vminfo",
		"font-desc", font_desc,
		"size-points", (double)9.0,
		NULL);
	pango_font_description_free(font_desc);
	gtk_text_buffer_create_tag(disp->srcbuf, "vmtype",
		"weight", 700,
		"weight-set", 1,
		NULL);

	return;
}

/* XXX: _destroy.
 * don't forget g_object_unref(disp->errstor);
 * - that was in _create originally, but that's not
 * quite right. we keep a reference, and therefore
 * we should hold it.
 */

void aed_init()
{
	pixs[0] = gdk_pixbuf_new_from_inline(-1, bluepng, FALSE, 0);
	pixs[1] = gdk_pixbuf_new_from_inline(-1, greenpng, FALSE, 0);
	pixs[2] = gdk_pixbuf_new_from_inline(-1, yellowpng, FALSE, 0);
	pixs[3] = gdk_pixbuf_new_from_inline(-1, redpng, FALSE, 0);
	pixs[4] = gdk_pixbuf_new_from_inline(-1, purplepng, FALSE, 0);
}

