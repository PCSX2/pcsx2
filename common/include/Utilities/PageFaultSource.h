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

    virtual void DispatchEvent(const PageFaultInfo &evtinfo, bool &handled)
    {
        OnPageFaultEvent(evtinfo, handled);
    }

    virtual void DispatchEvent(const PageFaultInfo &evtinfo)
    {
        pxFailRel("Don't call me, damnit.  Use DispatchException instead.");
    }

    virtual void OnPageFaultEvent(const PageFaultInfo &evtinfo, bool &handled) {}
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
    TypeToDispatchTo *Owner;

public:
    EventListenerHelper_PageFault(TypeToDispatchTo &dispatchTo)
    {
        Owner = &dispatchTo;
    }

    EventListenerHelper_PageFault(TypeToDispatchTo *dispatchTo)
    {
        Owner = dispatchTo;
    }

    virtual ~EventListenerHelper_PageFault() = default;

protected:
    virtual void OnPageFaultEvent(const PageFaultInfo &info, bool &handled)
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
    virtual void Dispatch(const PageFaultInfo &params);

protected:
    virtual void _DispatchRaw(ListenerIterator iter, const ListenerIterator &iend, const PageFaultInfo &evt);
};


// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve
// --------------------------------------------------------------------------------------
class VirtualMemoryReserve
{
    DeclareNoncopyableObject(VirtualMemoryReserve);

protected:
    wxString m_name;

    // Default size of the reserve, in bytes.  Can be specified when the object is constructed.
    // Is used as the reserve size when Reserve() is called, unless an override is specified
    // in the Reserve parameters.
    size_t m_defsize;

    void *m_baseptr;

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

public:
    VirtualMemoryReserve(const wxString &name = wxEmptyString, size_t size = 0);
    virtual ~VirtualMemoryReserve()
    {
        Release();
    }

    virtual void *Reserve(size_t size = 0, uptr base = 0, uptr upper_bounds = 0);
    virtual void *ReserveAt(uptr base = 0, uptr upper_bounds = 0)
    {
        return Reserve(m_defsize, base, upper_bounds);
    }

    virtual void Reset();
    virtual void Release();
    virtual bool TryResize(uint newsize);
    virtual bool Commit();

    virtual void ForbidModification();
    virtual void AllowModification();

    bool IsOk() const { return m_baseptr != NULL; }
    wxString GetName() const { return m_name; }

    uptr GetReserveSizeInBytes() const { return m_pages_reserved * __pagesize; }
    uptr GetReserveSizeInPages() const { return m_pages_reserved; }
    uint GetCommittedPageCount() const { return m_pages_commited; }
    uint GetCommittedBytes() const { return m_pages_commited * __pagesize; }

    u8 *GetPtr() { return (u8 *)m_baseptr; }
    const u8 *GetPtr() const { return (u8 *)m_baseptr; }
    u8 *GetPtrEnd() { return (u8 *)m_baseptr + (m_pages_reserved * __pagesize); }
    const u8 *GetPtrEnd() const { return (u8 *)m_baseptr + (m_pages_reserved * __pagesize); }

    VirtualMemoryReserve &SetName(const wxString &newname);
    VirtualMemoryReserve &SetBaseAddr(uptr newaddr);
    VirtualMemoryReserve &SetPageAccessOnCommit(const PageProtectionMode &mode);

    operator void *() { return m_baseptr; }
    operator const void *() const { return m_baseptr; }

    operator u8 *() { return (u8 *)m_baseptr; }
    operator const u8 *() const { return (u8 *)m_baseptr; }

    u8 &operator[](uint idx)
    {
        pxAssert(idx < (m_pages_reserved * __pagesize));
        return *((u8 *)m_baseptr + idx);
    }

    const u8 &operator[](uint idx) const
    {
        pxAssert(idx < (m_pages_reserved * __pagesize));
        return *((u8 *)m_baseptr + idx);
    }

protected:
    virtual void ReprotectCommittedBlocks(const PageProtectionMode &newmode);
};

#ifdef __POSIX__

#define PCSX2_PAGEFAULT_PROTECT
#define PCSX2_PAGEFAULT_EXCEPT

#elif defined(_WIN32)

struct _EXCEPTION_POINTERS;
extern int SysPageFaultExceptionFilter(struct _EXCEPTION_POINTERS *eps);

#define PCSX2_PAGEFAULT_PROTECT __try
#define PCSX2_PAGEFAULT_EXCEPT \
    __except (SysPageFaultExceptionFilter(GetExceptionInformation())) {}

#else
#error PCSX2 - Unsupported operating system platform.
#endif

extern void pxInstallSignalHandler();
extern void _platform_InstallSignalHandler();

#include "Threading.h"
extern SrcType_PageFault *Source_PageFault;
extern Threading::Mutex PageFault_Mutex;
