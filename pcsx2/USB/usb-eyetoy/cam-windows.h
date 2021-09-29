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

#include "videodev.h"
#include <mutex>

#pragma comment(lib, "strmiids")

#include <windows.h>
#include <dshow.h>

#include <wil/com.h>

extern "C" {
extern GUID IID_ISampleGrabberCB;
extern GUID CLSID_SampleGrabber;
extern GUID CLSID_NullRenderer;
}

#pragma region qedit.h
struct __declspec(uuid("0579154a-2b53-4994-b0d0-e773148eff85"))
	ISampleGrabberCB : IUnknown
{
	virtual HRESULT __stdcall SampleCB(double SampleTime, struct IMediaSample* pSample) = 0;
	virtual HRESULT __stdcall BufferCB(double SampleTime, unsigned char* pBuffer, long BufferLen) = 0;
};

struct __declspec(uuid("6b652fff-11fe-4fce-92ad-0266b5d7c78f"))
	ISampleGrabber : IUnknown
{
	virtual HRESULT __stdcall SetOneShot(long OneShot) = 0;
	virtual HRESULT __stdcall SetMediaType(struct _AMMediaType* pType) = 0;
	virtual HRESULT __stdcall GetConnectedMediaType(struct _AMMediaType* pType) = 0;
	virtual HRESULT __stdcall SetBufferSamples(long BufferThem) = 0;
	virtual HRESULT __stdcall GetCurrentBuffer(long* pBufferSize, long* pBuffer) = 0;
	virtual HRESULT __stdcall GetCurrentSample(struct IMediaSample** ppSample) = 0;
	virtual HRESULT __stdcall SetCallback(struct ISampleGrabberCB* pCallback, long WhichMethodToCallback) = 0;
};

struct __declspec(uuid("c1f400a0-3f08-11d3-9f0b-006008039e37"))
	SampleGrabber;

#pragma endregion


#ifndef MAXLONGLONG
#define MAXLONGLONG 0x7FFFFFFFFFFFFFFF
#endif

#ifndef MAX_DEVICE_NAME
#define MAX_DEVICE_NAME 80
#endif

#ifndef BITS_PER_PIXEL
#define BITS_PER_PIXEL 24
#endif

namespace usb_eyetoy
{
	namespace windows_api
	{

		typedef void (*DShowVideoCaptureCallback)(unsigned char* data, int len, int bitsperpixel);

		struct buffer_t
		{
			void* start = NULL;
			size_t length = 0;
		};

		static const char* APINAME = "DirectShow";

		class DirectShow : public VideoDevice
		{
		public:
			DirectShow(int port);
			~DirectShow();
			int Open(int width, int height, FrameFormat format, int mirror);
			int Close();
			int GetImage(uint8_t* buf, size_t len);
			void SetMirroring(bool state);
			int Reset() { return 0; };

			static const TCHAR* Name()
			{
				return TEXT("DirectShow");
			}
			static int Configure(int port, const char* dev_type, void* data);

			int Port() { return mPort; }
			void Port(int port) { mPort = port; }

		protected:
			void SetCallback(DShowVideoCaptureCallback cb) { callbackhandler->SetCallback(cb); }
			void Start();
			void Stop();
			int InitializeDevice(std::wstring selectedDevice);

		private:
			int mPort;

			wil::unique_couninitialize_call dshowCoInitialize;
			ICaptureGraphBuilder2* pGraphBuilder;
			IFilterGraph2* pGraph;
			IMediaControl* pControl;

			IBaseFilter* sourcefilter;
			IAMStreamConfig* pSourceConfig;
			IBaseFilter* samplegrabberfilter;
			ISampleGrabber* samplegrabber;
			IBaseFilter* nullrenderer;

			class CallbackHandler : public ISampleGrabberCB
			{
			public:
				CallbackHandler() { callback = 0; }
				~CallbackHandler() {}

				void SetCallback(DShowVideoCaptureCallback cb) { callback = cb; }

				virtual HRESULT __stdcall SampleCB(double time, IMediaSample* sample);
				virtual HRESULT __stdcall BufferCB(double time, BYTE* buffer, long len) { return S_OK; }
				virtual HRESULT __stdcall QueryInterface(REFIID iid, LPVOID* ppv);
				virtual ULONG __stdcall AddRef() { return 1; }
				virtual ULONG __stdcall Release() { return 2; }

			private:
				DShowVideoCaptureCallback callback;

			} * callbackhandler;
		};

	} // namespace windows_api
} // namespace usb_eyetoy
