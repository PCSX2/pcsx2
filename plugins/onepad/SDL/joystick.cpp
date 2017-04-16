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
    uint32_t flag = SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;

    if ((SDL_WasInit(0) & flag) != flag) {
        // Tell SDL to catch event even if the windows isn't focussed
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

        if (SDL_Init(flag) < 0)
            return;

        // WTF! Give me back the control of my system
        struct sigaction action = {0};
        action.sa_handler = SIG_DFL;
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);

        SDL_JoystickEventState(SDL_QUERY);
        SDL_GameControllerEventState(SDL_QUERY);
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

    if (haptic == nullptr)
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
    // Haptic must be closed before the joystick
    if (haptic != nullptr) {
        SDL_HapticClose(haptic);
        haptic = nullptr;
    }

    if (m_controller != nullptr) {
#if SDL_MINOR_VERSION >= 4
        // Version before 2.0.4 are bugged, JoystickClose crashes randomly
        // Note: GameControllerClose calls JoystickClose)
        SDL_GameControllerClose(m_controller);
#endif
        m_controller = nullptr;
    }
}

bool JoystickInfo::Init(int id)
{
    Destroy();

    SDL_Joystick *joy = nullptr;

    if (SDL_IsGameController(id)) {
        m_controller = SDL_GameControllerOpen(id);
        joy = SDL_GameControllerGetJoystick(m_controller);
    } else {
        m_controller = nullptr;
        joy = SDL_JoystickOpen(id);
    }

    if (joy == nullptr) {
        fprintf(stderr, "onepad:failed to open joystick %d\n", id);
        return false;
    }

    // Collect Device Information
    char guid[64];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 64);
    const char *devname = SDL_JoystickNameForIndex(id);

    if (m_controller == nullptr) {
        fprintf(stderr, "onepad: Joystick (%s,GUID:%s) isn't yet supported by the SDL2 game controller API\n"
                        "Fortunately you can use AntiMicro (https://github.com/AntiMicro/antimicro) or Steam to configure your joystick\n"
                        "Please report it to us (https://github.com/PCSX2/pcsx2/issues) so we can add your joystick to our internal database.",
                devname, guid);
        return false;
    }

    if (haptic == nullptr && SDL_JoystickIsHaptic(joy)) {
        haptic = SDL_HapticOpenFromJoystick(joy);
        first = true;
    }

    fprintf(stdout, "onepad: controller (%s) detected%s, GUID:%s\n",
            devname, haptic ? " with rumble support" : "", guid);

    return true;
}

const char *JoystickInfo::GetName()
{
    return SDL_JoystickName(SDL_GameControllerGetJoystick(m_controller));
}

bool JoystickInfo::TestForce(float strength = 0.60)
{
    // This code just use standard rumble to check that SDL handles the pad correctly! --3kinox
    if (haptic == nullptr)
        return false; // Otherwise, core dump!
    SDL_HapticRumbleInit(haptic);
    // Make the haptic pad rumble 60% strength for half a second, shoudld be enough for user to see if it works or not
    if (SDL_HapticRumblePlay(haptic, strength, 400) != 0) {
        fprintf(stderr, "ERROR: Rumble is not working! %s\n", SDL_GetError());
        return false;
    }

    return true;
}

int JoystickInfo::GetInput(gamePadValues input)
{
    int value = 0;

    // Handle standard button
    switch (input) {
        case PAD_L1:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            break;
        case PAD_R1:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            break;
        case PAD_TRIANGLE:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_Y);
            break;
        case PAD_CIRCLE:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_B);
            break;
        case PAD_CROSS:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_A);
            break;
        case PAD_SQUARE:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_X);
            break;
        case PAD_SELECT:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_BACK);
            break;
        case PAD_L3:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_LEFTSTICK);
            break;
        case PAD_R3:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
            break;
        case PAD_START:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_START);
            break;
        case PAD_UP:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
            break;
        case PAD_RIGHT:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            break;
        case PAD_DOWN:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            break;
        case PAD_LEFT:
            value = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            break;
        default:
            break;
    }

    // Return max pressure for button
    if (value)
        return 0xFF;

    // Handle trigger
    switch (input) {
        case PAD_L2:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            break;
        case PAD_R2:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            break;
        default:
            break;
    }

    // Note SDL values range from 0 to 32768 for trigger
    if (value > m_deadzone) {
        return value / 128;
    } else {
        value = 0;
    }

    // Handle analog input
    switch (input) {
        case PAD_L_UP:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
            break;
        case PAD_L_RIGHT:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
            break;
        case PAD_L_DOWN:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
            break;
        case PAD_L_LEFT:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
            break;
        case PAD_R_UP:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTY);
            break;
        case PAD_R_RIGHT:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTX);
            break;
        case PAD_R_DOWN:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTY);
            break;
        case PAD_R_LEFT:
            value = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTX);
            break;
        default:
            break;
    }

    return (abs(value) > m_deadzone) ? value : 0;
}

void JoystickInfo::UpdateGamePadState()
{
    SDL_GameControllerUpdate();
}
