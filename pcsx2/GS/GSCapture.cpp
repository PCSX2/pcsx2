/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "GSCapture.h"
#include "GSPng.h"
#include "GSUtil.h"
#include "GSExtra.h"
#include "common/StringUtil.h"

#ifdef _WIN32

static void __stdcall ClosePinInfo(_Inout_ PIN_INFO* info) WI_NOEXCEPT
{
	if (info->pFilter)
	{
		info->pFilter->Release();
	}
}

static void __stdcall CloseFilterInfo(_Inout_ FILTER_INFO* info) WI_NOEXCEPT
{
	if (info->pGraph)
	{
		info->pGraph->Release();
	}
}

using unique_pin_info = wil::unique_struct<PIN_INFO, decltype(&::ClosePinInfo), ::ClosePinInfo>;
using unique_filter_info = wil::unique_struct<FILTER_INFO, decltype(&::CloseFilterInfo), ::CloseFilterInfo>;

template<typename Func>
static void EnumFilters(IGraphBuilder* filterGraph, Func&& f)
{
	wil::com_ptr_nothrow<IEnumFilters> enumFilters;
	if (SUCCEEDED(filterGraph->EnumFilters(enumFilters.put())))
	{
		wil::com_ptr_nothrow<IBaseFilter> baseFilter;
		while (enumFilters->Next(1, baseFilter.put(), nullptr) == S_OK)
		{
			std::forward<Func>(f)(baseFilter.get());
		}
	}
}

template<typename Func>
static void EnumPins(IBaseFilter* baseFilter, Func&& f)
{
	wil::com_ptr_nothrow<IEnumPins> enumPins;
	if (SUCCEEDED(baseFilter->EnumPins(enumPins.put())))
	{
		wil::com_ptr_nothrow<IPin> pin;
		while (enumPins->Next(1, pin.put(), nullptr) == S_OK)
		{
			if (!std::forward<Func>(f)(pin.get()))
			{
				break;
			}
		}
	}
}

//
// GSSource
//
interface __declspec(uuid("59C193BB-C520-41F3-BC1D-E245B80A86FA"))
IGSSource : public IUnknown
{
	STDMETHOD(DeliverNewSegment)() PURE;
	STDMETHOD(DeliverFrame)(const void* bits, int pitch, bool rgba) PURE;
	STDMETHOD(DeliverEOS)() PURE;
};

class __declspec(uuid("F8BB6F4F-0965-4ED4-BA74-C6A01E6E6C77"))
GSSource : public CBaseFilter, private CCritSec, public IGSSource
{
	GSVector2i m_size;
	REFERENCE_TIME m_atpf;
	REFERENCE_TIME m_now;

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv)
	{
		return riid == __uuidof(IGSSource)
			? GetInterface((IGSSource*)this, ppv)
			: __super::NonDelegatingQueryInterface(riid, ppv);
	}

	class GSSourceOutputPin : public CBaseOutputPin
	{
		GSVector2i m_size;
		std::vector<CMediaType> m_mts;

	public:
		GSSourceOutputPin(const GSVector2i& size, REFERENCE_TIME atpf, CBaseFilter* pFilter, CCritSec* pLock, HRESULT& hr, int colorspace)
			: CBaseOutputPin("GSSourceOutputPin", pFilter, pLock, &hr, L"Output")
			, m_size(size)
		{
			CMediaType mt;
			mt.majortype = MEDIATYPE_Video;
			mt.formattype = FORMAT_VideoInfo;

			VIDEOINFOHEADER vih;
			memset(&vih, 0, sizeof(vih));
			vih.AvgTimePerFrame = atpf;
			vih.bmiHeader.biSize = sizeof(vih.bmiHeader);
			vih.bmiHeader.biWidth = m_size.x;
			vih.bmiHeader.biHeight = m_size.y;

			// YUY2

			mt.subtype = MEDIASUBTYPE_YUY2;
			mt.lSampleSize = m_size.x * m_size.y * 2;

			vih.bmiHeader.biCompression = '2YUY';
			vih.bmiHeader.biPlanes = 1;
			vih.bmiHeader.biBitCount = 16;
			vih.bmiHeader.biSizeImage = m_size.x * m_size.y * 2;
			mt.SetFormat((u8*)&vih, sizeof(vih));

			m_mts.push_back(mt);

			// RGB32

			mt.subtype = MEDIASUBTYPE_RGB32;
			mt.lSampleSize = m_size.x * m_size.y * 4;

			vih.bmiHeader.biCompression = BI_RGB;
			vih.bmiHeader.biPlanes = 1;
			vih.bmiHeader.biBitCount = 32;
			vih.bmiHeader.biSizeImage = m_size.x * m_size.y * 4;
			mt.SetFormat((u8*)&vih, sizeof(vih));

			if (colorspace == 1)
				m_mts.insert(m_mts.begin(), mt);
			else
				m_mts.push_back(mt);
		}

		HRESULT GSSourceOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
		{
			ASSERT(pAlloc && pProperties);

			HRESULT hr;

			pProperties->cBuffers = 1;
			pProperties->cbBuffer = m_mt.lSampleSize;

			ALLOCATOR_PROPERTIES Actual;

			if (FAILED(hr = pAlloc->SetProperties(pProperties, &Actual)))
			{
				return hr;
			}

			if (Actual.cbBuffer < pProperties->cbBuffer)
			{
				return E_FAIL;
			}

			ASSERT(Actual.cBuffers == pProperties->cBuffers);

			return S_OK;
		}

		HRESULT CheckMediaType(const CMediaType* pmt)
		{
			for (const auto& mt : m_mts)
			{
				if (mt.majortype == pmt->majortype && mt.subtype == pmt->subtype)
				{
					return S_OK;
				}
			}

			return E_FAIL;
		}

		HRESULT GetMediaType(int i, CMediaType* pmt)
		{
			CheckPointer(pmt, E_POINTER);

			if (i < 0)
				return E_INVALIDARG;
			if (i > 1)
				return VFW_S_NO_MORE_ITEMS;

			*pmt = m_mts[i];

			return S_OK;
		}

		STDMETHODIMP Notify(IBaseFilter* pSender, Quality q)
		{
			return E_NOTIMPL;
		}

		const CMediaType& CurrentMediaType()
		{
			return m_mt;
		}
	};

	GSSourceOutputPin* m_output;

