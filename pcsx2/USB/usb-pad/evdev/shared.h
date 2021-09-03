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

#pragma once
#include <linux/joystick.h>
#include <unistd.h>
#include "common/Pcsx2Types.h"
#include "USB/gtk.h"
#include "USB/usb-pad/padproxy.h"
#include "USB/configuration.h"

#define N_HIDRAW_FF_PT "hidraw_ff_pt"
#define N_GAIN_ENABLED "gain_enabled"
#define N_GAIN "gain"
#define N_AUTOCENTER "autocenter"
#define N_AUTOCENTER_MANAGED "ac_managed"

struct evdev_device
{
	std::string name;
	std::string id;
	std::string path;
	struct {
		uint16_t bustype;
		uint16_t vendor;
		uint16_t product;
		uint16_t version;
	} input_id;
};

typedef std::vector<evdev_device> device_list;

namespace usb_pad
{
	namespace evdev
	{

		enum
		{
			COL_NAME = 0,
			COL_PS2,
			COL_PC,
			COL_BINDING,
			NUM_COLS
		};

		// Keep in sync with PS2Buttons enum
		enum JoystickMap
		{
			JOY_CROSS = 0,
			JOY_SQUARE,
			JOY_CIRCLE,
			JOY_TRIANGLE,
			JOY_R1,
			JOY_L1,
			JOY_R2,
			JOY_L2,
			JOY_SELECT,
			JOY_START,
			JOY_R3,
			JOY_L3, //order, afaik not used on any PS2 wheel anyway
			JOY_DOWN,
			JOY_LEFT,
			JOY_UP,
			JOY_RIGHT,
			JOY_STEERING,
			JOY_THROTTLE,
			JOY_BRAKE,
			JOY_MAPS_COUNT
		};

		constexpr const char* JoystickMapNames[]{
			"cross",
			"square",
			"circle",
			"triangle",
			"r1",
			"l1",
			"r2",
			"l2",
			"select",
			"start",
			"r3",
			"l3",
			"down",
			"left",
			"up",
			"right",
			"steering",
			"throttle",
			"brake",
		};

		constexpr const char* buzz_map_names[]{
			"red",
			"yellow",
			"green",
			"orange",
			"blue",
		};

		constexpr const char* kbdmania_key_labels[]{
			"C 1",
			"C# 1",
			"D 1",
			"D# 1",
			"E 1",
			"F 1",
			"F# 1",
			"",
			"G 1",
			"G# 1",
			"A 1",
			"A# 1",
			"B 1",
			"C 2",
			"Select",
			"",
			"C# 2",
			"D 2",
			"D# 2",
			"E 2",
			"F 2",
			"F# 2",
			"Start",
			"",
			"G 2",
			"G# 2",
			"A 2",
			"A# 2",
			"B 2",
			"Up",
			"Down",
		};

		struct Point
		{
			int x;
			int y;
			JoystickMap type;
		};

		struct ConfigMapping
		{
			std::vector<int16_t> controls;
			int inverted[3];
			int initial[3];
			int fd = -1;

			ConfigMapping() = default;
			ConfigMapping(int fd_)
				: fd(fd_)
			{
			}
		};

		struct ApiCallbacks
		{
			bool (*get_event_name)(const char* dev_type, int map, int event, bool is_button, const char** name);
			void (*populate)(device_list& jsdata);
			bool (*poll)(const std::vector<std::pair<std::string, ConfigMapping>>& jsconf, std::string& dev_name, bool isaxis, int& value, bool& inverted, int& initial);
		};

		typedef std::pair<std::string, ConfigMapping> MappingPair;
		struct ConfigData
		{
			std::vector<MappingPair> jsconf;
			device_list joysticks;
			device_list::const_iterator js_iter;
			GtkWidget* label;
			GtkListStore* store;
			GtkTreeView* treeview;
			ApiCallbacks* cb;
			int use_hidraw_ff_pt;
			const char* dev_type;
			u32 max_axes, max_buttons;
		};

		struct axis_correct
		{
			int used;
			int coef[3];
		};

		struct device_data
		{
			ConfigMapping cfg;
			std::string name;
			uint8_t axis_map[ABS_MAX + 1];
			uint16_t btn_map[KEY_MAX + 1];
			struct axis_correct abs_correct[ABS_MAX];
		};

		int GtkPadConfigure(int port, const char* dev_type, const char* title, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs);
		int GtkBuzzConfigure(int port, const char* dev_type, const char* title, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs);
		int GtkKeyboardmaniaConfigure(int port, const char* dev_type, const char* apititle, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs);
		bool LoadMappings(const char* dev_type, int port, const std::string& joyname, u32 max_buttons, u32 max_axes, ConfigMapping& cfg);
		bool SaveMappings(const char* dev_type, int port, const std::string& joyname, u32 max_buttons, u32 max_axes, const ConfigMapping& cfg);
		bool LoadBuzzMappings(const char* dev_type, int port, const std::string& joyname, ConfigMapping& cfg);
		bool SaveBuzzMappings(const char* dev_type, int port, const std::string& joyname, const ConfigMapping& cfg);
	} // namespace evdev
} // namespace usb_pad
