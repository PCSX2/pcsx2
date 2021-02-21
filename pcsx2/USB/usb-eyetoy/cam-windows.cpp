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

#include "PrecompiledHeader.h"
#include "videodev.h"
#include "cam-windows.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"

#include "../Win32/Config_usb.h"
#include "../Win32/resource_usb.h"

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

		std::vector<std::wstring> getDevList()
		{
			std::vector<std::wstring> devList;

			ICreateDevEnum* pCreateDevEnum = nullptr;
			HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
			if (FAILED(hr))
			{
				Console.Warning("Error Creating Device Enumerator");
				return devList;
			}

			IEnumMoniker* pEnum = nullptr;
			hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			if (hr != S_OK)
			{
				Console.Warning("You have no video capture hardware");
				return devList;
			};

			IMoniker* pMoniker = nullptr;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
			{
				IPropertyBag* pPropBag = nullptr;
				hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
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
				Console.Warning("CoCreateInstance CLSID_CaptureGraphBuilder2 err : %x", hr);
				return -1;
			}

			// Create the Filter Graph Manager.
			hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
			if (FAILED(hr))
			{
				Console.Warning("CoCreateInstance CLSID_FilterGraph err : %x", hr);
				return -1;
			}

			hr = pGraphBuilder->SetFiltergraph(pGraph);
			if (FAILED(hr))
			{
				Console.Warning("SetFiltergraph err : %x", hr);
				return -1;
			}

			hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
			if (FAILED(hr))
			{
				Console.Warning("QueryInterface IID_IMediaControl err : %x", hr);
				return -1;
			}

			// enumerate all video capture devices
			ICreateDevEnum* pCreateDevEnum = nullptr;
			hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pCreateDevEnum));
			if (FAILED(hr))
			{
				Console.Warning("Error Creating Device Enumerator");
				return -1;
			}

			IEnumMoniker* pEnum = nullptr;
			hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			if (hr != S_OK)
			{
				Console.Warning("You have no video capture hardware");
				return -1;
			};

			pEnum->Reset();

			IMoniker* pMoniker = nullptr;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK && sourcefilter == nullptr)
			{
				IPropertyBag* pPropBag = nullptr;
				hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
				if (FAILED(hr))
				{
					Console.Warning("BindToStorage err : %x", hr);
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
					Console.Warning("Read name err : %x", hr);
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
					Console.Warning("AddSourceFilterForMoniker err : %x", hr);
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
							Console.Warning("GetStreamCaps min=%dx%d max=%dx%d, fmt=%x",
									scc.MinOutputSize.cx, scc.MinOutputSize.cy,
									scc.MaxOutputSize.cx, scc.MaxOutputSize.cy,
									pmtConfig->subtype);

							if (SUCCEEDED(hr))
							{
								if ((pmtConfig->majortype == MEDIATYPE_Video) &&
									(pmtConfig->formattype == FORMAT_VideoInfo) &&
									(pmtConfig->cbFormat >= sizeof(VIDEOINFOHEADER)) &&
									(pmtConfig->pbFormat != nullptr))
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
					Console.Warning("CoCreateInstance CLSID_SampleGrabber err : %x", hr);
					goto freeVar;
				}

				hr = pGraph->AddFilter(samplegrabberfilter, L"samplegrabberfilter");
				if (FAILED(hr))
				{
					Console.Warning("AddFilter samplegrabberfilter err : %x", hr);
					goto freeVar;
				}

				//set mediatype on the samplegrabber
				hr = samplegrabberfilter->QueryInterface(IID_PPV_ARGS(&samplegrabber));
				if (FAILED(hr))
				{
					Console.Warning("QueryInterface err : %x", hr);
					goto freeVar;
				}

				AM_MEDIA_TYPE mt;
				ZeroMemory(&mt, sizeof(mt));
				mt.majortype = MEDIATYPE_Video;
				mt.subtype = MEDIASUBTYPE_RGB24;
				hr = samplegrabber->SetMediaType(&mt);
				if (FAILED(hr))
				{
					Console.Warning("SetMediaType err : %x", hr);
					goto freeVar;
				}

				//add the callback to the samplegrabber
				hr = samplegrabber->SetCallback(callbackhandler, 0);
				if (hr != S_OK)
				{
					Console.Warning("SetCallback err : %x", hr);
					goto freeVar;
				}

				//set the null renderer
				hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nullrenderer));
				if (FAILED(hr))
				{
					Console.Warning("CoCreateInstance CLSID_NullRenderer err : %x", hr);
					goto freeVar;
				}

				hr = pGraph->AddFilter(nullrenderer, L"nullrenderer");
				if (FAILED(hr))
				{
					Console.Warning("AddFilter nullrenderer err : %x", hr);
					goto freeVar;
				}

				//set the render path
				hr = pGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, sourcefilter, samplegrabberfilter, nullrenderer);
				if (FAILED(hr))
				{
					Console.Warning("RenderStream err : %x", hr);
					goto freeVar;
				}

				// if the stream is started, start capturing immediatly
				LONGLONG start = 0, stop = MAXLONGLONG;
				hr = pGraphBuilder->ControlStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, sourcefilter, &start, &stop, 1, 2);
				if (FAILED(hr))
				{
					Console.Warning("ControlStream err : %x", hr);
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

		buffer_t mpeg_buffer{};
		std::mutex mpeg_mutex;
		bool mirroring_enabled = true;

		void store_mpeg_frame(unsigned char* data, unsigned int len)
		{
			mpeg_mutex.lock();
			memcpy(mpeg_buffer.start, data, len);
			mpeg_buffer.length = len;
			mpeg_mutex.unlock();
		}

		void dshow_callback(unsigned char* data, int len, int bitsperpixel)
		{
			if (bitsperpixel == 24)
			{
				unsigned char* mpegData = (unsigned char*)calloc(1, 320 * 240 * 2);
				int mpegLen = jo_write_mpeg(mpegData, data, 320, 240, JO_BGR24, mirroring_enabled ? JO_FLIP_X : JO_NONE, JO_FLIP_Y);
				store_mpeg_frame(mpegData, mpegLen);
				free(mpegData);
			}
			else
			{
				Console.Warning("dshow_callback: unk format: len=%d bpp=%d", len, bitsperpixel);
			}
		}

		void create_dummy_frame()
		{
			const int width = 320;
			const int height = 240;
			const int bytesPerPixel = 3;

			unsigned char* rgbData = (unsigned char*)calloc(1, width * height * bytesPerPixel);
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					unsigned char* ptr = rgbData + (y * width + x) * bytesPerPixel;
					ptr[0] = 255 - y;
					ptr[1] = y;
					ptr[2] = 255 - y;
				}
			}
			unsigned char* mpegData = (unsigned char*)calloc(1, width * height * bytesPerPixel);
			int mpegLen = jo_write_mpeg(mpegData, rgbData, width, height, JO_RGB24, JO_NONE, JO_NONE);
			free(rgbData);

			store_mpeg_frame(mpegData, mpegLen);
			free(mpegData);
		}

		DirectShow::DirectShow(int port)
		{
			mPort = port;
			pGraphBuilder = nullptr;
			pGraph = nullptr;
			pControl = nullptr;
			sourcefilter = nullptr;
			samplegrabberfilter = nullptr;
			nullrenderer = nullptr;
			pSourceConfig = nullptr;
			samplegrabber = nullptr;
			callbackhandler = new CallbackHandler();
			CoInitialize(NULL);
		}

		int DirectShow::Open()
		{
			mpeg_buffer.start = calloc(1, 320 * 240 * 2);
			create_dummy_frame();

			std::wstring selectedDevice;
			LoadSetting(EyeToyWebCamDevice::TypeName(), Port(), APINAME, N_DEVICE, selectedDevice);

			int ret = InitializeDevice(selectedDevice);
			if (ret < 0)
			{
				Console.Warning("Camera: cannot find '%ls'", selectedDevice.c_str());
				return -1;
			}

			pControl->Run();
			this->Stop();
			this->SetCallback(dshow_callback);
			this->Start();

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

			free(mpeg_buffer.start);
			mpeg_buffer.start = nullptr;
			return 0;
		};

		int DirectShow::GetImage(uint8_t* buf, int len)
		{
			mpeg_mutex.lock();
			int len2 = mpeg_buffer.length;
			if (len < mpeg_buffer.length)
				len2 = len;
			memcpy(buf, mpeg_buffer.start, len2);
			mpeg_mutex.unlock();
			return len2;
		};

		void DirectShow::SetMirroring(bool state)
		{
			mirroring_enabled = state;
		}

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
					SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_RESETCONTENT, 0, 0);

					std::vector<std::wstring> devList = getDevList();
					for (auto i = 0; i != devList.size(); i++)
					{
						SendDlgItemMessageW(hW, IDC_COMBO1_USB, CB_ADDSTRING, 0, (LPARAM)devList[i].c_str());
						if (selectedDevice == devList.at(i))
						{
							SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_SETCURSEL, i, i);
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
								GetWindowTextW(GetDlgItem(hW, IDC_COMBO1_USB), selectedDevice, countof(selectedDevice));
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
								  MAKEINTRESOURCE(IDD_DLG_EYETOY_USB),
								  handles.hWnd,
								  (DLGPROC)DirectShowDlgProc, port);
		};

	} // namespace windows_api
} // namespace usb_eyetoy
