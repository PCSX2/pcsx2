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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>

/// A ring buffer pretending to be a heap (screams if you don't actually use it like a ring buffer)
/// Meant for one producer thread creating data and sharing it with multiple consumer threads
/// Expectations:
/// - One thread allocates and writes to allocations
/// - Other threads read from allocations (once shared, no one writes)
/// - Any thread can free
/// - Frees are done in approximately the same order as allocations (but not exactly the same order)
class GSRingHeap
{
	struct Buffer;
	Buffer* m_current_buffer;

	void orphanBuffer() noexcept;
	/// Allocate a value of `size` bytes with `prefix_size` bytes before it (for allocation tracking) and alignment specified by `align_mask`
	void* alloc_internal(size_t size, size_t align_mask, size_t prefix_size);
	/// Free a value of size `size` (equal to prefix_size + size when allocated)
	static void free_internal(void* ptr, size_t size) noexcept;

	static constexpr size_t MIN_ALIGN = std::max(alignof(size_t), alignof(void*));

	static size_t getAlignMask(size_t align)
	{
		return std::max(MIN_ALIGN, align) - 1;
	}

public:
	GSRingHeap(GSRingHeap&&) = delete;
	GSRingHeap();
	~GSRingHeap() noexcept;

	/// Allocate a piece of memory with the given size and alignment
	void* alloc(size_t size, size_t align)
	{
		size_t alloc_size = size + sizeof(size_t);
		void* ptr = alloc_internal(size, getAlignMask(align), sizeof(size_t));
		size_t* header = static_cast<size_t*>(ptr);
		*header = alloc_size;
		return static_cast<void*>(header + 1);
	};

	/// Allocate and initialize a T*
	template <typename T, typename... Args>
	T* make(Args&&... args)
	{
		std::unique_ptr<void, void(*)(void*)> ptr(alloc(sizeof(T), alignof(T)), GSRingHeap::free);
		new (ptr.get()) T(std::forward<Args>(args)...);
		return static_cast<T*>(ptr.release());
	}

	/// Allocate and default-initialize `count` `T`s
	template <typename T>
	T* make_array(size_t count)
	{
		std::unique_ptr<void, void(*)(void*)> ptr(alloc(sizeof(T) * count, alignof(T)), GSRingHeap::free);
		new (ptr.get()) T[count]();
		return static_cast<T*>(ptr.release());
	}

	/// Free a pointer allocated with `alloc`
	static void free(void* ptr)
	{
		size_t* header = static_cast<size_t*>(ptr) - 1;
		free_internal(static_cast<void*>(header), *header);
	}

	/// Deinitialize and free a pointer created with `make`
	template <typename T>
	static void destroy(T* ptr)
	{
		ptr->~T();
		free(ptr);
	}

	/// Deinitialize and free an array allocated with `make_array`
	template <typename T>
	static void destroy_array(T* ptr)
	{
		size_t* header = const_cast<size_t*>(reinterpret_cast<const size_t*>(ptr)) - 1;
		size_t size = (*header - sizeof(size_t)) / sizeof(T);
		for (size_t i = 0; i < size; i++)
			ptr[i].~T();
		free(ptr);
	}

	/// Like `std::shared_ptr` but holds a pointer on this allocator
	template <typename T>
	class SharedPtr
	{
		friend class GSRingHeap;

		struct alignas(MIN_ALIGN) AllocationHeader
		{
			uint32_t size;
			std::atomic<uint32_t> refcnt;
		};

		T* m_ptr;

		SharedPtr(T* ptr)
			: m_ptr(ptr)
		{
		}

		AllocationHeader* getHeader()
		{
			return const_cast<AllocationHeader*>(reinterpret_cast<const AllocationHeader*>(m_ptr)) - 1;
		}

	public:
		SharedPtr()
			: m_ptr(nullptr)
		{
		}

		SharedPtr(std::nullptr_t)
			: m_ptr(nullptr)
		{
		}

		SharedPtr(const SharedPtr& other)
			: m_ptr(other.m_ptr)
		{
			if (m_ptr)
				getHeader()->refcnt.fetch_add(1, std::memory_order_relaxed);
		}

