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
#include "../Global.h"
#include "Dialogs.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#include <xaudio2.h>
#include <atlcomcli.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Exception
{
	class XAudio2Error : public std::runtime_error
	{
	private:
		static std::string CreateErrorMessage(const HRESULT result, const std::string& msg)
		{
			std::stringstream ss;
			ss << " (code 0x" << std::hex << result << ")\n\n";
			switch (result)
			{
				case XAUDIO2_E_INVALID_CALL:
					ss << "Invalid call for the XA2 object state.";
					break;
				case XAUDIO2_E_DEVICE_INVALIDATED:
					ss << "Device is unavailable, unplugged, unsupported, or has been consumed by The Nothing.";
					break;
				default:
					ss << "Unknown error code!";
					break;
			}
			return msg + ss.str();
		}

	public:
		explicit XAudio2Error(const HRESULT result, const std::string& msg)
			: std::runtime_error(CreateErrorMessage(result, msg))
		{
		}
	};
} // namespace Exception

static const double SndOutNormalizer = (double)(1UL << (SndOutVolumeShift + 16));

class XAudio2Mod : public SndOutModule
{
private:
	static const int PacketsPerBuffer = 8;
	static const int MAX_BUFFER_COUNT = 3;

	class BaseStreamingVoice : public IXAudio2VoiceCallback
	{
	protected:
		IXAudio2SourceVoice* pSourceVoice;
		std::unique_ptr<s16[]> m_buffer;

		const uint m_nBuffers;
		const uint m_nChannels;
		const uint m_BufferSize;
		const uint m_BufferSizeBytes;

		CRITICAL_SECTION cs;

	public:
		int GetEmptySampleCount()
		{
			XAUDIO2_VOICE_STATE state;
			pSourceVoice->GetState(&state);
			return state.SamplesPlayed & (m_BufferSize - 1);
		}

		virtual ~BaseStreamingVoice()
		{
			DeleteCriticalSection(&cs);
		}

		BaseStreamingVoice(uint numChannels)
			: pSourceVoice(nullptr)
			, m_nBuffers(Config_XAudio2.NumBuffers)
			, m_nChannels(numChannels)
			, m_BufferSize(SndOutPacketSize * m_nChannels * PacketsPerBuffer)
			, m_BufferSizeBytes(m_BufferSize * sizeof(s16))
		{
			InitializeCriticalSection(&cs);
		}

		virtual void Init(IXAudio2* pXAudio2) = 0;

	protected:
		// Several things must be initialized separate of the constructor, due to the fact that
		// virtual calls can't be made from the constructor's context.
		void _init(IXAudio2* pXAudio2, uint chanConfig)
		{
			WAVEFORMATEXTENSIBLE wfx;

			memset(&wfx, 0, sizeof(WAVEFORMATEXTENSIBLE));
			wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wfx.Format.nSamplesPerSec = SampleRate;
			wfx.Format.nChannels = m_nChannels;
			wfx.Format.wBitsPerSample = 16;
			wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
			wfx.Format.nAvgBytesPerSec = SampleRate * wfx.Format.nBlockAlign;
			wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
			wfx.Samples.wValidBitsPerSample = 16;
			wfx.dwChannelMask = chanConfig;
			wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

			HRESULT hr;
			if (FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx,
														XAUDIO2_VOICE_NOSRC, 1.0f, this)))
			{
				throw Exception::XAudio2Error(hr, "XAudio2 CreateSourceVoice failure: ");
			}

			EnterCriticalSection(&cs);

			pSourceVoice->FlushSourceBuffers();
			pSourceVoice->Start(0, 0);

			m_buffer = std::make_unique<s16[]>(m_nBuffers * m_BufferSize);

			// Start some buffers.
			for (uint i = 0; i < m_nBuffers; i++)
			{
				XAUDIO2_BUFFER buf = {0};
				buf.AudioBytes = m_BufferSizeBytes;
				buf.pContext = &m_buffer[i * m_BufferSize];
				buf.pAudioData = (BYTE*)buf.pContext;
				pSourceVoice->SubmitSourceBuffer(&buf);
			}

