// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Host.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"

#include "common/Console.h"
#include "common/SettingsInterface.h"
#include "common/WindowInfo.h"

#include "fmt/format.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <string>

#define PSXCLK 36864000 /* 36.864 Mhz */

namespace USB
{
	OHCIPort& GetOHCIPort(u32 port);

	static bool CreateDevice(u32 port);
	static void DestroyDevice(u32 port);
	static void UpdateDevice(u32 port);

	static bool DoOHCIState(StateWrapper& sw);
	static void DoEndpointState(USBEndpoint* ep, StateWrapper& sw);
	static void DoDeviceState(USBDevice* dev, StateWrapper& sw);
	static void DoPacketState(USBPacket* p, StateWrapper& sw, const std::array<bool, 2>& valid_devices);
} // namespace USB

static OHCIState* s_qemu_ohci = nullptr;
static USBDevice* s_usb_device[USB::NUM_PORTS] = {};
static const DeviceProxy* s_usb_device_proxy[USB::NUM_PORTS] = {};

static s64 s_usb_clocks = 0;
static s64 s_usb_remaining = 0;

int64_t g_usb_frame_time = 0;
int64_t g_usb_bit_time = 0;
int64_t g_usb_last_cycle = 0;

std::string USB::GetConfigSection(int port)
{
	return fmt::format("USB{}", port + 1);
}

OHCIPort& USB::GetOHCIPort(u32 port)
{
	// Apparently the ports on the hub are swapped.
	// Get this wrong and games like GT4 won't spin your wheelz.
	const u32 rhport = (port == 0) ? 1 : 0;
	return s_qemu_ohci->rhport[rhport];
}

bool USB::CreateDevice(u32 port)
{
	const Pcsx2Config::USBOptions::Port& portcfg = EmuConfig.USB.Ports[port];
	const DeviceProxy* proxy = (portcfg.DeviceType != DEVTYPE_NONE) ? RegisterDevice::instance().Device(portcfg.DeviceType) : nullptr;
	if (!proxy)
		return true;

	DevCon.WriteLn("(USB) Creating a %s in port %u", proxy->Name(), port + 1);
	USBDevice* dev;
	{
		auto lock = Host::GetSettingsLock();
		dev = proxy->CreateDevice(*Host::GetSettingsInterface(), port, portcfg.DeviceSubtype);
	}
	if (!dev)
	{
		Console.Error("Failed to create USB device in port %u (%s)", port + 1, proxy->Name());
		return false;
	}

	pxAssertRel(s_qemu_ohci, "Has OHCI");
	pxAssertRel(!GetOHCIPort(port).port.dev, "No device in OHCI when creating");
	GetOHCIPort(port).port.dev = dev;
	dev->attached = true;
	usb_attach(&GetOHCIPort(port).port);
	s_usb_device[port] = dev;
	s_usb_device_proxy[port] = proxy;
	return true;
}

void USB::DestroyDevice(u32 port)
{
	USBDevice* dev = s_usb_device[port];
	if (!dev)
		return;

	if (dev->klass.unrealize)
		dev->klass.unrealize(dev);
	GetOHCIPort(port).port.dev = nullptr;
	s_usb_device[port] = nullptr;
	s_usb_device_proxy[port] = nullptr;
}

void USB::UpdateDevice(u32 port)
{
	if (!s_usb_device[port])
		return;

	auto lock = Host::GetSettingsLock();
	s_usb_device_proxy[port]->UpdateSettings(s_usb_device[port], *Host::GetSettingsInterface());
}

void USBinit()
{
	RegisterDevice::Register();
}

void USBshutdown()
{
	RegisterDevice::instance().Unregister();
}

bool USBopen()
{
	s_qemu_ohci = ohci_create(0x1f801600, 2);
	if (!s_qemu_ohci)
		return false;

	s_usb_clocks = 0;
	s_usb_remaining = 0;
	g_usb_last_cycle = 0;

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
		USB::CreateDevice(port);

	return true;
}

