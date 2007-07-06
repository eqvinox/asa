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

#include "common.h"

#define CSRIAPI f_export

#define CSRI_OWN_HANDLES
typedef struct csri_asa_rend csri_rend;
typedef struct csri_asa_inst csri_inst;

#include <csri/csri.h>
#include <csri/stream.h>
#include <subhelp.h>

#include "asaproc.h"
#include "ssa.h"
#include "ssavm.h"
#include "blitter.h"


struct csri_asa_inst {
	struct ssa_vm vm;
	struct assp_fgroup *framegroup;

	unsigned streaming;
	struct ssa streamlex;
};

static struct csri_asa_rend {
	int ref;
} csri_asa = { 0 };


csri_inst *csri_open_file(csri_rend *renderer,
	const char *filename, struct csri_openflag *flags)
{
	return subhelp_open_file(renderer, csri_open_mem, filename, flags);
}

csri_inst *csri_open_mem(csri_rend *renderer,
	const void *data, size_t length, struct csri_openflag *flags)
{
	struct ssa lexout;
	int lexrv;
	csri_inst *rv;

	if (renderer != &csri_asa)
		return NULL;
	if (!csri_asa.ref++) {
		asa_opt_init();
		asaf_init();
	}

	memset(&lexout, 0, sizeof(lexout));
	if ((lexrv = ssa_lex(&lexout, data, length)))
		return NULL;
	
	rv = xnewz(csri_inst);
	ssav_create(&rv->vm, &lexout);
	ssa_free(&lexout);
	return rv;
}

csri_inst *csri_stream_ass_init_stream(csri_rend *renderer,
	const void *header, size_t headerlen, struct csri_openflag *flags)
{
	csri_inst *rv;

	if (renderer != &csri_asa)
		return NULL;
	if (!csri_asa.ref++) {
		asa_opt_init();
		asaf_init();
	}

	rv = xnewz(csri_inst);
	rv->streaming = 1;
	if (ssa_lex(&rv->streamlex, header, headerlen)) {
		xfree(rv);
		return NULL;
	}
	ssav_create(&rv->vm, &rv->streamlex);
	rv->vm.stream = SSAV_STREAM_AUTODISCARD;
	return rv;
}

static void csri_stream_ass_push_packet(csri_inst *inst,
	const void *packet, size_t packetlen, double start, double end)
{
	ssav_packet(&inst->vm, &inst->streamlex,
		packet, packetlen, start, end);
}

void csri_close(csri_inst *inst)
{
	if (inst->framegroup)
		assp_fgroupfree(inst->framegroup);
	ssar_flush(&inst->vm);
	if (inst->streaming)
		ssa_free(&inst->streamlex);
	free(inst);

#if 0
	/* temporarily disabled since it seems to crash on win32 */
	if (!--csri_asa.ref)
		asaf_done();
#endif
}

int csri_request_fmt(csri_inst *inst, const struct csri_fmt *fmt)
{
	if (inst->framegroup)
		assp_fgroupfree(inst->framegroup);
	inst->framegroup = NULL;

	if (!fmt->width || !fmt->height)
		return -1;

	inst->framegroup = assp_fgroupnew(fmt->width, fmt->height,
		fmt->pixfmt);
	if (!inst->framegroup)
		return -1;
	assa_setup(&inst->vm, fmt->width, fmt->height);
	return 0;
}

void csri_render(csri_inst *inst, struct csri_frame *frame, double time)
{
	if (!inst->framegroup)
		return;

	ssar_run(&inst->vm, time, inst->framegroup, frame);
}

static struct csri_stream_ext asa_stream_ext = {
	csri_stream_ass_init_stream,
	csri_stream_ass_push_packet,
	NULL
};

void *csri_query_ext(csri_rend *rend, csri_ext_id extname)
{
	void *rv;
	if ((rv = subhelp_query_ext_logging(extname)))
		return rv;
	if (rend != &csri_asa)
		return NULL;
	if (!strcmp(extname, CSRI_EXT_STREAM_ASS))
		return &asa_stream_ext;
	return NULL;
}

static struct csri_info csri_asa_info = {
	"asa",
	"  #SNAP#",
	"asa (0.3 series development version)",
	"equinox",
	"Copyright (c) 2004-2007 by David Lamparter"
};

struct csri_info *csri_renderer_info(csri_rend *rend)
{
	return &csri_asa_info;
}

csri_rend *csri_renderer_byname(const char *name,
	const char *specific)
{
	if (strcmp(name, csri_asa_info.name))
		return NULL;
	if (specific && strcmp(specific, csri_asa_info.specific))
		return NULL;
	return &csri_asa;
}

csri_rend *csri_renderer_default()
{
	return &csri_asa;
}

csri_rend *csri_renderer_next(csri_rend *prev)
{
	return NULL;
}

