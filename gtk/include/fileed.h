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
