/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2006  David Lamparter
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

#include <windows.h>
#include "avisynth.h"
#include <string.h>

#include "asa.h"

class ASAAviSynth : public GenericVideoFilter {
	struct asa_inst *asa;
	double fps;

public:
	ASAAviSynth(PClip _child, const char *file, IScriptEnvironment *env);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);

	static AVSValue __cdecl Create(AVSValue args, void* user_data,
		IScriptEnvironment* env);
};

ASAAviSynth::ASAAviSynth(PClip _child, const char *file,
	IScriptEnvironment *env)
	: GenericVideoFilter(_child)
{
	if (!vi.IsYV12())
		env->ThrowError("asa supports YV12 only");

	asa = asa_open(file, (enum asa_oflags)0);
	if (!asa)
		env->ThrowError("Failed to open script \"%s\"", file);
	asa_setsize(asa, vi.width, vi.height);
	fps = (double)vi.fps_numerator / (double)vi.fps_denominator;
}

PVideoFrame __stdcall ASAAviSynth::GetFrame(int n, IScriptEnvironment *env)
{
	PVideoFrame avsframe = child->GetFrame(n, env);
	struct asa_frame frame;

	env->MakeWritable(&avsframe);

	frame.csp = ASACSP_YUV_PLANAR;
	frame.bmp.yuv_planar.y.d = avsframe->GetWritePtr();
	frame.bmp.yuv_planar.y.stride = avsframe->GetPitch();
	frame.bmp.yuv_planar.u.d = avsframe->GetWritePtr(PLANAR_U);
	frame.bmp.yuv_planar.u.stride = avsframe->GetPitch(PLANAR_U);
	frame.bmp.yuv_planar.v.d = avsframe->GetWritePtr(PLANAR_V);
	frame.bmp.yuv_planar.v.stride = avsframe->GetPitch(PLANAR_V);
	frame.bmp.yuv_planar.chroma_x_red = 1;
	frame.bmp.yuv_planar.chroma_y_red = 1;

	asa_render(asa, n * fps, &frame);
	return avsframe;
}

AVSValue __cdecl ASAAviSynth::Create(AVSValue args, void* user_data,
	IScriptEnvironment* env)
{
	return new ASAAviSynth(args[0].AsClip(), args[1].AsString(), env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(
	IScriptEnvironment* env)
{
	static char versbuf[2048] = "asa ";
	const char *asa_version = asa_init(ASA_VERSION);
	if (!asa_version)
		return NULL;

	env->AddFunction("asa", "cs", ASAAviSynth::Create, 0);
	strcat(versbuf, asa_version);
	strcat(versbuf, " [AviSynth interface]");
	return versbuf;
}

