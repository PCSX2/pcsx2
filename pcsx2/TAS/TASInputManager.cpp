#include "PrecompiledHeader.h"
#include "TASInputManager.h"
#include "lua/LuaManager.h"
#include "KeyMovie.h"

TASInputManager g_TASInput;

TASInputManager::TASInputManager()
{
	for (u8 i = 0; i < 2; i++)
		virtualPad[i] = false;
}

void TASInputManager::ControllerInterrupt(u8 & data, u8 & port, u16 & BufCount, u8 buf[])
{
	if (port >= 2)
		return;

	g_Lua.ControllerInterrupt(data, port, BufCount, buf);

	if (virtualPad[port])
	{
		int bufIndex = BufCount - 3;
		if (bufIndex < 0 || 6 < bufIndex)
			return;
		// Normal keys
		// We want to perform an OR, but, since 255 means that no button is pressed and 0 that every button is pressed (and by De Morgan's Laws), we execute an AND.
		if (bufIndex <= 1)
			buf[BufCount] = buf[BufCount] & pad.buf[port][bufIndex];
		// Analog keys (! overrides !)
		else if (pad.buf[port][bufIndex] != 127)
			buf[BufCount] = pad.buf[port][bufIndex];

		// Updating movie file
		g_KeyMovie.ControllerInterrupt(data, port, BufCount, buf);
	}
}

void TASInputManager::SetButtonState(int port, wxString button, bool state)
{
	auto normalKeys = pad.getNormalKeys(port);
	normalKeys.at(button) = state;
	pad.setNormalKeys(port, normalKeys);
}

void TASInputManager::UpdateAnalog(int port, wxString key, int value)
{
	auto analogKeys = pad.getAnalogKeys(port);
	analogKeys.at(key) = value;
	pad.setAnalogKeys(port, analogKeys);
}

void TASInputManager::SetVirtualPadReading(int port, bool read)
{
	virtualPad[port] = read;
}

