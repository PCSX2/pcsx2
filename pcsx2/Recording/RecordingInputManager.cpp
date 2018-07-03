#include "PrecompiledHeader.h"

#include "RecordingInputManager.h"
#include "InputRecording.h"

RecordingInputManager g_RecordingInput;

RecordingInputManager::RecordingInputManager()
{
	for (u8 i = 0; i < 2; i++)
		virtualPad[i] = false;
}

void RecordingInputManager::ControllerInterrupt(u8 & data, u8 & port, u16 & BufCount, u8 buf[])
{
	if (port >= 2)
		return;

	if (virtualPad[port])
	{
		int bufIndex = BufCount - 3;
		// first two bytes have nothing of interest in the buffer
		// already handled by InputRecording.cpp
		if (BufCount < 3)
			return;

		// Normal keys
		// We want to perform an OR, but, since 255 means that no button is pressed and 0 that every button is pressed (and by De Morgan's Laws), we execute an AND.
		if (BufCount <= 4)
			buf[BufCount] = buf[BufCount] & pad.buf[port][BufCount - 3];
		// Analog keys (! overrides !)
		else if ((BufCount > 4 && BufCount <= 6) && pad.buf[port][BufCount - 3] != 127)
			buf[BufCount] = pad.buf[port][BufCount - 3];
		// Pressure sensitivity bytes
		else if (BufCount > 6)
			buf[BufCount] = pad.buf[port][BufCount - 3];

		// Updating movie file
		g_InputRecording.ControllerInterrupt(data, port, BufCount, buf);
	}
}

void RecordingInputManager::SetButtonState(int port, wxString button, int pressure)
{
	auto normalKeys = pad.getNormalKeys(port);
	normalKeys.at(button) = pressure;
	pad.setNormalKeys(port, normalKeys);
}

void RecordingInputManager::UpdateAnalog(int port, wxString key, int value)
{
	auto analogKeys = pad.getAnalogKeys(port);
	analogKeys.at(key) = value;
	pad.setAnalogKeys(port, analogKeys);
}

void RecordingInputManager::SetVirtualPadReading(int port, bool read)
{
	virtualPad[port] = read;
}
