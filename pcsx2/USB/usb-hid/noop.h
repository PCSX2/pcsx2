#include "usb-hid.h"
#include "hidproxy.h"

namespace usb_hid { namespace noop {

static const char *APINAME = "noop";

class NOOP : public UsbHID
{
public:
	NOOP(int port, const char* dev_type): UsbHID(port, dev_type) {}
	~NOOP() {}
	int Open() { return 0; }
	int Close() { return 0; }
	int TokenIn(uint8_t *buf, int len) { return len; }
	int TokenOut(const uint8_t *data, int len) { return len; }
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return TEXT("NOOP");
	}

	static int Configure(int port, const char* dev_type, HIDType type, void *data)
	{
		return RESULT_CANCELED;
	}
};

}}