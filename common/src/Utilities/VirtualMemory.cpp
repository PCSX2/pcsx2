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

#include "PrecompiledHeader.h"
#include "PageFaultSource.h"

#ifndef __WXMSW__
#include <wx/thread.h>
#endif

#include "EventSource.inl"
#include "MemsetFast.inl"

template class EventSource<IEventListener_PageFault>;

SrcType_PageFault *Source_PageFault = NULL;
Threading::Mutex PageFault_Mutex;

void pxInstallSignalHandler()
{
    if (!Source_PageFault) {
        Source_PageFault = new SrcType_PageFault();
    }

    _platform_InstallSignalHandler();

    // NOP on Win32 systems -- we use __try{} __except{} instead.
}

// --------------------------------------------------------------------------------------
//  EventListener_PageFault  (implementations)
// --------------------------------------------------------------------------------------
EventListener_PageFault::EventListener_PageFault()
{
    pxAssert(Source_PageFault);
    Source_PageFault->Add(*this);
}

EventListener_PageFault::~EventListener_PageFault()
{
    if (Source_PageFault)
        Source_PageFault->Remove(*this);
}

void SrcType_PageFault::Dispatch(const PageFaultInfo &params)
{
    m_handled = false;
    _parent::Dispatch(params);
}

void SrcType_PageFault::_DispatchRaw(ListenerIterator iter, const ListenerIterator &iend, const PageFaultInfo &evt)
{
    do {
        (*iter)->DispatchEvent(evt, m_handled);
    } while ((++iter != iend) && !m_handled);
}

static size_t pageAlign(size_t size)
{
    return (size + __pagesize - 1) / __pagesize * __pagesize;
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryBumpAllocator  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryBumpAllocator::VirtualMemoryBumpAllocator(void *mem, size_t size)
    : m_baseptr((uptr)mem), m_endptr(m_baseptr + size) {}

void *VirtualMemoryBumpAllocator::Alloc(size_t size)
{
    if (m_baseptr.load() == 0) // True if constructed from bad VirtualMemoryManager (assertion was on initialization)
        return nullptr;

    size_t reservedSize = pageAlign(size);

    uptr out = m_baseptr.fetch_add(reservedSize, std::memory_order_relaxed);

    if (!pxAssertDev(out - reservedSize + size <= m_endptr, "(VirtualMemoryBumpAllocator) ran out of memory"))
        return nullptr;

    return (void *)out;
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryReserve::VirtualMemoryReserve(const wxString &name, size_t size)
    : m_name(name)
{
    m_defsize = size;

    m_pages_commited = 0;
    m_pages_reserved = 0;
    m_baseptr = nullptr;
    m_prot_mode = PageAccess_None();
    m_allow_writes = true;
}

VirtualMemoryReserve &VirtualMemoryReserve::SetPageAccessOnCommit(const PageProtectionMode &mode)
{
    m_prot_mode = mode;
    return *this;
}

// Notes:
//  * This method should be called if the object is already in an released (unreserved) state.
//    Subsequent calls will be ignored, and the existing reserve will be returned.
//
// Parameters:
//   baseptr - the new base pointer that's about to be assigned
//   size - size of the region pointed to by baseptr
//
void *VirtualMemoryReserve::Assign(void * baseptr, size_t size)
{
    if (!pxAssertDev(m_baseptr == NULL, "(VirtualMemoryReserve) Invalid object state; object has already been reserved."))
        return m_baseptr;

    if (!size)
        return nullptr;

    m_baseptr = baseptr;

    uptr reserved_bytes = pageAlign(size);
    m_pages_reserved = reserved_bytes / __pagesize;

    if (!m_baseptr)
        return nullptr;

    FastFormatUnicode mbkb;
    uint mbytes = reserved_bytes / _1mb;
    if (mbytes)
        mbkb.Write("[%umb]", mbytes);
    else
        mbkb.Write("[%ukb]", reserved_bytes / 1024);

    DevCon.WriteLn(Color_Gray, L"%-32s @ %ls -> %ls %ls", WX_STR(m_name),
                   pxsPtr(m_baseptr), pxsPtr((uptr)m_baseptr + reserved_bytes), mbkb.c_str());

    return m_baseptr;
}

void VirtualMemoryReserve::ReprotectCommittedBlocks(const PageProtectionMode &newmode)
{
    if (!m_pages_commited)
        return;
    HostSys::MemProtect(m_baseptr, m_pages_commited * __pagesize, newmode);
}

// Clears all committed blocks, restoring the allocation to a reserve only.
void VirtualMemoryReserve::Reset()
{
    if (!m_pages_commited)
        return;

    ReprotectCommittedBlocks(PageAccess_None());
    HostSys::MmapResetPtr(m_baseptr, m_pages_commited * __pagesize);
    m_pages_commited = 0;
}

bool VirtualMemoryReserve::Commit()
{
    if (!m_pages_reserved)
        return false;
    if (!pxAssert(!m_pages_commited))
        return true;

    bool ok = HostSys::MmapCommitPtr(m_baseptr, m_pages_reserved * __pagesize, m_prot_mode);
    if (ok) { m_pages_commited = m_pages_reserved; }
    return ok;
}

void VirtualMemoryReserve::AllowModification()
{
    m_allow_writes = true;
    HostSys::MemProtect(m_baseptr, m_pages_commited * __pagesize, m_prot_mode);
}

void VirtualMemoryReserve::ForbidModification()
{
    m_allow_writes = false;
    HostSys::MemProtect(m_baseptr, m_pages_commited * __pagesize, PageProtectionMode(m_prot_mode).Write(false));
}

// --------------------------------------------------------------------------------------
//  PageProtectionMode  (implementations)
// --------------------------------------------------------------------------------------
wxString PageProtectionMode::ToString() const
{
    wxString modeStr;

    if (m_read)
        modeStr += L"Read";
    if (m_write)
        modeStr += L"Write";
    if (m_exec)
        modeStr += L"Exec";

    if (modeStr.IsEmpty())
        return L"NoAccess";
    if (modeStr.Length() <= 5)
        modeStr += L"Only";

    return modeStr;
}

// --------------------------------------------------------------------------------------
//  Common HostSys implementation
// --------------------------------------------------------------------------------------
void HostSys::Munmap(void *base, size_t size)
{
    Munmap((uptr)base, size);
}
