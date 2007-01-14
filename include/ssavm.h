/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

#ifndef _SSAVM_H
#define _SSAVM_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "ssa.h"
#include "ssarun.h"
#include "asaproc.h"

#ifndef WIN32
#define SSA_DEBUG 1
#endif

struct ssav_error;

union ssav_ctrval {
	struct {
		colour_t val;
		colour_t mask;
	} colour;
	double dval;
	FT_Pos pos;
};

/** controller type */
enum ssav_contrt {
	SSAVC_NONE = 0,
	SSAVC_MATRIX,
	SSAVC_COLOUR,
	SSAVC_FTPOS,

	SSAVC_COUNT
};

struct ssav_controller {
	enum ssav_contrt type;

	ptrdiff_t offset;
	union ssav_ctrval nextval;
	double t1, length_rez, accel;
};

struct ssav_params {
	unsigned nref;
	struct ssav_params *finalized;

	struct {
		char *name;
		long int weight, italic;
		double size;
	} f;
	struct asa_font *font;
	struct asa_fontinst *fsiz;

	struct {
		double	fscx, fscy,
			frx, fry, frz,
			fax, fay,
			fsp;
	} m;

	double border;
	double shadow;

	struct {
		colour_t colours[4];
	} r;

	unsigned nctr;
	struct ssav_controller ctrs[];
};

enum ssav_nodet {
	SSAVN_NONE = 0,
	SSAVN_TEXT,
	SSAVN_NEWLINE,
	SSAVN_NEWLINEH,

	SSAVN_COUNT
};

struct ssav_node {
	struct ssav_node *next;

	enum ssav_nodet type;
	struct ssav_params *params;
	struct assp_frameref *group;

	unsigned nchars;
	unsigned *indici;
	FT_OutlineGlyph *glyphs;

	FT_Matrix fx1;
};

enum ssav_unitt {
	SSAVU_TEXT = 0,
	SSAVU_NEWLINE
};

struct ssav_unit {
	struct ssav_unit *next;
	enum ssav_unitt type;
	unsigned idxstart;

	FT_Pos height;
	FT_Vector size, final;

	struct ssav_node *nl_node;
};

struct ssav_lineparams {
	FT_BBox clip;
	FT_Vector pos, org;
};

enum ssav_lineflags {
	SSAV_POS = 1 << 0,		/**< pos / move */
	SSAV_POSANIM = 1 << 1,		/**< set by \t(pos).
					 * to warn when POSANIM & ~POS */
	SSAV_ORG = 1 << 2,		/**< org */
	SSAV_ORGANIM = 1 << 3,		/**< similar to POSANIM */
};

struct ssav_line {
	struct ssav_node *node_first;
	struct ssav_unit *unit_first;

	double start, end;
	long int ass_layer;
	unsigned nchars;
#ifdef SSA_DEBUG
	struct ssa_line *input;
#endif

	double xalign, yalign;

	long int
		marginl,
		marginr,
		margint,
		marginb,
		wrap;

	struct ssav_lineparams base, active;
	enum ssav_lineflags flags;

	unsigned nctr;
	struct ssav_controller ctrs[];
};

/** a (time) fragment, with a list of lines */
struct ssa_frag {
	struct ssa_frag *next;
	double start;

	unsigned nrend;			/**< n. of lines to render */
	/** lines, in order.
	 * <nrend> lines that actually are visible,
	 *   in layer/appearance order; lowest one first. */
	struct ssav_line *lines[];
};

/** instance data for the VM */
struct ssa_vm {
/* vm core */
	struct ssa_frag *fragments;

	struct ssa_frag *cache;
	struct assa_layer *firstlayer;

	struct ssav_error *errlist, **errnext;

	enum ssar_redoflags redoflags;

/* positioner */
	double playresx, playresy;
	FT_Vector res;
	FT_Matrix scale;
	long scalebas;
};

extern struct ssa_frag *ssap_frag_init(struct ssa_vm *vm);
extern struct ssa_frag *ssap_frag_add(struct ssa_vm *v,
	struct ssa_frag *prev, struct ssav_line *l);

extern void ssav_create(struct ssa_vm *vm, struct ssa *ssa);

extern void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg);

#endif /* _SSAVM_H */

