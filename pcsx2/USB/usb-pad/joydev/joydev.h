#pragma once
#include "../../linux/util.h"
#include "../evdev/evdev-ff.h"
#include "../evdev/shared.h"

namespace usb_pad { namespace joydev {

void EnumerateDevices(vstring& list);

static const char *APINAME = "joydev";

class JoyDevPad : public Pad
{
public:
	JoyDevPad(int port, const char* dev_type): Pad(port, dev_type)
	{
	}

	~JoyDevPad() { Close(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return "Joydev";
	}

	static int Configure(int port, const char* dev_type, void *data);
protected:
	int mHandleFF;
	struct wheel_data_t mWheelData;
	std::vector<evdev::device_data> mDevices;
};

template< size_t _Size >
bool GetJoystickName(const std::string& path, char (&name)[_Size])
{
	int fd = 0;
	if ((fd = open(path.c_str(), O_RDONLY)) < 0)
	{
		fprintf(stderr, "Cannot open %s\n", path.c_str());
	}
	else
	{
		if (ioctl(fd, JSIOCGNAME(_Size), name) < -1)
		{
			fprintf(stderr, "Cannot get controller's name\n");
			close(fd);
			return false;
		}
		close(fd);
		return true;
	}
	return false;
}

}} //namespace