void USBclose()
{
	for (u32 port = 0; port < USB::NUM_PORTS; port++)
		USB::DestroyDevice(port);

	free(s_qemu_ohci);
	s_qemu_ohci = nullptr;
}

void USBreset()
{
	s_usb_clocks = 0;
	s_usb_remaining = 0;
	g_usb_last_cycle = 0;
	ohci_hard_reset(s_qemu_ohci);
}

u8 USBread8(u32 addr)
{
	return 0;
}

u16 USBread16(u32 addr)
{
	return 0;
}

u32 USBread32(u32 addr)
{
	u32 hard;

	hard = ohci_mem_read(s_qemu_ohci, addr);


	return hard;
}

void USBwrite8(u32 addr, u8 value)
{
}

void USBwrite16(u32 addr, u16 value)
{
}

void USBwrite32(u32 addr, u32 value)
{
	ohci_mem_write(s_qemu_ohci, addr, value);
}

bool USB::DoOHCIState(StateWrapper& sw)
{
	if (!sw.DoMarker("USBOHCI"))
		return false;

	sw.Do(&g_usb_last_cycle);
	sw.Do(&s_usb_clocks);
	sw.Do(&s_usb_remaining);

	sw.Do(&s_qemu_ohci->eof_timer);
	sw.Do(&s_qemu_ohci->sof_time);

	sw.Do(&s_qemu_ohci->ctl);
	sw.Do(&s_qemu_ohci->status);
	sw.Do(&s_qemu_ohci->intr_status);
	sw.Do(&s_qemu_ohci->intr);

	sw.Do(&s_qemu_ohci->hcca);
	sw.Do(&s_qemu_ohci->ctrl_head);
	sw.Do(&s_qemu_ohci->ctrl_cur);
	sw.Do(&s_qemu_ohci->bulk_head);
	sw.Do(&s_qemu_ohci->bulk_cur);
	sw.Do(&s_qemu_ohci->per_cur);
	sw.Do(&s_qemu_ohci->done);
	sw.Do(&s_qemu_ohci->done_count);

	s_qemu_ohci->fsmps = sw.DoBitfield(s_qemu_ohci->fsmps);
	s_qemu_ohci->fit = sw.DoBitfield(s_qemu_ohci->fit);
	s_qemu_ohci->fi = sw.DoBitfield(s_qemu_ohci->fi);
	s_qemu_ohci->frt = sw.DoBitfield(s_qemu_ohci->frt);
	sw.Do(&s_qemu_ohci->frame_number);
	sw.Do(&s_qemu_ohci->padding);
	sw.Do(&s_qemu_ohci->pstart);
	sw.Do(&s_qemu_ohci->lst);

	sw.Do(&s_qemu_ohci->rhdesc_a);
	sw.Do(&s_qemu_ohci->rhdesc_b);
	for (u32 i = 0; i < OHCI_MAX_PORTS; i++)
		sw.Do(&s_qemu_ohci->rhport[i].ctrl);

	sw.Do(&s_qemu_ohci->old_ctl);
	sw.DoArray(s_qemu_ohci->usb_buf, sizeof(s_qemu_ohci->usb_buf));
	sw.Do(&s_qemu_ohci->async_td);
	sw.Do(&s_qemu_ohci->async_complete);
	return true;
}

