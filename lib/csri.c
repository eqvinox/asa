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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <errno.h>

#ifdef WIN32
#include <windows.h>

#define CSRIAPI __declspec(dllexport)
#else
#define CSRIAPI
#endif

#define CSRI_OWN_HANDLES
typedef struct csri_asa_rend csri_rend;
typedef struct csri_asa_inst csri_inst;

#include <csri/csri.h>

#define ASA_DEPRECATED
#include "asa.h"
#include "asaproc.h"
#include "ssavm.h"
#include "blitter.h"


struct csri_asa_inst {
	struct ssa_vm vm;
	struct assp_fgroup *framegroup;
};

static struct csri_asa_rend {
	int ref;
} csri_asa = { 0 };


csri_inst *csri_open_file(csri_rend *renderer,
	const char *filename, struct csri_openflag *flags)
{
	csri_inst *rv = NULL;
	void *data;
#ifndef WIN32
	int fd;
	struct stat st;

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return NULL;
	if (fstat(fd, &st)
		|| !(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE,
			fd, 0))) {
		close(fd);
		return NULL;
	}

	rv = csri_open_mem(renderer, data, st.st_size, flags);

	munmap(data, st.st_size);
	close(fd);
#else
	HANDLE file, mapping;
	DWORD size;
	int namesize;
	wchar_t *namebuf;

	namesize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	if (!namesize)
		return NULL;
	namesize++;
	namebuf = xmalloc(sizeof(wchar_t) * namesize);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, namebuf, namesize);

	file = CreateFileW(namebuf, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	xfree(namebuf);
	if (file == INVALID_HANDLE_VALUE)
		return NULL;
	size = GetFileSize(file, NULL);
	if (size == INVALID_FILE_SIZE || !size)
		goto out_closefile;
	mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!mapping)
		goto out_closefile;
	data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
	if (!data)
		goto out_closemap;

	rv = csri_open_mem(renderer, data, size, flags);

	UnmapViewOfFile(data);
out_closemap:
	CloseHandle(mapping);
out_closefile:
	CloseHandle(file);
#endif
	return rv;
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
	
	rv = calloc(sizeof(csri_inst), 1);
	ssav_create(&rv->vm, &lexout);
	ssa_free(&lexout);
	return rv;
}


void csri_close(csri_inst *inst)
{
	if (inst->framegroup)
		assp_fgroupfree(inst->framegroup);
	ssar_flush(&inst->vm);
	free(inst);

	if (!--csri_asa.ref)
		asaf_done();
}

int csri_request_fmt(csri_inst *inst, const struct csri_fmt *fmt)
{
	if (inst->framegroup)
		assp_fgroupfree(inst->framegroup);
	inst->framegroup = NULL;

	if (!csri_is_rgb(fmt->pixfmt))
		return -1;
	if (!fmt->width || !fmt->height)
		return -1;

	inst->framegroup = assp_fgroupnew(fmt->width, fmt->height);
	assa_setup(&inst->vm, fmt->width, fmt->height);
	return 0;
}

void csri_render(csri_inst *inst, struct csri_frame *frame, double time)
{
	/* well, let's just make it work for now. */
	struct asa_frame f;
	if (!inst->framegroup)
		return;

	f.csp = ASACSP_RGB;
	switch (frame->pixfmt) {
#define map2(x,y) case CSRI_F_ ## x: f.bmp.rgb.fmt = ASACSPR_ ## y; break;
#define map(x) map2(x, x)
	map(RGBA) map(BGRA) map(ARGB) map(ABGR) map(RGB) map(BGR)
	map2(RGB_, RGBx) map2(_RGB, xRGB) map2(BGR_, BGRx) map2(_BGR, xBGR)
	default: return;
	}
	f.bmp.rgb.d.stride = frame->strides[0];
	f.bmp.rgb.d.d = frame->planes[0];

	inst->framegroup->active = &f;
	ssar_run(&inst->vm, time, inst->framegroup);
}

void *csri_query_ext(csri_rend *rend, csri_ext_id extname)
{
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

