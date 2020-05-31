///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/datecontrols.h
// Purpose:     implementation helpers for wxDatePickerCtrl and wxCalendarCtrl
// Author:      Vadim Zeitlin
// Created:     2008-04-04
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _MSW_PRIVATE_DATECONTROLS_H_
#define _MSW_PRIVATE_DATECONTROLS_H_

#include "wx/datetime.h"

#include "wx/msw/wrapwin.h"

// namespace for the helper functions related to the date controls
namespace wxMSWDateControls
{

// do the one time only initialization of date classes of comctl32.dll, return
// true if ok or log an error and return false if we failed (this can only
// happen with a very old version of common controls DLL, i.e. before 4.70)
extern bool CheckInitialization();

} // namespace wxMSWDateControls

#endif // _MSW_PRIVATE_DATECONTROLS_H_


