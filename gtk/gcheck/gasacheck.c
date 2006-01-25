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
#include "ssa.h"

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

#include "fileed.h"
#include "asaerrdisp.h"

struct asacheckwin {
	GtkWidget *form, *tab, *ucsopt;

	struct fileed ssa;
	struct asaerrdisp errdisp;
	struct ssa output;
	iconv_t ic;
};

static gboolean gac_delete_event(GtkWidget *widget, GdkEvent *event,
	gpointer data)
{
	return FALSE;
}

static void gac_destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

#define gac_error(x...) \
	dialog = gtk_message_dialog_new(GTK_WINDOW(wnd->form), \
		GTK_DIALOG_DESTROY_WITH_PARENT, \
		GTK_MESSAGE_ERROR, \
		GTK_BUTTONS_CLOSE, \
		x ); \
	gtk_dialog_run(GTK_DIALOG(dialog)); \
	gtk_widget_destroy(dialog);

static void gac_open(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	struct asacheckwin *wnd = (struct asacheckwin *) data;
	const gchar *curname;
	struct ssa_line *l;
	struct stat st;
	struct rusage bef, mid, aft;
	long usec1, usec2;
	int fd;
	int rv, errc, lines;
	int uopt;
	
	curname = gtk_entry_get_text(GTK_ENTRY(wnd->ssa.ed));
	uopt = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wnd->ucsopt));

	if ((fd = open(curname, O_RDONLY)) == -1) {
		gac_error("Error opening %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		close(fd);
		gac_error("Error mmap'ing %s:\n(%d) %s", curname,
			errno, strerror(errno));
		return;
	};

	wnd->output.ignoreenc = uopt;

	getrusage(RUSAGE_SELF, &bef);
	if ((rv = ssa_lex(&wnd->output, data, st.st_size))) {
		munmap(data, 0);
		close(fd);
		gac_error("Fundamental parse error in %s:\n%s", curname,
			rv == 1 ? "File too short" :
			"Invalid Unicode data");
		return;
	}
	getrusage(RUSAGE_SELF, &mid);
	//ssa_prepare(&wnd->output);
	getrusage(RUSAGE_SELF, &aft);
	usec1 = (mid.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(mid.ru_utime.tv_usec - bef.ru_utime.tv_usec);
	usec2 = (aft.ru_utime.tv_sec - mid.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - mid.ru_utime.tv_usec);

	munmap(data, 0);
	close(fd);

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

static void gac_info(GtkWidget *widget, gpointer data)
{
	struct asacheckwin *wnd = (struct asacheckwin *) data;
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
		"see the LICENSE file included with the program source.\n"
		ssa_lexer_version);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void gac_dlg(struct asacheckwin *wnd)
{
	GtkWidget *frame, *box, *errdisp;

	wnd->form = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(wnd->form), "delete_event",
		G_CALLBACK(gac_delete_event), wnd);
	g_signal_connect(G_OBJECT(wnd->form), "destroy",
		G_CALLBACK(gac_destroy), wnd);
	gtk_container_set_border_width(GTK_CONTAINER(wnd->form), 8);
	gtk_window_set_title(GTK_WINDOW(wnd->form), "gASAcheck 0.3");

	/* 5 rows, 3 cols */
	wnd->tab = gtk_table_new(5, 3, FALSE);

	fe_new(&wnd->ssa, GTK_TABLE(wnd->tab), GTK_WINDOW(wnd->form), 0,
		"SSA:", "_Load", "Load SSA...");
	g_signal_connect(G_OBJECT(wnd->ssa.open), "clicked",
		G_CALLBACK(gac_open), wnd);

	frame = gtk_frame_new("Parser options");
	box = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 3);
	wnd->ucsopt = gtk_check_button_new_with_mnemonic("_Ignore encoding options for Unicode");
	gtk_container_add(GTK_CONTAINER(box), wnd->ucsopt);
	gtk_widget_show(wnd->ucsopt);
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_widget_show(box);
	gtk_table_attach(GTK_TABLE(wnd->tab), frame,
		0, 4, 1, 2,
		GTK_FILL, 0,
		2, 0);
	gtk_widget_show(frame);

	errdisp = aed_create(&wnd->errdisp);
	gtk_table_attach(GTK_TABLE(wnd->tab), errdisp,
		0, 4, 2, 3,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		GTK_FILL | GTK_SHRINK | GTK_EXPAND,
		0, 8);
	gtk_widget_show(errdisp);

	frame = gtk_button_new_with_mnemonic("_Info");
	gtk_table_attach(GTK_TABLE(wnd->tab), frame,
		3, 4, 3, 4,
		GTK_FILL, 0,
		2, 0);
	gtk_widget_show(frame);
	g_signal_connect(G_OBJECT(frame), "clicked",
		G_CALLBACK(gac_info), wnd);
	/**/
	
	gtk_container_add(GTK_CONTAINER(wnd->form), wnd->tab);
	gtk_widget_show(wnd->tab);
}

