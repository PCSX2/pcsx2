/*
 * QEMU USB emulation
 *
 * Copyright (c) 2005 Fabrice Bellard
 *
 * 2008 Generic packet handler rewrite by Max Krasnyansky
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "PrecompiledHeader.h"
#include "USB/platcompat.h"
#include "vl.h"
#include "iov.h"
//#include "trace.h"

void usb_pick_speed(USBPort* port)
{
	static const int speeds[] = {
		//USB_SPEED_SUPER,
		//USB_SPEED_HIGH,
		USB_SPEED_FULL,
		USB_SPEED_LOW,
	};
	USBDevice* udev = port->dev;
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(speeds); i++)
	{
		if ((udev->speedmask & (1 << speeds[i])) &&
			(port->speedmask & (1 << speeds[i])))
		{
			udev->speed = speeds[i];
			return;
		}
	}
}

void usb_attach(USBPort* port)
{
	USBDevice* dev = port->dev;

	assert(dev != NULL);
	assert(dev->attached);
	assert(dev->state == USB_STATE_NOTATTACHED);
	usb_pick_speed(port);
	port->ops->attach(port);
	dev->state = USB_STATE_ATTACHED;
	usb_device_handle_attach(dev);
}

void usb_detach(USBPort* port)
{
	USBDevice* dev = port->dev;

	assert(dev != NULL);
	assert(dev->state != USB_STATE_NOTATTACHED);
	port->ops->detach(port);
	dev->state = USB_STATE_NOTATTACHED;
}

void usb_reattach(USBPort* port)
{
	usb_detach(port);
	usb_attach(port);
}

void usb_port_reset(USBPort* port)
{
	USBDevice* dev = port->dev;

	assert(dev != NULL);
	usb_detach(port);
	usb_attach(port);
	usb_device_reset(dev);
}

void usb_device_reset(USBDevice* dev)
{
	if (dev == NULL || !dev->attached)
	{
		return;
	}
	dev->remote_wakeup = 0;
	dev->addr = 0;
	dev->state = USB_STATE_DEFAULT;
	usb_device_handle_reset(dev);
}

void usb_wakeup(USBEndpoint* ep, unsigned int stream)
{
	USBDevice* dev = ep->dev;
	USBBus* bus = dev->bus; //usb_bus_from_device(dev);

	if (dev->remote_wakeup && dev->port && dev->port->ops->wakeup)
	{
		dev->port->ops->wakeup(dev->port);
	}
	if (bus && bus->ops->wakeup_endpoint)
	{
		bus->ops->wakeup_endpoint(bus, ep, stream);
	}
}

/**********************/

/* generic USB device helpers (you are not forced to use them when
   writing your USB device driver, but they help handling the
   protocol)
*/

#define SETUP_STATE_IDLE 0
#define SETUP_STATE_SETUP 1
#define SETUP_STATE_DATA 2
#define SETUP_STATE_ACK 3
#define SETUP_STATE_PARAM 4

static void do_token_setup(USBDevice* s, USBPacket* p)
{
	int request, value, index;

	if (p->iov.size != 8)
	{
		p->status = USB_RET_STALL;
		return;
	}

	usb_packet_copy(p, s->setup_buf, p->iov.size);
	s->setup_index = 0;
	p->actual_length = 0;
	s->setup_len = (s->setup_buf[7] << 8) | s->setup_buf[6];
	if (s->setup_len > (int32_t)sizeof(s->data_buf))
	{
		Console.Warning(
				"usb_generic_handle_packet: ctrl buffer too small (%d > %zu)\n",
				s->setup_len, sizeof(s->data_buf));
		p->status = USB_RET_STALL;
		return;
	}

	request = (s->setup_buf[0] << 8) | s->setup_buf[1];
	value = (s->setup_buf[3] << 8) | s->setup_buf[2];
	index = (s->setup_buf[5] << 8) | s->setup_buf[4];

	if (s->setup_buf[0] & USB_DIR_IN)
	{
		usb_device_handle_control(s, p, request, value, index,
								  s->setup_len, s->data_buf);
		if (p->status == USB_RET_ASYNC)
		{
			s->setup_state = SETUP_STATE_SETUP;
		}
		if (p->status != USB_RET_SUCCESS)
		{
			return;
		}

		if (p->actual_length < s->setup_len)
		{
			s->setup_len = p->actual_length;
		}
		s->setup_state = SETUP_STATE_DATA;
	}
	else
	{
		if (s->setup_len == 0)
			s->setup_state = SETUP_STATE_ACK;
		else
			s->setup_state = SETUP_STATE_DATA;
	}

	p->actual_length = 8;
}

