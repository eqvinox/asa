/*****************************************************************************
 * asa: portable digital subtitle renderer
 *****************************************************************************
 * Copyright (C) 2006  David Lamparter
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

#include <windows.h>
#include "avisynth.h"
#include <stdio.h>

#include <csri/csri.h>

class CSRIAviSynth : public GenericVideoFilter {
	csri_inst *inst;
	double spf;

public:
	CSRIAviSynth(PClip _child, IScriptEnvironment *env, const char *file,
		const char *rendname, const char *rendver);
	~CSRIAviSynth();

	enum csri_pixfmt GetPixfmt();

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);

	static AVSValue __cdecl Create(AVSValue args, void* user_data,
		IScriptEnvironment* env);
};

CSRIAviSynth::CSRIAviSynth(PClip _child, IScriptEnvironment *env,
	const char *file, const char *rendname, const char *rendver)
	: GenericVideoFilter(_child)
{
	csri_rend *r = csri_renderer_byname(rendname, rendver);
	if (!r) {
		if (rendver)
			env->ThrowError("Failed to load renderer \"%s\""
				" version \"%s\"", rendname, rendver);
		else if (rendname)
			env->ThrowError("Failed to load renderer \"%s\"",
				rendname);
		else
			env->ThrowError("Failed to load default renderer");
	}

	struct csri_fmt fmt;
	fmt.pixfmt = GetPixfmt();
	if (fmt.pixfmt == -1)
		env->ThrowError("Pixel format not supported by "
			"AviSynth interface");

	inst = csri_open_file(r, file, NULL);
	if (!inst)
		env->ThrowError("Failed to load \"%s\"", file);

	fmt.width = vi.width;
	fmt.height = vi.height;
	if (csri_request_fmt(inst, &fmt)) {
		csri_close(inst);
		env->ThrowError("Selected pixel format or size not supported "
			"by selected subtitle renderer", file);
	}
	spf = (double)vi.fps_denominator / (double)vi.fps_numerator;
}

CSRIAviSynth::~CSRIAviSynth()
{
	csri_close(inst);
}

enum csri_pixfmt CSRIAviSynth::GetPixfmt()
{
	switch (vi.pixel_type) {
	case VideoInfo::CS_BGR24:	return CSRI_F_BGR;
	case VideoInfo::CS_BGR32:	return CSRI_F_BGR_;
	case VideoInfo::CS_YUY2:	return CSRI_F_YUY2;
	case VideoInfo::CS_YV12:	return CSRI_F_YV12;
	}
	return (enum csri_pixfmt)-1;
}

PVideoFrame __stdcall CSRIAviSynth::GetFrame(int n, IScriptEnvironment *env)
{
	PVideoFrame avsframe = child->GetFrame(n, env);
	struct csri_frame frame;

	env->MakeWritable(&avsframe);

	frame.pixfmt = GetPixfmt();
	frame.planes[0] = avsframe->GetWritePtr();
	frame.strides[0] = avsframe->GetPitch();
	if (csri_is_yuv_planar(frame.pixfmt)) {
		frame.planes[1] = avsframe->GetWritePtr(PLANAR_U);
		frame.strides[1] = avsframe->GetPitch(PLANAR_U);
		frame.planes[2] = avsframe->GetWritePtr(PLANAR_V);
		frame.strides[2] = avsframe->GetPitch(PLANAR_V);
	}
	if (csri_is_rgb(frame.pixfmt)) {
		frame.planes[0] += (vi.height - 1) * frame.strides[0];
		frame.strides[0] = -frame.strides[0];
	}

	csri_render(inst, &frame, n * spf);
	return avsframe;
}

AVSValue __cdecl CSRIAviSynth::Create(AVSValue args, void* user_data,
	IScriptEnvironment* env)
{
	const char *rname = args.ArraySize() >= 2 ? args[2].AsString() : NULL,
		*rver = args.ArraySize() >= 3 ? args[3].AsString() : NULL;
	return new CSRIAviSynth(args[0].AsClip(), env, args[1].AsString(),
		rname, rver);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(
	IScriptEnvironment* env)
{
	static char avs_name_f[2048];
	const char *avs_cmdname = (const char *)csri_query_ext(NULL,
		"csri.avisynth25.command_name");
	const char *avs_name = (const char *)csri_query_ext(NULL,
		"csri.avisynth25.name");

	if (!csri_renderer_default())
		return NULL;

	if (!avs_cmdname)
		avs_cmdname = "CSRI";
	if (!avs_name)
		avs_name = "Common Subtitle Renderer Interface";

	env->AddFunction(avs_cmdname, "cs", CSRIAviSynth::Create, 0);
	env->AddFunction(avs_cmdname, "css", CSRIAviSynth::Create, 0);
	env->AddFunction(avs_cmdname, "csss", CSRIAviSynth::Create, 0);

	snprintf(avs_name_f, sizeof(avs_name_f),
		"%s [AviSynth 2.5 front-end]", avs_name);
	return avs_name_f;
}