void USB::DoDeviceState(USBDevice* dev, StateWrapper& sw)
{
	if (!sw.DoMarker("USBDevice"))
		return;

	sw.Do(&dev->speed);
	sw.Do(&dev->addr);
	sw.Do(&dev->state);
	sw.DoBytes(&dev->setup_buf, sizeof(dev->setup_buf));
	sw.DoBytes(&dev->data_buf, sizeof(dev->data_buf));
	sw.Do(&dev->remote_wakeup);
	sw.Do(&dev->setup_state);
	sw.Do(&dev->setup_len);
	sw.Do(&dev->setup_index);

	sw.Do(&dev->configuration);
	usb_desc_set_config(dev, dev->configuration);

	int altsetting[USB_MAX_INTERFACES];
	std::memcpy(altsetting, dev->altsetting, sizeof(altsetting));
	sw.DoPODArray(altsetting, std::size(altsetting));
	for (u32 i = 0; i < USB_MAX_INTERFACES; i++)
	{
		dev->altsetting[i] = altsetting[i];
		usb_desc_set_interface(dev, i, altsetting[i]);
	}

	DoEndpointState(&dev->ep_ctl, sw);
	for (u32 i = 0; i < USB_MAX_ENDPOINTS; i++)
		DoEndpointState(&dev->ep_in[i], sw);
	for (u32 i = 0; i < USB_MAX_ENDPOINTS; i++)
		DoEndpointState(&dev->ep_out[i], sw);
}

void USB::DoEndpointState(USBEndpoint* ep, StateWrapper& sw)
{
	// assumed the fields above are constant
	sw.Do(&ep->pipeline);
	sw.Do(&ep->halted);

	if (sw.IsReading())
	{
		// clear out all packets, we'll fill it in later
		while (!QTAILQ_EMPTY(&ep->queue))
			QTAILQ_REMOVE(&ep->queue, QTAILQ_FIRST(&ep->queue), queue);
	}
}

void USB::DoPacketState(USBPacket* p, StateWrapper& sw, const std::array<bool, 2>& valid_devices)
{
	if (!sw.DoMarker("USBPacket"))
		return;

	s32 dev_index = -1;
	s32 ep_index = -1;
	bool queued = false;
	if (sw.IsWriting())
	{
		USBEndpoint* ep = p->ep;
		if (ep)
		{
			for (u32 i = 0; i < NUM_PORTS; i++)
			{
				USBDevice* dev = s_usb_device[i];
				if (valid_devices[i] && ep->dev == dev)
				{
					dev_index = static_cast<s32>(i);
					if (ep == &dev->ep_ctl)
						ep_index = 0;
					else if (ep >= &dev->ep_in[0] && ep <= &dev->ep_in[USB_MAX_ENDPOINTS - 1])
						ep_index = static_cast<s32>(ep - &dev->ep_in[0]) + 1;
					else if (ep >= &dev->ep_out[0] && ep <= &dev->ep_out[USB_MAX_ENDPOINTS - 1])
						ep_index = static_cast<s32>(ep - &dev->ep_out[0]) + 1 + USB_MAX_ENDPOINTS;

					USBPacket* pp;
					QTAILQ_FOREACH(pp, &ep->queue, queue)
					{
						if (pp == p)
							queued = true;
					}

					break;
				}
			}
			if (dev_index < 0 || ep_index < 0)
				Console.Error("Failed to save USB packet from unknown endpoint");
		}
	}

	sw.Do(&dev_index);
	sw.Do(&ep_index);
	sw.Do(&p->buffer_size);
	sw.Do(&queued);

	sw.Do(&p->pid);
	sw.Do(&p->id);
	sw.Do(&p->stream);
	sw.Do(&p->parameter);
	sw.Do(&p->short_not_ok);
	sw.Do(&p->int_req);
	sw.Do(&p->status);
	sw.Do(&p->actual_length);
	sw.Do(&p->state);

	if (sw.IsReading())
	{
		p->ep = nullptr;

		if (dev_index >= 0 && ep_index >= 0 && valid_devices[static_cast<u32>(dev_index)])
		{
			USBDevice* dev = s_usb_device[static_cast<u32>(dev_index)];
			pxAssert(dev);

			p->buffer_ptr = (p->buffer_size > 0) ? s_qemu_ohci->usb_buf : nullptr;

			if (ep_index == 0)
				p->ep = &dev->ep_ctl;
			else if (ep_index < (1 + USB_MAX_ENDPOINTS))
				p->ep = &dev->ep_in[ep_index - 1];
			else if (ep_index < (1 + USB_MAX_ENDPOINTS + USB_MAX_ENDPOINTS))
				p->ep = &dev->ep_out[ep_index - 1 - USB_MAX_ENDPOINTS];

			if (p->ep && queued)
				QTAILQ_INSERT_TAIL(&p->ep->queue, p, queue);
		}
		else
		{
			p->buffer_ptr = nullptr;
			p->buffer_size = 0;
		}
	}
}

