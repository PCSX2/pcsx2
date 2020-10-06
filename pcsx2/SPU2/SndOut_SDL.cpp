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
#include <cassert>
#include <iostream>

#include "Global.h"
#include "SndOut.h"
#ifdef __linux__
#include "Linux/Dialogs.h"
#elif defined(_WIN32)
#include "Windows/Dialogs.h"
#endif

#include <memory>

/* Using SDL2 requires other SDL dependencies in pcsx2 get upgraded as well, or the
 * symbol tables would conflict. Other dependencies in Linux are wxwidgets (you can
 * build wx without sdl support, though) and onepad at the time of writing this. */
#include <SDL.h>
#include <SDL_audio.h>
typedef StereoOut16 StereoOut_SDL;

namespace
{
	/* Since spu2 only ever outputs stereo, we don't worry about emitting surround sound
	 * even though SDL2 supports it */
	const Uint8 channels = 2;
	/* SDL2 supports s32 audio */
	/* Samples should vary from [512,8192] according to SDL spec. Take note this is the desired
	 * sample count and SDL may provide otherwise. Pulseaudio will cut this value in half if
	 * PA_STREAM_ADJUST_LATENCY is set in the backened, for example. */
	const Uint16 desiredSamples = 2048;
	const Uint16 format = AUDIO_S16SYS;

	Uint16 samples = desiredSamples;

	std::unique_ptr<StereoOut_SDL[]> buffer;

	void callback_fillBuffer(void* userdata, Uint8* stream, int len)
	{
		Uint16 sdl_samples = samples;

#if SDL_MAJOR_VERSION >= 2
		memset(stream, 0, len);
		// As of SDL 2.0.4 the buffer is too small to contains all samples
		// len is 2048, samples is 1024 and sizeof(StereoOut_SDL) is 4
		sdl_samples = len / sizeof(StereoOut_SDL);
#endif

		// Length should always be samples in bytes.
		assert(len / sizeof(StereoOut_SDL) == sdl_samples);

		for (Uint16 i = 0; i < sdl_samples; i += SndOutPacketSize)
			SndBuffer::ReadSamples(&buffer[i]);
		SDL_MixAudio(stream, (Uint8*)buffer.get(), len, SDL_MIX_MAXVOLUME);
	}
} // namespace

struct SDLAudioMod : public SndOutModule
{
	static SDLAudioMod mod;
	std::string m_api;

	s32 Init()
	{
		ReadSettings();

#if SDL_MAJOR_VERSION >= 2
		std::cerr << "Request SDL audio driver: " << m_api.c_str() << std::endl;
#endif

		/* SDL backends will mangle the AudioSpec and change the sample count. If we reopen
		 * the audio backend, we need to make sure we keep our desired samples in the spec */
		spec.samples = desiredSamples;

		// Mandatory otherwise, init will be redone in SDL_OpenAudio
		if (SDL_Init(SDL_INIT_AUDIO) < 0)
		{
			std::cerr << "SPU2: SDL INIT audio error: " << SDL_GetError() << std::endl;
			return -1;
		}

#if SDL_MAJOR_VERSION >= 2
		if (m_api.compare("pulseaudio"))
		{
			// Close the audio, but keep the subsystem open
			SDL_AudioQuit();
			// Reopen the audio
			if (SDL_AudioInit(m_api.c_str()) < 0)
			{
				std::cerr << "SPU2: SDL audio init error: " << SDL_GetError() << std::endl;
				return -1;
			}
		}
#endif

		if (SDL_OpenAudio(&spec, nullptr) < 0)
		{
			std::cerr << "SPU2: SDL audio error: " << SDL_GetError() << std::endl;
			return -1;
		}

#if SDL_MAJOR_VERSION >= 2
		std::cerr << "Opened SDL audio driver: " << SDL_GetCurrentAudioDriver() << std::endl;
#endif

		/* This is so ugly. It is hilariously ugly. I didn't use a vector to save reallocs. */
		if (samples != spec.samples || buffer == nullptr)
			buffer = std::unique_ptr<StereoOut_SDL[]>(new StereoOut_SDL[spec.samples]);
		if (samples != spec.samples)
		{
			fprintf(stderr, "SPU2: SDL failed to get desired samples (%d) got %d samples instead\n", samples, spec.samples);

			// Samples must always be a multiple of packet size.
			assert(spec.samples % SndOutPacketSize == 0);
			samples = spec.samples;
		}
		SDL_PauseAudio(0);
		return 0;
	}

	const wchar_t* GetIdent() const { return L"SDLAudio"; }
	const wchar_t* GetLongName() const { return L"SDL Audio"; }

	void Close()
	{
		// Related to SDL_Init(SDL_INIT_AUDIO)
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	~SDLAudioMod() { Close(); }

	s32 Test() const { return 0; }
	int GetEmptySampleCount() { return 0; }

	void Configure(uptr parent) {}

	void ReadSettings()
	{
		wxString api(L"EMPTYEMPTYEMPTY");
		CfgReadStr(L"SDL", L"HostApi", api, L"pulseaudio");
		SetApiSettings(api);
	}

	void WriteSettings() const
	{
		CfgWriteStr(L"SDL", L"HostApi", wxString(m_api.c_str(), wxConvUTF8));
	};

	void SetApiSettings(wxString api)
	{
#if SDL_MAJOR_VERSION >= 2
		// Validate the api name
		bool valid = false;
		std::string api_name = std::string(api.utf8_str());
		for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
		{
			valid |= (api_name.compare(SDL_GetAudioDriver(i)) == 0);
		}
		if (valid)
		{
			m_api = api.utf8_str();
		}
		else
		{
			std::cerr << "SDL audio driver configuration is invalid!" << std::endl
					  << "It will be replaced by pulseaudio!" << std::endl;
			m_api = "pulseaudio";
		}
#endif
	}


private:
	SDL_AudioSpec spec;

	SDLAudioMod()
		: m_api("pulseaudio")
		, spec({SampleRate, format, channels, 0,
				desiredSamples, 0, 0, &callback_fillBuffer, nullptr})
	{
		// Number of samples must be a multiple of packet size.
		assert(samples % SndOutPacketSize == 0);
	}
};

SDLAudioMod SDLAudioMod::mod;

SndOutModule* const SDLOut = &SDLAudioMod::mod;
