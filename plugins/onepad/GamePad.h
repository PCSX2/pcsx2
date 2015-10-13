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

#include "onepad.h"
#include "controller.h"
#ifdef  SDL_BUILD
#include <SDL.h>
#define HAT_UP SDL_HAT_UP
#define HAT_DOWN SDL_HAT_DOWN
#define HAT_RIGHT SDL_HAT_RIGHT
#define HAT_LEFT SDL_HAT_LEFT
#endif

// generic class for GamePad backends(e.g. SDL2 or, hopefully soon, evdev.
class GamePad
{
	public:

		/**
         * Search for connected GamePads and create corresponding classes for each of them
         * Need to be generic as it may either be sdl's joystick or evdev or whatever else
         **/
		static void EnumerateGamePads(vector<GamePad*>& vGamePad);
        static void UpdateReleaseState();




        /*************************************************************************/
        // Everything after this point is meant to be overiden by correct backend!
        /*************************************************************************/
       
        virtual void UpdateGamePadState()
        {
            return;// Need to be done by backend. At least for SDL --3kinox
        }
        /**
         * In first call, create effects and upload them to device
         * For every call:
         * According to effect type(int type), an effect is uploaded and ran on this GamePad
         **/
        virtual void DoHapticEffect(int type)
        {
            return; // Obviously does nothing here, no backend!    
        }

        virtual bool Init(int i)
        {
            return false;
        }

		GamePad()
        {// Leave blank for child classes to handle however they wish
	    vbuttonstate.clear();
        vaxisstate.clear();
        vhatstate.clear();
        devname = ""; // pretty device name
		_id = -1;
		numbuttons = numaxes = numhats = 0;
		deadzone =1500;
		pad= -1;	    
        }

		virtual ~GamePad()
		{
		}

		virtual void Destroy()
        {
            return;
        }

        /**
         * method to test whether pad is able to rumble or not
         **/
		virtual void TestForce()
        {
            return;
        }

		virtual bool PollButtons(u32 &pkey)
        {
            return false;
        }
		virtual bool PollAxes(u32 &pkey)
		{
            return false;
        }
        virtual bool PollHats(u32 &pkey)
 		{
            return false;
        }     

        virtual int GetHat(int key_to_axis)
        {
            return 0;
        }

        virtual int GetButton(int key_to_button)
        {
            return 0;
        }

		virtual const string& GetName()
		{
			return devname;
		}

		virtual int GetNumButtons()
		{
			return numbuttons;
		}

		virtual int GetNumAxes()
		{
			return numaxes;
		}

		virtual int GetNumHats()
		{
			return numhats;
		}

		virtual int GetPAD()
		{
			return pad;
		}

		virtual int GetDeadzone()
		{
			return deadzone;
		}

		virtual void SaveState()
        {
            return;
        }

		virtual int GetButtonState(int i)
		{
			return vbuttonstate[i];
		}

		virtual int GetAxisState(int i)
		{
			return vaxisstate[i];
		}

		virtual int GetHatState(int i)
		{
			//PAD_LOG("Getting POV State of %d.\n", i);
			return vhatstate[i];
		}

		virtual void SetButtonState(int i, int state)
		{
			vbuttonstate[i] = state;
		}

		virtual void SetAxisState(int i, int value)
		{
			vaxisstate[i] = value;
		}

		virtual void SetHatState(int i, int value)
		{
			//PAD_LOG("We should set %d to %d.\n", i, value);
			vhatstate[i] = value;
		}

		virtual int GetAxisFromKey(int pad, int index)
        {
            return -1;
        }

		

// protected should be enough, child classes should have access to this
	    protected:
        vector<int> vbuttonstate, vaxisstate, vhatstate;
        string devname; // pretty device name
		int _id;
		int numbuttons, numaxes, numhats;
		int deadzone;
		int pad;
};

extern vector<GamePad*> s_vjoysticks;
extern int s_selectedpad;
extern bool JoystickIdWithinBounds(int joyid);
