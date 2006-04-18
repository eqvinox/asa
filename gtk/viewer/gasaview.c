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

#include "ssa.h"
#include "ssavm.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#ifdef HAVE_GETOPT
#include <getopt.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <iconv.h>

#include "asaerrdisp.h"
#include "fileed.h"

#include "asafont.h"

struct asaviewwin {
	GtkWidget *form, *tab, *ucsopt, *rendlist;
	GtkTreeStore *rendstor;

	struct fileed ssa;
	struct asaerrdisp errdisp;
	struct ssa output;
	struct ssa_vm vm;
	iconv_t ic;
};

static gboolean gvw_delete_event(GtkWidget *widget, GdkEvent *event,
	gpointer data)
{
	return FALSE;
}

static void gvw_destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

#define gvw_error(...) \
	dialog = gtk_message_dialog_new(GTK_WINDOW(wnd->form), \
		GTK_DIALOG_DESTROY_WITH_PARENT, \
		GTK_MESSAGE_ERROR, \
		GTK_BUTTONS_CLOSE, \
		__VA_ARGS__ ); \
	gtk_dialog_run(GTK_DIALOG(dialog)); \
	gtk_widget_destroy(dialog);

#define VIEW_W 640
#define VIEW_H 480

unsigned char graybuf[VIEW_W * VIEW_H];

static void gvw_open(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	struct asaviewwin *wnd = (struct asaviewwin *) data;
	struct ssa_frag *f;
	struct ssa_line *l;
	const gchar *curname;
	struct stat st;
	struct rusage bef, mid, aft;
	long usec1, usec2;
	int fd;
	int rv, errc, lines;
	int uopt;
	
	curname = gtk_entry_get_text(GTK_ENTRY(wnd->ssa.ed));
	uopt = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wnd->ucsopt));

	if ((fd = open(curname, O_RDONLY)) == -1) {
		gvw_error("Error opening %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		close(fd);
		gvw_error("Error mmap'ing %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return;
	};

	wnd->output.ignoreenc = uopt;

	getrusage(RUSAGE_SELF, &bef);
	if ((rv = ssa_lex(&wnd->output, data, st.st_size))) {
		munmap(data, 0);
		close(fd);
		gvw_error("Fundamental parse error in %s:\n%s", curname,
			rv == 1 ? "File too short" :
			"Invalid Unicode data");
		return;
	}
	getrusage(RUSAGE_SELF, &mid);
	ssav_create(&wnd->vm, &wnd->output);
	getrusage(RUSAGE_SELF, &aft);
	usec1 = (mid.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(mid.ru_utime.tv_usec - bef.ru_utime.tv_usec);
	usec2 = (aft.ru_utime.tv_sec - mid.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - mid.ru_utime.tv_usec);

	munmap(data, 0);
	close(fd);

	gtk_tree_store_clear(wnd->rendstor);
	f = wnd->vm.fragments;
	while (f) {
		struct ssav_node *n;
		char buf0[512], buf1[512], buf2[512];
		long h, m, s, ms;
		unsigned idx;
		GtkTreeIter parent, child, grandchild;

		ms = (int)(f->start * 1000.0);
		h = ms / 3600000;
		ms %=    3600000;
		m = ms /   60000;
		ms %=      60000;
		s = ms /    1000;
		ms %=       1000;
		snprintf(buf1, 63, "%01ld:%02ld:%02ld.%03ld", h, m, s, ms);

		gtk_tree_store_append(wnd->rendstor, &parent, NULL);
		gtk_tree_store_set(wnd->rendstor, &parent,
			0, buf1,
			-1);
		for (idx = 0; idx < f->nrend; idx++) {
			gtk_tree_store_append(wnd->rendstor, &child, &parent);
			gtk_tree_store_set(wnd->rendstor, &child,
				0, "node",
				-1);
			n = f->lines[idx]->node_first;
			while (n) {
				char *pos = buf0;
				unsigned *ptr = n->indici, *dst = n->indici + n->nchars;

				snprintf(buf1, 511, "%p <%d>", (void *)n->params, n->params->nref);
				snprintf(buf2, 511, "%4.1lfpt %s", n->params->f.size, n->params->f.name);
				pos += snprintf(pos, sizeof(buf0) - (pos - buf0), "%d: ", n->nchars);
				while (ptr < dst)
					pos += snprintf(pos, sizeof(buf0) - (pos - buf0), "%02X ", *ptr++);

				gtk_tree_store_append(wnd->rendstor, &grandchild, &child);
				gtk_tree_store_set(wnd->rendstor, &grandchild,
					0, buf0,
					1, buf1,
					2, buf2,
					-1);
				n = n->next;
			}
		}
		f = f->next;
	}

/*	ssa_render(&wnd->rend, 1.0, graybuf,
		VIEW_W, VIEW_H); */

	errc = aed_open(&wnd->errdisp, &wnd->output);

	lines = 0;
	l = wnd->output.line_first;
	while (l)
		lines++, l = l->next;

	dialog = gtk_message_dialog_new(GTK_WINDOW(wnd->form),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_CLOSE,
		"%d lines, %d errors\n%6.4fs parse time,\n%6.4fs preload time.",
		lines, errc, (float)usec1 / 1000000.0f, (float)usec2 / 1000000.0f);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

}

static void gvw_info(GtkWidget *widget, gpointer data)
{
	struct asaviewwin *wnd = (struct asaviewwin *) data;
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(wnd->form),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_CLOSE,
		"ASA viewer / checker / parser testing program\n\n%s\n\n"
		"asa, Copyright (C) 2004, 2005, 2006 David Lamparter\n"
		"asa comes with ABSOLUTELY NO WARRANTY;\n"
		"This is free software, and you are welcome to\n"
		"redistribute it under certain conditions; for details\n"
		"see the LICENSE file included with the program source.\n",
		ssa_lexer_version);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

gboolean gvw_va_expose(GtkWidget *widget, GdkEventExpose *event,
	gpointer user_data)
{
	gdk_draw_gray_image(widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
		0, 0, VIEW_W, VIEW_H, GDK_RGB_DITHER_NONE, graybuf, VIEW_W);
	return TRUE;
}

/*

 * GtkWindow
  + GtkVBox
   + GtkTable
    - File edits
   + GtkNotebook
    + Load page
    + Errors page
     + GtkVBox
    + View page

 */
static void gvw_dlg(struct asaviewwin *wnd)
{
	GtkWidget *nb;

	wnd->form = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(wnd->form), "delete_event",
		G_CALLBACK(gvw_delete_event), wnd);
	g_signal_connect(G_OBJECT(wnd->form), "destroy",
		G_CALLBACK(gvw_destroy), wnd);
	gtk_container_set_border_width(GTK_CONTAINER(wnd->form), 5);
	gtk_window_set_title(GTK_WINDOW(wnd->form), "gASAview 0.1");

	nb = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_TOP);

	{
		GtkWidget *tab, *frame, *pbox, *info;

		tab = gtk_table_new(4, 3, FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(tab), 5);

		fe_new(&wnd->ssa, GTK_TABLE(tab), GTK_WINDOW(wnd->form), 0,
			"SSA:", "_Load", "Load SSA...");
		g_signal_connect(G_OBJECT(wnd->ssa.open), "clicked",
			G_CALLBACK(gvw_open), wnd);

		frame = gtk_frame_new("Parser options");
		pbox = gtk_vbox_new(FALSE, 2);
		gtk_container_set_border_width(GTK_CONTAINER(pbox), 5);

		wnd->ucsopt = gtk_check_button_new_with_mnemonic(
			"_Ignore encoding options for Unicode");

		gtk_container_add(GTK_CONTAINER(pbox), wnd->ucsopt);
		gtk_widget_show(wnd->ucsopt);

		gtk_container_add(GTK_CONTAINER(frame), pbox);
		gtk_widget_show(pbox);

		gtk_table_attach(GTK_TABLE(tab), frame,
			0, 4, 1, 2,
			GTK_FILL, 0,
			2, 5);
		gtk_widget_show(frame);

		info = gtk_button_new_with_mnemonic("_Info");
		gtk_table_attach(GTK_TABLE(tab), info,
			2, 4, 3, 4,
			GTK_FILL, 0,
			2, 5);
		gtk_widget_show(info);
		g_signal_connect(G_OBJECT(info), "clicked",
			G_CALLBACK(gvw_info), wnd);
	
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab,
			gtk_label_new("Load"));
		gtk_widget_show(tab);
	}

	{
		GtkWidget *errdisp;

		errdisp = aed_create(&wnd->errdisp);
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), errdisp,
			gtk_label_new("Warnings"));
		gtk_widget_show(errdisp);
	}

	{
		GtkCellRenderer *rend;
		GtkWidget *frame;

		wnd->rendstor = gtk_tree_store_new(3,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
		wnd->rendlist = gtk_tree_view_new();

		rend = gtk_cell_renderer_text_new();
		gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(wnd->rendlist), -1,
			"Time/Text", rend, "text", 0, NULL);

		rend = gtk_cell_renderer_text_new();
		gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(wnd->rendlist), -1,
			"pset", rend, "text", 1, NULL);

		rend = gtk_cell_renderer_text_new();
		gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(wnd->rendlist), -1,
			"Font", rend, "text", 2, NULL);

