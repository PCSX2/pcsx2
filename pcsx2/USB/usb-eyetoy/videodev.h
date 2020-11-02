#ifndef VIDEODEV_H
#define VIDEODEV_H
#include "../qemu-usb/vl.h"
#include "../configuration.h"

namespace usb_eyetoy {

class VideoDevice
{
public:
	virtual ~VideoDevice() {}
	virtual int Open() = 0;
	virtual int Close() = 0;
	virtual int GetImage(uint8_t *buf, int len) = 0;
	virtual int Reset() = 0;

	virtual int Port() { return mPort; }
	virtual void Port(int port) { mPort = port; }

protected:
	int mPort;
};

} //namespace
#endif