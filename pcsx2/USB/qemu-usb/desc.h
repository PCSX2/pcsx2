// SPDX-FileCopyrightText: ?? QEMU, 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <cstdint>
#include <vector>

/* binary representation */
#pragma pack(push, 1)
typedef struct USBDescriptor
{
	u8 bLength;
	u8 bDescriptorType;
	union
	{
		struct
		{
			u8 bcdUSB_lo;
			u8 bcdUSB_hi;
			u8 bDeviceClass;
			u8 bDeviceSubClass;
			u8 bDeviceProtocol;
			u8 bMaxPacketSize0;
			u8 idVendor_lo;
			u8 idVendor_hi;
			u8 idProduct_lo;
			u8 idProduct_hi;
			u8 bcdDevice_lo;
			u8 bcdDevice_hi;
			u8 iManufacturer;
			u8 iProduct;
			u8 iSerialNumber;
			u8 bNumConfigurations;
		} device;
		struct
		{
			u8 bcdUSB_lo;
			u8 bcdUSB_hi;
			u8 bDeviceClass;
			u8 bDeviceSubClass;
			u8 bDeviceProtocol;
			u8 bMaxPacketSize0;
			u8 bNumConfigurations;
			u8 bReserved;
		} device_qualifier;
		struct
		{
			u8 wTotalLength_lo;
			u8 wTotalLength_hi;
			u8 bNumInterfaces;
			u8 bConfigurationValue;
			u8 iConfiguration;
			u8 bmAttributes;
			u8 bMaxPower;
		} config;
		struct
		{
			u8 bInterfaceNumber;
			u8 bAlternateSetting;
			u8 bNumEndpoints;
			u8 bInterfaceClass;
			u8 bInterfaceSubClass;
			u8 bInterfaceProtocol;
			u8 iInterface;
		} intf;
		struct
		{
			u8 bEndpointAddress;
			u8 bmAttributes;
			u8 wMaxPacketSize_lo;
			u8 wMaxPacketSize_hi;
			u8 bInterval;
			u8 bRefresh; /* only audio ep */
			u8 bSynchAddress; /* only audio ep */
		} endpoint;
		struct
		{
			u8 bMaxBurst;
			u8 bmAttributes;
			u8 wBytesPerInterval_lo;
			u8 wBytesPerInterval_hi;
		} super_endpoint;
		struct
		{
			u8 wTotalLength_lo;
			u8 wTotalLength_hi;
			u8 bNumDeviceCaps;
		} bos;
		struct
		{
			u8 bDevCapabilityType;
			union
			{
				struct
				{
					u8 bmAttributes_1;
					u8 bmAttributes_2;
					u8 bmAttributes_3;
					u8 bmAttributes_4;
				} usb2_ext;
				struct
				{
					u8 bmAttributes;
					u8 wSpeedsSupported_lo;
					u8 wSpeedsSupported_hi;
					u8 bFunctionalitySupport;
					u8 bU1DevExitLat;
					u8 wU2DevExitLat_lo;
					u8 wU2DevExitLat_hi;
				} super;
			} u;
		} cap;
	} u;
} USBDescriptor;
#pragma pack(pop)

struct USBDescID
{
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8 iManufacturer;
	u8 iProduct;
	u8 iSerialNumber;
};

struct USBDescDevice
{
	u16 bcdUSB;
	u8 bDeviceClass;
	u8 bDeviceSubClass;
	u8 bDeviceProtocol;
	u8 bMaxPacketSize0;
	u8 bNumConfigurations;

	std::vector<USBDescConfig> confs;
};

struct USBDescConfig
{
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;

	/* grouped interfaces */
	//u8                   nif_groups;
	std::vector<USBDescIfaceAssoc> if_groups;

	/* "normal" interfaces */
	//u8                   nif;
	std::vector<USBDescIface> ifs;
};

/* conceptually an Interface Association Descriptor, and releated interfaces */
struct USBDescIfaceAssoc
{
	u8 bFirstInterface;
	u8 bInterfaceCount;
	u8 bFunctionClass;
	u8 bFunctionSubClass;
	u8 bFunctionProtocol;
	u8 iFunction;

	//u8                   nif;
	std::vector<USBDescIface> ifs;
};

struct USBDescIface
{
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;

	//u8                   ndesc;
	std::vector<USBDescOther> descs;
	std::vector<USBDescEndpoint> eps;
};

struct USBDescEndpoint
{
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
	u8 bRefresh;
	u8 bSynchAddress;

	u8 is_audio; /* has bRefresh + bSynchAddress */
	const u8* extra;

	/* superspeed endpoint companion */
	u8 bMaxBurst;
	u8 bmAttributes_super;
	u16 wBytesPerInterval;
};

struct USBDescOther
{
	u8 length;
	const u8* data;
};

struct USBDescMSOS
{
	const char* CompatibleID;
	const wchar_t* Label;
	bool SelectiveSuspendEnabled;
};

typedef const char* USBDescStrings[256];

struct USBDesc
{
	USBDescID id;
	const USBDescDevice* full;
	const USBDescDevice* high;
	const USBDescDevice* super;
	const char* const* str;
	const USBDescMSOS* msos;
};

#define USB_DESC_FLAG_SUPER (1 << 1)

/* little helpers */
static inline u8 usb_lo(u16 val)
{
	return val & 0xff;
}

static inline u8 usb_hi(u16 val)
{
	return (val >> 8) & 0xff;
}

/* generate usb packages from structs */
int usb_desc_device(const USBDescID* id, const USBDescDevice* dev,
					bool msos, u8* dest, size_t len);
int usb_desc_device_qualifier(const USBDescDevice* dev,
							  u8* dest, size_t len);
int usb_desc_config(const USBDescConfig& conf, int flags,
					u8* dest, size_t len);
int usb_desc_iface_group(const USBDescIfaceAssoc& iad, int flags,
						 u8* dest, size_t len);
int usb_desc_iface(const USBDescIface& iface, int flags,
				   u8* dest, size_t len);
int usb_desc_endpoint(const USBDescEndpoint& ep, int flags,
					  u8* dest, size_t len);
int usb_desc_other(const USBDescOther& desc, u8* dest, size_t len);
//int usb_desc_msos(const USBDesc *desc, USBPacket *p,
//                  int index, u8 *dest, size_t len);
int usb_desc_parse_dev(const u8* data, int len, USBDesc& desc, USBDescDevice& dev);
int usb_desc_parse_config(const u8* data, int len, USBDescDevice& dev);

/* control message emulation helpers */
void usb_desc_init(USBDevice* dev);
void usb_desc_attach(USBDevice* dev);
int usb_desc_string(USBDevice* dev, int index, u8* dest, size_t len);
int usb_desc_get_descriptor(USBDevice* dev, USBPacket* p,
							int value, u8* dest, size_t len);
int usb_desc_handle_control(USBDevice* dev, USBPacket* p,
							int request, int value, int index, int length, u8* data);

int usb_desc_set_config(USBDevice* dev, int value);
int usb_desc_set_interface(USBDevice* dev, int index, int value);
