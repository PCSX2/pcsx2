/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include <guiddef.h>
#include "videodev.h"
#include "cam-windows.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"

#include "../Win32/Config.h"
#include "../Win32/resource.h"

#ifndef DIBSIZE
#define WIDTHBYTES(BTIS) ((DWORD)(((BTIS) + 31) & (~31)) / 8)
#define DIBWIDTHBYTES(BI) (DWORD)(BI).biBitCount) * (DWORD)WIDTHBYTES((DWORD)(BI).biWidth
#define _DIBSIZE(BI) (DIBWIDTHBYTES(BI) * (DWORD)(BI).biHeight)
#define DIBSIZE(BI) ((BI).biHeight < 0 ? (-1) * (_DIBSIZE(BI)) : _DIBSIZE(BI))
#endif

constexpr GUID make_guid(const char* const spec)
{
#define nybble_from_hex(c) ((c >= '0' && c <= '9') ? (c - '0') : ((c >= 'a' && c <= 'f') ? (c - 'a' + 10) : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0)))
#define byte_from_hex(c1, c2) ((nybble_from_hex(c1) << 4) | nybble_from_hex(c2))

	return {
		// Data1
		(((((((((((((
						static_cast<unsigned __int32>(nybble_from_hex(spec[0]))
						<< 4) |
					nybble_from_hex(spec[1]))
				   << 4) |
				  nybble_from_hex(spec[2]))
				 << 4) |
				nybble_from_hex(spec[3]))
			   << 4) |
			  nybble_from_hex(spec[4]))
			 << 4) |
			nybble_from_hex(spec[5]))
		   << 4) |
		  nybble_from_hex(spec[6]))
		 << 4) |
			nybble_from_hex(spec[7]),
		// Data2
		static_cast<unsigned short>(
			(((((
					static_cast<unsigned>(nybble_from_hex(spec[9]))
					<< 4) |
				nybble_from_hex(spec[10]))
			   << 4) |
			  nybble_from_hex(spec[11]))
			 << 4) |
			nybble_from_hex(spec[12])),
		// Data 3
		static_cast<unsigned short>(
			(((((
					static_cast<unsigned>(nybble_from_hex(spec[14]))
					<< 4) |
				nybble_from_hex(spec[15]))
			   << 4) |
			  nybble_from_hex(spec[16]))
			 << 4) |
			nybble_from_hex(spec[17])),
		// Data 4
		{
			static_cast<unsigned char>(byte_from_hex(spec[19], spec[20])),
			static_cast<unsigned char>(byte_from_hex(spec[21], spec[22])),
			static_cast<unsigned char>(byte_from_hex(spec[24], spec[25])),
			static_cast<unsigned char>(byte_from_hex(spec[26], spec[27])),
			static_cast<unsigned char>(byte_from_hex(spec[28], spec[29])),
			static_cast<unsigned char>(byte_from_hex(spec[30], spec[31])),
			static_cast<unsigned char>(byte_from_hex(spec[32], spec[33])),
			static_cast<unsigned char>(byte_from_hex(spec[34], spec[35]))}};
}

#if defined(_MSC_VER)
#define CPPX_MSVC_UUID_FOR(name, spec) \
	class __declspec(uuid(spec)) name
#else
#define CPPX_GNUC_UUID_FOR(name, spec)                    \
	template <>                                           \
	inline auto __mingw_uuidof<name>()                    \
		->GUID const&                                     \
	{                                                     \
		static constexpr GUID the_uuid = make_guid(spec); \
                                                          \
		return the_uuid;                                  \
	}                                                     \
                                                          \
	template <>                                           \
	inline auto __mingw_uuidof<name*>()                   \
		->GUID const&                                     \
	{                                                     \
		return __mingw_uuidof<name>();                    \
	}                                                     \
                                                          \
	static_assert(true, "")
#endif

#if !defined(CPPX_UUID_FOR)
#if defined(_MSC_VER)
#define CPPX_UUID_FOR CPPX_MSVC_UUID_FOR
#elif defined(__GNUC__)
#define CPPX_UUID_FOR CPPX_GNUC_UUID_FOR
#endif
#endif

CPPX_UUID_FOR(ISampleGrabber, "6b652fff-11fe-4fce-92ad-0266b5d7c78f");
CPPX_UUID_FOR(ISampleGrabberCB, "0579154a-2b53-4994-b0d0-e773148eff85");
//CPPX_UUID_FOR(SampleGrabber, "c1f400a0-3f08-11d3-9f0b-006008039e37");

namespace usb_eyetoy
{
	namespace windows_api
	{

