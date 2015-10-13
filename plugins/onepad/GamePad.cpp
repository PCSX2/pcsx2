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
#ifdef  SDL_BUILD
#include "SDL/joystick.h"
#define HAT_UP SDL_HAT_UP
#define HAT_DOWN SDL_HAT_DOWN
#define HAT_RIGHT SDL_HAT_RIGHT
#define HAT_LEFT SDL_HAT_LEFT
#endif
vector<GamePad*> s_vjoysticks;
/**
 * Just forward the statics to the correct backend
 **/
void GamePad::EnumerateGamePads(vector<GamePad*>& vGamePad)
{
#ifdef  SDL_BUILD
    JoystickInfo::EnumerateJoysticks(vGamePad);
#else
#error YOU STILL NEED SDL for now, evdev coming soon hopefully
#endif
}


/**
 * Just forward the statics to the correct backend
 **/
void GamePad::UpdateReleaseState()
{
#ifdef  SDL_BUILD
    JoystickInfo::UpdateReleaseState();
#else
#error YOU STILL NEED SDL for now, evdev coming soon hopefully
#endif
}
