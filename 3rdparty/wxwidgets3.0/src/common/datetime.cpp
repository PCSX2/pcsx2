///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/datetime.cpp
// Purpose:     implementation of time/date related classes
//              (for formatting&parsing see datetimefmt.cpp)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.05.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
//              parts of code taken from sndcal library by Scott E. Lee:
//
//               Copyright 1993-1995, Scott E. Lee, all rights reserved.
//               Permission granted to use, copy, modify, distribute and sell
//               so long as the above copyright and this permission statement
//               are retained in all copies.
//
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
 * Implementation notes:
 *
 * 1. the time is stored as a 64bit integer containing the signed number of
 *    milliseconds since Jan 1. 1970 (the Unix Epoch) - so it is always
 *    expressed in GMT.
 *
 * 2. the range is thus something about 580 million years, but due to current
 *    algorithms limitations, only dates from Nov 24, 4714BC are handled
 *
 * 3. standard ANSI C functions are used to do time calculations whenever
 *    possible, i.e. when the date is in the range Jan 1, 1970 to 2038
 *
 * 4. otherwise, the calculations are done by converting the date to/from JDN
 *    first (the range limitation mentioned above comes from here: the
 *    algorithm used by Scott E. Lee's code only works for positive JDNs, more
 *    or less)
 *
 * 5. the object constructed for the given DD-MM-YYYY HH:MM:SS corresponds to
 *    this moment in local time and may be converted to the object
 *    corresponding to the same date/time in another time zone by using
 *    ToTimezone()
 *
 * 6. the conversions to the current (or any other) timezone are done when the
 *    internal time representation is converted to the broken-down one in
 *    wxDateTime::Tm.
 */

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if !defined(wxUSE_DATETIME) || wxUSE_DATETIME

#ifndef WX_PRECOMP
    #ifdef __WINDOWS__
        #include "wx/msw/wrapwin.h"
    #endif
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/stopwatch.h"           // for wxGetLocalTimeMillis()
    #include "wx/module.h"
    #include "wx/crt.h"
#endif // WX_PRECOMP

#include "wx/thread.h"
#include "wx/time.h"
#include "wx/tokenzr.h"

#include <ctype.h>

#ifdef __WINDOWS__
    #include <winnls.h>
    #ifndef __WXWINCE__
        #include <locale.h>
    #endif
#endif

#include "wx/datetime.h"

// ----------------------------------------------------------------------------
// wxXTI
// ----------------------------------------------------------------------------

#if wxUSE_EXTENDED_RTTI

template<> void wxStringReadValue(const wxString &s , wxDateTime &data )
{
    data.ParseFormat(s,"%Y-%m-%d %H:%M:%S", NULL);
}

template<> void wxStringWriteValue(wxString &s , const wxDateTime &data )
{
    s = data.Format("%Y-%m-%d %H:%M:%S");
}

wxCUSTOM_TYPE_INFO(wxDateTime, wxToStringConverter<wxDateTime> , wxFromStringConverter<wxDateTime>)

#endif // wxUSE_EXTENDED_RTTI

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// debugging helper: just a convenient replacement of wxCHECK()
#define wxDATETIME_CHECK(expr, msg) \
    wxCHECK2_MSG(expr, *this = wxInvalidDateTime; return *this, msg)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

class wxDateTimeHolidaysModule : public wxModule
{
public:
    virtual bool OnInit()
    {
        wxDateTimeHolidayAuthority::AddAuthority(new wxDateTimeWorkDays);

        return true;
    }

