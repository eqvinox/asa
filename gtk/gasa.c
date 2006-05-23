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

gboolean gas_load()
{
	GtkWidget *dialog;
	gladewidget(filechooser);
	gladewidget(opt_unicodehaywire);
	gladewidget(i_scripttype);
	gladewidget(t_proc);
	gladewidget(t_parse);
	gchar *curname;
	int fd;
	void *data;
	struct ssa_line *l;
	struct stat st;
	struct rusage bef, mid, aft;
	long usec1, usec2;
	int rv, errc, lines;
	int uopt;
	gchar *text;
	
	curname = gtk_file_chooser_get_filename(
		GTK_FILE_CHOOSER(filechooser));
	uopt = !gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(opt_unicodehaywire));

	if ((fd = open(curname, O_RDONLY)) == -1) {
		gas_error("Error opening %s:\n(%d) %s", curname,
			errno, strerror(errno));
		g_free(curname);
		return FALSE;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		close(fd);
		gas_error("Error mmap'ing %s:\n(%d) %s", curname,
			errno, strerror(errno));
		g_free(curname);
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
		g_free(curname);
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

	gtk_entry_set_text(GTK_ENTRY(i_scripttype),
		output.version == SSAVV_UNDEF 	? "undefined / fuzzy mode" :
		output.version == SSAVV_4 	? "SSA v4.0 / original" :
		output.version == SSAVV_4P	? "ASS (v4.0+)" :
		output.version == SSAVV_4PP	? "ASS2 (v4.0++)" :
						  "unknown");

	text = g_strdup_printf("%6.4f s", (float)usec1 / 1000000.0f);
	gtk_entry_set_text(GTK_ENTRY(t_parse), text);
	g_free(text);
	text = g_strdup_printf("%6.4f s", (float)usec2 / 1000000.0f);
	gtk_entry_set_text(GTK_ENTRY(t_proc), text);
	g_free(text);

	g_free(curname);
	return TRUE;
}

void gas_open_ok(GtkButton *button, gpointer user_data)
{
	gladewidget(mainw);
	gladewidget(load);
	gladewidget(filechooser);

	gtk_window_set_transient_for(GTK_WINDOW(load),
		GTK_WINDOW(filechooser));
	gtk_widget_show(load);

	if (!gas_load()) {
		gtk_widget_hide(load);
		return;
	}
	something_open = 1;

	gtk_widget_hide(load);
	gtk_widget_hide(filechooser);
	gtk_widget_hide(glade_xml_get_widget(xml, "load_align"));
	gtk_widget_show(mainw);
	set_pane(0);
}

void gas_open_cancel(GtkButton *button, gpointer user_data)
{
	if (!something_open) {
		gtk_main_quit();
		return;
	}
	gtk_widget_hide(glade_xml_get_widget(xml, "filechooser"));
}

int main(int argc, char **argv, char **envp)
{
	asaf_init();
	gtk_init(&argc, &argv);
	aed_init();
    	xml = glade_xml_new(ASA_GLADE, NULL, NULL);
	glade_xml_signal_autoconnect(xml);
	aed_create(&errdisp, xml, "warnview", "warnline", "warninfo", "warnnext", "warnprev");
	gtk_main();
    
	return 0;
}

