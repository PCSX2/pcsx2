/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "SPU2/SndOut.h"
#include "Host.h"
#include "IconsFontAwesome5.h"

#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/RedtapeWindows.h"
#include "common/ScopedGuard.h"

#include "cubeb/cubeb.h"

#ifdef _WIN32
#include <objbase.h>
#endif

class Cubeb : public SndOutModule
{
private:
	//////////////////////////////////////////////////////////////////////////////////////////
	// Stuff necessary for speaker expansion
	class SampleReader
	{
	public:
		virtual ~SampleReader() = default;
		virtual void ReadSamples(void* outputBuffer, long frames) = 0;
	};

	template <class T>
	class ConvertedSampleReader final : public SampleReader
	{
		u64* const written;

	public:
		ConvertedSampleReader() = delete;

		explicit ConvertedSampleReader(u64* pWritten)
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
#ifdef _WIN32
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_COMInitializedByUs = SUCCEEDED(hr);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			Host::ReportErrorAsync("Cubeb Error", "Failed to initialize COM");
			return false;
		}
#endif

#ifdef PCSX2_DEVBUILD
		cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback);
#endif

		int rv = cubeb_init(&m_context, "PCSX2", EmuConfig.SPU2.BackendName.empty() ? nullptr : EmuConfig.SPU2.BackendName.c_str());
		if (rv != CUBEB_OK)
		{
			Host::ReportFormattedErrorAsync("Cubeb Error", "Could not initialize cubeb context: %d", rv);
			return false;
		}

		switch (EmuConfig.SPU2.SpeakerConfiguration) // speakers = (numSpeakers + 1) *2; ?
		{
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
				break; // Stereo
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
				switch (EmuConfig.SPU2.DplDecodingLevel)
				{
					case 1:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion with basic ProLogic dematrixing enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16Dpl>>(&writtenSoFar); // basic Dpl decoder without rear stereo balancing
						break;
					case 2:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion with experimental ProLogicII dematrixing enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16DplII>>(&writtenSoFar); //gigas PLII
						break;
					default:
						Console.WriteLn("(Cubeb) 5.1 speaker expansion enabled.");
						ActualReader = std::make_unique<ConvertedSampleReader<Stereo51Out16>>(&writtenSoFar); //"normal" stereo upmix
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

		const u32 requested_latency_frames = static_cast<u32>((EmuConfig.SPU2.OutputLatency * SampleRate) / 1000u);
		u32 latency_frames = 0;
		rv = cubeb_get_min_latency(m_context, &params, &latency_frames);
		if (rv == CUBEB_ERROR_NOT_SUPPORTED)
		{
			Console.WriteLn("(Cubeb) Cubeb backend does not support latency queries, using latency of %d ms (%u frames).",
				EmuConfig.SPU2.OutputLatency, requested_latency_frames);
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
			if (!EmuConfig.SPU2.OutputLatencyMinimal)
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

		cubeb_devid selected_device = nullptr;
		const std::string& selected_device_name = EmuConfig.SPU2.DeviceName;
		cubeb_device_collection devices;
		if (!selected_device_name.empty())
		{
			rv = cubeb_enumerate_devices(m_context, CUBEB_DEVICE_TYPE_OUTPUT, &devices);
			if (rv == CUBEB_OK)
			{
				for (size_t i = 0; i < devices.count; i++)
				{
					const cubeb_device_info& di = devices.device[i];
					if (di.device_id && selected_device_name == di.device_id)
					{
						Console.WriteLn("Using output device '%s' (%s).", di.device_id, di.friendly_name ? di.friendly_name : di.device_id);
						selected_device = di.devid;
						break;
					}
				}

				if (!selected_device)
				{
					Host::AddIconOSDMessage("CubebDeviceNotFound", ICON_FA_VOLUME_MUTE,
						fmt::format(
							TRANSLATE_FS("SPU2", "Requested audio output device '{}' not found, using default."),
							selected_device_name),
						Host::OSD_WARNING_DURATION);
				}
			}
			else
			{
				Console.Error("cubeb_enumerate_devices() returned %d, using default device.", rv);
			}
		}

		char stream_name[32];
		std::snprintf(stream_name, sizeof(stream_name), "%p", this);

		rv = cubeb_stream_init(m_context, &stream, stream_name, nullptr, nullptr, selected_device, &params,
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

	const char* GetDisplayName() const override
	{
		//: Cubeb is an audio engine name. Leave as-is.
		return TRANSLATE_NOOP("SPU2", "Cubeb (Cross-platform)");
	}

	const char* const* GetBackendNames() const override
	{
		return cubeb_get_backend_names();
	}

	std::vector<SndOutDeviceInfo> GetOutputDeviceList(const char* driver) const override
	{
		std::vector<SndOutDeviceInfo> ret;
		ret.emplace_back(std::string(), "Default", 0u);

		cubeb* context;
		int rv = cubeb_init(&context, "PCSX2", (driver && *driver) ? driver : nullptr);
		if (rv != CUBEB_OK)
		{
			Console.Error("(GetOutputDeviceList) cubeb_init() failed: %d", rv);
			return ret;
		}

		ScopedGuard context_cleanup([context]() { cubeb_destroy(context); });

		cubeb_device_collection devices;
		rv = cubeb_enumerate_devices(context, CUBEB_DEVICE_TYPE_OUTPUT, &devices);
		if (rv != CUBEB_OK)
		{
			Console.Error("(GetOutputDeviceList) cubeb_enumerate_devices() failed: %d", rv);
			return ret;
		}

		ScopedGuard devices_cleanup([context, &devices]() { cubeb_device_collection_destroy(context, &devices); });

		// we need stream parameters to query latency
		cubeb_stream_params params = {};
		params.format = CUBEB_SAMPLE_S16LE;
		params.rate = SampleRate;
		params.channels = 2;
		params.layout = CUBEB_LAYOUT_UNDEFINED;
		params.prefs = CUBEB_STREAM_PREF_NONE;

		u32 min_latency = 0;
		cubeb_get_min_latency(context, &params, &min_latency);
		ret[0].minimum_latency_frames = min_latency;

		for (size_t i = 0; i < devices.count; i++)
		{
			const cubeb_device_info& di = devices.device[i];
			if (!di.device_id)
				continue;

			ret.emplace_back(di.device_id, di.friendly_name ? di.friendly_name : di.device_id, min_latency);
		}

		return ret;
	}
};

static Cubeb s_Cubeb;
SndOutModule* CubebOut = &s_Cubeb;
