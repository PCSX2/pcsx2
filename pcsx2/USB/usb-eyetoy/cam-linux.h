#include "videodev.h"

namespace usb_eyetoy
{
namespace linux_api
{

typedef struct {
	void   *start;
	size_t  length;
} buffer_t;

static const char *APINAME = "V4L2";

class V4L2 : public VideoDevice
{
public:
	V4L2(int port) : mPort(port) {};
	~V4L2(){};
	int Open();
	int Close();
	int GetImage(uint8_t *buf, int len);
	int Reset() { return 0; };

	static const TCHAR *Name() {
		return TEXT("V4L2");
	}
	static int Configure(int port, const char *dev_type, void *data);

	int Port() { return mPort; }
	void Port(int port) { mPort = port; }

private:
	int mPort;
};

} // namespace linux_api
} // namespace usb_eyetoy
