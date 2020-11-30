/*
 * QEMU USB OHCI Emulation
 * Copyright (c) 2004 Gianni Tedesco
 * Copyright (c) 2006 CodeSourcery
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * TODO:
 *  o Isochronous transfers
 *  o Allocate bandwidth in frames properly
 *  o Disable timers when nothing needs to be done, or remove timer usage
 *    all together.
 *  o Handle unrecoverable errors properly
 *  o BIOS work to boot from USB storage
*/

//typedef CPUReadMemoryFunc

#include "PrecompiledHeader.h"
#include "vl.h"
#include "queue.h"
#include "USBinternal.h"

#define DMA_DIRECTION_TO_DEVICE 0
#define DMA_DIRECTION_FROM_DEVICE 1
#define ED_LINK_LIMIT 32

int64_t last_cycle = 0;
#define MIN_IRQ_INTERVAL 64 /* hack */

extern int64_t get_clock();
extern int get_ticks_per_second();
extern void usbIrq(int);

//#define DEBUG_PACKET
//#define DEBUG_OHCI

static void ohci_async_cancel_device(OHCIState* ohci, USBDevice* dev);

/* Update IRQ levels */
static inline void ohci_intr_update(OHCIState* ohci)
{
	int level = 0;

	if ((ohci->intr & OHCI_INTR_MIE) &&
		(ohci->intr_status & ohci->intr))
		level = 1;

	if (level)
	{

		if ((get_clock() - last_cycle) > MIN_IRQ_INTERVAL)
		{
			usbIrq(1);
			last_cycle = get_clock();
		}
	}
}

/* Set an interrupt */
static inline void ohci_set_interrupt(OHCIState* ohci, uint32_t intr)
{
	ohci->intr_status |= intr;
	ohci_intr_update(ohci);
}

static void ohci_die(OHCIState* ohci)
{
	//OHCIPCIState *dev = container_of(ohci, OHCIPCIState, state);

	Console.Warning("ohci_die: DMA error\n");

	ohci_set_interrupt(ohci, OHCI_INTR_UE);
	ohci_bus_stop(ohci);
	//pci_set_word(dev->parent_obj.config + PCI_STATUS,
	//             PCI_STATUS_DETECTED_PARITY);
}

/* Attach or detach a device on a root hub port.  */
static void ohci_attach2(USBPort* port1, USBDevice* dev)
{
	OHCIState* s = (OHCIState*)port1->opaque;
	OHCIPort* port = (OHCIPort*)&s->rhport[port1->index];
	uint32_t old_state = port->ctrl;

	if (dev)
	{
		if (port1->dev)
		{
			ohci_attach2(port1, NULL);
		}
		/* set connect status */
		port->ctrl |= OHCI_PORT_CCS | OHCI_PORT_CSC;

		/* update speed */
		if (dev->speed == USB_SPEED_LOW)
			port->ctrl |= OHCI_PORT_LSDA;
		else
			port->ctrl &= ~OHCI_PORT_LSDA;
		dev->port = port1;
		port1->dev = dev;
		dev->state = USB_STATE_ATTACHED;
	}
	else
	{
		/* set connect status */
		if (port->ctrl & OHCI_PORT_CCS)
		{
			port->ctrl &= ~OHCI_PORT_CCS;
			port->ctrl |= OHCI_PORT_CSC;
		}
		/* disable port */
		if (port->ctrl & OHCI_PORT_PES)
		{
			port->ctrl &= ~OHCI_PORT_PES;
			port->ctrl |= OHCI_PORT_PESC;
		}
		dev = port1->dev;
		if (dev)
		{
			dev->port = NULL;
			/* send the detach message */
			dev->state = USB_STATE_NOTATTACHED;
		}
		port1->dev = NULL;
	}

	if (old_state != port->ctrl)
		ohci_set_interrupt(s, OHCI_INTR_RHSC);
}

/* Attach or detach a device on a root hub port.  */
static void ohci_attach(USBPort* port1)
{
	OHCIState* s = (OHCIState*)port1->opaque;
	OHCIPort* port = &s->rhport[port1->index];
	uint32_t old_state = port->ctrl;

	port1->dev->port = port1;

	/* set connect status */
	port->ctrl |= OHCI_PORT_CCS | OHCI_PORT_CSC;

	/* update speed */
	if (port->port.dev->speed == USB_SPEED_LOW)
	{
		port->ctrl |= OHCI_PORT_LSDA;
	}
	else
	{
		port->ctrl &= ~OHCI_PORT_LSDA;
	}

	/* notify of remote-wakeup */
	if ((s->ctl & OHCI_CTL_HCFS) == OHCI_USB_SUSPEND)
	{
		ohci_set_interrupt(s, OHCI_INTR_RD);
	}

	//trace_usb_ohci_port_attach(port1->index);

	if (old_state != port->ctrl)
	{
		ohci_set_interrupt(s, OHCI_INTR_RHSC);
	}
}

static void ohci_detach(USBPort* port1)
{
	OHCIState* s = (OHCIState*)port1->opaque;
	OHCIPort* port = &s->rhport[port1->index];
	uint32_t old_state = port->ctrl;

	if (port1->dev)
		ohci_async_cancel_device(s, port1->dev);

	/* set connect status */
	if (port->ctrl & OHCI_PORT_CCS)
	{
		port->ctrl &= ~OHCI_PORT_CCS;
		port->ctrl |= OHCI_PORT_CSC;
	}
	/* disable port */
	if (port->ctrl & OHCI_PORT_PES)
	{
		port->ctrl &= ~OHCI_PORT_PES;
		port->ctrl |= OHCI_PORT_PESC;
	}
	//trace_usb_ohci_port_detach(port1->index);

	if (old_state != port->ctrl)
	{
		ohci_set_interrupt(s, OHCI_INTR_RHSC);
	}
}

static void ohci_wakeup(USBPort* port1)
{
	OHCIState* s = (OHCIState*)port1->opaque;
	OHCIPort* port = (OHCIPort*)&s->rhport[port1->index];
	uint32_t intr = 0;
	if (port->ctrl & OHCI_PORT_PSS)
	{
		//trace_usb_ohci_port_wakeup(port1->index);
		port->ctrl |= OHCI_PORT_PSSC;
		port->ctrl &= ~OHCI_PORT_PSS;
		intr = OHCI_INTR_RHSC;
	}
	/* Note that the controller can be suspended even if this port is not */
	if ((s->ctl & OHCI_CTL_HCFS) == OHCI_USB_SUSPEND)
	{
		//trace_usb_ohci_remote_wakeup(s->name);
		/* This is the one state transition the controller can do by itself */
		s->ctl &= ~OHCI_CTL_HCFS;
		s->ctl |= OHCI_USB_RESUME;
		/* In suspend mode only ResumeDetected is possible, not RHSC:
         * see the OHCI spec 5.1.2.3.
         */
		intr = OHCI_INTR_RD;
	}
	ohci_set_interrupt(s, intr);
}

