// SPDX-FileCopyrightText: ?? QEMU, 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/qusb.h"

#define USB_DEVICE_GET_CLASS(p) (&p->klass)

USBDevice* usb_device_find_device(USBDevice* dev, uint8_t addr)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->find_device)
	{
		return klass->find_device(dev, addr);
	}
	return nullptr;
}

void usb_device_cancel_packet(USBDevice* dev, USBPacket* p)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->cancel_packet)
	{
		klass->cancel_packet(dev, p);
	}
}

void usb_device_handle_attach(USBDevice* dev)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->handle_attach)
	{
		klass->handle_attach(dev);
	}
}

void usb_device_handle_reset(USBDevice* dev)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->handle_reset)
	{
		klass->handle_reset(dev);
	}
}

void usb_device_handle_control(USBDevice* dev, USBPacket* p, int request,
							   int value, int index, int length, uint8_t* data)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->handle_control)
	{
		klass->handle_control(dev, p, request, value, index, length, data);
	}
}

void usb_device_handle_data(USBDevice* dev, USBPacket* p)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->handle_data)
	{
		klass->handle_data(dev, p);
	}
}

const USBDesc* usb_device_get_usb_desc(USBDevice* dev)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (dev->usb_desc)
	{
		return dev->usb_desc;
	}
	return klass->usb_desc;
}

void usb_device_set_interface(USBDevice* dev, int intf,
							  int alt_old, int alt_new)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->set_interface)
	{
		klass->set_interface(dev, intf, alt_old, alt_new);
	}
}

void usb_device_flush_ep_queue(USBDevice* dev, USBEndpoint* ep)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->flush_ep_queue)
	{
		klass->flush_ep_queue(dev, ep);
	}
}

void usb_device_ep_stopped(USBDevice* dev, USBEndpoint* ep)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->ep_stopped)
	{
		klass->ep_stopped(dev, ep);
	}
}

int usb_device_alloc_streams(USBDevice* dev, USBEndpoint** eps, int nr_eps,
							 int streams)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->alloc_streams)
	{
		return klass->alloc_streams(dev, eps, nr_eps, streams);
	}
	return 0;
}

void usb_device_free_streams(USBDevice* dev, USBEndpoint** eps, int nr_eps)
{
	USBDeviceClass* klass = USB_DEVICE_GET_CLASS(dev);
	if (klass->free_streams)
	{
		klass->free_streams(dev, eps, nr_eps);
	}
}
