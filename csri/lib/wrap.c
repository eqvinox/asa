/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007  David Lamparter
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

#include "csrilib.h"
#include "subhelp.h"

#ifdef HAVE_GCC_VISIBILITY
#pragma GCC visibility push(default)
#endif

csri_inst *csri_open_file(csri_rend *rend,
	const char *filename, struct csri_openflag *flags)
{
	struct csri_wrap_rend *wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	return csrilib_inst_initadd(wrend,
		wrend->open_file(rend, filename, flags));
}

csri_inst *csri_open_mem(csri_rend *rend,
	const void *data, size_t length, struct csri_openflag *flags)
{
	struct csri_wrap_rend *wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	return csrilib_inst_initadd(wrend,
		wrend->open_mem(rend, data, length, flags));
}

void *csri_query_ext(csri_rend *rend, csri_ext_id extname)
{
	struct csri_wrap_rend *wrend;
	void *rv = subhelp_query_ext_logging(extname);
	if (rv)
		return rv;

	wrend = csrilib_rend_lookup(rend);
	if (!wrend)
		return NULL;
	return wrend->query_ext(rend, extname);
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

