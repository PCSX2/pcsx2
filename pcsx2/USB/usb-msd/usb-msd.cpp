/*
 * USB Mass Storage Device emulation
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licenced under the LGPL.
 */

#include "PrecompiledHeader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "USB/qemu-usb/vl.h"
#include "USB/qemu-usb/desc.h"
#include "usb-msd.h"

#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)

namespace usb_msd
{

	struct usb_msd_cbw
	{
		uint32_t sig;
		uint32_t tag;
		uint32_t data_len;
		uint8_t flags;
		uint8_t lun;
		uint8_t cmd_len;
		uint8_t cmd[16];
	};

	struct usb_msd_csw
	{
		uint32_t sig;
		uint32_t tag;
		uint32_t residue;
		uint8_t status;
	};

#define LBA_BLOCK_SIZE 512

/* USB requests.  */
#define MassStorageReset 0xff
#define GetMaxLun 0xfe

	enum USBMSDMode : int8_t
	{
		USB_MSDM_CBW,     /* Command Block.  */
		USB_MSDM_DATAOUT, /* Tranfer data to device.  */
		USB_MSDM_DATAIN,  /* Transfer data from device.  */
		USB_MSDM_CSW      /* Command Status.  */
	};

	typedef struct ReqState
	{
		uint32_t tag;
		//
		bool valid;
	} ReqState;

	typedef struct MSDState
	{
		USBDevice dev;

		struct freeze
		{
			struct usb_msd_csw csw;
			enum USBMSDMode mode;
			uint32_t data_len;
			uint32_t residue;
			uint32_t tag;
			uint32_t file_op_tag; // read from file or buf
			int32_t result;

			uint32_t off;      //buffer offset
			uint8_t buf[4096]; //random length right now
			uint8_t sense_buf[18];
			uint8_t last_cmd;
			ReqState req;

			//TODO how to detect if image is different
			uint32_t hash;
		} f; //freezable

		FILE* file;
		//char fn[MAX_PATH+1]; //TODO Could use with open/close,
		//but error recovery currently can't deal with file suddenly
		//becoming not accessible
		/* For async completion.  */
		USBPacket* packet;

		USBDesc desc;
		USBDescDevice desc_dev;
	} MSDState;

	static const uint8_t qemu_msd_dev_descriptor[] = {
		0x12,       /*  u8 bLength; */
		0x01,       /*  u8 bDescriptorType; Device */
		0x10, 0x00, /*  u16 bcdUSB; v1.0 */

		0x00, /*  u8  bDeviceClass; */
		0x00, /*  u8  bDeviceSubClass; */
		0x00, /*  u8  bDeviceProtocol; [ low/full speeds only ] */
		0x08, /*  u8  bMaxPacketSize0; 8 Bytes */

		/* Vendor and product id are arbitrary.  */
		0xf4, 0x46, /*  u16 idVendor; */
		0x01, 0x00, /*  u16 idProduct; */
		0x00, 0x00, /*  u16 bcdDevice */

		0x01, /*  u8  iManufacturer; */
		0x02, /*  u8  iProduct; */
		0x03, /*  u8  iSerialNumber; */
		0x01  /*  u8  bNumConfigurations; */
	};

	static const uint8_t qemu_msd_config_descriptor[] = {

		/* one configuration */
		0x09,       /*  u8  bLength; */
		0x02,       /*  u8  bDescriptorType; Configuration */
		0x20, 0x00, /*  u16 wTotalLength; */
		0x01,       /*  u8  bNumInterfaces; (1) */
		0x01,       /*  u8  bConfigurationValue; */
		0x00,       /*  u8  iConfiguration; */
		0xc0,       /*  u8  bmAttributes;
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		0x00,       /*  u8  MaxPower; */

		/* one interface */
		0x09, /*  u8  if_bLength; */
		0x04, /*  u8  if_bDescriptorType; Interface */
		0x00, /*  u8  if_bInterfaceNumber; */
		0x00, /*  u8  if_bAlternateSetting; */
		0x02, /*  u8  if_bNumEndpoints; */
		0x08, /*  u8  if_bInterfaceClass; MASS STORAGE */
		0x06, /*  u8  if_bInterfaceSubClass; SCSI */
		0x50, /*  u8  if_bInterfaceProtocol; Bulk Only */
		0x00, /*  u8  if_iInterface; */

		/* Bulk-In endpoint */
		0x07,       /*  u8  ep_bLength; */
		0x05,       /*  u8  ep_bDescriptorType; Endpoint */
		0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x02,       /*  u8  ep_bmAttributes; Bulk */
		0x40, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x00,       /*  u8  ep_bInterval; */

		/* Bulk-Out endpoint */
		0x07,       /*  u8  ep_bLength; */
		0x05,       /*  u8  ep_bDescriptorType; Endpoint */
		0x02,       /*  u8  ep_bEndpointAddress; OUT Endpoint 2 */
		0x02,       /*  u8  ep_bmAttributes; Bulk */
		0x00, 0x02, /*  u16 ep_wMaxPacketSize; */
		0x00        /*  u8  ep_bInterval; */
	};

