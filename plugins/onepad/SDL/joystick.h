/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include <SDL.h>
#if SDL_MAJOR_VERSION >= 2
#include <SDL_haptic.h>
#endif

#include "GamePad.h"
#include "onepad.h"
#include "controller.h"
#define NB_EFFECT 2 // Don't use more than two, ps2 only has one for big motor and one for small(like most systems)
// holds all joystick info
class JoystickInfo : GamePad
{
	public:
		JoystickInfo() : devname(""), _id(-1), numbuttons(0), numaxes(0), numhats(0),
		 deadzone(1500), pad(-1), joy(NULL) {
			vbuttonstate.clear();
			vaxisstate.clear();
			vhatstate.clear();
#if SDL_MAJOR_VERSION >= 2
			haptic = NULL;
			first = true;
#endif
		 }

		~JoystickInfo()
		{
			Destroy();
		}

		JoystickInfo(const JoystickInfo&);             // copy constructor
		JoystickInfo& operator=(const JoystickInfo&); // assignment

		void Destroy();
		// opens handles to all possible joysticks
		static void EnumerateJoysticks(vector<GamePad*>& vjoysticks);

		void Rumble(int type,int pad);

		bool Init(int id); // opens a handle and gets information

		bool TestForce(float);

		bool PollButtons(u32 &pkey);
		bool PollAxes(u32 &pkey);
		bool PollHats(u32 &pkey);

		int GetHat(int key_to_axis);

		int GetButton(int key_to_button);

		const string& GetName()
		{
			return devname;
		}

		int GetNumButtons()
		{
			return numbuttons;
		}

		int GetNumAxes()
		{
			return numaxes;
		}

		int GetNumHats()
		{
			return numhats;
		}

		int GetPAD()
		{
			return pad;
		}

		int GetDeadzone()
		{
			return deadzone;
		}

		void SaveState();

		int GetButtonState(int i)
		{
			return vbuttonstate[i];
		}

		int GetAxisState(int i)
		{
			return vaxisstate[i];
		}

		int GetHatState(int i)
		{
			//PAD_LOG("Getting POV State of %d.\n", i);
			return vhatstate[i];
		}

		void SetButtonState(int i, int state)
		{
			vbuttonstate[i] = state;
		}

		void SetAxisState(int i, int value)
		{
			vaxisstate[i] = value;
		}

		void SetHatState(int i, int value)
		{
			//PAD_LOG("We should set %d to %d.\n", i, value);
			vhatstate[i] = value;
		}

		int GetAxisFromKey(int pad, int index);

		static void UpdateReleaseState();

	private:
		SDL_Joystick* GetJoy()
		{
			return joy;
		}
		void GenerateDefaultEffect();
		string devname; // pretty device name
		int _id;
		int numbuttons, numaxes, numhats;
		int deadzone;
		int pad;

		vector<int> vbuttonstate, vaxisstate, vhatstate;

		SDL_Joystick*		joy;
#if SDL_MAJOR_VERSION >= 2
		SDL_Haptic*   		haptic;
		bool first;
		SDL_HapticEffect effects[NB_EFFECT];
		int effects_id[NB_EFFECT];
#endif
};
