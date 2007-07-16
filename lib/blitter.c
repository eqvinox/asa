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
#include "asaproc.h"
#include "blitter_internal.h"

/* - RGB preparation functions - */

#define ai a * -1 + 255
#define _rgb_prepfunc(name, v, w, x, y) \
	void asa_prep_ ## name (struct assp_frame *f) { \
		order(, f->colours[c].c, v, w, x, y) \
	}
#define rgb_prepfunx3(arg, v, w, x) \
	_rgb_prepfunc(arg ## v ## w ## x, ai, v, w, x) \
	_rgb_prepfunc(v ## w ## x ## arg, v, w, x, ai)
#define rgb_prepfunc3(arg, v, w, x) _rgb_prepfunc(v ## w ## x, v, w, x, ai)

rgb_orders3(rgb_prepfunc, 0, r, g, b)
rgb_orders3(rgb_prepfunx, a, r, g, b)
rgb_orders3(rgb_prepfunx, x, r, g, b)

/* - YUV preparation functions - */

#define yuv_prep \
	d.a = 255 - f->colours[c].c.a; \
	d.y = (unsigned char)(0.299 * f->colours[c].c.r \
		+ 0.587 * f->colours[c].c.g + 0.114 * f->colours[c].c.b); \
	d.u = (unsigned char)(0.713 * (f->colours[c].c.b - d.y) + 128); \
	d.v = (unsigned char)(0.564 * (f->colours[c].c.r - d.y) + 128); 

#define yuv_prepfunc(A, B, C, D) \
	void asa_prep_ ## A ## B ## C ## D (struct assp_frame *f) { \
		struct { unsigned char a, y, u, v; } d; \
		order(yuv_prep, d, A, B, C, D) \
	}

yuv_prepfunc(a, y, u, v)
yuv_prepfunc(y, u, v, a)
yuv_prepfunc(y, v, u, a)

/* - software blitter */

#ifdef _MSC
#define forceinline __forceinline
#elif defined(__GNUC__)
#if __GNUC__ >= 3
#define forceinline __extension__ __attribute__((always_inline))
#endif
#endif
#ifndef forceinline
#define forceinline
#endif

forceinline static inline void asa_blit_blend(struct assp_frame *f,
	struct csri_frame *dst,
	unsigned nbytes, unsigned first, unsigned last)
{
	unsigned c, lay, line;
	unsigned char *d;

	d = dst->planes[0];
	line = 0;
	while (line < f->group->h) {
		if (f->lines[line] != f->group->unused) {
			cell *now = f->lines[line]->data
					+ f->lines[line]->first,
				*lend = f->lines[line]->data
					+ f->lines[line]->last;
			unsigned char *dp = d
				+ nbytes * f->lines[line]->first;
			while (now < lend) {
				cell cl = *now;
				unsigned char accum = 0, raccum = 0;
				for (lay = 0; lay <= 3; lay++) {
					if (cl.e[lay] > 255 - accum) {
						cl.e[lay] = 255 - accum;
						accum = 255;
					} else
						accum += cl.e[lay];
					cl.e[lay] = ((cl.e[lay]	* (256
						- f->colours[lay].c.a)) >> 8);
					raccum += cl.e[lay];
				}
				for (c = first; c < last; c++) {
					unsigned short value = dp[c]
						* (256 - raccum);
					for (lay = 0; lay < 4; lay++)
						value += cl.e[lay]
							* f->blitter[
								4 * c + lay];
					dp[c] = value >> 8;
				}
				dp += nbytes;
				now++;
			}
		}
		d += dst->strides[0];
		line++;
	}
}

forceinline static inline void asa_blit_mix(struct assp_frame *f,
	struct csri_frame *dst,
	unsigned first, unsigned last, unsigned aidx)
{
	unsigned nbytes = 4;
	unsigned c, lay, line;
	unsigned char *d;

	d = dst->planes[0];
	line = 0;
	while (line < f->group->h) {
		if (f->lines[line] != f->group->unused) {
			cell *now = f->lines[line]->data
					+ f->lines[line]->first,
				*lend = f->lines[line]->data
					+ f->lines[line]->last;
			unsigned char *dp = d
				+ nbytes * f->lines[line]->first;
			while (now < lend) {
				cell cl = *now;

				unsigned char raccum = 0;
				unsigned short anew, naccum = 0;
				for (lay = 0; lay <= 3; lay++) {
					if (cl.e[lay] > 255 - raccum) {
						cl.e[lay] = 255 - raccum;
						raccum = 255;
					} else
						raccum += cl.e[lay];
					naccum += cl.e[lay]
						* f->blitter[4 * aidx + lay];
				}
				anew = (dp[aidx] * (65536 - naccum)) >> 16;

				for (c = first; c < last; c++) {
					unsigned int v2 = 0, value = dp[c]
						* (65536 - naccum) >> 16;
					for (lay = 0; lay < 4; lay++)
						v2 += cl.e[lay]
						    * f->blitter[4 * c + lay];
					v2 *= naccum;
					v2 >>= 16;
					if (anew + (naccum >> 8) > 0)
						dp[c] = (v2 + value) / (anew + (naccum >> 8));
				}
				dp[aidx] = anew + (naccum >> 8);
				dp += nbytes;
				now++;
			}
		}
		d += dst->strides[0];
		line++;
	}
}

void asa_blit_nnna_sw(struct assp_frame *f, struct csri_frame *dst)
{
	asa_blit_mix(f, dst, 0, 3, 3);
}

void asa_blit_annn_sw(struct assp_frame *f, struct csri_frame *dst)
{
	asa_blit_mix(f, dst, 1, 4, 0);
}

void asa_blit_nnnx_sw(struct assp_frame *f, struct csri_frame *dst)
{
	asa_blit_blend(f, dst, 4, 0, 3);
}

void asa_blit_xnnn_sw(struct assp_frame *f, struct csri_frame *dst)
{
	asa_blit_blend(f, dst, 4, 1, 4);
}

void asa_blit_nnn_sw(struct assp_frame *f, struct csri_frame *dst)
{
	asa_blit_blend(f, dst, 3, 0, 3);
}