	enum
	{
		STR_MANUFACTURER = 1,
		STR_PRODUCT,
		STR_SERIALNUMBER,
		STR_CONFIG_FULL,
	};

	static const USBDescStrings desc_strings = {
		"",
		/*[STR_MANUFACTURER] = */ "QEMU",
		/*[STR_PRODUCT]      = */ "QEMU USB HARDDRIVE",
		/*[STR_SERIALNUMBER] = */ "1",
		/*[STR_CONFIG_FULL]  = */ "Full speed config (usb 1.1)",
	};

	//static std::vector<USBDescEndpoint> desc_iface_full_eps
	//{
	//{USB_DIR_IN | 0x01, USB_ENDPOINT_XFER_BULK, 64},
	//{USB_DIR_OUT | 0x02, USB_ENDPOINT_XFER_BULK, 64},
	//};

	//static const USBDescIface desc_iface_full {
	///*.bInterfaceNumber              = */ 0,
	///*.bNumEndpoints                 = */ 2,
	///*.bInterfaceClass               = */ USB_CLASS_MASS_STORAGE,
	///*.bInterfaceSubClass            = */ 0x06, /* SCSI */
	///*.bInterfaceProtocol            = */ 0x50, /* Bulk */
	///*.eps = */ desc_iface_full_eps,
	//};

	//static const std::vector<USBDescConfig> desc_device_full_confs {
	//{
	///* .bNumInterfaces        = */ 1,
	///* .bConfigurationValue   = */ 1,
	///* .iConfiguration        = */ STR_CONFIG_FULL,
	///* .bmAttributes          = */ USB_CFG_ATT_ONE | USB_CFG_ATT_SELFPOWER,
	///* bMaxPower */  0,
	///* nif_groups  0,*/
	///* if_groups */  {},
	///* .nif = 1,*/
	///* .ifs = */ {desc_iface_full},
	//},
	//};

	//static const USBDescDevice desc_device_full = {
	///* .bcdUSB                        = */ 0x0200,
	///* .bDeviceClass */     0,
	///* .bDeviceSubClass */  0,
	///* .bDeviceProtocol */  0,
	///* .bMaxPacketSize0               = */ 8,
	///* .bNumConfigurations            = */ 1,
	///* .confs = */ desc_device_full_confs,
	//};

	//static const USBDesc desc = {
	///*.id = */ {
	///*.idVendor          = */ 0x46f4, /* CRC16() of "QEMU" */
	///*.idProduct         = */ 0x0001,
	///*.bcdDevice         = */ 0,
	///*.iManufacturer     = */ STR_MANUFACTURER,
	///*.iProduct          = */ STR_PRODUCT,
	///*.iSerialNumber     = */ STR_SERIALNUMBER,
	//},
	///*.full  = */ &desc_device_full,
	///*.high  = */ nullptr,
	///*.super = */ nullptr,
	///*.str   = */ desc_strings,
	///*.msos  = */ nullptr,
	//};

