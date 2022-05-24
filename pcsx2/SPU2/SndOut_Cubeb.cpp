/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/RedtapeWindows.h"
#include "cubeb/cubeb.h"

#include "Global.h"
#include "SndOut.h"

#ifdef PCSX2_CORE

#include "HostSettings.h"

#else

#include "gui/StringHelpers.h"

extern bool CfgReadBool(const wchar_t* Section, const wchar_t* Name, bool Default);
extern int CfgReadInt(const wchar_t* Section, const wchar_t* Name, int Default);
extern void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default);

#endif

class Cubeb : public SndOutModule
{
private:
	static constexpr int MINIMUM_LATENCY_MS = 20;
	static constexpr int MAXIMUM_LATENCY_MS = 200;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Stuff necessary for speaker expansion
	class SampleReader
	{
	public:
		virtual void ReadSamples(void* outputBuffer, long frames) = 0;
	};

	template <class T>
	class ConvertedSampleReader final : public SampleReader
	{
		u64* const written;

	public:
		ConvertedSampleReader() = delete;

		ConvertedSampleReader(u64* pWritten)
			: written(pWritten)
		{
		}

		void ReadSamples(void* outputBuffer, long frames) override
		{
			T* p1 = static_cast<T*>(outputBuffer);

			while (frames > 0)
			{
				const long frames_to_read = std::min<long>(frames, SndOutPacketSize);
				SndBuffer::ReadSamples(p1, frames_to_read);
				p1 += frames_to_read;
				frames -= frames_to_read;
			}

			(*written) += frames;
		}
	};

	void DestroyContextAndStream()
	{
		if (stream)
		{
			cubeb_stream_stop(stream);
			cubeb_stream_destroy(stream);
			stream = nullptr;
		}

		if (m_context)
		{
			cubeb_destroy(m_context);
			m_context = nullptr;
		}

		ActualReader.reset();

#ifdef _WIN32
		if (m_COMInitializedByUs)
		{
			CoUninitialize();
			m_COMInitializedByUs = false;
		}
#endif
	}

	static void LogCallback(const char* fmt, ...)
	{
		std::va_list ap;
		va_start(ap, fmt);
		std::string msg(StringUtil::StdStringFromFormatV(fmt, ap));
		va_end(ap);
		Console.WriteLn("(Cubeb): %s", msg.c_str());
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	// Configuration Vars
#ifdef _WIN32
	bool m_COMInitializedByUs = false;
#endif
	bool m_SuggestedLatencyMinimal = false;
	int m_SuggestedLatencyMS = 20;
	std::string m_Backend;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Instance vars
	u64 writtenSoFar = 0;
	u64 writtenLastTime = 0;
	u64 positionLastTime = 0;

	u32 channels = 0;
	cubeb* m_context = nullptr;
	cubeb_stream* stream = nullptr;
	std::unique_ptr<SampleReader> ActualReader;
	bool m_paused = false;


public:
	Cubeb() = default;

	~Cubeb()
	{
		DestroyContextAndStream();
	}

	bool Init() override
	{
		ReadSettings();

		// TODO(Stenzek): Migrate the errors to Host::ReportErrorAsync() once more Qt stuff is merged.

#ifdef _WIN32
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_COMInitializedByUs = SUCCEEDED(hr);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			Console.Error("(Cubeb) Failed to initialize COM");
			return false;
		}
#endif

#ifdef PCSX2_DEVBUILD
		cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback);
#endif

		int rv = cubeb_init(&m_context, "PCSX2", m_Backend.empty() ? nullptr : m_Backend.c_str());
		if (rv != CUBEB_OK)
		{
			Console.Error("(Cubeb) Could not initialize cubeb context: %d", rv);
			return false;
		}

		switch (numSpeakers) // speakers = (numSpeakers + 1) *2; ?
		{
			case 0:
				channels = 2;
				break; // Stereo
			case 1:
				channels = 4;
				break; // Quadrafonic
			case 2:
				channels = 6;
				break; // Surround 5.1
			case 3:
				channels = 8;
				break; // Surround 7.1
			default:
				channels = 2;
				break;
		}

		cubeb_channel_layout layout = CUBEB_LAYOUT_UNDEFINED;
		switch (channels)
		{
			case 2:
				Console.WriteLn("(Cubeb) Using normal 2 speaker stereo output.");
				ActualReader = std::make_unique<ConvertedSampleReader<StereoOut16>>(&writtenSoFar);
				break;

			case 3:
				Console.WriteLn("(Cubeb) 2.1 speaker expansion enabled.");
				ActualReader = std::make_unique<ConvertedSampleReader<Stereo21Out16>>(&writtenSoFar);
				layout = CUBEB_LAYOUT_STEREO_LFE;
				break;

			case 4:
				Console.WriteLn("(Cubeb) 4 speaker expansion enabled [quadraphenia]");
				ActualReader = std::make_unique<ConvertedSampleReader<Stereo40Out16>>(&writtenSoFar);
				layout = CUBEB_LAYOUT_QUAD;
				break;

			case 5:
				Console.WriteLn("(Cubeb) 4.1 speaker expansion enabled.");
				ActualReader = std::make_unique<ConvertedSampleReader<Stereo41Out16>>(&writtenSoFar);
				layout = CUBEB_LAYOUT_QUAD_LFE;
				break;

			case 6:
			case 7:
				switch (dplLevel)
				{
					case 0:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16>>(&writtenSoFar); //"normal" stereo upmix
						break;
					case 1:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion with basic ProLogic dematrixing enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16Dpl>>(&writtenSoFar); // basic Dpl decoder without rear stereo balancing
						break;
					case 2:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion with experimental ProLogicII dematrixing enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16DplII>>(&writtenSoFar); //gigas PLII
						break;
				}
				channels = 6; // we do not support 7.0 or 6.2 configurations, downgrade to 5.1
				layout = CUBEB_LAYOUT_3F2_LFE;
				break;

			default: // anything 8 or more gets the 7.1 treatment!
				Console.WriteLn("(Cubeb) 7.1 speaker expansion enabled.");
				ActualReader = std::make_unique<ConvertedSampleReader<Stereo71Out16>>(&writtenSoFar);
				channels = 8; // we do not support 7.2 or more, downgrade to 7.1
				layout = CUBEB_LAYOUT_3F4_LFE;
				break;
		}