static USBDevice* ohci_find_device(OHCIState* ohci, uint8_t addr)
{
	USBDevice* dev;

	for (unsigned int i = 0; i < ohci->num_ports; i++)
	{
		if ((ohci->rhport[i].ctrl & OHCI_PORT_PES) == 0)
		{
			continue;
		}
		dev = usb_find_device(&ohci->rhport[i].port, addr);
		if (dev != nullptr)
		{
			return dev;
		}
	}
	return nullptr;
}

//TODO no devices using this yet
static void ohci_stop_endpoints(OHCIState* ohci)
{
	USBDevice* dev;

	for (unsigned int i = 0; i < ohci->num_ports; i++)
	{
		dev = ohci->rhport[i].port.dev;
		if (dev && dev->attached)
		{
			usb_device_ep_stopped(dev, &dev->ep_ctl);
			for (int j = 0; j < USB_MAX_ENDPOINTS; j++)
			{
				usb_device_ep_stopped(dev, &dev->ep_in[j]);
				usb_device_ep_stopped(dev, &dev->ep_out[j]);
			}
		}
	}
}

static void ohci_roothub_reset(OHCIState* ohci)
{
	OHCIPort* port;

	ohci_bus_stop(ohci);
	ohci->rhdesc_a = OHCI_RHA_NPS | ohci->num_ports;
	ohci->rhdesc_b = 0x0; /* Impl. specific */
	ohci->rhstatus = 0;

	for (uint32_t i = 0; i < ohci->num_ports; i++)
	{
		port = &ohci->rhport[i];
		port->ctrl = 0;
		if (port->port.dev && port->port.dev->attached)
		{
			usb_port_reset(&port->port);
		}
	}
	if (ohci->async_td)
	{
		usb_cancel_packet(&ohci->usb_packet);
		ohci->async_td = 0;
	}
	ohci_stop_endpoints(ohci);
}

/* Reset the controller */
void ohci_soft_reset(OHCIState* ohci)
{
	ohci_bus_stop(ohci);
	ohci->ctl = (ohci->ctl & OHCI_CTL_IR) | OHCI_USB_SUSPEND;
	ohci->old_ctl = 0;
	ohci->status = 0;
	ohci->intr_status = 0;
	ohci->intr = OHCI_INTR_MIE;

	ohci->hcca = 0;
	ohci->ctrl_head = ohci->ctrl_cur = 0;
	ohci->bulk_head = ohci->bulk_cur = 0;
	ohci->per_cur = 0;
	ohci->done = 0;
	ohci->done_count = 7;

	/* FSMPS is marked TBD in OCHI 1.0, what gives ffs?
     * I took the value linux sets ...
     */
	ohci->fsmps = 0x2778;
	ohci->fi = 0x2edf;
	ohci->fit = 0;
	ohci->frt = 0;
	ohci->frame_number = 0;
	ohci->pstart = 0;
	ohci->lst = OHCI_LS_THRESH;
}

void ohci_hard_reset(OHCIState* ohci)
{
	ohci_soft_reset(ohci);
	ohci->ctl = 0;
	ohci_roothub_reset(ohci);
}

#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define le16_to_cpu(x) (x)
#define cpu_to_le16(x) (x)

/* Get an array of dwords from main memory */
static inline int get_dwords(uint32_t addr, uint32_t* buf, int num)
{
	int i;

	for (i = 0; i < num; i++, buf++, addr += sizeof(*buf))
	{
		if (cpu_physical_memory_rw(addr, (uint8_t*)buf, sizeof(*buf), 0))
			return 0;
		*buf = le32_to_cpu(*buf);
	}

	return 1;
}

/* Get an array of words from main memory */
static inline int get_words(uint32_t addr, uint16_t* buf, int num)
{
	int i;

	for (i = 0; i < num; i++, buf++, addr += sizeof(*buf))
	{
		if (cpu_physical_memory_rw(addr, (uint8_t*)buf, sizeof(*buf), 0))
			return 0;
		*buf = le16_to_cpu(*buf);
	}

	return 1;
}

/* Put an array of dwords in to main memory */
static inline int put_dwords(uint32_t addr, uint32_t* buf, int num)
{
	int i;

	for (i = 0; i < num; i++, buf++, addr += sizeof(*buf))
	{
		uint32_t tmp = cpu_to_le32(*buf);
		if (cpu_physical_memory_rw(addr, (uint8_t*)&tmp, sizeof(tmp), 1))
			return 0;
	}

	return 1;
}

/* Put an array of dwords in to main memory */
static inline int put_words(uint32_t addr, uint16_t* buf, int num)
{
	int i;

	for (i = 0; i < num; i++, buf++, addr += sizeof(*buf))
	{
		uint16_t tmp = cpu_to_le16(*buf);
		if (cpu_physical_memory_rw(addr, (uint8_t*)&tmp, sizeof(tmp), 1))
			return 0;
	}

	return 1;
}

static inline int ohci_read_ed(OHCIState* ohci, uint32_t addr, struct ohci_ed* ed)
{
	return get_dwords(addr, (uint32_t*)ed, sizeof(*ed) >> 2);
}

static inline int ohci_read_td(OHCIState* ohci, uint32_t addr, struct ohci_td* td)
{
	return get_dwords(addr, (uint32_t*)td, sizeof(*td) >> 2);
}

static inline int ohci_read_iso_td(OHCIState* ohci, uint32_t addr, struct ohci_iso_td* td)
{
	return get_dwords(addr, (uint32_t*)td, 4) &&
		   get_words(addr + 16, td->offset, 8);
}

static inline int ohci_put_ed(OHCIState* ohci, uint32_t addr, struct ohci_ed* ed)
{
	/* ed->tail is under control of the HCD.
     * Since just ed->head is changed by HC, just write back this
     */
	return put_dwords(addr + ED_WBACK_OFFSET,
					  (uint32_t*)((char*)ed + ED_WBACK_OFFSET),
					  ED_WBACK_SIZE >> 2);
}

static inline int ohci_put_td(OHCIState* ohci, uint32_t addr, struct ohci_td* td)
{
	return put_dwords(addr, (uint32_t*)td, sizeof(*td) >> 2);
}

static inline int ohci_put_iso_td(OHCIState* ohci, uint32_t addr, struct ohci_iso_td* td)
{
	return put_dwords(addr, (uint32_t*)td, 4) &&
		   put_words(addr + 16, td->offset, 8);
}

static inline int ohci_put_hcca(OHCIState* ohci,
								struct ohci_hcca* hcca)
{
	cpu_physical_memory_write(ohci->hcca + HCCA_WRITEBACK_OFFSET,
							  (uint8_t*)hcca + HCCA_WRITEBACK_OFFSET,
							  HCCA_WRITEBACK_SIZE);
	return 1;
}

