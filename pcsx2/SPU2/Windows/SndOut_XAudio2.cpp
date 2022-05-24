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
#include "SPU2/Global.h"
#ifndef PCSX2_CORE
#include "Dialogs.h"
#endif
#include "common/Console.h"

#include <xaudio2.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

//#define XAUDIO2_DEBUG

class XAudio2Mod final : public SndOutModule
{
private:
	static const int PacketsPerBuffer = 8;
	static const int MAX_BUFFER_COUNT = 3;

	class BaseStreamingVoice : public IXAudio2VoiceCallback
	{
	protected:
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		std::unique_ptr<s16[]> m_buffer;

		const uint m_nBuffers;
		const uint m_nChannels;
		const uint m_BufferSize;
		const uint m_BufferSizeBytes;

	public:
		virtual ~BaseStreamingVoice() = default;

		int GetEmptySampleCount() const
		{
			XAUDIO2_VOICE_STATE state;
			pSourceVoice->GetState(&state);
			return state.SamplesPlayed & (m_BufferSize - 1);
		}

		BaseStreamingVoice(uint numChannels)
#ifndef PCSX2_CORE
			: m_nBuffers(Config_XAudio2.NumBuffers)
#else
			: m_nBuffers(2)
#endif
			, m_nChannels(numChannels)
			, m_BufferSize(SndOutPacketSize * m_nChannels * PacketsPerBuffer)
			, m_BufferSizeBytes(m_BufferSize * sizeof(s16))
		{
		}

		bool Init(IXAudio2* pXAudio2)
		{
			DWORD chanMask = 0;
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

			WAVEFORMATEXTENSIBLE wfx{};

			wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wfx.Format.nSamplesPerSec = SampleRate;
			wfx.Format.nChannels = m_nChannels;
			wfx.Format.wBitsPerSample = 16;
			wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
			wfx.Format.nAvgBytesPerSec = SampleRate * wfx.Format.nBlockAlign;
			wfx.Format.cbSize = sizeof(wfx) - sizeof(WAVEFORMATEX);
			wfx.Samples.wValidBitsPerSample = 16;
			wfx.dwChannelMask = chanMask;
			wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

			const HRESULT hr = pXAudio2->CreateSourceVoice(&pSourceVoice, reinterpret_cast<WAVEFORMATEX*>(&wfx), XAUDIO2_VOICE_NOSRC, 1.0f, this);
			if (FAILED(hr))
			{
				Console.Error("XAudio2 CreateSourceVoice failure: %08X", hr);
				return false;
			}

			m_buffer = std::make_unique<s16[]>(m_nBuffers * m_BufferSize);

			// Start some buffers.
			for (size_t i = 0; i < m_nBuffers; i++)
			{
				XAUDIO2_BUFFER buf{};
				buf.AudioBytes = m_BufferSizeBytes;
				buf.pContext = &m_buffer[i * m_BufferSize];
				buf.pAudioData = static_cast<BYTE*>(buf.pContext);
				pSourceVoice->SubmitSourceBuffer(&buf);
			}

			Start();
			return true;
		}

		void Stop()
		{
			pSourceVoice->Stop();
		}

		void Start()
		{
			pSourceVoice->Start(0, 0);
		}

	protected:
		STDMETHOD_(void, OnVoiceProcessingPassStart)
		(UINT32) override {}
		STDMETHOD_(void, OnVoiceProcessingPassEnd)
		() override {}
		STDMETHOD_(void, OnStreamEnd)
		() override {}
		STDMETHOD_(void, OnBufferStart)
		(void*) override {}
		STDMETHOD_(void, OnLoopEnd)
		(void*) override {}
		STDMETHOD_(void, OnVoiceError)
		(THIS_ void* pBufferContext, HRESULT Error) override {}
	};

	template <typename T>
	class StreamingVoice final : public BaseStreamingVoice
	{
	public:
		StreamingVoice()
			: BaseStreamingVoice(sizeof(T) / sizeof(s16))
		{
		}

		virtual ~StreamingVoice() override
		{
			// Must be done here and not BaseStreamingVoice, as else OnBufferEnd will not be callable anymore
			// but it will be called by DestroyVoice.
			if (pSourceVoice != nullptr)
			{
				pSourceVoice->Stop();
				pSourceVoice->DestroyVoice();
			}
		}

	protected:
		STDMETHOD_(void, OnBufferEnd)
		(void* context) override
		{
			T* qb = static_cast<T*>(context);

			for (size_t p = 0; p < PacketsPerBuffer; p++, qb += SndOutPacketSize)
				SndBuffer::ReadSamples(qb);

			XAUDIO2_BUFFER buf{};
			buf.AudioBytes = m_BufferSizeBytes;
			buf.pAudioData = static_cast<BYTE*>(context);
			buf.pContext = context;

			pSourceVoice->SubmitSourceBuffer(&buf);
		}
	};

	wil::unique_couninitialize_call xaudio2CoInitialize;
	wil::unique_hmodule xAudio2DLL;
	wil::com_ptr_nothrow<IXAudio2> pXAudio2;
	IXAudio2MasteringVoice* pMasteringVoice = nullptr;
	std::unique_ptr<BaseStreamingVoice> m_voiceContext;
	bool m_paused = false;

public:
	bool Init() override
	{
		xaudio2CoInitialize = wil::CoInitializeEx_failfast(COINIT_MULTITHREADED);

		xAudio2DLL.reset(LoadLibraryEx(XAUDIO2_DLL, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
		if (xAudio2DLL == nullptr)
		{
			Console.Error("Could not load %s. Error code: %d" XAUDIO2_DLL_A, GetLastError());
			Close();
			return false;
		}

		auto pXAudio2Create = GetProcAddressByFunctionDeclaration(xAudio2DLL.get(), XAudio2Create);
		if (pXAudio2Create == nullptr)
		{
			Console.Error("XAudio2Create not found. Error code: %d", GetLastError());
			Close();
			return false;
		}

		HRESULT hr = pXAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr))
		{
			Console.Error("Failed to init XAudio2 engine. Error Details: %08X", hr);
			Close();
			return false;
		}

#ifdef XAUDIO2_DEBUG
		XAUDIO2_DEBUG_CONFIGURATION debugConfig{};
		debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
		pXAudio2->SetDebugConfiguration(&debugConfig, nullptr);
#endif

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
				break;
		}

		hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice, speakers, SampleRate);
		if (FAILED(hr))
		{
			Console.Error("Failed creating mastering voice: %08X", hr);
			Close();
			return false;
		}

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

		if (!m_voiceContext->Init(pXAudio2.get()))
		{
			Close();
			return false;
		}

		m_paused = false;
		return true;
	}

	void Close() override
	{
		m_voiceContext.reset();

		if (pMasteringVoice != nullptr)
		{
			pMasteringVoice->DestroyVoice();
			pMasteringVoice = nullptr;
		}

		pXAudio2.reset();
		xAudio2DLL.reset();
		xaudio2CoInitialize.reset();
	}

	int GetEmptySampleCount() override
	{
		if (m_voiceContext == nullptr)
			return 0;
		return m_voiceContext->GetEmptySampleCount();
	}

	void SetPaused(bool paused) override
	{
		if (m_voiceContext == nullptr || m_paused == paused)
			return;

		if (paused)
			m_voiceContext->Stop();
		else
			m_voiceContext->Start();

		m_paused = paused;
	}

	const char* GetIdent() const override
	{
		return "xaudio2";
	}

	const char* GetLongName() const override
	{
		return "XAudio 2 (Recommended)";
	}

} static XA2;

SndOutModule* XAudio2Out = &XA2;
