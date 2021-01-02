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

#include "App.h" // Access to g_Conf
#include <ctime> // time(time_t) function

// Discord SDK Implementation!
// 
// Discord SDK is callback based. Functions such as
// discord::ActivityManager::UpdateActivity are non-blocking and their payloads
// will run asynchronous to the threads they were called from. Discord wasn't
// lying when they said the SDK was not thread safe.
// 
// As such, these asynchronous functions do take callbacks to run after
// completion. The discord::Core::RunCallbacks function will execute any
// callbacks of asynchronous functions which have completed. Because obviously
// there is no guarantee at all with timing on this, discord::Core::RunCallbacks
// needs to be executed repeatedly in a loop. Currently we do this in
// SysCoreThread::VsyncInThread, which works beautifully, as long as the
// emulation threads are running and we are processing vsyncs.
// 
// The downfall of this is our UI thread is not constantly awake. It is an event
// listener; a giant idle loop with a bunch of event handlers bolted on to it.
// We certainly could plaster "CallbackDiscordSDK" all over every last event
// handler that works in the UI thread to. Or, we can just accept that for now
// the implementation will be unresponsive to any kind of "In the Menus"
// scenario and just reflect BIOS and ingame status. Hopefully with a new UI and
// removal of wx we may have some better options, time will tell.
// 
// Some side notes to the brave soul who finds this file in the future
// 
// The whole "are we in BIOS or ingame" thing is kind of interesting, it doesn't
// seem like that information really persists. We know the current serial if a
// game is loaded, or none if BIOS is running, that's about it. The title and
// all is loaded from GameDB on the fly as a bunch of locals, thrown into the
// console title bar (using an _ApplySettings function? Interesting place but
// okay) and then that's all, no persistence for referencing later. This too
// makes it a bit tricky to do "In the Menus" type updates on the fly because it
// basically means every pause and resume is going to be another GameDB lookup
// with the serial again. The GameDB in memory appears to be a map, so it should
// be relatively cheap to perform, but it just doesn't really feel right when it
// could be cached and a more accurate, "GameChanged" event perhaps could be a
// proper place to do a singular GameDB lookup.
// 
// Then there are PS1 games (yeah yeah we don't officially support them yet blah
// blah) which don't exist in GameDB so that lookup would fail, but extra extra
// problem is they don't even invoke 90% of the _ApplySettings function
// mentioned above. Until either they are added to GameDB or the title bar is
// set in one unified location, PS1 games will simply set off the PS2 BIOS
// condition, then never update Discord status again. The console title is
// updated instead by cdvdReloadElfInfo in CDVD.cpp, with the serial number
// simply being thrown in on its own.

#ifdef WIN32

// Honestly, if you thought of looking for this, congratulations.
// Here's the app ID. Don't be a dick, but have fun. :P
discord::ClientId CLIENT_ID = 739176831936233566;

// Main "Core" instance of the SDK. Everything else stems from here.
discord::Core* core{};

// Execute any pending callbacks in the Discord SDK. After an asynchronous
// function in the SDK (e.g. Core::ActivityManager::UpdateActivity) completes,
// its callback will be enqueued but not executed. This function calls
// Core::RunCallbacks, which will execute any enqueued callbacks. 
void CallbackDiscordSDK()
{
    if (core == nullptr || g_Conf->EmuOptions.DiscordSDK == false)
        return;

    ::core->RunCallbacks();
}

// Update the Discord activity status. The provided state and details will be
// shown in the activity popout in the client. The call to
// Core::ActivityManager::UpdateActivity is non-blocking, and the provided
// callback will be ran on the next call to CallbackDiscordSDK.
// 
// It is worth nothing that for extra confusion, Discord decided "state" should
// render below "details" in the popout. Nevertheless, for code consistency I
// thought it would be best to still match the variables up with their docs, 
// "state" first, then "details".
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

// Initialize the Discord SDK. Magic function that kicks off the SDK in the
// background. Unclear if discord::Core::Create is blocking or not, but doesn't
// seem to matter regardless.
void InitDiscordSDK() 
{
    if (g_Conf->EmuOptions.DiscordSDK == false)
        return;

    discord::Core::Create(CLIENT_ID, DiscordCreateFlags_Default, &core);
}

#endif
