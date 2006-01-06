#ifndef _SSAVM_H
#define _SSAVM_H

#include <stdint.h>
#include <malloc.h>
#include "ssa.h"
#include "ssarun.h"
#include "asaproc.h"

#define SSA_DEBUG 1

union ssav_ctrval {
	colour_t colour;
	double dval;
};

/** controller type */
enum ssav_contrt {
	SSAVC_NONE = 0,
	SSAVC_MATRIX,
	SSAVC_COLOUR,

	SSAVC_COUNT
};

struct ssav_controller {
	enum ssav_contrt type;

	ptrdiff_t offset;
	union ssav_ctrval start;

	struct {
		double t1, t2, accel;
		union ssav_ctrval nextval;
	} ;
};

struct ssav_params {
	unsigned nref;

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
//	ssav_mat *mat;

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
	struct ssar_nodegroup *group;

	unsigned nchars;
	unsigned *indici;
	FT_OutlineGlyph *glyphs;
};

struct ssav_unit {
	struct ssav_unit *next;
	unsigned idxstart;

	FT_Pos width, trailspace;
	FT_Vector final;
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

	long int
		align,
		marginl,
		marginr,
		marginv,
		wrap;

	struct ssa_node *pos; /**< XXX TEMP HACK XXX */
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
	struct assa_env ae;

/* positioner */
	double playresx, playresy;
	FT_Vector resx, resy;
};

extern struct ssa_frag *ssap_frag_init(struct ssa_vm *vm);
extern struct ssa_frag *ssap_frag_add(struct ssa_vm *v,
	struct ssa_frag *prev, struct ssav_line *l);

extern void ssav_create(struct ssa_vm *vm, struct ssa *ssa);

extern void ssar_run(struct ssa_vm *vm, double ftime, struct assp_fgroup *fg);

#endif /* _SSAVM_H */

