/*
 * QEMU USB HID devices
 *
 * Copyright (c) 2005 Fabrice Bellard
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

// Most stuff is based on Qemu 1.7 USB soundcard passthrough code.

#include "PrecompiledHeader.h"
#include "USB/qemu-usb/vl.h"
#include "USB/qemu-usb/desc.h"
#include "usb-mic-singstar.h"
#include "USB/shared/inifile_usb.h"
#include <assert.h>

static FILE* file = NULL;

#include "audio.h"

#define BUFFER_FRAMES 200

/*
 * A Basic Audio Device uses these specific values
 */
#define USBAUDIO_PACKET_SIZE 200 //192
#define USBAUDIO_SAMPLE_RATE 48000
#define USBAUDIO_PACKET_INTERVAL 1

namespace usb_mic
{

	/*
 * A USB audio device supports an arbitrary number of alternate
 * interface settings for each interface.  Each corresponds to a block
 * diagram of parameterized blocks.  This can thus refer to things like
 * number of channels, data rates, or in fact completely different
 * block diagrams.  Alternative setting 0 is always the null block diagram,
 * which is used by a disabled device.
 */
	enum usb_audio_altset : int8_t
	{
		ALTSET_OFF = 0x00, /* No endpoint */
		ALTSET_ON = 0x01,  /* Single endpoint */
	};

	typedef struct SINGSTARMICState
	{
		USBDevice dev;

		USBDesc desc;
		USBDescDevice desc_dev;

		AudioDevice* audsrc[2];
		AudioDeviceProxyBase* audsrcproxy;

		struct freeze
		{
			int port;
			int intf;
			MicMode mode;

			enum usb_audio_altset altset;
			bool mute;
			uint8_t vol[2];
			uint32_t srate[2]; //TODO can it have different rates?
		} f;                   //freezable

		/* properties */
		uint32_t debug;
		std::vector<int16_t> buffer[2];
		//uint8_t  fifo[2][200]; //on-chip 400byte fifo
	} SINGSTARMICState;

	static const USBDescStrings desc_strings = {
		"",
		"Nam Tai E&E Products Ltd.",
		"USBMIC",
		"310420811",
	};