bool USB::DoState(StateWrapper& sw)
{
	std::array<bool, 2> valid_devices = {};

	if (sw.IsReading())
	{
		if (!sw.DoMarker("USB") || !USB::DoOHCIState(sw))
		{
			Console.Error("USB state is invalid, resetting.");
			USBreset();
			return true;
		}

		for (u32 port = 0; port < USB::NUM_PORTS; port++)
		{
			s32 state_devtype;
			u32 state_devsubtype;
			u32 state_size;
			sw.Do(&state_devtype);
			sw.Do(&state_devsubtype);
			sw.Do(&state_size);

			// this is *assuming* the config is correct... there's no reason it shouldn't be.
			if (sw.HasError() ||
				EmuConfig.USB.Ports[port].DeviceType != state_devtype ||
				EmuConfig.USB.Ports[port].DeviceSubtype != state_devsubtype ||
				(state_devtype != DEVTYPE_NONE && !s_usb_device[port]))
			{
				Console.Error("Save state has device type %u, but config has %u. Reattaching device.", state_devtype, EmuConfig.USB.Ports[port].DeviceType);
				if (s_usb_device[port])
					usb_reattach(&USB::GetOHCIPort(port).port);
				
				sw.SkipBytes(state_size);
				continue;
			}

			if (!s_usb_device[port])
			{
				// nothing in this port
				sw.SkipBytes(state_size);
				continue;
			}

			USB::DoDeviceState(s_usb_device[port], sw);

			if (!s_usb_device_proxy[port]->Freeze(s_usb_device[port], sw) || sw.HasError())
			{
				Console.Error("Failed to deserialize USB port %u, removing device.", port);
				USB::DestroyDevice(port);
				continue;
			}

			valid_devices[port] = true;
		}

		USB::DoPacketState(&s_qemu_ohci->usb_packet, sw, valid_devices);
		if (sw.HasError())
		{
			Console.WriteLn("Failed to read USB packet, resetting all devices.");
			USBreset();
			return true;
		}
	}
	else
	{
		if (!sw.DoMarker("USB") || !USB::DoOHCIState(sw))
			return false;

		for (u32 port = 0; port < USB::NUM_PORTS; port++)
		{
			s32 state_devtype = EmuConfig.USB.Ports[port].DeviceType;
			u32 state_devsubtype = EmuConfig.USB.Ports[port].DeviceSubtype;
			sw.Do(&state_devtype);
			sw.Do(&state_devsubtype);

			const u32 size_pos = sw.GetStream()->GetPosition();
			u32 state_size = 0;
			sw.Do(&state_size);

			if (sw.HasError())
				return false;

			if (!s_usb_device[port])
			{
				// nothing in this port
				continue;
			}

			const u32 start_pos = sw.GetStream()->GetPosition();
			USB::DoDeviceState(s_usb_device[port], sw);
			if (!s_usb_device_proxy[port]->Freeze(s_usb_device[port], sw) || sw.HasError())
			{
				Console.Error("Failed to serialize USB port %u.", port);
				return false;
			}

			const u32 end_pos = sw.GetStream()->GetPosition();
			state_size = end_pos - start_pos;
			if (!sw.GetStream()->SeekAbsolute(size_pos) || (sw.Do(&state_size), sw.HasError()) || !sw.GetStream()->SeekAbsolute(end_pos))
				return false;

			valid_devices[port] = true;
		}

		USB::DoPacketState(&s_qemu_ohci->usb_packet, sw, valid_devices);
		if (sw.HasError())
			return false;
	}

	return true;
}

