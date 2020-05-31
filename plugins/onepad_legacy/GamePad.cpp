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

#include "GamePad.h"
#ifdef SDL_BUILD
#include "SDL/joystick.h"
#endif

std::vector<GamePad *> s_vgamePad;
bool GamePadIdWithinBounds(int GamePadId)
{
    return ((GamePadId >= 0) && (GamePadId < (int)s_vgamePad.size()));
}

/**
 * Following static methods are just forwarders to their backend
 * This is where link between agnostic and specific code is done
 **/

/**
 * Find every interesting devices and create right structure for them(depend on backend)
 **/
void GamePad::EnumerateGamePads(std::vector<GamePad *> &vgamePad)
{
#ifdef SDL_BUILD
    JoystickInfo::EnumerateJoysticks(vgamePad);
#endif
}

void GamePad::UpdateReleaseState()
{
#ifdef SDL_BUILD
    JoystickInfo::UpdateReleaseState();
#endif
}

/**
 * Safely dispatch to the Rumble method above
 **/
void GamePad::DoRumble(int type, int pad)
{
    u32 id = conf->get_joyid(pad);
    if (GamePadIdWithinBounds(id))
    {
        GamePad *gamePad = s_vgamePad[id];
        if (gamePad) gamePad->Rumble(type, pad);
    }
}

/**
 * Update state of every attached devices
 **/
void GamePad::UpdateGamePadState()
{
#ifdef SDL_BUILD
    SDL_JoystickUpdate(); // No need to make yet another function call for that
#endif
}
