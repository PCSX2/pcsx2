#include "PrecompiledHeader.h"

#include "Common.h"
#include "ConsoleLogger.h"
#include "PadData.h"

PadData::PadData()
{
	// TODO - multi-tap support eventually?
	for (int port = 0; port < 2; port++)
	{
		buf[port][0] = 255;
		buf[port][1] = 255;
		buf[port][2] = 127;
		buf[port][3] = 127;
		buf[port][4] = 127;
		buf[port][5] = 127;
	}
}

void PadData::logPadData(u8 port, u16 bufCount, u8 buf[512]) {
	// skip first two bytes because they dont seem to matter
	if (port == 0 && bufCount > 2) 
	{
		if (bufCount == 3)
		{
			controlLog(wxString::Format("\nController Port %d", port));
			controlLog(wxString::Format("\nPressed Flags - "));
		}
		if (bufCount == 5) // analog sticks
		{
			controlLog(wxString::Format("\nAnalog Sticks - "));
		}
		if (bufCount == 9) // pressure sensitive bytes
		{
			controlLog(wxString::Format("\nPressure Bytes - "));
		}
		controlLog(wxString::Format("%3d ", buf[bufCount]));
	}
}

int* PadData::getNormalButtons(int port) const
{
	int buttons[PadDataNormalButtonCount];
	for (int i = 0; i < PadDataNormalButtonCount; i++)
	{
		buttons[i] = getNormalButton(port, PadDataNormalButton(i));
	}
	return buttons;
}
void PadData::setNormalButtons(int port, int* buttons)
{
	for (int i = 0; i < PadDataNormalButtonCount; i++)
	{
		setNormalButton(port, PadDataNormalButton(i), buttons[i]);
	}
}

void PadData::setNormalButton(int port, PadDataNormalButton button, int fpushed)
{
	if (port < 0 || 1 < port)
		return;
	wxByte keybit[2];
	getKeyBit(keybit, button);
	int pressureByteIndex = getPressureByte(button);

	if (fpushed > 0)
	{
		// set whether or not the button is pressed
		buf[port][0] = ~(~buf[port][0] | keybit[0]);
		buf[port][1] = ~(~buf[port][1] | keybit[1]);

		// if the button supports pressure sensitivity
		if (pressureByteIndex != -1)
		{
			buf[port][6 + pressureByteIndex] = fpushed;
		}
	}
	else
	{
		buf[port][0] = (buf[port][0] | keybit[0]);
		buf[port][1] = (buf[port][1] | keybit[1]);

		// if the button supports pressure sensitivity
		if (pressureByteIndex != -1)
		{
			buf[port][6 + pressureByteIndex] = 0;
		}
	}
}

int PadData::getNormalButton(int port, PadDataNormalButton button) const
{
	if (port < 0 || 1 < port)
		return false;
	wxByte keybit[2];
	getKeyBit(keybit, button);
	int pressureByteIndex = getPressureByte(button);

	// If the button is pressed on either controller
	bool f1 = (~buf[port][0] & keybit[0])>0;
	bool f2 = (~buf[port][1] & keybit[1])>0;

	if (f1 || f2)
	{
		// If the button does not support pressure sensitive inputs
		// just return 1 for pressed.
		if (pressureByteIndex == -1)
		{
			return 1;
		}
		// else return the pressure information
		return buf[port][6 + pressureByteIndex];
	}

	// else the button isnt pressed at all
	return 0;
}

void PadData::getKeyBit(wxByte keybit[2], PadDataNormalButton button) const
{
	if (button == UP) { keybit[0] = 0b00010000; keybit[1] = 0b00000000; }
	else if (button == LEFT) { keybit[0] = 0b10000000; keybit[1] = 0b00000000; }
	else if (button == RIGHT) { keybit[0] = 0b00100000; keybit[1] = 0b00000000; }
	else if (button == DOWN) { keybit[0] = 0b01000000; keybit[1] = 0b00000000; }
	else if (button == START) { keybit[0] = 0b00001000; keybit[1] = 0b00000000; }
	else if (button == SELECT) { keybit[0] = 0b00000001; keybit[1] = 0b00000000; }
	else if (button == CROSS) { keybit[0] = 0b00000000; keybit[1] = 0b01000000; }
	else if (button == CIRCLE) { keybit[0] = 0b00000000; keybit[1] = 0b00100000; }
	else if (button == SQUARE) { keybit[0] = 0b00000000; keybit[1] = 0b10000000; }
	else if (button == TRIANGLE) { keybit[0] = 0b00000000; keybit[1] = 0b00010000; }
	else if (button == L1) { keybit[0] = 0b00000000; keybit[1] = 0b00000100; }
	else if (button == L2) { keybit[0] = 0b00000000; keybit[1] = 0b00000001; }
	else if (button == L3) { keybit[0] = 0b00000010; keybit[1] = 0b00000000; }
	else if (button == R1) { keybit[0] = 0b00000000; keybit[1] = 0b00001000; }
	else if (button == R2) { keybit[0] = 0b00000000; keybit[1] = 0b00000010; }
	else if (button == R3) { keybit[0] = 0b00000100; keybit[1] = 0b00000000; }
	else { keybit[0] = 0; keybit[1] = 0; }
}

// Returns an index for the buffer to set the pressure byte
// Returns -1 if it is a button that does not support pressure sensitivty
int PadData::getPressureByte(PadDataNormalButton button) const
{
	// Pressure Byte Order
	// R - L - U - D - Tri - Sqr - Circle - Cross - L1 - R1 - L2 - R2
	if (button == UP) 
		return 2;
	else if (button == LEFT) 
		return 1;
	else if (button == RIGHT) 
		return 0;
	else if (button == DOWN) 
		return 3;
	else if (button == CROSS) 
		return 6;
	else if (button == CIRCLE) 
		return 5;
	else if (button == SQUARE) 
		return 7;
	else if (button == TRIANGLE) 
		return 4;
	else if (button == L1) 
		return 8;
	else if (button == L2)
		return 10;
	else if (button == R1) 
		return 9;
	else if (button == R2) 
		return 11;
	else
		return -1;
}

int* PadData::getAnalogVectors(int port) const
{
	int vectors[PadDataAnalogVectorCount];
	for (int i = 0; i < PadDataAnalogVectorCount; i++)
	{
		vectors[i] = getAnalogVector(port, PadDataAnalogVector(i));
	}
	return vectors;
}

void PadData::setAnalogVectors(int port, int* vectors)
{
	for (int i = 0; i < PadDataAnalogVectorCount; i++)
	{
		setAnalogVector(port, PadDataAnalogVector(i), vectors[i]);
	}
}

void PadData::setAnalogVector(int port, PadDataAnalogVector vector, int val)
{
	if (port < 0 || 1 < port)
		return;
	if (val < 0)
		val = 0;
	else if (val > 255)
		val = 255;

	buf[port][getAnalogVectorByte(vector)] = val;
}

int PadData::getAnalogVector(int port, PadDataAnalogVector vector) const
{
	if (port < 0 || 1 < port)
		return 0;

	return buf[port][getAnalogVectorByte(vector)];
}

// Returns an index for the buffer to set the analog's vector
int PadData::getAnalogVectorByte(PadDataAnalogVector vector) const
{
	// Vector Byte Ordering
	// RX - RY - LX - LY
	if (vector == LEFT_ANALOG_X)
		return 4;
	else if (vector == LEFT_ANALOG_Y)
		return 5;
	else if (vector == RIGHT_ANALOG_X)
		return 2;
	else
		return 3;
}