	/* descriptor dumped from a real singstar MIC adapter */
	static const uint8_t singstar_mic_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0110), //(272)
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x1415), //(5141)
		/* idProduct           */ WBVAL(0x0000), //(0)
		/* bcdDevice           */ WBVAL(0x0001), //(1)
		/* iManufacturer       */ 0x01,          //(1)
		/* iProduct            */ 0x02,          //(2)
		/* iSerialNumber       */ 0x00,          //(0) unused
		/* bNumConfigurations  */ 0x01,          //(1)

	};

	static const uint8_t singstar_mic_config_descriptor[] = {

		/* Configuration 1 */
		0x09,                              /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(0x00b1),                     /* wTotalLength */
		0x02,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		USB_CONFIG_BUS_POWERED,            /* bmAttributes */
		USB_CONFIG_POWER_MA(90),           /* bMaxPower */

		/* Interface 0, Alternate Setting 0, Audio Control */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x00,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOCONTROL,   /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Control Interface */
		AUDIO_CONTROL_INTERFACE_DESC_SZ(1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,    /* bDescriptorType */
		AUDIO_CONTROL_HEADER,               /* bDescriptorSubtype */
		WBVAL(0x0100), /* 1.00 */           /* bcdADC */
		WBVAL(0x0028),                      /* wTotalLength */
		0x01,                               /* bInCollection */
		0x01,                               /* baInterfaceNr */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE,           /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,          /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL,             /* bDescriptorSubtype */
		0x01,                                     /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE),         /* wTerminalType */
		0x02,                                     /* bAssocTerminal */
		0x02,                                     /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L | AUDIO_CHANNEL_R), /* wChannelConfig */
		0x00,                                     /* iChannelNames */
		0x00,                                     /* iTerminal */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE,     /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL,       /* bDescriptorSubtype */
		0x02,                                /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING), /* wTerminalType */
		0x01,                                /* bAssocTerminal */
		0x03,                                /* bSourceID */
		0x00,                                /* iTerminal */

		/* Audio Feature Unit */
		AUDIO_FEATURE_UNIT_DESC_SZ(2, 1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT,       /* bDescriptorSubtype */
		0x03,                             /* bUnitID */
		0x01,                             /* bSourceID */
		0x01,                             /* bControlSize */
		0x01,                             /* bmaControls(0) */
		0x02,                             /* bmaControls(1) */
		0x02,                             /* bmaControls(2) */
		0x00,                             /* iTerminal */

		/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Interface 1, Alternate Setting 1, Audio Streaming - Operational */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x01,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x02,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE,     /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I,             /* bFormatType */
		0x01,                            /* bNrChannels */
		0x02,                            /* bSubFrameSize */
		0x10,                            /* bBitResolution */
		0x05,                            /* bSamFreqType */
		B3VAL(8000),                     /* tSamFreq 1 */
		B3VAL(11025),                    /* tSamFreq 2 */
		B3VAL(22050),                    /* tSamFreq 3 */
		B3VAL(44100),                    /* tSamFreq 4 */
		B3VAL(48000),                    /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                              /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                                   /* bDescriptorType */
		USB_ENDPOINT_IN(1),                                             /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x0064),                                                  /* wMaxPacketSize */
		0x01,                                                           /* bInterval */
		0x00,                                                           /* bRefresh */
		0x00,                                                           /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Interface 1, Alternate Setting 2, Audio Streaming - ? */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x02,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x02,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE,     /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I,             /* bFormatType */
		0x02,                            /* bNrChannels */
		0x02,                            /* bSubFrameSize */
		0x10,                            /* bBitResolution */
		0x05,                            /* bSamFreqType */
		B3VAL(8000),                     /* tSamFreq 1 */
		B3VAL(11025),                    /* tSamFreq 2 */
		B3VAL(22050),                    /* tSamFreq 3 */
		B3VAL(44100),                    /* tSamFreq 4 */
		B3VAL(48000),                    /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                              /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                                   /* bDescriptorType */
		USB_ENDPOINT_IN(1),                                             /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x00c8),                                                  /* wMaxPacketSize */
		0x01,                                                           /* bInterval */
		0x00,                                                           /* bRefresh */
		0x00,                                                           /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Terminator */
		0 /* bLength */
	};


	static void singstar_mic_handle_reset(USBDevice* dev)
	{
		/* XXX: do it */
		return;
	}

/*
 * Note: we arbitrarily map the volume control range onto -inf..+8 dB
 */
