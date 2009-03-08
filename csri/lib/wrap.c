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

#include "csrilib.h"
#include "subhelp.h"

#include <string.h>

csri_inst *csri_open_file(csri_rend *rend,
	const char *filename, struct csri_openflag *flags)
{
	struct csri_wrap_rend *wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	return csrilib_inst_initadd(wrend,
		wrend->open_file(rend, filename, flags));
}

#define instance_wrapper(wrapname, funcname) \
csri_inst *wrapname(csri_rend *rend, \
	const void *data, size_t length, struct csri_openflag *flags) \
{ \
	struct csri_wrap_rend *wrend = csrilib_rend_lookup(rend); \
	if (!wrend) \
		return NULL; \
	return csrilib_inst_initadd(wrend, \
		wrend->funcname(rend, data, length, flags)); \
}

instance_wrapper(csri_open_mem, open_mem)
static instance_wrapper(wrap_init_stream_ass, init_stream_ass)
static instance_wrapper(wrap_init_stream_text, init_stream_text)

void *csri_query_ext(csri_rend *rend, csri_ext_id extname)
{
	struct csri_wrap_rend *wrend;
	void *rv;

	if (!rend && (rv = subhelp_query_ext_logging(extname)))
		return rv;

	wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	rv = wrend->query_ext(rend, extname);
	if (rv && !strcmp(extname, CSRI_EXT_STREAM_ASS)) {
		struct csri_stream_ext *e = (struct csri_stream_ext *)rv;
		memcpy(&wrend->stream_ass, e, sizeof(*e));
		wrend->init_stream_ass = e->init_stream;
		wrend->stream_ass.init_stream = wrap_init_stream_ass;
		return &wrend->stream_ass;
	}
	if (rv && !strcmp(extname, CSRI_EXT_STREAM_TEXT)) {
		struct csri_stream_ext *e = (struct csri_stream_ext *)rv;
		memcpy(&wrend->stream_text, e, sizeof(*e));
		wrend->init_stream_text = e->init_stream;
		wrend->stream_text.init_stream = wrap_init_stream_text;
		return &wrend->stream_text;
	}
	return rv;
}

struct csri_info *csri_renderer_info(csri_rend *rend)
{
	struct csri_wrap_rend *wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	return wrend->info;
}

void csri_close(csri_inst *inst)
{
	struct csri_wrap_inst *winst = csrilib_inst_lookup(inst);
	if (!winst)
		return;
	winst->close(inst);
	csrilib_inst_remove(winst);
}

int csri_request_fmt(csri_inst *inst, const struct csri_fmt *fmt)
{
	struct csri_wrap_inst *winst = csrilib_inst_lookup(inst);
	if (!winst)
		return 0;
	return winst->request_fmt(inst, fmt);
}

void csri_render(csri_inst *inst, struct csri_frame *frame,
	double time)
{
	struct csri_wrap_inst *winst = csrilib_inst_lookup(inst);
	if (!winst)
		return;
	winst->render(inst, frame, time);
}

const char *csri_library()
{
	return "DEV";
}

