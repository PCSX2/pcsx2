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

// Used OBS as example

#include "PrecompiledHeader.h"
#include <assert.h>
#include <propsys.h>
#include <typeinfo>
#include <functiondiscoverykeys_devpkey.h>
#include <process.h>
#include "audiodev-wasapi.h"
#include "USB/Win32/Config_usb.h"
#include "USB/Win32/resource_usb.h"

#define SafeRelease(x) \
	if (x)             \
	{                  \
		x->Release();  \
		x = NULL;      \
	}
#define ConvertMSTo100NanoSec(ms) (ms * 1000 * 10) //1000 microseconds, then 10 "100nanosecond" segments

namespace usb_mic
{
	namespace audiodev_wasapi
	{

		static FILE* file = nullptr;

		//Config dlg temporaries
		struct WASAPISettings
		{
			int port;
			const char* dev_type;
			AudioDeviceInfoList sourceDevs;
			AudioDeviceInfoList sinkDevs;
			std::wstring selectedDev[3];
		};

		static BOOL CALLBACK WASAPIDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);

		LARGE_INTEGER clockFreq = {0};
		__declspec(thread) LONGLONG lastQPCTime = 0;

		LONGLONG GetQPCTimeMS()
		{
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			if (currentTime.QuadPart < lastQPCTime)

			lastQPCTime = currentTime.QuadPart;

			LONGLONG timeVal = currentTime.QuadPart;
			timeVal *= 1000;
			timeVal /= clockFreq.QuadPart;

			return timeVal;
		}

		LONGLONG GetQPCTime100NS()
		{
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			lastQPCTime = currentTime.QuadPart;

			double timeVal = double(currentTime.QuadPart);
			timeVal *= 10000000.0;
			timeVal /= double(clockFreq.QuadPart);

			return LONGLONG(timeVal);
		}

		MMAudioDevice::~MMAudioDevice()
		{
			mQuit = true;
			if (mThread != INVALID_HANDLE_VALUE)
			{
				if (WaitForSingleObject(mThread, 30000) != WAIT_OBJECT_0)
					TerminateThread(mThread, 0);
			}

			FreeData();
			SafeRelease(mmEnumerator);
			mResampler = src_delete(mResampler);
			if (file)
				fclose(file);
			file = nullptr;

			CloseHandle(mThread);
			mThread = INVALID_HANDLE_VALUE;
			CloseHandle(mMutex);
			mMutex = INVALID_HANDLE_VALUE;
		}

		void MMAudioDevice::FreeData()
		{
			SafeRelease(mmCapture);
			SafeRelease(mmRender);
			SafeRelease(mmClient);
			SafeRelease(mmDevice);
			SafeRelease(mmClock);
			//clear mBuffer
		}

		bool MMAudioDevice::Init()
		{
			const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
			const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

			if (mAudioDir == AUDIODIR_SOURCE)
			{
				if (!LoadSetting(mDevType, mPort, APINAME, (mDevice ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0), mDevID))
				{
					throw AudioDeviceError("MMAudioDevice:: failed to load source from ini!");
				}
			}
			else
			{
				if (!LoadSetting(mDevType, mPort, APINAME, (mDevice ? N_AUDIO_SINK1 : N_AUDIO_SINK0), mDevID))
				{
					throw AudioDeviceError("MMAudioDevice:: failed to load sink from ini!");
				}
			}

			if (!mDevID.length())
				return false;

			{
				int var;
				if (LoadSetting(mDevType, mPort, APINAME, (mAudioDir == AUDIODIR_SOURCE ? N_BUFFER_LEN_SRC : N_BUFFER_LEN_SINK), var))
					mBuffering = std::min(1000, std::max(1, var));
			}


			HRESULT err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
			if (FAILED(err))
			{
				Console.WriteLn("MMAudioDevice::Init(): Could not create IMMDeviceEnumerator = %08lX\n", err);
				return false;
			}
			//TODO Not starting thread here unnecesserily
			//mThread = CreateThread(NULL, 0, MMAudioDevice::CaptureThread, this, 0, 0);
			return true;
		}

