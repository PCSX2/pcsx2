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

#ifndef AUDIODEV_H
#define AUDIODEV_H

#include <string>
#include <vector>
#include <queue>

#define S_AUDIO_SOURCE0 TEXT("Audio source 1")
#define S_AUDIO_SOURCE1 TEXT("Audio source 2")
#define S_AUDIO_SINK0 TEXT("Audio sink 1")
#define S_AUDIO_SINK1 TEXT("Audio sink 2")
#define N_AUDIO_SOURCE0 TEXT("audio_src_0")
#define N_AUDIO_SOURCE1 TEXT("audio_src_1")
#define N_AUDIO_SINK0 TEXT("audio_sink_0")
#define N_AUDIO_SINK1 TEXT("audio_sink_1")
#define S_BUFFER_LEN TEXT("Buffer length")
#define N_BUFFER_LEN TEXT("buffer_len")
#define N_BUFFER_LEN_SRC TEXT("buffer_len_src")
#define N_BUFFER_LEN_SINK TEXT("buffer_len_sink")

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

//TODO sufficient for linux too?
struct AudioDeviceInfoA
{
	//int intID; //optional ID
	std::string strID;
	std::string strName; //gui name
};

struct AudioDeviceInfoW
{
	//int intID; //optional ID
	std::wstring strID;
	std::wstring strName; //gui name
};

#if _WIN32
#define AudioDeviceInfo AudioDeviceInfoW
#else
#define AudioDeviceInfo AudioDeviceInfoA
#endif

class AudioDevice
{
public:
	AudioDevice(int port, const char* dev_type, int device, AudioDir dir)
		: mPort(port)
		, mDevType(dev_type)
		, mDevice(device)
		, mAudioDir(dir)
	{
	}

protected:
	int mPort;
	const char* mDevType;
	int mDevice;
	AudioDir mAudioDir;

public:
	virtual ~AudioDevice() {}
	//get buffer, converted to 16bit int format
	virtual uint32_t GetBuffer(int16_t* buff, uint32_t len) = 0;
	virtual uint32_t SetBuffer(int16_t* buff, uint32_t len) = 0;
	/*
		Get how many frames has been recorded so that caller knows 
		how much to allocated for 16-bit buffer.
	*/
	virtual bool GetFrames(uint32_t* size) = 0;
	virtual void SetResampling(int samplerate) = 0;
	virtual uint32_t GetChannels() = 0;

	virtual void Start() {}
	virtual void Stop() {}

	// Compare if another instance is using the same device
	virtual bool Compare(AudioDevice* compare) = 0;

	//Remember to add to your class
	//static const wchar_t* GetName();
};

typedef std::vector<AudioDeviceInfo> AudioDeviceInfoList;
#endif
