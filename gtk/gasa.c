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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "common.h"
#include "ssa.h"
#include "ssavm.h"

#include "asaerrdisp.h"

static int something_open = 0;
static GladeXML *xml;

gint gas_file_open()
{
	return 0;
}

struct panes {
	const gchar *panename, *togglename, *menuname;
} panes[] = {
	{"box_info", "tool_info", "menu_view_info"},
	{"box_warnings", "tool_warnings", "menu_view_warnings"},
	{"box_display", "tool_display", "menu_view_display"},
	{NULL, NULL, NULL}
};

static void set_pane(int idx)
{
	int i;
	for (i = 0; panes[i].panename; i++) {
		g_object_set(glade_xml_get_widget(xml, panes[i].panename),
			"visible", i == idx, NULL);
		g_object_set(glade_xml_get_widget(xml, panes[i].togglename),
			"active", i == idx, NULL);
	}
}

void gas_view_toggle(GtkWidget *widget, gpointer user_data)
{
	const gchar *name;
	int i;

	if (GTK_IS_TOGGLE_TOOL_BUTTON(widget)
		&& !gtk_toggle_tool_button_get_active(
			GTK_TOGGLE_TOOL_BUTTON(widget)))
		return;
	name = gtk_widget_get_name(widget);
	for (i = 0; panes[i].panename; i++)
		if (!strcmp(panes[i].togglename, name)
			|| !strcmp(panes[i].menuname, name)) {
			set_pane(i);
			return;
		}
}

#define gas_error(...) \
	dialog = gtk_message_dialog_new(GTK_WINDOW(filechooser), \
		GTK_DIALOG_DESTROY_WITH_PARENT, \
		GTK_MESSAGE_ERROR, \
		GTK_BUTTONS_CLOSE, \
		__VA_ARGS__ ); \
	gtk_dialog_run(GTK_DIALOG(dialog)); \
	gtk_widget_destroy(dialog);
#define gladewidget(x) GtkWidget *x = glade_xml_get_widget(xml, #x);

static struct ssa output;
static struct ssa_vm vm;
static struct asaerrdisp errdisp;

enum info_cols {
	INFOCOL_TITLE = 0,
	INFOCOL_VALUE,
	INFOCOL_MAX
};

static void gas_info_init()
{
	gladewidget(infotree);
	GtkListStore *model;
	GtkTreeView *view = GTK_TREE_VIEW(infotree);

	model = gtk_list_store_new(INFOCOL_MAX,
		G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(view, GTK_TREE_MODEL(model));

	gtk_tree_view_insert_column_with_attributes(view, -1,
		"Property", gtk_cell_renderer_text_new(),
		"text", INFOCOL_TITLE,
		NULL);
	gtk_tree_view_insert_column_with_attributes(view, -1,
		"Value", gtk_cell_renderer_text_new(),
		"text", INFOCOL_VALUE,
		NULL);
}

#define setinfo(name, field) do { \
	if (ssa->field.s) { \
		gchar *value = g_ucs4_to_utf8(ssa->field.s, \
			ssa->field.e - ssa->field.s, NULL, NULL, NULL); \
		gtk_list_store_append(model, &iter); \
		gtk_list_store_set(model, &iter, \
			INFOCOL_TITLE, name, \
			INFOCOL_VALUE, value, -1); \
		g_free(value); \
	} } while (0)

static void gas_info_set(struct ssa *ssa)
{
	gladewidget(infotree);
	GtkTreeView *view = GTK_TREE_VIEW(infotree);
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(view));
	GtkTreeIter iter;

	gtk_list_store_clear(model);

	setinfo("Title:", title);
	setinfo("Collisions:", collisions);
	setinfo("Original Script:", orig_script);
	setinfo("Original Translation:", orig_transl);
	setinfo("Original Edit:", orig_edit);
	setinfo("Original Timing:", orig_timing);
	setinfo("Synch Point:", synch_point);
	setinfo("Script updated by:", script_upd_by);
	setinfo("Update details:", upd_details);
}

