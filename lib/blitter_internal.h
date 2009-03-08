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


#define decl(cct) \
	extern void asa_prep_ ## cct (struct assp_frame *f);
#define list(cct) cct,
#define initializer(cct) \
	{ asa_blit_ ## cct ## _sw, asa_prep_ ## cct },

#define ccat(arg, w, x, y, z) arg(w ## x ## y ## z)
#define ccat3(arg, x, y, z) arg(x ## y ## z)

#define rgb_orders(fun, arg, r, g, b, a) \
	fun(arg, r, g, b, a) \
	fun(arg, b, g, r, a) \
	fun(arg, a, r, g, b) \
	fun(arg, a, b, g, r)
#define rgb_orders3(fun, arg, r, g, b) \
	fun ## 3(arg, r, g, b) \
	fun ## 3(arg, b, g, r) \

#define rgb_alpha(fun, arg, r, g, b) \
	rgb_orders(fun, arg, r, g, b, a) \
	rgb_orders(fun, arg, r, g, b, x) \
	rgb_orders3(fun, arg, r, g, b)

rgb_alpha(ccat, decl, r, g, b)

#define order(prep, prefix, v, w, x, y) \
	unsigned c; \
	for (c = 0; c < 4; c++) { \
		prep \
		f->blitter[c]		= prefix.v; \
		f->blitter[c + 4]	= prefix.w; \
		f->blitter[c + 8]	= prefix.x; \
		f->blitter[c + 12]	= prefix.y; \
	}

extern void asa_prep_ayuv(struct assp_frame *f);
extern void asa_prep_yuva(struct assp_frame *f);
extern void asa_prep_yvua(struct assp_frame *f);

extern void asa_blit_nnna_sw(struct assp_frame *f, struct csri_frame *dst);
extern void asa_blit_annn_sw(struct assp_frame *f, struct csri_frame *dst);
extern void asa_blit_nnnx_sw(struct assp_frame *f, struct csri_frame *dst);
extern void asa_blit_xnnn_sw(struct assp_frame *f, struct csri_frame *dst);
extern void asa_blit_nnn_sw(struct assp_frame *f, struct csri_frame *dst);

#define asa_blit_rgbx_sw asa_blit_nnnx_sw
#define asa_blit_bgrx_sw asa_blit_nnnx_sw
#define asa_blit_xrgb_sw asa_blit_xnnn_sw
#define asa_blit_xbgr_sw asa_blit_xnnn_sw
#define asa_blit_rgba_sw asa_blit_nnna_sw
#define asa_blit_bgra_sw asa_blit_nnna_sw
#define asa_blit_argb_sw asa_blit_annn_sw
#define asa_blit_abgr_sw asa_blit_annn_sw
#define asa_blit_rgb_sw  asa_blit_nnn_sw
#define asa_blit_bgr_sw  asa_blit_nnn_sw
#define asa_blit_ayuv_sw asa_blit_annn_sw
#define asa_blit_yuva_sw asa_blit_nnna_sw
#define asa_blit_yvua_sw asa_blit_nnna_sw