static void do_token_in(USBDevice* s, USBPacket* p)
{
	int request, value, index;

	assert(p->ep->nr == 0);

	request = (s->setup_buf[0] << 8) | s->setup_buf[1];
	value = (s->setup_buf[3] << 8) | s->setup_buf[2];
	index = (s->setup_buf[5] << 8) | s->setup_buf[4];

	switch (s->setup_state)
	{
		case SETUP_STATE_ACK:
			if (!(s->setup_buf[0] & USB_DIR_IN))
			{
				usb_device_handle_control(s, p, request, value, index,
										  s->setup_len, s->data_buf);
				if (p->status == USB_RET_ASYNC)
				{
					return;
				}
				s->setup_state = SETUP_STATE_IDLE;
				p->actual_length = 0;
			}
			break;

		case SETUP_STATE_DATA:
			if (s->setup_buf[0] & USB_DIR_IN)
			{
				int len = s->setup_len - s->setup_index;
				if ((size_t)len > p->iov.size)
				{
					len = p->iov.size;
				}
				usb_packet_copy(p, s->data_buf + s->setup_index, len);
				s->setup_index += len;
				if (s->setup_index >= s->setup_len)
				{
					s->setup_state = SETUP_STATE_ACK;
				}
				return;
			}
			s->setup_state = SETUP_STATE_IDLE;
			p->status = USB_RET_STALL;
			break;

		default:
			p->status = USB_RET_STALL;
	}
}

static void do_token_out(USBDevice* s, USBPacket* p)
{
	assert(p->ep->nr == 0);

	switch (s->setup_state)
	{
		case SETUP_STATE_ACK:
			if (s->setup_buf[0] & USB_DIR_IN)
			{
				s->setup_state = SETUP_STATE_IDLE;
				/* transfer OK */
			}
			else
			{
				/* ignore additional output */
			}
			break;

		case SETUP_STATE_DATA:
			if (!(s->setup_buf[0] & USB_DIR_IN))
			{
				int len = s->setup_len - s->setup_index;
				if ((size_t)len > p->iov.size)
				{
					len = p->iov.size;
				}
				usb_packet_copy(p, s->data_buf + s->setup_index, len);
				s->setup_index += len;
				if (s->setup_index >= s->setup_len)
				{
					s->setup_state = SETUP_STATE_ACK;
				}
				return;
			}
			s->setup_state = SETUP_STATE_IDLE;
			p->status = USB_RET_STALL;
			break;

		default:
			p->status = USB_RET_STALL;
	}
}

static void do_parameter(USBDevice* s, USBPacket* p)
{
	int i, request, value, index;

	for (i = 0; i < 8; i++)
	{
		s->setup_buf[i] = p->parameter >> (i * 8);
	}

	s->setup_state = SETUP_STATE_PARAM;
	s->setup_len = (s->setup_buf[7] << 8) | s->setup_buf[6];
	s->setup_index = 0;

	request = (s->setup_buf[0] << 8) | s->setup_buf[1];
	value = (s->setup_buf[3] << 8) | s->setup_buf[2];
	index = (s->setup_buf[5] << 8) | s->setup_buf[4];

	if (s->setup_len > (int32_t)sizeof(s->data_buf))
	{
		Console.Warning(
				"usb_generic_handle_packet: ctrl buffer too small (%d > %zu)\n",
				s->setup_len, sizeof(s->data_buf));
		p->status = USB_RET_STALL;
		return;
	}

	if (p->pid == USB_TOKEN_OUT)
	{
		usb_packet_copy(p, s->data_buf, s->setup_len);
	}

	usb_device_handle_control(s, p, request, value, index,
							  s->setup_len, s->data_buf);
	if (p->status == USB_RET_ASYNC)
	{
		return;
	}

	if (p->actual_length < s->setup_len)
	{
		s->setup_len = p->actual_length;
	}
	if (p->pid == USB_TOKEN_IN)
	{
		p->actual_length = 0;
		usb_packet_copy(p, s->data_buf, s->setup_len);
	}
}

