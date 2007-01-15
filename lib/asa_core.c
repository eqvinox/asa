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
#endif

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
static unsigned cpuid;

#define CPUID_SSE2 (1 << 26)
#ifdef ASA_OPT_I686
extern unsigned asar_cpuid();
#else
#define asar_cpuid() 0
#endif

f_export const char *asa_init(unsigned version)
{
	if (version != ASA_VERSION)
		return NULL;
	cpuid = asar_cpuid();
	if (!nref++)
		asaf_init();
	return asa_version_string;
}

#ifndef WIN32
f_export asa_inst *asa_open(const char *filename, enum asa_oflags flags)
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
#else
f_export asa_inst *asa_open(const char *filename, enum asa_oflags flags)
{
	HANDLE file, mapping;
	DWORD size;
	void *data;
	asa_inst *rv = NULL;
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
	
	rv = asa_open_mem(data, size, flags);

	UnmapViewOfFile(data);
out_closemap:
	CloseHandle(mapping);
out_closefile:
	CloseHandle(file);
	return rv;
}
#endif

f_export asa_inst *asa_open_mem(const char *mem, size_t size,
	enum asa_oflags flags)
{
	struct ssa lexout;
	int lexrv;
	asa_inst *rv;

	memset(&lexout, 0, sizeof(lexout));
	if ((lexrv = ssa_lex(&lexout, mem, size))) {
		errno = (lexrv == 1) ? EINVAL : EILSEQ;
		return NULL;
	}
	
	rv = calloc(sizeof(asa_inst), 1);

	ssav_create(&rv->vm, &lexout);
	/* XXX: ssav_create error handling here... */
	ssa_free(&lexout);
	return rv;
}

f_export void asa_setsize(asa_inst *i, unsigned width, unsigned height)
{
	if (i->framegroup)
		assp_fgroupfree(i->framegroup);
	if (!width || !height) {
		i->framegroup = NULL;
		return;
	}
	i->framegroup = assp_fgroupnew(width, height);
	assa_setup(&i->vm, width, height);
}

f_export void asa_render(asa_inst *i, double ftime, struct asa_frame *frame)
{
	if (!i->framegroup)
		return;
	i->framegroup->active = frame;
	ssar_run(&i->vm, ftime, i->framegroup);
}

static void asar_commit_yuvp(struct assp_frame *f);
static void asar_commit_rgb(struct assp_frame *f);
static void _asar_commit(struct assp_frame *f, int lay);

#ifdef ASA_OPT_AMD64
extern void asar_commit_y420_x86_64(struct assp_fgroup *g, cellline **lines, cell colours[3]);
#endif
#ifdef ASA_OPT_I686
extern void asar_commit_rgbx_bgrx_SSE2(struct assp_fgroup *g,
	cellline **lines, unsigned char colours[4][4]);
#else
#define asar_commit_rgbx_bgrx_SSE2 NULL
#endif

void asar_commit(struct assp_frame *f)
{
	switch (f->group->active->csp) {
	case ASACSP_YUV_PLANAR:
		asar_commit_yuvp(f);
		break;
	case ASACSP_RGB:
		asar_commit_rgb(f);
		break;
	case ASACSP_YUV_PACKED:
	case ASACSP_COUNT:
		;
	}
}

static void asar_commit_rgb(struct assp_frame *f)
{
	unsigned char cv[4][4];
	struct asa_frame *dst = f->group->active;
	int c, lay;
	unsigned line;
	int blend = 0, nbytes = 4;
	unsigned char *d;
	void (*asmfunc)(struct assp_fgroup *g, cellline **lines,
		unsigned char colours[4][4]) = NULL;

#define order(v,w,x,y) \
		for (c = 0; c < 4; c++) \
			cv[0][c] = f->colours[c].c.v, \
			cv[1][c] = f->colours[c].c.w, \
			cv[2][c] = f->colours[c].c.x, \
			cv[3][c] = f->colours[c].c.y; \
		break;
#define orderx(v,w,x,y) blend = 1; order(v,w,x,y)
#define order3(v,w,x) blend = 1; nbytes = 3; order(v,w,x,a & 0)
#define x a
#define ai a * -1 + 255
	switch (dst->bmp.rgb.fmt) {
	case ASACSPR_RGBA: order(r,g,b,ai)
	case ASACSPR_BGRA: order(b,g,r,ai)
	case ASACSPR_ARGB: order(ai,r,g,b)
	case ASACSPR_ABGR: order(ai,b,g,r)
	case ASACSPR_RGBx: asmfunc = asar_commit_rgbx_bgrx_SSE2; orderx(r,g,b,a)
	case ASACSPR_BGRx: asmfunc = asar_commit_rgbx_bgrx_SSE2; orderx(b,g,r,a)
	case ASACSPR_xRGB: orderx(x,r,g,b)
	case ASACSPR_xBGR: orderx(x,b,g,r)
	case ASACSPR_RGB: order3(r,g,b)
	case ASACSPR_BGR: order3(b,g,r)
	case ASACSPR_COUNT: ;
	}
#undef x

	if (asmfunc && (cpuid & CPUID_SSE2)) {
		asmfunc(f->group, f->lines, cv);
		return;
	}
	d = dst->bmp.rgb.d.d;
	line = 0;
#define common1 \
	while (line < f->group->h) { \
		if (f->lines[line] != f->group->unused) { \
			cell *now = f->lines[line]->data + f->lines[line]->first, \
				*lend = f->lines[line]->data + f->lines[line]->last; \
			unsigned char *dp = d + nbytes * f->lines[line]->first; \
			while (now < lend) { \
				cell cl = *now;
#define common2 \
				for (c = 0; c < nbytes; c++) { \
					unsigned short value = dp[c] * (256 - raccum); \
					for (lay = 0; lay < 4; lay++) \
						value += cl.e[lay] * cv[c][lay]; \
					dp[c] = value >> 8; \
				} \
				dp += nbytes; \
				now++; \
			} \
		} \
		d += dst->bmp.rgb.d.stride; \
		line++; \
	}

	if (blend) {
		common1
		unsigned char accum = 0, raccum = 0;
		for (lay = 0; lay <= 3; lay++) {
			if (cl.e[lay] > 255 - accum) {
				cl.e[lay] = 255 - accum;
				accum = 255;
			} else
				accum += cl.e[lay];
			cl.e[lay] = ((cl.e[lay] * (256 - f->colours[lay].c.a)) >> 8);
			raccum += cl.e[lay];
		}
		common2
	} else {
		common1
		unsigned char raccum = 0;
		for (lay = 0; lay <= 3; lay++) {
			if (cl.e[lay] > 255 - raccum) {
				cl.e[lay] = 255 - raccum;
				raccum = 255;
			} else
				raccum += cl.e[lay];
		}
		common2
	}
#undef common1
#undef common2
}

