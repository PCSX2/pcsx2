#include "toysmarch_drumpad.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <deque>

namespace usb_python2
{
	uint8_t stateIdx = 0;

	int toysmarch_drumpad_device::read(std::vector<uint8_t>& buf, const size_t requestedLen)
	{
		// The game always expects to be able to read the state from the device, regardless of what was written
		// If this packet can't be read then the game will throw a drum I/O error
		uint8_t state[9] = {
			stateIdx,
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP1Cymbal") * 128),
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP1DrumL") * 128),
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP1DrumR") * 128),
			0,
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP2Cymbal") * 128),
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP2DrumL") * 128),
			uint8_t(p2dev->GetKeyStateOneShot(L"ToysMarchP2DrumR") * 128),
			0
		};
		stateIdx = (stateIdx + 1) % 8;

		buf.push_back(0xaa);
		buf.insert(buf.end(), std::begin(state), std::end(state));
		buf.push_back(std::accumulate(std::begin(state), std::end(state), 0));

		return std::size(state) + 2;
	}

	void toysmarch_drumpad_device::write(std::vector<uint8_t>& packet)
	{
		// Keep alive packets
		// These packets are sent every 1 sec and don't expect a direct response
		// aa 11 11 11 33
		// aa 22 22 22 66
	}
} // namespace usb_python2