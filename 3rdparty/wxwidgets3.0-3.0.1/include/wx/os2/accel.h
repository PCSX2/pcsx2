/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/accel.h
// Purpose:     wxAcceleratorTable class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ACCEL_H_
#define _WX_ACCEL_H_

#include "wx/object.h"

class WXDLLIMPEXP_FWD_CORE wxAcceleratorTable;

// Hold Ctrl key down
#define wxACCEL_ALT     0x01

// Hold Ctrl key down
#define wxACCEL_CTRL    0x02

 // Hold Shift key down
#define wxACCEL_SHIFT   0x04

 // Hold no key down
#define wxACCEL_NORMAL  0x00

class WXDLLIMPEXP_CORE wxAcceleratorTable: public wxObject
{
DECLARE_DYNAMIC_CLASS(wxAcceleratorTable)
public:
    wxAcceleratorTable();
    wxAcceleratorTable(const wxString& rsResource); // Load from .rc resource
    wxAcceleratorTable( int                n
                       ,const wxAcceleratorEntry vaEntries[]
                      ); // Load from array

    virtual ~wxAcceleratorTable();

    bool Ok() const { return IsOk(); }
    bool IsOk() const;
    void SetHACCEL(WXHACCEL hAccel);
    WXHACCEL GetHACCEL(void) const;

    // translate the accelerator, return TRUE if done
    bool Translate( WXHWND hWnd
                   ,WXMSG* pMsg
                  ) const;
};

WXDLLIMPEXP_DATA_CORE(extern wxAcceleratorTable) wxNullAcceleratorTable;

WXDLLIMPEXP_CORE wxString wxPMTextToLabel(const wxString& rsTitle);
#endif
    // _WX_ACCEL_H_
