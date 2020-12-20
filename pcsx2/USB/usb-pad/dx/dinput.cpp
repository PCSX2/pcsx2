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

#include "PrecompiledHeader.h"
#include <math.h>
#include <dinput.h>
#include "dx.h"

#define SAFE_DELETE(p)  \
	{                   \
		if (p)          \
		{               \
			delete (p); \
			(p) = NULL; \
		}               \
	}
#define SAFE_RELEASE(p)     \
	{                       \
		if (p)              \
		{                   \
			(p)->Release(); \
			(p) = NULL;     \
		}                   \
	}

//dialog window stuff
extern HWND gsWnd;

namespace usb_pad
{
	namespace dx
	{

		static std::atomic<int> refCount(0);
		static bool useRamp = false;

		DWORD pid = 0;
		DWORD old = 0;

		static LPDIRECTINPUT8 g_pDI = NULL;

		std::vector<JoystickDevice*> g_pJoysticks;
		std::map<int, InputMapped> g_Controls[2];

		static DWORD rgdwAxes[1] = {DIJOFS_X}; //FIXME if steering uses two axes, then this needs DIJOFS_Y too?
		static LONG rglDirection[1] = {0};

		//only two effect (constant force, spring)
		static bool FFB[2] = {false, false};
		static LPDIRECTINPUTEFFECT g_pEffectConstant[2] = {0, 0};
		static LPDIRECTINPUTEFFECT g_pEffectSpring[2] = {0, 0};
		static LPDIRECTINPUTEFFECT g_pEffectFriction[2] = {0, 0}; //DFP mode only
		static LPDIRECTINPUTEFFECT g_pEffectRamp[2] = {0, 0};
		static LPDIRECTINPUTEFFECT g_pEffectDamper[2] = {0, 0};
		static DWORD g_dwNumForceFeedbackAxis[4] = {0};
		static DIEFFECT eff;
		static DIEFFECT effSpring;
		static DIEFFECT effFriction;
		static DIEFFECT effRamp;
		static DIEFFECT effDamper;
		static DICONSTANTFORCE cfw;
		static DICONDITION cSpring;
		static DICONDITION cFriction;
		static DIRAMPFORCE cRamp;
		static DICONDITION cDamper;

		std::ostream& operator<<(std::ostream& os, REFGUID guid)
		{
			std::ios_base::fmtflags f(os.flags());
			os << std::uppercase;
			os.width(8);
			os << std::hex << guid.Data1 << '-';

			os.width(4);
			os << std::hex << guid.Data2 << '-';

			os.width(4);
			os << std::hex << guid.Data3 << '-';

			os.width(2);
			os << std::hex
			   << static_cast<short>(guid.Data4[0])
			   << static_cast<short>(guid.Data4[1])
			   << '-'
			   << static_cast<short>(guid.Data4[2])
			   << static_cast<short>(guid.Data4[3])
			   << static_cast<short>(guid.Data4[4])
			   << static_cast<short>(guid.Data4[5])
			   << static_cast<short>(guid.Data4[6])
			   << static_cast<short>(guid.Data4[7]);
			os.flags(f);
			return os;
		}

