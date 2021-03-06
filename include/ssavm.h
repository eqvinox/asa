/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004, 2005, 2006, 2007  David Lamparter
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

#ifndef _SSAVM_H
#define _SSAVM_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include "ssarun.h"
#include "asaproc.h"
#include "asafont.h"

#include <iconv.h>

#define SSA_DEBUG 1

struct ssa;
struct ssa_line;
struct ssa_node;
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
	unsigned blur, underline, strikeout;

	struct {
		colour_t colours[4];
		colour_t fade;		/**< warning: referred to as
					 * colours[4] */
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

enum ssav_karaoke_type {
	SSAVK_PLAIN = 1,
	SSAVK_BORD,
	SSAVK_SEQ
};

struct ssav_karaoke_unit {
	enum ssav_karaoke_type type;
	struct ssav_node *first, *end;
	double start, dur;
};

struct ssav_node {
	struct ssav_node *next;

	enum ssav_nodet type;
	struct ssav_params *params;
	struct assp_frameref *group;

	unsigned nchars;
	unsigned *indici;
	FT_OutlineGlyph *glyphs;

	struct ssav_karaoke_unit *kara;
	float matrix3d[16];
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

	SSAV_KARAOKE = 1 << 4
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

	FT_Pos x[2], y[2];

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

enum ssa_vm_streamflags {
	SSAV_STREAM_TEXT = (1 << 0),
	SSAV_STREAM_AUTODISCARD = (1 << 1)
};

/** instance data for the VM */
struct ssa_vm {
/* vm core */
	struct ssa_frag *fragments;

	struct ssa_frag *cache;
	struct assa_layer *firstlayer;

	struct ssav_error *errlist, **errnext;

	enum ssar_redoflags redoflags;

	enum ssa_vm_streamflags stream;
	iconv_t textconv;

/* positioner */
	double playresx, playresy;
	FT_Vector playres,	/**< playres */
		outsize;	/**< video res */
	FT_Matrix cscale,	/**< coordinate scale */
		fscale;		/**< font scale, xx=yy */
	long scalebas;
};

extern struct ssa_frag *ssap_frag_init(struct ssa_vm *vm);
extern struct ssa_frag *ssap_frag_add(struct ssa_vm *v,
	struct ssa_frag *prev, struct ssav_line *l);
extern void ssap_frag_expire(struct ssa_vm *vm, double ts);
extern void ssap_frag_free(struct ssa_vm *vm);

extern void ssav_free(struct ssav_line *l);

extern void ssav_create(struct ssa_vm *vm, struct ssa *ssa);
extern void ssav_packet(struct ssa_vm *vm, struct ssa *ssa,
	const void *data, size_t datasize, double start, double end);

#endif /* _SSAVM_H */