public:
	GSSource(int w, int h, float fps, IUnknown* pUnk, HRESULT& hr, int colorspace)
		: CBaseFilter("GSSource", pUnk, this, __uuidof(this), &hr)
		, m_output(NULL)
		, m_size(w, h)
		, m_atpf((REFERENCE_TIME)(10000000.0f / fps))
		, m_now(0)
	{
		m_output = new GSSourceOutputPin(m_size, m_atpf, this, this, hr, colorspace);
	}

	virtual ~GSSource()
	{
		delete m_output;
	}

	DECLARE_IUNKNOWN;

	int GetPinCount()
	{
		return 1;
	}

	CBasePin* GetPin(int n)
	{
		return n == 0 ? m_output : NULL;
	}

	// IGSSource

	STDMETHODIMP DeliverNewSegment()
	{
		m_now = 0;

		return m_output->DeliverNewSegment(0, _I64_MAX, 1.0);
	}

	STDMETHODIMP DeliverFrame(const void* bits, int pitch, bool rgba)
	{
		if (!m_output || !m_output->IsConnected())
		{
			return E_UNEXPECTED;
		}

		wil::com_ptr_nothrow<IMediaSample> sample;

		if (FAILED(m_output->GetDeliveryBuffer(sample.put(), NULL, NULL, 0)))
		{
			return E_FAIL;
		}

		REFERENCE_TIME start = m_now;
		REFERENCE_TIME stop = m_now + m_atpf;

		sample->SetTime(&start, &stop);
		sample->SetSyncPoint(TRUE);

		const CMediaType& mt = m_output->CurrentMediaType();

		u8* src = (u8*)bits;
		u8* dst = NULL;

		sample->GetPointer(&dst);

		int w = m_size.x;
		int h = m_size.y;
		int srcpitch = pitch;

		if (mt.subtype == MEDIASUBTYPE_YUY2)
		{
			int dstpitch = ((VIDEOINFOHEADER*)mt.Format())->bmiHeader.biWidth * 2;

			GSVector4 ys(0.257f, 0.504f, 0.098f, 0.0f);
			GSVector4 us(-0.148f / 2, -0.291f / 2, 0.439f / 2, 0.0f);
			GSVector4 vs(0.439f / 2, -0.368f / 2, -0.071f / 2, 0.0f);

			if (!rgba)
			{
				ys = ys.zyxw();
				us = us.zyxw();
				vs = vs.zyxw();
			}

			const GSVector4 offset(16, 128, 16, 128);

			for (int j = 0; j < h; j++, dst += dstpitch, src += srcpitch)
			{
				u32* s = (u32*)src;
				u16* d = (u16*)dst;

				for (int i = 0; i < w; i += 2)
				{
					GSVector4 c0 = GSVector4::rgba32(s[i + 0]);
					GSVector4 c1 = GSVector4::rgba32(s[i + 1]);
					GSVector4 c2 = c0 + c1;

					GSVector4 lo = (c0 * ys).hadd(c2 * us);
					GSVector4 hi = (c1 * ys).hadd(c2 * vs);

					GSVector4 c = lo.hadd(hi) + offset;

					*((u32*)&d[i]) = GSVector4i(c).rgba32();
				}
			}
		}
		else if (mt.subtype == MEDIASUBTYPE_RGB32)
		{
			int dstpitch = ((VIDEOINFOHEADER*)mt.Format())->bmiHeader.biWidth * 4;

			dst += dstpitch * (h - 1);
			dstpitch = -dstpitch;

			for (int j = 0; j < h; j++, dst += dstpitch, src += srcpitch)
			{
				if (rgba)
				{
					GSVector4i* s = (GSVector4i*)src;
					GSVector4i* d = (GSVector4i*)dst;

					GSVector4i mask(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);

					for (int i = 0, w4 = w >> 2; i < w4; i++)
					{
						d[i] = s[i].shuffle8(mask);
					}
				}
				else
				{
					memcpy(dst, src, w * 4);
				}
			}
		}
		else
		{
			return E_FAIL;
		}

		if (FAILED(m_output->Deliver(sample.get())))
		{
			return E_FAIL;
		}

		m_now = stop;

		return S_OK;
	}

	STDMETHODIMP DeliverEOS()
	{
		return m_output->DeliverEndOfStream();
	}
};

