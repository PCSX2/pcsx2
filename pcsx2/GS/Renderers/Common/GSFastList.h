/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS/GS_types.h"

template <class T>
struct Element
{
	T data;
	uint16 next_index;
	uint16 prev_index;
};

template <class T>
class FastListIterator;

template <class T>
class FastList
{
	friend class FastListIterator<T>;

private:
	// The index of the first element of the list is m_buffer[0].next_index
	//     The first Element<T> of the list has prev_index equal to 0
	// The index of the last element of the list is m_buffer[0].prev_index
	//     The last Element<T> of the list has next_index equal to 0
	// All the other Element<T> of the list are chained by next_index and prev_index
	// m_buffer has dynamic size m_capacity
	// Due to m_buffer reallocation, the pointers to Element<T> stored into the array
	//     are invalidated every time Grow() is executed. But FastListIterator<T> is
	//     index based, not pointer based, and the elements are copied in order on Grow(),
	//     so there is no iterator invalidation (which is an index invalidation) until
	//     the relevant iterator (or the index alone) are erased from the list.
	// m_buffer[0] is always present as auxiliary Element<T> of the list
	Element<T>* m_buffer;
	uint16 m_capacity;
	uint16 m_free_indexes_stack_top;
	// m_free_indexes_stack has dynamic size (m_capacity - 1)
	// m_buffer indexes that are free to be used are stacked here
	uint16* m_free_indexes_stack;

public:
	__forceinline FastList()
	{
		m_buffer = nullptr;
		clear();
	}

	__forceinline ~FastList()
	{
		_aligned_free(m_buffer);
	}

	void clear()
	{
		// Initialize m_capacity to 4 so we avoid to Grow() on initial insertions
		// The code doesn't break if this value is changed with anything from 1 to USHRT_MAX
		m_capacity = 4;

		// Initialize m_buffer and m_free_indexes_stack as a contiguous block of memory starting at m_buffer
		// This should increase cache locality and reduce memory fragmentation
		_aligned_free(m_buffer);
		m_buffer = (Element<T>*)_aligned_malloc(m_capacity * sizeof(Element<T>) + (m_capacity - 1) * sizeof(uint16), 64);
		m_free_indexes_stack = (uint16*)&m_buffer[m_capacity];

		// Initialize m_buffer[0], data field is unused but initialized using default T constructor
		m_buffer[0] = {T(), 0, 0};

		// m_free_indexes_stack top index is 0, bottom index is m_capacity - 2
		m_free_indexes_stack_top = 0;

		// m_buffer index 0 is reserved for auxiliary element
		for (uint16 i = 0; i < m_capacity - 1; i++)
		{
			m_free_indexes_stack[i] = i + 1;
		}
	}

	// Insert the element in front of the list and return its position in m_buffer
	__forceinline uint16 InsertFront(const T& data)
	{
		if (Full())
		{
			Grow();
		}

		// Pop a free index from the stack
		const uint16 free_index = m_free_indexes_stack[m_free_indexes_stack_top++];
		m_buffer[free_index].data = data;
		ListInsertFront(free_index);
		return free_index;
	}

	__forceinline void push_front(const T& data)
	{
		InsertFront(data);
	}

	__forceinline const T& back() const
	{
		return m_buffer[LastIndex()].data;
	}

	__forceinline void pop_back()
	{
		EraseIndex(LastIndex());
	}

	__forceinline uint16 size() const
	{
		return m_free_indexes_stack_top;
	}

	__forceinline bool empty() const
	{
		return size() == 0;
	}

	__forceinline void EraseIndex(const uint16 index)
	{
		ListRemove(index);
		m_free_indexes_stack[--m_free_indexes_stack_top] = index;
	}

	__forceinline void MoveFront(const uint16 index)
	{
		if (FirstIndex() != index)
		{
			ListRemove(index);
			ListInsertFront(index);
		}
	}

	__forceinline const FastListIterator<T> begin() const
	{
		return FastListIterator<T>(this, FirstIndex());
	}

	__forceinline const FastListIterator<T> end() const
	{
		return FastListIterator<T>(this, 0);
	}

