#ifndef _ASAPROC_H
#define _ASAPROC_H

#include "ssa.h"
#include "asa.h"
#include "asafont.h"

typedef union _cell {
	unsigned char e[4];
	unsigned int d;
} cell;

typedef struct _cellline {
	unsigned first;
	unsigned last;
	cell data[1];
} cellline;

struct assp_fgroup {
	struct asa_frame *active;

	unsigned w, h;
	cellline *unused;

	unsigned resvsize, ptr;
	cellline *reservoir[1];
};

struct assp_frame {
	struct assp_fgroup *group;
	colour_t colours[4];
	cellline *lines[1];
};

struct assp_param {
	struct assp_frame *f;
	int cx0, cx1, cy0, cy1;
	int xo, yo;
	int elem;
};

extern struct assp_fgroup *assp_fgroupnew(unsigned w, unsigned h);
extern void assp_fgroupfree(struct assp_fgroup *g);

extern struct assp_frame *assp_framenew(struct assp_fgroup *g);
extern void assp_spanfunc(int y, int count, FT_Span *spans, void *user);
extern void assp_framefree(struct assp_frame *f);
extern void assp_framekill(struct assp_frame *f);

extern void asar_commit(struct assp_frame *f);

#endif /* _ASAPROC_H */

