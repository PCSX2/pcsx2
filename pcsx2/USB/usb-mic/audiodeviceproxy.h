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

#ifndef AUDIODEVICEPROXY_H
#define AUDIODEVICEPROXY_H
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <iterator>
#include "USB/helpers.h"
#include "USB/configuration.h"
#include "USB/deviceproxy.h"
#include "audiodev.h"

namespace usb_mic
{

	class AudioDeviceError : public std::runtime_error
	{
	public:
		AudioDeviceError(const char* msg)
			: std::runtime_error(msg)
		{
		}
		virtual ~AudioDeviceError() throw() {}
	};

	class AudioDeviceProxyBase : public ProxyBase
	{
		AudioDeviceProxyBase(const AudioDeviceProxyBase&) = delete;
		AudioDeviceProxyBase& operator=(const AudioDeviceProxyBase&) = delete;

	public:
		AudioDeviceProxyBase(){};
		AudioDeviceProxyBase(const std::string& name);
		virtual AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const = 0; //Can be generalized? Probably not
		virtual void AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir) const = 0;
		virtual bool AudioInit() = 0;
		virtual void AudioDeinit() = 0;
	};

	template <class T>
	class AudioDeviceProxy : public AudioDeviceProxyBase
	{
		AudioDeviceProxy(const AudioDeviceProxy&) = delete;

	public:
		AudioDeviceProxy() {}
		AudioDeviceProxy(const std::string& name)
			: AudioDeviceProxyBase(name)
		{
		} //Why can't it automagically, ugh
		~AudioDeviceProxy() { }

		AudioDevice* CreateObject(int port, const char* dev_type, int mic, AudioDir dir) const
		{
			try
			{
				return new T(port, dev_type, mic, dir);
			}
			catch (AudioDeviceError& err)
			{
				(void)err;
				return nullptr;
			}
		}
		virtual const TCHAR* Name() const
		{
			return T::Name();
		}
		virtual int Configure(int port, const char* dev_type, void* data)
		{
			return T::Configure(port, dev_type, data);
		}
		virtual void AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir dir) const
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
} // namespace usb_mic
#endif
