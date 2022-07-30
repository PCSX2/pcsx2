#include "ddr_extio.h"

#ifdef INCLUDE_MINIMAID
#include <mmmagic.h>
#endif

#ifdef INCLUDE_BTOOLS
#include <windows.h>
#include <bemanitools/ddrio.h>

typedef void(WINAPI ddr_io_set_lights_extio_type)(uint32_t);
ddr_io_set_lights_extio_type* m_ddr_io_set_lights_extio = nullptr;
#endif

namespace usb_python2
{
	enum
	{
		EXTIO_LIGHT_PANEL_UP = 0x40,
		EXTIO_LIGHT_PANEL_DOWN = 0x20,
		EXTIO_LIGHT_PANEL_LEFT = 0x10,
		EXTIO_LIGHT_PANEL_RIGHT = 0x08,

		EXTIO_LIGHT_SENSOR_UP = 0x10,
		EXTIO_LIGHT_SENSOR_DOWN = 0x18,
		EXTIO_LIGHT_SENSOR_LEFT = 0x20,
		EXTIO_LIGHT_SENSOR_RIGHT = 0x28,
		EXTIO_LIGHT_SENSOR_ALL = 0x08,

		EXTIO_LIGHT_NEON = 0x40,
	};

	uint32_t oldLightPad1 = 0;
	uint32_t oldLightPad2 = 0;
	uint32_t oldLightBass = 0;
	uint32_t oldExtioState = 0;
	bool isMinimaidConnected = false;
	bool isUsingBtoolLights = false;

	extio_device::extio_device() {
		#ifdef INCLUDE_MINIMAID
		isMinimaidConnected = mm_connect_minimaid() == MINIMAID_CONNECTED;
		#endif

		#ifdef INCLUDE_BTOOLS
		HINSTANCE hDDRIO = LoadLibraryA("ddrio.dll");
		m_ddr_io_set_lights_extio = (ddr_io_set_lights_extio_type*)GetProcAddress(hDDRIO, "ddr_io_set_lights_extio");

		//if the function was found with the library successfully loaded, begin to use it.
		isUsingBtoolLights = m_ddr_io_set_lights_extio;

		//turn the lights off during boot.
		if (isUsingBtoolLights)
			m_ddr_io_set_lights_extio(0);
		#endif
	}

