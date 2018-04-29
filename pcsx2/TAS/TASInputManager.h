#pragma once
#include "TAS/PadData.h"

class TASInputManager
{
public:
	TASInputManager();

	void ControllerInterrupt(u8 &data, u8 &port, u16 & BufCount, u8 buf[]);

	// Handles normal keys
	void SetButtonState(int port, wxString button, int pressure);

	// Handles analog sticks
	void UpdateAnalog(int port, wxString key, int value);

	void SetVirtualPadReading(int port, bool read);

protected:
	PadData pad;
	bool virtualPad[2];
};
extern TASInputManager g_TASInput;
