/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005  David Lamparter
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

#define ASA_BUILD_RENDERER 1

#include "common.h"
#include "ssa.h"
#include "acconf.h"
#ifdef ASA_BUILD_RENDERER
#include "asafont.h"
#include "ssavm.h"
#endif

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <iconv.h>

void ut8out(char *start, char *end)
{
	char outbuf[4096], *outp = outbuf;
	size_t insize = end - start, outsize = 4096;
	iconv_t ic = iconv_open("UTF-8", "UCS-4LE");
	iconv(ic, &start, &insize, &outp, &outsize);
	write(1, outbuf, 4096 - outsize);
}

struct rendlist {
	struct rendlist *next;
	double time;
};

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	void *data;

	struct ssa_error *enow;
	struct ssa_style *snow;
	struct ssa_line *lnow;
	struct ssa output;
#ifdef ASA_BUILD_RENDERER
	struct ssa_vm vm;
#endif

	struct rendlist *rend = NULL, **rendnext = &rend;
	char *errpos;

	struct rusage bef, aft;
	long usec;

	setlocale(LC_ALL, NULL);
	int sparseout = 0;
#ifndef HAVE_GETOPT_LONG
	unsigned print = 3;
	char *fname = argv[1];
#else
	char next_opt;
	unsigned print = 0;
	const struct option long_opts[] = {
		{ "verbose",	0, NULL, 'v' },
		{ "file",	1, NULL, 'f' },
		{ "sparse",	0, NULL, 's' },
		{ "render",	1, NULL, 'r' },
		{ NULL,		0, NULL, 0 }
	};
	const char *short_opts = "vf:sr:";
	char *fname = NULL;

	do {
		next_opt = getopt_long (argc, argv, short_opts,	long_opts, NULL);
		switch( next_opt ) {
		case 'f':
			fname = optarg;
			break;
		case 'v':
			print++;
			break;
		case 's':
			sparseout = 1;
			break;
		case 'r':
			*rendnext = malloc(sizeof(struct rendlist));
			(*rendnext)->next = NULL;
			(*rendnext)->time = strtod(optarg, &errpos);
			if (!*optarg || *errpos) {
				fwprintf(stderr,
					L"-r requires an argument\n");
				return 1;
			}
			rendnext = &(*rendnext)->next;
			break;
		case -1:
			break;
		case '?':
		default:
			return 1;
		}
	} while (next_opt != -1);