		LONG GetAxisValueFromOffset(int axis, const DIJOYSTATE2& j)
		{
			constexpr int LVX_OFFSET = 8; // count POVs or not?
			switch (axis)
			{
				case 0:
					return j.lX;
					break;
				case 1:
					return j.lY;
					break;
				case 2:
					return j.lZ;
					break;
				case 3:
					return j.lRx;
					break;
				case 4:
					return j.lRy;
					break;
				case 5:
					return j.lRz;
					break;
				case 6:
					return j.rglSlider[0];
					break;
				case 7:
					return j.rglSlider[1];
					break;
				//case 8: return j.rgdwPOV[0]; break;
				//case 9: return j.rgdwPOV[1]; break;
				//case 10: return j.rgdwPOV[2]; break;
				//case 11: return j.rgdwPOV[3]; break;
				case LVX_OFFSET + 0:
					return j.lVX;
					break; /* 'v' as in velocity */
				case LVX_OFFSET + 1:
					return j.lVY;
					break;
				case LVX_OFFSET + 2:
					return j.lVZ;
					break;
				case LVX_OFFSET + 3:
					return j.lVRx;
					break;
				case LVX_OFFSET + 4:
					return j.lVRy;
					break;
				case LVX_OFFSET + 5:
					return j.lVRz;
					break;
				case LVX_OFFSET + 6:
					return j.rglVSlider[0];
					break;
				case LVX_OFFSET + 7:
					return j.rglVSlider[1];
					break;
				case LVX_OFFSET + 8:
					return j.lAX;
					break; /* 'a' as in acceleration */
				case LVX_OFFSET + 9:
					return j.lAY;
					break;
				case LVX_OFFSET + 10:
					return j.lAZ;
					break;
				case LVX_OFFSET + 11:
					return j.lARx;
					break;
				case LVX_OFFSET + 12:
					return j.lARy;
					break;
				case LVX_OFFSET + 13:
					return j.lARz;
					break;
				case LVX_OFFSET + 14:
					return j.rglASlider[0];
					break;
				case LVX_OFFSET + 15:
					return j.rglASlider[1];
					break;
				case LVX_OFFSET + 16:
					return j.lFX;
					break; /* 'f' as in force */
				case LVX_OFFSET + 17:
					return j.lFY;
					break;
				case LVX_OFFSET + 18:
					return j.lFZ;
					break;
				case LVX_OFFSET + 19:
					return j.lFRx;
					break; /* 'fr' as in rotational force aka torque */
				case LVX_OFFSET + 20:
					return j.lFRy;
					break;
				case LVX_OFFSET + 21:
					return j.lFRz;
					break;
				case LVX_OFFSET + 22:
					return j.rglFSlider[0];
					break;
				case LVX_OFFSET + 23:
					return j.rglFSlider[1];
					break;
			}
			return 0;
		}

		bool JoystickDevice::Poll()
		{
			HRESULT hr = 0;
			if (m_device)
			{
				hr = m_device->Poll();
				if (FAILED(hr))
				{
					hr = m_device->Acquire();
					//return SUCCEEDED(hr);
				}
				else
				{
					if (m_type == CT_JOYSTICK)
					{
						m_device->GetDeviceState(sizeof(m_controls.js2), &m_controls.js2);
					}
					else if (m_type == CT_MOUSE)
					{
						m_device->GetDeviceState(sizeof(m_controls.ms2), &m_controls.ms2);
					}
					else if (m_type == CT_KEYBOARD)
					{
						m_device->GetDeviceState(sizeof(m_controls.kbd), &m_controls.kbd);
					}
					return true;
				}
			}
			return false;
		}

		bool JoystickDevice::GetButton(int b)
		{
			if (m_type == CT_JOYSTICK)
			{
				if (b < ARRAY_SIZE(DIJOYSTATE2::rgbButtons) && m_controls.js2.rgbButtons[b] & 0x80)
					return true;

				if (b >= ARRAY_SIZE(DIJOYSTATE2::rgbButtons))
				{
					b -= ARRAY_SIZE(DIJOYSTATE2::rgbButtons);
					int i = b / 4;
					//hat switch cases (4 hat switches with 4 directions possible)  surely a better way... but this would allow funky joysticks to work.
					switch (b % 4)
					{
						case 0:
							if ((m_controls.js2.rgdwPOV[i] <= 4500 || m_controls.js2.rgdwPOV[i] >= 31500) && m_controls.js2.rgdwPOV[i] != -1)
							{
								return true;
							}
							break;
						case 1:
							if (m_controls.js2.rgdwPOV[i] >= 4500 && m_controls.js2.rgdwPOV[i] <= 13500)
							{
								return true;
							}
							break;
						case 2:
							if (m_controls.js2.rgdwPOV[i] >= 13500 && m_controls.js2.rgdwPOV[i] <= 22500)
							{
								return true;
							}
							break;
						case 3:
							if (m_controls.js2.rgdwPOV[i] >= 22500 && m_controls.js2.rgdwPOV[i] <= 31500)
							{
								return true;
							}
							break;
					}
				}
			}
			else if (m_type == CT_KEYBOARD)
			{
				return (b < ARRAY_SIZE(m_controls.kbd) && m_controls.kbd[b] & 0x80);
			}
			else if (m_type == CT_MOUSE)
			{
				return (b < ARRAY_SIZE(DIMOUSESTATE2::rgbButtons) && m_controls.ms2.rgbButtons[b] & 0x80);
			}
			return false;
		}

