/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the disclaimer
 *    contained in the COPYING file in the documentation and/or
 *    other materials provided with the distribution.
 *
 ****************************************************************************/

#ifndef _ASAPROC_H
#define _ASAPROC_H

struct assp_frameref;

#include <csri/csri.h>
#include "colour.h"
#include "blitter.h"

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
	unsigned w, h;
	cellline *unused;
	struct assp_frameref *issued;

	enum csri_pixfmt pixfmt;
	struct asa_blitter blitter;

	unsigned resvsize, ptr;
	cellline *reservoir[1];
};

struct assp_frame {
	struct assp_fgroup *group;
	colour_t colours[4];
	unsigned char blitter[16];
	cellline *lines[1];
};

struct assp_param {
	struct assp_frame *f;
	int cx0, cx1, cy0, cy1;
	int xo, yo;
	int elem;
};

struct assp_frameref {
	struct assp_frameref *next;
	struct assp_frame *frame;
};

extern struct assp_fgroup *assp_fgroupnew(unsigned w, unsigned h,
	enum csri_pixfmt fmt);
extern void assp_fgroupfree(struct assp_fgroup *g);

extern void assp_framenew(struct assp_frameref *ng, struct assp_fgroup *g);
extern void assp_framefree(struct assp_frameref *ng);

#endif /* _ASAPROC_H */

