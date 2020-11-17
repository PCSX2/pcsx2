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

#include <stdexcept>
#include <cstdlib>
#include <string>
#include <cerrno>
#include <cassert>

#include "PrecompiledHeader.h"
#include "Utilities/pxStreams.h"
#include "USB.h"
#include "qemu-usb/USBinternal.h"
#include "qemu-usb/desc.h"
#include "shared/shared_usb.h"
#include "deviceproxy.h"
#include "configuration.h"

#define PSXCLK 36864000 /* 36.864 Mhz */

OHCIState* qemu_ohci = NULL;
USBDevice* usb_device[2] = {NULL};
bool configChanged = false;
static bool usb_opened = false;

Config conf;
// we'll probably switch our save state system at some point to standardize in
// the core anyways
char USBfreezeID[] = "govqemUSB1";
typedef struct
{
	char freezeID[11];
	s64 cycles;
	s64 remaining;
	OHCIState t;
	struct
	{
		DeviceType index;
		u32 size;
		USBDevice dev;
	} device[2];

	struct usb_packet
	{
		USBEndpoint ep; //usb packet endpoint
		int dev_index;
		int data_size;
	} usb_packet;
} USBfreezeData;

u8* ram = 0;
FILE* usbLog;
int64_t usb_frame_time;
int64_t usb_bit_time;

s64 clocks = 0;
s64 remaining = 0;

#if _WIN32
HWND gsWnd = nullptr;
#elif defined(__linux__)
#include "gtk.h"
#include <gdk/gdkx.h>
#include <X11/X.h>
Display* g_GSdsp;
Window g_GSwin;
#endif

Config::Config()
	: Log(0)
{
	memset(&WheelType, 0, sizeof(WheelType));
}

//Simpler to reset and reattach after USBclose/USBopen
void Reset()
{
	if (qemu_ohci)
		ohci_hard_reset(qemu_ohci);
}

void OpenDevice(int port)
{
	//TODO Pass pDsp to open probably so dinput can bind to this HWND
	if (usb_device[port] && usb_device[port]->klass.open)
		usb_device[port]->klass.open(usb_device[port] /*, pDsp*/);
}

static void CloseDevice(int port)
{
	if (usb_device[port] && usb_device[port]->klass.close)
		usb_device[port]->klass.close(usb_device[port]);
}

void DestroyDevice(int port)
{
	if (qemu_ohci && qemu_ohci->rhport[port].port.dev)
	{
		qemu_ohci->rhport[port].port.dev->klass.unrealize(qemu_ohci->rhport[port].port.dev);
		qemu_ohci->rhport[port].port.dev = nullptr;
	}
	else if (usb_device[port])
		usb_device[port]->klass.unrealize(usb_device[port]);

	usb_device[port] = nullptr;
}

void DestroyDevices()
{
	for (int i = 0; i < 2; i++)
	{
		CloseDevice(i);
		DestroyDevice(i);
	}
}

static USBDevice* CreateDevice(DeviceType index, int port)
{
	USBDevice* device = nullptr;

	if (index == DEVTYPE_NONE)
		return nullptr;

	DeviceProxyBase* devProxy = RegisterDevice::instance().Device(index);
	if (devProxy)
		device = devProxy->CreateDevice(port);
	else
		Console.WriteLn(Color_Red, "Device %d: Unknown device type", 1 - port);

	if (!device)
	{
	}
	return device;
}

//TODO re-do sneaky attach
static void USBAttach(int port, USBDevice* dev, bool sneaky = false)
{
	if (!qemu_ohci)
		return;

	USBDevice* tmp = qemu_ohci->rhport[port].port.dev;
	if (tmp)
	{
		if (!sneaky)
			usb_detach(&qemu_ohci->rhport[port].port);
		tmp->klass.unrealize(tmp);
	}

	qemu_ohci->rhport[port].port.dev = dev;
	if (dev)
	{
		dev->attached = true;
		usb_attach(&qemu_ohci->rhport[port].port); //.ops->attach(&(qemu_ohci->rhport[port].port));
	}
}

