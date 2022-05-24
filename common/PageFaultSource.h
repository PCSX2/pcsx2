/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

// [TODO] Rename this file to VirtualMemory.h !!

#pragma once

// =====================================================================================================
//  Cross-Platform Memory Protection (Used by VTLB, Recompilers and Texture caches)
// =====================================================================================================
// Win32 platforms use the SEH model: __try {}  __except {}
// Linux platforms use the POSIX Signals model: sigaction()
// [TODO] OS-X (Darwin) platforms should use the Mach exception model (not implemented)

#include "EventSource.h"
#include "General.h"
#include "Assertions.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

struct PageFaultInfo
{
	uptr addr;

	PageFaultInfo(uptr address)
	{
		addr = address;
	}
};

// --------------------------------------------------------------------------------------
//  IEventListener_PageFault
// --------------------------------------------------------------------------------------
class IEventListener_PageFault : public IEventDispatcher<PageFaultInfo>
{
public:
	typedef PageFaultInfo EvtParams;

public:
	virtual ~IEventListener_PageFault() = default;

	virtual void DispatchEvent(const PageFaultInfo& evtinfo, bool& handled)
	{
		OnPageFaultEvent(evtinfo, handled);
	}

	virtual void DispatchEvent(const PageFaultInfo& evtinfo)
	{
		pxFailRel("Don't call me, damnit.  Use DispatchException instead.");
	}

	virtual void OnPageFaultEvent(const PageFaultInfo& evtinfo, bool& handled) {}
};

// --------------------------------------------------------------------------------------
//  EventListener_PageFault / EventListenerHelper_PageFault
// --------------------------------------------------------------------------------------
class EventListener_PageFault : public IEventListener_PageFault
{
public:
	EventListener_PageFault();
	virtual ~EventListener_PageFault();
};

template <typename TypeToDispatchTo>
class EventListenerHelper_PageFault : public EventListener_PageFault
{
public:
	TypeToDispatchTo* Owner;

public:
	EventListenerHelper_PageFault(TypeToDispatchTo& dispatchTo)
	{
		Owner = &dispatchTo;
	}

	EventListenerHelper_PageFault(TypeToDispatchTo* dispatchTo)
	{
		Owner = dispatchTo;
	}

	virtual ~EventListenerHelper_PageFault() = default;

protected:
	virtual void OnPageFaultEvent(const PageFaultInfo& info, bool& handled)
	{
		Owner->OnPageFaultEvent(info, handled);
	}
};

// --------------------------------------------------------------------------------------
//  SrcType_PageFault
// --------------------------------------------------------------------------------------
class SrcType_PageFault : public EventSource<IEventListener_PageFault>
{
protected:
	typedef EventSource<IEventListener_PageFault> _parent;

protected:
	bool m_handled;

public:
	SrcType_PageFault()
		: m_handled(false)
	{
	}
	virtual ~SrcType_PageFault() = default;

	bool WasHandled() const { return m_handled; }
	virtual void Dispatch(const PageFaultInfo& params);

protected:
	virtual void _DispatchRaw(ListenerIterator iter, const ListenerIterator& iend, const PageFaultInfo& evt);
};


// --------------------------------------------------------------------------------------
//  VirtualMemoryManager: Manages the allocation of PCSX2 VM
//    Ensures that all memory is close enough together for rip-relative addressing
// --------------------------------------------------------------------------------------
class VirtualMemoryManager
{
	DeclareNoncopyableObject(VirtualMemoryManager);

	std::string m_name;

	uptr m_baseptr;

	// An array to track page usage (to trigger asserts if things try to overlap)
	std::atomic<bool>* m_pageuse;

	// reserved memory (in pages)
	u32 m_pages_reserved;

public:
	// If upper_bounds is nonzero and the OS fails to allocate memory that is below it,
	// calls to IsOk() will return false and Alloc() will always return null pointers
	// strict indicates that the allocation should quietly fail if the memory can't be mapped at `base`
	VirtualMemoryManager(std::string name, uptr base, size_t size, uptr upper_bounds = 0, bool strict = false);
	~VirtualMemoryManager();

	void* GetBase() const { return (void*)m_baseptr; }

	// Request the use of the memory at offsetLocation bytes from the start of the reserved memory area
	// offsetLocation must be page-aligned
	void* Alloc(uptr offsetLocation, size_t size) const;

	void* AllocAtAddress(void* address, size_t size) const
	{
		return Alloc(size, (uptr)address - m_baseptr);
	}

	void Free(void* address, size_t size) const;

	// Was this VirtualMemoryManager successfully able to get its memory mapping?
	// (If not, calls to Alloc will return null pointers)
	bool IsOk() const { return m_baseptr != 0; }
};

typedef std::shared_ptr<const VirtualMemoryManager> VirtualMemoryManagerPtr;