		LONG JoystickDevice::GetAxis(int a)
		{
			return GetAxisValueFromOffset(a, m_controls.js2);
		}

		JoystickDevice::~JoystickDevice()
		{
			if (m_device)
			{
				m_device->Unacquire();
				m_device->Release();
			}
		}

		void ReleaseFFB(int port)
		{
			if (g_pEffectConstant[port])
				g_pEffectConstant[port]->Stop();
			if (g_pEffectSpring[port])
				g_pEffectSpring[port]->Stop();
			if (g_pEffectFriction[port])
				g_pEffectFriction[port]->Stop();
			if (g_pEffectRamp[port])
				g_pEffectRamp[port]->Stop();
			if (g_pEffectDamper[port])
				g_pEffectDamper[port]->Stop();

			SAFE_RELEASE(g_pEffectConstant[port]);
			SAFE_RELEASE(g_pEffectSpring[port]);
			SAFE_RELEASE(g_pEffectFriction[port]);
			SAFE_RELEASE(g_pEffectRamp[port]);
			SAFE_RELEASE(g_pEffectDamper[port]);

			FFB[port] = false;
		}

		void AddInputMap(int port, int cid, const InputMapped& im)
		{
			g_Controls[port][cid] = im;
		}

		void RemoveInputMap(int port, int cid)
		{
			g_Controls[port].erase(cid); //FIXME ini doesn't clear old entries duh
										 // override with MT_NONE instead
										 //g_Controls[port][cid].type = MT_NONE;
		}

		bool GetInputMap(int port, int cid, InputMapped& im)
		{
			auto it = g_Controls[port].find(cid);
			if (it != g_Controls[port].end())
			{
				im = it->second;
				return true;
			}
			return false;
		}

		void CreateFFB(int port, LPDIRECTINPUTDEVICE8 device, DWORD axis)
		{
			HRESULT hres;
			ReleaseFFB(port);

			if (!device)
				return;

			UpdateFFBSettings(port, device);

			rgdwAxes[0] = axis;
			//LPDIRECTINPUTDEVICE8 device = joy->GetDevice();
			//create the constant force effect
			ZeroMemory(&eff, sizeof(eff));
			ZeroMemory(&effSpring, sizeof(effSpring));
			ZeroMemory(&effFriction, sizeof(effFriction));
			ZeroMemory(&cfw, sizeof(cfw));
			ZeroMemory(&cSpring, sizeof(cSpring));
			ZeroMemory(&cFriction, sizeof(cFriction));
			ZeroMemory(&cRamp, sizeof(cRamp));

			//constantforce
			eff.dwSize = sizeof(eff);
			eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
			eff.dwSamplePeriod = 0;
			eff.dwGain = DI_FFNOMINALMAX;
			eff.dwTriggerButton = DIEB_NOTRIGGER;
			eff.dwTriggerRepeatInterval = 0;
			eff.cAxes = countof(rgdwAxes);
			eff.rgdwAxes = rgdwAxes; //TODO set actual "steering" axis though usually is DIJOFS_X
			eff.rglDirection = rglDirection;
			eff.dwStartDelay = 0;
			eff.dwDuration = INFINITE;

			// copy default values
			effSpring = eff;
			effFriction = eff;
			effRamp = eff;
			effDamper = eff;

			cfw.lMagnitude = 0;

			eff.cbTypeSpecificParams = sizeof(cfw);
			eff.lpvTypeSpecificParams = &cfw;
			hres = device->CreateEffect(GUID_ConstantForce, &eff, &g_pEffectConstant[port], NULL);

			cSpring.lNegativeCoefficient = 0;
			cSpring.lPositiveCoefficient = 0;

			effSpring.cbTypeSpecificParams = sizeof(cSpring);
			effSpring.lpvTypeSpecificParams = &cSpring;
			hres = device->CreateEffect(GUID_Spring, &effSpring, &g_pEffectSpring[port], NULL);

			effFriction.cbTypeSpecificParams = sizeof(cFriction);
			effFriction.lpvTypeSpecificParams = &cFriction;
			hres = device->CreateEffect(GUID_Friction, &effFriction, &g_pEffectFriction[port], NULL);

			effRamp.cbTypeSpecificParams = sizeof(cRamp);
			effRamp.lpvTypeSpecificParams = &cRamp;
			hres = device->CreateEffect(GUID_RampForce, &effRamp, &g_pEffectRamp[port], NULL);

			effDamper.cbTypeSpecificParams = sizeof(cDamper);
			effDamper.lpvTypeSpecificParams = &cDamper;
			hres = device->CreateEffect(GUID_Damper, &effDamper, &g_pEffectDamper[port], NULL);

			FFB[port] = true;

			//start the effect
			if (g_pEffectConstant[port])
			{
				g_pEffectConstant[port]->Start(1, 0);
			}
		}