static void asar_commit_yuvp(struct assp_frame *f)
{
#ifdef ASA_OPT_AMD64
	cell yuvc[2];
	for (int i = 0; i < 4; i++) {
		colour_t col = f->colours[i];
		yuvc[0].e[i] = col.c.a;
		yuvc[1].e[i] = 0.299 * col.c.r + 0.587 * col.c.g + 0.114 * col.c.b;
	}
	asar_commit_y420_x86_64(f->group, f->lines, yuvc);
#endif

	_asar_commit(f, 3);
	_asar_commit(f, 2);
	_asar_commit(f, 1);
	_asar_commit(f, 0);
}

static void _asar_commit(struct assp_frame *f, int lay)
{
	struct asa_frame *dst = f->group->active;
	unsigned line = 0,
		numx, numy;
	unsigned char *d, *origin,
		y, u, v;
#ifdef ASA_OPT_NONE
	cell *now, *lend;
#endif
	unsigned short va, vb, a;
	colour_t col = f->colours[lay];

	a = col.c.a;
	y = (unsigned char)(0.299 * col.c.r + 0.587 * col.c.g + 0.114 * col.c.b);
	u = (unsigned char)(0.713 * (col.c.b - y) + 128);
	v = (unsigned char)(0.564 * (col.c.r - y) + 128);

	d = dst->bmp.yuv_planar.y.d;
#ifdef ASA_OPT_NONE
	while (line < f->group->h) {
		now = f->lines[line]->data;
		lend = now + f->group->w;
		while (now < lend) {
			unsigned char tmp = (now->e[lay] * (256 - a)) >> 8;
			va = *d * (256 - tmp);
			vb = y * (tmp + 1);
			*d++ = (va + vb) >> 8;
			now++;
		}
		d -= f->group->w;
		d += dst->bmp.yuv_planar.y.stride;
		line++;
	}
#endif

	numx = 1 << dst->bmp.yuv_planar.chroma_x_red;
	numy = 1 << dst->bmp.yuv_planar.chroma_y_red;

	line = 0;
	origin = dst->bmp.yuv_planar.u.d;
	while (line < f->group->h) {
		cell *now[4];
		unsigned nlines = 0, c, c1, c2;

		for (c = 0; c < numy; c++)
			if (f->lines[line+c] != f->group->unused)
				now[nlines++] = f->lines[line+c]->data;
		
		d = origin;
		for (c = 0; c < (f->group->w >> dst->bmp.yuv_planar.chroma_x_red); c++) {
			unsigned v0 = 0;
			for (c1 = 0; c1 < numx; c1++)
				for (c2 = 0; c2 < nlines; c2++)
					v0 += (now[c2]++)->e[lay];
			v0 >>= dst->bmp.yuv_planar.chroma_x_red + dst->bmp.yuv_planar.chroma_y_red;
			v0 = (v0 * (256 - a)) >> 8;
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
		unsigned nlines = 0, c, c1, c2;

		for (c = 0; c < numy; c++)
			if (f->lines[line+c] != f->group->unused)
				now[nlines++] = f->lines[line+c]->data;
		
		d = origin;
		for (c = 0; c < (f->group->w >> dst->bmp.yuv_planar.chroma_x_red); c++) {
			unsigned v0 = 0;
			for (c1 = 0; c1 < numx; c1++)
				for (c2 = 0; c2 < nlines; c2++)
					v0 += (now[c2]++)->e[lay];
			v0 >>= dst->bmp.yuv_planar.chroma_x_red + dst->bmp.yuv_planar.chroma_y_red;
			v0 = (v0 * (256 - a)) >> 8;
			va = *d * (256 - v0);
			vb = v * (v0 + 1);
			*d++ = (va + vb) >> 8;
		}
		origin += dst->bmp.yuv_planar.v.stride;
		line += numy;
	}


//	assp_framefree(f); XXX XXX XXX put this somewhere else, memory leaking!
}

f_export void asa_close(asa_inst *i)
{
	if (i->framegroup)
		assp_fgroupfree(i->framegroup);
	ssar_flush(&i->vm);
	/* XXX */
	free(i);
}

f_export void asa_done()
{
	asaf_done();
}