// --------------------------------------------------------------------------------------
//  VirtualMemoryBumpAllocator: Allocates memory for things that don't have explicitly-reserved spots
// --------------------------------------------------------------------------------------
class VirtualMemoryBumpAllocator
{
	const VirtualMemoryManagerPtr m_allocator;
	std::atomic<uptr> m_baseptr{0};
	const uptr m_endptr = 0;

public:
	VirtualMemoryBumpAllocator(VirtualMemoryManagerPtr allocator, size_t size, uptr offsetLocation);
	void* Alloc(size_t size);
	const VirtualMemoryManagerPtr& GetAllocator() { return m_allocator; }
};

// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve
// --------------------------------------------------------------------------------------
class VirtualMemoryReserve
{
	DeclareNoncopyableObject(VirtualMemoryReserve);

protected:
	std::string m_name;

	// Where the memory came from (so we can return it)
	VirtualMemoryManagerPtr m_allocator;

	// Default size of the reserve, in bytes.  Can be specified when the object is constructed.
	// Is used as the reserve size when Reserve() is called, unless an override is specified
	// in the Reserve parameters.
	size_t m_defsize;

	void* m_baseptr;

	// reserved memory (in pages).
	uptr m_pages_reserved;

	// Records the number of pages committed to memory.
	// (metric for analysis of buffer usage)
	uptr m_pages_commited;

	// Protection mode to be applied to committed blocks.
	PageProtectionMode m_prot_mode;

	// Controls write access to the entire reserve.  When true (the default), the reserve
	// operates normally.  When set to false, all committed blocks are re-protected with
	// write disabled, and accesses to uncommitted blocks (read or write) will cause a GPF
	// as well.
	bool m_allow_writes;

	// Allows the implementation to decide how much memory it needs to allocate if someone requests the given size
	// Should translate requests of size 0 to m_defsize
	virtual size_t GetSize(size_t requestedSize);

public:
	VirtualMemoryReserve(std::string name, size_t size = 0);
	virtual ~VirtualMemoryReserve()
	{
		Release();
	}

	// Initialize with the given piece of memory
	// Note: The memory is already allocated, the allocator is for future use to free the region
	// It may be null in which case there is no way to free the memory in a way it will be usable again
	virtual void* Assign(VirtualMemoryManagerPtr allocator, void* baseptr, size_t size);

	void* Reserve(VirtualMemoryManagerPtr allocator, uptr baseOffset, size_t size = 0)
	{
		size = GetSize(size);
		void* allocation = allocator->Alloc(baseOffset, size);
		return Assign(std::move(allocator), allocation, size);
	}
	void* Reserve(VirtualMemoryBumpAllocator& allocator, size_t size = 0)
	{
		size = GetSize(size);
		return Assign(allocator.GetAllocator(), allocator.Alloc(size), size);
	}

	virtual void Reset();
	virtual void Release();
	virtual bool TryResize(uint newsize);
	virtual bool Commit();

	virtual void ForbidModification();
	virtual void AllowModification();

	bool IsOk() const { return m_baseptr != NULL; }
	const std::string& GetName() const { return m_name; }

	uptr GetReserveSizeInBytes() const { return m_pages_reserved * __pagesize; }
	uptr GetReserveSizeInPages() const { return m_pages_reserved; }
	uint GetCommittedPageCount() const { return m_pages_commited; }
	uint GetCommittedBytes() const { return m_pages_commited * __pagesize; }

	u8* GetPtr() { return (u8*)m_baseptr; }
	const u8* GetPtr() const { return (u8*)m_baseptr; }
	u8* GetPtrEnd() { return (u8*)m_baseptr + (m_pages_reserved * __pagesize); }
	const u8* GetPtrEnd() const { return (u8*)m_baseptr + (m_pages_reserved * __pagesize); }

	VirtualMemoryReserve& SetPageAccessOnCommit(const PageProtectionMode& mode);

	operator void*() { return m_baseptr; }
	operator const void*() const { return m_baseptr; }

	operator u8*() { return (u8*)m_baseptr; }
	operator const u8*() const { return (u8*)m_baseptr; }

	u8& operator[](uint idx)
	{
		pxAssert(idx < (m_pages_reserved * __pagesize));
		return *((u8*)m_baseptr + idx);
	}

	const u8& operator[](uint idx) const
	{
		pxAssert(idx < (m_pages_reserved * __pagesize));
		return *((u8*)m_baseptr + idx);
	}

protected:
	virtual void ReprotectCommittedBlocks(const PageProtectionMode& newmode);
};

#ifdef __POSIX__

#define PCSX2_PAGEFAULT_PROTECT
#define PCSX2_PAGEFAULT_EXCEPT

#elif defined(_WIN32)

struct _EXCEPTION_POINTERS;
extern long __stdcall SysPageFaultExceptionFilter(struct _EXCEPTION_POINTERS* eps);

#define PCSX2_PAGEFAULT_PROTECT __try
#define PCSX2_PAGEFAULT_EXCEPT \
	__except (SysPageFaultExceptionFilter(GetExceptionInformation())) {}

#else
#error PCSX2 - Unsupported operating system platform.
#endif

extern void pxInstallSignalHandler();
extern void _platform_InstallSignalHandler();

extern SrcType_PageFault* Source_PageFault;
extern std::mutex PageFault_Mutex;