/* ctrl complete function for devices which use usb_generic_handle_packet and
   may return USB_RET_ASYNC from their handle_control callback. Device code
   which does this *must* call this function instead of the normal
   usb_packet_complete to complete their async control packets. */
void usb_generic_async_ctrl_complete(USBDevice* s, USBPacket* p)
{
	if (p->status < 0)
	{
		s->setup_state = SETUP_STATE_IDLE;
	}

	switch (s->setup_state)
	{
		case SETUP_STATE_SETUP:
			if (p->actual_length < s->setup_len)
			{
				s->setup_len = p->actual_length;
			}
			s->setup_state = SETUP_STATE_DATA;
			p->actual_length = 8;
			break;

		case SETUP_STATE_ACK:
			s->setup_state = SETUP_STATE_IDLE;
			p->actual_length = 0;
			break;

		case SETUP_STATE_PARAM:
			if (p->actual_length < s->setup_len)
			{
				s->setup_len = p->actual_length;
			}
			if (p->pid == USB_TOKEN_IN)
			{
				p->actual_length = 0;
				usb_packet_copy(p, s->data_buf, s->setup_len);
			}
			break;

		default:
			break;
	}
	usb_packet_complete(s, p);
}

USBDevice* usb_find_device(USBPort* port, uint8_t addr)
{
	USBDevice* dev = port->dev;

	if (dev == NULL || !dev->attached || dev->state != USB_STATE_DEFAULT)
	{
		return NULL;
	}
	if (dev->addr == addr)
	{
		return dev;
	}
	return usb_device_find_device(dev, addr);
}

static void usb_process_one(USBPacket* p)
{
	USBDevice* dev = p->ep->dev;

	/*
     * Handlers expect status to be initialized to USB_RET_SUCCESS, but it
     * can be USB_RET_NAK here from a previous usb_process_one() call,
     * or USB_RET_ASYNC from going through usb_queue_one().
     */
	p->status = USB_RET_SUCCESS;

	if (p->ep->nr == 0)
	{
		/* control pipe */
		if (p->parameter)
		{
			do_parameter(dev, p);
			return;
		}
		switch (p->pid)
		{
			case USB_TOKEN_SETUP:
				do_token_setup(dev, p);
				break;
			case USB_TOKEN_IN:
				do_token_in(dev, p);
				break;
			case USB_TOKEN_OUT:
				do_token_out(dev, p);
				break;
			default:
				p->status = USB_RET_STALL;
		}
	}
	else
	{
		/* data pipe */
		usb_device_handle_data(dev, p);
	}
}

static void usb_queue_one(USBPacket* p)
{
	usb_packet_set_state(p, USB_PACKET_QUEUED);
	QTAILQ_INSERT_TAIL(&p->ep->queue, p, queue);
	p->status = USB_RET_ASYNC;
}

/* Hand over a packet to a device for processing.  p->status ==
   USB_RET_ASYNC indicates the processing isn't finished yet, the
   driver will call usb_packet_complete() when done processing it. */
void usb_handle_packet(USBDevice* dev, USBPacket* p)
{
	if (dev == NULL)
	{
		p->status = USB_RET_NODEV;
		return;
	}
	assert(dev == p->ep->dev);
	assert(dev->state == USB_STATE_DEFAULT);
	usb_packet_check_state(p, USB_PACKET_SETUP);
	assert(p->ep != NULL);

	/* Submitting a new packet clears halt */
	if (p->ep->halted)
	{
		assert(QTAILQ_EMPTY(&p->ep->queue));
		p->ep->halted = false;
	}

	if (QTAILQ_EMPTY(&p->ep->queue) || p->ep->pipeline || p->stream)
	{
		usb_process_one(p);
		if (p->status == USB_RET_ASYNC)
		{
			/* hcd drivers cannot handle async for isoc */
			assert(p->ep->type != USB_ENDPOINT_XFER_ISOC);
			/* using async for interrupt packets breaks migration */
			assert(p->ep->type != USB_ENDPOINT_XFER_INT ||
				   (dev->flags & (1 << USB_DEV_FLAG_IS_HOST)));
			usb_packet_set_state(p, USB_PACKET_ASYNC);
			QTAILQ_INSERT_TAIL(&p->ep->queue, p, queue);
		}
		else if (p->status == USB_RET_ADD_TO_QUEUE)
		{
			usb_queue_one(p);
		}
		else
		{
			/*
             * When pipelining is enabled usb-devices must always return async,
             * otherwise packets can complete out of order!
             */
			assert(p->stream || !p->ep->pipeline ||
				   QTAILQ_EMPTY(&p->ep->queue));
			if (p->status != USB_RET_NAK)
			{
				usb_packet_set_state(p, USB_PACKET_COMPLETE);
			}
		}
	}
	else
	{
		usb_queue_one(p);
	}
}