    virtual void OnExit()
    {
        wxDateTimeHolidayAuthority::ClearAllAuthorities();
        wxDateTimeHolidayAuthority::ms_authorities.clear();
    }

private:
    DECLARE_DYNAMIC_CLASS(wxDateTimeHolidaysModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxDateTimeHolidaysModule, wxModule)

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// some trivial ones
static const int MONTHS_IN_YEAR = 12;

static const int SEC_PER_MIN = 60;

static const int MIN_PER_HOUR = 60;

static const long SECONDS_PER_DAY = 86400l;

static const int DAYS_PER_WEEK = 7;

static const long MILLISECONDS_PER_DAY = 86400000l;

// this is the integral part of JDN of the midnight of Jan 1, 1970
// (i.e. JDN(Jan 1, 1970) = 2440587.5)
static const long EPOCH_JDN = 2440587l;

// these values are only used in asserts so don't define them if asserts are
// disabled to avoid warnings about unused static variables
#if wxDEBUG_LEVEL
// the date of JDN -0.5 (as we don't work with fractional parts, this is the
// reference date for us) is Nov 24, 4714BC
static const int JDN_0_YEAR = -4713;
static const int JDN_0_MONTH = wxDateTime::Nov;
static const int JDN_0_DAY = 24;
#endif // wxDEBUG_LEVEL

// the constants used for JDN calculations
static const long JDN_OFFSET         = 32046l;
static const long DAYS_PER_5_MONTHS  = 153l;
static const long DAYS_PER_4_YEARS   = 1461l;
static const long DAYS_PER_400_YEARS = 146097l;

// this array contains the cumulated number of days in all previous months for
// normal and leap years
static const wxDateTime::wxDateTime_t gs_cumulatedDays[2][MONTHS_IN_YEAR] =
{
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};

const long wxDateTime::TIME_T_FACTOR = 1000l;

// ----------------------------------------------------------------------------
// global data
// ----------------------------------------------------------------------------

const char wxDefaultDateTimeFormat[] = "%c";
const char wxDefaultTimeSpanFormat[] = "%H:%M:%S";

// in the fine tradition of ANSI C we use our equivalent of (time_t)-1 to
// indicate an invalid wxDateTime object
const wxDateTime wxDefaultDateTime;

wxDateTime::Country wxDateTime::ms_country = wxDateTime::Country_Unknown;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// debugger helper: this function can be called from a debugger to show what
// the date really is
extern const char *wxDumpDate(const wxDateTime* dt)
{
    static char buf[128];

    wxString fmt(dt->Format("%Y-%m-%d (%a) %H:%M:%S"));
    wxStrlcpy(buf,
              (fmt + " (" + dt->GetValue().ToString() + " ticks)").ToAscii(),
              WXSIZEOF(buf));

    return buf;
}

// get the number of days in the given month of the given year
static inline
wxDateTime::wxDateTime_t GetNumOfDaysInMonth(int year, wxDateTime::Month month)
{
    // the number of days in month in Julian/Gregorian calendar: the first line
    // is for normal years, the second one is for the leap ones
    static const wxDateTime::wxDateTime_t daysInMonth[2][MONTHS_IN_YEAR] =
    {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
    };

    return daysInMonth[wxDateTime::IsLeapYear(year)][month];
}

// return the integral part of the JDN for the midnight of the given date (to
// get the real JDN you need to add 0.5, this is, in fact, JDN of the
// noon of the previous day)
static long GetTruncatedJDN(wxDateTime::wxDateTime_t day,
                            wxDateTime::Month mon,
                            int year)
{
    // CREDIT: code below is by Scott E. Lee (but bugs are mine)

    // check the date validity
    wxASSERT_MSG(
      (year > JDN_0_YEAR) ||
      ((year == JDN_0_YEAR) && (mon > JDN_0_MONTH)) ||
      ((year == JDN_0_YEAR) && (mon == JDN_0_MONTH) && (day >= JDN_0_DAY)),
      wxT("date out of range - can't convert to JDN")
                );

    // make the year positive to avoid problems with negative numbers division
    year += 4800;

    // months are counted from March here
    int month;
    if ( mon >= wxDateTime::Mar )
    {
        month = mon - 2;
    }
    else
    {
        month = mon + 10;
        year--;
    }

    // now we can simply add all the contributions together
    return ((year / 100) * DAYS_PER_400_YEARS) / 4
            + ((year % 100) * DAYS_PER_4_YEARS) / 4
            + (month * DAYS_PER_5_MONTHS + 2) / 5
            + day
            - JDN_OFFSET;
}

#ifdef wxHAS_STRFTIME

// this function is a wrapper around strftime(3) adding error checking
// NOTE: not static because used by datetimefmt.cpp
wxString CallStrftime(const wxString& format, const tm* tm)
{
    wxChar buf[4096];
    // Create temp wxString here to work around mingw/cygwin bug 1046059
    // http://sourceforge.net/tracker/?func=detail&atid=102435&aid=1046059&group_id=2435
    wxString s;

    if ( !wxStrftime(buf, WXSIZEOF(buf), format, tm) )
    {
        // There is one special case in which strftime() can return 0 without
        // indicating an error: "%p" may give empty string depending on the
        // locale, so check for it explicitly. Apparently it's really the only
        // exception.
        if ( format != wxS("%p") )
        {
            // if the format is valid, buffer must be too small?
            wxFAIL_MSG(wxT("strftime() failed"));
        }

        buf[0] = '\0';
    }

    s = buf;
    return s;
}

#endif // wxHAS_STRFTIME

// if year and/or month have invalid values, replace them with the current ones
static void ReplaceDefaultYearMonthWithCurrent(int *year,
                                               wxDateTime::Month *month)
{
    struct tm *tmNow = NULL;
    struct tm tmstruct;

    if ( *year == wxDateTime::Inv_Year )
    {
        tmNow = wxDateTime::GetTmNow(&tmstruct);

        *year = 1900 + tmNow->tm_year;
    }

    if ( *month == wxDateTime::Inv_Month )
    {
        if ( !tmNow )
            tmNow = wxDateTime::GetTmNow(&tmstruct);

        *month = (wxDateTime::Month)tmNow->tm_mon;
    }
}

// fill the struct tm with default values
// NOTE: not static because used by datetimefmt.cpp
void InitTm(struct tm& tm)
{
    // struct tm may have etxra fields (undocumented and with unportable
    // names) which, nevertheless, must be set to 0
    memset(&tm, 0, sizeof(struct tm));

    tm.tm_mday = 1;   // mday 0 is invalid
    tm.tm_year = 76;  // any valid year
    tm.tm_isdst = -1; // auto determine
}

// ============================================================================
// implementation of wxDateTime
// ============================================================================

// ----------------------------------------------------------------------------
// struct Tm
// ----------------------------------------------------------------------------

wxDateTime::Tm::Tm()
{
    year = (wxDateTime_t)wxDateTime::Inv_Year;
    mon = wxDateTime::Inv_Month;
    mday =
    yday = 0;
    hour =
    min =
    sec =
    msec = 0;
    wday = wxDateTime::Inv_WeekDay;
}

wxDateTime::Tm::Tm(const struct tm& tm, const TimeZone& tz)
              : m_tz(tz)
{
    msec = 0;
    sec = (wxDateTime::wxDateTime_t)tm.tm_sec;
    min = (wxDateTime::wxDateTime_t)tm.tm_min;
    hour = (wxDateTime::wxDateTime_t)tm.tm_hour;
    mday = (wxDateTime::wxDateTime_t)tm.tm_mday;
    mon = (wxDateTime::Month)tm.tm_mon;
    year = 1900 + tm.tm_year;
    wday = (wxDateTime::wxDateTime_t)tm.tm_wday;
    yday = (wxDateTime::wxDateTime_t)tm.tm_yday;
}

bool wxDateTime::Tm::IsValid() const
{
    if ( mon == wxDateTime::Inv_Month )
        return false;

    // We need to check this here to avoid crashing in GetNumOfDaysInMonth() if
    // somebody passed us "(wxDateTime::Month)1000".
    wxCHECK_MSG( mon >= wxDateTime::Jan && mon < wxDateTime::Inv_Month, false,
                 wxS("Invalid month value") );

    // we allow for the leap seconds, although we don't use them (yet)
    return (year != wxDateTime::Inv_Year) && (mon != wxDateTime::Inv_Month) &&
           (mday > 0 && mday <= GetNumOfDaysInMonth(year, mon)) &&
           (hour < 24) && (min < 60) && (sec < 62) && (msec < 1000);
}

void wxDateTime::Tm::ComputeWeekDay()
{
    // compute the week day from day/month/year: we use the dumbest algorithm
    // possible: just compute our JDN and then use the (simple to derive)
    // formula: weekday = (JDN + 1.5) % 7
    wday = (wxDateTime::wxDateTime_t)((GetTruncatedJDN(mday, mon, year) + 2) % 7);
}

void wxDateTime::Tm::AddMonths(int monDiff)
{
    // normalize the months field
    while ( monDiff < -mon )
    {
        year--;

        monDiff += MONTHS_IN_YEAR;
    }

    while ( monDiff + mon >= MONTHS_IN_YEAR )
    {
        year++;

        monDiff -= MONTHS_IN_YEAR;
    }

    mon = (wxDateTime::Month)(mon + monDiff);

    wxASSERT_MSG( mon >= 0 && mon < MONTHS_IN_YEAR, wxT("logic error") );

    // NB: we don't check here that the resulting date is valid, this function
    //     is private and the caller must check it if needed
}

void wxDateTime::Tm::AddDays(int dayDiff)
{
    // normalize the days field
    while ( dayDiff + mday < 1 )
    {
        AddMonths(-1);

        dayDiff += GetNumOfDaysInMonth(year, mon);
    }

    mday = (wxDateTime::wxDateTime_t)( mday + dayDiff );
    while ( mday > GetNumOfDaysInMonth(year, mon) )
    {
        mday -= GetNumOfDaysInMonth(year, mon);

        AddMonths(1);
    }

    wxASSERT_MSG( mday > 0 && mday <= GetNumOfDaysInMonth(year, mon),
                  wxT("logic error") );
}

// ----------------------------------------------------------------------------
// class TimeZone
// ----------------------------------------------------------------------------

wxDateTime::TimeZone::TimeZone(wxDateTime::TZ tz)
{
    switch ( tz )
    {
        case wxDateTime::Local:
            // get the offset from C RTL: it returns the difference GMT-local
            // while we want to have the offset _from_ GMT, hence the '-'
            m_offset = -wxGetTimeZone();
            break;

        case wxDateTime::GMT_12:
        case wxDateTime::GMT_11:
        case wxDateTime::GMT_10:
        case wxDateTime::GMT_9:
        case wxDateTime::GMT_8:
        case wxDateTime::GMT_7:
        case wxDateTime::GMT_6:
        case wxDateTime::GMT_5:
        case wxDateTime::GMT_4:
        case wxDateTime::GMT_3:
        case wxDateTime::GMT_2:
        case wxDateTime::GMT_1:
            m_offset = -3600*(wxDateTime::GMT0 - tz);
            break;

        case wxDateTime::GMT0:
        case wxDateTime::GMT1:
        case wxDateTime::GMT2:
        case wxDateTime::GMT3:
        case wxDateTime::GMT4:
        case wxDateTime::GMT5:
        case wxDateTime::GMT6:
        case wxDateTime::GMT7:
        case wxDateTime::GMT8:
        case wxDateTime::GMT9:
        case wxDateTime::GMT10:
        case wxDateTime::GMT11:
        case wxDateTime::GMT12:
        case wxDateTime::GMT13:
            m_offset = 3600*(tz - wxDateTime::GMT0);
            break;

        case wxDateTime::A_CST:
            // Central Standard Time in use in Australia = UTC + 9.5
            m_offset = 60l*(9*MIN_PER_HOUR + MIN_PER_HOUR/2);
            break;

        default:
            wxFAIL_MSG( wxT("unknown time zone") );
    }
}

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------

/* static */
struct tm *wxDateTime::GetTmNow(struct tm *tmstruct)
{
    time_t t = GetTimeNow();
    return wxLocaltime_r(&t, tmstruct);
}

/* static */
bool wxDateTime::IsLeapYear(int year, wxDateTime::Calendar cal)
{
    if ( year == Inv_Year )
        year = GetCurrentYear();

    if ( cal == Gregorian )
    {
        // in Gregorian calendar leap years are those divisible by 4 except
        // those divisible by 100 unless they're also divisible by 400
        // (in some countries, like Russia and Greece, additional corrections
        // exist, but they won't manifest themselves until 2700)
        return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    }
    else if ( cal == Julian )
    {
        // in Julian calendar the rule is simpler
        return year % 4 == 0;
    }
    else
    {
        wxFAIL_MSG(wxT("unknown calendar"));

        return false;
    }
}

/* static */
int wxDateTime::GetCentury(int year)
{
    return year > 0 ? year / 100 : year / 100 - 1;
}

/* static */
int wxDateTime::ConvertYearToBC(int year)
{
    // year 0 is BC 1
    return year > 0 ? year : year - 1;
}

/* static */
int wxDateTime::GetCurrentYear(wxDateTime::Calendar cal)
{
    switch ( cal )
    {
        case Gregorian:
            return Now().GetYear();

        case Julian:
            wxFAIL_MSG(wxT("TODO"));
            break;

        default:
            wxFAIL_MSG(wxT("unsupported calendar"));
            break;
    }

    return Inv_Year;
}

/* static */
wxDateTime::Month wxDateTime::GetCurrentMonth(wxDateTime::Calendar cal)
{
    switch ( cal )
    {
        case Gregorian:
            return Now().GetMonth();

        case Julian:
            wxFAIL_MSG(wxT("TODO"));
            break;

        default:
            wxFAIL_MSG(wxT("unsupported calendar"));
            break;
    }

    return Inv_Month;
}

/* static */
wxDateTime::wxDateTime_t wxDateTime::GetNumberOfDays(int year, Calendar cal)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    switch ( cal )
    {
        case Gregorian:
        case Julian:
            return IsLeapYear(year) ? 366 : 365;

        default:
            wxFAIL_MSG(wxT("unsupported calendar"));
            break;
    }

