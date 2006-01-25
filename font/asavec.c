/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2004  David Lamparter
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

#include "asavec.h"
#include <math.h>

asa_matrix *asam_translate(asa_matrix *r, double xorg, double yorg)
{
	r->v.xx = r->v.yy = 1.0;
	r->v.xy = r->v.yx = 0.0;
	r->v.x1 = -xorg;
	r->v.y1 = -yorg;
	return r;
}

asa_matrix *asam_rotatex(asa_matrix *r, double xorg, double yorg)
{
	r->v.xx = r->v.yy = 1.0;
	r->v.xy = r->v.yx = 0.0;
	r->v.x1 = -xorg;
	r->v.y1 = -yorg;
	return r;
}


double x, y, z, xx, yy, zz;

/*
 * I.
 * [scalex |      0 | -org.x ]
 * [     0 | scaley | -org.y ]
 *
 * II.
 * [ cos(z)cos(y) - sin(z)cos(x)sin(y) | sin(z)cos(z) + cos(z)cos(x)sin(y)]
 * [ -sin(z)cos(x)                     | cos(z)cos(x)                     ]
 *
 * III.
 * z = 1 / ( 1 + 0.00005 * ( x[sin(z)sin(x)cos(y)+cos(z)] +
 *                           y[-cos(z)sin(x)cos(y)+sin(z)]))
 * [ z | 0 | 0 ]
 * [ 0 | z | 0 ]
 */


x = x( caz*cay - saz*cax*say)
    y(saz*cay + caz*cax*say)

y = x(-saz*cax)
    y(caz*cax)

z = x(saz*sax*cay + caz) + y(-caz*sax*cay + saz)
zv  1/(1+C* z )

		xx = x*cay + z*say;
		yy = y;

		zz = x*say - z*cay;

		zz = max(zz, -19000);

		x = xx * zv;
		y = yy * zv;

		mpPathPoints[i].x = (LONG)(x + org.x + 0.5);
		mpPathPoints[i].y = (LONG)(y + org.y + 0.5);