/* Read/Write the contents of a TD from/to main memory.  */
static int ohci_copy_td(OHCIState* ohci, struct ohci_td* td, uint8_t* buf, uint32_t len, int write)
{
	uint32_t ptr, n;

	ptr = td->cbp;
	n = 0x1000 - (ptr & 0xfff);
	if (n > len)
		n = len;
	if (cpu_physical_memory_rw(ptr, buf, n, write))
		return 1;
	if (n == len)
		return 0;
	ptr = td->be & ~0xfffu;
	buf += n;
	cpu_physical_memory_rw(ptr, buf, len - n, write);
	return 0;
}

/* Read/Write the contents of an ISO TD from/to main memory.  */
static int ohci_copy_iso_td(OHCIState* ohci, uint32_t start_addr, uint32_t end_addr,
							uint8_t* buf, uint32_t len, int write)
{
	uint32_t ptr, n;

	ptr = start_addr;
	n = 0x1000 - (ptr & 0xfff);
	if (n > len)
		n = len;
	if (cpu_physical_memory_rw(ptr, buf, n, write))
		return 1;
	if (n == len)
		return 0;
	ptr = end_addr & ~0xfffu;
	buf += n;
	cpu_physical_memory_rw(ptr, buf, len - n, write);
	return 0;
}

static void ohci_process_lists(OHCIState* ohci, int completion);

static void ohci_async_complete_packet(USBPort* port, USBPacket* packet)
{
	OHCIState* ohci = CONTAINER_OF(packet, OHCIState, usb_packet);

	//trace_usb_ohci_async_complete();
	ohci->async_complete = true;
	ohci_process_lists(ohci, 1);
}

#define USUB(a, b) ((int16_t)((uint16_t)(a) - (uint16_t)(b)))

static int ohci_service_iso_td(OHCIState* ohci, struct ohci_ed* ed,
							   int completion)
{
	int dir;
	uint32_t len = 0;
	[[maybe_unused]] const char* str = NULL;
	int pid;
	int ret;
	int i;
	USBDevice* dev;
	USBEndpoint* ep;
	struct ohci_iso_td iso_td;
	uint32_t addr;
	uint16_t starting_frame;
	int16_t relative_frame_number;
	int frame_count;
	uint32_t start_offset, next_offset, end_offset = 0;
	uint32_t start_addr, end_addr;

	addr = ed->head & OHCI_DPTR_MASK;

	if (!ohci_read_iso_td(ohci, addr, &iso_td))
	{
		//trace_usb_ohci_iso_td_read_failed(addr);
		ohci_die(ohci);
		return 1;
	}

	starting_frame = OHCI_BM(iso_td.flags, TD_SF);
	frame_count = OHCI_BM(iso_td.flags, TD_FC);
	relative_frame_number = USUB(ohci->frame_number, starting_frame);

	/*trace_usb_ohci_iso_td_head(
           ed->head & OHCI_DPTR_MASK, ed->tail & OHCI_DPTR_MASK,
           iso_td.flags, iso_td.bp, iso_td.next, iso_td.be,
           ohci->frame_number, starting_frame,
           frame_count, relative_frame_number);
    //trace_usb_ohci_iso_td_head_offset(
           iso_td.offset[0], iso_td.offset[1],
           iso_td.offset[2], iso_td.offset[3],
           iso_td.offset[4], iso_td.offset[5],
           iso_td.offset[6], iso_td.offset[7]);
*/
	if (relative_frame_number < 0)
	{
		//trace_usb_ohci_iso_td_relative_frame_number_neg(relative_frame_number);
		return 1;
	}
	else if (relative_frame_number > frame_count)
	{
		/* ISO TD expired - retire the TD to the Done Queue and continue with
           the next ISO TD of the same ED */
		//trace_usb_ohci_iso_td_relative_frame_number_big(relative_frame_number,
		//                                                frame_count);
		if (OHCI_CC_DATAOVERRUN == OHCI_BM(iso_td.flags, TD_CC))
		{
			/* avoid infinite loop */
			return 1;
		}
		OHCI_SET_BM(iso_td.flags, TD_CC, OHCI_CC_DATAOVERRUN);
		ed->head &= ~OHCI_DPTR_MASK;
		ed->head |= (iso_td.next & OHCI_DPTR_MASK);
		iso_td.next = ohci->done;
		ohci->done = addr;
		i = OHCI_BM(iso_td.flags, TD_DI);
		if (i < ohci->done_count)
			ohci->done_count = i;
		if (!ohci_put_iso_td(ohci, addr, &iso_td))
		{
			ohci_die(ohci);
			return 1;
		}
		return 0;
	}

	dir = OHCI_BM(ed->flags, ED_D);
	switch (dir)
	{
		case OHCI_TD_DIR_IN:
			str = "in";
			pid = USB_TOKEN_IN;
			break;
		case OHCI_TD_DIR_OUT:
			str = "out";
			pid = USB_TOKEN_OUT;
			break;
		case OHCI_TD_DIR_SETUP:
			str = "setup";
			pid = USB_TOKEN_SETUP;
			break;
		default:
			//trace_usb_ohci_iso_td_bad_direction(dir);
			return 1;
	}

	if (!iso_td.bp || !iso_td.be)
	{
		//trace_usb_ohci_iso_td_bad_bp_be(iso_td.bp, iso_td.be);
		return 1;
	}

	start_offset = iso_td.offset[relative_frame_number];
	if (relative_frame_number < frame_count)
	{
		next_offset = iso_td.offset[relative_frame_number + 1];
	}
	else
	{
		next_offset = iso_td.be;
	}

	if (!(OHCI_BM(start_offset, TD_PSW_CC) & 0xe) ||
		((relative_frame_number < frame_count) &&
		 !(OHCI_BM(next_offset, TD_PSW_CC) & 0xe)))
	{
		//trace_usb_ohci_iso_td_bad_cc_not_accessed(start_offset, next_offset);
		return 1;
	}

	if ((relative_frame_number < frame_count) && (start_offset > next_offset))
	{
		//trace_usb_ohci_iso_td_bad_cc_overrun(start_offset, next_offset);
		return 1;
	}

	if ((start_offset & 0x1000) == 0)
	{
		start_addr = (iso_td.bp & OHCI_PAGE_MASK) |
					 (start_offset & OHCI_OFFSET_MASK);
	}
	else
	{
		start_addr = (iso_td.be & OHCI_PAGE_MASK) |
					 (start_offset & OHCI_OFFSET_MASK);
	}

	if (relative_frame_number < frame_count)
	{
		end_offset = next_offset - 1;
		if ((end_offset & 0x1000) == 0)
		{
			end_addr = (iso_td.bp & OHCI_PAGE_MASK) |
					   (end_offset & OHCI_OFFSET_MASK);
		}
		else
		{
			end_addr = (iso_td.be & OHCI_PAGE_MASK) |
					   (end_offset & OHCI_OFFSET_MASK);
		}
	}
	else
	{
		/* Last packet in the ISO TD */
		end_addr = next_offset;
	}

	if (start_addr > end_addr)
	{
		//trace_usb_ohci_iso_td_bad_cc_overrun(start_addr, end_addr);
		return 1;
	}

	if ((start_addr & OHCI_PAGE_MASK) != (end_addr & OHCI_PAGE_MASK))
	{
		len = (end_addr & OHCI_OFFSET_MASK) + 0x1001 - (start_addr & OHCI_OFFSET_MASK);
	}
	else
	{
		len = end_addr - start_addr + 1;
	}
	if (len > sizeof(ohci->usb_buf))
	{
		len = sizeof(ohci->usb_buf);
	}

	if (len && dir != OHCI_TD_DIR_IN)
	{
		if (ohci_copy_iso_td(ohci, start_addr, end_addr, ohci->usb_buf, len,
							 DMA_DIRECTION_TO_DEVICE))
		{
			ohci_die(ohci);
			return 1;
		}
	}

	if (!completion)
	{
		bool int_req = relative_frame_number == frame_count &&
					   OHCI_BM(iso_td.flags, TD_DI) == 0;
		dev = ohci_find_device(ohci, OHCI_BM(ed->flags, ED_FA));
		if (dev == NULL)
		{
			//trace_usb_ohci_td_dev_error();
			return 1;
		}
		ep = usb_ep_get(dev, pid, OHCI_BM(ed->flags, ED_EN));
		usb_packet_setup(&ohci->usb_packet, pid, ep, 0, addr, false, int_req);
		usb_packet_addbuf(&ohci->usb_packet, ohci->usb_buf, len);
		usb_handle_packet(dev, &ohci->usb_packet);
		if (ohci->usb_packet.status == USB_RET_ASYNC)
		{
			usb_device_flush_ep_queue(dev, ep);
			return 1;
		}
	}
	if (ohci->usb_packet.status == USB_RET_SUCCESS)
	{
		ret = ohci->usb_packet.actual_length;
	}
	else
	{
		ret = ohci->usb_packet.status;
	}

	//trace_usb_ohci_iso_td_so(start_offset, end_offset, start_addr, end_addr,
	//                         str, len, ret);

	/* Writeback */
	if (dir == OHCI_TD_DIR_IN && ret >= 0 && ret <= (int)len)
	{
		/* IN transfer succeeded */
		if (ohci_copy_iso_td(ohci, start_addr, end_addr, ohci->usb_buf, ret,
							 DMA_DIRECTION_FROM_DEVICE))
		{
			ohci_die(ohci);
			return 1;
		}
		OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
					OHCI_CC_NOERROR);
		OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_SIZE, ret);
	}
	else if (dir == OHCI_TD_DIR_OUT && (ret == (int)len))
	{
		/* OUT transfer succeeded */
		OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
					OHCI_CC_NOERROR);
		OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_SIZE, 0);
	}
	else
	{
		if (ret > (ssize_t)len)
		{
			//trace_usb_ohci_iso_td_data_overrun(ret, len);
			OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
						OHCI_CC_DATAOVERRUN);
			OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_SIZE,
						len);
		}
		else if (ret >= 0)
		{
			//trace_usb_ohci_iso_td_data_underrun(ret);
			OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
						OHCI_CC_DATAUNDERRUN);
		}
		else
		{
			switch (ret)
			{
				case USB_RET_IOERROR:
				case USB_RET_NODEV:
					OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
								OHCI_CC_DEVICENOTRESPONDING);
					OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_SIZE,
								0);
					break;
				case USB_RET_NAK:
				case USB_RET_STALL:
					//trace_usb_ohci_iso_td_nak(ret);
					OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
								OHCI_CC_STALL);
					OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_SIZE,
								0);
					break;
				default:
					//trace_usb_ohci_iso_td_bad_response(ret);
					OHCI_SET_BM(iso_td.offset[relative_frame_number], TD_PSW_CC,
								OHCI_CC_UNDEXPETEDPID);
					break;
			}
		}
	}

	if (relative_frame_number == frame_count)
	{
		/* Last data packet of ISO TD - retire the TD to the Done Queue */
		OHCI_SET_BM(iso_td.flags, TD_CC, OHCI_CC_NOERROR);
		ed->head &= ~OHCI_DPTR_MASK;
		ed->head |= (iso_td.next & OHCI_DPTR_MASK);
		iso_td.next = ohci->done;
		ohci->done = addr;
		i = OHCI_BM(iso_td.flags, TD_DI);
		if (i < ohci->done_count)
			ohci->done_count = i;
	}
	if (!ohci_put_iso_td(ohci, addr, &iso_td))
	{
		ohci_die(ohci);
	}
	return 1;
}