#define ATTRIB_ID(cs, attrib, idif) \
	(((cs) << 24) | ((attrib) << 16) | (idif))


	//0x0300 - feature bUnitID 0x03
	static int usb_audio_get_control(SINGSTARMICState* s, uint8_t attrib,
									 uint16_t cscn, uint16_t idif,
									 int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		int ret = USB_RET_STALL;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0300):
				data[0] = s->f.mute;
				ret = 1;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0300):
				if (cn < 2)
				{
					//uint16_t vol = (s->f.vol[cn] * 0x8800 + 127) / 255 + 0x8000;
					uint16_t vol = (s->f.vol[cn] * 0x8800 + 127) / 255 + 0x8000;
					data[0] = (uint8_t)(vol & 0xFF);
					data[1] = vol >> 8;
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MIN, 0x0300):
				if (cn < 2)
				{
					data[0] = 0x01;
					data[1] = 0x80;
					//data[0] = 0x00;
					//data[1] = 0xE1; //0xE100 -31dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MAX, 0x0300):
				if (cn < 2)
				{
					data[0] = 0x00;
					data[1] = 0x08;
					//data[0] = 0x00;
					//data[1] = 0x18; //0x1800 +24dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_RES, 0x0300):
				if (cn < 2)
				{
					data[0] = 0x88;
					data[1] = 0x00;
					//data[0] = 0x00;
					//data[1] = 0x01; //0x0100 1.0 dB
					ret = 2;
				}
				break;
		}

		return ret;
	}

	static int usb_audio_set_control(SINGSTARMICState* s, uint8_t attrib,
									 uint16_t cscn, uint16_t idif,
									 int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		int ret = USB_RET_STALL;
		bool set_vol = false;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0300):
				s->f.mute = data[0] & 1;
				set_vol = true;
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0300):
				if (cn < 2)
				{
					uint16_t vol = data[0] + (data[1] << 8);

					//qemu usb audiocard formula, singstar has a bit different range
					vol -= 0x8000;
					vol = (vol * 255 + 0x4400) / 0x8800;
					if (vol > 255)
					{
						vol = 255;
					}

					if (s->f.vol[cn] != vol)
					{
						s->f.vol[cn] = (uint8_t)vol;
						set_vol = true;
					}
					ret = 0;
				}
				break;
		}

		if (set_vol)
		{
			//if (s->debug) {
			//}
		}

		return ret;
	}

	static int usb_audio_ep_control(SINGSTARMICState* s, uint8_t attrib,
									uint16_t cscn, uint16_t ep,
									int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, ep);
		int ret = USB_RET_STALL;

		//cs 1 cn 0xFF, ep 0x81 attrib 1
		Console.Warning("singstar: ep control cs %x, cn %X, %X %X data:", cs, cn, attrib, ep);
		/*for(int i=0; i<length; i++)
		Console.Warning("%02X ", data[i]);
	Console.Warning("\n");*/

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_SET_CUR, 0x81):
				if (cn == 0xFF)
				{
					s->f.srate[0] = data[0] | (data[1] << 8) | (data[2] << 16);
					s->f.srate[1] = s->f.srate[0];

					if (s->audsrc[0])
						s->audsrc[0]->SetResampling(s->f.srate[0]);

					if (s->audsrc[1])
						s->audsrc[1]->SetResampling(s->f.srate[1]);

				}
				else if (cn < 2)
				{

					s->f.srate[cn] = data[0] | (data[1] << 8) | (data[2] << 16);
					if (s->audsrc[cn])
						s->audsrc[cn]->SetResampling(s->f.srate[cn]);
				}
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_GET_CUR, 0x81):
				data[0] = s->f.srate[0] & 0xFF;
				data[1] = (s->f.srate[0] >> 8) & 0xFF;
				data[2] = (s->f.srate[0] >> 16) & 0xFF;
				ret = 3;
				break;
		}

		return ret;
	}

	static void singstar_mic_set_interface(USBDevice* dev, int intf,
										   int alt_old, int alt_new)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		s->f.intf = alt_new;
#if defined(_DEBUG)
		/* close previous debug audio output file */
		if (file && intf > 0 && alt_old != alt_new)
		{
			fclose(file);
			file = nullptr;
		}
