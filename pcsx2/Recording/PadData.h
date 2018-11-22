#pragma once

#include <map>

#define PadDataNormalButtonCount 16
enum PadDataNormalButton
{
	UP,
	RIGHT,
	LEFT,
	DOWN,
	CROSS,
	CIRCLE,
	SQUARE,
	TRIANGLE,
	L1,
	L2,
	R1,
	R2,
	L3,
	R3,
	SELECT,
	START
};

#define PadDataAnalogVectorCount 4
enum PadDataAnalogVector
{
	LEFT_ANALOG_X,
	LEFT_ANALOG_Y,
	RIGHT_ANALOG_X,
	RIGHT_ANALOG_Y
};

struct PadData
{
public:
	PadData();
	~PadData() {}

	bool fExistKey = false;
	u8 buf[2][18];

	// Prints controlller data every frame to the Controller Log filter, disabled by default
	static void logPadData(u8 port, u16 bufCount, u8 buf[512]);

	// Normal Buttons
	int* getNormalButtons(int port) const;
	void setNormalButtons(int port, int* buttons);

	// Analog Vectors
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	int* getAnalogVectors(int port) const;
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	void setAnalogVectors(int port, int* vector);

private:
	void setNormalButton(int port, PadDataNormalButton button, int pressure);
	int getNormalButton(int port, PadDataNormalButton button) const;
	void getKeyBit(wxByte keybit[2], PadDataNormalButton button) const;
	int getPressureByte(PadDataNormalButton button) const;

	void setAnalogVector(int port, PadDataAnalogVector vector, int val);
	int getAnalogVector(int port, PadDataAnalogVector vector) const;
	int getAnalogVectorByte(PadDataAnalogVector vector) const;
};
