/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "DiscordSDK.h" // Our template for this cpp

#include "discord.h" // Discord's own header for the SDK

#include "App.h"
#include <ctime>

#ifdef WIN32

discord::ClientId CLIENT_ID = 739176831936233566;
discord::Core* core{};

void CallbackDiscordSDK()
{
    if (core == nullptr || g_Conf->EmuOptions.DiscordSDK == false)
        return;

    ::core->RunCallbacks();
}

void UpdateDiscordSDK(std::string state, std::string details)
{
    if (core == nullptr || g_Conf->EmuOptions.DiscordSDK == false)
        return;

    discord::Activity activity{};
    activity.SetState(state.c_str());
    activity.SetDetails(details.c_str());
    activity.GetAssets().SetLargeImage("pcsx2");
    discord::Timestamp start;
    time(&start);
    activity.GetTimestamps().SetStart(start);

    core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
        {
            if (static_cast<int>(result))
                DevCon.WriteLn("[DiscordSDK] UpdateActivity failed with result %d", result);
        }
    );
}

void InitDiscordSDK() 
{
    if (g_Conf->EmuOptions.DiscordSDK == false)
        return;

    discord::Core::Create(CLIENT_ID, DiscordCreateFlags_Default, &core);
}

#endif
