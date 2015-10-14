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

class GamePad
{
	public:
		GamePad() : devname(""), _id(-1), numbuttons(0), numaxes(0), numhats(0),
		 deadzone(1500), pad(-1) {
			 vbuttonstate.clear();
			 vaxisstate.clear();
			 vhatstate.clear();
		 }

		virtual ~GamePad()
		{
			return;
		}

		GamePad(const GamePad&);             // copy constructor
		GamePad& operator=(const GamePad&); // assignment

		/**
		 * Find every interesting devices and create right structure for them(depend on backend)
		 **/
		static void EnumerateGamePads(vector<GamePad*>& vgamePad);
		static void UpdateReleaseState();
		/**
		 * Update state of every attached devices
		 **/
		static void UpdateGamePadState();

		/** 
		 * Causes devices to rumble
		 * Rumble will differ according to type which is either 0(small motor) or 1(big motor)
		 **/
		virtual void Rumble(int type,int pad){return;}

		virtual bool Init(int id){return false;} // opens a handle and gets information

		/**
		 * Used for GUI checkbox to give feedback to the user
		 **/
		virtual void TestForce(){return;}

		virtual bool PollButtons(u32 &pkey){return false;}
		virtual bool PollAxes(u32 &pkey){return false;}
		virtual bool PollHats(u32 &pkey){return false;}

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

		virtual int GetDeadzone()
		{
			return deadzone;
		}

		virtual void SaveState(){return;}

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

		virtual int GetAxisFromKey(int pad, int index){return 0;}
// These fields need to be inherited by child classes
	protected:
		string devname; // pretty device name
		int _id;
		int numbuttons, numaxes, numhats;
		int deadzone;
		int pad;
		vector<int> vbuttonstate, vaxisstate, vhatstate;
};

extern vector<GamePad*> s_vgamePad;
extern bool GamePadIdWithinBounds(int joyid);