	/*
 *      SCSI opcodes
 */

#define TEST_UNIT_READY 0x00
#define REZERO_UNIT 0x01
#define REQUEST_SENSE 0x03
#define FORMAT_UNIT 0x04
#define READ_BLOCK_LIMITS 0x05
#define REASSIGN_BLOCKS 0x07
#define READ_6 0x08
#define WRITE_6 0x0a
#define SEEK_6 0x0b
#define READ_REVERSE 0x0f
#define WRITE_FILEMARKS 0x10
#define SPACE 0x11
#define INQUIRY 0x12
#define RECOVER_BUFFERED_DATA 0x14
#define MODE_SELECT 0x15
#define RESERVE 0x16
#define RELEASE 0x17
#define COPY 0x18
#define ERASE 0x19
#define MODE_SENSE 0x1a
#define START_STOP 0x1b
#define RECEIVE_DIAGNOSTIC 0x1c
#define SEND_DIAGNOSTIC 0x1d
#define ALLOW_MEDIUM_REMOVAL 0x1e

#define SET_WINDOW 0x24
#define READ_CAPACITY_10 0x25
#define READ_10 0x28
#define WRITE_10 0x2a
#define SEEK_10 0x2b
#define WRITE_VERIFY 0x2e
#define VERIFY 0x2f
#define SEARCH_HIGH 0x30
#define SEARCH_EQUAL 0x31
#define SEARCH_LOW 0x32
#define SET_LIMITS 0x33
#define PRE_FETCH 0x34
#define READ_POSITION 0x34
#define SYNCHRONIZE_CACHE 0x35
#define LOCK_UNLOCK_CACHE 0x36
#define READ_DEFECT_DATA 0x37
#define MEDIUM_SCAN 0x38
#define COMPARE 0x39
#define COPY_VERIFY 0x3a
#define WRITE_BUFFER 0x3b
#define READ_BUFFER 0x3c
#define UPDATE_BLOCK 0x3d
#define READ_LONG 0x3e
#define WRITE_LONG 0x3f
#define CHANGE_DEFINITION 0x40
#define WRITE_SAME 0x41
#define READ_TOC 0x43
#define LOG_SELECT 0x4c
#define LOG_SENSE 0x4d
#define MODE_SELECT_10 0x55
#define RESERVE_10 0x56
#define RELEASE_10 0x57
#define MODE_SENSE_10 0x5a
#define PERSISTENT_RESERVE_IN 0x5e
#define PERSISTENT_RESERVE_OUT 0x5f
#define MAINTENANCE_IN 0xa3
#define MAINTENANCE_OUT 0xa4
#define MOVE_MEDIUM 0xa5
#define READ_12 0xa8
#define WRITE_12 0xaa
#define WRITE_VERIFY_12 0xae
#define SEARCH_HIGH_12 0xb0
#define SEARCH_EQUAL_12 0xb1
#define SEARCH_LOW_12 0xb2
#define READ_ELEMENT_STATUS 0xb8
#define SEND_VOLUME_TAG 0xb6
#define WRITE_LONG_2 0xea

/* from hw/scsi-generic.c */
#define REWIND 0x01
#define REPORT_DENSITY_SUPPORT 0x44
#define GET_CONFIGURATION 0x46
#define READ_16 0x88
#define WRITE_16 0x8a
#define WRITE_VERIFY_16 0x8e
#define SERVICE_ACTION_IN 0x9e
#define REPORT_LUNS 0xa0
#define LOAD_UNLOAD 0xa6
#define SET_CD_SPEED 0xbb
#define BLANK 0xa1

	/*
 *  Status codes
 */

#define GOOD 0x00
#define CHECK_CONDITION 0x01
#define CONDITION_GOOD 0x02
#define BUSY 0x04
#define INTERMEDIATE_GOOD 0x08
#define INTERMEDIATE_C_GOOD 0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED 0x11
#define QUEUE_FULL 0x14

#define STATUS_MASK 0x3e

	/*
 *  SENSE KEYS
 */

#define NO_SENSE 0x00
#define RECOVERED_ERROR 0x01
#define NOT_READY 0x02
#define MEDIUM_ERROR 0x03
#define HARDWARE_ERROR 0x04
#define ILLEGAL_REQUEST 0x05
#define UNIT_ATTENTION 0x06
#define DATA_PROTECT 0x07
#define BLANK_CHECK 0x08
#define COPY_ABORTED 0x0a
#define ABORTED_COMMAND 0x0b
#define VOLUME_OVERFLOW 0x0d
#define MISCOMPARE 0x0e
/* Additional sense codes */
#define INVALID_COMMAND_OPERATION 0x20

/* CSW status codes */
#define COMMAND_PASSED 0x00 // GOOD
#define COMMAND_FAILED 0x01
#define PHASE_ERROR 0x02

	typedef struct SCSISense
	{
		uint8_t key;
		uint8_t asc;
		uint8_t ascq;
	} SCSISense;

#define SENSE_CODE(x) sense_code_##x

	/*
 * Predefined sense codes
 */

	/* No sense data available */
	const struct SCSISense sense_code_NO_SENSE = {
		NO_SENSE, 0x00, 0x00};

	/* LUN not ready, Manual intervention required */
	[[maybe_unused]]const struct SCSISense sense_code_LUN_NOT_READY = {
		NOT_READY, 0x04, 0x03};

