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

// What MS calls a single process Mutex.  Faster, supposedly.
// More importantly, can be abbreviated, amusingly, as cSection.
#ifdef _MSC_VER
static CRITICAL_SECTION cSection;
static u8 csInitialized = 0;
#else
static std::mutex cSection;
#endif

#define EVENT_QUEUE_LEN 16
// Actually points one beyond the last queued event.
static u8 lastQueuedEvent = 0;
static u8 nextQueuedEvent = 0;
static keyEvent queuedEvents[EVENT_QUEUE_LEN];

void QueueKeyEvent(int key, int event)
{
#ifdef _MSC_VER
	if (!csInitialized)
	{
		csInitialized = 1;
		InitializeCriticalSection(&cSection);
	}
	EnterCriticalSection(&cSection);
#else
	std::lock_guard<std::mutex> lock(cSection);
#endif

	// Don't queue events if escape is on top of queue.  This is just for safety
	// purposes when a game is killing the emulator for whatever reason.
	if (nextQueuedEvent == lastQueuedEvent ||
		queuedEvents[nextQueuedEvent].key != VK_ESCAPE ||
		queuedEvents[nextQueuedEvent].evt != KEYPRESS)
	{
		// Clear queue on escape down, bringing escape to front.  May do something
		// with shift/ctrl/alt and F-keys, later.
		if (event == KEYPRESS && key == VK_ESCAPE)
		{
			nextQueuedEvent = lastQueuedEvent;
		}

		queuedEvents[lastQueuedEvent].key = key;
		queuedEvents[lastQueuedEvent].evt = event;

		lastQueuedEvent = (lastQueuedEvent + 1) % EVENT_QUEUE_LEN;
		// If queue wrapped around, remove last element.
		if (nextQueuedEvent == lastQueuedEvent)
		{
			nextQueuedEvent = (nextQueuedEvent + 1) % EVENT_QUEUE_LEN;
		}
	}
#ifdef _MSC_VER
	LeaveCriticalSection(&cSection);
#endif
}

int GetQueuedKeyEvent(keyEvent* event)
{
	if (lastQueuedEvent == nextQueuedEvent)
		return 0;

#ifdef _MSC_VER
	EnterCriticalSection(&cSection);
#else
	std::lock_guard<std::mutex> lock(cSection);
#endif
	*event = queuedEvents[nextQueuedEvent];
	nextQueuedEvent = (nextQueuedEvent + 1) % EVENT_QUEUE_LEN;
#ifdef _MSC_VER
	LeaveCriticalSection(&cSection);
#endif
	return 1;
}

void ClearKeyQueue()
{
	lastQueuedEvent = nextQueuedEvent;
#ifdef _MSC_VER
	if (csInitialized)
	{
		DeleteCriticalSection(&cSection);
		csInitialized = 0;
	}
#endif
}