    return 0;
}

/* static */
wxDateTime::wxDateTime_t wxDateTime::GetNumberOfDays(wxDateTime::Month month,
                                                     int year,
                                                     wxDateTime::Calendar cal)
{
    wxCHECK_MSG( month < MONTHS_IN_YEAR, 0, wxT("invalid month") );

    if ( cal == Gregorian || cal == Julian )
    {
        if ( year == Inv_Year )
        {
            // take the current year if none given
            year = GetCurrentYear();
        }

        return GetNumOfDaysInMonth(year, month);
    }
    else
    {
        wxFAIL_MSG(wxT("unsupported calendar"));

        return 0;
    }
}

namespace
{

// helper function used by GetEnglish/WeekDayName(): returns 0 if flags is
// Name_Full and 1 if it is Name_Abbr or -1 if the flags is incorrect (and
// asserts in this case)
//
// the return value of this function is used as an index into 2D array
// containing full names in its first row and abbreviated ones in the 2nd one
int NameArrayIndexFromFlag(wxDateTime::NameFlags flags)
{
    switch ( flags )
    {
        case wxDateTime::Name_Full:
            return 0;

        case wxDateTime::Name_Abbr:
            return 1;

        default:
            wxFAIL_MSG( "unknown wxDateTime::NameFlags value" );
    }

    return -1;
}

} // anonymous namespace

/* static */
wxString wxDateTime::GetEnglishMonthName(Month month, NameFlags flags)
{
    wxCHECK_MSG( month != Inv_Month, wxEmptyString, "invalid month" );

    static const char *const monthNames[2][MONTHS_IN_YEAR] =
    {
        { "January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December" },
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }
    };

    const int idx = NameArrayIndexFromFlag(flags);
    if ( idx == -1 )
        return wxString();

    return monthNames[idx][month];
}

/* static */
wxString wxDateTime::GetMonthName(wxDateTime::Month month,
                                  wxDateTime::NameFlags flags)
{
#ifdef wxHAS_STRFTIME
    wxCHECK_MSG( month != Inv_Month, wxEmptyString, wxT("invalid month") );

    // notice that we must set all the fields to avoid confusing libc (GNU one
    // gets confused to a crash if we don't do this)
    tm tm;
    InitTm(tm);
    tm.tm_mon = month;

    return CallStrftime(flags == Name_Abbr ? wxT("%b") : wxT("%B"), &tm);
#else // !wxHAS_STRFTIME
    return GetEnglishMonthName(month, flags);
#endif // wxHAS_STRFTIME/!wxHAS_STRFTIME
}

/* static */
wxString wxDateTime::GetEnglishWeekDayName(WeekDay wday, NameFlags flags)
{
    wxCHECK_MSG( wday != Inv_WeekDay, wxEmptyString, wxT("invalid weekday") );

    static const char *const weekdayNames[2][DAYS_PER_WEEK] =
    {
        { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
          "Saturday" },
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
    };

    const int idx = NameArrayIndexFromFlag(flags);
    if ( idx == -1 )
        return wxString();

    return weekdayNames[idx][wday];
}

/* static */
wxString wxDateTime::GetWeekDayName(wxDateTime::WeekDay wday,
                                    wxDateTime::NameFlags flags)
{
#ifdef wxHAS_STRFTIME
    wxCHECK_MSG( wday != Inv_WeekDay, wxEmptyString, wxT("invalid weekday") );

    // take some arbitrary Sunday (but notice that the day should be such that
    // after adding wday to it below we still have a valid date, e.g. don't
    // take 28 here!)
    tm tm;
    InitTm(tm);
    tm.tm_mday = 21;
    tm.tm_mon = Nov;
    tm.tm_year = 99;

    // and offset it by the number of days needed to get the correct wday
    tm.tm_mday += wday;

    // call mktime() to normalize it...
    (void)mktime(&tm);

    // ... and call strftime()
    return CallStrftime(flags == Name_Abbr ? wxT("%a") : wxT("%A"), &tm);
#else // !wxHAS_STRFTIME
    return GetEnglishWeekDayName(wday, flags);
#endif // wxHAS_STRFTIME/!wxHAS_STRFTIME
}

/* static */
void wxDateTime::GetAmPmStrings(wxString *am, wxString *pm)
{
    tm tm;
    InitTm(tm);
    wxChar buffer[64];
    // @Note: Do not call 'CallStrftime' here! CallStrftime checks the return code
    // and causes an assertion failed if the buffer is to small (which is good) - OR -
    // if strftime does not return anything because the format string is invalid - OR -
    // if there are no 'am' / 'pm' tokens defined for the current locale (which is not good).
    // wxDateTime::ParseTime will try several different formats to parse the time.
    // As a result, GetAmPmStrings might get called, even if the current locale
    // does not define any 'am' / 'pm' tokens. In this case, wxStrftime would
    // assert, even though it is a perfectly legal use.
    if ( am )
    {
        if (wxStrftime(buffer, WXSIZEOF(buffer), wxT("%p"), &tm) > 0)
            *am = wxString(buffer);
        else
            *am = wxString();
    }
    if ( pm )
    {
        tm.tm_hour = 13;
        if (wxStrftime(buffer, WXSIZEOF(buffer), wxT("%p"), &tm) > 0)
            *pm = wxString(buffer);
        else
            *pm = wxString();
    }
}


// ----------------------------------------------------------------------------
// Country stuff: date calculations depend on the country (DST, work days,
// ...), so we need to know which rules to follow.
// ----------------------------------------------------------------------------

/* static */
wxDateTime::Country wxDateTime::GetCountry()
{
    // TODO use LOCALE_ICOUNTRY setting under Win32
#ifndef __WXWINCE__
    if ( ms_country == Country_Unknown )
    {
        // try to guess from the time zone name
        time_t t = time(NULL);
        struct tm tmstruct;
        struct tm *tm = wxLocaltime_r(&t, &tmstruct);

        wxString tz = CallStrftime(wxT("%Z"), tm);
        if ( tz == wxT("WET") || tz == wxT("WEST") )
        {
            ms_country = UK;
        }
        else if ( tz == wxT("CET") || tz == wxT("CEST") )
        {
            ms_country = Country_EEC;
        }
        else if ( tz == wxT("MSK") || tz == wxT("MSD") )
        {
            ms_country = Russia;
        }
        else if ( tz == wxT("AST") || tz == wxT("ADT") ||
                  tz == wxT("EST") || tz == wxT("EDT") ||
                  tz == wxT("CST") || tz == wxT("CDT") ||
                  tz == wxT("MST") || tz == wxT("MDT") ||
                  tz == wxT("PST") || tz == wxT("PDT") )
        {
            ms_country = USA;
        }
        else
        {
            // well, choose a default one
            ms_country = USA;
        }
    }
#else // __WXWINCE__
     ms_country = USA;
#endif // !__WXWINCE__/__WXWINCE__

    return ms_country;
}

/* static */
void wxDateTime::SetCountry(wxDateTime::Country country)
{
    ms_country = country;
}

/* static */
bool wxDateTime::IsWestEuropeanCountry(Country country)
{
    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    return (Country_WesternEurope_Start <= country) &&
           (country <= Country_WesternEurope_End);
}