		cubeb_stream_params params = {};
		params.format = CUBEB_SAMPLE_S16LE;
		params.rate = SampleRate;
		params.channels = channels;
		params.layout = layout;
		params.prefs = CUBEB_STREAM_PREF_NONE;

		const u32 requested_latency_frames = static_cast<u32>((m_SuggestedLatencyMS * static_cast<u32>(SampleRate)) / 1000u);
		u32 latency_frames = 0;
		rv = cubeb_get_min_latency(m_context, &params, &latency_frames);
		if (rv == CUBEB_ERROR_NOT_SUPPORTED)
		{
			Console.WriteLn("(Cubeb) Cubeb backend does not support latency queries, using latency of %d ms (%u frames).",
				m_SuggestedLatencyMS, requested_latency_frames);
			latency_frames = requested_latency_frames;
		}
		else
		{
			if (rv != CUBEB_OK)
			{
				Console.Error("(Cubeb) Could not get minimum latency: %d", rv);
				DestroyContextAndStream();
				return false;
			}

			const float minimum_latency_ms = static_cast<float>(latency_frames * 1000u) / static_cast<float>(SampleRate);
			Console.WriteLn("(Cubeb) Minimum latency: %.2f ms (%u audio frames)", minimum_latency_ms, latency_frames);
			if (!m_SuggestedLatencyMinimal)
			{
				if (latency_frames > requested_latency_frames)
				{
					Console.Warning("(Cubeb) Minimum latency is above requested latency: %u vs %u, adjusting to compensate.",
						latency_frames, requested_latency_frames);
				}
				else
				{
					latency_frames = requested_latency_frames;
				}
			}
		}

		char stream_name[32];
		std::snprintf(stream_name, sizeof(stream_name), "%p", this);

		rv = cubeb_stream_init(m_context, &stream, stream_name, nullptr, nullptr, nullptr, &params,
			latency_frames, &Cubeb::DataCallback, &Cubeb::StateCallback, this);
		if (rv != CUBEB_OK)
		{
			Console.Error("(Cubeb) Could not create stream: %d", rv);
			DestroyContextAndStream();
			return false;
		}

		rv = cubeb_stream_start(stream);
		if (rv != CUBEB_OK)
		{
			Console.Error("(Cubeb) Could not start stream: %d", rv);
			DestroyContextAndStream();
			return false;
		}

		m_paused = false;
		return true;
	}

	void Close() override
	{
		DestroyContextAndStream();
	}

	static void StateCallback(cubeb_stream* stream, void* user_ptr, cubeb_state state)
	{
	}

	static long DataCallback(cubeb_stream* stm, void* user_ptr, const void* input_buffer, void* output_buffer, long nframes)
	{
		static_cast<Cubeb*>(user_ptr)->ActualReader->ReadSamples(output_buffer, nframes);
		return nframes;
	}

	void SetPaused(bool paused) override
	{
		if (paused == m_paused || !stream)
			return;

		const int rv = paused ? cubeb_stream_stop(stream) : cubeb_stream_start(stream);
		if (rv != CUBEB_OK)
		{
			Console.Error("(Cubeb) Could not %s stream: %d", paused ? "pause" : "resume", rv);
			return;
		}

		m_paused = paused;
	}

	int GetEmptySampleCount() override
	{
		u64 pos;
		if (cubeb_stream_get_position(stream, &pos) != CUBEB_OK)
			pos = 0;

		const int playedSinceLastTime = (writtenSoFar - writtenLastTime) + (pos - positionLastTime);
		writtenLastTime = writtenSoFar;
		positionLastTime = pos;
		return playedSinceLastTime;
	}

	const char* GetIdent() const override
	{
		return "cubeb";
	}

	const char* GetLongName() const override
	{
		return "Cubeb (Cross-platform)";
	}

	void ReadSettings()
	{
#ifndef PCSX2_CORE
		m_SuggestedLatencyMinimal = CfgReadBool(L"Cubeb", L"MinimalSuggestedLatency", false);
		m_SuggestedLatencyMS = std::clamp(CfgReadInt(L"Cubeb", L"ManualSuggestedLatencyMS", MINIMUM_LATENCY_MS), MINIMUM_LATENCY_MS, MAXIMUM_LATENCY_MS);

		// TODO: Once the config stuff gets merged, drop the wxString here.
		wxString backend;
		CfgReadStr(L"Cubeb", L"BackendName", backend, L"");
		m_Backend = StringUtil::wxStringToUTF8String(backend);
#else
		m_SuggestedLatencyMinimal = Host::GetBoolSettingValue("Cubeb", "MinimalSuggestedLatency", false);
		m_SuggestedLatencyMS = std::clamp(Host::GetIntSettingValue("Cubeb", "ManualSuggestedLatencyMS", MINIMUM_LATENCY_MS), MINIMUM_LATENCY_MS, MAXIMUM_LATENCY_MS);
		m_Backend = Host::GetStringSettingValue("Cubeb", "BackendName", "");
#endif
	}
};

static Cubeb s_Cubeb;
SndOutModule* CubebOut = &s_Cubeb;