		HRESULT DirectShow::CallbackHandler::SampleCB(double time, IMediaSample* sample)
		{
			HRESULT hr;
			unsigned char* buffer;

			hr = sample->GetPointer((BYTE**)&buffer);
			if (hr != S_OK)
				return S_OK;

			if (parent)
				std::invoke(&DirectShow::dshow_callback, parent, buffer, sample->GetActualDataLength(), BITS_PER_PIXEL);
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

		std::vector<std::wstring> getDevList()
		{
			std::vector<std::wstring> devList;
			devList.push_back(L"None");

			ICreateDevEnum* pCreateDevEnum = 0;
			HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
			if (FAILED(hr))
			{
				fprintf(stderr, "Error Creating Device Enumerator");
				return devList;
			}

			IEnumMoniker* pEnum = 0;
			hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			if (hr == S_FALSE || FAILED(hr))
			{
				{
					fprintf(stderr, "You have no video capture hardware");
					return devList;
				};

				IMoniker* pMoniker = NULL;
				while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
				{
					IPropertyBag* pPropBag;
					HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
					if (FAILED(hr))
					{
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
						devList.push_back(var.bstrVal);
						VariantClear(&var);
					}

					pPropBag->Release();
					pMoniker->Release();
				}

				pEnum->Release();
				CoUninitialize();

				return devList;
			}

			int DirectShow::InitializeDevice(std::wstring selectedDevice)
			{

				// Create the Capture Graph Builder.
				HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraphBuilder));
				if (FAILED(hr))
				{
					fprintf(stderr, "CoCreateInstance CLSID_CaptureGraphBuilder2 err : %x\n", hr);
					return -1;
				}

				// Create the Filter Graph Manager.
				hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
				if (FAILED(hr))
				{
					fprintf(stderr, "CoCreateInstance CLSID_FilterGraph err : %x\n", hr);
					return -1;
				}

				hr = pGraphBuilder->SetFiltergraph(pGraph);
				if (FAILED(hr))
				{
					fprintf(stderr, "SetFiltergraph err : %x\n", hr);
					return -1;
				}

				hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
				if (FAILED(hr))
				{
					fprintf(stderr, "QueryInterface IID_IMediaControl err : %x\n", hr);
					return -1;
				}

				// enumerate all video capture devices
				ICreateDevEnum* pCreateDevEnum = 0;
				hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
				if (FAILED(hr))
				{
					fprintf(stderr, "Error Creating Device Enumerator");
					return -1;
				}

				IEnumMoniker* pEnum = 0;
				hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
				if (hr == S_FALSE || FAILED(hr))
				{
					{
						fprintf(stderr, "You have no video capture hardware");
						return -1;
					};

					pEnum->Reset();

					IMoniker* pMoniker;
					while (pEnum->Next(1, &pMoniker, NULL) == S_OK && sourcefilter == NULL)
					{
						IPropertyBag* pPropBag = 0;
						hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
						if (FAILED(hr))
						{
							fprintf(stderr, "BindToStorage err : %x\n", hr);
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
							fprintf(stderr, "Read name err : %x\n", hr);
							goto freeVar;
						}
						fprintf(stderr, "Camera: '%ls'\n", var.bstrVal);
						if (!selectedDevice.empty() && selectedDevice != var.bstrVal)
						{
							goto freeVar;
						}

						//add a filter for the device
						hr = pGraph->AddSourceFilterForMoniker(pMoniker, NULL, L"sourcefilter", &sourcefilter);
						if (FAILED(hr))
						{
							fprintf(stderr, "AddSourceFilterForMoniker err : %x\n", hr);
							goto freeVar;
						}

						hr = pGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, IID_IAMStreamConfig, (void**)&pSourceConfig);
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
									fprintf(stderr, "GetStreamCaps min=%dx%d max=%dx%d, fmt=%x\n",
											scc.MinOutputSize.cx, scc.MinOutputSize.cy,
											scc.MaxOutputSize.cx, scc.MaxOutputSize.cy,
											pmtConfig->subtype);

									if (SUCCEEDED(hr))
									{
										if ((pmtConfig->majortype == MEDIATYPE_Video) &&
											(pmtConfig->formattype == FORMAT_VideoInfo) &&
											(pmtConfig->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
											(pmtConfig->pbFormat != NULL))
										{

											VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
											pVih->bmiHeader.biWidth = 320;
											pVih->bmiHeader.biHeight = 240;
											pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);
											hr = pSourceConfig->SetFormat(pmtConfig);
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
							fprintf(stderr, "CoCreateInstance CLSID_SampleGrabber err : %x\n", hr);
							goto freeVar;
						}

						hr = pGraph->AddFilter(samplegrabberfilter, L"samplegrabberfilter");
						if (FAILED(hr))
						{
							fprintf(stderr, "AddFilter samplegrabberfilter err : %x\n", hr);
							goto freeVar;
						}

						//set mediatype on the samplegrabber
						hr = samplegrabberfilter->QueryInterface(IID_PPV_ARGS(&samplegrabber));
						if (FAILED(hr))
						{
							fprintf(stderr, "QueryInterface err : %x\n", hr);
							goto freeVar;
						}

						AM_MEDIA_TYPE mt;
						ZeroMemory(&mt, sizeof(mt));
						mt.majortype = MEDIATYPE_Video;
						mt.subtype = MEDIASUBTYPE_RGB24;
						hr = samplegrabber->SetMediaType(&mt);
						if (FAILED(hr))
						{
							fprintf(stderr, "SetMediaType err : %x\n", hr);
							goto freeVar;
						}

						//add the callback to the samplegrabber
						hr = samplegrabber->SetCallback(callbackhandler, 0);
						if (hr != S_OK)
						{
							fprintf(stderr, "SetCallback err : %x\n", hr);
							goto freeVar;
						}

						//set the null renderer
						hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nullrenderer));
						if (FAILED(hr))
						{
							fprintf(stderr, "CoCreateInstance CLSID_NullRenderer err : %x\n", hr);
							goto freeVar;
						}

						hr = pGraph->AddFilter(nullrenderer, L"nullrenderer");
						if (FAILED(hr))
						{
							fprintf(stderr, "AddFilter nullrenderer err : %x\n", hr);
							goto freeVar;
						}

						//set the render path
						hr = pGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, sourcefilter, samplegrabberfilter, nullrenderer);
						if (FAILED(hr))
						{
							fprintf(stderr, "RenderStream err : %x\n", hr);
							goto freeVar;
						}

