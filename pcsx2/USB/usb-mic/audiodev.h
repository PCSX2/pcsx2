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

//
// Types to shared by platforms and config. dialog.
//

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>

enum MicMode
{
	MIC_MODE_NONE,
	MIC_MODE_SINGLE,
	MIC_MODE_SEPARATE,
	// Use same source for both player or
	// left channel for P1 and right for P2 if stereo.
	MIC_MODE_SHARED
};

enum AudioDir
{
	AUDIODIR_SOURCE = 0,
	AUDIODIR_SINK
};

class AudioDevice
{
public:
	static constexpr s32 DEFAULT_LATENCY = 100;
	static constexpr const char* DEFAULT_LATENCY_STR = "100";

	AudioDevice(u32 port, AudioDir dir, u32 channels)
		: mPort(port)
		, mAudioDir(dir)
		, mChannels(channels)
	{
	}

protected:
	u32 mPort;
	s32 mSubDevice;
	AudioDir mAudioDir;
	u32 mChannels;

public:
	virtual ~AudioDevice() = default;

	//get buffer, converted to 16bit int format
	virtual uint32_t GetBuffer(int16_t* buff, uint32_t len) = 0;
	virtual uint32_t SetBuffer(int16_t* buff, uint32_t len) = 0;
	/*
		Get how many frames has been recorded so that caller knows
		how much to allocated for 16-bit buffer.
	*/
	virtual bool GetFrames(uint32_t* size) = 0;
	virtual void SetResampling(int samplerate) = 0;
	uint32_t GetChannels() { return mChannels; }

	virtual bool Start() = 0;
	virtual void Stop() = 0;

	// Compare if another instance is using the same device
	virtual bool Compare(AudioDevice* compare) const = 0;

	static std::unique_ptr<AudioDevice> CreateDevice(u32 port, AudioDir dir, u32 channels, std::string devname, s32 latency);
	static std::unique_ptr<AudioDevice> CreateNoopDevice(u32 port, AudioDir dir, u32 channels);
	static std::vector<std::pair<std::string, std::string>> GetInputDeviceList();
	static std::vector<std::pair<std::string, std::string>> GetOutputDeviceList();
};