static USBDevice* CreateDevice(const std::string& name, int port)
{
	USBDevice* device = nullptr;

	if (!name.empty())
	{
		DeviceProxyBase* devProxy = RegisterDevice::instance().Device(name);
		if (devProxy)
			device = devProxy->CreateDevice(port);
		else
			Console.WriteLn(Color_Red, "Port %d: Unknown device type", port);
	}

	return device;
}

void CreateDevices()
{
	if (!qemu_ohci)
		return; //No USBinit yet ie. called from config. dialog
	DestroyDevices();

	for (int i = 0; i < 2; i++)
	{
		usb_device[i] = CreateDevice(conf.Port[i], i);
		USBAttach(i, usb_device[i]);
		if (usb_opened)
			OpenDevice(i);
	}
}

s32 USBinit()
{
	USBsetSettingsDir();

	RegisterDevice::Register();
	LoadConfig();

	if (conf.Log && !usbLog)
	{
#ifdef _WIN32
		usbLog = wfopen(LogDir.c_str(), L"wb"); // L"wb,ccs=UNICODE");
#else
		usbLog = wfopen(LogDir.c_str(), "wb"); // L"wb,ccs=UNICODE");
#endif
		//if(usbLog) setvbuf(usbLog, NULL,  _IONBF, 0);
	}

	qemu_ohci = ohci_create(0x1f801600, 2);
	if (!qemu_ohci)
		return 1;

	clocks = 0;
	remaining = 0;

	return 0;
}

void USBshutdown()
{

	DestroyDevices();
	RegisterDevice::instance().Unregister();

	free(qemu_ohci);

	ram = 0;

	//#ifdef _DEBUG
	if (conf.Log && usbLog)
	{
		fclose(usbLog);
		usbLog = nullptr;
	}
	//#endif
	usb_opened = false;
}

s32 USBopen(void* pDsp)
{

	if (conf.Log && !usbLog)
	{
		usbLog = fopen("logs/usbLog.txt", "a");
		//if(usbLog) setvbuf(usbLog, NULL,  _IONBF, 0);
	}

#if _WIN32

	HWND hWnd = (HWND)pDsp;
	//HWND hWnd=(HWND)((uptr*)pDsp)[0];

	if (!IsWindow(hWnd))
		hWnd = *(HWND*)hWnd;

	if (!IsWindow(hWnd))
		hWnd = NULL;
	else
	{
		while (GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD)
			hWnd = GetParent(hWnd);
	}
	gsWnd = hWnd;
	pDsp = gsWnd;
#elif defined(__linux__)

	g_GSdsp = (Display*)((uptr*)pDsp)[0];
	g_GSwin = (Window)((uptr*)pDsp)[1];
#endif

	try
	{
		shared::Initialize(pDsp);
	}
	catch (std::runtime_error& e)
	{
		Console.WriteLn(Color_Red, "USB: %s", e.what());
	}

	if (configChanged || (!usb_device[0] && !usb_device[1]))
	{
		configChanged = false;
		CreateDevices(); //TODO Pass pDsp to init?
	}

	OpenDevice(0 /*, pDsp */);
	OpenDevice(1 /*, pDsp */);
	usb_opened = true;
	return 0;
}

void USBclose()
{
	CloseDevice(0);
	CloseDevice(1);
	shared::Uninitialize();
	usb_opened = false;
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

	hard = ohci_mem_read(qemu_ohci, addr);


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
	ohci_mem_write(qemu_ohci, addr, value);
}

extern u32 bits;

void USBsetRAM(void* mem)
{
	ram = (u8*)mem;
	Reset();
}

