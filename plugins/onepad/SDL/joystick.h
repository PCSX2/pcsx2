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
#include <SDL_haptic.h>

#include "GamePad.h"
#include "onepad.h"
#include "controller.h"
#define NB_EFFECT 2 // Don't use more than two, ps2 only has one for big motor and one for small(like most systems)
// holds all joystick info
class JoystickInfo : public GamePad
{
public:
    JoystickInfo(int id);
    ~JoystickInfo();

    JoystickInfo(const JoystickInfo &) = delete;            // copy constructor
    JoystickInfo &operator=(const JoystickInfo &) = delete; // assignment


    // opens handles to all possible joysticks
    static void EnumerateJoysticks(std::vector<std::unique_ptr<GamePad>> &vjoysticks);

    void Rumble(unsigned type, unsigned pad) override;

    bool TestForce(float) override;

    const char *GetName() final;

    int GetInput(gamePadValues input) final;

    void UpdateGamePadState() final;

    size_t GetUniqueIdentifier() final;

private:
    SDL_GameController *m_controller;
    SDL_Haptic *m_haptic;
    std::array<int, NB_EFFECT> m_effects_id;
    size_t m_unique_id;
    std::array<int, MAX_KEYS> m_pad_to_sdl;
};
