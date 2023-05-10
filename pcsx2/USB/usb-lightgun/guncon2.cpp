/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-lightgun/guncon2.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/USB.h"
#include "GS/GS.h"
#include "StateWrapper.h"
#include "VMManager.h"

#include "Frontend/InputManager.h"

#include <tuple>

#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include "GS/Renderers/Common/GSDevice.h"

namespace usb_lightgun
{
	enum : u32
	{
		GUNCON2_FLAG_PROGRESSIVE = 0x0100,

		GUNCON2_CALIBRATION_DELAY = 9
	};

	enum : u32
	{
		BID_C = 1,
		BID_B = 2,
		BID_A = 3,
		BID_DPAD_UP = 4,
		BID_DPAD_RIGHT = 5,
		BID_DPAD_DOWN = 6,
		BID_DPAD_LEFT = 7,
		BID_TRIGGER = 13,
		BID_SELECT = 14,
		BID_START = 15,
		BID_SHOOT_OFFSCREEN = 16,
		BID_RECALIBRATE = 17,
	};

	// Right pain in the arse. Different games seem to have different scales..
	// Not worth putting these in the gamedb for such few games.
	// Values are from the old nuvee plugin.
	struct GameConfig
	{
		const char* serial;
		float scale_x, scale_y;
		u32 center_x, center_y;
		u32 screen_width, screen_height;
	};

	static constexpr const GameConfig s_game_config[] = {
		{"SLES-50930", 90.25f, 94.5f, 390, 132, 640, 256}, // Dino Stalker (E, English)
		{"SLES-51095", 90.25f, 94.5f, 390, 132, 640, 256}, // Dino Stalker (E, French)
		{"SLES-51096", 90.25f, 94.5f, 390, 132, 640, 256}, // Dino Stalker (E, German)
		{"SLUS-20485", 90.25f, 92.5f, 390, 132, 640, 240}, // Dino Stalker (U)
		{"SLUS-20389", 89.25f, 93.5f, 422, 141, 640, 240}, // Endgame (U)
		{"SLES-50936", 112.0f, 100.0f, 320, 120, 512, 256}, // Endgame (E) (Guncon2 needs to be connected to USB port 2)
		{"SLPM-65139", 90.0f, 91.5f, 320, 120, 640, 240}, // Gun Survivor 3: Dino Crisis (J)
		{"SLES-52620", 89.5f, 112.3f, 390, 147, 640, 256}, // Guncom 2 (E)
		{"SLES-51289", 84.5f, 89.0f, 456, 164, 640, 256}, // Gunfighter 2 - Jesse James (E)
		{"SLPS-25165", 90.25f, 98.0f, 390, 138, 640, 240}, // Gunvari Collection (J) (480i)
		// {"SLPS-25165", 86.75f, 96.0f, 454, 164, 640, 256}, // Gunvari Collection (J) (480p)
		{"SCES-50889", 90.25f, 94.5f, 390, 169, 640, 256}, // Ninja Assault (E)
		{"SLPS-20218", 90.0f, 92.0f, 320, 134, 640, 240}, // Ninja Assault (J)
		{"SLUS-20492", 90.25f, 92.5f, 390, 132, 640, 240}, // Ninja Assault (U)
		{"SLES-50650", 84.75f, 96.0f, 454, 164, 640, 240}, // Resident Evil Survivor 2 (E)
		{"SLES-51448", 90.25f, 95.0f, 420, 132, 640, 240}, // Resident Evil - Dead Aim (E)
		{"SLUS-20669", 90.25f, 93.5f, 420, 132, 640, 240}, // Resident Evil - Dead Aim (U)
		{"SLUS-20619", 90.25f, 91.75f, 453, 154, 640, 256}, // Starsky & Hutch (U)
		{"SCES-50300", 90.25f, 102.75f, 390, 138, 640, 256}, // Time Crisis II (E)
		{"SLUS-20219", 90.25f, 97.5f, 390, 154, 640, 240}, // Time Crisis 2 (U)
		{"SCES-51844", 90.25f, 102.75f, 390, 138, 640, 256}, // Time Crisis 3 (E)
		{"SLUS-20645", 90.25f, 97.5f, 390, 154, 640, 240}, // Time Crisis 3 (U)
		{"SCES-52530", 90.25f, 99.0f, 390, 153, 640, 256}, // Crisis Zone (E)
		{"SLUS-20927", 90.25f, 99.0f, 390, 153, 640, 240}, // Time Crisis - Crisis Zone (U) (480i)
		// {"SLUS-20927", 94.5f, 104.75f, 423, 407, 768, 768}, // Time Crisis - Crisis Zone (U) (480p)
		{"SCES-50411", 89.8f, 99.9f, 421, 138, 640, 256}, // Vampire Night (E)
		{"SLPS-25077", 90.0f, 97.5f, 422, 118, 640, 240}, // Vampire Night (J)
		{"SLUS-20221", 89.8f, 102.5f, 422, 124, 640, 228}, // Vampire Night (U)
		{"SLES-51229", 110.15f, 100.0f, 433, 159, 512, 256}, // Virtua Cop - Elite Edition (E,J) (480i)
		// {"SLES-51229", 85.75f, 92.0f, 456, 164, 640, 256}, // Virtua Cop - Elite Edition (E,J) (480p)
	};