s32 USBfreeze(int mode, freezeData* data)
{
	USBfreezeData usbd = {0};

	//TODO FREEZE_SIZE mismatch causes loading to fail in PCSX2 beforehand
	if (mode == FREEZE_LOAD)
	{
		if ((long unsigned int)data->size < sizeof(USBfreezeData))
		{
			Console.WriteLn(Color_Red, "USB: Unable to load freeze data! Got %d bytes, expected >= %zu.\n", data->size, sizeof(USBfreezeData));
			return -1;
		}

		usbd = *(USBfreezeData*)data->data;
		usbd.freezeID[10] = 0;

		if (strcmp(usbd.freezeID, USBfreezeID) != 0)
		{
			Console.WriteLn(Color_Red, "USB: Unable to load freeze data! Found ID %s, expected ID %s.\n", usbd.freezeID, USBfreezeID);
			return -1;
		}

		s8* ptr = data->data + sizeof(USBfreezeData);
		// Load the state of the attached devices
		if ((long unsigned int)data->size < sizeof(USBfreezeData) + usbd.device[0].size + usbd.device[1].size + 8192)
			return -1;

		//TODO Subsequent save state loadings make USB "stall" for n seconds since previous load
		//clocks = usbd.cycles;
		//remaining = usbd.remaining;

		CloseDevice(0);
		CloseDevice(1);

		for (uint32_t i = 0; i < qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = qemu_ohci;
			usbd.t.rhport[i].port.ops = qemu_ohci->rhport[i].port.ops;
			usbd.t.rhport[i].port.dev = qemu_ohci->rhport[i].port.dev;
		}
		//if (qemu_ohci->usb_packet.iov.iov)
		usb_packet_cleanup(&qemu_ohci->usb_packet);
		*qemu_ohci = usbd.t;
		// restore USBPacket for OHCIState
		usb_packet_init(&qemu_ohci->usb_packet);

		RegisterDevice& regInst = RegisterDevice::instance();
		for (int i = 0; i < 2; i++)
		{
			auto index = regInst.Index(conf.Port[i]);
			auto proxy = regInst.Device(index);

			//TODO FREEZE_SIZE mismatch causes loading to fail in PCSX2 beforehand
			// but just in case, recreate the same device type as was saved
			if (usbd.device[i].index != index)
			{
				index = usbd.device[i].index;
				DestroyDevice(i);
				conf.Port[i].clear();

				proxy = regInst.Device(index);
				if (proxy)
				{
					// re-create with saved device type
					conf.Port[i] = proxy->TypeName();
					usb_device[i] = CreateDevice(index, i);
					USBAttach(i, usb_device[i], index != DEVTYPE_MSD);
				}
			}

			if (proxy && usb_device[i]) /* usb device creation may have failed for some reason */
			{
				if (proxy->Freeze(FREEZE_SIZE, usb_device[i], nullptr) != (s32)usbd.device[i].size)
				{
					Console.WriteLn(Color_Red, "USB: Port %d: device's freeze size doesn't match.\n", 1 + (1 - i));
					return -1;
				}

				const USBDevice& tmp = usbd.device[i].dev;

				usb_device[i]->addr = tmp.addr;
				usb_device[i]->attached = tmp.attached;
				usb_device[i]->auto_attach = tmp.auto_attach;
				usb_device[i]->configuration = tmp.configuration;
				usb_device[i]->ninterfaces = tmp.ninterfaces;
				usb_device[i]->flags = tmp.flags;
				usb_device[i]->state = tmp.state;
				usb_device[i]->remote_wakeup = tmp.remote_wakeup;
				usb_device[i]->setup_state = tmp.setup_state;
				usb_device[i]->setup_len = tmp.setup_len;
				usb_device[i]->setup_index = tmp.setup_index;

				memcpy(usb_device[i]->data_buf, tmp.data_buf, sizeof(tmp.data_buf));
				memcpy(usb_device[i]->setup_buf, tmp.setup_buf, sizeof(tmp.setup_buf));

				usb_desc_set_config(usb_device[i], tmp.configuration);
				for (int k = 0; k < 16; k++)
				{
					usb_device[i]->altsetting[k] = tmp.altsetting[k];
					usb_desc_set_interface(usb_device[i], k, tmp.altsetting[k]);
				}

				proxy->Freeze(FREEZE_LOAD, usb_device[i], ptr);
				if (!usb_device[i]->attached)
				{ // FIXME FREEZE_SAVE fcked up
					usb_device[i]->attached = true;
					usb_device_reset(usb_device[i]);
					//TODO reset port if save state's and configured wheel types are different
					usb_detach(&qemu_ohci->rhport[i].port);
					usb_attach(&qemu_ohci->rhport[i].port);
				}
				OpenDevice(i);
			}
			else if (!proxy && index != DEVTYPE_NONE)
			{
				Console.WriteLn(Color_Red, "USB: Port %d: unknown device.\nPlugin is probably too old for this save.", i);
			}
			ptr += usbd.device[i].size;
		}

		int dev_index = usbd.usb_packet.dev_index;

		if (usb_device[dev_index])
		{
			USBPacket* p = &qemu_ohci->usb_packet;
			p->actual_length = usbd.usb_packet.data_size;

			QEMUIOVector* iov = p->combined ? &p->combined->iov : &p->iov;
			iov_from_buf(iov->iov, iov->niov, 0, ptr, p->actual_length);

			if (usbd.usb_packet.ep.pid == USB_TOKEN_SETUP)
			{
				if (usb_device[dev_index]->ep_ctl.ifnum == usbd.usb_packet.ep.ifnum)
					qemu_ohci->usb_packet.ep = &usb_device[dev_index]->ep_ctl;
			}
			else
			{
				USBEndpoint* eps = nullptr;
				if (usbd.usb_packet.ep.pid == USB_TOKEN_IN)
					eps = usb_device[dev_index]->ep_in;
				else //if (usbd.ep.pid == USB_TOKEN_OUT)
					eps = usb_device[dev_index]->ep_out;

				for (int k = 0; k < USB_MAX_ENDPOINTS; k++)
				{

					if (usbd.usb_packet.ep.type == eps[k].type && usbd.usb_packet.ep.nr == eps[k].nr && usbd.usb_packet.ep.ifnum == eps[k].ifnum && usbd.usb_packet.ep.pid == eps[k].pid)
					{
						qemu_ohci->usb_packet.ep = &eps[k];
						break;
					}
				}
			}
		}
		else
		{
			return -1;
		}

	}
	//TODO straight copying of structs can break cross-platform/cross-compiler save states 'cause padding 'n' stuff
	else if (mode == FREEZE_SAVE)
	{
		memset(data->data, 0, data->size); //maybe it already is...
		RegisterDevice& regInst = RegisterDevice::instance();
		usbd.usb_packet.dev_index = -1;

		for (int i = 0; i < 2; i++)
		{
			//TODO check that current created usb device and conf.Port[n] are the same
			auto index = regInst.Index(conf.Port[i]);
			auto proxy = regInst.Device(index);
			usbd.device[i].index = index;

			if (proxy && usb_device[i])
				usbd.device[i].size = proxy->Freeze(FREEZE_SIZE, usb_device[i], nullptr);
			else
				usbd.device[i].size = 0;

			if (qemu_ohci->usb_packet.ep && qemu_ohci->usb_packet.ep->dev == usb_device[i])
				usbd.usb_packet.dev_index = i;
		}

		strncpy(usbd.freezeID, USBfreezeID, strlen(USBfreezeID));
		usbd.t = *qemu_ohci;
		usbd.t.usb_packet.iov = {};
		usbd.t.usb_packet.ep = nullptr;
		if (qemu_ohci->usb_packet.ep)
			usbd.usb_packet.ep = *qemu_ohci->usb_packet.ep;

		for (uint32_t i = 0; i < qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = nullptr;
			usbd.t.rhport[i].port.ops = nullptr;
			usbd.t.rhport[i].port.dev = nullptr;
		}

		usbd.cycles = clocks;
		usbd.remaining = remaining;

		s8* ptr = data->data + sizeof(USBfreezeData);

		// Save the state of the attached devices
		for (int i = 0; i < 2; i++)
		{
			auto proxy = regInst.Device(conf.Port[i]);
			if (usb_device[i])
			{
				usbd.device[i].dev = *usb_device[i];
				if (proxy && usbd.device[i].size)
					proxy->Freeze(FREEZE_SAVE, usb_device[i], ptr);
			}
			memset(&usbd.device[i].dev.klass, 0, sizeof(USBDeviceClass));

			ptr += usbd.device[i].size;
		}

		USBPacket* p = &qemu_ohci->usb_packet;
		usbd.usb_packet.data_size = p->actual_length;
		QEMUIOVector* iov = p->combined ? &p->combined->iov : &p->iov;
		iov_to_buf(iov->iov, iov->niov, 0, ptr, p->actual_length);

		*(USBfreezeData*)data->data = usbd;
	}
	else if (mode == FREEZE_SIZE)
	{
		data->size = 0x10000;
	}

	return 0;
}