	/* LUN not ready, Medium not present */
	[[maybe_unused]]const struct SCSISense sense_code_NO_MEDIUM = {
		NOT_READY, 0x3a, 0x00};

	const struct SCSISense sense_code_UNKNOWN_ERROR = {
		NOT_READY, 0xFF, 0xFF};

	const struct SCSISense sense_code_NO_SEEK_COMPLETE = {
		MEDIUM_ERROR, 0x02, 0x00};

	const struct SCSISense sense_code_WRITE_FAULT = {
		MEDIUM_ERROR, 0x03, 0x00};

	const struct SCSISense sense_code_UNRECOVERED_READ_ERROR = {
		MEDIUM_ERROR, 0x11, 0x00};

	const struct SCSISense sense_code_INVALID_OPCODE = {
		ILLEGAL_REQUEST, 0x20, 0x00};

	const struct SCSISense sense_code_OUT_OF_RANGE = {
		ILLEGAL_REQUEST, 0x21, 0x00};

	/* Illegal request, Invalid Transfer Tag */
	//const struct SCSISense sense_code_INVALID_TAG = {
	//    .key = ILLEGAL_REQUEST, .asc = 0x4b, .ascq = 0x01
	//};

	static int64_t get_file_size(FILE* file)
	{
		int fd;

#if defined(_WIN32)
		struct _stat64 buf;
		fd = _fileno(file);
		if (_fstat64(fd, &buf) != 0)
			return -1;
		return buf.st_size;
#elif defined(__GNUC__)
		struct stat64 buf;
		fd = fileno(file);
		if (fstat64(fd, &buf) != 0)
			return -1;
		return buf.st_size;
#else
#error Unknown platform
#endif
	}

	static void usb_msd_handle_reset(USBDevice* dev)
	{
		MSDState* s = (MSDState*)dev;

		s->f.mode = USB_MSDM_CBW;
	}

#ifndef bswap32
#define bswap32(x) (          \
	(((x) >> 24) & 0xff) |    \
	(((x) >> 8) & 0xff00) |   \
	(((x) << 8) & 0xff0000) | \
	(((x) << 24) & 0xff000000))