	static constexpr s32 DEFAULT_SCREEN_WIDTH = 640;
	static constexpr s32 DEFAULT_SCREEN_HEIGHT = 240;
	static constexpr float DEFAULT_CENTER_X = 320.0f;
	static constexpr float DEFAULT_CENTER_Y = 120.0f;
	static constexpr float DEFAULT_SCALE_X = 100.0f;
	static constexpr float DEFAULT_SCALE_Y = 100.0f;

#pragma pack(push, 1)
	union GunCon2Out
	{
		u8 bits[6];

		struct
		{
			u16 buttons;
			s16 pos_x;
			s16 pos_y;
		};
	};
	static_assert(sizeof(GunCon2Out) == 6);
#pragma pack(pop)

	struct GunCon2State
	{
		explicit GunCon2State(u32 port_);
	        ~GunCon2State();

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;

		//////////////////////////////////////////////////////////////////////////
		// Configuration
		//////////////////////////////////////////////////////////////////////////
		bool custom_config = false;
		u32 screen_width = 640;
		u32 screen_height = 240;
		float center_x = 320;
		float center_y = 120;
		float scale_x = 1.0f;
		float scale_y = 1.0f;

		//////////////////////////////////////////////////////////////////////////
		// Host State (Not Saved)
		//////////////////////////////////////////////////////////////////////////
		u32 button_state = 0;

		//////////////////////////////////////////////////////////////////////////
		// Device State (Saved)
		//////////////////////////////////////////////////////////////////////////
		s16 param_x = 0;
		s16 param_y = 0;
		u16 param_mode = 0;

		u16 calibration_timer = 0;
		s16 calibration_pos_x = 0;
		s16 calibration_pos_y = 0;

		bool auto_config_done = false;

		void AutoConfigure();

		std::tuple<s16, s16> CalculatePosition();

		// udev
		int   udev_fd;
		float udev_internalGunX;
		float udev_internalGunY;
		int   udev_gunMinx, udev_gunMiny, udev_gunMaxx, udev_gunMaxy;
	};

	static const USBDescStrings desc_strings = {
		"Namco GunCon2",
	};

	/* mostly the same values as the Bochs USB Keyboard device */
	static const uint8_t guncon2_dev_desc[] = {
		/* bLength             */ 0x12,
		/* bDescriptorType     */ 0x01,
		/* bcdUSB              */ WBVAL(0x0100),
		/* bDeviceClass        */ 0x00,
		/* bDeviceSubClass     */ 0x00,
		/* bDeviceProtocol     */ 0x00,
		/* bMaxPacketSize0     */ 0x08,
		/* idVendor            */ WBVAL(0x0b9a),
		/* idProduct           */ WBVAL(0x016a),
		/* bcdDevice           */ WBVAL(0x0100),
		/* iManufacturer       */ 0x00,
		/* iProduct            */ 0x00,
		/* iSerialNumber       */ 0x00,
		/* bNumConfigurations  */ 0x01,
	};

	static const uint8_t guncon2_config_desc[] = {
		0x09, // Length
		0x02, // Type (Config)
		0x19, 0x00, // Total size

		0x01, // # interfaces
		0x01, // Configuration #
		0x00, // index of string descriptor
		0x80, // Attributes (bus powered)
		0x19, // Max power in mA


		// Interface
		0x09, // Length
		0x04, // Type (Interface)

		0x00, // Interface #
		0x00, // Alternative #
		0x01, // # endpoints

		0xff, // Class
		0x6a, // Subclass
		0x00, // Protocol
		0x00, // index of string descriptor


		// Endpoint
		0x07, // Length
		0x05, // Type (Endpoint)

		0x81, // Address
		0x03, // Attributes (interrupt transfers)
		0x08, 0x00, // Max packet size

		0x08, // Polling interval (frame counts)
	};

