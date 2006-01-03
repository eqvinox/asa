#include "common.h"
#include "ssa.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "asaerrdisp.h"

#define DISPWRAP

enum {
	COL_LINE = 0,
	COL_COLUMN,
	COL_ERROR,
	COL_PTR,
	COL_PIXBUF,
	NUM_COLS
};

#include "inline_pngs.h"
GdkPixbuf *pixs[5];
char *severity[5] = {"oddness", "normal", "warning", "error", "critical"};

static GdkColor srcols[4] = {
	{0, 0xFFFF, 0xCCCC, 0xAAAA},
	{0, 0xAAAA, 0x0000, 0x0000},
	{0, 0xFFFF, 0xFFFF, 0xCCCC},
	{0, 0x6666, 0x4444, 0x0000}};

static void aed_filliter(struct asaerrdisp *disp, struct ssa_error *e,
	GtkTreeIter *iter)
{
	gtk_list_store_set(disp->errstor, iter,
		COL_LINE, e->lineno,
		COL_COLUMN, e->column + 1,
		COL_ERROR, ssaec[e->errorcode].sh,
		COL_PTR, e,
		COL_PIXBUF, pixs[ssaec[e->errorcode].sev],
		-1);
}

unsigned aed_open(struct asaerrdisp *disp, struct ssa *ssa)
{
	GtkTreeIter iter;
	struct ssa_error *e;
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
	return errc;
}

static void aed_update_sel_int(struct asaerrdisp *disp, GtkTreeModel *mod,
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
		
		tagname = NULL;
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

	gtk_text_buffer_get_start_iter(disp->detlbuf, &ti1);
	gtk_text_buffer_get_end_iter(disp->detlbuf, &ti2);
	gtk_text_buffer_delete(disp->detlbuf, &ti1, &ti2);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		ssaec[e->errorcode].sh, -1, "bold", NULL);

	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		"\n(severity: ", 12, "sev", NULL);
	gtk_text_buffer_insert_pixbuf(disp->detlbuf, &ti1,
		pixs[ssaec[e->errorcode].sev]);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		" ", 1, "sev", NULL);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		severity[ssaec[e->errorcode].sev], -1, "sev",
		(ssaec[e->errorcode].sev > 1 ? "bold" : NULL), NULL);
	gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
		")\n\n", 3, "sev", NULL);
	if (ssaec[e->errorcode].add)
		gtk_text_buffer_insert(disp->detlbuf, &ti1,
			ssaec[e->errorcode].add, -1);
	else {
		gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
			"no additional information availiable", -1, "gray",
			NULL);
	}
	if (ssaec[e->errorcode].warn) {
		gtk_text_buffer_insert(disp->detlbuf, &ti1, "\n\n", 2);
		gtk_text_buffer_insert_with_tags_by_name(disp->detlbuf, &ti1,
			ssaec[e->errorcode].warn, -1, "bold", "red", NULL);
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

		aed_update_sel_int(disp, mod, &iter);
	}
	return TRUE;
}

GtkWidget *aed_create(struct asaerrdisp *disp)
{
	GtkCellRenderer *rend;
	PangoFontDescription *font_desc;
	GtkWidget *frame, *tab;
	GtkTextView *tv;
	GtkTreeViewColumn *col;
	GdkColormap *colormap;
	gboolean coloralloc[4];

//	disp->errmark = NULL;
	memset(disp, 0, sizeof(*disp));

	/* reason for using a table: the 7th & 8th param to table_attach */
	tab = gtk_table_new(3, 1, FALSE);

/* error list storage */
	disp->errstor = gtk_list_store_new(NUM_COLS,
		G_TYPE_UINT, G_TYPE_UINT,
		G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_PIXBUF);

/* error list itself */
	disp->list = gtk_tree_view_new();
	gtk_widget_set_size_request(disp->list, 400, 100);

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

/* frame error list */
	frame = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(frame),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(frame),
		GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(frame), disp->list);
	gtk_widget_show(disp->list);

/* add frame to vbox */
	gtk_table_attach(GTK_TABLE(tab), frame,
		0, 1, 0, 1,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		0, 3);
	gtk_widget_show(frame);

/* source view */
	disp->src = gtk_text_view_new();
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

/* source view: font */
/* XXX make font configurable? */
	font_desc = pango_font_description_from_string("Andale Mono");
	gtk_widget_modify_font(disp->src, font_desc);
	pango_font_description_free(font_desc);

/* frame source view */
	frame = gtk_scrolled_window_new(NULL, NULL);
#ifdef DISPWRAP
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(frame),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#else
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(frame),
		GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
#endif
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(frame),
		GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(frame), disp->src);
	gtk_widget_show(disp->src);
	
/* add frame to vbox */
	gtk_table_attach(GTK_TABLE(tab), frame,
		0, 1, 1, 2,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		0,
		0, 3);
	gtk_widget_show(frame);

/* detail infos */
	disp->detl = gtk_text_view_new();
	tv = GTK_TEXT_VIEW(disp->detl);
	disp->detlbuf = gtk_text_view_get_buffer(tv);

/* detail infos: attributes */
	gtk_text_view_set_editable(tv, 0);
	gtk_text_view_set_right_margin(tv, 4);
	gtk_text_view_set_left_margin(tv, 4);
	gtk_text_view_set_wrap_mode(tv, GTK_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(disp->detl, 400, 140);

/* frame detail infos */
	frame = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(frame),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(frame),
		GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(frame), disp->detl);
	gtk_widget_show(disp->detl);
	
/* add frame to vbox */
	gtk_table_attach(GTK_TABLE(tab), frame,
		0, 1, 2, 3,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		0,
		0, 3);
	gtk_widget_show(frame);

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

	gtk_text_buffer_create_tag(disp->srcbuf, "err",
		"background-gdk", &srcols[0],
		"foreground-gdk", &srcols[1],
		NULL);
	gtk_text_buffer_create_tag(disp->srcbuf, "origin",
		"background-gdk", &srcols[2],
		"foreground-gdk", &srcols[3],
		NULL);

	return tab;
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