/* Service a transport descriptor.
   Returns nonzero to terminate processing of this endpoint.  */

static int ohci_service_td(OHCIState* ohci, struct ohci_ed* ed)
{
	int dir;
	uint32_t len = 0, pktlen = 0;
	[[maybe_unused]]const char* str = NULL;
	int pid;
	int ret;
	int i;
	USBDevice* dev;
	USBEndpoint* ep;
	struct ohci_td td;
	uint32_t addr;
	int flag_r;
	int completion;

	addr = ed->head & OHCI_DPTR_MASK;
	/* See if this TD has already been submitted to the device.  */
	completion = (addr == ohci->async_td);
	if (completion && !ohci->async_complete)
	{
		//trace_usb_ohci_td_skip_async();
		return 1;
	}
	if (!ohci_read_td(ohci, addr, &td))
	{
		//trace_usb_ohci_td_read_error(addr);
		ohci_die(ohci);
		return 1;
	}

	dir = OHCI_BM(ed->flags, ED_D);
	switch (dir)
	{
		case OHCI_TD_DIR_OUT:
		case OHCI_TD_DIR_IN:
			/* Same value.  */
			break;
		default:
			dir = OHCI_BM(td.flags, TD_DP);
			break;
	}

	switch (dir)
	{
		case OHCI_TD_DIR_IN:
			str = "in";
			pid = USB_TOKEN_IN;
			break;
		case OHCI_TD_DIR_OUT:
			str = "out";
			pid = USB_TOKEN_OUT;
			break;
		case OHCI_TD_DIR_SETUP:
			str = "setup";
			pid = USB_TOKEN_SETUP;
			break;
		default:
			//trace_usb_ohci_td_bad_direction(dir);
			return 1;
	}
	if (td.cbp && td.be)
	{
		if ((td.cbp & 0xfffff000) != (td.be & 0xfffff000))
		{
			len = (td.be & 0xfff) + 0x1001 - (td.cbp & 0xfff);
		}
		else
		{
			if (td.cbp > td.be)
			{
				//trace_usb_ohci_iso_td_bad_cc_overrun(td.cbp, td.be);
				ohci_die(ohci);
				return 1;
			}
			len = (td.be - td.cbp) + 1;
		}
		if (len > sizeof(ohci->usb_buf))
		{
			len = sizeof(ohci->usb_buf);
		}

		pktlen = len;
		if (len && dir != OHCI_TD_DIR_IN)
		{
			/* The endpoint may not allow us to transfer it all now */
			pktlen = (ed->flags & OHCI_ED_MPS_MASK) >> OHCI_ED_MPS_SHIFT;
			if (pktlen > len)
			{
				pktlen = len;
			}
			if (!completion)
			{
				if (ohci_copy_td(ohci, &td, ohci->usb_buf, pktlen,
								 DMA_DIRECTION_TO_DEVICE))
				{
					ohci_die(ohci);
				}
			}
		}
	}

	flag_r = (td.flags & OHCI_TD_R) != 0;
	//trace_usb_ohci_td_pkt_hdr(addr, (int64_t)pktlen, (int64_t)len, str,
	//                          flag_r, td.cbp, td.be);
	//ohci_td_pkt("OUT", ohci->usb_buf, pktlen);

	if (completion)
	{
		ohci->async_td = 0;
		ohci->async_complete = false;
	}
	else
	{
		if (ohci->async_td)
		{
			/* ??? The hardware should allow one active packet per
               endpoint.  We only allow one active packet per controller.
               This should be sufficient as long as devices respond in a
               timely manner.
            */
			//trace_usb_ohci_td_too_many_pending();
			return 1;
		}
		dev = ohci_find_device(ohci, OHCI_BM(ed->flags, ED_FA));
		if (dev == NULL)
		{
			//trace_usb_ohci_td_dev_error();
			return 1;
		}
		ep = usb_ep_get(dev, pid, OHCI_BM(ed->flags, ED_EN));
		usb_packet_setup(&ohci->usb_packet, pid, ep, 0, addr, !flag_r,
						 OHCI_BM(td.flags, TD_DI) == 0);
		usb_packet_addbuf(&ohci->usb_packet, ohci->usb_buf, pktlen);
		usb_handle_packet(dev, &ohci->usb_packet);
		//trace_usb_ohci_td_packet_status(ohci->usb_packet.status);

		if (ohci->usb_packet.status == USB_RET_ASYNC)
		{
			usb_device_flush_ep_queue(dev, ep);
			ohci->async_td = addr;
			return 1;
		}
	}
	if (ohci->usb_packet.status == USB_RET_SUCCESS)
	{
		ret = ohci->usb_packet.actual_length;
	}
	else
	{
		ret = ohci->usb_packet.status;
	}

	if (ret >= 0)
	{
		if (dir == OHCI_TD_DIR_IN)
		{
			if (ohci_copy_td(ohci, &td, ohci->usb_buf, ret,
							 DMA_DIRECTION_FROM_DEVICE))
			{
				ohci_die(ohci);
			}
			//ohci_td_pkt("IN", ohci->usb_buf, pktlen);
		}
		else
		{
			ret = pktlen;
		}
	}

	/* Writeback */
	if (ret == (int)pktlen || (dir == OHCI_TD_DIR_IN && ret >= 0 && flag_r))
	{
		/* Transmission succeeded.  */
		if (ret == (int)len)
		{
			td.cbp = 0;
		}
		else
		{
			if ((td.cbp & 0xfff) + ret > 0xfff)
			{
				td.cbp = (td.be & ~0xfff) + ((td.cbp + ret) & 0xfff);
			}
			else
			{
				td.cbp += ret;
			}
		}
		td.flags |= OHCI_TD_T1;
		td.flags ^= OHCI_TD_T0;
		OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_NOERROR);
		OHCI_SET_BM(td.flags, TD_EC, 0);

		if ((dir != OHCI_TD_DIR_IN) && (ret != (int)len))
		{
			/* Partial packet transfer: TD not ready to retire yet */
			goto exit_no_retire;
		}

		/* Setting ED_C is part of the TD retirement process */
		ed->head &= ~OHCI_ED_C;
		if (td.flags & OHCI_TD_T0)
			ed->head |= OHCI_ED_C;
	}
	else
	{
		if (ret >= 0)
		{
			//trace_usb_ohci_td_underrun();
			OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_DATAUNDERRUN);
		}
		else
		{
			switch (ret)
			{
				case USB_RET_IOERROR:
				case USB_RET_NODEV:
					//trace_usb_ohci_td_dev_error();
					OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_DEVICENOTRESPONDING);
					break;
				case USB_RET_NAK:
					//trace_usb_ohci_td_nak();
					return 1;
				case USB_RET_STALL:
					//trace_usb_ohci_td_stall();
					OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_STALL);
					break;
				case USB_RET_BABBLE:
					//trace_usb_ohci_td_babble();
					OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_DATAOVERRUN);
					break;
				default:
					//trace_usb_ohci_td_bad_device_response(ret);
					OHCI_SET_BM(td.flags, TD_CC, OHCI_CC_UNDEXPETEDPID);
					OHCI_SET_BM(td.flags, TD_EC, 3);
					break;
			}
			/* An error occured so we have to clear the interrupt counter. See
             * spec at 6.4.4 on page 104 */
			ohci->done_count = 0;
		}
		ed->head |= OHCI_ED_H;
	}

	/* Retire this TD */
	ed->head &= ~OHCI_DPTR_MASK;
	ed->head |= td.next & OHCI_DPTR_MASK;
	td.next = ohci->done;
	ohci->done = addr;
	i = OHCI_BM(td.flags, TD_DI);
	if (i < ohci->done_count)
		ohci->done_count = i;
