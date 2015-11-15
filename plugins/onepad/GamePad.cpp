#include "GamePad.h"
#ifdef  SDL_BUILD
#include "SDL/joystick.h"
#endif

vector<GamePad*> s_vgamePad;
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
void GamePad::EnumerateGamePads(vector<GamePad*>& vgamePad)
{
#ifdef  SDL_BUILD
	JoystickInfo::EnumerateJoysticks(vgamePad);
#endif
}

void GamePad::UpdateReleaseState()
{
#ifdef  SDL_BUILD
	JoystickInfo::UpdateReleaseState();
#endif
}

/**
 * Safely dispatch to the Rumble method above
 **/
void GamePad::DoRumble(int type, int pad)
{
	u32 id = conf->get_joyid(pad);
	if (GamePadIdWithinBounds(id)) {
		GamePad* gamePad = s_vgamePad[id];
		if (gamePad)
			gamePad->Rumble(type, pad);
	}
}

/**
 * Update state of every attached devices
 **/
void GamePad::UpdateGamePadState()
{
#ifdef  SDL_BUILD
	SDL_JoystickUpdate(); // No need to make yet another function call for that
#endif
}