	// Reference: https://github.com/nchowning/open-io/blob/master/extio-emulator.ino
	void extio_device::write(std::vector<uint8_t>& packet)
	{
		if (!isOpen)
			return;

		#if 0
		printf("EXTIO packet: ");
		for (int i = 0; i < packet.size(); i++)
			printf("%02x ", packet[i]);
		printf("\n");
		#endif

		if (packet.size() != 4)
			return;

		/*
		* DDR:
		* 80 00 40 40 CCFL
		* 90 00 00 10 1P FOOT LEFT
		* c0 00 00 40 1P FOOT UP
		* 88 00 00 08 1P FOOT RIGHT
		* a0 00 00 20 1P FOOT DOWN
		* 80 10 00 10 2P FOOT LEFT
		* 80 40 00 40 2P FOOT UP
		* 80 08 00 08 2P FOOT RIGHT
		* 80 20 00 20 2P FOOT DOWN
		*/

		const auto expectedChecksum = packet[3];
		const uint8_t calculatedChecksum = (packet[0] + packet[1] + packet[2]) & 0x7f;

		if (calculatedChecksum != expectedChecksum)
		{
			//printf("EXTIO packet checksum invalid! %02x vs %02x\n", expectedChecksum, calculatedChecksum);
			return;
		}

		#if defined(INCLUDE_MINIMAID) || defined(INCLUDE_BTOOLS)
		const auto p1PanelLights = packet[0] & 0x7f;
		const auto p2PanelLights = packet[1] & 0x7f;
		const auto neonLights = packet[2];
		const auto panelSensors = packet[3] & 0x3f;

		if (isMinimaidConnected)
		{
			auto curLightPad1 = 0;
			curLightPad1 = mm_setDDRPad1Light(DDR_DOUBLE_PAD_UP, !!(p1PanelLights & EXTIO_LIGHT_PANEL_UP));
			curLightPad1 = mm_setDDRPad1Light(DDR_DOUBLE_PAD_LEFT, !!(p1PanelLights & EXTIO_LIGHT_PANEL_LEFT));
			curLightPad1 = mm_setDDRPad1Light(DDR_DOUBLE_PAD_RIGHT, !!(p1PanelLights & EXTIO_LIGHT_PANEL_RIGHT));
			curLightPad1 = mm_setDDRPad1Light(DDR_DOUBLE_PAD_DOWN, !!(p1PanelLights & EXTIO_LIGHT_PANEL_DOWN));

			auto curLightPad2 = 0;
			curLightPad2 = mm_setDDRPad2Light(DDR_DOUBLE_PAD_UP, !!(p2PanelLights & EXTIO_LIGHT_PANEL_UP));
			curLightPad2 = mm_setDDRPad2Light(DDR_DOUBLE_PAD_LEFT, !!(p2PanelLights & EXTIO_LIGHT_PANEL_LEFT));
			curLightPad2 = mm_setDDRPad2Light(DDR_DOUBLE_PAD_RIGHT, !!(p2PanelLights & EXTIO_LIGHT_PANEL_RIGHT));
			curLightPad2 = mm_setDDRPad2Light(DDR_DOUBLE_PAD_DOWN, !!(p2PanelLights & EXTIO_LIGHT_PANEL_DOWN));

			auto curLightBass = mm_setDDRBassLight(DDR_DOUBLE_BASS_LIGHTS, !!(neonLights & EXTIO_LIGHT_NEON));

			// extio gets spammed and it's not intensive to set the light flags in memory, but sending a new update every single extio update
			// may be a bit too much so only send when an update is detected.
			if (curLightPad1 != oldLightPad1 || curLightPad2 != oldLightPad2 || curLightBass != oldLightBass)
				mm_sendDDRMiniMaidUpdate();

			oldLightPad1 = curLightPad1;
			oldLightPad2 = curLightPad2;
			oldLightBass = curLightBass;
		}

		if (isUsingBtoolLights)
		{
			uint32_t extioState = 0;

			extioState |= (p1PanelLights & EXTIO_LIGHT_PANEL_UP) ? (1 << LIGHT_P1_UP) : 0;
			extioState |= (p1PanelLights & EXTIO_LIGHT_PANEL_DOWN) ? (1 << LIGHT_P1_DOWN) : 0;
			extioState |= (p1PanelLights & EXTIO_LIGHT_PANEL_LEFT) ? (1 << LIGHT_P1_LEFT) : 0;
			extioState |= (p1PanelLights & EXTIO_LIGHT_PANEL_RIGHT) ? (1 << LIGHT_P1_RIGHT) : 0;

			extioState |= (p2PanelLights & EXTIO_LIGHT_PANEL_UP) ? (1 << LIGHT_P2_UP) : 0;
			extioState |= (p2PanelLights & EXTIO_LIGHT_PANEL_DOWN) ? (1 << LIGHT_P2_DOWN) : 0;
			extioState |= (p2PanelLights & EXTIO_LIGHT_PANEL_LEFT) ? (1 << LIGHT_P2_LEFT) : 0;
			extioState |= (p2PanelLights & EXTIO_LIGHT_PANEL_RIGHT) ? (1 << LIGHT_P2_RIGHT) : 0;

			extioState |= (neonLights & EXTIO_LIGHT_NEON) ? (1 << LIGHT_NEONS) : 0;

			if (extioState != oldExtioState)
			{
				m_ddr_io_set_lights_extio(extioState);
			}

			oldExtioState = extioState;
		}
		#endif

		std::vector<uint8_t> response;
		response.push_back(0x11);
		packet.erase(packet.begin(), packet.begin() + 4);

		add_packet(response);
	}
} // namespace usb_python2