exit_no_retire:
	if (!ohci_put_td(ohci, addr, &td))
	{
		ohci_die(ohci);
		return 1;
	}
	return OHCI_BM(td.flags, TD_CC) != OHCI_CC_NOERROR;
}

/* Service an endpoint list.  Returns nonzero if active TD were found.  */
static int ohci_service_ed_list(OHCIState* ohci, uint32_t head, int completion)
{
	struct ohci_ed ed;
	uint32_t next_ed;
	uint32_t cur;
	int active;
	uint32_t link_cnt = 0;
	active = 0;

	if (head == 0)
		return 0;

	for (cur = head; cur && link_cnt++ < ED_LINK_LIMIT; cur = next_ed)
	{
		if (!ohci_read_ed(ohci, cur, &ed))
		{
			//trace_usb_ohci_ed_read_error(cur);
			ohci_die(ohci);
			return 0;
		}

		next_ed = ed.next & OHCI_DPTR_MASK;

		if ((ed.head & OHCI_ED_H) || (ed.flags & OHCI_ED_K))
		{
			uint32_t addr;
			/* Cancel pending packets for ED that have been paused.  */
			addr = ed.head & OHCI_DPTR_MASK;
			if (ohci->async_td && addr == ohci->async_td)
			{
				usb_cancel_packet(&ohci->usb_packet);
				ohci->async_td = 0;
				usb_device_ep_stopped(ohci->usb_packet.ep->dev,
									  ohci->usb_packet.ep);
			}
			continue;
		}

		while ((ed.head & OHCI_DPTR_MASK) != ed.tail)
		{
			/*trace_usb_ohci_ed_pkt(cur, (ed.head & OHCI_ED_H) != 0,
                    (ed.head & OHCI_ED_C) != 0, ed.head & OHCI_DPTR_MASK,
                    ed.tail & OHCI_DPTR_MASK, ed.next & OHCI_DPTR_MASK);
            trace_usb_ohci_ed_pkt_flags(
                    OHCI_BM(ed.flags, ED_FA), OHCI_BM(ed.flags, ED_EN),
                    OHCI_BM(ed.flags, ED_D), (ed.flags & OHCI_ED_S)!= 0,
                    (ed.flags & OHCI_ED_K) != 0, (ed.flags & OHCI_ED_F) != 0,
                    OHCI_BM(ed.flags, ED_MPS));*/

			active = 1;

			if ((ed.flags & OHCI_ED_F) == 0)
			{
				if (ohci_service_td(ohci, &ed))
					break;
			}
			else
			{
				/* Handle isochronous endpoints */
				if (ohci_service_iso_td(ohci, &ed, completion))
					break;
			}
		}

		if (!ohci_put_ed(ohci, cur, &ed))
		{
			ohci_die(ohci);
			return 0;
		}
	}

	return active;
}

