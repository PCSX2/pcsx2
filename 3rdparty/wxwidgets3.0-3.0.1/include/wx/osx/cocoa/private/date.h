///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/private/date.h
// Purpose:     NSDate-related helpers
// Author:      Vadim Zeitlin
// Created:     2011-12-19
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_COCOA_PRIVATE_DATE_H_
#define _WX_OSX_COCOA_PRIVATE_DATE_H_

#include "wx/datetime.h"

namespace wxOSXImpl
{

// Functions to convert between NSDate and wxDateTime.

// Returns an NSDate corresponding to the given wxDateTime which can be invalid
// (in which case nil is returned).
inline NSDate* NSDateFromWX(const wxDateTime& dt)
{
    if ( !dt.IsValid() )
        return nil;

    // Get the internal representation as a double used by NSDate.
    double ticks = dt.GetValue().ToDouble();

    // wxDateTime uses milliseconds while NSDate uses (fractional) seconds.
    return [NSDate dateWithTimeIntervalSince1970:ticks/1000.];
}


// Returns wxDateTime corresponding to the given NSDate (which may be nil).
inline wxDateTime NSDateToWX(const NSDate* d)
{
    if ( !d )
        return wxDefaultDateTime;

    // Reverse everything done above.
    wxLongLong ll;
    ll.Assign([d timeIntervalSince1970]*1000);
    wxDateTime dt(ll);
    return dt;
}

} // namespace wxOSXImpl

#endif // _WX_OSX_COCOA_PRIVATE_DATE_H_
