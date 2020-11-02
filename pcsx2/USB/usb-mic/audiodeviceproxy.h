#ifndef AUDIODEVICEPROXY_H
#define AUDIODEVICEPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "../helpers.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../osdebugout.h"
#include "audiodev.h"

namespace usb_mic {

class AudioDeviceError : public std::runtime_error
{
public:
	AudioDeviceError(const char* msg) : std::runtime_error(msg) {}
	virtual ~AudioDeviceError() throw () {}
};

class AudioDeviceProxyBase : public ProxyBase
{
	AudioDeviceProxyBase(const AudioDeviceProxyBase&) = delete;
	AudioDeviceProxyBase& operator=(const AudioDeviceProxyBase&) = delete;

	public:
	AudioDeviceProxyBase() {};
	AudioDeviceProxyBase(const std::string& name);
	virtual AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const = 0; //Can be generalized? Probably not
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir) const = 0;
	virtual bool AudioInit() = 0;
	virtual void AudioDeinit() = 0;
};

template <class T>
class AudioDeviceProxy : public AudioDeviceProxyBase
{
	AudioDeviceProxy(const AudioDeviceProxy&) = delete;

	public:
	AudioDeviceProxy() {}
	AudioDeviceProxy(const std::string& name): AudioDeviceProxyBase(name) {} //Why can't it automagically, ugh
	~AudioDeviceProxy() { OSDebugOut(TEXT("%p\n"), this); }

	AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const
	{
		try
		{
			return new T(port, dev_type, mic, dir);
		}
		catch(AudioDeviceError& err)
		{
			OSDebugOut(TEXT("AudioDevice port %d mic %d: %") TEXT(SFMTs) TEXT("\n"), port, mic, err.what());
			(void)err;
			return nullptr;
		}
	}
	virtual const TCHAR* Name() const
	{
		return T::Name();
	}
	virtual int Configure(int port, const char* dev_type, void *data)
	{
		return T::Configure(port, dev_type, data);
	}
	virtual void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir dir) const
	{
		T::AudioDevices(devices, dir);
	}
	virtual bool AudioInit()
	{
		return T::AudioInit();
	}
	virtual void AudioDeinit()
	{
		T::AudioDeinit();
	}
};

class RegisterAudioDevice : public RegisterProxy<AudioDeviceProxyBase>
{
	public:
	static void Register();
};
}
#endif