/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/hid.h
// Purpose:     DARWIN HID layer for WX
// Author:      Ryan Norton
// Modified by:
// Created:     11/11/2003
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifndef _WX_MACCARBONHID_H_
#define _WX_MACCARBONHID_H_

#include "wx/defs.h"
#include "wx/string.h"

//Mac OSX only
#ifdef __DARWIN__

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>

//Darn apple - doesn't properly wrap their headers in extern "C"!
//http://www.macosx.com/forums/archive/index.php/t-68069.html
extern "C" {
#include <mach/mach_port.h>
}

#include <mach/mach.h> //this actually includes mach_port.h (see above)

// ===========================================================================
// definitions
// ===========================================================================


// ---------------------------------------------------------------------------
// wxHIDDevice
//
// A wrapper around OS X HID Manager procedures.
// The tutorial "Working With HID Class Device Interfaces" Is
// Quite good, as is the sample program associated with it
// (Depite the author's protests!).
// ---------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxHIDDevice
{
public:
    wxHIDDevice() : m_ppDevice(NULL), m_ppQueue(NULL), m_pCookies(NULL) {}

    bool Create (int nClass = -1, int nType = -1, int nDev = 1);

    static size_t GetCount(int nClass = -1, int nType = -1);

    void AddCookie(CFTypeRef Data, int i);
    void AddCookieInQueue(CFTypeRef Data, int i);
    void InitCookies(size_t dwSize, bool bQueue = false);

    //Must be implemented by derived classes
    //builds the cookie array -
    //first call InitCookies to initialize the cookie
    //array, then AddCookie to add a cookie at a certain point in an array
    virtual void BuildCookies(CFArrayRef Array) = 0;

    //checks to see whether the cookie at nIndex is active (element value != 0)
    bool IsActive(int nIndex);

    //checks to see whether an element in the internal cookie array
    //exists
    bool HasElement(int nIndex);

    //closes the device and cleans the queue and cookies
    virtual ~wxHIDDevice();

protected:
    IOHIDDeviceInterface** m_ppDevice; //this, essentially
    IOHIDQueueInterface**  m_ppQueue;  //queue (if we want one)
    IOHIDElementCookie*    m_pCookies; //cookies

    wxString    m_szProductName; //product name
    int         m_nProductId; //product id
    int         m_nManufacturerId; //manufacturer id
    mach_port_t m_pPort;            //mach port to use
};

// ---------------------------------------------------------------------------
// wxHIDKeyboard
//
// Semi-simple implementation that opens a connection to the first
// keyboard of the machine. Used in wxGetKeyState.
// ---------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxHIDKeyboard : public wxHIDDevice
{
public:
    static int GetCount();
    bool Create(int nDev = 1);
    void AddCookie(CFTypeRef Data, int i);
    virtual void BuildCookies(CFArrayRef Array);
    void DoBuildCookies(CFArrayRef Array);
};

#endif //__DARWIN__

#endif
    // _WX_MACCARBONHID_H_
