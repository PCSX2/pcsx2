/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#pragma once

#include <atomic>

#include "common/Assertions.h"

//Designed to allow one thread to queue data to another thread
template <class T>
class SimpleQueue
{
private:
	struct SimpleQueueEntry
	{
		std::atomic_bool ready{false};
		SimpleQueueEntry* next;
		T value;
	};

	std::atomic<SimpleQueueEntry*> head{nullptr};
	SimpleQueueEntry* tail = nullptr;

public:
	SimpleQueue();

	//Used by single queue thread (i.e. EE)
	void Enqueue(T entry);
	//Used by single worker thread (i.e. IO)
	bool Dequeue(T* entry);
	//May return false negative when another thread is mid Queue()
	//Intended to only be used from queue thread
	bool IsQueueEmpty();

	~SimpleQueue();
};

template <class T>
SimpleQueue<T>::SimpleQueue()
{
	tail = new SimpleQueueEntry();
	head.store(tail);
}

template <class T>
void SimpleQueue<T>::Enqueue(T entry)
{
	//Allocate Next entry, and assign to head
	SimpleQueueEntry* newHead = new SimpleQueueEntry();
	SimpleQueueEntry* newEntry = head.exchange(newHead);

	//Fill in
	newEntry->value = entry;
	newEntry->next = newHead;

	//Set ready (can be dequeued)
	newEntry->ready.store(true);
}

template <class T>
bool SimpleQueue<T>::Dequeue(T* entry)
{
	if (!tail->ready.load())
		return false;

	SimpleQueueEntry* retEntry = tail;
	tail = retEntry->next;

	*entry = retEntry->value;
	delete retEntry;
	return true;
}

//Note, next entry may not be ready to dequeue
template <class T>
bool SimpleQueue<T>::IsQueueEmpty()
{
	return head.load() == tail;
}

template <class T>
SimpleQueue<T>::~SimpleQueue()
{
	if (head != nullptr)
	{
		if (!IsQueueEmpty())
		{
			Console.Error("DEV9: Queue not empty");
			pxAssert(false);

			//Empty Queue
			T entry;
			while (!IsQueueEmpty())
				Dequeue(&entry);
		}

		delete head;
		head = nullptr;
		tail = nullptr;
	}
}
