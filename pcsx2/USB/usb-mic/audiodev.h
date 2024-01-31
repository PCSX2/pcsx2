// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

	AudioDevice(AudioDir dir, u32 channels)
		: mAudioDir(dir)
		, mChannels(channels)
	{
	}

protected:
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

	static std::unique_ptr<AudioDevice> CreateDevice(AudioDir dir, u32 channels, std::string devname, s32 latency);
	static std::unique_ptr<AudioDevice> CreateNoopDevice(AudioDir dir, u32 channels);
	static std::vector<std::pair<std::string, std::string>> GetInputDeviceList();
	static std::vector<std::pair<std::string, std::string>> GetOutputDeviceList();
};