/* Generate a SOF event, and set a timer for EOF */
static void ohci_sof(OHCIState* ohci)
{
	ohci->sof_time = get_clock();
	ohci->eof_timer = usb_frame_time;
	ohci_set_interrupt(ohci, OHCI_INTR_SF);
}

/* Process Control and Bulk lists.  */
static void ohci_process_lists(OHCIState* ohci, int completion)
{
	if ((ohci->ctl & OHCI_CTL_CLE) && (ohci->status & OHCI_STATUS_CLF))
	{
		if (ohci->ctrl_cur && ohci->ctrl_cur != ohci->ctrl_head)
		{
		}
		if (!ohci_service_ed_list(ohci, ohci->ctrl_head, completion))
		{
			ohci->ctrl_cur = 0;
			ohci->status &= ~OHCI_STATUS_CLF;
		}
	}

	if ((ohci->ctl & OHCI_CTL_BLE) && (ohci->status & OHCI_STATUS_BLF))
	{
		if (!ohci_service_ed_list(ohci, ohci->bulk_head, completion))
		{
			ohci->bulk_cur = 0;
			ohci->status &= ~OHCI_STATUS_BLF;
		}
	}
}

/* Do frame processing on frame boundary */
void ohci_frame_boundary(void* opaque)
{
	OHCIState* ohci = (OHCIState*)opaque;
	struct ohci_hcca hcca;

	cpu_physical_memory_read(ohci->hcca, (uint8_t*)&hcca, sizeof(hcca));

	/* Process all the lists at the end of the frame */
	/* if reset bit was set, don't process possibly invalid descriptors */
	/* TODO intr_status is interrupts that driver wants, so not quite right to us it here */
	bool hack = false; // ohci->intr_status & ohci->intr & OHCI_INTR_RHSC;

	/* Process all the lists at the end of the frame */
	if ((ohci->ctl & OHCI_CTL_PLE) && !hack)
	{
		int n;

		n = ohci->frame_number & 0x1f;
		ohci_service_ed_list(ohci, le32_to_cpu(hcca.intr[n]), 0);
	}

	/* Cancel all pending packets if either of the lists has been disabled.  */
	if (ohci->old_ctl & (~ohci->ctl) & (OHCI_CTL_BLE | OHCI_CTL_CLE))
	{
		if (ohci->async_td)
		{
			usb_cancel_packet(&ohci->usb_packet);
			ohci->async_td = 0;
		}
		ohci_stop_endpoints(ohci);
	}
	ohci->old_ctl = ohci->ctl;
	ohci_process_lists(ohci, 0);

	/* Stop if UnrecoverableError happened or ohci_sof will crash */
	if (ohci->intr_status & OHCI_INTR_UE)
	{
		return;
	}

	/* Frame boundary, so do EOF stuf here */
	ohci->frt = ohci->fit;

	/* Increment frame number and take care of endianness. */
	ohci->frame_number = (ohci->frame_number + 1) & 0xffff;
	hcca.frame = cpu_to_le16(ohci->frame_number);

	if (ohci->done_count == 0 && !(ohci->intr_status & OHCI_INTR_WD))
	{
		if (!ohci->done)
			abort();
		if (ohci->intr & ohci->intr_status)
			ohci->done |= 1;
		hcca.done = cpu_to_le32(ohci->done);
		ohci->done = 0;
		ohci->done_count = 7;
		ohci_set_interrupt(ohci, OHCI_INTR_WD);
	}

	if (ohci->done_count != 7 && ohci->done_count != 0)
		ohci->done_count--;

	/* Do SOF stuff here */
	ohci_sof(ohci);

	/* Writeback HCCA */
	ohci_put_hcca(ohci, &hcca);
}

/* Start sending SOF tokens across the USB bus, lists are processed in
 * next frame
 */
int ohci_bus_start(OHCIState* ohci)
{
	ohci->eof_timer = 0;


	ohci_sof(ohci);

	return 1;
}

/* Stop sending SOF tokens on the bus */
void ohci_bus_stop(OHCIState* ohci)
{
	if (ohci->eof_timer)
		ohci->eof_timer = 0;
}

/* Sets a flag in a port status register but only set it if the port is
 * connected, if not set ConnectStatusChange flag. If flag is enabled
 * return 1.
 */
static int ohci_port_set_if_connected(OHCIState* ohci, int i, uint32_t val)
{
	int ret = 1;

	/* writing a 0 has no effect */
	if (val == 0)
		return 0;

	/* If CurrentConnectStatus is cleared we set
     * ConnectStatusChange
     */
	if (!(ohci->rhport[i].ctrl & OHCI_PORT_CCS))
	{
		ohci->rhport[i].ctrl |= OHCI_PORT_CSC;
		if (ohci->rhstatus & OHCI_RHS_DRWE)
		{
			/* TODO: CSC is a wakeup event */
		}
		return 0;
	}

	if (ohci->rhport[i].ctrl & val)
		ret = 0;

	/* set the bit */
	ohci->rhport[i].ctrl |= val;

	return ret;
}

/* Set the frame interval - frame interval toggle is manipulated by the hcd only */
static void ohci_set_frame_interval(OHCIState* ohci, uint16_t val)
{
	val &= OHCI_FMI_FI;

	if (val != ohci->fi)
	{
	}

	ohci->fi = val;
}

