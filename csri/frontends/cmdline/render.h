/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007  David Lamparter
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

#ifndef _RENDER_H
#define _RENDER_H

extern struct csri_frame *png_load(const char *filename,
 	uint32_t *width, uint32_t *height, enum csri_pixfmt fmt);
extern void png_store(struct csri_frame *frame, const char *filename,
 	uint32_t width, uint32_t height);
extern struct csri_frame *frame_alloc(uint32_t width, uint32_t height,
	enum csri_pixfmt fmt);
extern void frame_free(struct csri_frame *frame);
extern void frame_copy(struct csri_frame *dst, struct csri_frame *src,
	uint32_t width, uint32_t height);

#endif
