#include "../padproxy.h"
#include "../../Win32/Config.h"

namespace usb_pad { namespace dx {

static const char *APINAME = "dinput";

class DInputPad : public Pad
{
public:
	DInputPad(int port, const char* dev_type) : Pad(port, dev_type), mUseRamp(0){}
	~DInputPad();
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return TEXT("DInput");
	}

	static int Configure(int port, const char* dev_type, void *data);
private:
	int32_t mUseRamp;
};

}} //namespace