// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Console.h"
#include "common/Error.h"
#include "common/StringUtil.h"

#include "videodev.h"
#include "cam-jpeg.h"
#include "cam-windows.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"
#include "USB/USB.h"

#include <jpeglib.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-goto"
#endif

#define safe_release(ptr) \
	((void)((((ptr) != NULL) && ((ptr)->Release(), !!0)), (ptr) = NULL))

namespace usb_eyetoy
{
	namespace windows_api
	{
		buffer_t mpeg_buffer{};
		std::mutex mpeg_mutex;
		int frame_width;
		int frame_height;
		FrameFormat frame_format;
		bool mirroring_enabled = true;

		HRESULT DirectShow::CallbackHandler::SampleCB(double time, IMediaSample* sample)
		{
			unsigned char* buffer;

			const HRESULT hr = sample->GetPointer((BYTE**)&buffer);
			if (hr != S_OK)
				return S_OK;

			if (callback)
				callback(buffer, sample->GetActualDataLength(), BITS_PER_PIXEL);
			return S_OK;
		}

		HRESULT DirectShow::CallbackHandler::QueryInterface(REFIID iid, LPVOID* ppv)
		{
			if (iid == IID_ISampleGrabberCB || iid == IID_IUnknown)
			{
				*ppv = (void*)static_cast<ISampleGrabberCB*>(this);
				return S_OK;
			}
			return E_NOINTERFACE;
		}

		std::vector<std::pair<std::string, std::string>> getDevList()
		{
			std::vector<std::pair<std::string, std::string>> devList;

			ICreateDevEnum* pCreateDevEnum = nullptr;
			HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
			if (FAILED(hr))
			{
				Console.Warning("Camera: Error Creating Device Enumerator");
				return devList;
			}

			IEnumMoniker* pEnum = nullptr;
			hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			if (hr != S_OK)
			{
				Console.Warning("Camera: You have no video capture hardware");
				return devList;
			};

			IMoniker* pMoniker = nullptr;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
			{
				IPropertyBag* pPropBag = nullptr;
				hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
				if (FAILED(hr))
				{
					Console.Warning("Camera: BindToStorage err : %x", hr);
					pMoniker->Release();
					continue;
				}

				VARIANT var;
				VariantInit(&var);
				hr = pPropBag->Read(L"Description", &var, 0);
				if (FAILED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &var, 0);
				}
				if (SUCCEEDED(hr))
				{
					std::string u8name(StringUtil::WideStringToUTF8String(var.bstrVal));
					devList.emplace_back(u8name, u8name);
					VariantClear(&var);
				}

				pPropBag->Release();
				pMoniker->Release();
			}

			pEnum->Release();
			CoUninitialize();

			return devList;
		}