// ----------------------------------------------------------------------------
// DST calculations: we use 3 different rules for the West European countries,
// USA and for the rest of the world. This is undoubtedly false for many
// countries, but I lack the necessary info (and the time to gather it),
// please add the other rules here!
// ----------------------------------------------------------------------------

/* static */
bool wxDateTime::IsDSTApplicable(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    switch ( country )
    {
        case USA:
        case UK:
            // DST was first observed in the US and UK during WWI, reused
            // during WWII and used again since 1966
            return year >= 1966 ||
                   (year >= 1942 && year <= 1945) ||
                   (year == 1918 || year == 1919);

        default:
            // assume that it started after WWII
            return year > 1950;
    }
}

/* static */
wxDateTime wxDateTime::GetBeginDST(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    if ( !IsDSTApplicable(year, country) )
    {
        return wxInvalidDateTime;
    }

    wxDateTime dt;

    if ( IsWestEuropeanCountry(country) || (country == Russia) )
    {
        // DST begins at 1 a.m. GMT on the last Sunday of March
        if ( !dt.SetToLastWeekDay(Sun, Mar, year) )
        {
            // weird...
            wxFAIL_MSG( wxT("no last Sunday in March?") );
        }

        dt += wxTimeSpan::Hours(1);
    }
    else switch ( country )
    {
        case USA:
            switch ( year )
            {
                case 1918:
                case 1919:
                    // don't know for sure - assume it was in effect all year

                case 1943:
                case 1944:
                case 1945:
                    dt.Set(1, Jan, year);
                    break;

                case 1942:
                    // DST was installed Feb 2, 1942 by the Congress
                    dt.Set(2, Feb, year);
                    break;

                    // Oil embargo changed the DST period in the US
                case 1974:
                    dt.Set(6, Jan, 1974);
                    break;

                case 1975:
                    dt.Set(23, Feb, 1975);
                    break;

                default:
                    // before 1986, DST begun on the last Sunday of April, but
                    // in 1986 Reagan changed it to begin at 2 a.m. of the
                    // first Sunday in April
                    if ( year < 1986 )
                    {
                        if ( !dt.SetToLastWeekDay(Sun, Apr, year) )
                        {
                            // weird...
                            wxFAIL_MSG( wxT("no first Sunday in April?") );
                        }
                    }
                    else if ( year > 2006 )
                    // Energy Policy Act of 2005, Pub. L. no. 109-58, 119 Stat 594 (2005).
                    // Starting in 2007, daylight time begins in the United States on the
                    // second Sunday in March and ends on the first Sunday in November
                    {
                        if ( !dt.SetToWeekDay(Sun, 2, Mar, year) )
                        {
                            // weird...
                            wxFAIL_MSG( wxT("no second Sunday in March?") );
                        }
                    }
                    else
                    {
                        if ( !dt.SetToWeekDay(Sun, 1, Apr, year) )
                        {
                            // weird...
                            wxFAIL_MSG( wxT("no first Sunday in April?") );
                        }
                    }

                    dt += wxTimeSpan::Hours(2);

                    // TODO what about timezone??
            }

            break;

        default:
            // assume Mar 30 as the start of the DST for the rest of the world
            // - totally bogus, of course
            dt.Set(30, Mar, year);
    }

    return dt;
}

/* static */
wxDateTime wxDateTime::GetEndDST(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    if ( !IsDSTApplicable(year, country) )
    {
        return wxInvalidDateTime;
    }

    wxDateTime dt;

    if ( IsWestEuropeanCountry(country) || (country == Russia) )
    {
        // DST ends at 1 a.m. GMT on the last Sunday of October
        if ( !dt.SetToLastWeekDay(Sun, Oct, year) )
        {
            // weirder and weirder...
            wxFAIL_MSG( wxT("no last Sunday in October?") );
        }

        dt += wxTimeSpan::Hours(1);
    }
    else switch ( country )
    {
        case USA:
            switch ( year )
            {
                case 1918:
                case 1919:
                    // don't know for sure - assume it was in effect all year

                case 1943:
                case 1944:
                    dt.Set(31, Dec, year);
                    break;

                case 1945:
                    // the time was reset after the end of the WWII
                    dt.Set(30, Sep, year);
                    break;

                default: // default for switch (year)
                    if ( year > 2006 )
                      // Energy Policy Act of 2005, Pub. L. no. 109-58, 119 Stat 594 (2005).
                      // Starting in 2007, daylight time begins in the United States on the
                      // second Sunday in March and ends on the first Sunday in November
                    {
                        if ( !dt.SetToWeekDay(Sun, 1, Nov, year) )
                        {
                            // weird...
                            wxFAIL_MSG( wxT("no first Sunday in November?") );
                        }
                    }
                    else
                     // pre-2007
                     // DST ends at 2 a.m. on the last Sunday of October
                    {
                        if ( !dt.SetToLastWeekDay(Sun, Oct, year) )
                        {
                            // weirder and weirder...
                            wxFAIL_MSG( wxT("no last Sunday in October?") );
                        }
                    }

                    dt += wxTimeSpan::Hours(2);

            // TODO: what about timezone??
            }
            break;

        default: // default for switch (country)
            // assume October 26th as the end of the DST - totally bogus too
            dt.Set(26, Oct, year);
    }

    return dt;
}

// ----------------------------------------------------------------------------
// constructors and assignment operators
// ----------------------------------------------------------------------------

// return the current time with ms precision
/* static */ wxDateTime wxDateTime::UNow()
{
    return wxDateTime(wxGetUTCTimeMillis());
}

// the values in the tm structure contain the local time
wxDateTime& wxDateTime::Set(const struct tm& tm)
{
    struct tm tm2(tm);
    time_t timet = mktime(&tm2);

    if ( timet == (time_t)-1 )
    {
        // mktime() rather unintuitively fails for Jan 1, 1970 if the hour is
        // less than timezone - try to make it work for this case
        if ( tm2.tm_year == 70 && tm2.tm_mon == 0 && tm2.tm_mday == 1 )
        {
            return Set((time_t)(
                       wxGetTimeZone() +
                       tm2.tm_hour * MIN_PER_HOUR * SEC_PER_MIN +
                       tm2.tm_min * SEC_PER_MIN +
                       tm2.tm_sec));
        }

        wxFAIL_MSG( wxT("mktime() failed") );

        *this = wxInvalidDateTime;

        return *this;
    }

    // mktime() only adjusts tm_wday, tm_yday and tm_isdst fields normally, if
    // it changed anything else, it must have performed the DST adjustment. But
    // the trouble with this is that different implementations do it
    // differently, e.g. GNU libc moves the time forward if the specified time
    // is invalid in the local time zone, while MSVC CRT moves it backwards
    // which is especially pernicious as it can change the date if the DST
    // starts at midnight, as it does in some time zones (see #15419), and this
    // is completely unexpected for the code working with dates only.
    //
    // So standardize on moving the time forwards to have consistent behaviour
    // under all platforms and to avoid the problem above.
    if ( tm2.tm_hour != tm.tm_hour )
    {
        tm2 = tm;
        tm2.tm_hour++;
        if ( tm2.tm_hour == 24 )
        {
            // This shouldn't normally happen as the DST never starts at 23:00
            // but if it does, we have a problem as we need to adjust the day
            // as well. However we stop here, i.e. we don't adjust the month
            // (or the year) because mktime() is supposed to take care of this
            // for us.
            tm2.tm_hour = 0;
            tm2.tm_mday++;
        }

        timet = mktime(&tm2);
    }

    return Set(timet);
}

wxDateTime& wxDateTime::Set(wxDateTime_t hour,
                            wxDateTime_t minute,
                            wxDateTime_t second,
                            wxDateTime_t millisec)
{
    // we allow seconds to be 61 to account for the leap seconds, even if we
    // don't use them really
    wxDATETIME_CHECK( hour < 24 &&
                      second < 62 &&
                      minute < 60 &&
                      millisec < 1000,
                      wxT("Invalid time in wxDateTime::Set()") );

    // get the current date from system
    struct tm tmstruct;
    struct tm *tm = GetTmNow(&tmstruct);

    wxDATETIME_CHECK( tm, wxT("wxLocaltime_r() failed") );

    // make a copy so it isn't clobbered by the call to mktime() below
    struct tm tm1(*tm);

    // adjust the time
    tm1.tm_hour = hour;
    tm1.tm_min = minute;
    tm1.tm_sec = second;

    // and the DST in case it changes on this date
    struct tm tm2(tm1);
    mktime(&tm2);
    if ( tm2.tm_isdst != tm1.tm_isdst )
        tm1.tm_isdst = tm2.tm_isdst;

    (void)Set(tm1);

    // and finally adjust milliseconds
    return SetMillisecond(millisec);
}

