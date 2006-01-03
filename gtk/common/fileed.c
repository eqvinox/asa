#include <gtk/gtk.h>
#include "fileed.h"

static void fe_browse(GtkWidget *widget, gpointer data)
{
	struct fileed *ed = (struct fileed *) data;
	const gchar *curname;
	GtkWidget *filesel;

	filesel = gtk_file_chooser_dialog_new(ed->title,
		ed->fileparent, GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	
	curname = gtk_entry_get_text(GTK_ENTRY(ed->ed));
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel), curname);
	if (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_OK) {
		curname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));
		gtk_entry_set_text(GTK_ENTRY(ed->ed), curname);
	}

	gtk_widget_destroy(filesel);
}

void fe_new(struct fileed *ed, GtkTable *tab, GtkWindow *selparent,
	signed row, const gchar *lbl, const gchar *btnlbl, const gchar *brwslbl)
{
	ed->title = brwslbl;
	ed->fileparent = selparent;

	ed->ed = gtk_entry_new();
	gtk_widget_set_size_request(ed->ed, 280, 20);
	gtk_table_attach(tab, ed->ed,
		1, 2, row, row + 1,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0,
		4, 0);
	gtk_widget_show(ed->ed);
	
	ed->lbl = gtk_accel_label_new(lbl);
	gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(ed->lbl), ed->ed);
	gtk_table_attach(tab, ed->lbl,
		0, 1, row, row + 1,
		GTK_FILL, 0,
		0, 3);
	gtk_widget_show(ed->lbl);

	ed->brws = gtk_button_new_with_label("...");
	g_signal_connect(G_OBJECT(ed->brws), "clicked",
		G_CALLBACK(fe_browse), ed);
	gtk_table_attach(tab, ed->brws,
		2, 3, row, row + 1,
		GTK_FILL, 0,
		2, 0);
	gtk_widget_show(ed->brws);

	ed->open = gtk_button_new_with_mnemonic(btnlbl);
	gtk_table_attach(tab, ed->open,
		3, 4, row, row + 1,
		GTK_FILL, 0,
		2, 0);
	gtk_widget_show(ed->open);
}