static gboolean gas_load(const gchar *curname, gboolean uopt)
{
	GtkWidget *dialog;
	gladewidget(filechooser);
	gladewidget(infotext);
	GtkTextBuffer *infobuf = gtk_text_view_get_buffer(
		GTK_TEXT_VIEW(infotext));
	char buf[1024];
	int fd;
	void *data;
	struct ssa_line *l;
	struct stat st;
	struct rusage bef, mid, aft;
	long usec1, usec2;
	int rv, errc, lines;
	
	if ((fd = open(curname, O_RDONLY)) == -1) {
		gas_error("Error opening %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return FALSE;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		close(fd);
		gas_error("Error mmap'ing %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return FALSE;
	};

	memset(&output, 0, sizeof(output));
	output.ignoreenc = uopt;

	getrusage(RUSAGE_SELF, &bef);
	if ((rv = ssa_lex(&output, data, st.st_size))) {
		munmap(data, 0);
		close(fd);
		gas_error("Fundamental parse error in %s:\n%s", curname,
			rv == 1 ? "File too short" :
			"Invalid Unicode data");
		return FALSE;
	}
	getrusage(RUSAGE_SELF, &mid);
	ssav_create(&vm, &output);
	getrusage(RUSAGE_SELF, &aft);
	usec1 = (mid.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(mid.ru_utime.tv_usec - bef.ru_utime.tv_usec);
	usec2 = (aft.ru_utime.tv_sec - mid.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - mid.ru_utime.tv_usec);

	munmap(data, 0);
	close(fd);

	errc = aed_open(&errdisp, &output, &vm);

	lines = 0;
	l = output.line_first;
	while (l)
		lines++, l = l->next;

	snprintf(buf, sizeof(buf),
		"Script Type: %s\n"
		"Load time: %6.4fs parsing, %6.4fs processing\n"
		"%d lines, %d warnings",
		output.version == SSAVV_UNDEF 	? "undefined / fuzzy mode" :
		output.version == SSAVV_4 	? "SSA v4.0 / original" :
		output.version == SSAVV_4P	? "ASS (v4.0+)" :
		output.version == SSAVV_4PP	? "ASS2 (v4.0++)" :
						  "unknown",
		(float)usec1 / 1000000.0f,
		(float)usec2 / 1000000.0f,
		lines, errc);

	gtk_text_buffer_set_text(infobuf, buf, -1);
	gas_info_set(&output);
	return TRUE;
}

#if 0
	gladewidget(load);
	gtk_window_set_transient_for(GTK_WINDOW(load),
		GTK_WINDOW(filechooser));
	gtk_widget_show(load);
	...
	gtk_widget_hide(load);
#endif

gboolean gas_open_do(const gchar *curname, gboolean uopt)
{
	gladewidget(mainw);
	gladewidget(filechooser);

	if (!gas_load(curname, uopt))
		return FALSE;

	something_open = 1;

	gtk_widget_hide(filechooser);
	gtk_widget_hide(glade_xml_get_widget(xml, "load_align"));
	gtk_widget_show(mainw);
	set_pane(0);
	return TRUE;
}

void gas_open_ok(GtkButton *button, gpointer user_data)
{
	gladewidget(filechooser);
	gladewidget(opt_unicodehaywire);
	gchar *curname;
	gboolean uopt;

	curname = gtk_file_chooser_get_filename(
		GTK_FILE_CHOOSER(filechooser));
	uopt = !gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(opt_unicodehaywire));
	gas_open_do(curname, uopt);
	g_free(curname);
}

void gas_open_cancel(GtkButton *button, gpointer user_data)
{
	if (!something_open) {
		gtk_main_quit();
		return;
	}
	gtk_widget_hide(glade_xml_get_widget(xml, "filechooser"));
}

void gas_help_info(GtkWidget *widget, gpointer user_data)
{
	gladewidget(about);
	gladewidget(mainw);

	gtk_window_set_transient_for(GTK_WINDOW(about),
		GTK_WINDOW(mainw));
	gtk_widget_show(about);
}

static gboolean unicodefe = FALSE;

static GOptionEntry entries[] =
{
  { "unicode-fe", 'U', 0, G_OPTION_ARG_NONE, &unicodefe,
  	"Interpret (screw up) \\fe in Unicode files", NULL },
  { NULL, 0, 0, 0, NULL, NULL, NULL }
};

int main(int argc, char **argv, char **envp)
{
	GError *error = NULL;

	asaf_init();
	gtk_init_with_args(&argc, &argv, "[<SUBFILE>]",
		entries, NULL, &error);
	aed_init();
    	xml = glade_xml_new(ASA_GLADE, NULL, NULL);
	glade_xml_signal_autoconnect(xml);
	aed_create(&errdisp, xml, "warnview", "warnline", "warninfo",
		"warnnext", "warnprev");
	gas_info_init();
	if (unicodefe) {
		gladewidget(opt_unicodehaywire);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt_unicodehaywire), TRUE);
	}
	if (argc != 2 || !gas_open_do(argv[1], unicodefe)) {
		gladewidget(filechooser);
		gtk_widget_show(filechooser);
	}
	gtk_main();
    
	return 0;
}

