/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
// This is undoubtedly completely unnecessary.
#include "KeyboardQueue.h"

#ifdef __linux__
// Above code is for events that go from the plugin to core
// Here we need the contrary, event that come from core to the plugin
// Yes it is a crazy ping-pong hell ! I mostly copy past with
// a R_ (which stand for reverse)

#define R_EVENT_QUEUE_LEN 256
static std::mutex core_event;

static u8 R_lastQueuedEvent = 0;
static u8 R_nextQueuedEvent = 0;
static keyEvent R_queuedEvents[R_EVENT_QUEUE_LEN];

void R_QueueKeyEvent(const keyEvent &evt) {
	std::lock_guard<std::mutex> lock(core_event);

	R_queuedEvents[R_lastQueuedEvent] = evt;
	R_lastQueuedEvent = (R_lastQueuedEvent + 1) % R_EVENT_QUEUE_LEN;
	// In case someone has a severe Parkingson's disease
	assert(R_nextQueuedEvent != R_lastQueuedEvent);
}

int R_GetQueuedKeyEvent(keyEvent *event) {
	if (R_lastQueuedEvent == R_nextQueuedEvent) return 0;

	std::lock_guard<std::mutex> lock(core_event);
	*event = R_queuedEvents[R_nextQueuedEvent];
	R_nextQueuedEvent = (R_nextQueuedEvent + 1) % R_EVENT_QUEUE_LEN;
	return 1;
}

void R_ClearKeyQueue() {
	R_lastQueuedEvent = R_nextQueuedEvent;
}

EXPORT_C_(void) PADWriteEvent(keyEvent &evt)
{
	R_QueueKeyEvent(evt);
}
#endif