void USBasync(u32 cycles)
{
	if (!s_usb_device[0] && !s_usb_device[1])
		return;

	s_usb_remaining += cycles;
	s_usb_clocks += s_usb_remaining;
	if (s_qemu_ohci->eof_timer > 0)
	{
		while ((uint64_t)s_usb_remaining >= s_qemu_ohci->eof_timer)
		{
			s_usb_remaining -= s_qemu_ohci->eof_timer;
			s_qemu_ohci->eof_timer = 0;
			ohci_frame_boundary(s_qemu_ohci);

			/*
			 * Break out of the loop if bus was stopped.
			 * If ohci_frame_boundary hits an UE, but doesn't stop processing,
			 * it seems to cause a hang inside the game instead.
			*/
			if (!s_qemu_ohci->eof_timer)
				break;
		}
		if ((s_usb_remaining > 0) && (s_qemu_ohci->eof_timer > 0))
		{
			s64 m = s_qemu_ohci->eof_timer;
			if (s_usb_remaining < m)
				m = s_usb_remaining;
			s_qemu_ohci->eof_timer -= m;
			s_usb_remaining -= m;
		}
	}
	//if(qemu_ohci->eof_timer <= 0)
	//{
	//ohci_frame_boundary(qemu_ohci);
	//}
}

int usb_get_ticks_per_second()
{
	return PSXCLK;
}

s64 usb_get_clock()
{
	return s_usb_clocks;
}

s32 USB::DeviceTypeNameToIndex(const std::string_view& device)
{
	RegisterDevice& rd = RegisterDevice::instance();
	return rd.Index(device);
}

const char* USB::DeviceTypeIndexToName(s32 device)
{
	RegisterDevice& rd = RegisterDevice::instance();
	const DeviceProxy* proxy = (device != DEVTYPE_NONE) ? rd.Device(device) : nullptr;
	return proxy ? proxy->TypeName() : TRANSLATE_NOOP("USB", "None");
}

std::vector<std::pair<const char*, const char*>> USB::GetDeviceTypes()
{
	RegisterDevice& rd = RegisterDevice::instance();
	std::vector<std::pair<const char*, const char*>> ret;
	ret.reserve(rd.Map().size() + 1);
	ret.emplace_back("None", TRANSLATE_NOOP("USB", "Not Connected"));
	for (const auto& it : rd.Map())
		ret.emplace_back(it.second->TypeName(), it.second->Name());
	return ret;
}

const char* USB::GetDeviceName(const std::string_view& device)
{
	const DeviceProxy* dev = RegisterDevice::instance().Device(device);
	return dev ? dev->Name() : TRANSLATE_NOOP("USB", "Not Connected");
}

const char* USB::GetDeviceSubtypeName(const std::string_view& device, u32 subtype)
{
	const DeviceProxy* dev = RegisterDevice::instance().Device(device);
	if (!dev)
		return "Unknown";

	const std::span<const char*> subtypes(dev->SubTypes());
	if (subtypes.empty() || subtype >= subtypes.size())
		return "";

	return subtypes[subtype];
}

std::span<const char*> USB::GetDeviceSubtypes(const std::string_view& device)
{
	const DeviceProxy* dev = RegisterDevice::instance().Device(device);
	return dev ? dev->SubTypes() : std::span<const char*>();
}

std::span<const InputBindingInfo> USB::GetDeviceBindings(const std::string_view& device, u32 subtype)
{
	const DeviceProxy* dev = RegisterDevice::instance().Device(device);
	return dev ? dev->Bindings(subtype) : std::span<const InputBindingInfo>();
}

std::span<const SettingInfo> USB::GetDeviceSettings(const std::string_view& device, u32 subtype)
{
	const DeviceProxy* dev = RegisterDevice::instance().Device(device);
	return dev ? dev->Settings(subtype) : std::span<const SettingInfo>();
}

