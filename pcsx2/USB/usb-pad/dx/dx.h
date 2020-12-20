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

#include <dinput.h>
#include <windows.h>
#include <stdint.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <sstream>

#include "USB/usb-pad/usb-pad.h"
#include "USB/configuration.h"
#include "USB/usb-pad/lg/lg_ff.h"

#define DINPUT_AXES_COUNT 32

namespace usb_pad
{
	namespace dx
	{
		//dinput control mappings

		static const DWORD PRECMULTI = 100; //floating point precision multiplier, 100 - two digit precision after comma

		extern int32_t GAINZ[2][1];
		extern int32_t FFMULTI[2][1];
		extern int32_t INVERTFORCES[2];

		static bool didDIinit = false; //we have a handle

		std::ostream& operator<<(std::ostream& os, REFGUID guid);

		enum ControlID
		{
			CID_STEERING,
			CID_STEERING_R,
			CID_THROTTLE,
			CID_BRAKE,
			CID_HATUP,
			CID_HATDOWN,
			CID_HATLEFT,
			CID_HATRIGHT,
			CID_SQUARE,
			CID_TRIANGLE,
			CID_CROSS,
			CID_CIRCLE,
			CID_L1,
			CID_R1,
			CID_L2,
			CID_R2,
			CID_L3,
			CID_R3,
			CID_SELECT,
			CID_START,
			CID_BUTTON20,
			CID_BUTTON21,
			CID_BUTTON22,
			CID_BUTTON23,
			CID_BUTTON24,
			CID_BUTTON25,
			CID_BUTTON26,
			CID_BUTTON27,
			CID_BUTTON28,
			CID_BUTTON29,
			CID_BUTTON30,
			CID_COUNT,
		};

		// Maybe merge with JoystickDevice
		class JoystickDeviceFF : public FFDevice
		{
		public:
			JoystickDeviceFF(int port)
				: m_port(port)
			{
			}
			~JoystickDeviceFF() {}

			void SetConstantForce(int level);
			void SetSpringForce(const parsed_ff_data& ff);
			void SetDamperForce(const parsed_ff_data& ff);
			void SetFrictionForce(const parsed_ff_data& ff);
			void SetAutoCenter(int value);
			void DisableForce(EffectID force);

		private:
			int m_port;
		};

		enum ControlType
		{
			CT_NONE,
			CT_KEYBOARD,
			CT_MOUSE,
			CT_JOYSTICK,
		};

		enum MappingType
		{
			MT_NONE = 0, //TODO leave for sanity checking?
			MT_AXIS,
			MT_BUTTON,
		};

		struct InputMapped
		{
			size_t index; //index into g_pJoysticks
			MappingType type = MT_NONE;
			int32_t mapped; //device axis/button
			bool INVERTED;
			int32_t HALF;
			int32_t LINEAR;
			int32_t OFFSET;
			int32_t DEADZONE;
		};

		class JoystickDevice
		{
		public:
			JoystickDevice(ControlType type, LPDIRECTINPUTDEVICE8 device, GUID guid, TSTDSTRING name)
				: m_type(type)
				, m_guid(guid)
				, m_device(device)
				, m_product(name)
			{
			}

			bool Poll();

			/*void GetDeviceState(size_t sz, void *ptr)
	{
		if (sz == sizeof(DIJOYSTATE2) && ptr)
			*ptr = m_jstate;
	}*/

			DIJOYSTATE2 GetDeviceState()
			{
				//assert(m_type == CT_JOYSTICK);
				return m_controls.js2;
			}

			HRESULT GetDeviceState(DWORD sz, LPVOID ptr)
			{
				return m_device->GetDeviceState(sz, ptr);
			}

			bool GetButton(int b);
			LONG GetAxis(int a);

			LPDIRECTINPUTDEVICE8 GetDevice()
			{
				return m_device;
			}

			GUID GetGUID()
			{
				return m_guid;
			}

			const TSTDSTRING& Product() const
			{
				return m_product;
			}

			ControlType GetControlType() { return m_type; }

			~JoystickDevice();

		private:
			GUID m_guid;
			TSTDSTRING m_product;
			LPDIRECTINPUTDEVICE8 m_device;
			ControlType m_type = CT_NONE;
			union
			{
				DIJOYSTATE2 js2;
				DIMOUSESTATE2 ms2;
				BYTE kbd[256];
			} m_controls = {};
		};

		extern std::vector<JoystickDevice*> g_pJoysticks;
		extern std::map<int, InputMapped> g_Controls[2];

		void ApplySettings(int port);
		void LoadDInputConfig(int port, const char* dev_type);
		void SaveDInputConfig(int port, const char* dev_type);

		void InitDI(int port, const char* dev_type);
		HRESULT InitDirectInput(HWND hWindow, int port);
		void FreeDirectInput();
		void PollDevices();
		float ReadAxis(const InputMapped& im);
		float ReadAxis(int port, int axisid);
		float FilterControl(float input, LONG linear, LONG offset, LONG dead);

		bool StartTestForce(int port);
		bool UpdateTestForce(int port, unsigned int stage);
		bool EndTestForce(int port);

		LONG GetAxisValueFromOffset(int axis, const DIJOYSTATE2& j);
		bool GetControl(int port, int id);
		float GetAxisControl(int port, ControlID id);
		int32_t GetAxisControlUnfiltered(int port, ControlID id);
		void CreateFFB(int port, LPDIRECTINPUTDEVICE8 device, DWORD axis);
		bool FindFFDevice(int port);
		bool UpdateFFBSettings(int port, LPDIRECTINPUTDEVICE8 device);

		void AddInputMap(int port, int cid, const InputMapped& im);
		void RemoveInputMap(int port, int cid);
		bool GetInputMap(int port, int cid, InputMapped& im);

	} // namespace dx
} // namespace usb_pad
