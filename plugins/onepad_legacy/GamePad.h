#pragma once

#include "onepad.h"
#include "controller.h"
#ifdef SDL_BUILD
#include <SDL.h>
#define HAT_UP SDL_HAT_UP
#define HAT_DOWN SDL_HAT_DOWN
#define HAT_RIGHT SDL_HAT_RIGHT
#define HAT_LEFT SDL_HAT_LEFT
#endif

class GamePad
{
public:
    GamePad()
        : devname("")
        , _id(-1)
        , numbuttons(0)
        , numaxes(0)
        , numhats(0)
        , deadzone(1500)
        , pad(-1)
    {
        vbuttonstate.clear();
        vaxisstate.clear();
        vhatstate.clear();
    }

    virtual ~GamePad()
    {
        return;
    }

    GamePad(const GamePad &);            // copy constructor
    GamePad &operator=(const GamePad &); // assignment

    /**
		 * Find every interesting devices and create right structure for them(depend on backend)
		 **/
    static void EnumerateGamePads(std::vector<GamePad *> &vgamePad);
    static void UpdateReleaseState();
    /**
		 * Update state of every attached devices
		 **/
    static void UpdateGamePadState();

    /**
		 * Causes devices to rumble
		 * Rumble will differ according to type which is either 0(small motor) or 1(big motor)
		 **/
    virtual void Rumble(int type, int pad) { return; }
    /**
		 * Safely dispatch to the Rumble method above
		 **/
    static void DoRumble(int type, int pad);

    virtual bool Init(int id) { return false; } // opens a handle and gets information

    /**
		 * Used for GUI checkbox to give feedback to the user
		 **/
    virtual bool TestForce(float strength = 0.6) { return false; }

    virtual bool PollButtons(u32 &pkey) { return false; }
    virtual bool PollAxes(u32 &pkey) { return false; }
    virtual bool PollHats(u32 &pkey) { return false; }

    virtual int GetHat(int key_to_axis)
    {
        return 0;
    }

    virtual int GetButton(int key_to_button)
    {
        return 0;
    }

    const std::string &GetName()
    {
        return devname;
    }

    int GetPAD()
    {
        return pad;
    }

    int GetNumButtons()
    {
        return numbuttons;
    }

    int GetNumAxes()
    {
        return numaxes;
    }

    int GetNumHats()
    {
        return numhats;
    }

    virtual int GetDeadzone()
    {
        return deadzone;
    }

    virtual void SaveState() {}

    int GetButtonState(int i)
    {
        return vbuttonstate[i];
    }

    int GetAxisState(int i)
    {
        return vaxisstate[i];
    }

    int GetHatState(int i)
    {
        //PAD_LOG("Getting POV State of %d.\n", i);
        return vhatstate[i];
    }

    void SetButtonState(int i, int state)
    {
        vbuttonstate[i] = state;
    }

    void SetAxisState(int i, int value)
    {
        vaxisstate[i] = value;
    }

    void SetHatState(int i, int value)
    {
        //PAD_LOG("We should set %d to %d.\n", i, value);
        vhatstate[i] = value;
    }

    virtual int GetAxisFromKey(int pad, int index) { return 0; }
    // These fields need to be inherited by child classes
protected:
    std::string devname; // pretty device name
    int _id;
    int numbuttons, numaxes, numhats;
    int deadzone;
    int pad;
    std::vector<int> vbuttonstate, vaxisstate, vhatstate;
};

extern std::vector<GamePad *> s_vgamePad;
extern bool GamePadIdWithinBounds(int joyid);
