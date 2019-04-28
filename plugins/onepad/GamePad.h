/*  
 *  PCSX2 Dev Team
 *  Copyright (C) 2015
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

#ifdef SDL_BUILD
#include <SDL.h>
#endif

class GamePad
{
public:
    GamePad()
        : m_deadzone(1500)
        , m_no_error(false)
    {
    }

    virtual ~GamePad()
    {
    }

    GamePad(const GamePad &);            // copy constructor
    GamePad &operator=(const GamePad &); // assignment

    /*
     * Find every interesting devices and create right structure for them(depend on backend)
     */
    static void EnumerateGamePads(std::vector<std::unique_ptr<GamePad>> &vgamePad);

    /*
     * Update state of every attached devices
     */
    virtual void UpdateGamePadState() = 0;

    /*
     * Causes devices to rumble
     * Rumble will differ according to type which is either 0(small motor) or 1(big motor)
     */
    virtual void Rumble(unsigned type, unsigned pad) {}
    /*
     * Safely dispatch to the Rumble method above
     */
    static void DoRumble(unsigned type, unsigned pad);

    /*
     * Used for GUI checkbox to give feedback to the user
     */
    virtual bool TestForce(float strength = 0.6) { return false; }

    virtual const char *GetName() = 0;

    virtual int GetInput(gamePadValues input) = 0;

    int GetDeadzone()
    {
        return m_deadzone;
    }

    virtual size_t GetUniqueIdentifier() = 0;

    static size_t index_to_uid(int index);
    static int uid_to_index(int pad);

    bool IsProperlyInitialized()
    {
        return m_no_error;
    }

protected:
    int m_deadzone;
    bool m_no_error;
};

extern std::vector<std::unique_ptr<GamePad>> s_vgamePad;