#define bswap16(x) ((((x) >> 8) & 0xff) | (((x) << 8) & 0xff00))
#endif

	static void set_sense(void* opaque, SCSISense sense)
	{
		MSDState* s = (MSDState*)opaque;
		memset(s->f.sense_buf, 0, sizeof(s->f.sense_buf));
		//SENSE request
		s->f.sense_buf[0] = 0x70 | 0x80; //0x70 - current sense, 0x80 - set Valid bit
		//s->f.sense_buf[1] = 0x00;
		s->f.sense_buf[2] = sense.key & 0x0F; //ILLEGAL_REQUEST;
		//sense information, like LBA where error occured
		//s->f.sense_buf[3] = 0x00; //MSB
		//s->f.sense_buf[4] = 0x00;
		//s->f.sense_buf[5] = 0x00;
		//s->f.sense_buf[6] = 0x00; //LSB
		s->f.sense_buf[7] = sense.asc ? 0x0a : 0x00; //Additional sense length (10 bytes if any)
		s->f.sense_buf[12] = sense.asc;              //Additional sense code
		s->f.sense_buf[13] = sense.ascq;             //Additional sense code qualifier
	}

	static void usb_msd_send_status(MSDState* s, USBPacket* p)
	{
		size_t len;

		assert(s->f.csw.sig == cpu_to_le32(0x53425355));
		len = MIN(sizeof(s->f.csw), p->iov.size);
		usb_packet_copy(p, &s->f.csw, len);
		memset(&s->f.csw, 0, sizeof(s->f.csw));
	}

	static void usb_msd_packet_complete(MSDState* s)
	{
		USBPacket* p = s->packet;

		/* Set s->packet to NULL before calling usb_packet_complete
       because another request may be issued before
       usb_packet_complete returns.  */
		s->packet = NULL;
		usb_packet_complete(&s->dev, p);
	}

	//static void usb_msd_transfer_data(SCSIRequest *req, uint32_t len)
	//{
	//MSDState *s = DO_UPCAST(MSDState, dev.qdev, req->bus->qbus.parent);
	//USBPacket *p = s->packet;

	//assert((s->mode == USB_MSDM_DATAOUT) == (req->cmd.mode == SCSI_XFER_TO_DEV));
	//s->scsi_len = len;
	//s->scsi_off = 0;
	//if (p) {
	//usb_msd_copy_data(s, p);
	//p = s->packet;
	//if (p && p->actual_length == p->iov.size) {
	//p->status = USB_RET_SUCCESS; /* Clear previous ASYNC status */
	//usb_msd_packet_complete(s);
	//}
	//}
	//}

	static void usb_msd_command_complete(MSDState* req, uint32_t status)
	{
		MSDState* s = req;
		USBPacket* p = s->packet;

		s->f.csw.sig = cpu_to_le32(0x53425355);
		s->f.csw.tag = cpu_to_le32(s->f.req.tag);
		s->f.csw.residue = cpu_to_le32(s->f.data_len);
		s->f.csw.status = status != 0;

		if (s->packet)
		{
			if (s->f.data_len == 0 && s->f.mode == USB_MSDM_DATAOUT)
			{
				/* A deferred packet with no write data remaining must be
               the status read packet.  */
				usb_msd_send_status(s, p);
				s->f.mode = USB_MSDM_CBW;
			}
			else if (s->f.mode == USB_MSDM_CSW)
			{
				usb_msd_send_status(s, p);
				s->f.mode = USB_MSDM_CBW;
			}
			else
			{
				if (s->f.data_len)
				{
					int len = (p->iov.size - p->actual_length);
					usb_packet_skip(p, len);
					s->f.data_len -= len;
				}
				if (s->f.data_len == 0)
				{
					s->f.mode = USB_MSDM_CSW;
				}
			}
			p->status = USB_RET_SUCCESS; /* Clear previous ASYNC status */
			usb_msd_packet_complete(s);
		}
		else if (s->f.data_len == 0)
		{
			s->f.mode = USB_MSDM_CSW;
		}
		s->f.req.valid = false;
	}

	static void usb_msd_copy_data(MSDState* s, USBPacket* p)
	{
		size_t len, file_ret;
		len = p->iov.size - p->actual_length;
		//if (len > s->scsi_len)
		//    len = s->scsi_len;

		if (len > sizeof(s->f.buf))
			len = sizeof(s->f.buf);

		//TODO No async reader/writer so do it right here
		if (s->f.tag == s->f.file_op_tag)
		{
			switch (s->f.mode)
			{
				case USB_MSDM_DATAOUT:
					usb_packet_copy(p, s->f.buf, len);
					if (len > 0 && (file_ret = fwrite(s->f.buf, 1, len, s->file)) < len)
					{
						s->f.result = COMMAND_FAILED; //PHASE_ERROR;
						set_sense(s, SENSE_CODE(WRITE_FAULT));
						goto fail;
					}
					break;
				case USB_MSDM_DATAIN:
					if ((file_ret = fread(s->f.buf, 1, p->iov.size, s->file)) < p->iov.size)
					{
						s->f.result = COMMAND_FAILED;
						set_sense(s, SENSE_CODE(UNRECOVERED_READ_ERROR));
						goto fail;
					}
					usb_packet_copy(p, s->f.buf, len);
					break;
				default: //TODO
				fail:
					p->actual_length = 0;
					p->status = USB_RET_STALL;
					return;
			}
		}
		else
			usb_packet_copy(p, s->f.buf + s->f.off, len);

		s->f.off += len;
		s->f.data_len -= len;

		//XXX Now continue async activity or...
		// Force complete, no async support right now
		usb_msd_command_complete(s, s->f.result);
	}

	static void send_command(void* opaque, struct usb_msd_cbw* cbw)
	{
		MSDState* s = (MSDState*)opaque;

		int64_t lba;
		uint32_t xfer_len;
		s->f.last_cmd = cbw->cmd[0];

		s->f.result = COMMAND_PASSED;
		s->f.off = 0;
		if (cbw->cmd[0] != REQUEST_SENSE)
			set_sense(s, SENSE_CODE(NO_SENSE));

		switch (cbw->cmd[0])
		{
			case TEST_UNIT_READY:
				//Do something?
				/* If error */
				//s->f.result = COMMAND_FAILED;
				//set_sense(s, SENSE_CODE(LUN_NOT_READY));
				break;
			case REQUEST_SENSE: //device shall keep old sense data
				memcpy(s->f.buf, s->f.sense_buf,
					   /* XXX the UFI device shall return only the number of bytes requested, as is */
					   cbw->cmd[4] < sizeof(s->f.sense_buf) ? (size_t)cbw->cmd[4] : sizeof(s->f.sense_buf));
				break;
			case INQUIRY:
				memset(s->f.buf, 0, sizeof(s->f.buf));
				s->f.buf[0] = 0;      //SCSI Peripheral Device Type: 0x0 - direct access device, 0x1f - unknown/no device
				s->f.buf[1] = 1 << 7; //removable
				s->f.buf[3] = 1;      //UFI response data format
				//inq data len can be zero
				strncpy((char*)&s->f.buf[8], "QEMU", 8);        //8 bytes vendor
				strncpy((char*)&s->f.buf[16], "USB Drive", 16); //16 bytes product
				strncpy((char*)&s->f.buf[32], "1.00", 4);       //4 bytes product revision
				break;

			case READ_CAPACITY_10:
				int64_t fsize, lbas;
				uint32_t *last_lba, *blk_len;

				memset(s->f.buf, 0, sizeof(s->f.buf));

				fsize = get_file_size(s->file);

				if (fsize == -1) //TODO
				{
					s->f.result = COMMAND_FAILED;
					set_sense(s, SENSE_CODE(UNKNOWN_ERROR));
					break;
				}

				last_lba = (uint32_t*)&s->f.buf[0];
				blk_len = (uint32_t*)&s->f.buf[4]; //in bytes
				//right?
				*blk_len = LBA_BLOCK_SIZE; //descriptor is currently max 64 bytes for bulk though

				lbas = fsize / LBA_BLOCK_SIZE;
				if (lbas > 0xFFFFFFFF)
				{
					s->f.result = COMMAND_FAILED;
					set_sense(s, SENSE_CODE(OUT_OF_RANGE));
					*last_lba = 0xFFFFFFFF;
				}
				else
					*last_lba = static_cast<uint32_t>(lbas);


				*last_lba = bswap32(*last_lba);
				*blk_len = bswap32(*blk_len);
				break;

			case READ_12:
			case READ_10:
				lba = bswap32(*(uint32_t*)&cbw->cmd[2]);
				if (cbw->cmd[0] == READ_10)
					xfer_len = bswap16(*(uint16_t*)&cbw->cmd[7]);
				else
					xfer_len = bswap32(*(uint32_t*)&cbw->cmd[6]);

				s->f.data_len = xfer_len * LBA_BLOCK_SIZE;
				s->f.file_op_tag = s->f.tag;


				if (xfer_len == 0) // nothing to do
					break;

				if (fseeko64(s->file, lba * LBA_BLOCK_SIZE, SEEK_SET))
				{
					s->f.result = COMMAND_FAILED;
					//TODO use errno
					int64_t fsize = get_file_size(s->file);
					if ((lba + xfer_len) * LBA_BLOCK_SIZE > fsize)
						set_sense(s, SENSE_CODE(OUT_OF_RANGE));
					else
						set_sense(s, SENSE_CODE(NO_SEEK_COMPLETE));
					return;
				}

				//memset(s->f.buf, 0, sizeof(s->f.buf));
				//Or do actual reading in USB_MSDM_DATAIN?
				//TODO probably dont set data_len to read length
				//if(!(s->f.data_len = fread(s->f.buf, 1, /*s->f.data_len*/ xfer_len * LBA_BLOCK_SIZE, s->file))) {
				//  s->f.result = PHASE_ERROR;
				//  set_sense(s, SENSE_CODE(UNRECOVERED_READ_ERROR));
				//}
				break;

			case WRITE_12:
			case WRITE_10:
				lba = bswap32(*(uint32_t*)&cbw->cmd[2]);
				if (cbw->cmd[0] == WRITE_10)
					xfer_len = bswap16(*(uint16_t*)&cbw->cmd[7]);
				else
					xfer_len = bswap32(*(uint32_t*)&cbw->cmd[6]);

				s->f.data_len = xfer_len * LBA_BLOCK_SIZE;
				s->f.file_op_tag = s->f.tag;

				if (xfer_len == 0) //nothing to do
					break;
				if (fseeko64(s->file, lba * LBA_BLOCK_SIZE, SEEK_SET))
				{
					s->f.result = COMMAND_FAILED;
					//TODO use errno
					int64_t fsize = get_file_size(s->file);
					if ((lba + xfer_len) * LBA_BLOCK_SIZE > fsize)
						set_sense(s, SENSE_CODE(OUT_OF_RANGE));
					else
						set_sense(s, SENSE_CODE(NO_SEEK_COMPLETE));
					return;
				}

				//Actual write comes with next command in USB_MSDM_DATAOUT
				break;
			default:
				s->f.result = COMMAND_FAILED;
				set_sense(s, SENSE_CODE(INVALID_OPCODE));
				s->f.mode = USB_MSDM_CSW; //TODO
				break;
		}
	}

	static void usb_msd_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
									   int index, int length, uint8_t* data)
	{
		MSDState* s = (MSDState*)dev;
		int ret = 0;

		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
				/* Class specific requests.  */
			case ClassInterfaceOutRequest | MassStorageReset:
				/* Reset state ready for the next CBW.  */
				s->f.mode = USB_MSDM_CBW;
				ret = 0;
				break;
			case ClassInterfaceRequest | GetMaxLun:
				data[0] = 0;
				p->actual_length = 1;
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_msd_cancel_io(USBDevice* dev, USBPacket* p)
	{
		MSDState* s = (MSDState*)(dev);

		assert(s->packet == p);
		s->packet = NULL;

		if (s->f.req.valid)
		{
			//scsi_req_cancel(s->req);
		}
	}

	static void usb_msd_handle_data(USBDevice* dev, USBPacket* p)
	{
		MSDState* s = (MSDState*)dev;
		struct usb_msd_cbw cbw;
		uint8_t devep = p->ep->nr;

		//XXX Note for self if using async td: see qemu dev-storage.c
		// 1.) USB_MSDM_CBW: set requested mode USB_MSDM_DATAOUT/IN and enqueue command,
		// 2.) USB_MSDM_DATAOUT: return USB_RET_ASYNC status if command is in progress,
		// 3.) USB_MSDM_CSW: return USB_RET_ASYNC status if command is still in progress
		//     or complete and set mode to USB_MSDM_CBW.

		switch (p->pid)
		{
			case USB_TOKEN_OUT:
				if (devep != 2)
					goto fail;

				switch (s->f.mode)
				{
					case USB_MSDM_CBW:
						if (p->iov.size != 31)
						{
							Console.Warning("usb-msd: Bad CBW size\n");
							goto fail;
						}
						usb_packet_copy(p, &cbw, 31);
						if (le32_to_cpu(cbw.sig) != 0x43425355)
						{
							Console.Warning("usb-msd: Bad signature %08x\n",
									le32_to_cpu(cbw.sig));
							goto fail;
						}
						if (cbw.lun != 0)
						{
							Console.Warning("usb-msd: Bad LUN %d\n", cbw.lun);
							goto fail;
						}
						s->f.tag = le32_to_cpu(cbw.tag);
						s->f.data_len = le32_to_cpu(cbw.data_len);
						if (s->f.data_len == 0)
						{
							s->f.mode = USB_MSDM_CSW;
						}
						else if (cbw.flags & 0x80)
						{
							s->f.mode = USB_MSDM_DATAIN;
						}
						else
						{
							s->f.mode = USB_MSDM_DATAOUT;
						}

						//async fread/fwrite handle or something
						s->f.req.valid = true;
						s->f.req.tag = le32_to_cpu(cbw.tag);
						send_command(s, &cbw);
						break;

					case USB_MSDM_DATAOUT:
						//TODO check if CBW still falls into here on write error a.k.a s->f.mode is set wrong
						if (p->iov.size > s->f.data_len)
						{
							goto fail;
						}

						if (p->iov.size == 0) //TODO send status?
							goto send_csw;

						//if (s->scsi_len)
						{
							usb_msd_copy_data(s, p);
						}
						if (le32_to_cpu(s->f.csw.residue))
						{
							int len = p->iov.size - p->actual_length;
							if (len)
							{
								usb_packet_skip(p, len);
								s->f.data_len -= len;
								if (s->f.data_len == 0)
								{
									s->f.mode = USB_MSDM_CSW;
								}
							}
						}
						if ((size_t)p->actual_length < p->iov.size)
						{
							s->packet = p;
							p->status = USB_RET_ASYNC;
						}

						//if (s->f.data_len == 0)
						//    s->f.mode = USB_MSDM_CSW;
						break;

					default:
						goto fail;
				}
				break;

			case USB_TOKEN_IN:
				if (devep != 1)
					goto fail;

				switch (s->f.mode)
				{
					case USB_MSDM_DATAOUT:
						if (s->f.data_len != 0 || p->iov.size < 13)
						{
							goto fail;
						}
						/* Waiting for SCSI write to complete.  */
						s->packet = p;
						p->status = USB_RET_ASYNC;
						break;

					case USB_MSDM_CSW:
					send_csw:
						if (p->iov.size < 13)
						{
							goto fail;
						}

						if (false && s->f.req.valid)
						{ // If reading/writing using something asynchronous
							/* still in flight */
							s->packet = p;
							p->status = USB_RET_ASYNC;
						}
						else
						{
							//TODO primarily for setting csw.sig with correct value
							usb_msd_command_complete(s, s->f.result);

							usb_msd_send_status(s, p);
							s->f.mode = USB_MSDM_CBW;
						}
						break;

					case USB_MSDM_DATAIN:
						//if (len == 13) goto send_csw;
						if (p->iov.size > s->f.data_len)
						{
							//len = s->f.data_len;
							s->f.result = COMMAND_FAILED;
							set_sense(s, SENSE_CODE(UNRECOVERED_READ_ERROR));
							goto fail;
						}

						//if (s->scsi_len)
						{
							usb_msd_copy_data(s, p);
						}
						if (le32_to_cpu(s->f.csw.residue))
						{
							int len = p->iov.size - p->actual_length;
							if (len)
							{
								usb_packet_skip(p, len);
								s->f.data_len -= len;
								if (s->f.data_len == 0)
								{
									s->f.mode = USB_MSDM_CSW;
								}
							}
						}

						if ((size_t)p->actual_length < p->iov.size)
						{
							s->packet = p;
							p->status = USB_RET_ASYNC;
						}
						break;

					default:
						goto fail;
				}
				break;

			default:
			fail:
				p->status = USB_RET_STALL;
				//s->f.mode = USB_MSDM_CSW;
				break;
		}
	}

	static void usb_msd_handle_destroy(USBDevice* dev)
	{
		MSDState* s = (MSDState*)dev;
		if (s && s->file)
		{
			fclose(s->file);
			s->file = NULL;
		}
		delete s;
	}

	USBDevice* MsdDevice::CreateDevice(int port)
	{
		MSDState* s = new MSDState();

		//CONFIGVARIANT varApi(N_DEVICE_API, CONFIG_TYPE_CHAR);
		//LoadSetting(port, DEVICENAME, varApi);
		std::string api = *MsdDevice::ListAPIs().begin();

		TSTDSTRING var;

		if (!LoadSetting(TypeName(), port, api, N_CONFIG_PATH, var))
		{
			Console.Warning("usb-msd: Could not load settings\n");
			delete s;
			return NULL;
		}

		s->file = wfopen(var.c_str(), TEXT("r+b"));
		if (!s->file)
		{
			Console.WriteLn("usb-msd: Could not open image file '%s'\n", var.c_str());
			goto fail;
		}

		s->f.hash = 0;
		s->f.last_cmd = -1;
		s->dev.speed = USB_SPEED_FULL;

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;
		if (usb_desc_parse_dev(qemu_msd_dev_descriptor, sizeof(qemu_msd_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(qemu_msd_config_descriptor, sizeof(qemu_msd_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.klass.cancel_packet = usb_msd_cancel_io;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_msd_handle_reset;
		s->dev.klass.handle_control = usb_msd_handle_control;
		s->dev.klass.handle_data = usb_msd_handle_data;
		s->dev.klass.unrealize = usb_msd_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = desc_strings[STR_PRODUCT];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		usb_msd_handle_reset((USBDevice*)s);
		return (USBDevice*)s;

	fail:
		usb_msd_handle_destroy((USBDevice*)s);
		return NULL;
	}

	const char* MsdDevice::TypeName()
	{
		return "msd";
	}

	int MsdDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		MSDState* s = (MSDState*)dev;
		MSDState::freeze* tmp;

		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				//if (s->f.req) free (s->f.req);

				tmp = (MSDState::freeze*)data;
				s->f = *tmp;
				//ReqState *req = (ReqState *)((char*)data + sizeof(MSDState::freeze));
				//s->f.req = qemu_mallocz (sizeof(ReqState));
				//*s->f.req = *req;

				//TODO resetting port to try to avoid possible data corruption
				//if (s->f.mode == USB_MSDM_DATAOUT)
				usb_reattach(dev->port);
				return sizeof(MSDState::freeze); // + sizeof(ReqState);

			case FreezeAction::Save:
				tmp = (MSDState::freeze*)data;
				*tmp = s->f;
				return sizeof(MSDState::freeze);

			case FreezeAction::Size:
				return sizeof(MSDState::freeze); // + sizeof(ReqState);
			default:
				break;
		}
		return 0;
	}

} // namespace usb_msd