static void ohci_port_power(OHCIState* ohci, int i, int p)
{
	if (p)
	{
		ohci->rhport[i].ctrl |= OHCI_PORT_PPS;
	}
	else
	{
		ohci->rhport[i].ctrl &= ~(OHCI_PORT_PPS |
								  OHCI_PORT_CCS |
								  OHCI_PORT_PSS |
								  OHCI_PORT_PRS);
	}
}

/* Set HcControlRegister */
static void ohci_set_ctl(OHCIState* ohci, uint32_t val)
{
	uint32_t old_state;
	uint32_t new_state;

	old_state = ohci->ctl & OHCI_CTL_HCFS;
	ohci->ctl = val;
	new_state = ohci->ctl & OHCI_CTL_HCFS;

	/* no state change */
	if (old_state == new_state)
		return;

	switch (new_state)
	{
		case OHCI_USB_OPERATIONAL:
			ohci_bus_start(ohci);
			break;
		case OHCI_USB_SUSPEND:
			ohci_bus_stop(ohci);
			/* clear pending SF otherwise linux driver loops in ohci_irq() */
			ohci->intr_status &= ~OHCI_INTR_SF;
			ohci_intr_update(ohci);
			break;
		case OHCI_USB_RESUME:
			//trace_usb_ohci_resume(ohci->name);
			break;
		case OHCI_USB_RESET:
			ohci_roothub_reset(ohci);
			break;
	}
	//ohci_intr_update(ohci);
}

static uint32_t ohci_get_frame_remaining(OHCIState* ohci)
{
	uint16_t fr;
	int64_t tks;

	if ((ohci->ctl & OHCI_CTL_HCFS) != OHCI_USB_OPERATIONAL)
		return (ohci->frt << 31);

	/* Being in USB operational state guarnatees sof_time was
     * set already.
     */
	tks = get_clock() - ohci->sof_time;

	/* avoid muldiv if possible */
	if (tks >= usb_frame_time)
		return (ohci->frt << 31);

	tks = muldiv64(1, tks, usb_bit_time);
	fr = (uint16_t)(ohci->fi - tks);

	return (ohci->frt << 31) | fr;
}


/* Set root hub status */
static void ohci_set_hub_status(OHCIState* ohci, uint32_t val)
{
	uint32_t old_state;

	old_state = ohci->rhstatus;

	/* write 1 to clear OCIC */
	if (val & OHCI_RHS_OCIC)
		ohci->rhstatus &= ~OHCI_RHS_OCIC;

	if (val & OHCI_RHS_LPS)
	{
		for (unsigned int i = 0; i < ohci->num_ports; i++)
			ohci_port_power(ohci, i, 0);
	}

	if (val & OHCI_RHS_LPSC)
	{
		for (unsigned int i = 0; i < ohci->num_ports; i++)
			ohci_port_power(ohci, i, 1);
	}

	if (val & OHCI_RHS_DRWE)
		ohci->rhstatus |= OHCI_RHS_DRWE;

	if (val & OHCI_RHS_CRWE)
		ohci->rhstatus &= ~OHCI_RHS_DRWE;

	if (old_state != ohci->rhstatus)
		ohci_set_interrupt(ohci, OHCI_INTR_RHSC);
}

/* Set root hub port status */
static void ohci_port_set_status(OHCIState* ohci, int portnum, uint32_t val)
{
	uint32_t old_state;
	OHCIPort* port;

	port = &ohci->rhport[portnum];
	old_state = port->ctrl;

	/* Write to clear CSC, PESC, PSSC, OCIC, PRSC */
	if (val & OHCI_PORT_WTC)
		port->ctrl &= ~(val & OHCI_PORT_WTC);

	if (val & OHCI_PORT_CCS)
		port->ctrl &= ~OHCI_PORT_PES;

	ohci_port_set_if_connected(ohci, portnum, val & OHCI_PORT_PES);

	if (ohci_port_set_if_connected(ohci, portnum, val & OHCI_PORT_PSS))
	{
	}

	if (ohci_port_set_if_connected(ohci, portnum, val & OHCI_PORT_PRS))
	{
		usb_device_reset(port->port.dev);
		port->ctrl &= ~OHCI_PORT_PRS;
		/* ??? Should this also set OHCI_PORT_PESC.  */
		port->ctrl |= OHCI_PORT_PES | OHCI_PORT_PRSC;
	}

	/* Invert order here to ensure in ambiguous case, device is
     * powered up...
     */
	if (val & OHCI_PORT_LSDA)
		ohci_port_power(ohci, portnum, 0);
	if (val & OHCI_PORT_PPS)
		ohci_port_power(ohci, portnum, 1);

	if (old_state != port->ctrl)
		ohci_set_interrupt(ohci, OHCI_INTR_RHSC);
}

#ifdef DEBUG_OHCI
static const char* reg_names[] = {
	"HcRevision",
	"HcControl",
	"HcCommandStatus",
	"HcInterruptStatus",
	"HcInterruptEnable",
	"HcInterruptDisable",
	"HcHCCA",
	"HcPeriodCurrentED",
	"HcControlHeadED",
	"HcControlCurrentED",
	"HcBulkHeadED",
	"HcBulkCurrentED",
	"HcDoneHead",
	"HcFmInterval",
	"HcFmRemaining",
	"HcFmNumber",
	"HcPeriodicStart",
	"HcLSThreshold",
	"HcRhDescriptorA",
	"HcRhDescriptorB",
	"HcRhStatus",
};

uint32_t ohci_mem_read_impl(OHCIState* ptr, uint32_t addr);
uint32_t ohci_mem_read(OHCIState* ptr, uint32_t addr)
{
	auto val = ohci_mem_read_impl(ptr, addr);
	int idx = (addr - ptr->mem_base) >> 2;
	if (idx < countof(reg_names))
	{
		Console.Warning("ohci_mem_read %s(%d): %08x\n", reg_names[idx], idx, val);
	}
	return val;
}

