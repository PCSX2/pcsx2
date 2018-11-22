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

void RecordingInputManager::SetButtonState(int port, PadDataNormalButton button, int pressure)
{
	int* buttons = pad.getNormalButtons(port);
	buttons[button] = pressure;
	pad.setNormalButtons(port, buttons);
}

void RecordingInputManager::UpdateAnalog(int port, PadDataAnalogVector vector, int value)
{
	int* vectors = pad.getAnalogVectors(port);
	vectors[vector] = value;
	pad.setAnalogVectors(port, vectors);
}

void RecordingInputManager::SetVirtualPadReading(int port, bool read)
{
	virtualPad[port] = read;
}
