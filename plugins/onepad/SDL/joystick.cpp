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
#include <signal.h> // sigaction

//////////////////////////
// Joystick definitions //
//////////////////////////

static u32 s_bSDLInit = false;

void JoystickInfo::UpdateReleaseState()
{
	vector<GamePad*>::iterator itjoy = s_vgamePad.begin();

	SDL_JoystickUpdate();

	// Save everything in the vector s_vjoysticks.
	while (itjoy != s_vgamePad.end())
	{
		(*itjoy)->SaveState();
		itjoy++;
	}
}

// opens handles to all possible joysticks
void JoystickInfo::EnumerateJoysticks(vector<GamePad*>& vjoysticks)
{

	if (!s_bSDLInit)
	{
#if SDL_MAJOR_VERSION >= 2
		// Tell SDL to catch event even if the windows isn't focussed
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		if (SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC|SDL_INIT_EVENTS) < 0) return;
		// WTF! Give me back the control of my system
		struct sigaction action = { 0 };
		action.sa_handler = SIG_DFL;
		sigaction(SIGINT, &action, NULL);
		sigaction(SIGTERM, &action, NULL);
#else
		if (SDL_Init(SDL_INIT_JOYSTICK) < 0) return;
#endif
		SDL_JoystickEventState(SDL_QUERY);
		s_bSDLInit = true;
	}

	vector<GamePad*>::iterator it = vjoysticks.begin();

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

void JoystickInfo::GenerateDefaultEffect()
{
#if SDL_MAJOR_VERSION >= 2
	for(int i=0;i<NB_EFFECT;i++)
	{
		SDL_HapticEffect effect;
		memset( &effect, 0, sizeof(SDL_HapticEffect) ); // 0 is safe default
		SDL_HapticDirection direction;
		direction.type = SDL_HAPTIC_POLAR; // We'll be using polar direction encoding.
		direction.dir[0] = 18000;
		effect.periodic.direction = direction;
		effect.periodic.period = 10;
		effect.periodic.magnitude = (Sint16)(conf->get_ff_intensity()); // Effect at maximum instensity
		effect.periodic.offset = 0;
		effect.periodic.phase = 18000;
		effect.periodic.length = 125; // 125ms feels quite near to original
		effect.periodic.delay = 0;
		effect.periodic.attack_length = 0;
		effects[i] = effect;
	}
#endif
}

void JoystickInfo::Rumble(int type, int pad)
{
	if (type > 1) return;
	if ( !(conf->pad_options[pad].forcefeedback) ) return;

#if SDL_MAJOR_VERSION >= 2
	if (haptic == NULL) return;

	if(first)
	{// If done multiple times, device memory will be filled
		first = 0;
		GenerateDefaultEffect();
		/** Sine and triangle are quite probably the best, don't change that lightly and if you do
		 *	keep effects ordered by type
		 **/
		/** Effect for small motor **/
		/** Sine seems to be the only effect making little motor from DS3/4 react
		 *	Intensity has pretty much no effect either(which is coherent with what is explain in hid_sony driver
		 **/
		effects[0].type = SDL_HAPTIC_SINE;
		effects_id[0] = SDL_HapticNewEffect(haptic, &effects[0]);
		if(effects_id[0] < 0)
		{
			fprintf(stderr,"ERROR: Effect is not uploaded! %s, id is %d\n",SDL_GetError(),effects_id[0]);
		}
		
		/** Effect for big motor **/
		effects[1].type = SDL_HAPTIC_TRIANGLE;
		effects_id[1] = SDL_HapticNewEffect(haptic, &effects[1]);
		if(effects_id[1] < 0)
		{
			fprintf(stderr,"ERROR: Effect is not uploaded! %s, id is %d\n",SDL_GetError(),effects_id[1]);
		}
	}

	int id;
	id = effects_id[type];
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
		}
#endif

#if SDL_MAJOR_VERSION >= 2
#if SDL_MINOR_VERSION >= 4 // Version before 2.0.4 are bugged, JoystickClose crashes randomly
		if (joy) SDL_JoystickClose(joy);
#endif
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
	// FIXME: people need to restart the plugin to take the option into account.
	bool hack_enabled = (conf->pad_options[0].sixaxis_pressure) || (conf->pad_options[1].sixaxis_pressure);
	if (found_hack != string::npos && numaxes > 4  && hack_enabled) {
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
			first = true;
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
{
#if SDL_MAJOR_VERSION >= 2
	// This code just use standard rumble to check that SDL handles the pad correctly! --3kinox
	if(haptic == NULL) return; // Otherwise, core dump!
	SDL_HapticRumbleInit( haptic );
    // Make the haptic pad rumble 60% strength for half a second, shoudld be enough for user to see if it works or not
	if( SDL_HapticRumblePlay( haptic, 0.60, 400 ) != 0)
	{
		fprintf(stderr,"ERROR: Rumble is not working! %s\n",SDL_GetError());
	}
#endif
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
	u32 found_hack = devname.find(string("PLAYSTATION(R)3"));

	for (int i = 0; i < GetNumAxes(); ++i)
	{
		// Sixaxis, dualshock3 hack
		if (found_hack != string::npos) {
			// The analog mode of the hat button is quite erratic. Values can be in half- axis
			// or full axis... So better keep them as button for the moment -- gregory
			if (i >= 8 && i <= 11 && (conf->pad_options[pad].sixaxis_usb))
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

int JoystickInfo::GetHat(int key_to_axis)
{
	return SDL_JoystickGetHat(GetJoy(),key_to_axis);
}

int JoystickInfo::GetButton(int key_to_button)
{
	return SDL_JoystickGetButton(GetJoy(),key_to_button);
}

int JoystickInfo::GetAxisFromKey(int pad, int index)
{
	return SDL_JoystickGetAxis(GetJoy(), key_to_axis(pad, index));
}
