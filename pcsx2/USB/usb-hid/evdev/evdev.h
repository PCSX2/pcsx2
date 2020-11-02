#pragma once
#include "../../linux/util.h"
#include <linux/input.h>
#include <unistd.h>
#include <dirent.h>
#include <thread>
#include <atomic>
#include "../usb-hid.h"

namespace usb_hid { namespace evdev {

static const char *APINAME = "evdev";

class EvDev : public UsbHID
{
public:
	EvDev(int port, const char* dev_type): UsbHID(port, dev_type)
	, mHandle(-1)
	, mReaderThreadIsRunning(false)
	{
	}

	~EvDev() { Close(); }
	int Open();
	int Close();
	int TokenIn(uint8_t *buf, int len);
	int TokenOut(const uint8_t *data, int len);
	int Reset() { return 0; }

	static const TCHAR* Name()
	{
		return TEXT("Evdev");
	}

	static int Configure(int port, const char* dev_type, HIDType hid_type, void *data);
protected:
	static void ReaderThread(void *ptr);

	int mHandle;
	uint16_t mAxisMap[ABS_MAX + 1];
	uint16_t mBtnMap[KEY_MAX + 1];
	int mAxisCount;
	int mButtonCount;

	std::thread mReaderThread;
	std::atomic<bool> mReaderThreadIsRunning;
};

template< size_t _Size >
bool GetEvdevName(const std::string& path, char (&name)[_Size])
{
	int fd = 0;
	if ((fd = open(path.c_str(), O_RDONLY)) < 0)
	{
		fprintf(stderr, "Cannot open %s\n", path.c_str());
	}
	else
	{
		if (ioctl(fd, EVIOCGNAME(_Size), name) < -1)
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