static wil::com_ptr_nothrow<IPin> GetFirstPin(IBaseFilter* pBF, PIN_DIRECTION dir)
{
	wil::com_ptr_nothrow<IPin> result;
	if (pBF)
	{
		EnumPins(pBF, [&](IPin* pin)
		{
			PIN_DIRECTION dir2;
			pin->QueryDirection(&dir2);
			if (dir == dir2)
			{
				result = pin;
				return false;
			}
			return true;
		});
	}

	return result;
}

#endif

//
// GSCapture
//

GSCapture::GSCapture()
	: m_capturing(false), m_frame(0)
	, m_out_dir("/tmp/GS_Capture") // FIXME Later add an option
{
}

GSCapture::~GSCapture()
{
	EndCapture();
}

bool GSCapture::BeginCapture(float fps, GSVector2i recommendedResolution, float aspect, std::string& filename)
{
	printf("Recommended resolution: %d x %d, DAR for muxing: %.4f\n", recommendedResolution.x, recommendedResolution.y, aspect);
	std::lock_guard<std::recursive_mutex> lock(m_lock);

	ASSERT(fps != 0);

	EndCapture();

	// reload settings because they may have changed
	m_out_dir = theApp.GetConfigS("capture_out_dir");
	m_threads = theApp.GetConfigI("capture_threads");
#if defined(__unix__)
	m_compression_level = theApp.GetConfigI("png_compression_level");
#endif

#ifdef _WIN32

	GSCaptureDlg dlg;

	if (IDOK != dlg.DoModal())
		return false;

	{
		const int start = dlg.m_filename.length() - 4;
		if (start > 0)
		{
			std::wstring test = dlg.m_filename.substr(start);
			std::transform(test.begin(), test.end(), test.begin(), (char(_cdecl*)(int))tolower);
			if (test.compare(L".avi") != 0)
				dlg.m_filename += L".avi";
		}
		else
			dlg.m_filename += L".avi";

		FILE* test = _wfopen(dlg.m_filename.c_str(), L"w");
		if (test)
			fclose(test);
		else
		{
			dlg.InvalidFile();
			return false;
		}
	}

	m_size.x = (dlg.m_width + 7) & ~7;
	m_size.y = (dlg.m_height + 7) & ~7;
	//

	auto graph = wil::CoCreateInstanceNoThrow<IGraphBuilder>(CLSID_FilterGraph);
	if (!graph)
	{
		return false;
	}
	auto cgb = wil::CoCreateInstanceNoThrow<ICaptureGraphBuilder2>(CLSID_CaptureGraphBuilder2);
	if (!cgb)
	{
		return false;
	}

	wil::com_ptr_nothrow<IBaseFilter> mux;
	if (FAILED(cgb->SetFiltergraph(graph.get()))
	 || FAILED(cgb->SetOutputFileName(&MEDIASUBTYPE_Avi, std::wstring(dlg.m_filename.begin(), dlg.m_filename.end()).c_str(), mux.put(), nullptr)))
	{
		return false;
	}

	HRESULT source_hr = S_OK;
	// WARNING: This increases the reference count! Right now it's fine, since GSSource inherits from CUnknown that
	// starts the reference count from 0. Should this ever change and GSSource ends up with a refcount of 1 after constructing,
	// change this to `.attach(new GSSource(...))`.
	wil::com_ptr_nothrow<IBaseFilter> src = new GSSource(m_size.x, m_size.y, fps, NULL, source_hr, dlg.m_colorspace);

	if (dlg.m_enc == 0)
	{
		if (FAILED(graph->AddFilter(src.get(), L"Source")))
			return false;
		if (FAILED(graph->ConnectDirect(GetFirstPin(src.get(), PINDIR_OUTPUT).get(), GetFirstPin(mux.get(), PINDIR_INPUT).get(), nullptr)))
			return false;
	}
	else
	{
		if (FAILED(graph->AddFilter(src.get(), L"Source")) || FAILED(graph->AddFilter(dlg.m_enc.get(), L"Encoder")))
		{
			return false;
		}

		if (FAILED(graph->ConnectDirect(GetFirstPin(src.get(), PINDIR_OUTPUT).get(), GetFirstPin(dlg.m_enc.get(), PINDIR_INPUT).get(), nullptr))
		 || FAILED(graph->ConnectDirect(GetFirstPin(dlg.m_enc.get(), PINDIR_OUTPUT).get(), GetFirstPin(mux.get(), PINDIR_INPUT).get(), nullptr)))
		{
			return false;
		}
	}

	EnumFilters(graph.get(), [](IBaseFilter* baseFilter)
	{
		unique_filter_info filter;
		baseFilter->QueryFilterInfo(&filter);
		printf("Filter [%p]: %ls\n", baseFilter, filter.achName);

		EnumPins(baseFilter, [](IPin* pin)
		{
			wil::com_ptr_nothrow<IPin> pinTo;
			pin->ConnectedTo(pinTo.put());

			unique_pin_info pi;
			pin->QueryPinInfo(&pi);
			printf("- Pin [%p - %p]: %ls (%s)\n", pin, pinTo.get(), pi.achName, pi.dir ? "out" : "in");
			return true;
		});
	});

	// Moving forward, we want failfast semantics so "commit" these interfaces by persisting them in the class
	m_graph = std::move(graph);
	m_src = std::move(src);

	m_graph.query<IMediaControl>()->Run();
	m_src.query<IGSSource>()->DeliverNewSegment();

	m_capturing = true;
	filename = StringUtil::WideStringToUTF8String(dlg.m_filename.erase(dlg.m_filename.length() - 3, 3) + L"wav");
	return true;
#elif defined(__unix__)
	// Note I think it doesn't support multiple depth creation
	GSmkdir(m_out_dir.c_str());

	// Really cheap recording
	m_frame = 0;
	// Add option !!!
	m_size.x = theApp.GetConfigI("CaptureWidth");
	m_size.y = theApp.GetConfigI("CaptureHeight");

	for (int i = 0; i < m_threads; i++)
	{
		m_workers.push_back(std::unique_ptr<GSPng::Worker>(new GSPng::Worker({}, &GSPng::Process, {})));
	}

	m_capturing = true;
	filename = m_out_dir + "/audio_recording.wav";
	return true;
#endif
}