#endif
	}

	static void singstar_mic_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
											int index, int length, uint8_t* data)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		int ret = 0;


		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			/*
    * Audio device specific request
    */
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_CUR:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_MIN:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_MAX:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_RES:
				ret = usb_audio_get_control(s, request & 0xff, value, index,
											length, data);
				if (ret < 0)
				{
					//if (s->debug) {
					Console.Warning("singstar: fail: get control\n");
					//}
					goto fail;
				}
				p->actual_length = ret;
				break;

			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_CUR:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MIN:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MAX:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_RES:
				ret = usb_audio_set_control(s, request & 0xff, value, index,
											length, data);
				if (ret < 0)
				{
					//if (s->debug) {
					Console.Warning("singstar: fail: set control\n data:");
					//}
					goto fail;
				}
				break;

			case ClassEndpointRequest | AUDIO_REQUEST_GET_CUR:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_MIN:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_MAX:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_RES:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_CUR:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_MIN:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_MAX:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_RES:
				ret = usb_audio_ep_control(s, request & 0xff, value, index,
										   length, data);
				if (ret < 0)
					goto fail;
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	//naive, needs interpolation and stuff
	inline static int16_t SetVolume(int16_t sample, int vol)
	{
		//return (int16_t)(((uint16_t)(0x7FFF + sample) * vol / 0xFF) - 0x7FFF );
		return (int16_t)((int32_t)sample * vol / 0xFF);
	}

	static void singstar_mic_handle_data(USBDevice* dev, USBPacket* p)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		int ret = 0;
		uint8_t devep = p->ep->nr;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				//Console.Warning("token in ep: %d len: %zd\n", devep, p->iov.size);
				if (devep == 1)
				{

					//TODO
					int outChns = s->f.intf == 1 ? 1 : 2;
					uint32_t frames, out_frames[2] = {0}, chn;
					int16_t *src1, *src2;
					int16_t* dst = nullptr;
					std::vector<int16_t> dst_alloc(0); //TODO
					size_t len = p->iov.size;

					// send only 1ms (bInterval) of samples
					if (s->f.srate[0] == 48000 || s->f.srate[0] == 8000)
						len = std::min(p->iov.size, outChns * sizeof(int16_t) * s->f.srate[0] / 1000);

					//Divide 'len' bytes between 2 channels of 16 bits
					uint32_t max_frames = len / (outChns * sizeof(uint16_t));

					if (p->iov.niov == 1)
						dst = (int16_t*)p->iov.iov[0].iov_base;
					else
					{
						dst_alloc.resize(len / sizeof(int16_t));
						dst = dst_alloc.data();
					}

					memset(dst, 0, len);

					for (int i = 0; i < 2; i++)
					{
						frames = max_frames;
						if (s->audsrc[i] &&
							s->audsrc[i]->GetFrames(&frames))
						{
							frames = MIN(max_frames, frames); //max 50 frames usually
							out_frames[i] = s->audsrc[i]->GetBuffer(s->buffer[i].data(), frames);
						}
					}

					//if (frames < max_frames)
					if (!frames)
					{
						p->status = USB_RET_NAK;
						return;
					}


					//TODO well, it is 16bit interleaved, right?
					//Merge with MIC_MODE_SHARED case?
					switch (s->f.mode)
					{
						case MIC_MODE_SINGLE:
						{
							int k = s->audsrc[0] ? 0 : 1;
							int off = s->f.intf == 1 ? 0 : k;
							chn = s->audsrc[k]->GetChannels();
							frames = out_frames[k];

							uint32_t i = 0;
							for (; i < frames && i < max_frames; i++)
							{
								dst[i * outChns + off] = SetVolume(s->buffer[k][i * chn], s->f.vol[0]);
								//dst[i * 2 + 1] = 0;
							}

							ret = i;
						}
						break;
						case MIC_MODE_SHARED:
						{
							chn = s->audsrc[0]->GetChannels();
							frames = out_frames[0];
							src1 = s->buffer[0].data();

							uint32_t i = 0;
							for (; i < frames && i < max_frames; i++)
							{
								dst[i * outChns] = SetVolume(src1[i * chn], s->f.vol[0]);
								if (outChns > 1)
								{
									if (chn == 1)
										dst[i * 2 + 1] = dst[i * 2];
									else
										dst[i * 2 + 1] = SetVolume(src1[i * chn + 1], s->f.vol[0]);
								}
							}

							ret = i;
						}
						break;
						case MIC_MODE_SEPARATE:
						{
							uint32_t cn1 = s->audsrc[0]->GetChannels();
							uint32_t cn2 = s->audsrc[1]->GetChannels();
							uint32_t minLen = MIN(out_frames[0], out_frames[1]);

							src1 = s->buffer[0].data();
							src2 = s->buffer[1].data();

							uint32_t i = 0;
							for (; i < minLen && i < max_frames; i++)
							{
								dst[i * outChns] = SetVolume(src1[i * cn1], s->f.vol[0]);
								if (outChns > 1)
									dst[i * 2 + 1] = SetVolume(src2[i * cn2], s->f.vol[1]);
							}

							ret = i;
						}
						break;
						default:
							break;
					}

					ret = ret * outChns * sizeof(int16_t);
					if (p->iov.niov > 1)
					{
						usb_packet_copy(p, dst_alloc.data(), ret);
					}
					else
						p->actual_length = ret;

#if 0 //defined(_DEBUG) && _MSC_VER > 1800
					if (!file)
					{
						char name[1024] = {0};
						snprintf(name, sizeof(name), "singstar_%dch_%uHz.raw", outChns, s->f.srate[0]);
						file = fopen(name, "wb");
					}

					if (file)
						fwrite(dst, 1, ret, file);
#endif
				}
				break;
			case USB_TOKEN_OUT:
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}


	static void singstar_mic_handle_destroy(USBDevice* dev)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		if (file)
			fclose(file);
		file = NULL;

		if (!s)
			return;
		for (int i = 0; i < 2; i++)
		{
			if (s->audsrc[i])
			{
				s->audsrc[i]->Stop();
				delete s->audsrc[i];
				s->audsrc[i] = NULL;
				s->buffer[i].clear();
			}
		}
		s->audsrcproxy->AudioDeinit();
		delete s;
	}

	static int singstar_mic_handle_open(USBDevice* dev)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		if (s)
		{
			for (int i = 0; i < 2; i++)
				if (s->audsrc[i])
					s->audsrc[i]->Start();
		}
		return 0;
	}

	static void singstar_mic_handle_close(USBDevice* dev)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		if (s)
		{
			for (int i = 0; i < 2; i++)
				if (s->audsrc[i])
					s->audsrc[i]->Stop();
		}
	}

	//USBDevice *singstar_mic_init(int port, TSTDSTRING *devs)
	USBDevice* SingstarDevice::CreateDevice(int port)
	{
		std::string api;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, SingstarDevice::TypeName(), N_DEVICE_API, tmp);
		api = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, SingstarDevice::TypeName(), N_DEVICE_API, api);