void USBasync(u32 cycles)
{
	remaining += cycles;
	clocks += remaining;
	if (qemu_ohci->eof_timer > 0)
	{
		while ((uint64_t)remaining >= qemu_ohci->eof_timer)
		{
			remaining -= qemu_ohci->eof_timer;
			qemu_ohci->eof_timer = 0;
			ohci_frame_boundary(qemu_ohci);

			/*
			 * Break out of the loop if bus was stopped.
			 * If ohci_frame_boundary hits an UE, but doesn't stop processing,
			 * it seems to cause a hang inside the game instead.
			*/
			if (!qemu_ohci->eof_timer)
				break;
		}
		if ((remaining > 0) && (qemu_ohci->eof_timer > 0))
		{
			s64 m = qemu_ohci->eof_timer;
			if (remaining < m)
				m = remaining;
			qemu_ohci->eof_timer -= m;
			remaining -= m;
		}
	}
	//if(qemu_ohci->eof_timer <= 0)
	//{
	//ohci_frame_boundary(qemu_ohci);
	//}
}

int cpu_physical_memory_rw(u32 addr, u8* buf, size_t len, int is_write)
{
	// invalid address, reset and try again
	if ((u64)addr + len >= 0x200000)
	{
		if (qemu_ohci)
			ohci_soft_reset(qemu_ohci);
		return 1;
	}

	if (is_write)
		memcpy(&(ram[addr]), buf, len);
	else
		memcpy(buf, &(ram[addr]), len);
	return 0;
}