void usb_packet_complete_one(USBDevice* dev, USBPacket* p)
{
	USBEndpoint* ep = p->ep;

	assert(p->stream || QTAILQ_FIRST(&ep->queue) == p);
	assert(p->status != USB_RET_ASYNC && p->status != USB_RET_NAK);

	if (p->status != USB_RET_SUCCESS ||
		(p->short_not_ok && ((size_t)p->actual_length < p->iov.size)))
	{
		ep->halted = true;
	}
	usb_packet_set_state(p, USB_PACKET_COMPLETE);
	QTAILQ_REMOVE(&ep->queue, p, queue);
	dev->port->ops->complete(dev->port, p);
}

/* Notify the controller that an async packet is complete.  This should only
   be called for packets previously deferred by returning USB_RET_ASYNC from
   handle_packet. */
void usb_packet_complete(USBDevice* dev, USBPacket* p)
{
	USBEndpoint* ep = p->ep;

	usb_packet_check_state(p, USB_PACKET_ASYNC);
	usb_packet_complete_one(dev, p);

	while (!QTAILQ_EMPTY(&ep->queue))
	{
		p = QTAILQ_FIRST(&ep->queue);
		if (ep->halted)
		{
			/* Empty the queue on a halt */
			p->status = USB_RET_REMOVE_FROM_QUEUE;
			dev->port->ops->complete(dev->port, p);
			continue;
		}
		if (p->state == USB_PACKET_ASYNC)
		{
			break;
		}
		usb_packet_check_state(p, USB_PACKET_QUEUED);
		usb_process_one(p);
		if (p->status == USB_RET_ASYNC)
		{
			usb_packet_set_state(p, USB_PACKET_ASYNC);
			break;
		}
		usb_packet_complete_one(ep->dev, p);
	}
}

/* Cancel an active packet.  The packed must have been deferred by
   returning USB_RET_ASYNC from handle_packet, and not yet
   completed.  */
void usb_cancel_packet(USBPacket* p)
{
	bool callback = (p->state == USB_PACKET_ASYNC);
	assert(usb_packet_is_inflight(p));
	usb_packet_set_state(p, USB_PACKET_CANCELED);
	QTAILQ_REMOVE(&p->ep->queue, p, queue);
	if (callback)
	{
		usb_device_cancel_packet(p->ep->dev, p);
	}
}


void usb_packet_init(USBPacket* p)
{
	qemu_iovec_init(&p->iov, 1);
}

static const char* usb_packet_state_name(USBPacketState state)
{
	static const char* name[] = {
		/*[USB_PACKET_UNDEFINED] =*/"undef",
		/*[USB_PACKET_SETUP]     =*/"setup",
		/*[USB_PACKET_QUEUED]    =*/"queued",
		/*[USB_PACKET_ASYNC]     =*/"async",
		/*[USB_PACKET_COMPLETE]  =*/"complete",
		/*[USB_PACKET_CANCELED]  =*/"canceled",
	};
	if (state < ARRAY_SIZE(name))
	{
		return name[state];
	}
	return "INVALID";
}

void usb_packet_check_state(USBPacket* p, USBPacketState expected)
{
	if (p->state == expected)
	{
		return;
	}
	//trace_usb_packet_state_fault(bus->busnr, dev->port->path, p->ep->nr, p,
	//                             usb_packet_state_name(p->state),
	//                             usb_packet_state_name(expected));
	assert(!"usb packet state check failed");
}

void usb_packet_set_state(USBPacket* p, USBPacketState state)
{
	/*if (p->ep) {
        USBDevice *dev = p->ep->dev;
        USBBus *bus = usb_bus_from_device(dev);
        trace_usb_packet_state_change(bus->busnr, dev->port->path, p->ep->nr, p,
                                      usb_packet_state_name(p->state),
                                      usb_packet_state_name(state));
    } else {
        trace_usb_packet_state_change(-1, "", -1, p,
                                      usb_packet_state_name(p->state),
                                      usb_packet_state_name(state));
    }*/
	p->state = state;
}