	__forceinline FastListIterator<T> erase(FastListIterator<T> i)
	{
		EraseIndex(i.Index());
		return ++i;
	}

private:
	// Accessed by FastListIterator<T> using class friendship
	__forceinline const T& Data(const uint16 index) const
	{
		return m_buffer[index].data;
	}

	// Accessed by FastListIterator<T> using class friendship
	__forceinline uint16 NextIndex(const uint16 index) const
	{
		return m_buffer[index].next_index;
	}

	// Accessed by FastListIterator<T> using class friendship
	__forceinline uint16 PrevIndex(const uint16 index) const
	{
		return m_buffer[index].prev_index;
	}

	__forceinline uint16 FirstIndex() const
	{
		return m_buffer[0].next_index;
	}

	__forceinline uint16 LastIndex() const
	{
		return m_buffer[0].prev_index;
	}

	__forceinline bool Full() const
	{
		// The minus one is due to the presence of the auxiliary element
		return size() == m_capacity - 1;
	}

	__forceinline void ListInsertFront(const uint16 index)
	{
		// Update prev / next indexes to add m_buffer[index] to the chain
		Element<T>& head = m_buffer[0];
		m_buffer[index].prev_index = 0;
		m_buffer[index].next_index = head.next_index;
		m_buffer[head.next_index].prev_index = index;
		head.next_index = index;
	}

	__forceinline void ListRemove(const uint16 index)
	{
		// Update prev / next indexes to remove m_buffer[index] from the chain
		const Element<T>& to_remove = m_buffer[index];
		m_buffer[to_remove.prev_index].next_index = to_remove.next_index;
		m_buffer[to_remove.next_index].prev_index = to_remove.prev_index;
	}

	void Grow()
	{
		if (m_capacity == USHRT_MAX)
		{
			throw std::runtime_error("FastList size maxed out at USHRT_MAX (65535) elements, cannot grow futhermore.");
		}

		const uint16 new_capacity = m_capacity <= (USHRT_MAX / 2) ? (m_capacity * 2) : USHRT_MAX;

		Element<T>* new_buffer = (Element<T>*)_aligned_malloc(new_capacity * sizeof(Element<T>) + (new_capacity - 1) * sizeof(uint16), 64);
		uint16* new_free_indexes_stack = (uint16*)&new_buffer[new_capacity];

		memcpy(new_buffer, m_buffer, m_capacity * sizeof(Element<T>));
		memcpy(new_free_indexes_stack, m_free_indexes_stack, (m_capacity - 1) * sizeof(uint16));

		_aligned_free(m_buffer);

		m_buffer = new_buffer;
		m_free_indexes_stack = new_free_indexes_stack;

		// Initialize the additional space in the stack
		for (uint16 i = m_capacity - 1; i < new_capacity - 1; i++)
		{
			m_free_indexes_stack[i] = i + 1;
		}

		m_capacity = new_capacity;
	}
};


template <class T>
// This iterator is const_iterator
class FastListIterator
{
private:
	const FastList<T>* m_fastlist;
	uint16 m_index;

public:
	__forceinline FastListIterator(const FastList<T>* fastlist, const uint16 index)
	{
		m_fastlist = fastlist;
		m_index = index;
	}

	__forceinline bool operator!=(const FastListIterator<T>& other) const
	{
		return (m_index != other.m_index);
	}

	__forceinline bool operator==(const FastListIterator<T>& other) const
	{
		return (m_index == other.m_index);
	}

	// Prefix increment
	__forceinline const FastListIterator<T>& operator++()
	{
		m_index = m_fastlist->NextIndex(m_index);
		return *this;
	}

	// Postfix increment
	__forceinline const FastListIterator<T> operator++(int)
	{
		FastListIterator<T> copy(*this);
		++(*this);
		return copy;
	}

	// Prefix decrement
	__forceinline const FastListIterator<T>& operator--()
	{
		m_index = m_fastlist->PrevIndex(m_index);
		return *this;
	}

	// Postfix decrement
	__forceinline const FastListIterator<T> operator--(int)
	{
		FastListIterator<T> copy(*this);
		--(*this);
		return copy;
	}

	__forceinline const T& operator*() const
	{
		return m_fastlist->Data(m_index);
	}

	__forceinline uint16 Index() const
	{
		return m_index;
	}
};