			LeaveCriticalSection(&cs);
		}

		STDMETHOD_(void, OnVoiceProcessingPassStart)
		() {}
		STDMETHOD_(void, OnVoiceProcessingPassStart)
		(UINT32) {}
		STDMETHOD_(void, OnVoiceProcessingPassEnd)
		() {}
		STDMETHOD_(void, OnStreamEnd)
		() {}
		STDMETHOD_(void, OnBufferStart)
		(void*) {}
		STDMETHOD_(void, OnLoopEnd)
		(void*) {}
		STDMETHOD_(void, OnVoiceError)
		(THIS_ void* pBufferContext, HRESULT Error) {}
	};

	template <typename T>
	class StreamingVoice : public BaseStreamingVoice
	{
	public:
		StreamingVoice()
			: BaseStreamingVoice(sizeof(T) / sizeof(s16))
		{
		}

		virtual ~StreamingVoice()
		{
			IXAudio2SourceVoice* killMe = pSourceVoice;
			// XXX: Potentially leads to a race condition that causes a nullptr
			// dereference when SubmitSourceBuffer is called in OnBufferEnd?
			pSourceVoice = nullptr;
			if (killMe != nullptr)
			{
				killMe->FlushSourceBuffers();
				killMe->DestroyVoice();
			}

			// XXX: Not sure we even need a critical section - DestroyVoice is
			// blocking, and the documentation states no callbacks are called
			// or audio data is read after it returns, so it's safe to free up
			// resources.
			EnterCriticalSection(&cs);
			m_buffer = nullptr;
			LeaveCriticalSection(&cs);
		}

		void Init(IXAudio2* pXAudio2)
		{
			int chanMask = 0;
			switch (m_nChannels)
			{
				case 1:
					chanMask |= SPEAKER_FRONT_CENTER;
					break;
				case 2:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
					break;
				case 3:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY;
					break;
				case 4:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
					break;
				case 5:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
					break;
				case 6:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY;
					break;
				case 8:
					chanMask |= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_LOW_FREQUENCY;
					break;
			}
			_init(pXAudio2, chanMask);
		}

	protected:
		STDMETHOD_(void, OnBufferEnd)
		(void* context)
		{
			EnterCriticalSection(&cs);

			// All of these checks are necessary because XAudio2 is wonky shizat.
			// XXX: The pSourceVoice nullptr check seems a bit self-inflicted
			// due to the destructor logic.
			if (pSourceVoice == nullptr || context == nullptr)
			{
				LeaveCriticalSection(&cs);
				return;
			}

			T* qb = (T*)context;

			for (int p = 0; p < PacketsPerBuffer; p++, qb += SndOutPacketSize)
				SndBuffer::ReadSamples(qb);

			XAUDIO2_BUFFER buf = {0};
			buf.AudioBytes = m_BufferSizeBytes;
			buf.pAudioData = (BYTE*)context;
			buf.pContext = context;

			pSourceVoice->SubmitSourceBuffer(&buf);
			LeaveCriticalSection(&cs);
		}
	};

	HMODULE xAudio2DLL = nullptr;
	decltype(&XAudio2Create) pXAudio2Create = nullptr;
	CComPtr<IXAudio2> pXAudio2;
	IXAudio2MasteringVoice* pMasteringVoice = nullptr;
	std::unique_ptr<BaseStreamingVoice> m_voiceContext;

