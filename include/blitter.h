/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

#ifndef _BLITTER_H
#define _BLITTER_H

#include <csri/csri.h>

struct assp_fgroup;
struct assp_frame;

typedef void asa_blitf(struct assp_frame *f, struct csri_frame *dst);
typedef void asa_prepf(struct assp_frame *f);

struct asa_blitter {
	asa_blitf *blit;
	asa_prepf *prep;
};

extern void asa_opt_init();

extern int asa_blit_set(struct assp_fgroup *fg, enum csri_pixfmt pixfmt);
extern void asa_blit(struct assp_frame *f, struct csri_frame *dst);

#endif

