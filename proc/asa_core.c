/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006  David Lamparter
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

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "asa.h"
#include "asaproc.h"
#include "ssavm.h"

struct asa_inst {
	struct assp_fgroup *framegroup;

	struct ssa_vm vm;
};

static const char *asa_version_string = 
	"asa 0.1.00, " __DATE__ ", [*]";

static int nref = 0;

const char *asa_init(unsigned version)
{
	if (version != ASA_VERSION)
		return NULL;
	if (!nref++)
		asaf_init();
	return asa_version_string;
}

asa_inst *asa_open(const char *filename, enum asa_oflags flags)
{
	int fd;
	struct stat st;
	void *data;
	int save_errno;
	asa_inst *rv;

	if ( -1 == (fd = open(filename, O_RDONLY))
		|| fstat(fd, &st)
		|| !(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE,
			fd, 0)))
		return NULL;

	rv = asa_open_mem(data, st.st_size, flags);
	save_errno = errno;
	munmap(data, st.st_size);
	close(fd);
	errno = save_errno;
	return rv;
}

asa_inst *asa_open_mem(const char *mem, size_t size,
	enum asa_oflags flags)
{
	struct ssa lexout;
	int lexrv;
	asa_inst *rv;

	memset(&lexout, 0, sizeof(lexout));
	lexout.ignoreenc = !(flags & ASAF_HAYWIRE_UNICODEENC);
	if ((lexrv = ssa_lex(&lexout, mem, size))) {
		errno = (lexrv == 1) ? EINVAL : EILSEQ;
		return NULL;
	}
	
	rv = calloc(sizeof(asa_inst), 1);

	ssav_create(&rv->vm, &lexout);
#if 0
	ssav_create error handling here...
	ssa_free(&lexout); <- not written yet
#endif
	return rv;
}

void asa_setsize(asa_inst *i, unsigned width, unsigned height)
{
	if (i->framegroup)
		assp_fgroupfree(i->framegroup);
	if (!width || !height) {
		i->framegroup = NULL;
		return;
	}
	i->framegroup = assp_fgroupnew(width, height);
}

void asa_render(asa_inst *i, double ftime, struct asa_frame *frame)
{
	if (!i->framegroup)
		return;
	i->framegroup->active = frame;
	ssar_run(&i->vm, ftime, i->framegroup);
}

static void _asar_commit(struct assp_frame *f, int lay);

extern void asar_commit_y420_x86_64(struct assp_fgroup *g, cellline **lines, cell colours[3]);

#define OPT (__x86_64__ && __SSE2__)

void asar_commit(struct assp_frame *f)
{
#if OPT
	static char offset = 0;
	cell yuvc[2];
	for (int i = 0; i < 4; i++) {
		colour_t col = f->colours[i];
		yuvc[0].e[i] = col.c.a + offset;
		yuvc[1].e[i] = 0.299 * col.c.r + 0.587 * col.c.g + 0.114 * col.c.b;
	}
	offset++;
	asar_commit_y420_x86_64(f->group, f->lines, yuvc);
#endif

	_asar_commit(f, 2);
	_asar_commit(f, 0);
}

static void _asar_commit(struct assp_frame *f, int lay)
{
	struct asa_frame *dst = f->group->active;
	unsigned line = 0;
	unsigned char *d, *origin,
		y, u, v;
	cell *now, *lend;
	unsigned short va, vb;
	colour_t col = f->colours[lay];
	
	y = 0.299 * col.c.r + 0.587 * col.c.g + 0.114 * col.c.b;
	u = 0.713 * (col.c.r - y) + 128;
	v = 0.564 * (col.c.b - y) + 128;

	d = dst->bmp.yuv_planar.y.d;
#if !OPT
	while (line < f->group->h) {
		now = f->lines[line]->data;
		lend = now + f->group->w;
		while (now < lend) {
			va = *d * (256 - now->e[lay]);
			vb = y * (now->e[lay] + 1);
			*d++ = (va + vb) >> 8;
			now++;
		}
		d -= f->group->w;
		d += dst->bmp.yuv_planar.y.stride;
		line++;
	}
#endif

	unsigned numx = 1 << dst->bmp.yuv_planar.chroma_x_red;
	unsigned numy = 1 << dst->bmp.yuv_planar.chroma_y_red;

	line = 0;
	origin = dst->bmp.yuv_planar.u.d;
	while (line < f->group->h) {
		cell *now[4];
		unsigned nlines = 0, c, b, a;

		for (c = 0; c < numy; c++)
			if (f->lines[line+c] != f->group->unused)
				now[nlines++] = f->lines[line+c]->data;
		
		d = origin;
		for (c = 0; c < (f->group->w >> dst->bmp.yuv_planar.chroma_x_red); c++) {
			unsigned v0 = 0;
			for (b = 0; b < numx; b++)
				for (a = 0; a < nlines; a++)
					v0 += (now[a]++)->e[lay];
			v0 >>= dst->bmp.yuv_planar.chroma_x_red + dst->bmp.yuv_planar.chroma_y_red;
			va = *d * (256 - v0);
			vb = u * (v0 + 1);
			*d++ = (va + vb) >> 8;
		}
		origin += dst->bmp.yuv_planar.u.stride;
		line += numy;
	}

	line = 0;
	origin = dst->bmp.yuv_planar.v.d;
	while (line < f->group->h) {
		cell *now[4];
		unsigned nlines = 0, c, b, a;

		for (c = 0; c < numy; c++)
			if (f->lines[line+c] != f->group->unused)
				now[nlines++] = f->lines[line+c]->data;
		
		d = origin;
		for (c = 0; c < (f->group->w >> dst->bmp.yuv_planar.chroma_x_red); c++) {
			unsigned v0 = 0;
			for (b = 0; b < numx; b++)
				for (a = 0; a < nlines; a++)
					v0 += (now[a]++)->e[lay];
			v0 >>= dst->bmp.yuv_planar.chroma_x_red + dst->bmp.yuv_planar.chroma_y_red;
			va = *d * (256 - v0);
			vb = v * (v0 + 1);
			*d++ = (va + vb) >> 8;
		}
		origin += dst->bmp.yuv_planar.v.stride;
		line += numy;
	}


//	assp_framefree(f); XXX XXX XXX put this somewhere else, memory leaking!
}

void asa_close(asa_inst *i)
{
	/* XXX */
	free(i);
}

void asa_done()
{
}


