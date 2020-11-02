#include "shared/rawinput.h"
#include "../hidproxy.h"
#include "../usb-hid.h"

namespace usb_hid { namespace raw {

static const char* APINAME = "rawinput";

class RawInput : public UsbHID, shared::rawinput::ParseRawInputCB
{
public:
	RawInput(int port, const char* dev_type) : UsbHID(port, dev_type)
	{
	}
	~RawInput()
	{ 
		Close();
	}
	int Open();
	int Close();
//	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }
	void ParseRawInput(PRAWINPUT pRawInput);

	static const TCHAR* Name()
	{
		return TEXT("Raw Input");
	}

	static int Configure(int port, const char* dev_type, HIDType, void *data);
};

}} // namespace