wxDateTime& wxDateTime::Set(wxDateTime_t day,
                            Month        month,
                            int          year,
                            wxDateTime_t hour,
                            wxDateTime_t minute,
                            wxDateTime_t second,
                            wxDateTime_t millisec)
{
    wxDATETIME_CHECK( hour < 24 &&
                      second < 62 &&
                      minute < 60 &&
                      millisec < 1000,
                      wxT("Invalid time in wxDateTime::Set()") );

    ReplaceDefaultYearMonthWithCurrent(&year, &month);

    wxDATETIME_CHECK( (0 < day) && (day <= GetNumberOfDays(month, year)),
                      wxT("Invalid date in wxDateTime::Set()") );

    // the range of time_t type (inclusive)
    static const int yearMinInRange = 1970;
    static const int yearMaxInRange = 2037;

    // test only the year instead of testing for the exact end of the Unix
    // time_t range - it doesn't bring anything to do more precise checks
    if ( year >= yearMinInRange && year <= yearMaxInRange )
    {
        // use the standard library version if the date is in range - this is
        // probably more efficient than our code
        struct tm tm;
        tm.tm_year = year - 1900;
        tm.tm_mon = month;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;       // mktime() will guess it

        (void)Set(tm);

        // and finally adjust milliseconds
        if (IsValid())
            SetMillisecond(millisec);

        return *this;
    }
    else
    {
        // do time calculations ourselves: we want to calculate the number of
        // milliseconds between the given date and the epoch

        // get the JDN for the midnight of this day
        m_time = GetTruncatedJDN(day, month, year);
        m_time -= EPOCH_JDN;
        m_time *= SECONDS_PER_DAY * TIME_T_FACTOR;

        // JDN corresponds to GMT, we take localtime
        Add(wxTimeSpan(hour, minute, second + wxGetTimeZone(), millisec));
    }

    return *this;
}

wxDateTime& wxDateTime::Set(double jdn)
{
    // so that m_time will be 0 for the midnight of Jan 1, 1970 which is jdn
    // EPOCH_JDN + 0.5
    jdn -= EPOCH_JDN + 0.5;

    m_time.Assign(jdn*MILLISECONDS_PER_DAY);

    // JDNs always are in UTC, so we don't need any adjustments for time zone

    return *this;
}

wxDateTime& wxDateTime::ResetTime()
{
    Tm tm = GetTm();

    if ( tm.hour || tm.min || tm.sec || tm.msec )
    {
        tm.msec =
        tm.sec =
        tm.min =
        tm.hour = 0;

        Set(tm);
    }

    return *this;
}

wxDateTime wxDateTime::GetDateOnly() const
{
    Tm tm = GetTm();
    tm.msec =
    tm.sec =
    tm.min =
    tm.hour = 0;
    return wxDateTime(tm);
}

// ----------------------------------------------------------------------------
// DOS Date and Time Format functions
// ----------------------------------------------------------------------------
// the dos date and time value is an unsigned 32 bit value in the format:
// YYYYYYYMMMMDDDDDhhhhhmmmmmmsssss
//
// Y = year offset from 1980 (0-127)
// M = month (1-12)
// D = day of month (1-31)
// h = hour (0-23)
// m = minute (0-59)
// s = bisecond (0-29) each bisecond indicates two seconds
// ----------------------------------------------------------------------------

wxDateTime& wxDateTime::SetFromDOS(unsigned long ddt)
{
    struct tm tm;
    InitTm(tm);

    long year = ddt & 0xFE000000;
    year >>= 25;
    year += 80;
    tm.tm_year = year;

    long month = ddt & 0x1E00000;
    month >>= 21;
    month -= 1;
    tm.tm_mon = month;

    long day = ddt & 0x1F0000;
    day >>= 16;
    tm.tm_mday = day;

    long hour = ddt & 0xF800;
    hour >>= 11;
    tm.tm_hour = hour;

    long minute = ddt & 0x7E0;
    minute >>= 5;
    tm.tm_min = minute;

    long second = ddt & 0x1F;
    tm.tm_sec = second * 2;

    return Set(mktime(&tm));
}

unsigned long wxDateTime::GetAsDOS() const
{
    unsigned long ddt;
    time_t ticks = GetTicks();
    struct tm tmstruct;
    struct tm *tm = wxLocaltime_r(&ticks, &tmstruct);
    wxCHECK_MSG( tm, ULONG_MAX, wxT("time can't be represented in DOS format") );

    long year = tm->tm_year;
    year -= 80;
    year <<= 25;

    long month = tm->tm_mon;
    month += 1;
    month <<= 21;

    long day = tm->tm_mday;
    day <<= 16;

    long hour = tm->tm_hour;
    hour <<= 11;

    long minute = tm->tm_min;
    minute <<= 5;

    long second = tm->tm_sec;
    second /= 2;

    ddt = year | month | day | hour | minute | second;
    return ddt;
}

// ----------------------------------------------------------------------------
// time_t <-> broken down time conversions
// ----------------------------------------------------------------------------

wxDateTime::Tm wxDateTime::GetTm(const TimeZone& tz) const
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    time_t time = GetTicks();
    if ( time != (time_t)-1 )
    {
        // use C RTL functions
        struct tm tmstruct;
        tm *tm;
        if ( tz.GetOffset() == -wxGetTimeZone() )
        {
            // we are working with local time
            tm = wxLocaltime_r(&time, &tmstruct);

            // should never happen
            wxCHECK_MSG( tm, Tm(), wxT("wxLocaltime_r() failed") );
        }
        else
        {
            time += (time_t)tz.GetOffset();
#if defined(__VMS__) || defined(__WATCOMC__) // time is unsigned so avoid warning
            int time2 = (int) time;
            if ( time2 >= 0 )
#else
            if ( time >= 0 )
#endif
            {
                tm = wxGmtime_r(&time, &tmstruct);

                // should never happen
                wxCHECK_MSG( tm, Tm(), wxT("wxGmtime_r() failed") );
            }
            else
            {
                tm = (struct tm *)NULL;
            }
        }

        if ( tm )
        {
            // adjust the milliseconds
            Tm tm2(*tm, tz);
            long timeOnly = (m_time % MILLISECONDS_PER_DAY).ToLong();
            tm2.msec = (wxDateTime_t)(timeOnly % 1000);
            return tm2;
        }
        //else: use generic code below
    }

    // remember the time and do the calculations with the date only - this
    // eliminates rounding errors of the floating point arithmetics

    wxLongLong timeMidnight = m_time + tz.GetOffset() * 1000;

    long timeOnly = (timeMidnight % MILLISECONDS_PER_DAY).ToLong();

    // we want to always have positive time and timeMidnight to be really
    // the midnight before it
    if ( timeOnly < 0 )
    {
        timeOnly = MILLISECONDS_PER_DAY + timeOnly;
    }

    timeMidnight -= timeOnly;

    // calculate the Gregorian date from JDN for the midnight of our date:
    // this will yield day, month (in 1..12 range) and year

    // actually, this is the JDN for the noon of the previous day
    long jdn = (timeMidnight / MILLISECONDS_PER_DAY).ToLong() + EPOCH_JDN;

    // CREDIT: code below is by Scott E. Lee (but bugs are mine)

    wxASSERT_MSG( jdn > -2, wxT("JDN out of range") );

    // calculate the century
    long temp = (jdn + JDN_OFFSET) * 4 - 1;
    long century = temp / DAYS_PER_400_YEARS;

    // then the year and day of year (1 <= dayOfYear <= 366)
    temp = ((temp % DAYS_PER_400_YEARS) / 4) * 4 + 3;
    long year = (century * 100) + (temp / DAYS_PER_4_YEARS);
    long dayOfYear = (temp % DAYS_PER_4_YEARS) / 4 + 1;

    // and finally the month and day of the month
    temp = dayOfYear * 5 - 3;
    long month = temp / DAYS_PER_5_MONTHS;
    long day = (temp % DAYS_PER_5_MONTHS) / 5 + 1;

    // month is counted from March - convert to normal
    if ( month < 10 )
    {
        month += 3;
    }
    else
    {
        year += 1;
        month -= 9;
    }

    // year is offset by 4800
    year -= 4800;

    // check that the algorithm gave us something reasonable
    wxASSERT_MSG( (0 < month) && (month <= 12), wxT("invalid month") );
    wxASSERT_MSG( (1 <= day) && (day < 32), wxT("invalid day") );

    // construct Tm from these values
    Tm tm;
    tm.year = (int)year;
    tm.yday = (wxDateTime_t)(dayOfYear - 1); // use C convention for day number
    tm.mon = (Month)(month - 1); // algorithm yields 1 for January, not 0
    tm.mday = (wxDateTime_t)day;
    tm.msec = (wxDateTime_t)(timeOnly % 1000);
    timeOnly -= tm.msec;
    timeOnly /= 1000;               // now we have time in seconds

    tm.sec = (wxDateTime_t)(timeOnly % SEC_PER_MIN);
    timeOnly -= tm.sec;
    timeOnly /= SEC_PER_MIN;        // now we have time in minutes

    tm.min = (wxDateTime_t)(timeOnly % MIN_PER_HOUR);
    timeOnly -= tm.min;

    tm.hour = (wxDateTime_t)(timeOnly / MIN_PER_HOUR);

    return tm;
}