		SharedPtr(SharedPtr&& other)
			: m_ptr(other.m_ptr)
		{
			other.m_ptr = nullptr;
		}

		SharedPtr& operator=(const SharedPtr& other)
		{
			this->~SharedPtr();
			new (this) SharedPtr(other);
			return *this;
		}

		SharedPtr& operator=(SharedPtr&& other)
		{
			this->~SharedPtr();
			new (this) SharedPtr(other);
			return *this;
		}

		~SharedPtr()
		{
			if (!m_ptr)
				return;
			AllocationHeader* header = getHeader();
			// (See top) Expectation: Once shared, no one writes
			// Therefore we don't need acquire/release semantics here
			if (header->refcnt.fetch_sub(1, std::memory_order_relaxed) == 1)
			{
				m_ptr->~T();
				free_internal(static_cast<void*>(header), header->size);
			}
		}

		T& operator*() const { return *m_ptr; }
		T* operator->() const { return m_ptr; }
		T* get() const { return m_ptr; }

		/// static_cast the pointer to another type
		template <typename Other>
		SharedPtr<Other> cast() const&
		{
			getHeader().refcount.fetch_add(1, std::memory_order_relaxed);
			return SharedPtr<Other>(static_cast<Other*>(m_ptr));
		}

		/// static_cast the pointer to another type
		template <typename Other>
		SharedPtr<Other> cast() &&
		{
			SharedPtr<Other> other(static_cast<Other*>(m_ptr));
			m_ptr = nullptr;
			return other;
		}
	};

	/// Make a shared pointer with a different alignment from what the type would normally expect
	template <typename T, typename... Args>
	SharedPtr<T> make_shared(Args&&... args)
	{
		using Header = typename SharedPtr<T>::AllocationHeader;
		static constexpr size_t alloc_size = sizeof(T) + sizeof(Header);
		static_assert(alignof(Header) <= MIN_ALIGN, "Header alignment too high");
		static_assert(alloc_size <= UINT32_MAX, "Allocation overflow");

		void* ptr = alloc_internal(sizeof(T), getAlignMask(alignof(T)), sizeof(Header));
		std::unique_ptr<void, void(*)(void*)> guard(ptr, [](void* p){ free_internal(p, alloc_size); });
		Header* header = static_cast<Header*>(ptr);
		header->size = static_cast<uint32_t>(alloc_size);
		header->refcnt.store(1, std::memory_order_relaxed);

		T* tptr = reinterpret_cast<T*>(header + 1);
		new (tptr) T(std::forward<Args>(args)...);
		guard.release();
		return SharedPtr<T>(tptr);
	}

	template <typename T>
	struct Deleter
	{
		void operator()(T* t)
		{
			if (t)
				destroy(t);
		}
	};

	template <typename T>
	struct Deleter<T[]>
	{
		void operator()(T* t)
		{
			if (t)
				destroy_array(t);
		}
	};

	template <typename T>
	using UniquePtr = std::unique_ptr<T, Deleter<T>>;

	template <typename T>
	struct _unique_if
	{
		typedef UniquePtr<T> _unique_single;
	};

	template <typename T>
	struct _unique_if<T[]>
	{
		typedef UniquePtr<T[]> _unique_array_unknown_bound;
	};

	template <typename T, size_t N>
	struct _unique_if<T[N]>
	{
		typedef void _unique_array_known_bound;
	};

	template <typename T, typename... Args>
	typename _unique_if<T>::_unique_single make_unique(Args&&... args)
	{
		return UniquePtr<T>(make<T>(std::forward<Args>(args)...));
	}

	template <typename T>
	typename _unique_if<T>::_unique_array_unknown_bound make_unique(size_t count)
	{
		typedef typename std::remove_extent<T>::type Base;
		return UniquePtr<T>(make_array<Base>(count));
	}

	template <class T, class... _Args>
	typename _unique_if<T>::_unique_array_known_bound make_unique(_Args&&...) = delete;
};