#endif
		return SingstarDevice::CreateDevice(port, api);
	}
	USBDevice* SingstarDevice::CreateDevice(int port, const std::string& api)
	{
		SINGSTARMICState* s;
		AudioDeviceInfo info;

		s = new SINGSTARMICState();

		s->audsrcproxy = RegisterAudioDevice::instance().Proxy(api);
		if (!s->audsrcproxy)
		{
			Console.WriteLn("singstar: Invalid audio API: '%s'\n", api.c_str());
			delete s;
			return NULL;
		}

		s->audsrcproxy->AudioInit();

		s->audsrc[0] = s->audsrcproxy->CreateObject(port, TypeName(), 0, AUDIODIR_SOURCE);
		s->audsrc[1] = s->audsrcproxy->CreateObject(port, TypeName(), 1, AUDIODIR_SOURCE);

		if (!s->audsrc[0] && !s->audsrc[1])
			goto fail;

		if (s->audsrc[0] && s->audsrc[1] && s->audsrc[0]->Compare(s->audsrc[1]))
		{
			s->f.mode = MIC_MODE_SHARED;
			// And don't capture the same source twice
			s->audsrc[1]->Stop();
			delete s->audsrc[1];
			s->audsrc[1] = nullptr;
		}
		else if (!s->audsrc[0] || !s->audsrc[1])
			s->f.mode = MIC_MODE_SINGLE;
		else
			s->f.mode = MIC_MODE_SEPARATE;

		for (int i = 0; i < 2; i++)
			if (s->audsrc[i])
				s->buffer[i].resize(BUFFER_FRAMES * s->audsrc[i]->GetChannels());

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;
		if (usb_desc_parse_dev(singstar_mic_dev_descriptor, sizeof(singstar_mic_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(singstar_mic_config_descriptor, sizeof(singstar_mic_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = singstar_mic_handle_reset;
		s->dev.klass.handle_control = singstar_mic_handle_control;
		s->dev.klass.handle_data = singstar_mic_handle_data;
		s->dev.klass.set_interface = singstar_mic_set_interface;
		s->dev.klass.unrealize = singstar_mic_handle_destroy;
		s->dev.klass.open = singstar_mic_handle_open;
		s->dev.klass.close = singstar_mic_handle_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = desc_strings[2];

		// set defaults
		s->f.vol[0] = 240; /* 0 dB */
		s->f.vol[1] = 240; /* 0 dB */
		s->f.srate[0] = 48000;
		s->f.srate[1] = 48000;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		singstar_mic_handle_reset((USBDevice*)s);

		return (USBDevice*)s;

	fail:
		singstar_mic_handle_destroy((USBDevice*)s);
		return NULL;
	}

	int SingstarDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterAudioDevice::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int SingstarDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		SINGSTARMICState* s = (SINGSTARMICState*)dev;
		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				s->f = *(SINGSTARMICState::freeze*)data;
				if (s->audsrc[0])
					s->audsrc[0]->SetResampling(s->f.srate[0]);
				if (s->audsrc[1])
					s->audsrc[1]->SetResampling(s->f.srate[1]);
				return sizeof(SINGSTARMICState::freeze);
			case FreezeAction::Save:
				*(SINGSTARMICState::freeze*)data = s->f;
				return sizeof(SINGSTARMICState::freeze);
			case FreezeAction::Size:
				return sizeof(SINGSTARMICState::freeze);
			default:
				break;
		}
		return 0;
	}

} // namespace usb_mic
