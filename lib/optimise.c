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
#include "blitter.h"

struct asa_optfuncs asa_optfuncs = {NULL, NULL, NULL, NULL};

#ifdef ASA_OPT_I686
extern unsigned asar_cpuid();
#define CPUID_SSE2 (1 << 26)
extern void asar_commit_rgbx_bgrx_SSE2(struct assp_fgroup *g,
	cellline **lines, unsigned char colours[4][4]);
extern void asar_commit_xrgb_xbgr_SSE2(struct assp_fgroup *g,
	cellline **lines, unsigned char colours[4][4]);
#endif

void asa_opt_init()
{
#ifdef ASA_OPT_I686
	unsigned cpuid = asar_cpuid();

	if (cpuid & CPUID_SSE2) {
		asa_optfuncs.rgbx = asar_commit_rgbx_bgrx_SSE2;
		asa_optfuncs.xrgb = asar_commit_xrgb_xbgr_SSE2;
	}
#endif
}

