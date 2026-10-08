// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS.h"
#include "GS/GS.h"
#include "Host.h"
#include "MTGS.h"
#include "IconsPromptFont.h"
#include "ImGui/ImGuiManager.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-lightgun/guncon2.h"
#include "VMManager.h"

#include "common/Console.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <cstring>
#include <tuple>

#include "fmt/format.h"

namespace usb_lightgun
{
	enum : u32
	{
		GUNCON2_FLAG_PROGRESSIVE = 0x0100,

		GUNCON2_CALIBRATION_DELAY = 12,
		GUNCON2_CALIBRATION_REPORT_DELAY = 5,
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
		BID_RELATIVE_LEFT = 18,
		BID_RELATIVE_RIGHT = 19,
		BID_RELATIVE_UP = 20,
		BID_RELATIVE_DOWN = 21,
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
		{"SLPS-20122", 90.25f, 97.5f, 390, 154, 640, 240}, // Time Crisis II (J)
		{"SLPS-20113", 90.25f, 97.5f, 390, 154, 640, 240}, // Time Crisis II (with GunCon 2) (J)
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
		{"SLPM-62205", 110.15f, 100.0f, 433, 159, 512, 256}, // Virtua Cop Re-Birth (J) (480i)
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

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;

		//////////////////////////////////////////////////////////////////////////
		// Configuration
		//////////////////////////////////////////////////////////////////////////
		bool has_relative_binds = false;
		bool custom_config = false;

		// Which host pointer (Pointer-N) this gun aims with. Only differs from 0 when multi-mouse
		// raw input is enabled on Windows. -1 in the config means "follow the Trigger binding".
		u32 pointer_index = 0;
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
		std::string cursor_path;
		float cursor_scale = 1.0f;
		u32 cursor_color = 0xFFFFFFFF;
		float relative_pos[4] = {};

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

		// Shot diagnostics (not saved): log where each trigger pull aimed, and where the aim was a few polls later.
		bool trigger_was_down = false;
		u8 shot_log_polls = 0;
		s16 shot_pos_x = 0, shot_pos_y = 0;
		u64 last_display_key = 0;

		// Debug: screenshot a few polls after each trigger pull ([USBn] guncon2_shot_screenshots = true).
		bool shot_screenshots = false;
		u8 shot_screenshot_delay = 12; // polls after the trigger; the game flashes the screen for the first few
		u8 shot_screenshot_polls = 0;
		u32 shot_counter = 0;
		float shot_display_x = 0.0f, shot_display_y = 0.0f;
		void LogDisplayModeChanges();

		// Real-TV mapping: a real GunCon reads the CRT beam position, so if a game shrinks/moves its picture
		// (e.g. Time Crisis 2's half-height 2-player split screen) the readings change accordingly. PCSX2
		// stretches the picture to the window, so we map the aim back to where it would be on the CRT, relative
		// to the largest picture the game has shown (its normal full-screen mode, which the scale values assume).
		struct CRTRect
		{
			float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
			bool valid() const { return (x1 > x0 && y1 > y0); }
		};
		CRTRect display_ref; // largest display area seen
		CRTRect display_cur;
		bool real_tv_mapping = true;
		// extra vertical correction while the picture is reduced (e.g. TC2 2P), separately above/below the middle
		float reduced_picture_y_gain_top = 1.02f;
		float reduced_picture_y_gain_bottom = 0.78f;
		bool split_aim_tuned = false; // the gains above were measured for this game (Time Crisis 2/3 NTSC)
		std::string geometry_serial; // game the display geometry/tuning below belongs to
		void UpdateDisplayRects();

		void AutoConfigure();

		std::tuple<s16, s16> CalculatePosition();

		// 0..1, not -1..1.
		std::pair<float, float> GetAbsolutePositionFromRelativeAxes() const;
		u32 GetSoftwarePointerIndex() const;
		void UpdateSoftwarePointerPosition();
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
			// Logged so unexpected recalibrations (offset changes mid-game) show up. Needs proper testing.
			Console.WriteLn("(GunCon2) USB port %u: game set calibration offset x=%d y=%d (mode %04X)", us->port + 1,
				us->param_x, us->param_y, us->param_mode);
			return;
		}

