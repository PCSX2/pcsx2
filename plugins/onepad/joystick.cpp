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

#include "joystick.h"

//////////////////////////
// Joystick initions //
//////////////////////////

vector<JoystickInfo*> s_vjoysticks;
static u32 s_bSDLInit = false;

void JoystickInfo::UpdateReleaseState()
{
	vector<JoystickInfo*>::iterator itjoy = s_vjoysticks.begin();

	SDL_JoystickUpdate();

	// Save everything in the vector s_vjoysticks.
	while (itjoy != s_vjoysticks.end())
	{
		(*itjoy)->SaveState();
		itjoy++;
	}
}

bool JoystickIdWithinBounds(int joyid)
{
	return ((joyid >= 0) && (joyid < (int)s_vjoysticks.size()));
}

// opens handles to all possible joysticks
void JoystickInfo::EnumerateJoysticks(vector<JoystickInfo*>& vjoysticks)
{

	if (!s_bSDLInit)
	{
#if SDL_MAJOR_VERSION >= 2
		// Tell SDL to catch event even if the windows isn't focussed
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		// SDL in 3rdparty wrap X11 call. In order to get x11 symbols loaded
		// video must be loaded too.
		// Example of X11 symbol are XAutoRepeatOn/XAutoRepeatOff
		if (SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_VIDEO|SDL_INIT_HAPTIC|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS) < 0) return;
#else
		if (SDL_Init(SDL_INIT_JOYSTICK) < 0) return;
#endif
		SDL_JoystickEventState(SDL_QUERY);
		s_bSDLInit = true;
	}

	vector<JoystickInfo*>::iterator it = vjoysticks.begin();

	// Delete everything in the vector vjoysticks.
	while (it != vjoysticks.end())
	{
		delete *it;
		it ++;
	}

	vjoysticks.resize(SDL_NumJoysticks());

	for (int i = 0; i < (int)vjoysticks.size(); ++i)
	{
		vjoysticks[i] = new JoystickInfo();
		vjoysticks[i]->Init(i);
	}
}

void JoystickInfo::InitHapticEffect()
{

#if SDL_MAJOR_VERSION >= 2
	if (haptic == NULL) return;
    first = 1;
    Triangle_id = -1;
    Sine_id = -1;
#endif
}

void JoystickInfo::DoHapticEffect(int type)
{
	if (type > 1) return;
	//if ( !(conf->options & (PADOPTION_FORCEFEEDBACK << 16)) ) return;

#if SDL_MAJOR_VERSION >= 2
	if (haptic == NULL) return;

    if(first)
    {// If done multiple times, device memory will be filled
        first = 0;
        /** Effect for big motor **/
        Triangle_effect.type = SDL_HAPTIC_TRIANGLE;
	
        SDL_HapticDirection direction;
        direction.type = SDL_HAPTIC_POLAR; // We'll be using polar direction encoding.
        direction.dir[0] = 18000;
        Triangle_effect.periodic.direction = direction;
        Triangle_effect.periodic.period = 10;
	    Triangle_effect.periodic.magnitude = (Sint16)(0x7FFF); // Effect at maximum instensity
	    Triangle_effect.periodic.offset = 0;
	    Triangle_effect.periodic.phase = 18000;
	    Triangle_effect.periodic.length = 125; // 125ms feels quite near to original
	    Triangle_effect.periodic.delay = 0;
	    Triangle_effect.periodic.attack_length = 0;
    
        Triangle_id = SDL_HapticNewEffect(haptic, (&Triangle_effect));
        if(Triangle_id < 0)
        {
            fprintf(stderr,"ERROR: Effect is not uploaded! %s, id is %d\n",SDL_GetError(),Triangle_id);
        }
        /** Effect for small motor, more soft **/
        Sine_effect.type = SDL_HAPTIC_SINE;

        Sine_effect.periodic.direction = direction;
        Sine_effect.periodic.period = 10;
	    Sine_effect.periodic.magnitude = (Sint16)(0x4FFF); // slightly less powerful effect
	    Sine_effect.periodic.offset = 0;
	    Sine_effect.periodic.phase = 18000;
	    Sine_effect.periodic.length = 125;
	    Sine_effect.periodic.delay = 0;
	    Sine_effect.periodic.attack_length = 0;
    
        Sine_id = SDL_HapticNewEffect(haptic, &(Sine_effect));
        if(Sine_id < 0)
        {
            fprintf(stderr,"ERROR: Effect is not uploaded! %s, id is %d\n",SDL_GetError(),Sine_id);
        }

        if(SDL_HapticUpdateEffect(haptic, Triangle_id, &Triangle_effect) != 0)
        {
            fprintf(stderr,"ERROR: Effect is not updated! %s, id is %d\n",SDL_GetError(),Triangle_id);
        }
        if(SDL_HapticUpdateEffect(haptic, Sine_id, &Sine_effect) != 0)
        {
            fprintf(stderr,"ERROR: Effect is not updated! %s, id is %d\n",SDL_GetError(),Sine_id);
        }
    }
    
    int id;
    SDL_HapticEffect effect;
    if(type == 0)
    {
         id = Sine_id;
         effect = Sine_effect;
    }
    else
    {
         id = Triangle_id;
         effect = Triangle_effect;
    }
        if(SDL_HapticUpdateEffect(haptic, id, &effect) != 0)
        {
            fprintf(stderr,"ERROR: Effect is not updated! %s, id is %d\n",SDL_GetError(),id);
        }
        if(SDL_HapticRunEffect(haptic, id, 1) != 0)
        {
            fprintf(stderr,"ERROR: Effect is not working! %s, id is %d\n",SDL_GetError(),id);
        }
#endif
}