void usb_packet_setup(USBPacket* p, int pid,
					  USBEndpoint* ep, unsigned int stream,
					  uint64_t id, bool short_not_ok, bool int_req)
{
	assert(!usb_packet_is_inflight(p));
	assert(p->iov.iov != NULL);
	p->id = id;
	p->pid = pid;
	p->ep = ep;
	p->stream = stream;
	p->status = USB_RET_SUCCESS;
	p->actual_length = 0;
	p->parameter = 0;
	p->short_not_ok = short_not_ok;
	p->int_req = int_req;
	p->combined = NULL;
	qemu_iovec_reset(&p->iov);
	usb_packet_set_state(p, USB_PACKET_SETUP);
}

void usb_packet_addbuf(USBPacket* p, void* ptr, size_t len)
{
	qemu_iovec_add(&p->iov, ptr, len);
}

void usb_packet_copy(USBPacket* p, void* ptr, size_t bytes)
{
	QEMUIOVector* iov = p->combined ? &p->combined->iov : &p->iov;

	assert(bytes <= INT_MAX);
	assert(p->actual_length >= 0);
	assert(p->actual_length + bytes <= iov->size);
	switch (p->pid)
	{
		case USB_TOKEN_SETUP:
		case USB_TOKEN_OUT:
			iov_to_buf(iov->iov, iov->niov, p->actual_length, ptr, bytes);
			break;
		case USB_TOKEN_IN:
			iov_from_buf(iov->iov, iov->niov, p->actual_length, ptr, bytes);
			break;
		default:
			Console.Warning("%s: invalid pid: %x\n", __func__, p->pid);
			abort();
	}
	p->actual_length += bytes;
}

void usb_packet_skip(USBPacket* p, size_t bytes)
{
	QEMUIOVector* iov = p->combined ? &p->combined->iov : &p->iov;

	assert(bytes <= INT_MAX);
	assert(p->actual_length >= 0);
	assert(p->actual_length + bytes <= iov->size);
	if (p->pid == USB_TOKEN_IN)
	{
		iov_memset(iov->iov, iov->niov, p->actual_length, 0, bytes);
	}
	p->actual_length += bytes;
}

size_t usb_packet_size(USBPacket* p)
{
	return p->combined ? p->combined->iov.size : p->iov.size;
}

void usb_packet_cleanup(USBPacket* p)
{
	assert(!usb_packet_is_inflight(p));
	qemu_iovec_destroy(&p->iov);
}

void usb_ep_reset(USBDevice* dev)
{
	int ep;

	dev->ep_ctl.nr = 0;
	dev->ep_ctl.type = USB_ENDPOINT_XFER_CONTROL;
	dev->ep_ctl.ifnum = 0;
	dev->ep_ctl.max_packet_size = 64;
	dev->ep_ctl.max_streams = 0;
	dev->ep_ctl.dev = dev;
	dev->ep_ctl.pipeline = false;
	for (ep = 0; ep < USB_MAX_ENDPOINTS; ep++)
	{
		dev->ep_in[ep].nr = ep + 1;
		dev->ep_out[ep].nr = ep + 1;
		dev->ep_in[ep].pid = USB_TOKEN_IN;
		dev->ep_out[ep].pid = USB_TOKEN_OUT;
		dev->ep_in[ep].type = USB_ENDPOINT_XFER_INVALID;
		dev->ep_out[ep].type = USB_ENDPOINT_XFER_INVALID;
		dev->ep_in[ep].ifnum = USB_INTERFACE_INVALID;
		dev->ep_out[ep].ifnum = USB_INTERFACE_INVALID;
		dev->ep_in[ep].max_packet_size = 0;
		dev->ep_out[ep].max_packet_size = 0;
		dev->ep_in[ep].max_streams = 0;
		dev->ep_out[ep].max_streams = 0;
		dev->ep_in[ep].dev = dev;
		dev->ep_out[ep].dev = dev;
		dev->ep_in[ep].pipeline = false;
		dev->ep_out[ep].pipeline = false;
	}
}

void usb_ep_init(USBDevice* dev)
{
	int ep;

	usb_ep_reset(dev);
	QTAILQ_INIT(&dev->ep_ctl.queue);
	for (ep = 0; ep < USB_MAX_ENDPOINTS; ep++)
	{
		QTAILQ_INIT(&dev->ep_in[ep].queue);
		QTAILQ_INIT(&dev->ep_out[ep].queue);
	}
}

