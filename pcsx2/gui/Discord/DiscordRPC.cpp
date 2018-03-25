/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2018  PCSX2 Dev Team
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

// A Discord Rich Presence (DPRC) implementation.

// Discord's official documentation for future expansion:
// https://discordapp.com/developers/docs/rich-presence/how-to

#include "PrecompiledHeader.h"   // memset, sprintf, integer shorthands
#include "Discord/discord_rpc.h" // Discord's provided header for interfacing with their lib
#include "DiscordRPC.h"          // Our customized header

char *drpcGameTitle;
u64 drpcStartTime;

// Set the display name for the active game for the next update.
// Pass dprcMenuStr or nullptr to reset to "In the menus".
// * Does not immediately update Discord RPC, use drpcUpdate to do so
// ** Side note, wx is a complete dumpster fire. Apparently passing a wxString in to a function crashes
// the entire library, so we're just going to do wxString to char* conversions in core code and accept
// that it adds a little more bulk.
void drpcSetGame(char* newTitle) {
	drpcGameTitle = newTitle;
}

// Update Discord RPC.
void drpcUpdate() {
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.details = drpcGameTitle != nullptr ? drpcGameTitle : drpcMenuStr;
	discordPresence.startTimestamp = drpcStartTime; // This doesn't seem to be displaying... Not quite sure why either...
	discordPresence.largeImageKey = "pcsx2";
	discordPresence.smallImageKey = "pcsx2_small";

	// Probably useful fields for netplay but not for PCSX2 core.
	// Note these are not in the header nor this cpp file, so if someone (cough SmileTheory)
	// wants to use these for online features they'll need to make the externs and setters.
	// discordPresence.partyId = "";
	// discordPresence.partySize = 1;
	// discordPresence.partyMax = 6;
	// discordPresence.joinSecret = "4b2fdce12f639de8bfa7e3591b71a0d679d7c93f"; // These are sample secrets provided in their setup guides, not actual.
	// discordPresence.spectateSecret = "e7eb30d2ee025ed05c71ea495f770b76454ee4e0";
	
	Discord_UpdatePresence(&discordPresence);
}

// Self documenting.
void drpcShutdown() {
	Discord_Shutdown();
}

// Not quite sure. Thought this was a Discord RPC ready indicator, but it never fires after init.
void drpcHandleDiscordReady() {}

// Discord RPC error. Not quite sure the scope of this, but we'll just do a console warn for now.
void drpcHandleDiscordError(int errorCode, const char *message) {
	Console.Warning("[DiscordRPC] Error %i occurred: %s", errorCode, message);
}

// Discord RPC disconnected. Pretty much same deal as above.
void drpcHandleDiscordDisconnected(int errorCode, const char *message) {
	Console.Warning("[DiscordRPC] Connection lost! Error %i occurred: %s", errorCode, message);
	// Is RPC going to reconnect when possible? Or is it just dead... Might want to shutdown if dead.
	// But as above, we don't know scope here so we'll leave it for now.
}

// Discord RPC event for game join (Unused, but might be useful for netplay builds)
void drpcHandleDiscordJoinGame(const char *joinSecret) {}

// Discord RPC event for when someone tries to spectate the user. (Unused, but might be useful for netplay builds)
void drpcHandleDiscordSpectateGame(const char *spectateSecret) {}

// Discord RPC event for when the user receives a join request. (Unused, but might be useful for netplay builds)
void drpcHandleDiscordJoinRequest(const DiscordJoinRequest *request) {}

// Initialize Discord RPC. Sets event handler associations and connects with the Discord app.
void drpcInit() {
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = drpcHandleDiscordReady;
	handlers.errored = drpcHandleDiscordError;
	handlers.disconnected = drpcHandleDiscordDisconnected;
	handlers.joinGame = drpcHandleDiscordJoinGame;
	handlers.spectateGame = drpcHandleDiscordSpectateGame;
	handlers.joinRequest = drpcHandleDiscordJoinRequest;

	// Discord_Initialize(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId)
	Discord_Initialize("425430121822814219", &handlers, 1, nullptr);
}