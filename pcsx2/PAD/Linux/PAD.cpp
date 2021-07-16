/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>

#include "keyboard.h"
#include "PAD.h"
#include "state_management.h"

#if defined(__unix__) || defined(__APPLE__)
#include "Device.h"
#endif

#ifdef __linux__
#include <unistd.h>
#endif

const u32 revision = 3;
const u32 build = 0; // increase that with each version
#define PAD_SAVE_STATE_VERSION ((revision << 8) | (build << 0))

PADconf g_conf;
keyEvent event;

static keyEvent s_event;
std::string s_padstrLogPath("logs/");

FILE* padLog = NULL;

KeyStatus g_key_status;

MtQueue<keyEvent> g_ev_fifo;


void __LogToConsole(const char* fmt, ...)
{
	va_list list;

	va_start(list, fmt);

	if (padLog != NULL)
		vfprintf(padLog, fmt, list);

	printf("PAD: ");
	vprintf(fmt, list);
	va_end(list);
}

void initLogging()
{
#ifdef PAD_LOG
	if (padLog)
		return;

	const std::string LogFile(s_padstrLogPath + "padLog.txt");
	padLog = fopen(LogFile.c_str(), "w");

	if (padLog)
		setvbuf(padLog, NULL, _IONBF, 0);

	PAD_LOG("PADinit\n");
#endif
}

void CloseLogging()
{
#ifdef PAD_LOG
	if (padLog)
	{
		fclose(padLog);
		padLog = NULL;
	}
#endif
}

s32 PADinit()
{
	initLogging();

	PADLoadConfig();

	Pad::reset_all();

	query.reset();

	for (int port = 0; port < 2; port++)
		slots[port] = 0;

	return 0;
}

void PADshutdown()
{
	CloseLogging();
}

s32 PADopen(void* pDsp)
{
	memset(&event, 0, sizeof(event));
	g_key_status.Init();

	g_ev_fifo.reset();

#if defined(__unix__) || defined(__APPLE__)
	EnumerateDevices();
#endif
	return _PADopen(pDsp);
}

void PADsetLogDir(const char* dir)
{
	// Get the path to the log directory.
	s_padstrLogPath = (dir == NULL) ? "logs/" : dir;

	// Reload the log file after updated the path
	CloseLogging();
	initLogging();
}

void PADclose()
{
	_PADclose();
}

s32 PADsetSlot(u8 port, u8 slot)
{
	port--;
	slot--;
	if (port > 1 || slot > 3)
	{
		return 0;
	}
	// Even if no pad there, record the slot, as it is the active slot regardless.
	slots[port] = slot;

	return 1;
}

s32 PADfreeze(FreezeAction mode, freezeData* data)
{
	if (!data)
		return -1;

	if (mode == FreezeAction::Size)
	{
		data->size = sizeof(PadFullFreezeData);
	}
	else if (mode == FreezeAction::Load)
	{
		PadFullFreezeData* pdata = (PadFullFreezeData*)(data->data);

		Pad::stop_vibrate_all();

		if (data->size != sizeof(PadFullFreezeData) || pdata->version != PAD_SAVE_STATE_VERSION ||
			strncmp(pdata->format, "LinPad", sizeof(pdata->format)))
			return 0;

		query = pdata->query;
		if (pdata->query.slot < 4)
		{
			query = pdata->query;
		}

		// Tales of the Abyss - pad fix
		// - restore data for both ports
		for (int port = 0; port < 2; port++)
		{
			for (int slot = 0; slot < 4; slot++)
			{
				u8 mode = pdata->padData[port][slot].mode;

				if (mode != MODE_DIGITAL && mode != MODE_ANALOG && mode != MODE_DS2_NATIVE)
				{
					break;
				}

				memcpy(&pads[port][slot], &pdata->padData[port][slot], sizeof(PadFreezeData));
			}

			if (pdata->slot[port] < 4)
				slots[port] = pdata->slot[port];
		}
	}
	else if (mode == FreezeAction::Save)
	{
		if (data->size != sizeof(PadFullFreezeData))
			return 0;

		PadFullFreezeData* pdata = (PadFullFreezeData*)(data->data);

		// Tales of the Abyss - pad fix
		// - PCSX2 only saves port0 (save #1), then port1 (save #2)

		memset(pdata, 0, data->size);
		strncpy(pdata->format, "LinPad", sizeof(pdata->format));
		pdata->version = PAD_SAVE_STATE_VERSION;
		pdata->query = query;

		for (int port = 0; port < 2; port++)
		{
			for (int slot = 0; slot < 4; slot++)
			{
				pdata->padData[port][slot] = pads[port][slot];
			}

			pdata->slot[port] = slots[port];
		}
	}
	else
	{
		return -1;
	}

	return 0;
}

u8 PADstartPoll(int pad)
{
	return pad_start_poll(pad);
}

u8 PADpoll(u8 value)
{
	return pad_poll(value);
}

// PADkeyEvent is called every vsync (return NULL if no event)
keyEvent* PADkeyEvent()
{
#ifdef SDL_BUILD
	// Take the opportunity to handle hot plugging here
	SDL_Event events;
	while (SDL_PollEvent(&events))
	{
		switch (events.type)
		{
			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
				EnumerateDevices();
				break;
			default:
				break;
		}
	}
#endif
#ifdef __unix__
	if (g_ev_fifo.size() == 0)
	{
		// PAD_LOG("No events in queue, returning empty event\n");
		s_event = event;
		event.evt = 0;
		event.key = 0;
		return &s_event;
	}
	s_event = g_ev_fifo.dequeue();

	AnalyzeKeyEvent(s_event);
	// PAD_LOG("Returning Event. Event Type: %d, Key: %d\n", s_event.evt, s_event.key);
	return &s_event;
#else // MacOS
	s_event = event;
	event.evt = 0;
	event.key = 0;
	return &s_event;
#endif
}

#if defined(__unix__)
void PADWriteEvent(keyEvent& evt)
{
	// if (evt.evt != 6) { // Skip mouse move events for logging
	//     PAD_LOG("Pushing Event. Event Type: %d, Key: %d\n", evt.evt, evt.key);
	// }
	g_ev_fifo.push(evt);
}
#endif