						// if the stream is started, start capturing immediatly
						LONGLONG start, stop;
						start = 0;
						stop = MAXLONGLONG;
						hr = pGraphBuilder->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, &start, &stop, 1, 2);
						if (FAILED(hr))
						{
							fprintf(stderr, "ControlStream err : %x\n", hr);
							goto freeVar;
						}

					freeVar:
						VariantClear(&var);
						pPropBag->Release();

					freeMoniker:
						pMoniker->Release();
					}
					pEnum->Release();
					if (sourcefilter == NULL)
					{
						return -1;
					}
					return 0;
				}

				void DirectShow::Start()
				{
					HRESULT hr = nullrenderer->Run(0);
					if (FAILED(hr))
						throw hr;

					hr = samplegrabberfilter->Run(0);
					if (FAILED(hr))
						throw hr;

					hr = sourcefilter->Run(0);
					if (FAILED(hr))
						throw hr;
				}

				void DirectShow::Stop()
				{
					if (!sourcefilter)
						return;
					HRESULT hr = sourcefilter->Stop();
					if (FAILED(hr))
						throw hr;

					hr = samplegrabberfilter->Stop();
					if (FAILED(hr))
						throw hr;

					hr = nullrenderer->Stop();
					if (FAILED(hr))
						throw hr;
				}
				void DirectShow::store_mpeg_frame(const std::vector<unsigned char>& data)
				{
					std::lock_guard<std::mutex> lk(mpeg_mutex);
					mpeg_buffer = data;
				}

				void DirectShow::dshow_callback(unsigned char* data, int len, int bitsperpixel)
				{
					if (bitsperpixel == 24)
					{
						std::vector<unsigned char> mpegData(320 * 240 * 2);
						int mpegLen = jo_write_mpeg(mpegData.data(), data, 320, 240, JO_RGB24, JO_FLIP_X, JO_FLIP_Y);
						//OSDebugOut(_T("MPEG: alloced: %d, got: %d\n"), mpegData.size(), mpegLen);
						mpegData.resize(mpegLen);
						store_mpeg_frame(mpegData);
					}
					else
					{
						fprintf(stderr, "dshow_callback: unk format: len=%d bpp=%d\n", len, bitsperpixel);
					}
				}

				void DirectShow::create_dummy_frame()
				{
					const int width = 320;
					const int height = 240;
					const int bytesPerPixel = 3;

					std::vector<unsigned char> rgbData(width * height * bytesPerPixel, 0);
					for (int y = 0; y < height; y++)
					{
						for (int x = 0; x < width; x++)
						{
							unsigned char* ptr = &rgbData[(y * width + x) * bytesPerPixel];
							int c = (255 * y) / height;
							ptr[0] = 255 - c;
							ptr[1] = c;
							ptr[2] = 255 - c;
						}
					}
					std::vector<unsigned char> mpegData(width * height * bytesPerPixel, 255);
					int mpegLen = jo_write_mpeg(mpegData.data(), rgbData.data(), width, height, JO_RGB24, JO_NONE, JO_NONE);
					mpegData.resize(mpegLen);
					store_mpeg_frame(mpegData);
				}

				DirectShow::DirectShow(int port)
				{
					mPort = port;
					pGraphBuilder = NULL;
					pGraph = NULL;
					pControl = NULL;
					sourcefilter = NULL;
					samplegrabberfilter = NULL;
					nullrenderer = NULL;
					pSourceConfig = NULL;
					samplegrabber = NULL;
					callbackhandler = new CallbackHandler(this);
					CoInitialize(NULL);
				}

				int DirectShow::Open()
				{
					mpeg_buffer.resize(320 * 240 * 2);
					std::fill(mpeg_buffer.begin(), mpeg_buffer.end(), 0);

					create_dummy_frame();

					std::wstring selectedDevice;
					LoadSetting(EyeToyWebCamDevice::TypeName(), Port(), APINAME, N_DEVICE, selectedDevice);

					int ret = InitializeDevice(selectedDevice);
					if (ret < 0)
					{
						fprintf(stderr, "Camera: cannot find '%ls'\n", selectedDevice.c_str());
						return -1;
					}

					pControl->Run();
					this->Stop();
					this->Start();

					return 0;
				};

				int DirectShow::Close()
				{
					if (!sourcefilter)
						return 0;

					this->Stop();
					pControl->Stop();

					sourcefilter->Release();
					pSourceConfig->Release();
					samplegrabberfilter->Release();
					samplegrabber->Release();
					nullrenderer->Release();
					sourcefilter = nullptr;

					pGraphBuilder->Release();
					pGraph->Release();
					pControl->Release();

					std::lock_guard<std::mutex> lck(mpeg_mutex);
					mpeg_buffer.resize(0);
					return 0;
				};

				int DirectShow::GetImage(uint8_t * buf, int len)
				{
					std::lock_guard<std::mutex> lck(mpeg_mutex);
					int len2 = mpeg_buffer.size();
					if (len < mpeg_buffer.size())
						len2 = len;
					memcpy(buf, mpeg_buffer.data(), len2);
					return len2;
				};

				BOOL CALLBACK DirectShowDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
				{
					int port;

					switch (uMsg)
					{
						case WM_CREATE:
							SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);
							break;
						case WM_INITDIALOG:
						{
							port = (int)lParam;
							SetWindowLongPtr(hW, GWLP_USERDATA, (LONG)lParam);

							std::wstring selectedDevice;
							LoadSetting(EyeToyWebCamDevice::TypeName(), port, APINAME, N_DEVICE, selectedDevice);
							SendDlgItemMessage(hW, IDC_COMBO1, CB_RESETCONTENT, 0, 0);

							std::vector<std::wstring> devList = getDevList();
							for (auto i = 0; i != devList.size(); i++)
							{
								SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)devList[i].c_str());
								if (selectedDevice == devList.at(i))
								{
									SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
								}
							}
							return TRUE;
						}
						case WM_COMMAND:
							if (HIWORD(wParam) == BN_CLICKED)
							{
								switch (LOWORD(wParam))
								{
									case IDOK:
									{
										INT_PTR res = RESULT_OK;
										static wchar_t selectedDevice[500] = {0};
										GetWindowTextW(GetDlgItem(hW, IDC_COMBO1), selectedDevice, countof(selectedDevice));
										port = (int)GetWindowLongPtr(hW, GWLP_USERDATA);
										if (!SaveSetting<std::wstring>(EyeToyWebCamDevice::TypeName(), port, APINAME, N_DEVICE, selectedDevice))
										{
											res = RESULT_FAILED;
										}
										EndDialog(hW, res);
										return TRUE;
									}
									case IDCANCEL:
										EndDialog(hW, RESULT_CANCELED);
										return TRUE;
								}
							}
					}
					return FALSE;
				}

				int DirectShow::Configure(int port, const char* dev_type, void* data)
				{
					Win32Handles handles = *(Win32Handles*)data;
					return DialogBoxParam(handles.hInst,
										  MAKEINTRESOURCE(IDD_DLG_EYETOY),
										  handles.hWnd,
										  (DLGPROC)DirectShowDlgProc, port);
				};

			} // namespace windows_api
		}     // namespace usb_eyetoy