int get_ticks_per_second()
{
	return PSXCLK;
}

s64 get_clock()
{
	return clocks;
}

void USBDoFreezeOut(void* dest)
{
	freezeData fP = {0, (s8*)dest};
	if (USBfreeze(FREEZE_SIZE, &fP) != 0)
		return;
	if (!fP.size)
		return;

	Console.Indent().WriteLn("Saving USB");

	if (USBfreeze(FREEZE_SAVE, &fP) != 0)
		throw std::runtime_error(" * USB: Error saving state!\n");
}


void USBDoFreezeIn(pxInputStream& infp)
{
	freezeData fP = {(int)infp.Length(), nullptr};
	//if (USBfreeze(FREEZE_SIZE, &fP) != 0)
	//	fP.size = 0;

	Console.Indent().WriteLn("Loading USB");

	if (!infp.IsOk() || !infp.Length())
	{
		// no state data to read, but USB expects some state data?
		// Issue a warning to console...
		if (fP.size != 0)
			Console.Indent().Warning("Warning: No data for USB found. Status may be unpredictable.");

		return;

		// Note: Size mismatch check could also be done here on loading, but
		// some plugins may have built-in version support for non-native formats or
		// older versions of a different size... or could give different sizes depending
		// on the status of the plugin when loading, so let's ignore it.
	}

	ScopedAlloc<s8> data(fP.size);
	fP.data = data.GetPtr();

	infp.Read(fP.data, fP.size);
	//if (USBfreeze(FREEZE_LOAD, &fP) != 0)
	//	throw std::runtime_error(" * USB: Error loading state!\n");
	USBfreeze(FREEZE_LOAD, &fP);
}
