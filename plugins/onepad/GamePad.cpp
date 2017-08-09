#include "GamePad.h"
#ifdef SDL_BUILD
#include "SDL/joystick.h"
#endif

std::vector<std::unique_ptr<GamePad>> s_vgamePad;

/**
 * Following static methods are just forwarders to their backend
 * This is where link between agnostic and specific code is done
 **/

/**
 * Find every interesting devices and create right structure for them(depend on backend)
 **/
void GamePad::EnumerateGamePads(std::vector<std::unique_ptr<GamePad>> &vgamePad)
{
#ifdef SDL_BUILD
    JoystickInfo::EnumerateJoysticks(vgamePad);
#endif
}

/**
 * Safely dispatch to the Rumble method above
 **/
void GamePad::DoRumble(unsigned type, unsigned pad)
{
    int index = uid_to_index(pad);
    if (index >= 0)
        s_vgamePad[index]->Rumble(type, pad);
}

size_t GamePad::index_to_uid(int index)
{
    if ((index >= 0) && (index < (int)s_vgamePad.size()))
        return s_vgamePad[index]->GetUniqueIdentifier();
    else
        return 0;
}

int GamePad::uid_to_index(int pad)
{
    size_t uid = g_conf.get_joy_uid(pad);

    for (int i = 0; i < (int)s_vgamePad.size(); ++i) {
        if (s_vgamePad[i]->GetUniqueIdentifier() == uid)
            return i;
    }

    // Current uid wasn't found maybe the pad was unplugged. Or
    // user didn't select it. Fallback to 1st pad for
    // 1st player. And 2nd pad for 2nd player.
    if ((int)s_vgamePad.size() > pad)
        return pad;

    return -1;
}