void JoystickInfo::Destroy()
{
	if (joy != NULL)
	{
#if SDL_MAJOR_VERSION >= 2
		// Haptic must be closed before the joystick
		if (haptic != NULL) {
			SDL_HapticClose(haptic);
			haptic = NULL;
            first = 1;
		}
#endif

#if SDL_MAJOR_VERSION >= 2
		//if (joy) SDL_JoystickClose(joy);
#else
		if (SDL_JoystickOpened(_id)) SDL_JoystickClose(joy);
#endif
		joy = NULL;
	}
}

bool JoystickInfo::Init(int id)
{
	Destroy();
	_id = id;

	joy = SDL_JoystickOpen(id);
	if (joy == NULL)
	{
		PAD_LOG("failed to open joystick %d\n", id);
		return false;
	}

	numaxes = SDL_JoystickNumAxes(joy);
	numbuttons = SDL_JoystickNumButtons(joy);
	numhats = SDL_JoystickNumHats(joy);
#if SDL_MAJOR_VERSION >= 2
	devname = SDL_JoystickName(joy);
#else
	devname = SDL_JoystickName(id);
#endif

	vaxisstate.resize(numaxes);
	vbuttonstate.resize(numbuttons);
	vhatstate.resize(numhats);

	// Sixaxis, dualshock3 hack
	// Most buttons are actually axes due to analog pressure support. Only the first 4 buttons
	// are digital (select, start, l3, r3). To avoid conflict just forget the others.
	// Keep the 4 hat buttons too (usb driver). (left pressure does not work with recent kernel). Moreover the pressure
	// work sometime on half axis neg others time in fulll axis. So better keep them as button for the moment
	u32 found_hack = devname.find(string("PLAYSTATION(R)3"));
	if (found_hack != string::npos && numaxes > 4) {
		numbuttons = 4; // (select, start, l3, r3)
		// Enable this hack in bluetooth too. It avoid to restart the onepad gui
		numbuttons += 4; // the 4 hat buttons
	}

#if SDL_MAJOR_VERSION >= 2
	if ( haptic == NULL ) {
		if (!SDL_JoystickIsHaptic(joy)) {
			PAD_LOG("Haptic devices not supported!\n");
		} else {
			haptic = SDL_HapticOpenFromJoystick(joy);
			// upload some default effect
            InitHapticEffect();
		}
	}
#endif

	//PAD_LOG("There are %d buttons, %d axises, and %d hats.\n", numbuttons, numaxes, numhats);
	return true;
}

