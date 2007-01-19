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

#pragma warning(disable:4996)

#include <streams.h>
#include "directshow_h.h"

#define ASA_DEPRECATED
#include "common.h"
#include "asa.h"

#pragma comment(lib, "strmiids")
#pragma comment(lib, "strmbase")
#pragma comment(lib, "winmm")

class ASADShow : public CTransInPlaceFilter
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

	DECLARE_IUNKNOWN;

	HRESULT Transform(IMediaSample *pSample);
	HRESULT CheckInputType(const CMediaType *mtIn);
	HRESULT SetMediaType(PIN_DIRECTION pindir,
		const CMediaType *pMediaType);

private:
	ASADShow(LPUNKNOWN punk, HRESULT *phr);
	~ASADShow();

	struct asa_inst *asa;

	int width, height;
};

const AMOVIESETUP_FILTER asaDShowSetup = {
	&CLSID_ASADShow,
	L"asa",
	MERIT_DO_NOT_USE,
	0,						// pin count
	NULL						// pin details
};

CUnknown * WINAPI ASADShow::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
	CUnknown *dinst;
	static int asa_initialized = 0;

	if (!asa_initialized) {
		const char *asa_version = asa_init(ASA_VERSION);
		if (!asa_version) {
			return NULL;
			*phr = E_UNEXPECTED;
		}
		asa_initialized++;
	}

	try {
		dinst = new ASADShow(lpunk, phr);
	} catch (...) {
		dinst = NULL;
	}
	if (!dinst)
		*phr = E_OUTOFMEMORY;
	return dinst;
}

ASADShow::ASADShow(LPUNKNOWN punk,HRESULT *phr)
	: CTransInPlaceFilter(TEXT("ASADShow"), punk, CLSID_ASADShow, phr)
{
	asa = asa_open("C:\\asa_dshow.ass", ASAF_NONE);
	if (!asa)
		throw 1;
}

ASADShow::~ASADShow()
{
	if (asa)
		asa_close(asa);
}

HRESULT ASADShow::SetMediaType(PIN_DIRECTION pindir,
	const CMediaType *pMediaType)
{
	CheckPointer(pMediaType, E_POINTER);
	VIDEOINFO* pVI = (VIDEOINFO*)pMediaType->Format();
	CheckPointer(pVI, E_UNEXPECTED);

	width = pVI->bmiHeader.biWidth;
	height = pVI->bmiHeader.biHeight;
	asa_setsize(asa, width, height);

	return CTransInPlaceFilter::SetMediaType(pindir, pMediaType);
}


HRESULT ASADShow::CheckInputType(const CMediaType *mtIn)
{
	CheckPointer(mtIn,E_POINTER);

	if (*mtIn->FormatType() != FORMAT_VideoInfo)
		return E_INVALIDARG;
	if (*mtIn->Type() != MEDIATYPE_Video)
		return E_INVALIDARG;
	if (*mtIn->Subtype() != MEDIASUBTYPE_YV12
		&& *mtIn->Subtype() != MEDIASUBTYPE_RGB32
		&& *mtIn->Subtype() != MEDIASUBTYPE_RGB24)
        return E_INVALIDARG;

	VIDEOINFO *pVI = (VIDEOINFO *)mtIn->Format();
	CheckPointer(pVI,E_UNEXPECTED);

	if (pVI->bmiHeader.biHeight < 0)
		return E_INVALIDARG;

	return NOERROR;
}


HRESULT ASADShow::Transform(IMediaSample *pSample)
{
	CMediaType *mtIn;
	REFERENCE_TIME rtStart = 0, rtStop = 0;
	struct asa_frame frame;
	double mytime;
	BYTE *framed = NULL;
	CheckPointer(pSample,E_POINTER);

	mtIn = &m_pInput->CurrentMediaType();
	CheckPointer(mtIn,E_UNEXPECTED);

	pSample->GetTime(&rtStart, &rtStop);
	mytime = double(double(rtStart) / UNITS);

	pSample->GetPointer(&framed);

	if (*mtIn->Subtype() == MEDIASUBTYPE_YV12) {
		frame.csp = ASACSP_YUV_PLANAR;
		frame.bmp.yuv_planar.y.d = framed;
		frame.bmp.yuv_planar.y.stride = width;
		frame.bmp.yuv_planar.u.d = framed + width * height;
		frame.bmp.yuv_planar.u.stride = width >> 1;
		frame.bmp.yuv_planar.v.d = framed + width * height + (width >> 1) * (height >> 1);
		frame.bmp.yuv_planar.v.stride = width >> 1;
		frame.bmp.yuv_planar.chroma_x_red = 1;
		frame.bmp.yuv_planar.chroma_y_red = 1;
	} else if (*mtIn->Subtype() == MEDIASUBTYPE_RGB32) {
		frame.csp = ASACSP_RGB;
		frame.bmp.rgb.fmt = ASACSPR_RGBx;
		frame.bmp.rgb.d.d = framed + (height - 1) * width * 4;
		frame.bmp.rgb.d.stride = -width * 4;
	} else if (*mtIn->Subtype() == MEDIASUBTYPE_RGB24) {
		frame.csp = ASACSP_RGB;
		frame.bmp.rgb.fmt = ASACSPR_RGB;
		frame.bmp.rgb.d.d = framed;
		frame.bmp.rgb.d.stride = width * 3;
	}

	asa_render(asa, mytime, &frame);

	return NOERROR;
}


/*
 * Bookkeeping
 */

#include "directshow_i.c"
CFactoryTemplate g_Templates[] =
{
	{ L"ASADShow", &CLSID_ASADShow, ASADShow::CreateInstance, NULL,
		&asaDShowSetup }
};
int g_cTemplates = 1;

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2(FALSE);
}

