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

// opens handles to all possible joysticks
void JoystickInfo::EnumerateJoysticks(std::vector<std::unique_ptr<GamePad>> &vjoysticks)
{
    if (!s_bSDLInit) {
        // Tell SDL to catch event even if the windows isn't focussed
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_EVENTS) < 0)
            return;

        // WTF! Give me back the control of my system
        struct sigaction action = {0};
        action.sa_handler = SIG_DFL;
        sigaction(SIGINT, &action, NULL);
        sigaction(SIGTERM, &action, NULL);

        SDL_JoystickEventState(SDL_QUERY);
        s_bSDLInit = true;
    }

    vjoysticks.clear();

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        vjoysticks.push_back(std::unique_ptr<GamePad>(new JoystickInfo(i)));
    }
}

void JoystickInfo::GenerateDefaultEffect()
{
    for (int i = 0; i < NB_EFFECT; i++) {
        SDL_HapticEffect effect;
        memset(&effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
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
}

void JoystickInfo::Rumble(int type, int pad)
{
    if (type > 1)
        return;
    if (!(conf->pad_options[pad].forcefeedback))
        return;

    if (haptic == NULL)
        return;

    if (first) { // If done multiple times, device memory will be filled
        first = 0;
        GenerateDefaultEffect();
        /* Sine and triangle are quite probably the best, don't change that lightly and if you do
         * keep effects ordered by type
         */
        /* Effect for small motor */
        /* Sine seems to be the only effect making little motor from DS3/4 react
         * Intensity has pretty much no effect either(which is coherent with what is explain in hid_sony driver
         */
        effects[0].type = SDL_HAPTIC_SINE;
        effects_id[0] = SDL_HapticNewEffect(haptic, &effects[0]);
        if (effects_id[0] < 0) {
            fprintf(stderr, "ERROR: Effect is not uploaded! %s, id is %d\n", SDL_GetError(), effects_id[0]);
        }

        /** Effect for big motor **/
        effects[1].type = SDL_HAPTIC_TRIANGLE;
        effects_id[1] = SDL_HapticNewEffect(haptic, &effects[1]);
        if (effects_id[1] < 0) {
            fprintf(stderr, "ERROR: Effect is not uploaded! %s, id is %d\n", SDL_GetError(), effects_id[1]);
        }
    }

    int id;
    id = effects_id[type];
    if (SDL_HapticRunEffect(haptic, id, 1) != 0) {
        fprintf(stderr, "ERROR: Effect is not working! %s, id is %d\n", SDL_GetError(), id);
    }
}

void JoystickInfo::Destroy()
{
    if (joy != NULL) {
        // Haptic must be closed before the joystick
        if (haptic != NULL) {
            SDL_HapticClose(haptic);
            haptic = NULL;
        }

#if SDL_MINOR_VERSION >= 4 // Version before 2.0.4 are bugged, JoystickClose crashes randomly
        if (joy)
            SDL_JoystickClose(joy);
#endif
        joy = NULL;
    }
}

bool JoystickInfo::Init(int id)
{
    Destroy();

    joy = SDL_JoystickOpen(id);
    if (joy == NULL) {
        PAD_LOG("failed to open joystick %d\n", id);
        return false;
    }

    devname = SDL_JoystickName(joy);

    if (haptic == NULL) {
        if (!SDL_JoystickIsHaptic(joy)) {
            PAD_LOG("Haptic devices not supported!\n");
        } else {
            haptic = SDL_HapticOpenFromJoystick(joy);
            first = true;
        }
    }

    //PAD_LOG("There are %d buttons, %d axises, and %d hats.\n", numbuttons, numaxes, numhats);
    return true;
}

bool JoystickInfo::TestForce(float strength = 0.60)
{
    // This code just use standard rumble to check that SDL handles the pad correctly! --3kinox
    if (haptic == NULL)
        return false; // Otherwise, core dump!
    SDL_HapticRumbleInit(haptic);
    // Make the haptic pad rumble 60% strength for half a second, shoudld be enough for user to see if it works or not
    if (SDL_HapticRumblePlay(haptic, strength, 400) != 0) {
        fprintf(stderr, "ERROR: Rumble is not working! %s\n", SDL_GetError());
        return false;
    }

    return true;
}