wxDateTime& wxDateTime::SetYear(int year)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.year = year;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMonth(Month month)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.mon = month;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetDay(wxDateTime_t mday)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.mday = mday;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetHour(wxDateTime_t hour)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.hour = hour;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMinute(wxDateTime_t min)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.min = min;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetSecond(wxDateTime_t sec)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.sec = sec;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMillisecond(wxDateTime_t millisecond)
{
    wxASSERT_MSG( IsValid(), wxT("invalid wxDateTime") );

    // we don't need to use GetTm() for this one
    m_time -= m_time % 1000l;
    m_time += millisecond;

    return *this;
}

// ----------------------------------------------------------------------------
// wxDateTime arithmetics
// ----------------------------------------------------------------------------

wxDateTime& wxDateTime::Add(const wxDateSpan& diff)
{
    Tm tm(GetTm());

    tm.year += diff.GetYears();
    tm.AddMonths(diff.GetMonths());

    // check that the resulting date is valid
    if ( tm.mday > GetNumOfDaysInMonth(tm.year, tm.mon) )
    {
        // We suppose that when adding one month to Jan 31 we want to get Feb
        // 28 (or 29), i.e. adding a month to the last day of the month should
        // give the last day of the next month which is quite logical.
        //
        // Unfortunately, there is no logic way to understand what should
        // Jan 30 + 1 month be - Feb 28 too or Feb 27 (assuming non leap year)?
        // We make it Feb 28 (last day too), but it is highly questionable.
        tm.mday = GetNumOfDaysInMonth(tm.year, tm.mon);
    }

    tm.AddDays(diff.GetTotalDays());

    Set(tm);

    wxASSERT_MSG( IsSameTime(tm),
                  wxT("Add(wxDateSpan) shouldn't modify time") );

    return *this;
}

wxDateSpan wxDateTime::DiffAsDateSpan(const wxDateTime& dt) const
{
    wxASSERT_MSG( IsValid() && dt.IsValid(), wxT("invalid wxDateTime"));

    // If dt is larger than this, calculations below needs to be inverted.
    int inv = 1;
    if ( dt > *this )
        inv = -1;

    int y = GetYear() - dt.GetYear();
    int m = GetMonth() - dt.GetMonth();
    int d = GetDay() - dt.GetDay();

    // If month diff is negative, dt is the year before, so decrease year
    // and set month diff to its inverse, e.g. January - December should be 1,
    // not -11.
    if ( m * inv < 0 || (m == 0 && d * inv < 0))
    {
        m += inv * MONTHS_IN_YEAR;
        y -= inv;
    }

    // Same logic for days as for months above.
    if ( d * inv < 0 )
    {
        // Use number of days in month from the month which end date we're
        // crossing. That is month before this for positive diff, and this
        // month for negative diff.
        // If we're on january and using previous month, we get december
        // previous year, but don't care, december has same amount of days
        // every year.
        wxDateTime::Month monthfordays = GetMonth();
        if (inv > 0 && monthfordays == wxDateTime::Jan)
            monthfordays = wxDateTime::Dec;
        else if (inv > 0)
            monthfordays = static_cast<wxDateTime::Month>(monthfordays - 1);

        d += inv * wxDateTime::GetNumberOfDays(monthfordays, GetYear());
        m -= inv;
    }

    int w =  d / DAYS_PER_WEEK;

    // Remove weeks from d, since wxDateSpan only keep days as the ones
    // not in complete weeks
    d -= w * DAYS_PER_WEEK;

    return wxDateSpan(y, m, w, d);
}

// ----------------------------------------------------------------------------
// Weekday and monthday stuff
// ----------------------------------------------------------------------------

// convert Sun, Mon, ..., Sat into 6, 0, ..., 5
static inline int ConvertWeekDayToMondayBase(int wd)
{
    return wd == wxDateTime::Sun ? 6 : wd - 1;
}

/* static */
wxDateTime
wxDateTime::SetToWeekOfYear(int year, wxDateTime_t numWeek, WeekDay wd)
{
    wxASSERT_MSG( numWeek > 0,
                  wxT("invalid week number: weeks are counted from 1") );

    // Jan 4 always lies in the 1st week of the year
    wxDateTime dt(4, Jan, year);
    dt.SetToWeekDayInSameWeek(wd);
    dt += wxDateSpan::Weeks(numWeek - 1);

    return dt;
}

#if WXWIN_COMPATIBILITY_2_6
// use a separate function to avoid warnings about using deprecated
// SetToTheWeek in GetWeek below
static wxDateTime
SetToTheWeek(int year,
             wxDateTime::wxDateTime_t numWeek,
             wxDateTime::WeekDay weekday,
             wxDateTime::WeekFlags flags)
{
    // Jan 4 always lies in the 1st week of the year
    wxDateTime dt(4, wxDateTime::Jan, year);
    dt.SetToWeekDayInSameWeek(weekday, flags);
    dt += wxDateSpan::Weeks(numWeek - 1);

    return dt;
}

bool wxDateTime::SetToTheWeek(wxDateTime_t numWeek,
                              WeekDay weekday,
                              WeekFlags flags)
{
    int year = GetYear();
    *this = ::SetToTheWeek(year, numWeek, weekday, flags);
    if ( GetYear() != year )
    {
        // oops... numWeek was too big
        return false;
    }

    return true;
}

wxDateTime wxDateTime::GetWeek(wxDateTime_t numWeek,
                               WeekDay weekday,
                               WeekFlags flags) const
{
    return ::SetToTheWeek(GetYear(), numWeek, weekday, flags);
}
#endif // WXWIN_COMPATIBILITY_2_6

wxDateTime& wxDateTime::SetToLastMonthDay(Month month,
                                          int year)
{
    // take the current month/year if none specified
    if ( year == Inv_Year )
        year = GetYear();
    if ( month == Inv_Month )
        month = GetMonth();

    return Set(GetNumOfDaysInMonth(year, month), month, year);
}

wxDateTime& wxDateTime::SetToWeekDayInSameWeek(WeekDay weekday, WeekFlags flags)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, wxT("invalid weekday") );

    int wdayDst = weekday,
        wdayThis = GetWeekDay();
    if ( wdayDst == wdayThis )
    {
        // nothing to do
        return *this;
    }

    if ( flags == Default_First )
    {
        flags = GetCountry() == USA ? Sunday_First : Monday_First;
    }

    // the logic below based on comparing weekday and wdayThis works if Sun (0)
    // is the first day in the week, but breaks down for Monday_First case so
    // we adjust the week days in this case
    if ( flags == Monday_First )
    {
        if ( wdayThis == Sun )
            wdayThis += 7;
        if ( wdayDst == Sun )
            wdayDst += 7;
    }
    //else: Sunday_First, nothing to do

    // go forward or back in time to the day we want
    if ( wdayDst < wdayThis )
    {
        return Subtract(wxDateSpan::Days(wdayThis - wdayDst));
    }
    else // weekday > wdayThis
    {
        return Add(wxDateSpan::Days(wdayDst - wdayThis));
    }
}

