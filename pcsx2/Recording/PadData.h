#pragma once

#include <map>

#define PadDataNormalKeysSize 16
const wxString PadDataNormalKeys[PadDataNormalKeysSize] =
{
	"up",
	"right",
	"left",
	"down",
	"cross",
	"circle",
	"square",
	"triangle",
	"l1",
	"l2",
	"r1",
	"r2",
	"l3",
	"r3",
	"select",
	"start"
};

#define PadDataAnalogKeysSize 4
const wxString PadDataAnalogKeys[PadDataAnalogKeysSize] =
{
	"l_analog_x",
	"l_analog_y",
	"r_analog_x",
	"r_analog_y"
};


//----------------------------
// Pad info
//----------------------------
struct PadData
{
public:
	PadData();
	~PadData() {}
public:

	bool fExistKey = false;
	u8 buf[2][18];

public:
	// Prints controlller data every frame to the Controller Log filter, disabled by default
	static void logPadData(u8 port, u16 bufCount, u8 buf[512]);
	wxString serialize()const;
	void deserialize(wxString s);

	//------------------------------------------
	// normalKey
	//------------------------------------------
	std::map<wxString, int> getNormalKeys(int port)const;
	void setNormalKeys(int port, std::map<wxString, int> key);

	//------------------------------------------
	// analogKey 0~255
	//   max left/up    : 0
	//   neutral        : 127
	//   max right/down : 255
	//------------------------------------------
	std::map<wxString, int> getAnalogKeys(int port)const;
	void setAnalogKeys(int port, std::map<wxString, int> key);


private:
	void setNormalButton(int port, wxString button, int pressure);
	int getNormalButton(int port, wxString button)const;
	void getKeyBit(wxByte keybit[2], wxString button)const;
	int getPressureByte(wxString button)const;

	void setAnalogButton(int port, wxString button, int push);
	int getAnalogButton(int port, wxString button)const;


};