void JoystickInfo::SaveState()
{
	for (int i = 0; i < numbuttons; ++i)
		SetButtonState(i, SDL_JoystickGetButton(joy, i));
	for (int i = 0; i < numaxes; ++i)
		SetAxisState(i, SDL_JoystickGetAxis(joy, i));
	for (int i = 0; i < numhats; ++i)
		SetHatState(i, SDL_JoystickGetHat(joy, i));
}

void JoystickInfo::TestForce()
{ // This code just use standard rumble to check that SDL handles the pad correctly! --3kinox

    SDL_HapticRumbleInit( haptic );
    // Make the haptic pad rumble full strength for half a second, shoudld be enough for user to see if it works or not
    if( SDL_HapticRumblePlay( haptic, 1, 500 ) != 0)
    {
        fprintf(stderr,"ERROR: Rumble is not working! %s\n",SDL_GetError());
    }
}

bool JoystickInfo::PollButtons(u32 &pkey)
{
	// MAKE sure to look for changes in the state!!
	for (int i = 0; i < GetNumButtons(); ++i)
	{
		int but = SDL_JoystickGetButton(GetJoy(), i);
		if (but != GetButtonState(i))
		{
			// Pressure sensitive button are detected as both button (digital) and axes (analog). So better
			// drop the button to emulate the pressure sensiblity of the ds2 :)
			// Trick: detect the release of the button. It avoid all races condition between axes and buttons :)
			// If the button support pressure it will be detected as an axis when it is pressed.
			if (but) {
				SetButtonState(i, but);
				return false;
			}


			pkey = button_to_key(i);
			return true;
		}
	}

	return false;
}

bool JoystickInfo::PollAxes(u32 &pkey)
{
	for (int i = 0; i < GetNumAxes(); ++i)
	{
		// Sixaxis, dualshock3 hack
		u32 found_hack = devname.find(string("PLAYSTATION(R)3"));
		if (found_hack != string::npos) {
			// The analog mode of the hat button is quite erratic. Values can be in half- axis
			// or full axis... So better keep them as button for the moment -- gregory
			if (i >= 8 && i <= 11 && (conf->options & PADOPTION_SIXAXIS_USB))
				continue;
			// Disable accelerometer
			if ((i >= 4 && i <= 6))
				continue;
		}

		s32 value = SDL_JoystickGetAxis(GetJoy(), i);
		s32 old_value = GetAxisState(i);

		if (abs(value - old_value) < 0x1000)
			continue;

		if (value != old_value)
		{
			PAD_LOG("Change in joystick %d: %d.\n", i, value);
			// There are several kinds of axes
			// Half+: 0 (release) -> 32768
			// Half-: 0 (release) -> -32768
			// Full (like dualshock 3): -32768 (release) ->32768
			const s32 full_axis_ceil = -0x6FFF;
			const s32 half_axis_ceil = 0x1FFF;

			// Normally, old_value contains the release state so it can be used to detect the types of axis.
			bool is_full_axis = (old_value < full_axis_ceil) ? true : false;

			if ((!is_full_axis && abs(value) <= half_axis_ceil)
					|| (is_full_axis && value <= full_axis_ceil))  // we don't want this
			{
				continue;
			}

			if ((!is_full_axis && abs(value) > half_axis_ceil)
					|| (is_full_axis && value > full_axis_ceil)) 
			{
				bool sign = (value < 0);
				pkey = axis_to_key(is_full_axis, sign, i);

				return true;
			}
		}
	}

	return false;
}

bool JoystickInfo::PollHats(u32 &pkey)
{
	for (int i = 0; i < GetNumHats(); ++i)
	{
		int value = SDL_JoystickGetHat(GetJoy(), i);

		if ((value != GetHatState(i)) && (value != SDL_HAT_CENTERED))
		{
			switch (value)
			{
				case SDL_HAT_UP:
				case SDL_HAT_RIGHT:
				case SDL_HAT_DOWN:
				case SDL_HAT_LEFT:
					pkey = hat_to_key(value, i);
					PAD_LOG("Hat Pressed!");
					return true;
				default:
					break;
			}
		}
	}
	return false;
}

int JoystickInfo::GetAxisFromKey(int pad, int index)
{
	return SDL_JoystickGetAxis(GetJoy(), key_to_axis(pad, index));
}
