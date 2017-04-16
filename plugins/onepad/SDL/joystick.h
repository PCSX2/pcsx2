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
class JoystickInfo : GamePad
{
public:
    JoystickInfo(int id)
        : GamePad()
        , m_controller(nullptr)
    {
        haptic = nullptr;
        first = true;
        memset(effects, 0, sizeof(effects));
        memset(effects_id, 0, sizeof(effects_id));
        Init(id);
    }

    ~JoystickInfo()
    {
        Destroy();
    }

    JoystickInfo(const JoystickInfo &) = delete;            // copy constructor
    JoystickInfo &operator=(const JoystickInfo &) = delete; // assignment


    void Destroy();
    // opens handles to all possible joysticks
    static void EnumerateJoysticks(std::vector<std::unique_ptr<GamePad>> &vjoysticks);

    void Rumble(int type, int pad);

    bool Init(int id); // opens a handle and gets information

    bool TestForce(float);

    virtual const char *GetName();

    virtual int GetInput(gamePadValues input);

    virtual void UpdateGamePadState();

private:
    void GenerateDefaultEffect();

    SDL_GameController *m_controller;
    SDL_Haptic *haptic;
    bool first;
    SDL_HapticEffect effects[NB_EFFECT];
    int effects_id[NB_EFFECT];
};