void usb_ep_dump(USBDevice* dev)
{
	static const char* tname[] = {
		/* [USB_ENDPOINT_XFER_CONTROL] = */ "control",
		/* [USB_ENDPOINT_XFER_ISOC]    = */ "isoc",
		/* [USB_ENDPOINT_XFER_BULK]    = */ "bulk",
		/* [USB_ENDPOINT_XFER_INT]     = */ "int",
	};
	int ifnum, ep, first;

	Console.Warning("Device \"%s\", config %d\n",
			dev->product_desc, dev->configuration);
	for (ifnum = 0; ifnum < 16; ifnum++)
	{
		first = 1;
		for (ep = 0; ep < USB_MAX_ENDPOINTS; ep++)
		{
			if (dev->ep_in[ep].type != USB_ENDPOINT_XFER_INVALID &&
				dev->ep_in[ep].ifnum == ifnum)
			{
				if (first)
				{
					first = 0;
					Console.Warning("  Interface %d, alternative %d\n",
							ifnum, dev->altsetting[ifnum]);
				}
				Console.Warning("    Endpoint %d, IN, %s, %d max\n", ep,
						tname[dev->ep_in[ep].type],
						dev->ep_in[ep].max_packet_size);
			}
			if (dev->ep_out[ep].type != USB_ENDPOINT_XFER_INVALID &&
				dev->ep_out[ep].ifnum == ifnum)
			{
				if (first)
				{
					first = 0;
					Console.Warning("  Interface %d, alternative %d\n",
							ifnum, dev->altsetting[ifnum]);
				}
				Console.Warning("    Endpoint %d, OUT, %s, %d max\n", ep,
						tname[dev->ep_out[ep].type],
						dev->ep_out[ep].max_packet_size);
			}
		}
	}
	Console.Warning("--\n");
}

struct USBEndpoint* usb_ep_get(USBDevice* dev, int pid, int ep)
{
	struct USBEndpoint* eps;

	if (dev == NULL)
	{
		return NULL;
	}
	eps = (pid == USB_TOKEN_IN) ? dev->ep_in : dev->ep_out;
	if (ep == 0)
	{
		return &dev->ep_ctl;
	}
	assert(pid == USB_TOKEN_IN || pid == USB_TOKEN_OUT);
	assert(ep > 0 && ep <= USB_MAX_ENDPOINTS);
	return eps + ep - 1;
}

uint8_t usb_ep_get_type(USBDevice* dev, int pid, int ep)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	return uep->type;
}

void usb_ep_set_type(USBDevice* dev, int pid, int ep, uint8_t type)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	uep->type = type;
}

void usb_ep_set_ifnum(USBDevice* dev, int pid, int ep, uint8_t ifnum)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	uep->ifnum = ifnum;
}

void usb_ep_set_max_packet_size(USBDevice* dev, int pid, int ep,
								uint16_t raw)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	int size, microframes;

	size = raw & 0x7ff;
	switch ((raw >> 11) & 3)
	{
		case 1:
			microframes = 2;
			break;
		case 2:
			microframes = 3;
			break;
		default:
			microframes = 1;
			break;
	}
	uep->max_packet_size = size * microframes;
}

void usb_ep_set_max_streams(USBDevice* dev, int pid, int ep, uint8_t raw)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	int MaxStreams;

	MaxStreams = raw & 0x1f;
	if (MaxStreams)
	{
		uep->max_streams = 1 << MaxStreams;
	}
	else
	{
		uep->max_streams = 0;
	}
}

void usb_ep_set_halted(USBDevice* dev, int pid, int ep, bool halted)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	uep->halted = halted;
}

USBPacket* usb_ep_find_packet_by_id(USBDevice* dev, int pid, int ep,
									uint64_t id)
{
	struct USBEndpoint* uep = usb_ep_get(dev, pid, ep);
	USBPacket* p;

	QTAILQ_FOREACH(p, &uep->queue, queue)
	{
		if (p->id == id)
		{
			return p;
		}
	}

	return NULL;
}

void usb_wakeup(USBDevice* dev)
{
	if (dev->remote_wakeup && dev->port && dev->port->ops->wakeup)
	{
		dev->port->ops->wakeup(dev->port);
	}
}