std::span<const InputBindingInfo> USB::GetDeviceBindings(u32 port)
{
	pxAssert(port < NUM_PORTS);
	if (s_usb_device_proxy[port])
		return s_usb_device_proxy[port]->Bindings(EmuConfig.USB.Ports[port].DeviceSubtype);
	else
		return {};
}

float USB::GetDeviceBindValue(u32 port, u32 bind_index)
{
	pxAssert(port < NUM_PORTS);
	if (!s_usb_device[port])
		return 0.0f;

	return s_usb_device_proxy[port]->GetBindingValue(s_usb_device[port], bind_index);
}

void USB::SetDeviceBindValue(u32 port, u32 bind_index, float value)
{
	pxAssert(port < NUM_PORTS);
	if (!s_usb_device[port])
		return;

	s_usb_device_proxy[port]->SetBindingValue(s_usb_device[port], bind_index, value);
}

void USB::InputDeviceConnected(const std::string_view& identifier)
{
	for (u32 i = 0; i < NUM_PORTS; i++)
	{
		if (s_usb_device[i])
			s_usb_device_proxy[i]->InputDeviceConnected(s_usb_device[i], identifier);
	}
}

void USB::InputDeviceDisconnected(const std::string_view& identifier)
{
	for (u32 i = 0; i < NUM_PORTS; i++)
	{
		if (s_usb_device[i])
			s_usb_device_proxy[i]->InputDeviceDisconnected(s_usb_device[i], identifier);
	}
}

std::string USB::GetConfigDevice(const SettingsInterface& si, u32 port)
{
	return si.GetStringValue(GetConfigSection(port).c_str(), "Type", "None");
}

void USB::SetConfigDevice(SettingsInterface& si, u32 port, const char* devname)
{
	si.SetStringValue(GetConfigSection(port).c_str(), "Type", devname);
}

u32 USB::GetConfigSubType(const SettingsInterface& si, u32 port, const std::string_view& devname)
{
	return si.GetUIntValue(GetConfigSection(port).c_str(), fmt::format("{}_subtype", devname).c_str(), 0u);
}

void USB::SetConfigSubType(SettingsInterface& si, u32 port, const std::string_view& devname, u32 subtype)
{
	si.SetUIntValue(GetConfigSection(port).c_str(), fmt::format("{}_subtype", devname).c_str(), subtype);
}

std::string USB::GetConfigSubKey(const std::string_view& device, const std::string_view& bind_name)
{
	return fmt::format("{}_{}", device, bind_name);
}

bool USB::ConfigKeyExists(SettingsInterface& si, u32 port, const char* devname, const char* key)
{
	const std::string real_key(fmt::format("{}_{}", devname, key));
	return si.ContainsValue(GetConfigSection(port).c_str(), real_key.c_str());
}

bool USB::GetConfigBool(SettingsInterface& si, u32 port, const char* devname, const char* key, bool default_value)
{
	const std::string real_key(fmt::format("{}_{}", devname, key));
	return si.GetBoolValue(GetConfigSection(port).c_str(), real_key.c_str(), default_value);
}

s32 USB::GetConfigInt(SettingsInterface& si, u32 port, const char* devname, const char* key, s32 default_value)
{
	const std::string real_key(fmt::format("{}_{}", devname, key));
	return si.GetIntValue(GetConfigSection(port).c_str(), real_key.c_str(), default_value);
}

float USB::GetConfigFloat(SettingsInterface& si, u32 port, const char* devname, const char* key, float default_value)
{
	const std::string real_key(fmt::format("{}_{}", devname, key));
	return si.GetFloatValue(GetConfigSection(port).c_str(), real_key.c_str(), default_value);
}


std::string USB::GetConfigString(SettingsInterface& si, u32 port, const char* devname, const char* key, const char* default_value /*= ""*/)
{
	const std::string real_key(fmt::format("{}_{}", devname, key));
	return si.GetStringValue(GetConfigSection(port).c_str(), real_key.c_str(), default_value);
}


