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
#include <string.h>

#include "blitter.h"
#include "asaproc.h"
#include "blitter_internal.h"

static struct asa_optfuncs {
	struct asa_blitter
		rgb_alpha(ccat, list, r, g, b)
		ayuv, yuva, yvua,
		yuy2,
		yv12a, yv12;
} asa_optfuncs = {
	rgb_alpha(ccat, initializer, r, g, b)
	{asa_blit_annn_sw, asa_prep_ayuv},
	{asa_blit_nnna_sw, asa_prep_yuva},
	{asa_blit_nnna_sw, asa_prep_yvua},
	{NULL, NULL},
	{NULL, NULL}, {NULL, NULL}
};

#undef ASA_OPT_I686
#ifdef ASA_OPT_I686
extern unsigned asar_cpuid();
#define CPUID_SSE2 (1 << 26)
extern void asar_blit_nnnx_SSE2(struct assp_frame *f,
	struct csri_frame *dst);
extern void asar_blit_xnnn_SSE2(struct assp_frame *f,
	struct csri_frame *dst);
#endif

void asa_opt_init()
{
#ifdef ASA_OPT_I686
	unsigned cpuid = asar_cpuid();

	if (cpuid & CPUID_SSE2) {
		asa_optfuncs.rgbx.blit = asar_blit_nnnx_SSE2;
		asa_optfuncs.bgrx.blit = asar_blit_nnnx_SSE2;
		asa_optfuncs.xrgb.blit = asar_blit_xnnn_SSE2;
		asa_optfuncs.xbgr.blit = asar_blit_xnnn_SSE2;
	}
#endif
}

int asa_blit_set(struct assp_fgroup *fg, enum csri_pixfmt pixfmt)
{
	struct asa_blitter *of = NULL;
	switch (pixfmt) {
	case CSRI_F_RGBA:  of = &asa_optfuncs.rgba;  break;
	case CSRI_F_BGRA:  of = &asa_optfuncs.bgra;  break;
	case CSRI_F_ARGB:  of = &asa_optfuncs.argb;  break;
	case CSRI_F_ABGR:  of = &asa_optfuncs.abgr;  break;
	case CSRI_F_RGB_:  of = &asa_optfuncs.rgbx;  break;
	case CSRI_F_BGR_:  of = &asa_optfuncs.bgrx;  break;
	case CSRI_F__RGB:  of = &asa_optfuncs.xrgb;  break;
	case CSRI_F__BGR:  of = &asa_optfuncs.xbgr;  break;
	case CSRI_F_RGB:   of = &asa_optfuncs.rgb;   break;
	case CSRI_F_BGR:   of = &asa_optfuncs.bgr;   break;
	case CSRI_F_AYUV:  of = &asa_optfuncs.ayuv;  break;
	case CSRI_F_YUVA:  of = &asa_optfuncs.yuva;  break;
	case CSRI_F_YVUA:  of = &asa_optfuncs.yvua;  break;
	case CSRI_F_YUY2:  of = &asa_optfuncs.yuy2;  break;
	case CSRI_F_YV12A: of = &asa_optfuncs.yv12a; break;
	case CSRI_F_YV12:  of = &asa_optfuncs.yv12;  break;
	default: return 1;
	}
	if (!of || !of->blit || !of->prep)
		return 1;
	fg->pixfmt = pixfmt;
	memcpy(&fg->blitter, of, sizeof(fg->blitter));
	return 0;
}

void asa_blit(struct assp_frame *f, struct csri_frame *dst)
{
	if (dst->pixfmt != f->group->pixfmt)
		return;
	f->group->blitter.prep(f);
	f->group->blitter.blit(f, dst);
}

