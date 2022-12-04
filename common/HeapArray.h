/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include <algorithm>
#include <cassert>
#include <type_traits>

template <typename T, std::size_t SIZE>
class HeapArray
{
public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using this_type = HeapArray<T, SIZE>;

	HeapArray() { m_data = new T[SIZE]; }

	HeapArray(const this_type& copy)
	{
		m_data = new T[SIZE];
		std::copy(copy.cbegin(), copy.cend(), begin());
	}

	HeapArray(this_type&& move)
	{
		m_data = move.m_data;
		move.m_data = nullptr;
	}

	~HeapArray() { delete[] m_data; }

	size_type size() const { return SIZE; }
	size_type capacity() const { return SIZE; }
	bool empty() const { return false; }

	pointer begin() { return m_data; }
	pointer end() { return m_data + SIZE; }

	const_pointer data() const { return m_data; }
	pointer data() { return m_data; }

	const_pointer cbegin() const { return m_data; }
	const_pointer cend() const { return m_data + SIZE; }

	const_reference operator[](size_type index) const
	{
		assert(index < SIZE);
		return m_data[index];
	}
	reference operator[](size_type index)
	{
		assert(index < SIZE);
		return m_data[index];
	}

	const_reference front() const { return m_data[0]; }
	const_reference back() const { return m_data[SIZE - 1]; }
	reference front() { return m_data[0]; }
	reference back() { return m_data[SIZE - 1]; }

	void fill(const_reference value) { std::fill(begin(), end(), value); }

	void swap(this_type& move) { std::swap(m_data, move.m_data); }

	this_type& operator=(const this_type& rhs)
	{
		std::copy(begin(), end(), rhs.cbegin());
		return *this;
	}

	this_type& operator=(this_type&& move)
	{
		delete[] m_data;
		m_data = move.m_data;
		move.m_data = nullptr;
		return *this;
	}

#define RELATIONAL_OPERATOR(op) \
	bool operator op(const this_type& rhs) const \
	{ \
		for (size_type i = 0; i < SIZE; i++) \
		{ \
			if (!(m_data[i] op rhs.m_data[i])) \
				return false; \
		} \
	}

	RELATIONAL_OPERATOR(==);
	RELATIONAL_OPERATOR(!=);
	RELATIONAL_OPERATOR(<);
	RELATIONAL_OPERATOR(<=);
	RELATIONAL_OPERATOR(>);
	RELATIONAL_OPERATOR(>=);

#undef RELATIONAL_OPERATOR

private:
	T* m_data;
};