wxDateTime& wxDateTime::SetToNextWeekDay(WeekDay weekday)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, wxT("invalid weekday") );

    int diff;
    WeekDay wdayThis = GetWeekDay();
    if ( weekday == wdayThis )
    {
        // nothing to do
        return *this;
    }
    else if ( weekday < wdayThis )
    {
        // need to advance a week
        diff = 7 - (wdayThis - weekday);
    }
    else // weekday > wdayThis
    {
        diff = weekday - wdayThis;
    }

    return Add(wxDateSpan::Days(diff));
}

wxDateTime& wxDateTime::SetToPrevWeekDay(WeekDay weekday)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, wxT("invalid weekday") );

    int diff;
    WeekDay wdayThis = GetWeekDay();
    if ( weekday == wdayThis )
    {
        // nothing to do
        return *this;
    }
    else if ( weekday > wdayThis )
    {
        // need to go to previous week
        diff = 7 - (weekday - wdayThis);
    }
    else // weekday < wdayThis
    {
        diff = wdayThis - weekday;
    }

    return Subtract(wxDateSpan::Days(diff));
}

bool wxDateTime::SetToWeekDay(WeekDay weekday,
                              int n,
                              Month month,
                              int year)
{
    wxCHECK_MSG( weekday != Inv_WeekDay, false, wxT("invalid weekday") );

    // we don't check explicitly that -5 <= n <= 5 because we will return false
    // anyhow in such case - but may be should still give an assert for it?

    // take the current month/year if none specified
    ReplaceDefaultYearMonthWithCurrent(&year, &month);

    wxDateTime dt;

    // TODO this probably could be optimised somehow...

    if ( n > 0 )
    {
        // get the first day of the month
        dt.Set(1, month, year);

        // get its wday
        WeekDay wdayFirst = dt.GetWeekDay();

        // go to the first weekday of the month
        int diff = weekday - wdayFirst;
        if ( diff < 0 )
            diff += 7;

        // add advance n-1 weeks more
        diff += 7*(n - 1);

        dt += wxDateSpan::Days(diff);
    }
    else // count from the end of the month
    {
        // get the last day of the month
        dt.SetToLastMonthDay(month, year);

        // get its wday
        WeekDay wdayLast = dt.GetWeekDay();

        // go to the last weekday of the month
        int diff = wdayLast - weekday;
        if ( diff < 0 )
            diff += 7;

        // and rewind n-1 weeks from there
        diff += 7*(-n - 1);

        dt -= wxDateSpan::Days(diff);
    }

    // check that it is still in the same month
    if ( dt.GetMonth() == month )
    {
        *this = dt;

        return true;
    }
    else
    {
        // no such day in this month
        return false;
    }
}

static inline
wxDateTime::wxDateTime_t GetDayOfYearFromTm(const wxDateTime::Tm& tm)
{
    return (wxDateTime::wxDateTime_t)(gs_cumulatedDays[wxDateTime::IsLeapYear(tm.year)][tm.mon] + tm.mday);
}

wxDateTime::wxDateTime_t wxDateTime::GetDayOfYear(const TimeZone& tz) const
{
    return GetDayOfYearFromTm(GetTm(tz));
}

wxDateTime::wxDateTime_t
wxDateTime::GetWeekOfYear(wxDateTime::WeekFlags flags, const TimeZone& tz) const
{
    if ( flags == Default_First )
    {
        flags = GetCountry() == USA ? Sunday_First : Monday_First;
    }

    Tm tm(GetTm(tz));
    wxDateTime_t nDayInYear = GetDayOfYearFromTm(tm);

    int wdTarget = GetWeekDay(tz);
    int wdYearStart = wxDateTime(1, Jan, GetYear()).GetWeekDay();
    int week;
    if ( flags == Sunday_First )
    {
        // FIXME: First week is not calculated correctly.
        week = (nDayInYear - wdTarget + 7) / 7;
        if ( wdYearStart == Wed || wdYearStart == Thu )
            week++;
    }
    else // week starts with monday
    {
        // adjust the weekdays to non-US style.
        wdYearStart = ConvertWeekDayToMondayBase(wdYearStart);

        // quoting from http://www.cl.cam.ac.uk/~mgk25/iso-time.html:
        //
        //      Week 01 of a year is per definition the first week that has the
        //      Thursday in this year, which is equivalent to the week that
        //      contains the fourth day of January. In other words, the first
        //      week of a new year is the week that has the majority of its
        //      days in the new year. Week 01 might also contain days from the
        //      previous year and the week before week 01 of a year is the last
        //      week (52 or 53) of the previous year even if it contains days
        //      from the new year. A week starts with Monday (day 1) and ends
        //      with Sunday (day 7).
        //

        // if Jan 1 is Thursday or less, it is in the first week of this year
        int dayCountFix = wdYearStart < 4 ? 6 : -1;

        // count the number of week
        week = (nDayInYear + wdYearStart + dayCountFix) / DAYS_PER_WEEK;

        // check if we happen to be at the last week of previous year:
        if ( week == 0 )
        {
            week = wxDateTime(31, Dec, GetYear() - 1).GetWeekOfYear();
        }
        else if ( week == 53 )
        {
            int wdYearEnd = (wdYearStart + 364 + IsLeapYear(GetYear()))
                                % DAYS_PER_WEEK;

            // Week 53 only if last day of year is Thursday or later.
            if ( wdYearEnd < 3 )
                week = 1;
        }
    }

    return (wxDateTime::wxDateTime_t)week;
}

wxDateTime::wxDateTime_t wxDateTime::GetWeekOfMonth(wxDateTime::WeekFlags flags,
                                                    const TimeZone& tz) const
{
    Tm tm = GetTm(tz);
    const wxDateTime dateFirst = wxDateTime(1, tm.mon, tm.year);
    const wxDateTime::WeekDay wdFirst = dateFirst.GetWeekDay();

    if ( flags == Default_First )
    {
        flags = GetCountry() == USA ? Sunday_First : Monday_First;
    }

    // compute offset of dateFirst from the beginning of the week
    int firstOffset;
    if ( flags == Sunday_First )
        firstOffset = wdFirst - Sun;
    else
        firstOffset = wdFirst == Sun ? DAYS_PER_WEEK - 1 : wdFirst - Mon;

    return (wxDateTime::wxDateTime_t)((tm.mday - 1 + firstOffset)/7 + 1);
}

wxDateTime& wxDateTime::SetToYearDay(wxDateTime::wxDateTime_t yday)
{
    int year = GetYear();
    wxDATETIME_CHECK( (0 < yday) && (yday <= GetNumberOfDays(year)),
                      wxT("invalid year day") );

    bool isLeap = IsLeapYear(year);
    for ( Month mon = Jan; mon < Inv_Month; wxNextMonth(mon) )
    {
        // for Dec, we can't compare with gs_cumulatedDays[mon + 1], but we
        // don't need it neither - because of the CHECK above we know that
        // yday lies in December then
        if ( (mon == Dec) || (yday <= gs_cumulatedDays[isLeap][mon + 1]) )
        {
            Set((wxDateTime::wxDateTime_t)(yday - gs_cumulatedDays[isLeap][mon]), mon, year);

            break;
        }
    }

    return *this;
}

// ----------------------------------------------------------------------------
// Julian day number conversion and related stuff
// ----------------------------------------------------------------------------

double wxDateTime::GetJulianDayNumber() const
{
    return m_time.ToDouble() / MILLISECONDS_PER_DAY + EPOCH_JDN + 0.5;
}

double wxDateTime::GetRataDie() const
{
    // March 1 of the year 0 is Rata Die day -306 and JDN 1721119.5
    return GetJulianDayNumber() - 1721119.5 - 306;
}

// ----------------------------------------------------------------------------
// timezone and DST stuff
// ----------------------------------------------------------------------------

int wxDateTime::IsDST(wxDateTime::Country country) const
{
    wxCHECK_MSG( country == Country_Default, -1,
                 wxT("country support not implemented") );

    // use the C RTL for the dates in the standard range
    time_t timet = GetTicks();
    if ( timet != (time_t)-1 )
    {
        struct tm tmstruct;
        tm *tm = wxLocaltime_r(&timet, &tmstruct);

        wxCHECK_MSG( tm, -1, wxT("wxLocaltime_r() failed") );

        return tm->tm_isdst;
    }
    else
    {
        int year = GetYear();

        if ( !IsDSTApplicable(year, country) )
        {
            // no DST time in this year in this country
            return -1;
        }

        return IsBetween(GetBeginDST(year, country), GetEndDST(year, country));
    }
}