/*		rend = gtk_cell_renderer_text_new();
		g_object_set(rend, "xalign", (gfloat)1.0f, NULL);
		gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(wnd->rendlist), -1,
			"ndisp/flags", rend, "text", 2, NULL); */

		gtk_tree_view_set_model(GTK_TREE_VIEW(wnd->rendlist),
			GTK_TREE_MODEL(wnd->rendstor));
		g_object_unref(wnd->rendstor);

		frame = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(frame),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(frame),
			GTK_SHADOW_IN);
		gtk_container_add(GTK_CONTAINER(frame), wnd->rendlist);
		gtk_widget_show(wnd->rendlist);
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), frame,
			gtk_label_new("debug"));
		gtk_widget_show(frame);
	}

	{
		GtkWidget *darea, *vbox;
	
		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

		darea = gtk_drawing_area_new ();
		gtk_signal_connect(GTK_OBJECT(darea), "expose-event",
			GTK_SIGNAL_FUNC(gvw_va_expose), NULL);
		gtk_widget_set_size_request(darea, VIEW_W, VIEW_H);
		gtk_container_add(GTK_CONTAINER(vbox), darea);
		gtk_widget_show(darea);

		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox,
			gtk_label_new("View"));
		gtk_widget_show(vbox);
	}
	
	gtk_container_add(GTK_CONTAINER(wnd->form), nb);
	gtk_widget_show(nb);
}

int main(int argc, char **argv, char **envp)
{
	struct asaviewwin wnd;

	asaf_init();
	wnd.ic = iconv_open("UTF-8", "MS-ANSI");
	gtk_init(&argc, &argv);
	aed_init();
	gvw_dlg(&wnd);
	
	gtk_widget_show(wnd.form);
	gtk_main ();
    
	return 0;
}