	struct event_udev_entry
	{
	  const char *devnode;
	  struct udev_list_entry *item;
	};

	static void guncon2_handle_control(
		USBDevice* dev, USBPacket* p, int request, int value, int index, int length, uint8_t* data)
	{
		GunCon2State* const us = USB_CONTAINER_OF(dev, GunCon2State, dev);

		// Apply configuration on the first control packet.
		// The ELF should be well and truely loaded by then.
		if (!us->auto_config_done && !us->custom_config)
		{
			us->AutoConfigure();
			us->auto_config_done = true;
		}

		DevCon.WriteLn("guncon2: req %04X val: %04X idx: %04X len: %d\n", request, value, index, length);
		if (usb_desc_handle_control(dev, p, request, value, index, length, data) >= 0)
			return;

		if (request == (ClassInterfaceOutRequest | 0x09))
		{
			us->param_x = static_cast<u16>(data[0]) | (static_cast<u16>(data[1]) << 8);
			us->param_y = static_cast<u16>(data[2]) | (static_cast<u16>(data[3]) << 8);
			us->param_mode = static_cast<u16>(data[4]) | (static_cast<u16>(data[5]) << 8);
			DevCon.WriteLn("GunCon2 Set Param %04X %d %d", us->param_mode, us->param_x, us->param_y);
			return;
		}

		p->status = USB_RET_STALL;
	}

	void updateState(GunCon2State* us, u32 bid, bool pressed) {
	  const u32 bit = 1u << bid;
	  if (pressed)
	    us->button_state |= bit;
	  else
	    us->button_state &= ~bit;
	}

	static bool udev_has(GunCon2State* us) {
	  return us->udev_fd != -1;
	}

	static void udev_handle_event(GunCon2State* us, input_event* event) {
	  switch (event->type) {
	  case EV_KEY:
	    switch (event->code) {
	    case BTN_LEFT:
		updateState(us, BID_TRIGGER, event->value == 1);
		break;
	    case BTN_RIGHT:
		updateState(us, BID_SHOOT_OFFSCREEN, event->value == 1);
		break;
	    case BTN_MIDDLE:
		updateState(us, BID_RECALIBRATE, event->value == 1);
		break;
	    case BTN_1:
		updateState(us, BID_START, event->value == 1);
		break;
	    case BTN_2:
		updateState(us, BID_A, event->value == 1);
		break;
	    case BTN_3:
		updateState(us, BID_B, event->value == 1);
		break;
	    case BTN_4:
		updateState(us, BID_C, event->value == 1);
		break;
	    case BTN_5:
		updateState(us, BID_DPAD_UP, event->value == 1);
		break;
	    case BTN_6:
		updateState(us, BID_DPAD_DOWN, event->value == 1);
		break;
	    case BTN_7:
		updateState(us, BID_DPAD_LEFT, event->value == 1);
		break;
	    case BTN_8:
		updateState(us, BID_DPAD_RIGHT, event->value == 1);
		break;
	    case BTN_9:
		break;
	    default:
		break;
	    }
	    break;

	  case EV_ABS:
	    switch (event->code) {
	    case ABS_X:
	      us->udev_internalGunX = ((event->value - us->udev_gunMinx) / ((float)(us->udev_gunMaxx - us->udev_gunMinx))) * g_gs_device->GetWindowWidth();
	      break;
	    case ABS_Y:
	      us->udev_internalGunY = ((event->value - us->udev_gunMiny) / ((float)(us->udev_gunMaxy - us->udev_gunMiny))) * g_gs_device->GetWindowHeight();
	      break;
	    }
	    break;
	  }
	}

	static void udev_poll_gun(GunCon2State* us) {
	  struct input_event input_events[32];
	  int j, len;

	  if(us->udev_fd == -1) return;

	  while ((len = read(us->udev_fd, input_events, sizeof(input_events))) > 0) {
	    len /= sizeof(*input_events);
	    for (j = 0; j < len; j++) {
		udev_handle_event(us, &(input_events[j]));
	    }
	  }
	}