wxDateTime& wxDateTime::MakeTimezone(const TimeZone& tz, bool noDST)
{
    long secDiff = wxGetTimeZone() + tz.GetOffset();

    // We are converting from the local time, but local time zone does not
    // include the DST offset (as it varies depending on the date), so we have
    // to handle DST manually, unless a special flag inhibiting this was
    // specified.
    //
    // Notice that we also shouldn't add the DST offset if we're already in the
    // local time zone, as indicated by offset of 0, converting from local time
    // to local time zone shouldn't change it, whether DST is in effect or not.
    if ( !noDST && secDiff && (IsDST() == 1) )
    {
        // FIXME we assume that the DST is always shifted by 1 hour
        secDiff -= 3600;
    }

    return Add(wxTimeSpan::Seconds(secDiff));
}

wxDateTime& wxDateTime::MakeFromTimezone(const TimeZone& tz, bool noDST)
{
    long secDiff = wxGetTimeZone() + tz.GetOffset();

    // See comment in MakeTimezone() above, the logic here is exactly the same.
    if ( !noDST && secDiff && (IsDST() == 1) )
    {
        // FIXME we assume that the DST is always shifted by 1 hour
        secDiff -= 3600;
    }

    return Subtract(wxTimeSpan::Seconds(secDiff));
}

// ============================================================================
// wxDateTimeHolidayAuthority and related classes
// ============================================================================

#include "wx/arrimpl.cpp"

WX_DEFINE_OBJARRAY(wxDateTimeArray)

static int wxCMPFUNC_CONV
wxDateTimeCompareFunc(wxDateTime **first, wxDateTime **second)
{
    wxDateTime dt1 = **first,
               dt2 = **second;

    return dt1 == dt2 ? 0 : dt1 < dt2 ? -1 : +1;
}

// ----------------------------------------------------------------------------
// wxDateTimeHolidayAuthority
// ----------------------------------------------------------------------------

wxHolidayAuthoritiesArray wxDateTimeHolidayAuthority::ms_authorities;

/* static */
bool wxDateTimeHolidayAuthority::IsHoliday(const wxDateTime& dt)
{
    size_t count = ms_authorities.size();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( ms_authorities[n]->DoIsHoliday(dt) )
        {
            return true;
        }
    }

    return false;
}

/* static */
size_t
wxDateTimeHolidayAuthority::GetHolidaysInRange(const wxDateTime& dtStart,
                                               const wxDateTime& dtEnd,
                                               wxDateTimeArray& holidays)
{
    wxDateTimeArray hol;

    holidays.Clear();

    const size_t countAuth = ms_authorities.size();
    for ( size_t nAuth = 0; nAuth < countAuth; nAuth++ )
    {
        ms_authorities[nAuth]->DoGetHolidaysInRange(dtStart, dtEnd, hol);

        WX_APPEND_ARRAY(holidays, hol);
    }

    holidays.Sort(wxDateTimeCompareFunc);

    return holidays.size();
}

/* static */
void wxDateTimeHolidayAuthority::ClearAllAuthorities()
{
    WX_CLEAR_ARRAY(ms_authorities);
}

/* static */
void wxDateTimeHolidayAuthority::AddAuthority(wxDateTimeHolidayAuthority *auth)
{
    ms_authorities.push_back(auth);
}

wxDateTimeHolidayAuthority::~wxDateTimeHolidayAuthority()
{
    // required here for Darwin
}

// ----------------------------------------------------------------------------
// wxDateTimeWorkDays
// ----------------------------------------------------------------------------

bool wxDateTimeWorkDays::DoIsHoliday(const wxDateTime& dt) const
{
    wxDateTime::WeekDay wd = dt.GetWeekDay();

    return (wd == wxDateTime::Sun) || (wd == wxDateTime::Sat);
}

size_t wxDateTimeWorkDays::DoGetHolidaysInRange(const wxDateTime& dtStart,
                                                const wxDateTime& dtEnd,
                                                wxDateTimeArray& holidays) const
{
    if ( dtStart > dtEnd )
    {
        wxFAIL_MSG( wxT("invalid date range in GetHolidaysInRange") );

        return 0u;
    }

    holidays.Empty();

    // instead of checking all days, start with the first Sat after dtStart and
    // end with the last Sun before dtEnd
    wxDateTime dtSatFirst = dtStart.GetNextWeekDay(wxDateTime::Sat),
               dtSatLast = dtEnd.GetPrevWeekDay(wxDateTime::Sat),
               dtSunFirst = dtStart.GetNextWeekDay(wxDateTime::Sun),
               dtSunLast = dtEnd.GetPrevWeekDay(wxDateTime::Sun),
               dt;

    for ( dt = dtSatFirst; dt <= dtSatLast; dt += wxDateSpan::Week() )
    {
        holidays.Add(dt);
    }

    for ( dt = dtSunFirst; dt <= dtSunLast; dt += wxDateSpan::Week() )
    {
        holidays.Add(dt);
    }

    return holidays.GetCount();
}

// ============================================================================
// other helper functions
// ============================================================================

// ----------------------------------------------------------------------------
// iteration helpers: can be used to write a for loop over enum variable like
// this:
//  for ( m = wxDateTime::Jan; m < wxDateTime::Inv_Month; wxNextMonth(m) )
// ----------------------------------------------------------------------------

WXDLLIMPEXP_BASE void wxNextMonth(wxDateTime::Month& m)
{
    wxASSERT_MSG( m < wxDateTime::Inv_Month, wxT("invalid month") );

    // no wrapping or the for loop above would never end!
    m = (wxDateTime::Month)(m + 1);
}

WXDLLIMPEXP_BASE void wxPrevMonth(wxDateTime::Month& m)
{
    wxASSERT_MSG( m < wxDateTime::Inv_Month, wxT("invalid month") );

    m = m == wxDateTime::Jan ? wxDateTime::Inv_Month
                             : (wxDateTime::Month)(m - 1);
}

WXDLLIMPEXP_BASE void wxNextWDay(wxDateTime::WeekDay& wd)
{
    wxASSERT_MSG( wd < wxDateTime::Inv_WeekDay, wxT("invalid week day") );

    // no wrapping or the for loop above would never end!
    wd = (wxDateTime::WeekDay)(wd + 1);
}

WXDLLIMPEXP_BASE void wxPrevWDay(wxDateTime::WeekDay& wd)
{
    wxASSERT_MSG( wd < wxDateTime::Inv_WeekDay, wxT("invalid week day") );

    wd = wd == wxDateTime::Sun ? wxDateTime::Inv_WeekDay
                               : (wxDateTime::WeekDay)(wd - 1);
}

#ifdef __WINDOWS__

wxDateTime& wxDateTime::SetFromMSWSysTime(const SYSTEMTIME& st)
{
    return Set(st.wDay,
            static_cast<wxDateTime::Month>(wxDateTime::Jan + st.wMonth - 1),
            st.wYear,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

wxDateTime& wxDateTime::SetFromMSWSysDate(const SYSTEMTIME& st)
{
    return Set(st.wDay,
            static_cast<wxDateTime::Month>(wxDateTime::Jan + st.wMonth - 1),
            st.wYear,
            0, 0, 0, 0);
}

void wxDateTime::GetAsMSWSysTime(SYSTEMTIME* st) const
{
    const wxDateTime::Tm tm(GetTm());

    st->wYear = (WXWORD)tm.year;
    st->wMonth = (WXWORD)(tm.mon - wxDateTime::Jan + 1);
    st->wDay = tm.mday;

    st->wDayOfWeek = 0;
    st->wHour = tm.hour;
    st->wMinute = tm.min;
    st->wSecond = tm.sec;
    st->wMilliseconds = tm.msec;
}

void wxDateTime::GetAsMSWSysDate(SYSTEMTIME* st) const
{
    const wxDateTime::Tm tm(GetTm());

    st->wYear = (WXWORD)tm.year;
    st->wMonth = (WXWORD)(tm.mon - wxDateTime::Jan + 1);
    st->wDay = tm.mday;

    st->wDayOfWeek =
    st->wHour =
    st->wMinute =
    st->wSecond =
    st->wMilliseconds = 0;
}

#endif // __WINDOWS__

#endif // wxUSE_DATETIME