#endif

	if (!fname) {
		fwprintf(stderr, L"%s -f <ssafile> [-v[v]]\n", argv[0]);
		return 1;
	}

	if ((fd = open(fname, O_RDONLY)) == -1) {
		fwprintf(stderr, L"error opening %s\n", fname);
		return 2;
	};
	fstat(fd, &st);
	if (!(data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		fwprintf(stderr, L"error mmapping %s\n", fname);
		return 2;
	};

#ifdef ASA_BUILD_RENDERER
	asaf_init();
#endif
	output.ignoreenc = 1;
	getrusage(RUSAGE_SELF, &bef);
	ssa_lex(&output, data, st.st_size);
	getrusage(RUSAGE_SELF, &aft);

	munmap(data, 0);
	close(fd);

	
	usec = (aft.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - bef.ru_utime.tv_usec);
	fwprintf(stdout, L"- parsing complete (%5.3f s)\n", (float)usec / 1000000.0f);

#ifdef ASA_BUILD_RENDERER
#define WIDTH 640
#define HEIGHT 480
#define LSIZE (WIDTH*HEIGHT)
#define CSIZE ((WIDTH*HEIGHT)>>2)
#define LSTRIDE WIDTH
#define CSTRIDE (WIDTH >> 1)
	getrusage(RUSAGE_SELF, &bef);
	ssav_create(&vm, &output);
	getrusage(RUSAGE_SELF, &aft);
	usec = (aft.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
		(aft.ru_utime.tv_usec - bef.ru_utime.tv_usec);
	fwprintf(stdout, L"- processing complete (%5.3f s)\n", (float)usec / 1000000.0f);
	if (rend) {
		struct assp_fgroup *fgroup = assp_fgroupnew(WIDTH, HEIGHT);
		unsigned char *y = malloc(LSIZE), *u = malloc(CSIZE), *v = malloc(CSIZE);
		struct asa_frame frame;
		frame.csp = ASACSP_YUV_PLANAR;
		frame.bmp.yuv_planar.y.d = y;
		frame.bmp.yuv_planar.y.stride = LSTRIDE;
		frame.bmp.yuv_planar.u.d = u;
		frame.bmp.yuv_planar.u.stride = CSTRIDE;
		frame.bmp.yuv_planar.v.d = v;
		frame.bmp.yuv_planar.v.stride = CSTRIDE;
		frame.bmp.yuv_planar.chroma_x_red = 1;
		frame.bmp.yuv_planar.chroma_y_red = 1;
		fgroup->active = &frame;

		assa_setup(&vm, WIDTH, HEIGHT);
		do {
			fwprintf(stdout, L"rendering: %lf\n", rend->time);
			fflush(stdout);
			getrusage(RUSAGE_SELF, &bef);
			ssar_run(&vm, rend->time, fgroup);
			getrusage(RUSAGE_SELF, &aft);
			usec = (aft.ru_utime.tv_sec - bef.ru_utime.tv_sec) * 1000000 +
				(aft.ru_utime.tv_usec - bef.ru_utime.tv_usec);
			fwprintf(stdout, L" - %5.3f s\n", (float)usec / 1000000.0f);
			rend = rend->next;
		} while (rend);
		free(y); free(u); free(v);
		return 0;
	}
#endif

	fwprintf(stdout, L"\n");
	enow = output.errlist;
	while (enow) {
		fwprintf(stderr, L"+- (severity %d) line %ld, column %ld: %s\n| %s\n+-",
			ssaec[enow->errorcode].sev, (long)enow->lineno, (long)enow->column + 1,
			ssaec[enow->errorcode].sh,
			enow->textline);
		if (!sparseout) {
			while (enow->column != 0) {
				fwprintf(stderr, L" ");
				enow->column--;
			}
			fwprintf(stderr, L"^----\n\n");
		}
		enow = enow->next;
	}
	if (!print--)
		return 0;

	snow = output.style_first;
	while (snow) {
		fwprintf(stdout, L"%p %p style %20ls font %20ls, %7.3lfpt w %d i %d\n",
			snow, &snow->fontsize, snow->name.s, snow->fontname.s, snow->fontsize, snow->fontweight, snow->italic);
		snow = snow->next;
	}
	if (!print--)
		return 0;
	fwprintf(stdout, L"\n");
	lnow = output.line_first;
	while (lnow) {
/*		fprintf(stdout, "line %8.2lf - %8.2lf: %20ls %ls\n",
			lnow->start, lnow->end, lnow->style ? lnow->style->name.s : L"(?)", lnow->text.s); */
		if (lnow->type == SSAL_DIALOGUE) {
			struct ssa_node *nnow = lnow->node_first;
			while (nnow) {
/*				if (nnow->type == SSAN_TEXT)
					ut8out(nnow->v.text.s, nnow->v.text.e); */
				fwprintf(stdout, L"\t%d %ls\n",
					nnow->type,
					nnow->type == SSAN_TEXT ? (wchar_t *)nnow->v.text.s :
					nnow->type == SSAN_NEWLINE ? L"\\n" :
					nnow->type == SSAN_NEWLINEH ? L"\\N" :
					nnow->type == SSAN_RESET ? (nnow->v.style ? (wchar_t*)(nnow->v.style->name.s) : L"default" ) :
					L"\\?");
				nnow = nnow->next;
			}
		}
		lnow = lnow->next;
	}
	return 0;
}

