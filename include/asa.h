/* #include(LICENSE) */
#ifndef _ASA_H
#define _ASA_H

#include <stddef.h>

/** to be passed to asa_init.
 * 0xAAXXYYZZ
 *  - scheme only valid if AA = 00
 *  - XX = major, YY = minor, ZZ = release
 *  - YY uneven indicates unstable/devel,
 *    even indicates stable.
 * \ref asa_init
 */
#define ASA_VERSION	0x00100

/** colorspaces for asa_frame */
enum asa_csp {
	ASACSP_YUV_PLANAR = 0,
	ASACSP_YUV_PACKED,
	ASACSP_RGB,

	ASACSP_COUNT
};

enum asa_csp_yuv_packed {
	ASACSPY_YV12 = 0,

	ASACSPY_COUNT
};

/** single bitmap plane */
struct asa_plane {
	unsigned char *d;
	ptrdiff_t stride;
};

/** frame */
struct asa_frame {
	enum asa_csp csp;
	union {
		struct {
			struct asa_plane y, u, v;
			unsigned char chroma_x_red, chroma_y_red;
		} yuv_planar;
		struct {
			enum asa_csp_yuv_packed fmt;
			struct asa_plane d;
		} yuv_packed;
	} bmp;
};

enum asa_oflags {
	ASAF_HAYWIRE_UNICODEENC = 1 << 0,
};

/** opaque renderer instance */
typedef struct asa_inst asa_inst;

extern const char *asa_init(unsigned version);

extern asa_inst *asa_open(const char *filename, enum asa_oflags flags);
extern asa_inst *asa_open_mem(const char *mem, size_t size,
	enum asa_oflags flags);

extern void asa_setsize(asa_inst *i, unsigned width, unsigned height);
extern void asa_render(asa_inst *i, double ftime, struct asa_frame *frame);

extern void asa_close(asa_inst *i);

extern void asa_done();

#endif /* _ASA_H */

