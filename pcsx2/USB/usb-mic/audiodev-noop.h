#pragma once
#include "audiodeviceproxy.h"

namespace usb_mic { namespace audiodev_noop {

static const char *APINAME = "noop";

class NoopAudioDevice : public AudioDevice
{
public:
	NoopAudioDevice(int port, const char* dev_type, int mic, AudioDir dir): AudioDevice(port, dev_type, mic, dir) {}
	~NoopAudioDevice() {}
	void Start() {}
	void Stop() {}
	virtual bool GetFrames(uint32_t *size)
	{
		return true;
	}
	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outFrames)
	{
		return outFrames;
	}
	virtual uint32_t SetBuffer(int16_t *inBuf, uint32_t inFrames)
	{
		return inFrames;
	}
	virtual void SetResampling(int samplerate) {}
	virtual uint32_t GetChannels()
	{
		return 1;
	}

	virtual bool Compare(AudioDevice* compare)
	{
		return false;
	}

	static const TCHAR* Name()
	{
		return TEXT("NOOP");
	}

	static bool AudioInit()
	{
		return true;
	}

	static void AudioDeinit()
	{
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir )
	{
		AudioDeviceInfo info;
		info.strID = TEXT("silence");
		info.strName = TEXT("Silence");
		devices.push_back(info);
	}

	static int Configure(int port, const char* dev_type, void *data)
	{
		return RESULT_OK;
	}
};

}}