int main(int argc, char **argv, char **envp)
{
	struct asacheckwin wnd;

	//asaf_init();

	wnd.ic = iconv_open("UTF-8", "MS-ANSI");
	gtk_init(&argc, &argv);
    
    	aed_init();
	gac_dlg(&wnd);
	
	gtk_widget_show(wnd.form);
	gtk_main ();
    
	return 0;
}
#if 0

void ut8out(char *start, char *end)
{
	char outbuf[4096], *outp = outbuf;
	size_t insize = end - start, outsize = 4096;
	iconv_t ic = iconv_open("UTF-8", "UCS-4LE");
	iconv(ic, &start, &insize, &outp, &outsize);
	write(1, outbuf, 4096 - outsize);
}

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	void *data;

	struct ssa_error *enow;
	struct ssa_style *snow;
	struct ssa_line *lnow;
	struct ssa output;

	struct rusage bef, aft;
	long usec;

	setlocale(LC_ALL, NULL);
#ifndef HAVE_GETOPT
	unsigned print = 3;
	char *fname = argv[1];
#else
	char next_opt;
	unsigned print = 0;
	const struct option long_opts[] = {
		{ "verbose",	0, NULL, 'v' },
		{ "file",	0, NULL, 'f' },
		{ NULL,		0, NULL, 0 }
	};
	const char *short_opts = "vf:";
	char *fname = NULL;

	do {
		next_opt = getopt_long (argc, argv, short_opts,	long_opts, NULL);
		switch( next_opt ) {
		case 'f':
			fname = optarg;
			break;
		case 'v':
			print++;
			break;
		case -1:
			break;
		case '?':
		default:
			return 1;
		}
	} while (next_opt != -1);
#endif

	if (!fname) {
		fwprintf(stderr, L"%s -f <ssafile> [-v[v]]\n", argv[0]);
		return 1;
	}

	if ((fd = open(fname, O_RDONLY)) == -1) {
		fwprintf(stderr, L"error opening %s\n", fname);
		return 2;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		fwprintf(stderr, L"error mmapping %s\n", fname);
		return 2;
	};

	getrusage(RUSAGE_SELF, &bef);
	ssa_parse(&output, data, st.st_size);
	getrusage(RUSAGE_SELF, &aft);

	munmap(data, 0);
	close(fd);

	
	usec = (aft.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - bef.ru_utime.tv_usec);

	fwprintf(stdout, L"- parsing complete (%5.3f s)\n\n", (float)usec / 1000000.0f);

	enow = output.errlist;
	while (enow) {
		fwprintf(stderr, L"+- line %d, column %d: %s\n| %s\n+-",
			enow->lineno, enow->column + 1,
			ssaec[enow->errorcode],
			enow->textline);
		while (enow->column != 0) {
			fwprintf(stderr, L" ");
			enow->column--;
		}
		fwprintf(stderr, L"^----\n\n");
		enow = enow->next;
	}
	if (!print--)
		return 0;

	snow = output.style_first;
	while (snow) {
		fwprintf(stdout, L"%p %p style %20ls font %20ls, %7.3lfpt\n",
			snow, &snow->fontsize, snow->name.s, snow->fontname.s, snow->fontsize);
		snow = snow->next;
	}
	if (!print--)
		return 0;
	fwprintf(stdout, L"\n");
	lnow = output.line_first;
	while (lnow) {
/*		fprintf(stdout, "line %8.2lf - %8.2lf: %20ls %ls\n",
			lnow->start, lnow->end, lnow->style ? lnow->style->name.s : L"(?)", lnow->text.s); */
		if (lnow->type == SSAL_DIALOGUE) {
			struct ssa_node *nnow = lnow->node_first;
			while (nnow) {
				if (nnow->type == SSAN_TEXT)
					ut8out(nnow->v.text.s, nnow->v.text.e);
/*				fprintf(stdout, "\t%d %ls\n",
					nnow->type,
					nnow->type == SSAN_TEXT ? nnow->v.text.s :
					nnow->type == SSAN_NEWLINE ? L"\\n" :
					nnow->type == SSAN_NEWLINEH ? L"\\N" :
					nnow->type == SSAN_RESET ? (nnow->v.style ? nnow->v.style->name.s : L"default" ) :
					L"\\?"); */
				nnow = nnow->next;
			}
		}
		lnow = lnow->next;
	}
	return 0;
}
#endif
