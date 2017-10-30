#include "PrecompiledHeader.h"

#include "PadData.h"

PadData::PadData()
{
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

std::vector<wxString> split(const wxString &s, char delim) {
	std::vector<wxString> elems;
	wxString item;
	for (char ch : s) {
		if (ch == delim) {
			if (!item.empty())
				elems.push_back(item);
			item.clear();
		}
		else {
			item += ch;
		}
	}
	if (!item.empty())
		elems.push_back(item);
	return elems;
}
void deserializeConvert(u8 & n, wxString s)
{
	try {
		n = std::stoi(s.ToStdString(), NULL, 16);
	}
	catch (std::invalid_argument e) {/*none*/ }
	catch (std::out_of_range e) {/*none*/ }
}
wxString PadData::serialize()const
{
	if (!fExistKey)return L"";
	wxString s = wxString::Format(L"%X", buf[0][0]);
	for (int i = 1; i < ArraySize(buf[0]); i++)
	{
		s += wxString::Format(L",%X", buf[0][i]);
	}
	for (int i = 0; i < ArraySize(buf[1]); i++)
	{
		s += wxString::Format(L",%X", buf[1][i]);
	}
	return s;
}
void PadData::deserialize(wxString s)
{
	std::vector<wxString> v = split(s, L',');
	if (v.size() != 12)return;

	for (int i = 0; i < 6; i++)
	{
		deserializeConvert(buf[0][i], v[i]);
	}
	for (int i = 0; i < 6; i++)
	{
		deserializeConvert(buf[1][i], v[6 + i]);
	}
	fExistKey = true;
}

//=====================================
// normal key
//=====================================
std::map<wxString, int> PadData::getNormalKeys(int port)const
{
	std::map<wxString, int> key;
	for (int i = 0; i < PadDataNormalKeysSize; i++)
	{
		key.insert(std::map<wxString, int>::value_type(PadDataNormalKeys[i], getNormalButton(port, PadDataNormalKeys[i])));
	}
	return key;
}
void PadData::setNormalKeys(int port, std::map<wxString, int> key)
{
	for (auto it = key.begin(); it != key.end(); ++it)
	{
		setNormalButton(port, it->first, it->second);
	}
}

void PadData::setNormalButton(int port, wxString button, int fpushed)
{
	if (port < 0 || 1 < port)return;
	byte keybit[2];
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

int PadData::getNormalButton(int port, wxString button)const
{
	if (port < 0 || 1 < port)return false;
	byte keybit[2];
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

void PadData::getKeyBit(byte keybit[2], wxString button)const
{
	if (button == L"up") { keybit[0] = 0b00010000; keybit[1] = 0b00000000; }
	else if (button == L"left") { keybit[0] = 0b10000000; keybit[1] = 0b00000000; }
	else if (button == L"right") { keybit[0] = 0b00100000; keybit[1] = 0b00000000; }
	else if (button == L"down") { keybit[0] = 0b01000000; keybit[1] = 0b00000000; }

	else if (button == L"start") { keybit[0] = 0b00001000; keybit[1] = 0b00000000; }
	else if (button == L"select") { keybit[0] = 0b00000001; keybit[1] = 0b00000000; }

	else if (button == L"x") { keybit[0] = 0b00000000; keybit[1] = 0b01000000; }
	else if (button == L"circle") { keybit[0] = 0b00000000; keybit[1] = 0b00100000; }
	else if (button == L"square") { keybit[0] = 0b00000000; keybit[1] = 0b10000000; }
	else if (button == L"triangle") { keybit[0] = 0b00000000; keybit[1] = 0b00010000; }

	else if (button == L"l1") { keybit[0] = 0b00000000; keybit[1] = 0b00000100; }
	else if (button == L"l2") { keybit[0] = 0b00000000; keybit[1] = 0b00000001; }
	else if (button == L"l3") { keybit[0] = 0b00000010; keybit[1] = 0b00000000; }
	else if (button == L"r1") { keybit[0] = 0b00000000; keybit[1] = 0b00001000; }
	else if (button == L"r2") { keybit[0] = 0b00000000; keybit[1] = 0b00000010; }
	else if (button == L"r3") { keybit[0] = 0b00000100; keybit[1] = 0b00000000; }
	else
	{
		keybit[0] = 0;
		keybit[1] = 0;
	}
}

// just returns an index for the buffer to set the pressure byte
// returns -1 if it is a button that does not support pressure sensitivty
int PadData::getPressureByte(wxString button)const
{
	// button order
	// R - L - U - D - Tri - Sqr - Circle - Cross - L1 - R1 - L2 - R2

	if (button == L"up") { return 2; }
	else if (button == L"left") { return 1; }
	else if (button == L"right") { return 0; }
	else if (button == L"down") { return 3; }

	else if (button == L"x") { return 6; }
	else if (button == L"circle") { return 5; }
	else if (button == L"square") { return 7; }
	else if (button == L"triangle") { return 4; }

	else if (button == L"l1") { return 8; }
	else if (button == L"l2") { return 10; }
	else if (button == L"r1") { return 9; }
	else if (button == L"r2") { return 11; }
	else
	{
		return 1;
	}
}

//=====================================
// analog key
//=====================================
std::map<wxString, int> PadData::getAnalogKeys(int port)const
{
	std::map<wxString, int> key;
	for (int i = 0; i < PadDataAnalogKeysSize; i++)
	{
		key.insert(std::map<wxString, int>::value_type(PadDataAnalogKeys[i], getAnalogButton(port, PadDataAnalogKeys[i])));
	}
	return key;
}
void PadData::setAnalogKeys(int port, std::map<wxString, int> key)
{
	for (auto it = key.begin(); it != key.end(); ++it)
	{
		setAnalogButton(port, it->first, it->second);
	}
}

void PadData::setAnalogButton(int port, wxString button, int push)
{
	if (port < 0 || 1 < port)return;
	if (push < 0)push = 0;
	else if (push > 255)push = 255;

	if (button == L"l_leftright") { buf[port][4] = push; }
	else if (button == L"l_updown") { buf[port][5] = push; }
	else if (button == L"r_leftright") { buf[port][2] = push; }
	else if (button == L"r_updown") { buf[port][3] = push; }
}

int PadData::getAnalogButton(int port, wxString button)const
{
	if (port < 0 || 1 < port)return 0;
	int val = 127;
	if (button == L"l_leftright") { val = buf[port][4]; }
	else if (button == L"l_updown") { val = buf[port][5]; }
	else if (button == L"r_leftright") { val = buf[port][2]; }
	else if (button == L"r_updown") { val = buf[port][3]; }
	return val;
}
