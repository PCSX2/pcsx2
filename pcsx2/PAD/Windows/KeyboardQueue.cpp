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

#include "PrecompiledHeader.h"
#include "Global.h"
// This is undoubtedly completely unnecessary.
#include "KeyboardQueue.h"

static std::mutex cSection;

#define EVENT_QUEUE_LEN 16
// Actually points one beyond the last queued event.
static u8 lastQueuedEvent = 0;
static u8 nextQueuedEvent = 0;
static HostKeyEvent queuedEvents[EVENT_QUEUE_LEN];

void QueueKeyEvent(u32 key, HostKeyEvent::Type event)
{
	std::lock_guard<std::mutex> lock(cSection);

	// Don't queue events if escape is on top of queue.  This is just for safety
	// purposes when a game is killing the emulator for whatever reason.
	if (nextQueuedEvent == lastQueuedEvent ||
		queuedEvents[nextQueuedEvent].key != VK_ESCAPE ||
		queuedEvents[nextQueuedEvent].type != HostKeyEvent::Type::KeyPressed)
	{
		// Clear queue on escape down, bringing escape to front.  May do something
		// with shift/ctrl/alt and F-keys, later.
		if (event == HostKeyEvent::Type::KeyPressed && key == VK_ESCAPE)
		{
			nextQueuedEvent = lastQueuedEvent;
		}

		queuedEvents[lastQueuedEvent].key = key;
		queuedEvents[lastQueuedEvent].type = event;

		lastQueuedEvent = (lastQueuedEvent + 1) % EVENT_QUEUE_LEN;
		// If queue wrapped around, remove last element.
		if (nextQueuedEvent == lastQueuedEvent)
		{
			nextQueuedEvent = (nextQueuedEvent + 1) % EVENT_QUEUE_LEN;
		}
	}
}

int GetQueuedKeyEvent(HostKeyEvent* event)
{
	if (lastQueuedEvent == nextQueuedEvent)
		return 0;

	std::lock_guard<std::mutex> lock(cSection);

	*event = queuedEvents[nextQueuedEvent];
	nextQueuedEvent = (nextQueuedEvent + 1) % EVENT_QUEUE_LEN;
	return 1;
}

void ClearKeyQueue()
{
	lastQueuedEvent = nextQueuedEvent;
}