uint32_t ohci_mem_read_impl(OHCIState* ptr, uint32_t addr)
#else
uint32_t ohci_mem_read(OHCIState* ptr, uint32_t addr)
#endif
{
	OHCIState* ohci = ptr;

	addr -= ohci->mem_base;

	/* Only aligned reads are allowed on OHCI */
	if (addr & 3)
	{
		return 0xffffffff;
	}

	if (addr >= 0x54 && addr < 0x54 + ohci->num_ports * 4)
	{
		/* HcRhPortStatus */
		return ohci->rhport[(addr - 0x54) >> 2].ctrl | OHCI_PORT_PPS;
	}
	switch (addr >> 2)
	{
		case 0: /* HcRevision */
			return 0x10;

		case 1: /* HcControl */
			return ohci->ctl;

		case 2: /* HcCommandStatus */
			return ohci->status;

		case 3: /* HcInterruptStatus */
			return ohci->intr_status;

		case 4: /* HcInterruptEnable */
		case 5: /* HcInterruptDisable */
			return ohci->intr;

		case 6: /* HcHCCA */
			return ohci->hcca;

		case 7: /* HcPeriodCurrentED */
			return ohci->per_cur;

		case 8: /* HcControlHeadED */
			return ohci->ctrl_head;

		case 9: /* HcControlCurrentED */
			return ohci->ctrl_cur;

		case 10: /* HcBulkHeadED */
			return ohci->bulk_head;

		case 11: /* HcBulkCurrentED */
			return ohci->bulk_cur;

		case 12: /* HcDoneHead */
			return ohci->done;

		case 13: /* HcFmInterval */
			return (ohci->fit << 31) | (ohci->fsmps << 16) | (ohci->fi);

		case 14: /* HcFmRemaining */
			return ohci_get_frame_remaining(ohci);

		case 15: /* HcFmNumber */
			return ohci->frame_number;

		case 16: /* HcPeriodicStart */
			return ohci->pstart;

		case 17: /* HcLSThreshold */
			return ohci->lst;

		case 18: /* HcRhDescriptorA */
			return ohci->rhdesc_a;

		case 19: /* HcRhDescriptorB */
			return ohci->rhdesc_b;

		case 20: /* HcRhStatus */
			return ohci->rhstatus;

		default:
			return 0xffffffff;
	}
}

#ifdef DEBUG_OHCI
void ohci_mem_write_impl(OHCIState* ptr, uint32_t addr, uint32_t val);
void ohci_mem_write(OHCIState* ptr, uint32_t addr, uint32_t val)
{
	int idx = (addr - ptr->mem_base) >> 2;
	if (idx < countof(reg_names))
	{
		Console.Warning("ohci_mem_write %s(%d): %08x\n", reg_names[idx], idx, val);
	}
	ohci_mem_write_impl(ptr, addr, val);
}

void ohci_mem_write_impl(OHCIState* ptr, uint32_t addr, uint32_t val)
#else
void ohci_mem_write(OHCIState* ptr, uint32_t addr, uint32_t val)
#endif
{
	OHCIState* ohci = ptr;

	addr -= ohci->mem_base;

	/* Only aligned reads are allowed on OHCI */
	if (addr & 3)
	{
		Console.Warning("usb-ohci: Mis-aligned write\n");
		return;
	}

	if ((addr >= 0x54) && (addr < (0x54 + ohci->num_ports * 4)))
	{
		/* HcRhPortStatus */
		ohci_port_set_status(ohci, (addr - 0x54) >> 2, val);
		return;
	}
	switch (addr >> 2)
	{
		case 1: /* HcControl */
			ohci_set_ctl(ohci, val);
			break;

		case 2: /* HcCommandStatus */
			/* SOC is read-only */
			val = (val & ~OHCI_STATUS_SOC);

			/* Bits written as '0' remain unchanged in the register */
			ohci->status |= val;

			if (ohci->status & OHCI_STATUS_HCR)
				ohci_soft_reset(ohci);
			break;

		case 3: /* HcInterruptStatus */
			ohci->intr_status &= ~val;
			ohci_intr_update(ohci);
			break;

		case 4: /* HcInterruptEnable */
			ohci->intr |= val;
			ohci_intr_update(ohci);
			break;

		case 5: /* HcInterruptDisable */
			ohci->intr &= ~val;
			ohci_intr_update(ohci);
			break;

		case 6: /* HcHCCA */
			ohci->hcca = val & OHCI_HCCA_MASK;
			break;

		case 8: /* HcControlHeadED */
			ohci->ctrl_head = val & OHCI_EDPTR_MASK;
			break;

		case 9: /* HcControlCurrentED */
			ohci->ctrl_cur = val & OHCI_EDPTR_MASK;
			break;

		case 10: /* HcBulkHeadED */
			ohci->bulk_head = val & OHCI_EDPTR_MASK;
			break;

		case 11: /* HcBulkCurrentED */
			ohci->bulk_cur = val & OHCI_EDPTR_MASK;
			break;

		case 13: /* HcFmInterval */
			ohci->fsmps = (val & OHCI_FMI_FSMPS) >> 16;
			ohci->fit = (val & OHCI_FMI_FIT) >> 31;
			ohci_set_frame_interval(ohci, val);
			break;

		case 16: /* HcPeriodicStart */
			ohci->pstart = val & 0xffff;
			break;

		case 17: /* HcLSThreshold */
			ohci->lst = val & 0xffff;
			break;

		case 18: /* HcRhDescriptorA */
			ohci->rhdesc_a &= ~OHCI_RHA_RW_MASK;
			ohci->rhdesc_a |= val & OHCI_RHA_RW_MASK;
			break;

		case 19: /* HcRhDescriptorB */
			break;

		case 20: /* HcRhStatus */
			ohci_set_hub_status(ohci, val);
			break;

		default:
			break;
	}
}

static void ohci_async_cancel_device(OHCIState* ohci, USBDevice* dev)
{
	if (ohci->async_td &&
		usb_packet_is_inflight(&ohci->usb_packet) &&
		ohci->usb_packet.ep->dev == dev)
	{
		usb_cancel_packet(&ohci->usb_packet);
		ohci->async_td = 0;
	}
}

static USBPortOps ohci_port_ops = {
	/*.attach =*/ohci_attach,
	/*.detach =*/ohci_detach,
	//.child_detach = ohci_child_detach,
	/*.wakeup =*/ohci_wakeup,
	/*.complete =*/ohci_async_complete_packet,
};

OHCIState* ohci_create(uint32_t base, int ports)
{
	OHCIState* ohci = (OHCIState*)malloc(sizeof(OHCIState));
	if (!ohci)
		return NULL;
	int i;

	const int ticks_per_sec = get_ticks_per_second();

	memset(ohci, 0, sizeof(OHCIState));

	ohci->mem_base = base;

	if (usb_frame_time == 0)
	{
#if OHCI_TIME_WARP
		usb_frame_time = ticks_per_sec;
		usb_bit_time = muldiv64(1, ticks_per_sec, USB_HZ / 1000);
#else
		usb_frame_time = muldiv64(1, ticks_per_sec, 1000);
		if (ticks_per_sec >= USB_HZ)
		{
			usb_bit_time = muldiv64(1, ticks_per_sec, USB_HZ);
		}
		else
		{
			usb_bit_time = 1;
		}
#endif
	}

	ohci->num_ports = ports;
	for (i = 0; i < ports; i++)
	{
		memset(&(ohci->rhport[i].port), 0, sizeof(USBPort));
		ohci->rhport[i].port.opaque = ohci;
		ohci->rhport[i].port.index = i;
		ohci->rhport[i].port.speedmask = USB_SPEED_MASK_LOW | USB_SPEED_MASK_FULL;
		ohci->rhport[i].port.ops = &ohci_port_ops;
	}

	ohci_hard_reset(ohci);
	usb_packet_init(&ohci->usb_packet);

	return ohci;
}