		int DirectShow::InitializeDevice(const std::wstring& selectedDevice)
		{

			// Create the Capture Graph Builder.
			HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraphBuilder));
			if (FAILED(hr))
			{
				Console.Warning("Camera: CoCreateInstance CLSID_CaptureGraphBuilder2 err : %x", hr);
				return -1;
			}

			// Create the Filter Graph Manager.
			hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
			if (FAILED(hr))
			{
				Console.Warning("Camera: CoCreateInstance CLSID_FilterGraph err : %x", hr);
				return -1;
			}

			hr = pGraphBuilder->SetFiltergraph(pGraph);
			if (FAILED(hr))
			{
				Console.Warning("Camera: SetFiltergraph err : %x", hr);
				return -1;
			}

			hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
			if (FAILED(hr))
			{
				Console.Warning("Camera: QueryInterface IID_IMediaControl err : %x", hr);
				return -1;
			}

			// enumerate all video capture devices
			ICreateDevEnum* pCreateDevEnum = nullptr;
			hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
			if (FAILED(hr))
			{
				Console.Warning("Camera: Error Creating Device Enumerator");
				return -1;
			}

			IEnumMoniker* pEnum = nullptr;
			hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			if (hr != S_OK)
			{
				Console.Warning("Camera: You have no video capture hardware");
				return -1;
			};

			pEnum->Reset();

			IMoniker* pMoniker = nullptr;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK && sourcefilter == nullptr)
			{
				LONGLONG start = 0, stop = MAXLONGLONG;

				IPropertyBag* pPropBag = nullptr;
				hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
				if (FAILED(hr))
				{
					Console.Warning("Camera: BindToStorage err : %x", hr);
					goto freeMoniker;
				}

				VARIANT var;
				VariantInit(&var);
				hr = pPropBag->Read(L"Description", &var, 0);
				if (FAILED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &var, 0);
				}
				if (FAILED(hr))
				{
					Console.Warning("Camera: Read name err : %x", hr);
					goto freeVar;
				}
				Console.Warning("Camera: '%ls'", var.bstrVal);
				if (!selectedDevice.empty() && selectedDevice != var.bstrVal)
				{
					goto freeVar;
				}

				//add a filter for the device
				hr = pGraph->AddSourceFilterForMoniker(pMoniker, NULL, L"sourcefilter", &sourcefilter);
				if (FAILED(hr))
				{
					Console.Warning("Camera: AddSourceFilterForMoniker err : %x", hr);
					goto freeVar;
				}

				hr = pGraphBuilder->FindInterface(
					&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, IID_IAMStreamConfig, (void**)&pSourceConfig);
				if (SUCCEEDED(hr))
				{
					int iCount = 0, iSize = 0;
					hr = pSourceConfig->GetNumberOfCapabilities(&iCount, &iSize);

					// Check the size to make sure we pass in the correct structure.
					if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
					{
						// Use the video capabilities structure.
						for (int iFormat = 0; iFormat < iCount; iFormat++)
						{
							VIDEO_STREAM_CONFIG_CAPS scc;
							AM_MEDIA_TYPE* pmtConfig;
							hr = pSourceConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
							Console.Warning("Camera: GetStreamCaps min=%dx%d max=%dx%d, fmt=%x", scc.MinOutputSize.cx, scc.MinOutputSize.cy,
								scc.MaxOutputSize.cx, scc.MaxOutputSize.cy, pmtConfig->subtype);

							if (SUCCEEDED(hr))
							{
								if ((pmtConfig->majortype == MEDIATYPE_Video) && (pmtConfig->formattype == FORMAT_VideoInfo) &&
									(pmtConfig->cbFormat >= sizeof(VIDEOINFOHEADER)) && (pmtConfig->pbFormat != nullptr))
								{
									VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
									pVih->bmiHeader.biWidth = frame_width;
									pVih->bmiHeader.biHeight = frame_height;
									pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);
									hr = pSourceConfig->SetFormat(pmtConfig);
									if (FAILED(hr))
									{
										Console.Warning("Camera: SetFormat err : %x", hr);
									}
								}
								//DeleteMediaType(pmtConfig);
							}
						}
					}
				}

				// Create the Sample Grabber filter.
				hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&samplegrabberfilter));
				if (FAILED(hr))
				{
					Console.Warning("Camera: CoCreateInstance CLSID_SampleGrabber err : %x", hr);
					goto freeVar;
				}

				hr = pGraph->AddFilter(samplegrabberfilter, L"samplegrabberfilter");
				if (FAILED(hr))
				{
					Console.Warning("Camera: AddFilter samplegrabberfilter err : %x", hr);
					goto freeVar;
				}

				//set mediatype on the samplegrabber
				hr = samplegrabberfilter->QueryInterface(IID_PPV_ARGS(&samplegrabber));
				if (FAILED(hr))
				{
					Console.Warning("Camera: QueryInterface err : %x", hr);
					goto freeVar;
				}

				AM_MEDIA_TYPE mt;
				ZeroMemory(&mt, sizeof(mt));
				mt.majortype = MEDIATYPE_Video;
				mt.subtype = MEDIASUBTYPE_RGB24;
				hr = samplegrabber->SetMediaType(&mt);
				if (FAILED(hr))
				{
					Console.Warning("Camera: SetMediaType err : %x", hr);
					goto freeVar;
				}

				//add the callback to the samplegrabber
				hr = samplegrabber->SetCallback(callbackhandler, 0);
				if (hr != S_OK)
				{
					Console.Warning("Camera: SetCallback err : %x", hr);
					goto freeVar;
				}

				//set the null renderer
				hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nullrenderer));
				if (FAILED(hr))
				{
					Console.Warning("Camera: CoCreateInstance CLSID_NullRenderer err : %x", hr);
					goto freeVar;
				}

				hr = pGraph->AddFilter(nullrenderer, L"nullrenderer");
				if (FAILED(hr))
				{
					Console.Warning("Camera: AddFilter nullrenderer err : %x", hr);
					goto freeVar;
				}

				//set the render path
				hr = pGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, sourcefilter, samplegrabberfilter, nullrenderer);
				if (FAILED(hr))
				{
					Console.Warning("Camera: RenderStream err : %x", hr);
					goto freeVar;
				}

				// if the stream is started, start capturing immediatly
				hr = pGraphBuilder->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, &start, &stop, 1, 2);
				if (FAILED(hr))
				{
					Console.Warning("Camera: ControlStream err : %x", hr);
					goto freeVar;
				}

			freeVar:
				VariantClear(&var);
				pPropBag->Release();

			freeMoniker:
				pMoniker->Release();
			}
			pEnum->Release();
			if (sourcefilter == nullptr)
			{
				return -1;
			}
			return 0;
		}

		bool DirectShow::Start()
		{
			HRESULT hr = nullrenderer->Run(0);
			if (FAILED(hr))
			{
				Console.Error("nullrenderer->Run() failed: %08X", hr);
				return false;
			}

			hr = samplegrabberfilter->Run(0);
			if (FAILED(hr))
			{
				Console.Error("samplegrabberfilter->Run() failed: %08X", hr);
				return false;
			}

			hr = sourcefilter->Run(0);
			if (FAILED(hr))
			{
				Console.Error("sourcefilter->Run() failed: %08X", hr);
				return false;
			}

			return true;
		}

		void DirectShow::Stop()
		{
			HRESULT hr = sourcefilter->Stop();
			if (FAILED(hr))
				Console.Error("sourcefilter->Stop() failed: %08X", hr);

			hr = samplegrabberfilter->Stop();
			if (FAILED(hr))
				Console.Error("samplegrabberfilter->Stop() failed: %08X", hr);

			hr = nullrenderer->Stop();
			if (FAILED(hr))
				Console.Error("nullrenderer->Stop() failed: %08X", hr);
		}

		void store_mpeg_frame(const unsigned char* data, const unsigned int len)
		{
			mpeg_mutex.lock();
			if (len > 0)
				memcpy(mpeg_buffer.start, data, len);
			mpeg_buffer.length = len;
			mpeg_mutex.unlock();
		}

		void dshow_callback(unsigned char* data, int len, int bitsperpixel)
		{
			if (bitsperpixel == 24)
			{
				const int bytesPerPixel = 3;
				const size_t comprBufSize = frame_width * frame_height * bytesPerPixel;
				std::vector<u8> comprBuf(comprBufSize);
				if (frame_format == format_mpeg)
				{
					const size_t comprLen = jo_write_mpeg(
						comprBuf.data(), data, frame_width, frame_height, JO_BGR24, mirroring_enabled ? JO_FLIP_X : JO_NONE, JO_FLIP_Y);
					comprBuf.resize(comprLen);
				}
				else if (frame_format == format_jpeg)
				{
					// flip Y - always required on windows
					unsigned char* data2 = (unsigned char*)calloc(1, comprBufSize);
					for (int y = 0; y < frame_height; y++)
					{
						for (int x = 0; x < frame_width; x++)
						{
							unsigned char* src = data + (y * frame_width + x) * bytesPerPixel;
							unsigned char* dst = data2 + ((frame_height - y - 1) * frame_width + x) * bytesPerPixel;
							dst[0] = src[2];
							dst[1] = src[1];
							dst[2] = src[0];
						}
					}

					if (!CompressCamJPEG(&comprBuf, data2, frame_width, frame_height, 80))
						comprBuf.clear();

					free(data2);
				}
				else if (frame_format == format_yuv400)
				{
					int in_pos = 0;
					for (int my = 0; my < 8; my++)
						for (int mx = 0; mx < 10; mx++)
							for (int y = 0; y < 8; y++)
								for (int x = 0; x < 8; x++)
								{
									int srcx = 4 * (8 * mx + x);
									int srcy = frame_height - 4 * (8 * my + y) - 1;
									unsigned char* src = data + (srcy * frame_width + srcx) * bytesPerPixel;
									if (srcy < 0)
									{
										comprBuf[in_pos++] = 0x01;
									}
									else
									{
										float r = src[2];
										float g = src[1];
										float b = src[0];
										comprBuf[in_pos++] = std::clamp<u8>(0.299f * r + 0.587f * g + 0.114f * b, 1, 255);
									}
								}
					comprBuf.resize(80 * 64);
				}
				else
				{
					comprBuf.clear();
				}
				store_mpeg_frame(comprBuf.data(), static_cast<unsigned int>(comprBuf.size()));
			}
			else
			{
				Console.Warning("Camera: dshow_callback: unknown format: len=%d bpp=%d", len, bitsperpixel);
			}
		}

		void create_dummy_frame_eyetoy()
		{
			constexpr int bytesPerPixel = 3;
			const int comprBufSize = frame_width * frame_height * bytesPerPixel;
			unsigned char* rgbData = (unsigned char*)calloc(1, comprBufSize);
			for (int y = 0; y < frame_height; y++)
			{
				for (int x = 0; x < frame_width; x++)
				{
					unsigned char* ptr = rgbData + (y * frame_width + x) * bytesPerPixel;
					ptr[0] = 255 * y / frame_height;
					ptr[1] = 255 * x / frame_width;
					ptr[2] = 255 * y / frame_height;
				}
			}

			std::vector<u8> comprBuf(comprBufSize);
			if (frame_format == format_mpeg)
			{
				const size_t comprLen = jo_write_mpeg(comprBuf.data(), rgbData, frame_width, frame_height, JO_RGB24, JO_NONE, JO_NONE);
				comprBuf.resize(comprLen);
			}
			else if (frame_format == format_jpeg)
			{
				if (!CompressCamJPEG(&comprBuf, rgbData, frame_width, frame_height, 80))
					comprBuf.clear();
			}
			else
			{
				comprBuf.clear();
			}
			free(rgbData);

			store_mpeg_frame(comprBuf.data(), static_cast<unsigned int>(comprBuf.size()));				
		}

		void create_dummy_frame_ov511p()
		{
			int comprBufSize = 80 * 64;
			unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
			if (frame_format == format_yuv400)
			{
				for (int y = 0; y < 64; y++)
				{
					for (int x = 0; x < 80; x++)
					{
						comprBuf[80 * y + x] = std::clamp<u8>(255 * y / 80, 1, 255);
					}
				}
			}
			store_mpeg_frame(comprBuf, comprBufSize);
			free(comprBuf);
		}

		DirectShow::DirectShow()
		{
			pGraphBuilder = nullptr;
			pGraph = nullptr;
			pControl = nullptr;
			sourcefilter = nullptr;
			samplegrabberfilter = nullptr;
			nullrenderer = nullptr;
			pSourceConfig = nullptr;
			samplegrabber = nullptr;
			callbackhandler = new CallbackHandler();
			mpeg_buffer.start = calloc(1, 640 * 480 * 2);
		}

		DirectShow::~DirectShow()
		{
			free(mpeg_buffer.start);
			mpeg_buffer.start = nullptr;
		}

		int DirectShow::Open(int width, int height, FrameFormat format, int mirror)
		{
			const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (FAILED(hr))
			{
				Console.ErrorFmt("CoInitializeEx failed: {}", Error::CreateHResult(hr).GetDescription());
				return -1;
			}
			wil::unique_couninitialize_call uninit;

			frame_width = width;
			frame_height = height;
			frame_format = format;
			mirroring_enabled = mirror;
			if (format == format_yuv400)
			{
				create_dummy_frame_ov511p();
			}
			else
			{
				create_dummy_frame_eyetoy();
			}

			int ret = InitializeDevice(StringUtil::UTF8StringToWideString(mHostDevice));
			if (ret < 0)
			{
				Console.WarningFmt("Camera: cannot find '{}'", mHostDevice);
				return -1;
			}

			pControl->Run();
			Stop();
			SetCallback(dshow_callback);
			if (!Start())
			{
				Console.Error("Camera: Failed to start");
				Stop();
				return -1;
			}

			dshowCoInitialize = std::move(uninit);
			return 0;
		};

		int DirectShow::Close()
		{
			if (sourcefilter)
			{
				this->Stop();
				pControl->Stop();

				safe_release(sourcefilter);
				safe_release(pSourceConfig);
				safe_release(samplegrabberfilter);
				safe_release(samplegrabber);
				safe_release(nullrenderer);
			}

			safe_release(pGraphBuilder);
			safe_release(pGraph);
			safe_release(pControl);
			dshowCoInitialize.reset();
			return 0;
		};

		int DirectShow::GetImage(uint8_t* buf, size_t len)
		{
			mpeg_mutex.lock();
			int len2 = mpeg_buffer.length;
			if (static_cast<size_t>(len) < mpeg_buffer.length)
				len2 = len;
			memcpy(buf, mpeg_buffer.start, len2);
			mpeg_buffer.length = 0;
			mpeg_mutex.unlock();
			return len2;
		};

		void DirectShow::SetMirroring(bool state) { mirroring_enabled = state; }
	} // namespace windows_api

	std::unique_ptr<VideoDevice> VideoDevice::CreateInstance()
	{
		return std::make_unique<windows_api::DirectShow>();
	}

	std::vector<std::pair<std::string, std::string>> VideoDevice::GetDeviceList()
	{
		return windows_api::getDevList();
	}
} // namespace usb_eyetoy

#ifdef __clang__
#pragma clang diagnostic pop
#endif