		p->status = USB_RET_STALL;
	}

	static void guncon2_handle_data(USBDevice* dev, USBPacket* p)
	{
		GunCon2State* const us = USB_CONTAINER_OF(dev, GunCon2State, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
			{
				if (p->ep->nr == 1)
				{
					const auto [pos_x, pos_y] = us->CalculatePosition();
					us->LogDisplayModeChanges();

					// Shot diagnostics: did the aim move while the trigger was being pulled (e.g. recoil)?
					const bool trigger_down = (us->button_state & (1u << BID_TRIGGER)) != 0;
					if (trigger_down && !us->trigger_was_down)
					{
						us->shot_pos_x = pos_x;
						us->shot_pos_y = pos_y;
						us->shot_log_polls = 6;

						if (us->shot_screenshots)
						{
							// Where the gun was aimed, as a fraction of the game picture (0..1) - same as the screenshot area.
							const auto& [wx, wy] = InputManager::GetPointerAbsolutePosition(us->pointer_index);
							GSTranslateWindowToDisplayCoordinates(wx, wy, &us->shot_display_x, &us->shot_display_y);
							us->shot_screenshot_polls = us->shot_screenshot_delay;
						}
					}
					if (us->shot_screenshot_polls > 0 && --us->shot_screenshot_polls == 0)
					{
						const std::string path = Path::Combine(EmuFolders::Snapshots,
							fmt::format("guncon_port{}_shot{:03}.png", us->port + 1, ++us->shot_counter));
						Console.WriteLn("(GunCon2) USB port %u: shot screenshot %03u aim=(%.4f,%.4f) guncon=(%d,%d) -> %s", us->port + 1,
							us->shot_counter, us->shot_display_x, us->shot_display_y, us->shot_pos_x, us->shot_pos_y, path.c_str());
						MTGS::RunOnGSThread([path]() { GSQueueSnapshot(path, 0); });
					}
					us->trigger_was_down = trigger_down;
					if (us->shot_log_polls > 0 && --us->shot_log_polls == 0)
					{
						Console.WriteLn("(GunCon2) USB port %u: shot at (%d,%d), 6 polls later (%d,%d), moved (%+d,%+d)", us->port + 1,
							us->shot_pos_x, us->shot_pos_y, pos_x, pos_y, pos_x - us->shot_pos_x, pos_y - us->shot_pos_y);
					}

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

						if (us->calibration_timer < GUNCON2_CALIBRATION_REPORT_DELAY)
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

		if (!us->cursor_path.empty())
			ImGuiManager::ClearSoftwareCursor(us->GetSoftwarePointerIndex());

		delete us;
	}

	GunCon2State::GunCon2State(u32 port_)
		: port(port_)
	{
	}

	void GunCon2State::AutoConfigure()
	{
		const std::string serial = VMManager::GetDiscSerial();

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

	// Diagnostic: log the PS2's display output settings whenever they change, to see whether a game
	// (e.g. Time Crisis 2 in 2-player split screen) moves/resizes its picture. A real GunCon reads the
	// TV beam position, so such a change shifts its readings, while PCSX2 uses fixed per-game numbers.
	void GunCon2State::LogDisplayModeChanges()
	{
		GSRegPMODE pmode;
		GSRegSMODE2 smode2;
		GSRegDISPLAY disp[2];
		std::memcpy(&pmode, PS2GS_BASE(0x0000), sizeof(pmode));
		std::memcpy(&smode2, PS2GS_BASE(0x0020), sizeof(smode2));
		std::memcpy(&disp[0], PS2GS_BASE(0x0080), sizeof(disp[0]));
		std::memcpy(&disp[1], PS2GS_BASE(0x00A0), sizeof(disp[1]));

		u64 d0, d1;
		std::memcpy(&d0, &disp[0], sizeof(d0));
		std::memcpy(&d1, &disp[1], sizeof(d1));
		const u64 key = d0 ^ (d1 * 0x9E3779B97F4A7C15ULL) ^ (static_cast<u64>(pmode.EN1 | (pmode.EN2 << 1) | (smode2.INT << 2) | (smode2.FFMD << 3)) << 60);
		if (key == last_display_key)
			return;
		last_display_key = key;

		Console.WriteLn("(GunCon2) USB port %u: display changed: EN1=%u EN2=%u INT=%u FFMD=%u | "
						"D1 DX=%u DY=%u DW=%u DH=%u MAGH=%u MAGV=%u | D2 DX=%u DY=%u DW=%u DH=%u MAGH=%u MAGV=%u",
			port + 1, pmode.EN1, pmode.EN2, smode2.INT, smode2.FFMD, disp[0].DX, disp[0].DY, disp[0].DW, disp[0].DH,
			disp[0].MAGH, disp[0].MAGV, disp[1].DX, disp[1].DY, disp[1].DW, disp[1].DH, disp[1].MAGH, disp[1].MAGV);
	}

	void GunCon2State::UpdateDisplayRects()
	{
		GSRegPMODE pmode;
		GSRegDISPLAY disp[2];
		std::memcpy(&pmode, PS2GS_BASE(0x0000), sizeof(pmode));
		std::memcpy(&disp[0], PS2GS_BASE(0x0080), sizeof(disp[0]));
		std::memcpy(&disp[1], PS2GS_BASE(0x00A0), sizeof(disp[1]));

		// Union of the enabled read circuits (2-player split screen uses both, side by side).
		CRTRect r;
		bool any = false;
		const bool enabled[2] = {pmode.EN1 != 0, pmode.EN2 != 0};
		for (u32 i = 0; i < 2; i++)
		{
			if (!enabled[i] || disp[i].DW == 0 || disp[i].DH == 0)
				continue;
			const float x0 = static_cast<float>(disp[i].DX);
			const float y0 = static_cast<float>(disp[i].DY);
			const float x1 = x0 + static_cast<float>(disp[i].DW + 1);
			const float y1 = y0 + static_cast<float>(disp[i].DH + 1);
			if (!any)
			{
				r = CRTRect{x0, y0, x1, y1};
				any = true;
			}
			else
			{
				r.x0 = std::min(r.x0, x0);
				r.y0 = std::min(r.y0, y0);
				r.x1 = std::max(r.x1, x1);
				r.y1 = std::max(r.y1, y1);
			}
		}

		// While the picture is blanked (both circuits off, e.g. scene changes), keep the last geometry.
		if (!any)
			return;

		display_cur = r;
		if (!display_ref.valid() || ((r.x1 - r.x0) * (r.y1 - r.y0)) > ((display_ref.x1 - display_ref.x0) * (display_ref.y1 - display_ref.y0)))
			display_ref = r;
	}

	std::tuple<s16, s16> GunCon2State::CalculatePosition()
	{
		float pointer_x, pointer_y;
		const auto& [window_x, window_y] =
			(has_relative_binds) ? GetAbsolutePositionFromRelativeAxes() : InputManager::GetPointerAbsolutePosition(pointer_index);
		GSTranslateWindowToDisplayCoordinates(window_x, window_y, &pointer_x, &pointer_y);

		// Map from "fraction of the stretched picture" to "fraction of the game's normal full-screen area on a
		// real TV". No change while the game shows its normal full picture. Needs proper testing with other games.
		// New game (or a savestate of another game): forget the previous game's picture geometry, and only
		// use the split-screen aim corrections for the game they were measured on.
		if (std::string serial = VMManager::GetDiscSerial(); serial != geometry_serial)
		{
			// Time Crisis 2 and 3 (NTSC) use the same split-screen layout and measured the same.
			split_aim_tuned = (serial == "SLUS-20219" || serial == "SLPS-20122" || serial == "SLPS-20113" ||
							   serial == "SLUS-20645");
			Console.WriteLn(fmt::format("(GunCon2) USB port {}: game '{}', split-screen aim tuning {}", port + 1, serial,
				split_aim_tuned ? "on" : "off"));
			geometry_serial = std::move(serial);
			display_ref = CRTRect();
			display_cur = CRTRect();
		}
		UpdateDisplayRects();
		if (real_tv_mapping && pointer_x >= 0.0f && pointer_y >= 0.0f && display_cur.valid() && display_ref.valid())
		{
			const CRTRect& c = display_cur;
			const CRTRect& f = display_ref;
			pointer_x = (c.x0 + pointer_x * (c.x1 - c.x0) - f.x0) / (f.x1 - f.x0);
			pointer_y = (c.y0 + pointer_y * (c.y1 - c.y0) - f.y0) / (f.y1 - f.y0);

			// Measured in Time Crisis 2's 2-player split screen: after the mapping above, hits below the middle
			// needed ~0.78x and hits above it ~1.02x (fitted from shot screenshots; the game responds differently above/below). Only applied
			// while the game shows a shorter picture than its normal one, so 1-player aim is unchanged.
			if (split_aim_tuned && (c.y1 - c.y0) < (f.y1 - f.y0) * 0.9f)
			{
				const float gain = (pointer_y < 0.5f) ? reduced_picture_y_gain_top : reduced_picture_y_gain_bottom;
				pointer_y = std::max(0.0f, 0.5f + (pointer_y - 0.5f) * gain);
			}
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

	std::pair<float, float> GunCon2State::GetAbsolutePositionFromRelativeAxes() const
	{
		const float screen_rel_x = (((relative_pos[1] > 0.0f) ? relative_pos[1] : -relative_pos[0]) + 1.0f) * 0.5f;
		const float screen_rel_y = (((relative_pos[3] > 0.0f) ? relative_pos[3] : -relative_pos[2]) + 1.0f) * 0.5f;
		return std::make_pair(
			screen_rel_x * ImGuiManager::GetWindowWidth(), screen_rel_y * ImGuiManager::GetWindowHeight());
	}

	u32 GunCon2State::GetSoftwarePointerIndex() const
	{
		return has_relative_binds ? (InputManager::MAX_POINTER_DEVICES + port) : pointer_index;
	}

	void GunCon2State::UpdateSoftwarePointerPosition()
	{
		if (cursor_path.empty())
			return;

		const auto& [window_x, window_y] = GetAbsolutePositionFromRelativeAxes();
		ImGuiManager::SetSoftwareCursorPosition(GetSoftwarePointerIndex(), window_x, window_y);
	}

	// Short, readable label for a raw input device path such as
	// \\?\HID#VID_16C0&PID_0F38&MI_02#7&2a1b3c4d&0&0000#{guid}  ->  VID_16C0&PID_0F38&MI_02 [7&2a1b3c4d&0&0000]
	static std::string GetShortDeviceLabel(const std::string& path)
	{
		const size_t vid = path.find("VID_");
		if (vid == std::string::npos)
			return path;

		const size_t id_end = path.find('#', vid);
		std::string label = path.substr(vid, (id_end == std::string::npos) ? std::string::npos : (id_end - vid));
		if (id_end != std::string::npos)
		{
			const size_t inst_end = path.find('#', id_end + 1);
			const std::string instance = path.substr(id_end + 1, (inst_end == std::string::npos) ? std::string::npos : (inst_end - id_end - 1));
			if (!instance.empty())
				label += fmt::format(" [{}]", instance);
		}
		return label;
	}

	static std::vector<std::pair<std::string, std::string>> GetLightgunDeviceOptions()
	{
		std::vector<std::pair<std::string, std::string>> options;
		options.emplace_back(std::string(), TRANSLATE_STR("USB", "Auto (aim follows the Trigger binding)"));

		const std::vector<std::string> assigned = InputManager::GetHostPointerDeviceNames();
		for (size_t i = 0; i < assigned.size(); i++)
		{
			if (!assigned[i].empty())
				options.emplace_back(assigned[i], fmt::format("{} (now Pointer-{})", GetShortDeviceLabel(assigned[i]), i));
		}
		for (const std::string& name : InputManager::GetUnassignedHostPointerDeviceNames())
			options.emplace_back(name, fmt::format("{} (no pointer)", GetShortDeviceLabel(name)));

		return options;
	}

	const char* GunCon2Device::Name() const
	{
		return TRANSLATE_NOOP("USB", "GunCon 2");
	}

	const char* GunCon2Device::TypeName() const
	{
		return "guncon2";
	}

	const char* GunCon2Device::IconName() const
	{
		return ICON_PF_GUNCON2;
	}

	USBDevice* GunCon2Device::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		GunCon2State* s = new GunCon2State(port);
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

		UpdateSettings(&s->dev, si);

		return &s->dev;
	fail:
		usb_hid_unrealize(&s->dev);
		return nullptr;
	}

	void GunCon2Device::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		GunCon2State* s = USB_CONTAINER_OF(dev, GunCon2State, dev);

		s->custom_config = USB::GetConfigBool(si, s->port, TypeName(), "custom_config", false);
		s->shot_screenshots = USB::GetConfigBool(si, s->port, TypeName(), "shot_screenshots", false);
		s->real_tv_mapping = USB::GetConfigBool(si, s->port, TypeName(), "real_tv_mapping", true);
		s->reduced_picture_y_gain_top = std::clamp(USB::GetConfigFloat(si, s->port, TypeName(), "reduced_picture_y_gain_top", 102.0f), 25.0f, 200.0f) / 100.0f;
		s->reduced_picture_y_gain_bottom = std::clamp(USB::GetConfigFloat(si, s->port, TypeName(), "reduced_picture_y_gain_bottom", 78.0f), 25.0f, 200.0f) / 100.0f;
		s->shot_screenshot_delay = static_cast<u8>(std::clamp(USB::GetConfigInt(si, s->port, TypeName(), "shot_screenshot_delay", 12), 1, 120));

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

		// Pointer settings.
		const std::string pointer_binding = USB::GetConfigString(si, s->port, TypeName(), "Pointer", "");
		std::string cursor_path(USB::GetConfigString(si, s->port, TypeName(), "cursor_path"));
		// The settings file picker can store paths relative to the user data folder; resolve them there,
		// not against the working directory, or the image won't load.
		if (!cursor_path.empty() && !Path::IsAbsolute(cursor_path))
			cursor_path = Path::Combine(EmuFolders::DataRoot, cursor_path);
		const float cursor_scale = USB::GetConfigFloat(si, s->port, TypeName(), "cursor_scale", 1.0f);
		u32 cursor_color = 0xFFFFFF;
		if (std::string cursor_color_str(USB::GetConfigString(si, s->port, TypeName(), "cursor_color")); !cursor_color_str.empty())
		{
			// Strip the leading hash, if it's a CSS style colour.
			const std::optional<u32> cursor_color_opt(
				StringUtil::FromChars<u32>(cursor_color_str[0] == '#' ?
					std::string_view(cursor_color_str).substr(1) : std::string_view(cursor_color_str), 16));
			if (cursor_color_opt.has_value())
				cursor_color = cursor_color_opt.value();
		}

		const s32 prev_pointer_index = s->GetSoftwarePointerIndex();

		// Pick the host pointer to aim with. Needs proper testing with real multi-gun setups.
		const s32 aim_pointer_setting = USB::GetConfigInt(si, s->port, TypeName(), "pointer_index", -1);
		if (aim_pointer_setting >= 0)
		{
			s->pointer_index = static_cast<u32>(aim_pointer_setting);
		}
		else if (!USB::GetConfigString(si, s->port, TypeName(), "lightgun_device", "").empty())
		{
			// A specific gun was picked for this port; the host pins it to Pointer-<port>.
			s->pointer_index = s->port;
		}
		else
		{
			// Auto: if the trigger is bound to e.g. "Pointer-1/LeftButton", aim with Pointer-1 too.
			s->pointer_index = 0;
			const std::string trigger_binding = USB::GetConfigString(si, s->port, TypeName(), "Trigger", "");
			const std::string_view trigger_device = std::string_view(trigger_binding).substr(0, trigger_binding.find('/'));
			if (const std::optional<u32> idx = InputManager::GetIndexFromPointerBinding(trigger_device); idx.has_value())
				s->pointer_index = idx.value();
		}
		if (s->pointer_index >= InputManager::MAX_POINTER_DEVICES)
			s->pointer_index = 0;

		s->has_relative_binds = (USB::ConfigKeyExists(si, s->port, TypeName(), "RelativeLeft") ||
			USB::ConfigKeyExists(si, s->port, TypeName(), "RelativeRight") ||
			USB::ConfigKeyExists(si, s->port, TypeName(), "RelativeUp") ||
			USB::ConfigKeyExists(si, s->port, TypeName(), "RelativeDown"));

		const s32 new_pointer_index = s->GetSoftwarePointerIndex();

		if (prev_pointer_index != new_pointer_index || s->cursor_path != cursor_path ||
			s->cursor_scale != cursor_scale || s->cursor_color != cursor_color)
		{
			if (prev_pointer_index != new_pointer_index)
				ImGuiManager::ClearSoftwareCursor(prev_pointer_index);

			// Pointer changed, so need to update software cursor.
			const bool had_software_cursor = !s->cursor_path.empty();
			s->cursor_path = std::move(cursor_path);
			s->cursor_scale = cursor_scale;
			s->cursor_color = cursor_color;
			if (!s->cursor_path.empty())
			{
				ImGuiManager::SetSoftwareCursor(new_pointer_index, s->cursor_path, s->cursor_scale, s->cursor_color);
				s->UpdateSoftwarePointerPosition();
			}
			else if (had_software_cursor)
			{
				ImGuiManager::ClearSoftwareCursor(new_pointer_index);
			}
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

		if (bind_index < BID_RELATIVE_LEFT)
		{
			const u32 bit = 1u << bind_index;
			if (value >= 0.5f)
				s->button_state |= bit;
			else
				s->button_state &= ~bit;
		}
		else if (bind_index <= BID_RELATIVE_DOWN)
		{
			const u32 rel_index = bind_index - BID_RELATIVE_LEFT;
			if (s->relative_pos[rel_index] != value)
			{
				s->relative_pos[rel_index] = value;
				s->UpdateSoftwarePointerPosition();
			}
		}
	}

	std::span<const InputBindingInfo> GunCon2Device::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			//{"pointer", "Pointer/Aiming", InputBindingInfo::Type::Pointer, BID_POINTER_X, GenericInputBinding::Unknown},
			{"Up", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, BID_DPAD_UP, GenericInputBinding::DPadUp},
			{"Down", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, BID_DPAD_DOWN, GenericInputBinding::DPadDown},
			{"Left", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, BID_DPAD_LEFT, GenericInputBinding::DPadLeft},
			{"Right", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, BID_DPAD_RIGHT,
				GenericInputBinding::DPadRight},
			{"Trigger", TRANSLATE_NOOP("USB", "Trigger"), nullptr, InputBindingInfo::Type::Button, BID_TRIGGER, GenericInputBinding::R2},
			{"ShootOffscreen", TRANSLATE_NOOP("USB", "Shoot Offscreen"), nullptr, InputBindingInfo::Type::Button, BID_SHOOT_OFFSCREEN,
				GenericInputBinding::R1},
			{"Recalibrate", TRANSLATE_NOOP("USB", "Calibration Shot (not Trigger)"), nullptr, InputBindingInfo::Type::Button, BID_RECALIBRATE,
				GenericInputBinding::Unknown},
			{"A", TRANSLATE_NOOP("USB", "A"), nullptr, InputBindingInfo::Type::Button, BID_A, GenericInputBinding::Cross},
			{"B", TRANSLATE_NOOP("USB", "B"), nullptr, InputBindingInfo::Type::Button, BID_B, GenericInputBinding::Circle},
			{"C", TRANSLATE_NOOP("USB", "C"), nullptr, InputBindingInfo::Type::Button, BID_C, GenericInputBinding::Triangle},
			{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, BID_SELECT, GenericInputBinding::Select},
			{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, BID_START, GenericInputBinding::Start},
			{"RelativeLeft", TRANSLATE_NOOP("USB", "Relative Left"), nullptr, InputBindingInfo::Type::HalfAxis, BID_RELATIVE_LEFT, GenericInputBinding::Unknown},
			{"RelativeRight", TRANSLATE_NOOP("USB", "Relative Right"), nullptr, InputBindingInfo::Type::HalfAxis, BID_RELATIVE_RIGHT, GenericInputBinding::Unknown},
			{"RelativeUp", TRANSLATE_NOOP("USB", "Relative Up"), nullptr, InputBindingInfo::Type::HalfAxis, BID_RELATIVE_UP, GenericInputBinding::Unknown},
			{"RelativeDown", TRANSLATE_NOOP("USB", "Relative Down"), nullptr, InputBindingInfo::Type::HalfAxis, BID_RELATIVE_DOWN, GenericInputBinding::Unknown},
		};

		return bindings;
	}

	std::span<const SettingInfo> GunCon2Device::Settings(u32 subtype) const
	{
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::Path, "cursor_path", TRANSLATE_NOOP("USB", "Cursor Path"),
				TRANSLATE_NOOP("USB", "Sets the crosshair image that this lightgun will use. Setting a crosshair image "
									  "will disable the system cursor."),
				""},
			{SettingInfo::Type::Float, "cursor_scale", TRANSLATE_NOOP("USB", "Cursor Scale"),
				TRANSLATE_NOOP("USB", "Scales the crosshair image set above."), "1", "0.01", "10", "0.01", TRANSLATE_NOOP("USB", "%.0f%%"),
				nullptr, nullptr, 100.0f},
			{SettingInfo::Type::String, "cursor_color", TRANSLATE_NOOP("USB", "Cursor Color"),
				TRANSLATE_NOOP("USB", "Applies a color to the chosen crosshair images, can be used for multiple "
									  "players. Specify in HTML/CSS format (e.g. #aabbcc)"),
				"#ffffff"},
			{SettingInfo::Type::StringList, "lightgun_device", TRANSLATE_NOOP("USB", "Lightgun Device"),
				TRANSLATE_NOOP("USB", "Which physical mouse/lightgun this port uses (needs multi-mouse raw input). The chosen gun "
									  "always gets Pointer-0 on USB port 1 and Pointer-1 on USB port 2, so bind this port's buttons to that "
									  "pointer. Takes effect when a game starts or a binding is set."),
				"", nullptr, nullptr, nullptr, nullptr, nullptr, &GetLightgunDeviceOptions, 1.0f},
			{SettingInfo::Type::Integer, "pointer_index", TRANSLATE_NOOP("USB", "Aim Pointer"),
				TRANSLATE_NOOP("USB", "Which mouse/lightgun (Pointer-N) aims this gun. -1 follows the Trigger binding. "
									  "Values above 0 need multi-mouse raw input enabled in Controller settings."),
				"-1", "-1", "3", "1", TRANSLATE_NOOP("USB", "%d"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Boolean, "real_tv_mapping", TRANSLATE_NOOP("USB", "Follow Game Resolution Changes"),
				TRANSLATE_NOOP("USB", "Adjusts aiming when a game switches to a smaller or lower-resolution picture (e.g. Time Crisis 2's "
									  "2-player split screen), so shots keep landing under the crosshair. Turn off if a game's aim is off "
									  "only when this is on."),
				"true"},
			{SettingInfo::Type::Float, "reduced_picture_y_gain_top", TRANSLATE_NOOP("USB", "Split Screen Aim: Top Half"),
				TRANSLATE_NOOP("USB", "Extra up/down correction for the top half of the screen, used only while the game shows a smaller "
									  "picture (e.g. Time Crisis 2 2-player split screen) and Follow Game Resolution Changes is on. Lower it "
									  "if shots land above the crosshair; raise it if they land below. 100% = no extra correction."),
				"102", "25", "200", "1", TRANSLATE_NOOP("USB", "%.0f%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Float, "reduced_picture_y_gain_bottom", TRANSLATE_NOOP("USB", "Split Screen Aim: Bottom Half"),
				TRANSLATE_NOOP("USB", "Same as above for the bottom half of the screen. Lower it if shots land below the crosshair; "
									  "raise it if they land above."),
				"78", "25", "200", "1", TRANSLATE_NOOP("USB", "%.0f%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Boolean, "custom_config", TRANSLATE_NOOP("USB", "Manual Screen Configuration"),
				TRANSLATE_NOOP("USB",
					"Forces the use of the screen parameters below, instead of automatic parameters if available."),
				"false"},
			{SettingInfo::Type::Float, "scale_x", TRANSLATE_NOOP("USB", "X Scale (Sensitivity)"),
				TRANSLATE_NOOP("USB", "Scales the position to simulate CRT curvature."), "100", "0", "200", "0.1",
				TRANSLATE_NOOP("USB", "%.2f%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Float, "scale_y", TRANSLATE_NOOP("USB", "Y Scale (Sensitivity)"),
				TRANSLATE_NOOP("USB", "Scales the position to simulate CRT curvature."), "100", "0", "200", "0.1",
				TRANSLATE_NOOP("USB", "%.2f%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Float, "center_x", TRANSLATE_NOOP("USB", "Center X"),
				TRANSLATE_NOOP("USB", "Sets the horizontal center position of the simulated screen."), "320", "0",
				"1024", "1", TRANSLATE_NOOP("USB", "%.0fpx"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Float, "center_y", TRANSLATE_NOOP("USB", "Center Y"),
				TRANSLATE_NOOP("USB", "Sets the vertical center position of the simulated screen."), "120", "0", "1024",
				"1", TRANSLATE_NOOP("USB", "%.0fpx"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Integer, "screen_width", TRANSLATE_NOOP("USB", "Screen Width"),
				TRANSLATE_NOOP("USB", "Sets the width of the simulated screen."), "640", "1", "1024", "1", TRANSLATE_NOOP("USB", "%dpx"),
				nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Integer, "screen_height", TRANSLATE_NOOP("USB", "Screen Height"),
				TRANSLATE_NOOP("USB", "Sets the height of the simulated screen."), "240", "1", "1024", "1", TRANSLATE_NOOP("USB", "%dpx"),
				nullptr, nullptr, 1.0f},
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