bool GSCapture::DeliverFrame(const void* bits, int pitch, bool rgba)
{
	std::lock_guard<std::recursive_mutex> lock(m_lock);

	if (bits == NULL || pitch == 0)
	{
		ASSERT(0);

		return false;
	}

#ifdef _WIN32

	if (m_src)
	{
		m_src.query<IGSSource>()->DeliverFrame(bits, pitch, rgba);

		return true;
	}

#elif defined(__unix__)

	std::string out_file = m_out_dir + StringUtil::StdStringFromFormat("/frame.%010d.png", m_frame);
	//GSPng::Save(GSPng::RGB_PNG, out_file, (u8*)bits, m_size.x, m_size.y, pitch, m_compression_level);
	m_workers[m_frame % m_threads]->Push(std::make_shared<GSPng::Transaction>(GSPng::RGB_PNG, out_file, static_cast<const u8*>(bits), m_size.x, m_size.y, pitch, m_compression_level));

	m_frame++;

#endif

	return false;
}

bool GSCapture::EndCapture()
{
	if (!m_capturing)
		return false;

	std::lock_guard<std::recursive_mutex> lock(m_lock);

#ifdef _WIN32

	if (m_src)
	{
		m_src.query<IGSSource>()->DeliverEOS();
		m_src.reset();
	}

	if (m_graph)
	{
		m_graph.query<IMediaControl>()->Stop();
		m_graph.reset();;
	}

#elif defined(__unix__)
	m_workers.clear();

	m_frame = 0;

#endif

	m_capturing = false;

	return true;
}
