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
#include "deviceproxy.h"
#include "usb-eyetoy/usb-eyetoy-webcam.h"
#include "usb-hid/usb-hid.h"
#include "usb-mic/usb-headset.h"
#include "usb-mic/usb-mic-singstar.h"
#include "usb-msd/usb-msd.h"
#include "usb-pad/usb-pad.h"
#include "usb-printer/usb-printer.h"

void RegisterDevice::Register()
{
	auto& inst = RegisterDevice::instance();
	if (inst.Map().size()) // FIXME Don't clear proxies, singstar keeps a copy to uninit audio
		return;
	inst.Add(DEVTYPE_PAD, new DeviceProxy<usb_pad::PadDevice>());
	inst.Add(DEVTYPE_MSD, new DeviceProxy<usb_msd::MsdDevice>());
	inst.Add(DEVTYPE_SINGSTAR, new DeviceProxy<usb_mic::SingstarDevice>());
	inst.Add(DEVTYPE_LOGITECH_MIC, new DeviceProxy<usb_mic::LogitechMicDevice>());
	inst.Add(DEVTYPE_LOGITECH_HEADSET, new DeviceProxy<usb_mic::HeadsetDevice>());
	inst.Add(DEVTYPE_HIDKBD, new DeviceProxy<usb_hid::HIDKbdDevice>());
	inst.Add(DEVTYPE_HIDMOUSE, new DeviceProxy<usb_hid::HIDMouseDevice>());
	inst.Add(DEVTYPE_RBKIT, new DeviceProxy<usb_pad::RBDrumKitDevice>());
	inst.Add(DEVTYPE_BUZZ, new DeviceProxy<usb_pad::BuzzDevice>());
#ifdef _WIN32
	inst.Add(DEVTYPE_GAMETRAK, new DeviceProxy<usb_pad::GametrakDevice>());
	inst.Add(DEVTYPE_REALPLAY, new DeviceProxy<usb_pad::RealPlayDevice>());
#endif
	inst.Add(DEVTYPE_EYETOY, new DeviceProxy<usb_eyetoy::EyeToyWebCamDevice>());
	inst.Add(DEVTYPE_BEATMANIA_DADADA, new DeviceProxy<usb_hid::BeatManiaDevice>());
	inst.Add(DEVTYPE_SEGA_SEAMIC, new DeviceProxy<usb_pad::SeamicDevice>());
	inst.Add(DEVTYPE_PRINTER, new DeviceProxy<usb_printer::PrinterDevice>());
	inst.Add(DEVTYPE_KEYBOARDMANIA, new DeviceProxy<usb_pad::KeyboardmaniaDevice>());

	RegisterAPIs();
}

void RegisterDevice::Unregister()
{
	/*for (auto& i: registerDeviceMap)
		delete i.second;*/
	registerDeviceMap.clear();
	delete registerDevice;
	registerDevice = nullptr;

	UnregisterAPIs();
}