	static void guncon2_handle_data(USBDevice* dev, USBPacket* p)
	{
		GunCon2State* const us = USB_CONTAINER_OF(dev, GunCon2State, dev);
		if(udev_has(us)) udev_poll_gun(us);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
			{
				if (p->ep->nr == 1)
				{
					const auto [pos_x, pos_y] = us->CalculatePosition();

					// Time Crisis games do a "calibration" by displaying a black frame for a single frame,
					// waiting for the gun to report (0, 0), and then computing an offset on the first non-zero
					// value. So, after the trigger is pulled, we wait for a few frames, then send the (0, 0)
					// report, then go back to normal values. To reduce error if the mouse is moving during
					// these frames (unlikely), we store the fire position and keep returning that.
					if (us->button_state & (1u << BID_RECALIBRATE) && us->calibration_timer == 0)
					{
						us->calibration_timer = GUNCON2_CALIBRATION_DELAY;
						us->calibration_pos_x = pos_x;
						us->calibration_pos_y = pos_y;
					}

					// Buttons are active low.
					GunCon2Out out;
					out.buttons = static_cast<u16>(~us->button_state) | (us->param_mode & GUNCON2_FLAG_PROGRESSIVE);
					out.pos_x = pos_x;
					out.pos_y = pos_y;

					if (us->calibration_timer > 0)
					{
						// Force trigger down while calibrating.
						out.buttons &= ~(1u << BID_TRIGGER);
						out.pos_x = us->calibration_pos_x;
						out.pos_y = us->calibration_pos_y;
						us->calibration_timer--;

						if (us->calibration_timer == 0)
						{
							out.pos_x = 0;
							out.pos_y = 0;
						}
					}
					else if (us->button_state & (1u << BID_SHOOT_OFFSCREEN))
					{
						// Offscreen shot - use 0,0.
						out.buttons &= ~(1u << BID_TRIGGER);
						out.pos_x = 0;
						out.pos_y = 0;
					}

					usb_packet_copy(p, &out, sizeof(out));
					break;
				}
			}
				[[fallthrough]];

			case USB_TOKEN_OUT:
			default:
			{
				Console.Error("Unhandled GunCon2 request pid=%d ep=%u", p->pid, p->ep->nr);
				p->status = USB_RET_STALL;
			}
			break;
		}
	}

	static void usb_hid_unrealize(USBDevice* dev)
	{
		GunCon2State* us = USB_CONTAINER_OF(dev, GunCon2State, dev);
		delete us;
	}

	GunCon2State::GunCon2State(u32 port_)
		: port(port_)
	{
	  udev_fd = -1;
	  udev_internalGunX = 0.0;
	  udev_internalGunY = 0.0;
	}

	GunCon2State::~GunCon2State()
	{
	  if(udev_fd != -1) close(udev_fd);
	}

	void GunCon2State::AutoConfigure()
	{
		const std::string serial(VMManager::GetGameSerial());

		for (const GameConfig& gc : s_game_config)
		{
			if (serial != gc.serial)
				continue;

			Console.WriteLn(fmt::format("(GunCon2) Using automatic config for '{}'", serial));
			Console.WriteLn(fmt::format("  Scale: {}x{}", gc.scale_x / 100.0f, gc.scale_y / 100.0f));
			Console.WriteLn(fmt::format("  Center Position: {}x{}", gc.center_x, gc.center_y));
			Console.WriteLn(fmt::format("  Screen Size: {}x{}", gc.screen_width, gc.screen_height));

			scale_x = gc.scale_x / 100.0f;
			scale_y = gc.scale_y / 100.0f;
			center_x = static_cast<float>(gc.center_x);
			center_y = static_cast<float>(gc.center_y);
			screen_width = gc.screen_width;
			screen_height = gc.screen_height;
			return;
		}

		Console.Warning(fmt::format("(GunCon2) No automatic config found for '{}'.", serial));
	}

	std::tuple<s16, s16> GunCon2State::CalculatePosition()
	{
		float pointer_x, pointer_y;
		const std::pair<float, float> abs_pos(InputManager::GetPointerAbsolutePosition(0));

		if(udev_has(this)) {
		  GSTranslateWindowToDisplayCoordinates(udev_internalGunX, udev_internalGunY, &pointer_x, &pointer_y);
		} else {
		  // basic mouse position
		  GSTranslateWindowToDisplayCoordinates(abs_pos.first, abs_pos.second, &pointer_x, &pointer_y);
		}

		s16 pos_x, pos_y;
		if (pointer_x < 0.0f || pointer_y < 0.0f)
		{
			// off-screen
			pos_x = 0;
			pos_y = 0;
		}
		else
		{
			// scale to internal coordinate system and center
			float fx = (pointer_x * static_cast<float>(screen_width)) - static_cast<float>(screen_width / 2u);
			float fy = (pointer_y * static_cast<float>(screen_height)) - static_cast<float>(screen_height / 2u);

			// apply curvature scale
			fx *= scale_x;
			fy *= scale_y;

			// and re-center based on game center
			s32 x = static_cast<s32>(std::round(fx + center_x));
			s32 y = static_cast<s32>(std::round(fy + center_y));

			// apply game-configured offset
			if (param_mode & GUNCON2_FLAG_PROGRESSIVE)
			{
				x -= param_x / 2;
				y -= param_y / 2;
			}
			else
			{
				x -= param_x;
				y -= param_y;
			}

			// 0,0 is reserved for offscreen, so ensure we don't send that
			pos_x = static_cast<s16>(std::max(x, 1));
			pos_y = static_cast<s16>(std::max(y, 1));
		}

		return std::tie(pos_x, pos_y);
	}

	const char* GunCon2Device::Name() const
	{
		return "GunCon 2";
	}

	const char* GunCon2Device::TypeName() const
	{
		return "guncon2";
	}

	int event_isNumber(const char *s) {
	  int n;

	  if(strlen(s) == 0) {
	    return 0;
	  }

	  for(n=0; n<strlen(s); n++) {
	    if(!(s[n] == '0' || s[n] == '1' || s[n] == '2' || s[n] == '3' || s[n] == '4' ||
	         s[n] == '5' || s[n] == '6' || s[n] == '7' || s[n] == '8' || s[n] == '9'))
	      return 0;
	  }
	  return 1;
	}

	// compare /dev/input/eventX and /dev/input/eventY where X and Y are numbers
	int event_strcmp_events(const char* x, const char* y) {
	  // find a common string
	  int n, common, is_number;
	  int a, b;

	  n=0;
	  while(x[n] == y[n] && x[n] != '\0' && y[n] != '\0') {
	    n++;
	  }
	  common = n;

	  // check if remaining string is a number
	  is_number = 1;
	  if(event_isNumber(x+common) == 0) is_number = 0;
	  if(event_isNumber(y+common) == 0) is_number = 0;

	  if(is_number == 1) {
	    a = atoi(x+common);
	    b = atoi(y+common);

	    if(a == b) return  0;
	    if(a < b)  return -1;
	    return 1;
	  } else {
	    return strcmp(x, y);
	  }
	}

	/* Used for sorting devnodes to appear in the correct order */
	int sort_devnodes(const void *a, const void *b)
	{
	  const struct event_udev_entry *aa = (const struct event_udev_entry*)a;
	  const struct event_udev_entry *bb = (const struct event_udev_entry*)b;
	  return event_strcmp_events(aa->devnode, bb->devnode);
	}

	void GunCon2Device::udev_open_gun(GunCon2State* us) {
	  struct udev_enumerate *enumerate;
	  struct udev_list_entry     *devs = NULL;
	  struct udev_list_entry     *item = NULL;
	  unsigned sorted_count = 0;
	  struct event_udev_entry sorted[8]; // max devices
	  unsigned int i;
	  struct udev *udev;
	  int fd = -1;

	  udev = udev_new();
	  if(udev == NULL) return;

	  enumerate = udev_enumerate_new(udev);

	  if (enumerate != NULL) {
	    udev_enumerate_add_match_property(enumerate, "ID_INPUT_GUN", "1");
	    udev_enumerate_add_match_subsystem(enumerate, "input");
	    udev_enumerate_scan_devices(enumerate);
	    devs = udev_enumerate_get_list_entry(enumerate);

	    for (item = devs; item; item = udev_list_entry_get_next(item)) {
	      const char         *name = udev_list_entry_get_name(item);
	      struct udev_device  *dev = udev_device_new_from_syspath(udev, name);
	      const char      *devnode = udev_device_get_devnode(dev);

	      if (devnode != NULL && sorted_count < 8) {
		sorted[sorted_count].devnode = devnode;
		sorted[sorted_count].item = item;
		sorted_count++;
	      } else {
		udev_device_unref(dev);
	      }
	    }

	    /* Sort the udev entries by devnode name so that they are
	     * created in the proper order */
	    qsort(sorted, sorted_count,
		  sizeof(struct event_udev_entry), sort_devnodes);

	    for (i = 0; i < sorted_count; i++) {
	      if(i == us->port) {
		const char *name = udev_list_entry_get_name(sorted[i].item);

		/* Get the filename of the /sys entry for the device
		 * and create a udev_device object (dev) representing it. */
		struct udev_device *dev = udev_device_new_from_syspath(udev, name);
		const char *devnode     = udev_device_get_devnode(dev);
		char devname[64];

		if (devnode) {
		  fd = open(devnode, O_RDONLY | O_NONBLOCK);
		  if (fd != -1) {
		    if (ioctl(fd, EVIOCGNAME(sizeof(devname)), devname) < 0) {
		      devname[0] = '\0';
		    }
		  }
		}
		udev_device_unref(dev);
	      }
	    }
	    udev_enumerate_unref(enumerate);
	  }
	  if (udev != NULL) udev_unref(udev);

	  // configure
	  us->udev_fd = fd;
	  if(fd != -1) {
	    udev_configure_gun(us);
	  }
	}

  	void GunCon2Device::udev_configure_gun(GunCon2State* us) {
	  struct input_absinfo absx, absy;
	  if(ioctl(us->udev_fd, EVIOCGABS(ABS_X), &absx) >= 0) {
	    if(ioctl(us->udev_fd, EVIOCGABS(ABS_Y), &absy) >= 0) {
	      us->udev_gunMinx = absx.minimum;
	      us->udev_gunMaxx = absx.maximum;
	      us->udev_gunMiny = absy.minimum;
	      us->udev_gunMaxy = absy.maximum;
	    }
	  }
	}

	USBDevice* GunCon2Device::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		GunCon2State* s = new GunCon2State(port);

		udev_open_gun(s);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(guncon2_dev_desc, sizeof(guncon2_dev_desc), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(guncon2_config_desc, sizeof(guncon2_config_desc), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_control = guncon2_handle_control;
		s->dev.klass.handle_data = guncon2_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		return &s->dev;
	fail:
		usb_hid_unrealize(&s->dev);
		return nullptr;
	}

	void GunCon2Device::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		GunCon2State* s = USB_CONTAINER_OF(dev, GunCon2State, dev);

		s->custom_config = USB::GetConfigBool(si, s->port, TypeName(), "custom_config", false);

		// Don't override auto config if we've set it.
		if (!s->auto_config_done || s->custom_config)
		{
			s->screen_width = USB::GetConfigInt(si, s->port, TypeName(), "screen_width", DEFAULT_SCREEN_WIDTH);
			s->screen_height = USB::GetConfigInt(si, s->port, TypeName(), "screen_height", DEFAULT_SCREEN_HEIGHT);
			s->center_x = USB::GetConfigFloat(si, s->port, TypeName(), "center_x", DEFAULT_CENTER_X);
			s->center_y = USB::GetConfigFloat(si, s->port, TypeName(), "center_y", DEFAULT_CENTER_Y);
			s->scale_x = USB::GetConfigFloat(si, s->port, TypeName(), "scale_x", DEFAULT_SCALE_X) / 100.0f;
			s->scale_y = USB::GetConfigFloat(si, s->port, TypeName(), "scale_y", DEFAULT_SCALE_Y) / 100.0f;
		}
	}

	float GunCon2Device::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		GunCon2State* s = USB_CONTAINER_OF(dev, GunCon2State, dev);

		const u32 bit = 1u << bind_index;
		return ((s->button_state & bit) != 0) ? 1.0f : 0.0f;
	}

	void GunCon2Device::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		GunCon2State* s = USB_CONTAINER_OF(dev, GunCon2State, dev);

		const u32 bit = 1u << bind_index;
		if (value >= 0.5f)
			s->button_state |= bit;
		else
			s->button_state &= ~bit;
	}

	gsl::span<const InputBindingInfo> GunCon2Device::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			//{"pointer", "Pointer/Aiming", InputBindingInfo::Type::Pointer, BID_POINTER_X, GenericInputBinding::Unknown},
			{"Up", "D-Pad Up", InputBindingInfo::Type::Button, BID_DPAD_UP, GenericInputBinding::DPadUp},
			{"Down", "D-Pad Down", InputBindingInfo::Type::Button, BID_DPAD_DOWN, GenericInputBinding::DPadDown},
			{"Left", "D-Pad Left", InputBindingInfo::Type::Button, BID_DPAD_LEFT, GenericInputBinding::DPadLeft},
			{"Right", "D-Pad Right", InputBindingInfo::Type::Button, BID_DPAD_RIGHT,
				GenericInputBinding::DPadRight},
			{"Trigger", "Trigger", InputBindingInfo::Type::Button, BID_TRIGGER, GenericInputBinding::R2},
			{"ShootOffscreen", "Shoot Offscreen", InputBindingInfo::Type::Button, BID_SHOOT_OFFSCREEN,
				GenericInputBinding::R1},
			{"Recalibrate", "Calibration Shot", InputBindingInfo::Type::Button, BID_RECALIBRATE,
				GenericInputBinding::Unknown},
			{"A", "A", InputBindingInfo::Type::Button, BID_A, GenericInputBinding::Cross},
			{"B", "B", InputBindingInfo::Type::Button, BID_B, GenericInputBinding::Circle},
			{"C", "C", InputBindingInfo::Type::Button, BID_C, GenericInputBinding::Triangle},
			{"Select", "Select", InputBindingInfo::Type::Button, BID_SELECT, GenericInputBinding::Select},
			{"Start", "Start", InputBindingInfo::Type::Button, BID_START, GenericInputBinding::Start},
		};

		return bindings;
	}

	gsl::span<const SettingInfo> GunCon2Device::Settings(u32 subtype) const
	{
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::Boolean, "custom_config", "Manual Screen Configuration",
				"Forces the use of the screen parameters below, instead of automatic parameters if available.",
				"false"},
			{SettingInfo::Type::Float, "scale_x", "X Scale (Sensitivity)",
				"Scales the position to simulate CRT curvature.", "100", "0", "200", "0.1", "%.2f%%", nullptr, nullptr,
				1.0f},
			{SettingInfo::Type::Float, "scale_y", "Y Scale (Sensitivity)",
				"Scales the position to simulate CRT curvature.", "100", "0", "200", "0.1", "%.2f%%", nullptr, nullptr,
				1.0f},
			{SettingInfo::Type::Float, "center_x", "Center X", "Sets the horizontal center position of the simulated screen.",
				"320", "0", "1024", "1", "%.0fpx", nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Float, "center_y", "Center Y", "Sets the vertical center position of the simulated screen.",
				"120", "0", "1024", "1", "%.0fpx", nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Integer, "screen_width", "Screen Width", "Sets the width of the simulated screen.",
				"640", "1", "1024", "1", "%dpx", nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Integer, "screen_height", "Screen Height", "Sets the height of the simulated screen.",
				"240", "1", "1024", "1", "%dpx", nullptr, nullptr, 1.0f},
		};
		return info;
	}

	bool GunCon2Device::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		GunCon2State* s = USB_CONTAINER_OF(dev, GunCon2State, dev);

		if (!sw.DoMarker("GunCon2Device"))
			return false;

		sw.Do(&s->param_x);
		sw.Do(&s->param_y);
		sw.Do(&s->param_mode);
		sw.Do(&s->calibration_timer);
		sw.Do(&s->calibration_pos_x);
		sw.Do(&s->calibration_pos_y);
		sw.Do(&s->auto_config_done);

		float scale_x = s->scale_x;
		float scale_y = s->scale_y;
		float center_x = s->center_x;
		float center_y = s->center_y;
		u32 screen_width = s->screen_width;
		u32 screen_height = s->screen_height;
		sw.Do(&scale_x);
		sw.Do(&scale_y);
		sw.Do(&center_x);
		sw.Do(&center_y);
		sw.Do(&screen_width);
		sw.Do(&screen_height);

		// Only save automatic settings to state.
		if (sw.IsReading() && !s->custom_config && s->auto_config_done)
		{
			s->scale_x = scale_x;
			s->scale_y = scale_y;
			s->center_x = center_x;
			s->center_y = center_y;
			s->screen_width = screen_width;
			s->screen_height = screen_height;
		}

		return !sw.HasError();
	}
} // namespace usb_lightgun