public:
	s32 Init()
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		try
		{
			HRESULT hr;

			xAudio2DLL = LoadLibraryEx(XAUDIO2_DLL, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (xAudio2DLL == nullptr)
				throw std::runtime_error("Could not load " XAUDIO2_DLL_A ". Error code:" + std::to_string(GetLastError()));

			pXAudio2Create = reinterpret_cast<decltype(&XAudio2Create)>(GetProcAddress(xAudio2DLL, "XAudio2Create"));
			if (pXAudio2Create == nullptr)
				throw std::runtime_error("XAudio2Create not found. Error code: " + std::to_string(GetLastError()));

			if (FAILED(hr = pXAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
				throw Exception::XAudio2Error(hr, "Failed to init XAudio2 engine. Error Details:");

			// Stereo Expansion was planned to grab the currently configured number of
			// Speakers from Windows's audio config.
			// This doesn't always work though, so let it be a user configurable option.

			int speakers;
			// speakers = (numSpeakers + 1) *2; ?
			switch (numSpeakers)
			{
				case 0: // Stereo
					speakers = 2;
					break;
				case 1: // Quadrafonic
					speakers = 4;
					break;
				case 2: // Surround 5.1
					speakers = 6;
					break;
				case 3: // Surround 7.1
					speakers = 8;
					break;
				default:
					speakers = 2;
			}

			if (FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice, speakers, SampleRate)))
				throw Exception::XAudio2Error(hr, "Failed creating mastering voice: ");

			switch (speakers)
			{
				case 2:
					ConLog("* SPU2 > Using normal 2 speaker stereo output.\n");
					m_voiceContext = std::make_unique<StreamingVoice<StereoOut16>>();
					break;
				case 3:
					ConLog("* SPU2 > 2.1 speaker expansion enabled.\n");
					m_voiceContext = std::make_unique<StreamingVoice<Stereo21Out16>>();
					break;
				case 4:
					ConLog("* SPU2 > 4 speaker expansion enabled [quadraphenia]\n");
					m_voiceContext = std::make_unique<StreamingVoice<Stereo40Out16>>();
					break;
				case 5:
					ConLog("* SPU2 > 4.1 speaker expansion enabled.\n");
					m_voiceContext = std::make_unique<StreamingVoice<Stereo41Out16>>();
					break;
				case 6:
				case 7:
					switch (dplLevel)
					{
						case 0: // "normal" stereo upmix
							ConLog("* SPU2 > 5.1 speaker expansion enabled.\n");
							m_voiceContext = std::make_unique<StreamingVoice<Stereo51Out16>>();
							break;
						case 1: // basic Dpl decoder without rear stereo balancing
							ConLog("* SPU2 > 5.1 speaker expansion with basic ProLogic dematrixing enabled.\n");
							m_voiceContext = std::make_unique<StreamingVoice<Stereo51Out16Dpl>>();
							break;
						case 2: // gigas PLII
							ConLog("* SPU2 > 5.1 speaker expansion with experimental ProLogicII dematrixing enabled.\n");
							m_voiceContext = std::make_unique<StreamingVoice<Stereo51Out16DplII>>();
							break;
					}
					break;
				default: // anything 8 or more gets the 7.1 treatment!
					ConLog("* SPU2 > 7.1 speaker expansion enabled.\n");
					m_voiceContext = std::make_unique<StreamingVoice<Stereo51Out16>>();
					break;
			}

			m_voiceContext->Init(pXAudio2);
		}
		catch (std::runtime_error& ex)
		{
			SysMessage(ex.what());
			Close();
			return -1;
		}

		return 0;
	}

	void Close()
	{
		// Clean up?
		// All XAudio2 interfaces are released when the engine is destroyed,
		// but being tidy never hurt.

		// Actually it can hurt.  As of DXSDK Aug 2008, doing a full cleanup causes
		// XA2 on Vista to crash.  Even if you copy/paste code directly from Microsoft.
		// But doing no cleanup at all causes XA2 under XP to crash.  So after much trial
		// and error we found a happy compromise as follows:

		m_voiceContext = nullptr;

		if (pMasteringVoice != nullptr)
			pMasteringVoice->DestroyVoice();

		pMasteringVoice = nullptr;

		pXAudio2.Release();
		CoUninitialize();

		if (xAudio2DLL)
		{
			FreeLibrary(xAudio2DLL);
			xAudio2DLL = nullptr;
			pXAudio2Create = nullptr;
		}
	}

	virtual void Configure(uptr parent)
	{
	}

	s32 Test() const
	{
		return 0;
	}

	int GetEmptySampleCount()
	{
		if (m_voiceContext == nullptr)
			return 0;
		return m_voiceContext->GetEmptySampleCount();
	}

	const wchar_t* GetIdent() const
	{
		return L"xaudio2";
	}

	const wchar_t* GetLongName() const
	{
		return L"XAudio 2 (Recommended)";
	}

	void ReadSettings()
	{
	}

	void SetApiSettings(wxString api)
	{
	}

	void WriteSettings() const
	{
	}

} static XA2;

SndOutModule* XAudio2Out = &XA2;