		bool UpdateFFBSettings(int port, LPDIRECTINPUTDEVICE8 device)
		{
			DIPROPDWORD prop { sizeof(prop), sizeof(prop.diph) };
			prop.diph.dwObj = 0;
			prop.diph.dwHow = DIPH_DEVICE;
			prop.dwData = std::clamp(GAINZ[port][0], 0, DI_FFNOMINALMAX);
			return SUCCEEDED(device->SetProperty(DIPROP_FFGAIN, &prop.diph));
		}

		BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance,
											VOID* pContext)
		{
			HRESULT hr;

			// Obtain an interface to the enumerated joystick.
			LPDIRECTINPUTDEVICE8 joy = nullptr;
			hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &joy, NULL);
			if (SUCCEEDED(hr) && joy)
				g_pJoysticks.push_back(new JoystickDevice(CT_JOYSTICK, joy, pdidInstance->guidInstance, pdidInstance->tszProductName));

			return DIENUM_CONTINUE;
		}

		BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi,
										  VOID* pContext)
		{
			HRESULT hr;
			LPDIRECTINPUTDEVICE8 pWheel = (LPDIRECTINPUTDEVICE8)pContext;
			// For axes that are returned, set the DIPROP_RANGE property for the
			// enumerated axis in order to scale min/max values.
			if (pdidoi->dwType & DIDFT_AXIS)
			{
				DIPROPRANGE diprg { sizeof(diprg), sizeof(diprg.diph) };
				diprg.diph.dwHow = DIPH_BYID;
				diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
				diprg.lMin = 0;
				diprg.lMax = 65535;

				// Set the range for the axis  (not used, DX defaults 65535 all axis)
				if (FAILED(hr = pWheel->SetProperty(DIPROP_RANGE, &diprg.diph)))
					return DIENUM_STOP;

				//DIPROPDWORD dipdw;
				//dipdw.diph.dwSize = sizeof(DIPROPDWORD);
				//dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
				//dipdw.diph.dwHow = DIPH_BYID; //DIPH_DEVICE;
				//dipdw.diph.dwObj = pdidoi->dwType; //0;
				//dipdw.dwData = DIPROPAXISMODE_ABS;

				//if (FAILED(hr = pWheel->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
				//    return DIENUM_CONTINUE;

				//dipdw.dwData = 0;
				//if (FAILED(hr = pWheel->SetProperty(DIPROP_DEADZONE, &dipdw.diph)))
				//    return DIENUM_CONTINUE;

				//dipdw.dwData = DI_FFNOMINALMAX;
				//if (FAILED(hr = pWheel->SetProperty(DIPROP_SATURATION, &dipdw.diph)))
				//    return DIENUM_CONTINUE;
			}

			return DIENUM_CONTINUE;
		}

		//read all joystick states
		void PollDevices()
		{
			for (auto& joy : g_pJoysticks)
			{
				joy->Poll();
			}
		}

		//the non-linear filter (input/output 0.0-1.0 only) (parameters -50 to +50, expanded with PRECMULTI)
		float FilterControl(float input, LONG linear, LONG offset, LONG dead)
		{
			//ugly, but it works gooood

			//format+shorten variables
			float lf = float(linear) / PRECMULTI;
			float hs = 0;
			if (linear > 0)
				hs = 1.0f - ((lf * 2) * 0.01f);
			else
				hs = 1.0f - (abs(lf * 2) * 0.01f);

			float hs2 = (offset + 50 * PRECMULTI) / PRECMULTI * 0.01f;
			float v = input;
			float d = float(dead) / PRECMULTI * 0.005f;

			//format and apply deadzone
			v = (v * (1.0f + (d * 2.0f))) - d;

			//clamp
			if (v < 0.0f)
				v = 0.0f;
			if (v > 1.0f)
				v = 1.0f;

			//clamp negdead
			//if (v == -d) v = 0.0;
			if (fabs(v + d) < FLT_EPSILON)
				v = 0.0f;

			//possibilities
			float c1 = v - (1.0f - (pow((1.0f - v), (1.0f / hs))));
			float c2 = v - pow(v, hs);
			float c3 = float(v - (1.0 - (pow((1.0 - v), hs))));
			float c4 = ((v - pow(v, (1.0f / hs))));
			float res = 0;

			if (linear < 0)
			{
				res = v - (((1.0f - hs2) * c3) + (hs2 * c4)); //get negative result
			}
			else
			{
				res = v - (((1.0f - hs2) * c1) + (hs2 * c2)); //get positive result
			}

			//return our result
			return res;
		}

		float ReadAxis(const InputMapped& im)
		{
			assert(im.index < g_pJoysticks.size());
			if (im.index >= g_pJoysticks.size())
				return 0;

			LONG value = 0;
			if (im.type == MT_AXIS)
				value = g_pJoysticks[im.index]->GetAxis(im.mapped);
			else if (im.type == MT_BUTTON && g_pJoysticks[im.index]->GetButton(im.mapped)) // for the lulz
			{
				value = USHRT_MAX;
			}


			float retval = 0;

			if (im.HALF > 60000) // origin somewhere near top
			{
				retval = (65535 - value) * (1.0f / 65535);
			}
			else if (im.HALF > 26000 && im.HALF < 38000) // origin somewhere near center
			{
				if (im.INVERTED)
					retval = (value - 32767) * (1.0f / 32767);
				else
					retval = (32767 - value) * (1.0f / 32767);
			}
			else if (im.HALF >= 0 && im.HALF < 4000) // origin somewhere near bottom
			{
				retval = value * (1.0f / 65535);
			}

			if (retval < 0.0f)
				retval = 0.0f;

			return retval;
		}

		float ReadAxis(int port, int cid)
		{
			InputMapped im;
			if (!GetInputMap(port, cid, im))
				return 0;
			return ReadAxis(im);
		}

		//using both above functions
		float ReadAxisFiltered(int port, int cid)
		{
			InputMapped im;
			if (!GetInputMap(port, cid, im))
				return 0;
			return FilterControl(ReadAxis(im), im.LINEAR, im.OFFSET, im.DEADZONE);
		}

		void AutoCenter(LPDIRECTINPUTDEVICE8 device, bool onoff)
		{
			if (!device)
				return;
			//disable the auto-centering spring.
			DIPROPDWORD dipdw { sizeof(dipdw), sizeof(dipdw.diph) };
			dipdw.diph.dwObj = 0;
			dipdw.diph.dwHow = DIPH_DEVICE;
			dipdw.dwData = onoff ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF;

			device->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph);
		}

		void SetRamp(int port, const ramp& var)
		{
		}

		void SetRampVariable(int port, int forceids, const variable& var)
		{
			if (!FFB[port])
				return;

			// one main loop is 2ms, too erratic
			effRamp.dwDuration = 2000 * (var.t1 + 1) * 25;

			// Force0 only (Force2 is Y axis?)
			if (forceids & 1)
			{
				int force = var.l1;
				int dir = (var.d1 & 1 ? 1 : -1);

				if (INVERTFORCES[port])
				{
					cRamp.lStart = (127 - force) * DI_FFNOMINALMAX / 127;
					int sign = 1;
					if (cRamp.lStart < 0)
						sign = -1; // pull to force's direction?
					cRamp.lEnd = sign * DI_FFNOMINALMAX * dir;
				}
				else
				{
					cRamp.lStart = -(127 - force) * DI_FFNOMINALMAX / 127;
					//int sign = -1;
					//if (cRamp.lStart < 0) sign = 1; // pull to force's direction?
					//cRamp.lEnd = sign * DI_FFNOMINALMAX * dir; // or to center?
					cRamp.lEnd = -(127 - (force + /* var.t1 **/ var.s1 * dir)) * DI_FFNOMINALMAX / 127;
				}
			}

			if (g_pEffectRamp[port])
				g_pEffectRamp[port]->SetParameters(&effRamp,
												   DIEP_TYPESPECIFICPARAMS | DIEP_START | DIEP_DURATION);
		}

		void DisableRamp(int port)
		{
			if (g_pEffectRamp[port])
				g_pEffectRamp[port]->Stop();
		}

		// IDK
		void SetSpringSlopeForce(int port, const spring& spring)
		{
			cSpring.lOffset = 0;
			cSpring.lNegativeCoefficient = (spring.s1 & 1 ? -1 : 1) * spring.k1 * 10000 / 15;
			cSpring.lPositiveCoefficient = (spring.s2 & 1 ? -1 : 1) * spring.k2 * 10000 / 15;
			cSpring.dwNegativeSaturation = spring.dead1 * 10000 / 0xFF;
			cSpring.dwPositiveSaturation = spring.dead2 * 10000 / 0xFF;
			cSpring.lDeadBand = 0;

			if (g_pEffectSpring[port])
				g_pEffectSpring[port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
		}

		void JoystickDeviceFF::SetConstantForce(int level)
		{

			//FIXME either this or usb-pad-ff was inverted
			if (INVERTFORCES[m_port])
				cfw.lMagnitude = -level * DI_FFNOMINALMAX / SHRT_MAX;
			else
				cfw.lMagnitude = level * DI_FFNOMINALMAX / SHRT_MAX;

			if (FFMULTI[m_port][0] > 0)
				cfw.lMagnitude *= 1 + FFMULTI[m_port][0];

			if (g_pEffectConstant[m_port])
			{
				g_pEffectConstant[m_port]->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);

				//DWORD flags;
				//g_pEffectConstant->GetEffectStatus(&flags);

				//if(!(flags & DIEGES_PLAYING))
				//{
				//	InitDI();
				//}
			}
		}

		void JoystickDeviceFF::SetSpringForce(const parsed_ff_data& ff)
		{
			cSpring.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
			cSpring.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

			cSpring.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
			cSpring.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

			cSpring.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
			cSpring.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

			if (g_pEffectSpring[m_port])
				g_pEffectSpring[m_port]->SetParameters(&effSpring, DIEP_TYPESPECIFICPARAMS | DIEP_START);
		}

		void JoystickDeviceFF::SetDamperForce(const parsed_ff_data& ff)
		{
			cDamper.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
			cDamper.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

			cDamper.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
			cDamper.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

			cDamper.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
			cDamper.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

			if (g_pEffectDamper[m_port])
				g_pEffectDamper[m_port]->SetParameters(&effDamper, DIEP_TYPESPECIFICPARAMS | DIEP_START);
		}

		//LG driver converts it into high-precision damper instead, hmm
		void JoystickDeviceFF::SetFrictionForce(const parsed_ff_data& ff)
		{
			cFriction.dwNegativeSaturation = ff.u.condition.left_saturation * DI_FFNOMINALMAX / SHRT_MAX;
			cFriction.dwPositiveSaturation = ff.u.condition.right_saturation * DI_FFNOMINALMAX / SHRT_MAX;

			cFriction.lNegativeCoefficient = ff.u.condition.left_coeff * DI_FFNOMINALMAX / SHRT_MAX;
			cFriction.lPositiveCoefficient = ff.u.condition.right_coeff * DI_FFNOMINALMAX / SHRT_MAX;

			cFriction.lOffset = ff.u.condition.center * DI_FFNOMINALMAX / SHRT_MAX;
			cFriction.lDeadBand = ff.u.condition.deadband * DI_FFNOMINALMAX / USHRT_MAX;

			if (g_pEffectFriction[m_port])
				g_pEffectFriction[m_port]->SetParameters(&effFriction, DIEP_TYPESPECIFICPARAMS | DIEP_START);
		}

		void JoystickDeviceFF::DisableForce(EffectID force)
		{
			switch (force)
			{
				case EFF_CONSTANT:
					if (g_pEffectConstant[m_port])
						g_pEffectConstant[m_port]->Stop();
					break;
				case EFF_SPRING:
					if (g_pEffectSpring[m_port])
						g_pEffectSpring[m_port]->Stop();
					break;
				case EFF_DAMPER:
					if (g_pEffectDamper[m_port])
						g_pEffectDamper[m_port]->Stop();
					break;
				case EFF_FRICTION:
					if (g_pEffectFriction[m_port])
						g_pEffectFriction[m_port]->Stop();
					break;
				case EFF_RUMBLE:
					break;
			}
		}

		void JoystickDeviceFF::SetAutoCenter(int value)
		{
			InputMapped im;
			LPDIRECTINPUTDEVICE8 dev = nullptr;
			if (GetInputMap(m_port, CID_STEERING, im))
				dev = g_pJoysticks[im.index]->GetDevice();

			AutoCenter(dev, value > 0);
		}

		void FreeDirectInput()
		{
			if (!refCount || --refCount > 0)
				return;

			ReleaseFFB(0);
			ReleaseFFB(1);

			// Release any DirectInput objects.
			for (auto joy : g_pJoysticks)
				delete joy;
			g_pJoysticks.clear();

			SAFE_RELEASE(g_pDI);
			didDIinit = false;
		}

		//initialize all available devices
		HRESULT InitDirectInput(HWND hWindow, int port)
		{

			HRESULT hr;
			LPDIRECTINPUTDEVICE8 pKeyboard = NULL;
			LPDIRECTINPUTDEVICE8 pMouse = NULL;

			//release any previous resources

			if (refCount == 0)
			{
				// Create a DInput object
				if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
												   IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
					return hr;

				//Create Keyboard
				g_pDI->CreateDevice(GUID_SysKeyboard, &pKeyboard, NULL);
				if (pKeyboard)
				{
					pKeyboard->SetDataFormat(&c_dfDIKeyboard);
					pKeyboard->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
					pKeyboard->Acquire();
					g_pJoysticks.push_back(new JoystickDevice(CT_KEYBOARD, pKeyboard, GUID_SysKeyboard, TEXT("SysKeyboard")));
				}

				//Create Mouse
				g_pDI->CreateDevice(GUID_SysMouse, &pMouse, NULL);
				if (pMouse)
				{
					pMouse->SetDataFormat(&c_dfDIMouse2);
					pMouse->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
					pMouse->Acquire();
					g_pJoysticks.push_back(new JoystickDevice(CT_MOUSE, pMouse, GUID_SysMouse, TEXT("SysMouse")));
				}

				//enumerate attached only
				g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);

				//loop through all attached joysticks
				for (size_t i = 0; i < g_pJoysticks.size(); i++)
				{
					auto joy = g_pJoysticks[i];
					auto device = joy->GetDevice();
					device->SetDataFormat(&c_dfDIJoystick2);

					DIDEVCAPS diCaps { sizeof(diCaps) };
					device->GetCapabilities(&diCaps);

					if (diCaps.dwFlags & DIDC_FORCEFEEDBACK)
					{
						//Exclusive
						device->SetCooperativeLevel(hWindow, DISCL_EXCLUSIVE | DISCL_BACKGROUND);

						/*DIDEVICEINSTANCE instance_;
				ZeroMemory(&instance_, sizeof(DIDEVICEINSTANCE));
				instance_.dwSize = sizeof(DIDEVICEINSTANCE);
				g_pJoysticks[i]->GetDeviceInfo(&instance_);
				std::stringstream str;
				str << instance_.guidInstance;*/
					}
					else
						device->SetCooperativeLevel(hWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

					device->EnumObjects(EnumObjectsCallback, device, DIDFT_ALL);
					device->Acquire();
				}
			}

			++refCount;

			didDIinit = true;
			return S_OK;
		}

		HWND GetWindowHandle(DWORD tPID)
		{
			//Get first window handle
			HWND res = FindWindow(NULL, NULL);
			DWORD mPID = 0;
			while (res != 0)
			{
				if (!GetParent(res))
				{
					GetWindowThreadProcessId(res, &mPID);
					if (mPID == tPID)
						return res;
				}
				res = GetWindow(res, GW_HWNDNEXT);
			}
			return NULL;
		}

		bool FindFFDevice(int port)
		{
			InputMapped im;
			if (!GetInputMap(port, CID_STEERING, im))
				return false;

			auto device = g_pJoysticks[im.index]->GetDevice();
			DIDEVCAPS diCaps { sizeof(diCaps) };
			device->GetCapabilities(&diCaps);

			//has ffb?
			if (!FFB[port] && (diCaps.dwFlags & DIDC_FORCEFEEDBACK))
			{

				//FIXME im.mapped is offset to GetAxisValueFromOffset, compatibility with DIEFFECT::rgdwAxes is questionable after DIJOYSTATE2::rglSlider
				CreateFFB(port, device, im.mapped);

				AutoCenter(device, false); //TODO some games set autocenter. Figure out default for ones that don't.

				/*DIDEVICEINSTANCE instance_;
		ZeroMemory(&instance_, sizeof(DIDEVICEINSTANCE));
		instance_.dwSize = sizeof(DIDEVICEINSTANCE);
		g_pJoysticks[i]->GetDeviceInfo(&instance_);
		std::stringstream str;
		str << instance_.guidInstance;*/
			}
			return FFB[port];
		}

		//use direct input
		void InitDI(int port, const char* dev_type)
		{
			HWND hWin = nullptr;

			if (gsWnd)
			{
				hWin = gsWnd;
			}
			else
			{
				pid = GetCurrentProcessId();
				while (hWin == 0)
				{
					hWin = GetWindowHandle(pid);
				}
			}

			// DirectInput needs a top-level window
			InitDirectInput(GetAncestor(hWin, GA_ROOT), port);
			LoadDInputConfig(port, dev_type);
			FindFFDevice(port);
		}

		bool GetControl(int port, int id)
		{
			InputMapped im;
			if (!GetInputMap(port, id, im))
				return false;

			assert(im.index < g_pJoysticks.size());
			if (im.index >= g_pJoysticks.size())
				return false;

			auto joy = g_pJoysticks[im.index];

			if (im.type == MT_AXIS)
			{
				return ReadAxisFiltered(port, id) >= 0.5f;
			}
			else if (im.type == MT_BUTTON)
			{
				return joy->GetButton(im.mapped);
			}
			return false;
		}

		float GetAxisControl(int port, ControlID id)
		{
			if (id == CID_STEERING)
			{
				//apply steering, single axis is split to two for filtering
				if (ReadAxisFiltered(port, CID_STEERING) > 0.0)
				{
					return -ReadAxisFiltered(port, CID_STEERING);
				}
				else
				{
					if (ReadAxisFiltered(port, CID_STEERING_R) > 0.0)
					{
						return ReadAxisFiltered(port, CID_STEERING_R);
					}
					else
					{
						return 0;
					}
				}
			}

			return ReadAxisFiltered(port, id);
		}

		int32_t GetAxisControlUnfiltered(int port, ControlID id)
		{
			InputMapped im;
			if (!GetInputMap(port, id, im))
				return 0;

			assert(im.index < g_pJoysticks.size());
			if (im.index >= g_pJoysticks.size())
				return 0;

			LONG value = 0;
			if (im.type == MT_AXIS)
			{
				value = g_pJoysticks[im.index]->GetAxis(im.mapped);
			}
			return value;
		}

		//set left/right ffb torque
		HRESULT SetConstantForce(int port, LONG magnitude)
		{
			if (!FFB[port] || !g_pEffectConstant[port])
				return DIERR_NOTINITIALIZED;

			if (INVERTFORCES[port])
				cfw.lMagnitude = -magnitude;
			else
				cfw.lMagnitude = magnitude;

			if (FFMULTI[port][0] > 0)
				cfw.lMagnitude *= 1 + FFMULTI[port][0];

			return g_pEffectConstant[port]->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
		}

		bool StartTestForce(int port)
		{
			InputMapped im;
			LPDIRECTINPUTDEVICE8 dev = nullptr;
			if (GetInputMap(port, CID_STEERING, im))
				dev = g_pJoysticks[im.index]->GetDevice();

			// Gain value may have changed, so update it for the constant force effect
			return UpdateFFBSettings(port, dev);
		}

		bool UpdateTestForce(int port, unsigned int stage)
		{
			// FFB test ticks every 500ms and goes as follows:
			// Turn right, wait 500ms, turn left, wait 1000ms, turn right, wait 500ms, end
			if (stage == 0)
			{
				return SUCCEEDED(SetConstantForce(port, DI_FFNOMINALMAX / 3));
			}
			if (stage == 1)
			{
				return SUCCEEDED(SetConstantForce(port, -DI_FFNOMINALMAX / 3));
			}
			if (stage == 2)
			{
				// Do nothing, as we wait 1000ms
				return true;
			}
			if (stage == 3)
			{
				return SUCCEEDED(SetConstantForce(port, DI_FFNOMINALMAX / 3));
			}
			return false;
		}

		bool EndTestForce(int port)
		{
			return SUCCEEDED(SetConstantForce(port, 0));
		}

	} // namespace dx
} // namespace usb_pad
