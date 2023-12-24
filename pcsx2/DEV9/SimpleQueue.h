// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <atomic>

#include "common/Assertions.h"
#include "common/Console.h"

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