static u32 TryMapGenericMapping(SettingsInterface& si, const std::string& section, const std::string& type,
	const std::vector<std::pair<GenericInputBinding, std::string>>& mapping, GenericInputBinding generic_name,
	const char* bind_name)
{
	// find the mapping it corresponds to
	const std::string* found_mapping = nullptr;
	for (const std::pair<GenericInputBinding, std::string>& it : mapping)
	{
		if (it.first == generic_name)
		{
			found_mapping = &it.second;
			break;
		}
	}

	const std::string key(USB::GetConfigSubKey(type, bind_name));
	if (found_mapping)
	{
		Console.WriteLn("(MapDevice) Map %s/%s to '%s'", section.c_str(), bind_name, found_mapping->c_str());
		si.SetStringValue(section.c_str(), key.c_str(), found_mapping->c_str());
		return 1;
	}
	else
	{
		si.DeleteValue(section.c_str(), key.c_str());
		return 0;
	}
}

bool USB::MapDevice(SettingsInterface& si, u32 port, const std::vector<std::pair<GenericInputBinding, std::string>>& mapping)
{
	const std::string section(GetConfigSection(port));
	const std::string type(GetConfigDevice(si, port));
	const u32 subtype = GetConfigSubType(si, port, type);
	const DeviceProxy* dev = RegisterDevice::instance().Device(type);
	if (!dev)
		return false;

	u32 num_mappings = 0;
	for (const InputBindingInfo& bi : dev->Bindings(subtype))
	{
		if (bi.generic_mapping == GenericInputBinding::Unknown)
			continue;

		num_mappings += TryMapGenericMapping(si, section, type, mapping, bi.generic_mapping, bi.name);
	}

	return (num_mappings > 0);
}

void USB::ClearPortBindings(SettingsInterface& si, u32 port)
{
	const std::string section = GetConfigSection(port);
	const std::string type = GetConfigDevice(si, port);
	const u32 subtype = GetConfigSubType(si, port, type);
	const DeviceProxy* dev = RegisterDevice::instance().Device(type);
	if (!dev)
		return;

	for (const InputBindingInfo& bi : dev->Bindings(subtype))
		si.DeleteValue(section.c_str(), GetConfigSubKey(type, bi.name).c_str());
}

void USB::CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si,
	bool copy_devices, bool copy_bindings)
{
	for (u32 port = 0; port < NUM_PORTS; port++)
	{
		const std::string section = GetConfigSection(port);
		const std::string type = GetConfigDevice(src_si, port);
		const u32 subtype = GetConfigSubType(src_si, port, type);
		const DeviceProxy* dev = RegisterDevice::instance().Device(type);

		if (copy_devices)
		{
			dest_si->CopyStringValue(src_si, section.c_str(), "Type");
			if (dev)
			{
				dest_si->CopyUIntValue(src_si, section.c_str(), fmt::format("{}_subtype", type).c_str());

				for (const SettingInfo& si : dev->Settings(subtype))
					si.CopyValue(dest_si, src_si, section.c_str(), GetConfigSubKey(type, si.name).c_str());
			}
		}

		if (copy_bindings && dev)
		{
			for (const InputBindingInfo& bi : dev->Bindings(subtype))
				dest_si->CopyStringValue(src_si, section.c_str(), GetConfigSubKey(type, bi.name).c_str());
		}
	}
}

void USB::SetDefaultConfiguration(SettingsInterface* si)
{
	for (u32 port = 0; port < NUM_PORTS; port++)
	{
		const std::string section = GetConfigSection(port);

		si->ClearSection(section.c_str());
		si->SetStringValue(section.c_str(), "Type", "None");
	}
}

void USB::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	static_assert(Pcsx2Config::USBOptions::NUM_PORTS == NUM_PORTS);

	for (u32 port = 0; port < NUM_PORTS; port++)
	{
		if (EmuConfig.USB.Ports[port] == old_config.USB.Ports[port])
		{
			UpdateDevice(port);
			continue;
		}

		if (s_usb_device[port])
			DestroyDevice(port);
		CreateDevice(port);
	}
}