		bool MMAudioDevice::Reinitialize()
		{
			const IID IID_IAudioClient = __uuidof(IAudioClient);
			const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
			const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
			HRESULT err;

			if (!mDeviceLost && mmClock)
				return true;
			else
			{
				if (GetQPCTimeMS() - mLastTimeMS < 1000)
					return false;
				mLastTimeMS = GetQPCTimeMS();
			}

			err = mmEnumerator->GetDevice(mDevID.c_str(), &mmDevice);

			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not create IMMDevice = %08lX\n", err);
				return false;
			}

			err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not create IAudioClient = %08lX\n", err);
				return false;
			}

			// get name

			/*IPropertyStore *store;
	if(SUCCEEDED(mmDevice->OpenPropertyStore(STGM_READ, &store)))
	{
		PROPVARIANT varName;

		PropVariantInit(&varName);
		if(SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
		{
			const WCHAR* wstrName = varName.pwszVal;
			mDeviceName = wstrName;
		}

		store->Release();
	}*/

			// get format

			WAVEFORMATEX* pwfx;
			err = mmClient->GetMixFormat(&pwfx);
			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not get mix format from audio client = %08lX\n", err);
				return false;
			}

			WAVEFORMATEXTENSIBLE* wfext = NULL;

			if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
			{
				wfext = (WAVEFORMATEXTENSIBLE*)pwfx;
				mInputChannelMask = wfext->dwChannelMask;

				if (wfext->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
				{
					if (!mDeviceLost)
						Console.WriteLn("MMAudioDevice::Reinitialize(): Unsupported wave format\n");
					CoTaskMemFree(pwfx);
					return false;
				}
			}
			else if (pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Unsupported wave format\n");
				CoTaskMemFree(pwfx);
				return false;
			}

			mFloat = true;
			mDeviceChannels = pwfx->nChannels;
			mDeviceBitsPerSample = 32;
			mDeviceBlockSize = pwfx->nBlockAlign;
			mDeviceSamplesPerSec = pwfx->nSamplesPerSec;
			//sampleWindowSize      = (inputSamplesPerSec/100);

			DWORD flags = 0; //useInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

			//Random limit of 1ms to 1 seconds
			if (mBuffering == 0)
				mBuffering = 50;
			mBuffering = std::min(std::max(mBuffering, 1LL), 1000LL);

			err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(mBuffering), 0, pwfx, NULL);
			//err = AUDCLNT_E_UNSUPPORTED_FORMAT;

			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not initialize audio client, result = %08lX\n", err);
				CoTaskMemFree(pwfx);
				return false;
			}

			// acquire services
			if (mAudioDir == AUDIODIR_SOURCE)
				err = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
			else
				err = mmClient->GetService(IID_IAudioRenderClient, (void**)&mmRender);

			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not get audio %s client, result = %08lX\n",
							   (mAudioDir == AUDIODIR_SOURCE ? TEXT("capture") : TEXT("render")), err);
				CoTaskMemFree(pwfx);
				return false;
			}

			err = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);

			if (FAILED(err))
			{
				if (!mDeviceLost)
					Console.WriteLn("MMAudioDevice::Reinitialize(): Could not get audio capture clock, result = %08lX\n", err);
				CoTaskMemFree(pwfx);
				return false;
			}

			CoTaskMemFree(pwfx);

			// Setup resampler
			int converterType = SRC_SINC_FASTEST;
			int errVal = 0;

			mResampler = src_delete(mResampler);
			mResampler = src_new(converterType, mDeviceChannels, &errVal);

			if (!mResampler)
			{
#ifndef _DEBUG
				Console.WriteLn("USB: Failed to create resampler: error %08lX", errVal);
#endif
				return false;
			}

			ResetBuffers();

			if (mDeviceLost && !mFirstSamples) //TODO really lost and just first run. Call Start() from ctor always anyway?
				this->Start();

			mDeviceLost = false;

			return true;
		}

		void MMAudioDevice::Start()
		{
			src_reset(mResampler);
			if (mmClient)
				mmClient->Start();
			mPaused = false;
		}

		void MMAudioDevice::Stop()
		{
			mPaused = true;
			if (mmClient)
				mmClient->Stop();
		}

		void MMAudioDevice::ResetBuffers()
		{
			if (WaitForSingleObject(mMutex, 5000) != WAIT_OBJECT_0)
			{
				return;
			}

			size_t bytes;
			if (mAudioDir == AUDIODIR_SOURCE)
			{
				bytes = mDeviceChannels * mDeviceSamplesPerSec * sizeof(float) * mBuffering / 1000;
				bytes += bytes % (mDeviceChannels * sizeof(float));
				mInBuffer.reserve(bytes);

				bytes = mDeviceChannels * mSamplesPerSec * sizeof(short) * mBuffering / 1000;
				bytes += bytes % (mDeviceChannels * sizeof(short));
				mOutBuffer.reserve(bytes);
			}
			else
			{
				bytes = mDeviceChannels * mDeviceSamplesPerSec * sizeof(float) * mBuffering / 1000;
				bytes += bytes % (mDeviceChannels * sizeof(float));
				mOutBuffer.reserve(bytes);

				bytes = mDeviceChannels * mSamplesPerSec * sizeof(short) * mBuffering / 1000;
				bytes += bytes % (mDeviceChannels * sizeof(short));
				mInBuffer.reserve(bytes);
			}

			ReleaseMutex(mMutex);
		}

		//TODO or just return samples count in mOutBuffer?
		bool MMAudioDevice::GetFrames(uint32_t* size)
		{
			if (WaitForSingleObject(mMutex, 5000) != WAIT_OBJECT_0)
			{
				*size = 0;
				return false;
			}
			*size = mOutBuffer.size<short>() / mDeviceChannels;
			ReleaseMutex(mMutex);
			return true;
		}

		unsigned WINAPI MMAudioDevice::CaptureThread(LPVOID ptr)
		{
			MMAudioDevice* src = (MMAudioDevice*)ptr;
			std::vector<float> rebuf;
			unsigned ret = 1;
			bool bThreadComInitialized = false;

			//TODO APARTMENTTHREADED instead?
			HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if ((S_OK != hr) && (S_FALSE != hr) /* already inited */ && (hr != RPC_E_CHANGED_MODE))
			{
				goto error;
			}

			if (hr != RPC_E_CHANGED_MODE)
				bThreadComInitialized = true;

			//Call mmClient->Start() here instead?

			while (!src->mQuit)
			{
				while (src->mPaused)
				{
					Sleep(100);
					if (src->mQuit)
						goto quit;
				}

				src->GetMMBuffer();
				if (src->mInBuffer.size())
				{
					size_t resampled = static_cast<size_t>(src->mInBuffer.size<float>() * src->mResampleRatio * src->mTimeAdjust);
					if (resampled == 0)
						resampled = src->mInBuffer.size<float>();
					rebuf.resize(resampled);

					SRC_DATA srcData;
					memset(&srcData, 0, sizeof(SRC_DATA));
					size_t output_frames = 0;
					float* pBegin = rebuf.data();
					float* pEnd = pBegin + rebuf.size();

					memset(&srcData, 0, sizeof(SRC_DATA));

					while (src->mInBuffer.peek_read() > 0)
					{
						srcData.data_in = src->mInBuffer.front<float>();
						srcData.input_frames = src->mInBuffer.peek_read<float>() / src->GetChannels();
						srcData.data_out = pBegin;
						srcData.output_frames = (pEnd - pBegin) / src->GetChannels();
						srcData.src_ratio = src->mResampleRatio * src->mTimeAdjust;

						src_process(src->mResampler, &srcData);
						output_frames += srcData.output_frames_gen;
						pBegin += srcData.output_frames_gen * src->GetChannels();

						size_t samples = srcData.input_frames_used * src->GetChannels();
						if (!samples)
							break; //TODO happens?
						src->mInBuffer.read<float>(samples);
					}

					DWORD resMutex = WaitForSingleObject(src->mMutex, 30000);
					if (resMutex != WAIT_OBJECT_0)
					{
						goto error;
					}

					size_t len = output_frames * src->GetChannels();
					float* pSrc = rebuf.data();
					while (len > 0)
					{
						size_t samples = std::min(len, src->mOutBuffer.peek_write<short>(true));
						src_float_to_short_array(pSrc, src->mOutBuffer.back<short>(), samples);
						src->mOutBuffer.write<short>(samples);
						len -= samples;
						pSrc += samples;
					}

					if (!ReleaseMutex(src->mMutex))
					{
						goto error;
					}
				}
				Sleep(src->mDeviceLost ? 1000 : 1);
			}

		quit:
			ret = 0;
		error:
			if (bThreadComInitialized)
				CoUninitialize();

			_endthreadex(ret);
			return ret;
		}

		unsigned WINAPI MMAudioDevice::RenderThread(LPVOID ptr)
		{
			MMAudioDevice* src = (MMAudioDevice*)ptr;
			std::vector<float> buffer;
			UINT32 bufferFrameCount, numFramesPadding, numFramesAvailable;
			BYTE* pData;
			SRC_DATA srcData;
			unsigned ret = 1;
			HRESULT hr = 0;
			bool bThreadComInitialized = false;

			memset(&srcData, 0, sizeof(SRC_DATA));

			//TODO APARTMENTTHREADED instead?
			hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if ((S_OK != hr) && (S_FALSE != hr) /* already inited */ && (hr != RPC_E_CHANGED_MODE))
			{
				goto error;
			}

			if (hr != RPC_E_CHANGED_MODE)
				bThreadComInitialized = true;

			//Call mmClient->Start() here instead?

			while (!src->mQuit)
			{
				while (src->mPaused)
				{
					Sleep(100);
					if (src->mQuit)
						goto quit;
				}

				if (src->mDeviceLost && !src->Reinitialize())
				{
					Sleep(1000);
					continue;
				}

				DWORD resMutex = WaitForSingleObject(src->mMutex, 5000);
				if (resMutex != WAIT_OBJECT_0)
				{
					goto error;
				}

				hr = src->mmClient->GetBufferSize(&bufferFrameCount);
				if (FAILED(hr))
					goto device_error;

				hr = src->mmClient->GetCurrentPadding(&numFramesPadding);
				if (FAILED(hr))
					goto device_error;

				numFramesAvailable = std::min(bufferFrameCount - numFramesPadding, UINT32(src->mInBuffer.size<short>() / src->GetChannels()));

				if (src->mInBuffer.size<short>())
				{
					buffer.resize(src->mInBuffer.size<short>());
					size_t read = buffer.size();

					while (read > 0 && numFramesAvailable > 0)
					{
						size_t samples = std::min(src->mInBuffer.peek_read<short>(), read);
						src_short_to_float_array(src->mInBuffer.front<short>(),
												 buffer.data(), samples);

						//XXX May get AUDCLNT_E_BUFFER_TOO_LARGE
						hr = src->mmRender->GetBuffer(numFramesAvailable, &pData);
						if (FAILED(hr))
							goto device_error;

						srcData.data_in = buffer.data();
						srcData.input_frames = samples / src->GetChannels();
						srcData.data_out = (float*)pData;
						srcData.output_frames = numFramesAvailable;
						srcData.src_ratio = src->mResampleRatio;

						src_process(src->mResampler, &srcData);

						hr = src->mmRender->ReleaseBuffer(srcData.output_frames_gen, 0);
						if (FAILED(hr))
							goto device_error;

						read -= srcData.input_frames_used * src->GetChannels();
						src->mInBuffer.read<short>(srcData.input_frames_used * src->GetChannels());
					}
				}
				// TODO WASAPI seems to stop playing when buffer underrun, so skip this
				/*else
		{
			if (numFramesPadding < src->mDeviceSamplesPerSec / 1000)
			{
				numFramesAvailable = std::min(bufferFrameCount - numFramesPadding, (src->mDeviceSamplesPerSec / 1000));

				hr = src->mmRender->GetBuffer(numFramesAvailable, &pData);
				if (FAILED(hr))
					goto device_error;
				hr = src->mmRender->ReleaseBuffer(numFramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
			}
		}*/

			device_error:
				if (!ReleaseMutex(src->mMutex))
				{
					goto error;
				}

				if (FAILED(hr))
				{
					if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
					{
						src->mDeviceLost = true;
					}
					else
						goto error;
				}

				Sleep(src->mDeviceLost ? 1000 : 10);
			}

		quit:
			ret = 0;
		error:
			if (bThreadComInitialized)
				CoUninitialize();

			_endthreadex(ret);
			return ret;
		}

		uint32_t MMAudioDevice::GetBuffer(int16_t* outBuf, uint32_t outFrames)
		{
			if (!mQuit && (mThread == INVALID_HANDLE_VALUE ||
						   WaitForSingleObject(mThread, 0) == WAIT_OBJECT_0)) //Thread got killed prematurely
			{
				mThread = (HANDLE)_beginthreadex(NULL, 0, MMAudioDevice::CaptureThread, this, 0, NULL);
			}

			DWORD resMutex = WaitForSingleObject(mMutex, 1000);
			if (resMutex != WAIT_OBJECT_0)
			{
				return 0;
			}

			//mSamples += outFrames;
			//mTime = GetQPCTime100NS();
			//if (mLastTimeNS == 0) mLastTimeNS = mTime;
			//LONGLONG diff = mTime - mLastTimeNS;
			//if (diff >= LONGLONG(1e7))
			//{
			//	mTimeAdjust = (mSamples / (diff / 1e7)) / mSamplesPerSec;
			//	//if(mTimeAdjust > 1.0) mTimeAdjust = 1.0; //If game is in 'turbo mode', just return zero samples or...?
			//	mLastTimeNS = mTime;
			//	mSamples = 0;
			//}

			int samples_to_read = outFrames * mDeviceChannels;
			short* pDst = (short*)outBuf;
			//assert(samples_to_read <= mOutBuffer.size<short>());

			while (samples_to_read > 0)
			{
				int samples = std::min(samples_to_read, (int)mOutBuffer.peek_read<short>());
				if (!samples)
					break;
				memcpy(pDst, mOutBuffer.front(), samples * sizeof(short));

				mOutBuffer.read<short>(samples);
				pDst += samples;
				samples_to_read -= samples;
			}

			resMutex = ReleaseMutex(mMutex);
			return (outFrames - (samples_to_read / mDeviceChannels));
		}

		uint32_t MMAudioDevice::SetBuffer(int16_t* inBuf, uint32_t inFrames)
		{
			if (!mQuit && (mThread == INVALID_HANDLE_VALUE ||
						   WaitForSingleObject(mThread, 0) == WAIT_OBJECT_0)) //Thread got killed prematurely
			{
				mThread = (HANDLE)_beginthreadex(NULL, 0, MMAudioDevice::RenderThread, this, 0, NULL);
			}

			DWORD resMutex = WaitForSingleObject(mMutex, 1000);
			if (resMutex != WAIT_OBJECT_0)
			{
				return 0;
			}

			size_t nbytes = inFrames * sizeof(short) * GetChannels();
			mInBuffer.write((uint8_t*)inBuf, nbytes);

			if (!ReleaseMutex(mMutex))
			{
			}

			return inFrames;
		}

		/*
	Returns read frame count.
*/
		uint32_t MMAudioDevice::GetMMBuffer()
		{
			UINT64 devPosition, qpcTimestamp;
			LPBYTE captureBuffer;
			UINT32 numFramesRead;
			DWORD dwFlags = 0;

			if (mDeviceLost)
			{
				FreeData();
				if (Reinitialize())
				{
					Start();
				}
				else
				{
					return 0;
				}
			}

			UINT32 captureSize = 0;
			HRESULT hRes = mmCapture->GetNextPacketSize(&captureSize);

			if (FAILED(hRes))
			{
				if (hRes == AUDCLNT_E_DEVICE_INVALIDATED)
				{
					mDeviceLost = true;
					FreeData();
				}
				return 0;
			}

			if (!captureSize)
				return 0;

			if (SUCCEEDED(mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp)))
			{
				size_t totalLen = numFramesRead * mDeviceChannels;
				if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					while (totalLen && mInBuffer.peek_write<float>() > 0)
					{
						size_t len = std::min(totalLen, mInBuffer.peek_write<float>());
						memset(mInBuffer.back(), 0, sizeof(float) * len);
						mInBuffer.write<float>(len);
						totalLen -= len;
					}
				}
				else
				{
					mInBuffer.write((uint8_t*)captureBuffer, sizeof(float) * totalLen);
				}

				mmCapture->ReleaseBuffer(numFramesRead);
			}

			return numFramesRead;
		}

		void MMAudioDevice::SetResampling(int samplerate)
		{
			if (mDeviceSamplesPerSec == samplerate)
			{
				mResample = false;
				return;
			}
			mSamplesPerSec = samplerate;
			if (mAudioDir == AUDIODIR_SOURCE)
				mResampleRatio = double(samplerate) / double(mDeviceSamplesPerSec);
			else
				mResampleRatio = double(mDeviceSamplesPerSec) / double(samplerate);
			mResample = true;
			ResetBuffers();
		}

		bool MMAudioDevice::AudioInit()
		{
			QueryPerformanceFrequency(&clockFreq);
			return true;
		}

		void MMAudioDevice::AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir dir)
		{
			const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
			const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
			IMMDeviceEnumerator* mmEnumerator;
			HRESULT err;

			err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
			if (FAILED(err))
			{
				Console.WriteLn("AudioDevices: Could not create IMMDeviceEnumerator\n");
				return;
			}

			IMMDeviceCollection* collection;
			EDataFlow audioDeviceType = (dir == AUDIODIR_SOURCE ? eCapture : eRender);
			DWORD flags = DEVICE_STATE_ACTIVE;
			//if (!bConnectedOnly)
			flags |= DEVICE_STATE_UNPLUGGED;

			err = mmEnumerator->EnumAudioEndpoints(audioDeviceType, flags, &collection);
			if (FAILED(err))
			{
				Console.WriteLn("AudioDevices: Could not enumerate audio endpoints\n");
				SafeRelease(mmEnumerator);
				return;
			}

			UINT count;
			if (SUCCEEDED(collection->GetCount(&count)))
			{
				for (UINT i = 0; i < count; i++)
				{
					IMMDevice* device;
					if (SUCCEEDED(collection->Item(i, &device)))
					{
						const WCHAR* wstrID;
						if (SUCCEEDED(device->GetId((LPWSTR*)&wstrID)))
						{
							IPropertyStore* store;
							if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)))
							{
								PROPVARIANT varName;

								PropVariantInit(&varName);
								if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName)))
								{
									const WCHAR* wstrName = varName.pwszVal;

									AudioDeviceInfo info;
									info.strID = wstrID;
									info.strName = wstrName;
									devices.push_back(info);
								}
							}

							CoTaskMemFree((LPVOID)wstrID);
						}

						SafeRelease(device);
					}
				}
			}

			SafeRelease(collection);
			SafeRelease(mmEnumerator);
		}

		int MMAudioDevice::Configure(int port, const char* dev_type, void* data)
		{
			Win32Handles h = *(Win32Handles*)data;
			WASAPISettings settings;
			settings.port = port;
			settings.dev_type = dev_type;

			return (int)DialogBoxParam(h.hInst,
									   MAKEINTRESOURCE(IDD_DLGWASAPI_USB),
									   h.hWnd,
									   (DLGPROC)WASAPIDlgProc, (LPARAM)&settings);
		}

		static void RefreshInputAudioList(HWND hW, LRESULT idx, WASAPISettings* settings)
		{
			settings->sourceDevs.clear();

			SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_RESETCONTENT, 0, 0);

			SendDlgItemMessageW(hW, IDC_COMBO1_USB, CB_ADDSTRING, 0, (LPARAM)L"None");
			SendDlgItemMessageW(hW, IDC_COMBO2_USB, CB_ADDSTRING, 0, (LPARAM)L"None");

			SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_SETCURSEL, 0, 0);
			SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_SETCURSEL, 0, 0);

			MMAudioDevice::AudioDevices(settings->sourceDevs, AUDIODIR_SOURCE);
			AudioDeviceInfoList::iterator it;
			int i = 0;
			for (it = settings->sourceDevs.begin(); it != settings->sourceDevs.end(); ++it)
			{
				SendDlgItemMessageW(hW, IDC_COMBO1_USB, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());
				SendDlgItemMessageW(hW, IDC_COMBO2_USB, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

				i++;
				if (it->strID == settings->selectedDev[0])
					SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_SETCURSEL, i, i);
				if (it->strID == settings->selectedDev[1])
					SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_SETCURSEL, i, i);
			}
		}

		static void RefreshOutputAudioList(HWND hW, LRESULT idx, WASAPISettings* settings)
		{
			settings->sinkDevs.clear();

			SendDlgItemMessage(hW, IDC_COMBO3_USB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessageW(hW, IDC_COMBO3_USB, CB_ADDSTRING, 0, (LPARAM)L"None");
			SendDlgItemMessage(hW, IDC_COMBO3_USB, CB_SETCURSEL, 0, 0);

			MMAudioDevice::AudioDevices(settings->sinkDevs, AUDIODIR_SINK);
			AudioDeviceInfoList::iterator it;
			int i = 0;
			for (it = settings->sinkDevs.begin(); it != settings->sinkDevs.end(); ++it)
			{
				SendDlgItemMessageW(hW, IDC_COMBO3_USB, CB_ADDSTRING, 0, (LPARAM)it->strName.c_str());

				i++;
				if (it->strID == settings->selectedDev[2])
					SendDlgItemMessage(hW, IDC_COMBO3_USB, CB_SETCURSEL, i, i);
			}
		}

		static BOOL CALLBACK WASAPIDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			int tmp = 0;
			WASAPISettings* s;

			switch (uMsg)
			{
				case WM_CREATE:
					SetWindowLongPtr(hW, GWLP_USERDATA, lParam);
					break;
				case WM_INITDIALOG:
				{
					s = (WASAPISettings*)lParam;
					SetWindowLongPtr(hW, GWLP_USERDATA, lParam);
					int buffering = 50;
					LoadSetting(s->dev_type, s->port, APINAME, N_BUFFER_LEN_SRC, buffering);

					SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_SETRANGEMIN, TRUE, 1);
					SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_SETRANGEMAX, TRUE, 1000);
					SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_SETPOS, TRUE, buffering);
					SetDlgItemInt(hW, IDC_BUFFER1_USB, buffering, FALSE);

					buffering = 50;
					LoadSetting(s->dev_type, s->port, APINAME, N_BUFFER_LEN_SINK, buffering);

					SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_SETRANGEMIN, TRUE, 1);
					SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_SETRANGEMAX, TRUE, 1000);
					SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_SETPOS, TRUE, buffering);
					SetDlgItemInt(hW, IDC_BUFFER2_USB, buffering, FALSE);

					for (int i = 0; i < 2; i++)
					{
						LoadSetting(s->dev_type, s->port, APINAME, (i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0), s->selectedDev[i]);
					}

					LoadSetting(s->dev_type, s->port, APINAME, N_AUDIO_SINK0, s->selectedDev[2]);

					RefreshInputAudioList(hW, -1, s);
					RefreshOutputAudioList(hW, -1, s);
					return TRUE;
				}
				case WM_HSCROLL:
					if ((HWND)lParam == GetDlgItem(hW, IDC_SLIDER1_USB))
					{
						int pos = SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_GETPOS, 0, 0);
						SetDlgItemInt(hW, IDC_BUFFER1_USB, pos, FALSE);
						break;
					}
					else if ((HWND)lParam == GetDlgItem(hW, IDC_SLIDER2_USB))
					{
						int pos = SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_GETPOS, 0, 0);
						SetDlgItemInt(hW, IDC_BUFFER2_USB, pos, FALSE);
						break;
					}
					break;

				case WM_COMMAND:
					switch (HIWORD(wParam))
					{
						case EN_CHANGE:
						{
							switch (LOWORD(wParam))
							{
								case IDC_BUFFER1_USB:
									CHECKED_SET_MAX_INT(tmp, hW, IDC_BUFFER1_USB, FALSE, 1, 1000);
									SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_SETPOS, TRUE, tmp);
									break;
								case IDC_BUFFER2_USB:
									CHECKED_SET_MAX_INT(tmp, hW, IDC_BUFFER2_USB, FALSE, 1, 1000);
									SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_SETPOS, TRUE, tmp);
									break;
							}
						}
						break;
						case BN_CLICKED:
						{
							switch (LOWORD(wParam))
							{
								case IDOK:
								{
									int p[3];
									s = (WASAPISettings*)GetWindowLongPtr(hW, GWLP_USERDATA);
									INT_PTR res = RESULT_OK;
									p[0] = SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_GETCURSEL, 0, 0);
									p[1] = SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
									p[2] = SendDlgItemMessage(hW, IDC_COMBO3_USB, CB_GETCURSEL, 0, 0);

									for (int i = 0; i < 3; i++)
									{
										s->selectedDev[i].clear();

										if (p[i] > 0)
											s->selectedDev[i] = ((i < 2 ? s->sourceDevs.begin() : s->sinkDevs.begin()) + p[i] - 1)->strID;
									}

									const wchar_t* n[] = {N_AUDIO_SOURCE0, N_AUDIO_SOURCE1, N_AUDIO_SINK0};
									for (int i = 0; i < 3; i++)
									{
										if (!SaveSetting(s->dev_type, s->port, APINAME, n[i], s->selectedDev[i]))
											res = RESULT_FAILED;
									}

									if (!SaveSetting(s->dev_type, s->port, APINAME, N_BUFFER_LEN_SRC, (int32_t)SendDlgItemMessage(hW, IDC_SLIDER1_USB, TBM_GETPOS, 0, 0)))
										res = RESULT_FAILED;

									if (!SaveSetting(s->dev_type, s->port, APINAME, N_BUFFER_LEN_SINK, (int32_t)SendDlgItemMessage(hW, IDC_SLIDER2_USB, TBM_GETPOS, 0, 0)))
										res = RESULT_FAILED;

									EndDialog(hW, res);
									return TRUE;
								}
								case IDCANCEL:
									EndDialog(hW, RESULT_CANCELED);
									return TRUE;
							}
						}
						break;
					}
			}
			return FALSE;
		}

	} // namespace audiodev_wasapi
} // namespace usb_mic
