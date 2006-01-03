#ifndef _ASAERRDISP_H
#define _ASAERRDISP_H

struct asaerrdisp {
	GtkWidget *list, *src, *detl;

	GtkListStore *errstor;
	GtkTreeSelection *errsel;

	GtkTextBuffer *detlbuf, *srcbuf;
	GtkTextMark *errmark;

	struct ssa *ssa;
};

extern void aed_init();
extern GtkWidget *aed_create(struct asaerrdisp *disp);
extern unsigned aed_open(struct asaerrdisp *disp, struct ssa *ssa);

#endif /* _ASAERRDISP_H */
