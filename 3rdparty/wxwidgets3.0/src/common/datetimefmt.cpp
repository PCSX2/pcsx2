///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/datetimefmt.cpp
// Purpose:     wxDateTime formatting & parsing code
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

#include <ctype.h>

#ifdef __WINDOWS__
    #include <winnls.h>
    #ifndef __WXWINCE__
        #include <locale.h>
    #endif
#endif

#include "wx/datetime.h"
#include "wx/time.h"

// ============================================================================
// implementation of wxDateTime
// ============================================================================

// ----------------------------------------------------------------------------
// helpers shared between datetime.cpp and datetimefmt.cpp
// ----------------------------------------------------------------------------

extern void InitTm(struct tm& tm);

extern wxString CallStrftime(const wxString& format, const tm* tm);

// ----------------------------------------------------------------------------
// constants (see also datetime.cpp)
// ----------------------------------------------------------------------------

static const int DAYS_PER_WEEK = 7;

static const int HOURS_PER_DAY = 24;

static const int SEC_PER_MIN = 60;

static const int MIN_PER_HOUR = 60;

// ----------------------------------------------------------------------------
// parsing helpers
// ----------------------------------------------------------------------------

namespace
{

// all the functions below taking non-const wxString::const_iterator p advance
// it until the end of the match

// scans all digits (but no more than len) and returns the resulting number
bool GetNumericToken(size_t len,
                     wxString::const_iterator& p,
                     const wxString::const_iterator& end,
                     unsigned long *number)
{
    size_t n = 1;
    wxString s;
    while ( p != end && wxIsdigit(*p) )
    {
        s += *p++;

        if ( len && ++n > len )
            break;
    }

    return !s.empty() && s.ToULong(number);
}

// scans all alphabetic characters and returns the resulting string
wxString
GetAlphaToken(wxString::const_iterator& p,
              const wxString::const_iterator& end)
{
    wxString s;
    while ( p != end && wxIsalpha(*p) )
    {
        s += *p++;
    }

    return s;
}

enum
{
    DateLang_English = 1,
    DateLang_Local   = 2
};

// return the month if the string is a month name or Inv_Month otherwise
//
// flags can contain wxDateTime::Name_Abbr/Name_Full or both of them and lang
// can be either DateLang_Local (default) to interpret string as a localized
// month name or DateLang_English to parse it as a standard English name or
// their combination to interpret it in any way
wxDateTime::Month
GetMonthFromName(wxString::const_iterator& p,
                 const wxString::const_iterator& end,
                 int flags,
                 int lang)
{
    const wxString::const_iterator pOrig = p;
    const wxString name = GetAlphaToken(p, end);
    if ( name.empty() )
        return wxDateTime::Inv_Month;

    wxDateTime::Month mon;
    for ( mon = wxDateTime::Jan; mon < wxDateTime::Inv_Month; wxNextMonth(mon) )
    {
        // case-insensitive comparison either one of or with both abbreviated
        // and not versions
        if ( flags & wxDateTime::Name_Full )
        {
            if ( lang & DateLang_English )
            {
                if ( name.CmpNoCase(wxDateTime::GetEnglishMonthName(mon,
                        wxDateTime::Name_Full)) == 0 )
                    break;
            }

            if ( lang & DateLang_Local )
            {
                if ( name.CmpNoCase(wxDateTime::GetMonthName(mon,
                        wxDateTime::Name_Full)) == 0 )
                    break;
            }
        }

        if ( flags & wxDateTime::Name_Abbr )
        {
            if ( lang & DateLang_English )
            {
                if ( name.CmpNoCase(wxDateTime::GetEnglishMonthName(mon,
                        wxDateTime::Name_Abbr)) == 0 )
                    break;
            }

            if ( lang & DateLang_Local )
            {
                // some locales (e.g. French one) use periods for the
                // abbreviated month names but it's never part of name so
                // compare it specially
                wxString nameAbbr = wxDateTime::GetMonthName(mon,
                    wxDateTime::Name_Abbr);
                const bool hasPeriod = *nameAbbr.rbegin() == '.';
                if ( hasPeriod )
                    nameAbbr.erase(nameAbbr.end() - 1);

                if ( name.CmpNoCase(nameAbbr) == 0 )
                {
                    if ( hasPeriod )
                    {
                        // skip trailing period if it was part of the match
                        if ( *p == '.' )
                            ++p;
                        else // no match as no matching period
                            continue;
                    }

                    break;
                }
            }
        }
    }

    if ( mon == wxDateTime::Inv_Month )
        p = pOrig;

    return mon;
}

// return the weekday if the string is a weekday name or Inv_WeekDay otherwise
//
// flags and lang parameters have the same meaning as for GetMonthFromName()
// above
wxDateTime::WeekDay
GetWeekDayFromName(wxString::const_iterator& p,
                   const wxString::const_iterator& end,
                   int flags, int lang)
{
    const wxString::const_iterator pOrig = p;
    const wxString name = GetAlphaToken(p, end);
    if ( name.empty() )
        return wxDateTime::Inv_WeekDay;

    wxDateTime::WeekDay wd;
    for ( wd = wxDateTime::Sun; wd < wxDateTime::Inv_WeekDay; wxNextWDay(wd) )
    {
        if ( flags & wxDateTime::Name_Full )
        {
            if ( lang & DateLang_English )
            {
                if ( name.CmpNoCase(wxDateTime::GetEnglishWeekDayName(wd,
                        wxDateTime::Name_Full)) == 0 )
                    break;
            }

            if ( lang & DateLang_Local )
            {
                if ( name.CmpNoCase(wxDateTime::GetWeekDayName(wd,
                        wxDateTime::Name_Full)) == 0 )
                    break;
            }
        }

        if ( flags & wxDateTime::Name_Abbr )
        {
            if ( lang & DateLang_English )
            {
                if ( name.CmpNoCase(wxDateTime::GetEnglishWeekDayName(wd,
                        wxDateTime::Name_Abbr)) == 0 )
                    break;
            }

            if ( lang & DateLang_Local )
            {
                if ( name.CmpNoCase(wxDateTime::GetWeekDayName(wd,
                        wxDateTime::Name_Abbr)) == 0 )
                    break;
            }
        }
    }

    if ( wd == wxDateTime::Inv_WeekDay )
        p = pOrig;

    return wd;
}

// return the year of the Monday of the week containing the given date
int
GetWeekBasedYear(const wxDateTime& dt)
{
    const wxDateTime::Tm tm = dt.GetTm();

    int year = tm.year;

    // The week-based year can only be different from the normal year for few
    // days in the beginning and the end of the year.
    if ( tm.yday > 361 )
    {
        if ( dt.GetWeekOfYear() == 1 )
            year++;
    }
    else if ( tm.yday < 5 )
    {
        if ( dt.GetWeekOfYear() == 53 )
            year--;
    }

    return year;
}

// parses string starting at given iterator using the specified format and,
// optionally, a fall back format (and optionally another one... but it stops
// there, really)
//
// if unsuccessful, returns invalid wxDateTime without changing p; otherwise
// advance p to the end of the match and returns wxDateTime containing the
// results of the parsing
wxDateTime
ParseFormatAt(wxString::const_iterator& p,
              const wxString::const_iterator& end,
              const wxString& fmt,
              // FIXME-VC6: using wxString() instead of wxEmptyString in the
              //            line below results in error C2062: type 'class
              //            wxString (__cdecl *)(void)' unexpected
              const wxString& fmtAlt = wxEmptyString)
{
    const wxString str(p, end);
    wxString::const_iterator endParse;
    wxDateTime dt;

    // Use a default date outside of the DST period to avoid problems with
    // parsing the time differently depending on the today's date (which is used
    // as the fall back date if none is explicitly specified).
    static const wxDateTime dtDef(1, wxDateTime::Jan, 2012);

    if ( dt.ParseFormat(str, fmt, dtDef, &endParse) ||
            (!fmtAlt.empty() && dt.ParseFormat(str, fmtAlt, dtDef, &endParse)) )
    {
        p += endParse - str.begin();
    }
    //else: all formats failed

    return dt;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxDateTime to/from text representations
// ----------------------------------------------------------------------------

wxString wxDateTime::Format(const wxString& formatp, const TimeZone& tz) const
{
    wxCHECK_MSG( !formatp.empty(), wxEmptyString,
                 wxT("NULL format in wxDateTime::Format") );

    wxString format = formatp;
#ifdef __WXOSX__
    if ( format.Contains("%c") )
        format.Replace("%c", wxLocale::GetInfo(wxLOCALE_DATE_TIME_FMT));
    if ( format.Contains("%x") )
        format.Replace("%x", wxLocale::GetInfo(wxLOCALE_SHORT_DATE_FMT));
    if ( format.Contains("%X") )
        format.Replace("%X", wxLocale::GetInfo(wxLOCALE_TIME_FMT));
#endif
    // we have to use our own implementation if the date is out of range of
    // strftime()
#ifdef wxHAS_STRFTIME
    time_t time = GetTicks();

    bool canUseStrftime = time != (time_t)-1;

    // We also can't use strftime() if we use non standard specifier: either
    // our own extension "%l" or one of "%g", "%G", "%V", "%z" which are POSIX
    // but not supported under Windows.
    for ( wxString::const_iterator p = format.begin();
          canUseStrftime && p != format.end();
          ++p )
    {
        if ( *p != '%' )
            continue;

        // set the default format
        switch ( (*++p).GetValue() )
        {
            case 'l':
#ifdef __WINDOWS__
            case 'g':
            case 'G':
            case 'V':
            case 'z':
#endif // __WINDOWS__
                canUseStrftime = false;
                break;
        }
    }

    if ( canUseStrftime )
    {
        // use strftime()
        struct tm tmstruct;
        struct tm *tm;
        if ( tz.GetOffset() == -wxGetTimeZone() )
        {
            // we are working with local time
            tm = wxLocaltime_r(&time, &tmstruct);

            // should never happen
            wxCHECK_MSG( tm, wxEmptyString, wxT("wxLocaltime_r() failed") );
        }
        else
        {
            time += (int)tz.GetOffset();

#if defined(__VMS__) || defined(__WATCOMC__) // time is unsigned so avoid warning
            int time2 = (int) time;
            if ( time2 >= 0 )
#else
            if ( time >= 0 )
#endif
            {
                tm = wxGmtime_r(&time, &tmstruct);

                // should never happen
                wxCHECK_MSG( tm, wxEmptyString, wxT("wxGmtime_r() failed") );
            }
            else
            {
                tm = (struct tm *)NULL;
            }
        }

        if ( tm )
        {
            return CallStrftime(format, tm);
        }
    }
    //else: use generic code below
#endif // wxHAS_STRFTIME

    // we only parse ANSI C format specifications here, no POSIX 2
    // complications, no GNU extensions but we do add support for a "%l" format
    // specifier allowing to get the number of milliseconds
    Tm tm = GetTm(tz);

    // used for calls to strftime() when we only deal with time
    struct tm tmTimeOnly;
    memset(&tmTimeOnly, 0, sizeof(tmTimeOnly));
    tmTimeOnly.tm_hour = tm.hour;
    tmTimeOnly.tm_min = tm.min;
    tmTimeOnly.tm_sec = tm.sec;
    tmTimeOnly.tm_mday = 1;         // any date will do, use 1976-01-01
    tmTimeOnly.tm_mon = 0;
    tmTimeOnly.tm_year = 76;
    tmTimeOnly.tm_isdst = 0;        // no DST, we adjust for tz ourselves

    wxString tmp, res, fmt;
    for ( wxString::const_iterator p = format.begin(); p != format.end(); ++p )
    {
        if ( *p != wxT('%') )
        {
            // copy as is
            res += *p;

            continue;
        }

        // set the default format
        switch ( (*++p).GetValue() )
        {
            case wxT('Y'):               // year has 4 digits
            case wxT('G'):               // (and ISO week year too)
            case wxT('z'):               // time zone as well
                fmt = wxT("%04d");
                break;

            case wxT('j'):               // day of year has 3 digits
            case wxT('l'):               // milliseconds have 3 digits
                fmt = wxT("%03d");
                break;

            case wxT('w'):               // week day as number has only one
                fmt = wxT("%d");
                break;

            default:
                // it's either another valid format specifier in which case
                // the format is "%02d" (for all the rest) or we have the
                // field width preceding the format in which case it will
                // override the default format anyhow
                fmt = wxT("%02d");
        }

        bool restart = true;
        while ( restart )
        {
            restart = false;

            // start of the format specification
            switch ( (*p).GetValue() )
            {
                case wxT('a'):       // a weekday name
                case wxT('A'):
                    // second parameter should be true for abbreviated names
                    res += GetWeekDayName(tm.GetWeekDay(),
                                          *p == wxT('a') ? Name_Abbr : Name_Full);
                    break;

                case wxT('b'):       // a month name
                case wxT('B'):
                    res += GetMonthName(tm.mon,
                                        *p == wxT('b') ? Name_Abbr : Name_Full);
                    break;

                case wxT('c'):       // locale default date and time  representation
                case wxT('x'):       // locale default date representation
#ifdef wxHAS_STRFTIME
                    //
                    // the problem: there is no way to know what do these format
                    // specifications correspond to for the current locale.
                    //
                    // the solution: use a hack and still use strftime(): first
                    // find the YEAR which is a year in the strftime() range (1970
                    // - 2038) whose Jan 1 falls on the same week day as the Jan 1
                    // of the real year. Then make a copy of the format and
                    // replace all occurrences of YEAR in it with some unique
                    // string not appearing anywhere else in it, then use
                    // strftime() to format the date in year YEAR and then replace
                    // YEAR back by the real year and the unique replacement
                    // string back with YEAR. Notice that "all occurrences of YEAR"
                    // means all occurrences of 4 digit as well as 2 digit form!
                    //
                    // the bugs: we assume that neither of %c nor %x contains any
                    // fields which may change between the YEAR and real year. For
                    // example, the week number (%U, %W) and the day number (%j)
                    // will change if one of these years is leap and the other one
                    // is not!
                    {
                        // find the YEAR: normally, for any year X, Jan 1 of the
                        // year X + 28 is the same weekday as Jan 1 of X (because
                        // the weekday advances by 1 for each normal X and by 2
                        // for each leap X, hence by 5 every 4 years or by 35
                        // which is 0 mod 7 every 28 years) but this rule breaks
                        // down if there are years between X and Y which are
                        // divisible by 4 but not leap (i.e. divisible by 100 but
                        // not 400), hence the correction.

                        int yearReal = GetYear(tz);
                        int mod28 = yearReal % 28;

                        // be careful to not go too far - we risk to leave the
                        // supported range
                        int year;
                        if ( mod28 < 10 )
                        {
                            year = 1988 + mod28;      // 1988 == 0 (mod 28)
                        }
                        else
                        {
                            year = 1970 + mod28 - 10; // 1970 == 10 (mod 28)
                        }

                        int nCentury = year / 100,
                            nCenturyReal = yearReal / 100;

                        // need to adjust for the years divisble by 400 which are
                        // not leap but are counted like leap ones if we just take
                        // the number of centuries in between for nLostWeekDays
                        int nLostWeekDays = (nCentury - nCenturyReal) -
                                            (nCentury / 4 - nCenturyReal / 4);

                        // we have to gain back the "lost" weekdays: note that the
                        // effect of this loop is to not do anything to
                        // nLostWeekDays (which we won't use any more), but to
                        // (indirectly) set the year correctly
                        while ( (nLostWeekDays % 7) != 0 )
                        {
                            nLostWeekDays += (year++ % 4) ? 1 : 2;
                        }

                        // finally move the year below 2000 so that the 2-digit
                        // year number can never match the month or day of the
                        // month when we do the replacements below
                        if ( year >= 2000 )
                            year -= 28;

                        wxASSERT_MSG( year >= 1970 && year < 2000,
                                      wxT("logic error in wxDateTime::Format") );


                        // use strftime() to format the same date but in supported
                        // year
                        //
                        // NB: we assume that strftime() doesn't check for the
                        //     date validity and will happily format the date
                        //     corresponding to Feb 29 of a non leap year (which
                        //     may happen if yearReal was leap and year is not)
                        struct tm tmAdjusted;
                        InitTm(tmAdjusted);
                        tmAdjusted.tm_hour = tm.hour;
                        tmAdjusted.tm_min = tm.min;
                        tmAdjusted.tm_sec = tm.sec;
                        tmAdjusted.tm_wday = tm.GetWeekDay();
                        tmAdjusted.tm_yday = GetDayOfYear();
                        tmAdjusted.tm_mday = tm.mday;
                        tmAdjusted.tm_mon = tm.mon;
                        tmAdjusted.tm_year = year - 1900;
                        tmAdjusted.tm_isdst = 0; // no DST, already adjusted
                        wxString str = CallStrftime(*p == wxT('c') ? wxT("%c")
                                                                  : wxT("%x"),
                                                    &tmAdjusted);

                        // now replace the replacement year with the real year:
                        // notice that we have to replace the 4 digit year with
                        // a unique string not appearing in strftime() output
                        // first to prevent the 2 digit year from matching any
                        // substring of the 4 digit year (but any day, month,
                        // hours or minutes components should be safe because
                        // they are never in 70-99 range)
                        wxString replacement("|");
                        while ( str.find(replacement) != wxString::npos )
                            replacement += '|';

                        str.Replace(wxString::Format("%d", year),
                                    replacement);
                        str.Replace(wxString::Format("%d", year % 100),
                                    wxString::Format("%d", yearReal % 100));
                        str.Replace(replacement,
                                    wxString::Format("%d", yearReal));

                        res += str;
                    }
#else // !wxHAS_STRFTIME
                    // Use "%m/%d/%y %H:%M:%S" format instead
                    res += wxString::Format(wxT("%02d/%02d/%04d %02d:%02d:%02d"),
                            tm.mon+1,tm.mday, tm.year, tm.hour, tm.min, tm.sec);
#endif // wxHAS_STRFTIME/!wxHAS_STRFTIME
                    break;

                case wxT('d'):       // day of a month (01-31)
                    res += wxString::Format(fmt, tm.mday);
                    break;

                case wxT('g'):      // 2-digit week-based year
                    res += wxString::Format(fmt, GetWeekBasedYear(*this) % 100);
                    break;

                case wxT('G'):       // week-based year with century
                    res += wxString::Format(fmt, GetWeekBasedYear(*this));
                    break;

                case wxT('H'):       // hour in 24h format (00-23)
                    res += wxString::Format(fmt, tm.hour);
                    break;

                case wxT('I'):       // hour in 12h format (01-12)
                    {
                        // 24h -> 12h, 0h -> 12h too
                        int hour12 = tm.hour > 12 ? tm.hour - 12
                                                  : tm.hour ? tm.hour : 12;
                        res += wxString::Format(fmt, hour12);
                    }
                    break;

                case wxT('j'):       // day of the year
                    res += wxString::Format(fmt, GetDayOfYear(tz));
                    break;

                case wxT('l'):       // milliseconds (NOT STANDARD)
                    res += wxString::Format(fmt, GetMillisecond(tz));
                    break;

                case wxT('m'):       // month as a number (01-12)
                    res += wxString::Format(fmt, tm.mon + 1);
                    break;

                case wxT('M'):       // minute as a decimal number (00-59)
                    res += wxString::Format(fmt, tm.min);
                    break;

                case wxT('p'):       // AM or PM string
#ifdef wxHAS_STRFTIME
                    res += CallStrftime(wxT("%p"), &tmTimeOnly);
#else // !wxHAS_STRFTIME
                    res += (tmTimeOnly.tm_hour > 12) ? wxT("pm") : wxT("am");
#endif // wxHAS_STRFTIME/!wxHAS_STRFTIME
                    break;

                case wxT('S'):       // second as a decimal number (00-61)
                    res += wxString::Format(fmt, tm.sec);
                    break;

                case wxT('U'):       // week number in the year (Sunday 1st week day)
                    res += wxString::Format(fmt, GetWeekOfYear(Sunday_First, tz));
                    break;

                case wxT('V'):       // ISO week number
                case wxT('W'):       // week number in the year (Monday 1st week day)
                    res += wxString::Format(fmt, GetWeekOfYear(Monday_First, tz));
                    break;

                case wxT('w'):       // weekday as a number (0-6), Sunday = 0
                    res += wxString::Format(fmt, tm.GetWeekDay());
                    break;

                // case wxT('x'): -- handled with "%c"

                case wxT('X'):       // locale default time representation
                    // just use strftime() to format the time for us
#ifdef wxHAS_STRFTIME
                    res += CallStrftime(wxT("%X"), &tmTimeOnly);
#else // !wxHAS_STRFTIME
                    res += wxString::Format(wxT("%02d:%02d:%02d"),tm.hour, tm.min, tm.sec);
#endif // wxHAS_STRFTIME/!wxHAS_STRFTIME
                    break;

                case wxT('y'):       // year without century (00-99)
                    res += wxString::Format(fmt, tm.year % 100);
                    break;

                case wxT('Y'):       // year with century
                    res += wxString::Format(fmt, tm.year);
                    break;

                case wxT('z'):       // time zone as [-+]HHMM
                    {
                        int ofs = tz.GetOffset();

                        // The time zone offset does not include the DST, but
                        // we do need to take it into account when showing the
                        // time in the local time zone to the user.
                        if ( ofs == -wxGetTimeZone() && IsDST() == 1 )
                        {
                            // FIXME: As elsewhere in wxDateTime, we assume
                            // that the DST is always 1 hour, but this is not
                            // true in general.
                            ofs += 3600;
                        }

                        if ( ofs < 0 )
                        {
                            res += '-';
                            ofs = -ofs;
                        }
                        else
                        {
                            res += '+';
                        }

                        // Converts seconds to HHMM representation.
                        res += wxString::Format(fmt,
                                                100*(ofs/3600) + (ofs/60)%60);
                    }
                    break;

                case wxT('Z'):       // timezone name
#ifdef wxHAS_STRFTIME
                    res += CallStrftime(wxT("%Z"), &tmTimeOnly);
#endif
                    break;

                default:
                    // is it the format width?
                    for ( fmt.clear();
                          *p == wxT('-') || *p == wxT('+') ||
                            *p == wxT(' ') || wxIsdigit(*p);
                          ++p )
                    {
                        fmt += *p;
                    }

                    if ( !fmt.empty() )
                    {
                        // we've only got the flags and width so far in fmt
                        fmt.Prepend(wxT('%'));
                        fmt.Append(wxT('d'));

                        restart = true;

                        break;
                    }

                    // no, it wasn't the width
                    wxFAIL_MSG(wxT("unknown format specifier"));

                    // fall through and just copy it nevertheless

                case wxT('%'):       // a percent sign
                    res += *p;
                    break;

                case 0:             // the end of string
                    wxFAIL_MSG(wxT("missing format at the end of string"));

                    // just put the '%' which was the last char in format
                    res += wxT('%');
                    break;
            }
        }
    }

    return res;
}

// this function parses a string in (strict) RFC 822 format: see the section 5
// of the RFC for the detailed description, but briefly it's something of the
// form "Sat, 18 Dec 1999 00:48:30 +0100"
//
// this function is "strict" by design - it must reject anything except true
// RFC822 time specs.
bool
wxDateTime::ParseRfc822Date(const wxString& date, wxString::const_iterator *end)
{
    const wxString::const_iterator pEnd = date.end();
    wxString::const_iterator p = date.begin();

    // 1. week day
    const wxDateTime::WeekDay
        wd = GetWeekDayFromName(p, pEnd, Name_Abbr, DateLang_English);
    if ( wd == Inv_WeekDay )
        return false;
    //else: ignore week day for now, we could also check that it really
    //      corresponds to the specified date

    // 2. separating comma
    if ( *p++ != ',' || *p++ != ' ' )
        return false;

    // 3. day number
    if ( !wxIsdigit(*p) )
        return false;

    wxDateTime_t day = (wxDateTime_t)(*p++ - '0');
    if ( wxIsdigit(*p) )
    {
        day *= 10;
        day = (wxDateTime_t)(day + (*p++ - '0'));
    }

    if ( *p++ != ' ' )
        return false;

    // 4. month name
    const Month mon = GetMonthFromName(p, pEnd, Name_Abbr, DateLang_English);
    if ( mon == Inv_Month )
        return false;

    if ( *p++ != ' ' )
        return false;

    // 5. year
    if ( !wxIsdigit(*p) )
        return false;

    int year = *p++ - '0';
    if ( !wxIsdigit(*p) ) // should have at least 2 digits in the year
        return false;

    year *= 10;
    year += *p++ - '0';

    // is it a 2 digit year (as per original RFC 822) or a 4 digit one?
    if ( wxIsdigit(*p) )
    {
        year *= 10;
        year += *p++ - '0';

        if ( !wxIsdigit(*p) )
        {
            // no 3 digit years please
            return false;
        }

        year *= 10;
        year += *p++ - '0';
    }

    if ( *p++ != ' ' )
        return false;

    // 6. time in hh:mm:ss format with seconds being optional
    if ( !wxIsdigit(*p) )
        return false;

    wxDateTime_t hour = (wxDateTime_t)(*p++ - '0');

    if ( !wxIsdigit(*p) )
        return false;

    hour *= 10;
    hour = (wxDateTime_t)(hour + (*p++ - '0'));

    if ( *p++ != ':' )
        return false;

    if ( !wxIsdigit(*p) )
        return false;

    wxDateTime_t min = (wxDateTime_t)(*p++ - '0');

    if ( !wxIsdigit(*p) )
        return false;

    min *= 10;
    min += (wxDateTime_t)(*p++ - '0');

    wxDateTime_t sec = 0;
    if ( *p == ':' )
    {
        p++;
        if ( !wxIsdigit(*p) )
            return false;

        sec = (wxDateTime_t)(*p++ - '0');

        if ( !wxIsdigit(*p) )
            return false;

        sec *= 10;
        sec += (wxDateTime_t)(*p++ - '0');
    }

    if ( *p++ != ' ' )
        return false;

    // 7. now the interesting part: the timezone
    int offset = 0; // just to suppress warnings
    if ( *p == '-' || *p == '+' )
    {
        // the explicit offset given: it has the form of hhmm
        bool plus = *p++ == '+';

        if ( !wxIsdigit(*p) || !wxIsdigit(*(p + 1)) )
            return false;


        // hours
        offset = MIN_PER_HOUR*(10*(*p - '0') + (*(p + 1) - '0'));

        p += 2;

        if ( !wxIsdigit(*p) || !wxIsdigit(*(p + 1)) )
            return false;

        // minutes
        offset += 10*(*p - '0') + (*(p + 1) - '0');

        if ( !plus )
            offset = -offset;

        p += 2;
    }
    else // not numeric
    {
        // the symbolic timezone given: may be either military timezone or one
        // of standard abbreviations
        if ( !*(p + 1) )
        {
            // military: Z = UTC, J unused, A = -1, ..., Y = +12
            static const int offsets[26] =
            {
                //A  B   C   D   E   F   G   H   I    J    K    L    M
                -1, -2, -3, -4, -5, -6, -7, -8, -9,   0, -10, -11, -12,
                //N  O   P   R   Q   S   T   U   V    W    Z    Y    Z
                +1, +2, +3, +4, +5, +6, +7, +8, +9, +10, +11, +12, 0
            };

            if ( *p < wxT('A') || *p > wxT('Z') || *p == wxT('J') )
                return false;

            offset = offsets[*p++ - 'A'];
        }
        else
        {
            // abbreviation
            const wxString tz(p, date.end());
            if ( tz == wxT("UT") || tz == wxT("UTC") || tz == wxT("GMT") )
                offset = 0;
            else if ( tz == wxT("AST") )
                offset = AST - GMT0;
            else if ( tz == wxT("ADT") )
                offset = ADT - GMT0;
            else if ( tz == wxT("EST") )
                offset = EST - GMT0;
            else if ( tz == wxT("EDT") )
                offset = EDT - GMT0;
            else if ( tz == wxT("CST") )
                offset = CST - GMT0;
            else if ( tz == wxT("CDT") )
                offset = CDT - GMT0;
            else if ( tz == wxT("MST") )
                offset = MST - GMT0;
            else if ( tz == wxT("MDT") )
                offset = MDT - GMT0;
            else if ( tz == wxT("PST") )
                offset = PST - GMT0;
            else if ( tz == wxT("PDT") )
                offset = PDT - GMT0;
            else
                return false;

            p += tz.length();
        }

        // make it minutes
        offset *= MIN_PER_HOUR;
    }


    // the spec was correct, construct the date from the values we found
    Set(day, mon, year, hour, min, sec);

    // As always, dealing with the time zone is the most interesting part: we
    // can't just use MakeFromTimeZone() here because it wouldn't handle the
    // DST correctly because the TZ specified in the string is DST-invariant
    // and so we have to manually shift to the UTC first and then convert to
    // the local TZ.
    *this -= wxTimeSpan::Minutes(offset);
    MakeFromUTC();

    if ( end )
        *end = p;

    return true;
}

const char* wxDateTime::ParseRfc822Date(const char* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseRfc822Date(dateStr, &end) )
        return NULL;

    return date + dateStr.IterOffsetInMBStr(end);
}

const wchar_t* wxDateTime::ParseRfc822Date(const wchar_t* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseRfc822Date(dateStr, &end) )
        return NULL;

    return date + (end - dateStr.begin());
}

bool
wxDateTime::ParseFormat(const wxString& date,
                        const wxString& format,
                        const wxDateTime& dateDef,
                        wxString::const_iterator *endParse)
{
    wxCHECK_MSG( !format.empty(), false, "format can't be empty" );
    wxCHECK_MSG( endParse, false, "end iterator pointer must be specified" );

    wxString str;
    unsigned long num;

    // what fields have we found?
    bool haveWDay = false,
         haveYDay = false,
         haveDay = false,
         haveMon = false,
         haveYear = false,
         haveHour = false,
         haveMin = false,
         haveSec = false,
         haveMsec = false;

    bool hourIsIn12hFormat = false, // or in 24h one?
         isPM = false;              // AM by default

    bool haveTimeZone = false;

    // and the value of the items we have (init them to get rid of warnings)
    wxDateTime_t msec = 0,
                 sec = 0,
                 min = 0,
                 hour = 0;
    WeekDay wday = Inv_WeekDay;
    wxDateTime_t yday = 0,
                 mday = 0;
    wxDateTime::Month mon = Inv_Month;
    int year = 0;
    long timeZone = 0;  // time zone in seconds as expected in Tm structure

    wxString::const_iterator input = date.begin();
    const wxString::const_iterator end = date.end();
    for ( wxString::const_iterator fmt = format.begin(); fmt != format.end(); ++fmt )
    {
        if ( *fmt != wxT('%') )
        {
            if ( wxIsspace(*fmt) )
            {
                // a white space in the format string matches 0 or more white
                // spaces in the input
                while ( input != end && wxIsspace(*input) )
                {
                    input++;
                }
            }
            else // !space
            {
                // any other character (not whitespace, not '%') must be
                // matched by itself in the input
                if ( input == end || *input++ != *fmt )
                {
                    // no match
                    return false;
                }
            }

            // done with this format char
            continue;
        }

        // start of a format specification

        // parse the optional width
        size_t width = 0;
        while ( wxIsdigit(*++fmt) )
        {
            width *= 10;
            width += *fmt - '0';
        }

        // the default widths for the various fields
        if ( !width )
        {
            switch ( (*fmt).GetValue() )
            {
                case wxT('Y'):               // year has 4 digits
                    width = 4;
                    break;

                case wxT('j'):               // day of year has 3 digits
                case wxT('l'):               // milliseconds have 3 digits
                    width = 3;
                    break;

                case wxT('w'):               // week day as number has only one
                    width = 1;
                    break;

                default:
                    // default for all other fields
                    width = 2;
            }
        }

        // then the format itself
        switch ( (*fmt).GetValue() )
        {
            case wxT('a'):       // a weekday name
            case wxT('A'):
                {
                    wday = GetWeekDayFromName
                           (
                            input, end,
                            *fmt == 'a' ? Name_Abbr : Name_Full,
                            DateLang_Local
                           );
                    if ( wday == Inv_WeekDay )
                    {
                        // no match
                        return false;
                    }
                }
                haveWDay = true;
                break;

            case wxT('b'):       // a month name
            case wxT('B'):
                {
                    mon = GetMonthFromName
                          (
                            input, end,
                            *fmt == 'b' ? Name_Abbr : Name_Full,
                            DateLang_Local
                          );
                    if ( mon == Inv_Month )
                    {
                        // no match
                        return false;
                    }
                }
                haveMon = true;
                break;

            case wxT('c'):       // locale default date and time  representation
                {
                    wxDateTime dt;

#if wxUSE_INTL
                    const wxString
                        fmtDateTime = wxLocale::GetInfo(wxLOCALE_DATE_TIME_FMT);
                    if ( !fmtDateTime.empty() )
                        dt = ParseFormatAt(input, end, fmtDateTime);
#endif // wxUSE_INTL
                    if ( !dt.IsValid() )
                    {
                        // also try the format which corresponds to ctime()
                        // output (i.e. the "C" locale default)
                        dt = ParseFormatAt(input, end, wxS("%a %b %d %H:%M:%S %Y"));
                    }

                    if ( !dt.IsValid() )
                    {
                        // and finally also the two generic date/time formats
                        dt = ParseFormatAt(input, end, wxS("%x %X"), wxS("%X %x"));
                    }

                    if ( !dt.IsValid() )
                        return false;

                    const Tm tm = dt.GetTm();

                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;

                    year = tm.year;
                    mon = tm.mon;
                    mday = tm.mday;

                    haveDay = haveMon = haveYear =
                    haveHour = haveMin = haveSec = true;
                }
                break;

            case wxT('d'):       // day of a month (01-31)
            case 'e':           // day of a month (1-31) (GNU extension)
                if ( !GetNumericToken(width, input, end, &num) ||
                        (num > 31) || (num < 1) )
                {
                    // no match
                    return false;
                }

                // we can't check whether the day range is correct yet, will
                // do it later - assume ok for now
                haveDay = true;
                mday = (wxDateTime_t)num;
                break;

            case wxT('H'):       // hour in 24h format (00-23)
                if ( !GetNumericToken(width, input, end, &num) || (num > 23) )
                {
                    // no match
                    return false;
                }

                haveHour = true;
                hour = (wxDateTime_t)num;
                break;

            case wxT('I'):       // hour in 12h format (01-12)
                if ( !GetNumericToken(width, input, end, &num) ||
                        !num || (num > 12) )
                {
                    // no match
                    return false;
                }

                haveHour = true;
                hourIsIn12hFormat = true;
                hour = (wxDateTime_t)(num % 12);        // 12 should be 0
                break;

            case wxT('j'):       // day of the year
                if ( !GetNumericToken(width, input, end, &num) ||
                        !num || (num > 366) )
                {
                    // no match
                    return false;
                }

                haveYDay = true;
                yday = (wxDateTime_t)num;
                break;

            case wxT('l'):       // milliseconds (0-999)
                if ( !GetNumericToken(width, input, end, &num) )
                    return false;

                haveMsec = true;
                msec = (wxDateTime_t)num;
                break;

            case wxT('m'):       // month as a number (01-12)
                if ( !GetNumericToken(width, input, end, &num) ||
                        !num || (num > 12) )
                {
                    // no match
                    return false;
                }

                haveMon = true;
                mon = (Month)(num - 1);
                break;

            case wxT('M'):       // minute as a decimal number (00-59)
                if ( !GetNumericToken(width, input, end, &num) ||
                        (num > 59) )
                {
                    // no match
                    return false;
                }

                haveMin = true;
                min = (wxDateTime_t)num;
                break;

            case wxT('p'):       // AM or PM string
                {
                    wxString am, pm;
                    GetAmPmStrings(&am, &pm);

                    // we can never match %p in locales which don't use AM/PM
                    if ( am.empty() || pm.empty() )
                        return false;

                    const size_t pos = input - date.begin();
                    if ( date.compare(pos, pm.length(), pm) == 0 )
                    {
                        isPM = true;
                        input += pm.length();
                    }
                    else if ( date.compare(pos, am.length(), am) == 0 )
                    {
                        input += am.length();
                    }
                    else // no match
                    {
                        return false;
                    }
                }
                break;

            case wxT('r'):       // time as %I:%M:%S %p
                {
                    wxDateTime dt;
                    if ( !dt.ParseFormat(wxString(input, end),
                                         wxS("%I:%M:%S %p"), &input) )
                        return false;

                    haveHour = haveMin = haveSec = true;

                    const Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;
                }
                break;

            case wxT('R'):       // time as %H:%M
                {
                    const wxDateTime
                        dt = ParseFormatAt(input, end, wxS("%H:%M"));
                    if ( !dt.IsValid() )
                        return false;

                    haveHour =
                    haveMin = true;

                    const Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                }
                break;

            case wxT('S'):       // second as a decimal number (00-61)
                if ( !GetNumericToken(width, input, end, &num) ||
                        (num > 61) )
                {
                    // no match
                    return false;
                }

                haveSec = true;
                sec = (wxDateTime_t)num;
                break;

            case wxT('T'):       // time as %H:%M:%S
                {
                    const wxDateTime
                        dt = ParseFormatAt(input, end, wxS("%H:%M:%S"));
                    if ( !dt.IsValid() )
                        return false;

                    haveHour =
                    haveMin =
                    haveSec = true;

                    const Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;
                }
                break;

            case wxT('w'):       // weekday as a number (0-6), Sunday = 0
                if ( !GetNumericToken(width, input, end, &num) ||
                        (wday > 6) )
                {
                    // no match
                    return false;
                }

                haveWDay = true;
                wday = (WeekDay)num;
                break;

            case wxT('x'):       // locale default date representation
                {
#if wxUSE_INTL
                    wxString
                        fmtDate = wxLocale::GetInfo(wxLOCALE_SHORT_DATE_FMT),
                        fmtDateAlt = wxLocale::GetInfo(wxLOCALE_LONG_DATE_FMT);
#else // !wxUSE_INTL
                    wxString fmtDate, fmtDateAlt;
#endif // wxUSE_INTL/!wxUSE_INTL
                    if ( fmtDate.empty() )
                    {
                        if ( IsWestEuropeanCountry(GetCountry()) ||
                             GetCountry() == Russia )
                        {
                            fmtDate = wxS("%d/%m/%Y");
                            fmtDateAlt = wxS("%m/%d/%Y");
                         }
                        else // assume USA
                        {
                            fmtDate = wxS("%m/%d/%Y");
                            fmtDateAlt = wxS("%d/%m/%Y");
                        }
                    }

                    wxDateTime
                        dt = ParseFormatAt(input, end, fmtDate, fmtDateAlt);

                    if ( !dt.IsValid() )
                    {
                        // try with short years too
                        fmtDate.Replace("%Y","%y");
                        fmtDateAlt.Replace("%Y","%y");
                        dt = ParseFormatAt(input, end, fmtDate, fmtDateAlt);

                        if ( !dt.IsValid() )
                            return false;
                    }

                    const Tm tm = dt.GetTm();

                    haveDay =
                    haveMon =
                    haveYear = true;

                    year = tm.year;
                    mon = tm.mon;
                    mday = tm.mday;
                }

                break;

            case wxT('X'):       // locale default time representation
                {
#if wxUSE_INTL
                    wxString fmtTime = wxLocale::GetInfo(wxLOCALE_TIME_FMT),
                             fmtTimeAlt;
#else // !wxUSE_INTL
                    wxString fmtTime, fmtTimeAlt;
#endif // wxUSE_INTL/!wxUSE_INTL
                    if ( fmtTime.empty() )
                    {
                        // try to parse what follows as "%H:%M:%S" and, if this
                        // fails, as "%I:%M:%S %p" - this should catch the most
                        // common cases
                        fmtTime = "%T";
                        fmtTimeAlt = "%r";
                    }

                    const wxDateTime
                        dt = ParseFormatAt(input, end, fmtTime, fmtTimeAlt);
                    if ( !dt.IsValid() )
                        return false;

                    haveHour =
                    haveMin =
                    haveSec = true;

                    const Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;
                }
                break;

            case wxT('y'):       // year without century (00-99)
                if ( !GetNumericToken(width, input, end, &num) ||
                        (num > 99) )
                {
                    // no match
                    return false;
                }

                haveYear = true;

                // TODO should have an option for roll over date instead of
                //      hard coding it here
                year = (num > 30 ? 1900 : 2000) + (wxDateTime_t)num;
                break;

            case wxT('Y'):       // year with century
                if ( !GetNumericToken(width, input, end, &num) )
                {
                    // no match
                    return false;
                }

                haveYear = true;
                year = (wxDateTime_t)num;
                break;

            case wxT('z'):
                {
                    // check that we have something here at all
                    if ( input == end )
                        return false;

                    // and then check that it's either plus or minus sign
                    bool minusFound;
                    if ( *input == wxT('-') )
                        minusFound = true;
                    else if ( *input == wxT('+') )
                        minusFound = false;
                    else
                        return false;   // no match

                    // here should follow 4 digits HHMM
                    ++input;
                    unsigned long tzHourMin;
                    if ( !GetNumericToken(4, input, end, &tzHourMin) )
                        return false;   // no match

                    const unsigned hours = tzHourMin / 100;
                    const unsigned minutes = tzHourMin % 100;

                    if ( hours > 12 || minutes > 59 )
                        return false;   // bad format

                    timeZone = 3600*hours + 60*minutes;
                    if ( minusFound )
                        timeZone = -timeZone;

                    haveTimeZone = true;
                }
                break;

            case wxT('Z'):       // timezone name
                // FIXME: currently we just ignore everything that looks like a
                //        time zone here
                GetAlphaToken(input, end);
                break;

            case wxT('%'):       // a percent sign
                if ( input == end || *input++ != wxT('%') )
                {
                    // no match
                    return false;
                }
                break;

            case 0:             // the end of string
                wxFAIL_MSG(wxT("unexpected format end"));

                // fall through

            default:            // not a known format spec
                return false;
        }
    }

    // format matched, try to construct a date from what we have now
    Tm tmDef;
    if ( dateDef.IsValid() )
    {
        // take this date as default
        tmDef = dateDef.GetTm();
    }
    else if ( IsValid() )
    {
        // if this date is valid, don't change it
        tmDef = GetTm();
    }
    else
    {
        // no default and this date is invalid - fall back to Today()
        tmDef = Today().GetTm();
    }

    Tm tm = tmDef;

    // set the date
    if ( haveMon )
    {
        tm.mon = mon;
    }

    if ( haveYear )
    {
        tm.year = year;
    }

    // TODO we don't check here that the values are consistent, if both year
    //      day and month/day were found, we just ignore the year day and we
    //      also always ignore the week day
    if ( haveDay )
    {
        if ( mday > GetNumberOfDays(tm.mon, tm.year) )
            return false;

        tm.mday = mday;
    }
    else if ( haveYDay )
    {
        if ( yday > GetNumberOfDays(tm.year) )
            return false;

        Tm tm2 = wxDateTime(1, Jan, tm.year).SetToYearDay(yday).GetTm();

        tm.mon = tm2.mon;
        tm.mday = tm2.mday;
    }

    // deal with AM/PM
    if ( haveHour && hourIsIn12hFormat && isPM )
    {
        // translate to 24hour format
        hour += 12;
    }
    //else: either already in 24h format or no translation needed

    // set the time
    if ( haveHour )
    {
        tm.hour = hour;
    }

    if ( haveMin )
    {
        tm.min = min;
    }

    if ( haveSec )
    {
        tm.sec = sec;
    }

    if ( haveMsec )
        tm.msec = msec;

    Set(tm);

    // If a time zone was specified and it is not the local time zone, we need
    // to shift the time accordingly.
    //
    // Note that avoiding the call to MakeFromTimeZone is necessary to avoid
    // DST problems.
    if ( haveTimeZone && timeZone != -wxGetTimeZone() )
        MakeFromTimezone(timeZone);

    // finally check that the week day is consistent -- if we had it
    if ( haveWDay && GetWeekDay() != wday )
        return false;

    *endParse = input;

    return true;
}

const char*
wxDateTime::ParseFormat(const char* date,
                        const wxString& format,
                        const wxDateTime& dateDef)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseFormat(dateStr, format, dateDef, &end) )
        return NULL;

    return date + dateStr.IterOffsetInMBStr(end);
}

const wchar_t*
wxDateTime::ParseFormat(const wchar_t* date,
                        const wxString& format,
                        const wxDateTime& dateDef)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseFormat(dateStr, format, dateDef, &end) )
        return NULL;

    return date + (end - dateStr.begin());
}

bool
wxDateTime::ParseDateTime(const wxString& date, wxString::const_iterator *end)
{
    wxCHECK_MSG( end, false, "end iterator pointer must be specified" );

    wxDateTime
        dtDate,
        dtTime;

    wxString::const_iterator
        endTime,
        endDate,
        endBoth;

    // If we got a date in the beginning, see if there is a time specified
    // after the date
    if ( dtDate.ParseDate(date, &endDate) )
    {
        // Skip spaces, as the ParseTime() function fails on spaces
        while ( endDate != date.end() && wxIsspace(*endDate) )
            ++endDate;

        const wxString timestr(endDate, date.end());
        if ( !dtTime.ParseTime(timestr, &endTime) )
            return false;

        endBoth = endDate + (endTime - timestr.begin());
    }
    else // no date in the beginning
    {
        // check if we have a time followed by a date
        if ( !dtTime.ParseTime(date, &endTime) )
            return false;

        while ( endTime != date.end() && wxIsspace(*endTime) )
            ++endTime;

        const wxString datestr(endTime, date.end());
        if ( !dtDate.ParseDate(datestr, &endDate) )
            return false;

        endBoth = endTime + (endDate - datestr.begin());
    }

    Set(dtDate.GetDay(), dtDate.GetMonth(), dtDate.GetYear(),
        dtTime.GetHour(), dtTime.GetMinute(), dtTime.GetSecond(),
        dtTime.GetMillisecond());

    *end = endBoth;

    return true;
}

const char* wxDateTime::ParseDateTime(const char* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseDateTime(dateStr, &end) )
        return NULL;

    return date + dateStr.IterOffsetInMBStr(end);
}

const wchar_t* wxDateTime::ParseDateTime(const wchar_t* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseDateTime(dateStr, &end) )
        return NULL;

    return date + (end - dateStr.begin());
}

bool
wxDateTime::ParseDate(const wxString& date, wxString::const_iterator *end)
{
    wxCHECK_MSG( end, false, "end iterator pointer must be specified" );

    // this is a simplified version of ParseDateTime() which understands only
    // "today" (for wxDate compatibility) and digits only otherwise (and not
    // all esoteric constructions ParseDateTime() knows about)

    const wxString::const_iterator pBegin = date.begin();
    const wxString::const_iterator pEnd = date.end();

    wxString::const_iterator p = pBegin;
    while ( p != pEnd && wxIsspace(*p) )
        p++;

    // some special cases
    static struct
    {
        const char *str;
        int dayDiffFromToday;
    } literalDates[] =
    {
        { wxTRANSLATE("today"),             0 },
        { wxTRANSLATE("yesterday"),        -1 },
        { wxTRANSLATE("tomorrow"),          1 },
    };

    const size_t lenRest = pEnd - p;
    for ( size_t n = 0; n < WXSIZEOF(literalDates); n++ )
    {
        const wxString dateStr = wxGetTranslation(literalDates[n].str);
        size_t len = dateStr.length();

        if ( len > lenRest )
            continue;

        const wxString::const_iterator pEndStr = p + len;
        if ( wxString(p, pEndStr).CmpNoCase(dateStr) == 0 )
        {
            // nothing can follow this, so stop here

            p = pEndStr;

            int dayDiffFromToday = literalDates[n].dayDiffFromToday;
            *this = Today();
            if ( dayDiffFromToday )
            {
                *this += wxDateSpan::Days(dayDiffFromToday);
            }

            *end = pEndStr;

            return true;
        }
    }

    // We try to guess what we have here: for each new (numeric) token, we
    // determine if it can be a month, day or a year. Of course, there is an
    // ambiguity as some numbers may be days as well as months, so we also
    // have the ability to back track.

    // what do we have?
    bool haveDay = false,       // the months day?
         haveWDay = false,      // the day of week?
         haveMon = false,       // the month?
         haveYear = false;      // the year?

    bool monWasNumeric = false; // was month specified as a number?

    // and the value of the items we have (init them to get rid of warnings)
    WeekDay wday = Inv_WeekDay;
    wxDateTime_t day = 0;
    wxDateTime::Month mon = Inv_Month;
    int year = 0;

    // tokenize the string
    while ( p != pEnd )
    {
        // skip white space and date delimiters
        if ( wxStrchr(".,/-\t\r\n ", *p) )
        {
            ++p;
            continue;
        }

        // modify copy of the iterator as we're not sure if the next token is
        // still part of the date at all
        wxString::const_iterator pCopy = p;

        // we can have either alphabetic or numeric token, start by testing if
        // it's the latter
        unsigned long val;
        if ( GetNumericToken(10 /* max length */, pCopy, pEnd, &val) )
        {
            // guess what this number is

            bool isDay = false,
                 isMonth = false,
                 isYear = false;

            if ( !haveMon && val > 0 && val <= 12 )
            {
                // assume it is month
                isMonth = true;
            }
            else // not the month
            {
                if ( haveDay )
                {
                    // this can only be the year
                    isYear = true;
                }
                else // may be either day or year
                {
                    // use a leap year if we don't have the year yet to allow
                    // dates like 2/29/1976 which would be rejected otherwise
                    wxDateTime_t max_days = (wxDateTime_t)(
                        haveMon
                        ? GetNumberOfDays(mon, haveYear ? year : 1976)
                        : 31
                    );

                    // can it be day?
                    if ( (val == 0) || (val > (unsigned long)max_days) )
                    {
                        // no
                        isYear = true;
                    }
                    else // yes, suppose it's the day
                    {
                        isDay = true;
                    }
                }
            }

            if ( isYear )
            {
                if ( haveYear )
                    break;

                haveYear = true;

                year = (wxDateTime_t)val;
            }
            else if ( isDay )
            {
                if ( haveDay )
                    break;

                haveDay = true;

                day = (wxDateTime_t)val;
            }
            else if ( isMonth )
            {
                haveMon = true;
                monWasNumeric = true;

                mon = (Month)(val - 1);
            }
        }
        else // not a number
        {
            // be careful not to overwrite the current mon value
            Month mon2 = GetMonthFromName
                         (
                            pCopy, pEnd,
                            Name_Full | Name_Abbr,
                            DateLang_Local | DateLang_English
                         );
            if ( mon2 != Inv_Month )
            {
                // it's a month
                if ( haveMon )
                {
                    // but we already have a month - maybe we guessed wrong
                    // when we had interpreted that numeric value as a month
                    // and it was the day number instead?
                    if ( haveDay || !monWasNumeric )
                        break;

                    // assume we did and change our mind: reinterpret the month
                    // value as a day (notice that there is no need to check
                    // that it is valid as month values are always < 12, but
                    // the days are counted from 1 unlike the months)
                    day = (wxDateTime_t)(mon + 1);
                    haveDay = true;
                }

                mon = mon2;

                haveMon = true;
            }
            else // not a valid month name
            {
                WeekDay wday2 = GetWeekDayFromName
                                (
                                    pCopy, pEnd,
                                    Name_Full | Name_Abbr,
                                    DateLang_Local | DateLang_English
                                );
                if ( wday2 != Inv_WeekDay )
                {
                    // a week day
                    if ( haveWDay )
                        break;

                    wday = wday2;

                    haveWDay = true;
                }
                else // not a valid weekday name
                {
                    // try the ordinals
                    static const char *const ordinals[] =
                    {
                        wxTRANSLATE("first"),
                        wxTRANSLATE("second"),
                        wxTRANSLATE("third"),
                        wxTRANSLATE("fourth"),
                        wxTRANSLATE("fifth"),
                        wxTRANSLATE("sixth"),
                        wxTRANSLATE("seventh"),
                        wxTRANSLATE("eighth"),
                        wxTRANSLATE("ninth"),
                        wxTRANSLATE("tenth"),
                        wxTRANSLATE("eleventh"),
                        wxTRANSLATE("twelfth"),
                        wxTRANSLATE("thirteenth"),
                        wxTRANSLATE("fourteenth"),
                        wxTRANSLATE("fifteenth"),
                        wxTRANSLATE("sixteenth"),
                        wxTRANSLATE("seventeenth"),
                        wxTRANSLATE("eighteenth"),
                        wxTRANSLATE("nineteenth"),
                        wxTRANSLATE("twentieth"),
                        // that's enough - otherwise we'd have problems with
                        // composite (or not) ordinals
                    };

                    size_t n;
                    for ( n = 0; n < WXSIZEOF(ordinals); n++ )
                    {
                        const wxString ord = wxGetTranslation(ordinals[n]);
                        const size_t len = ord.length();
                        if ( date.compare(p - pBegin, len, ord) == 0 )
                        {
                            p += len;
                            break;
                        }
                    }

                    if ( n == WXSIZEOF(ordinals) )
                    {
                        // stop here - something unknown
                        break;
                    }

                    // it's a day
                    if ( haveDay )
                    {
                        // don't try anything here (as in case of numeric day
                        // above) - the symbolic day spec should always
                        // precede the month/year
                        break;
                    }

                    haveDay = true;

                    day = (wxDateTime_t)(n + 1);
                }
            }
        }

        // advance iterator past a successfully parsed token
        p = pCopy;
    }

    // either no more tokens or the scan was stopped by something we couldn't
    // parse - in any case, see if we can construct a date from what we have
    if ( !haveDay && !haveWDay )
        return false;

    if ( haveWDay && (haveMon || haveYear || haveDay) &&
         !(haveDay && haveMon && haveYear) )
    {
        // without adjectives (which we don't support here) the week day only
        // makes sense completely separately or with the full date
        // specification (what would "Wed 1999" mean?)
        return false;
    }

    if ( !haveWDay && haveYear && !(haveDay && haveMon) )
    {
        // may be we have month and day instead of day and year?
        if ( haveDay && !haveMon )
        {
            if ( day <= 12  )
            {
                // exchange day and month
                mon = (wxDateTime::Month)(day - 1);

                // we're in the current year then
                if ( (year > 0) && (year <= (int)GetNumberOfDays(mon, Inv_Year)) )
                {
                    day = (wxDateTime_t)year;

                    haveMon = true;
                    haveYear = false;
                }
                //else: no, can't exchange, leave haveMon == false
            }
        }

        if ( !haveMon )
            return false;
    }

    if ( !haveMon )
    {
        mon = GetCurrentMonth();
    }

    if ( !haveYear )
    {
        year = GetCurrentYear();
    }

    if ( haveDay )
    {
        // normally we check the day above but the check is optimistic in case
        // we find the day before its month/year so we have to redo it now
        if ( day > GetNumberOfDays(mon, year) )
            return false;

        Set(day, mon, year);

        if ( haveWDay )
        {
            // check that it is really the same
            if ( GetWeekDay() != wday )
                return false;
        }
    }
    else // haveWDay
    {
        *this = Today();

        SetToWeekDayInSameWeek(wday);
    }

    *end = p;

    return true;
}

const char* wxDateTime::ParseDate(const char* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseDate(dateStr, &end) )
        return NULL;

    return date + dateStr.IterOffsetInMBStr(end);
}

const wchar_t* wxDateTime::ParseDate(const wchar_t* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseDate(dateStr, &end) )
        return NULL;

    return date + (end - dateStr.begin());
}

bool
wxDateTime::ParseTime(const wxString& time, wxString::const_iterator *end)
{
    wxCHECK_MSG( end, false, "end iterator pointer must be specified" );

    // first try some extra things
    static const struct
    {
        const char *name;
        wxDateTime_t hour;
    } stdTimes[] =
    {
        { wxTRANSLATE("noon"),      12 },
        { wxTRANSLATE("midnight"),  00 },
        // anything else?
    };

    for ( size_t n = 0; n < WXSIZEOF(stdTimes); n++ )
    {
        const wxString timeString = wxGetTranslation(stdTimes[n].name);
        if ( timeString.CmpNoCase(wxString(time, timeString.length())) == 0 )
        {
            // casts required by DigitalMars
            Set(stdTimes[n].hour, wxDateTime_t(0), wxDateTime_t(0));

            if ( end )
                *end = time.begin() + timeString.length();

            return true;
        }
    }

    // try all time formats we may think about in the order from longest to
    // shortest
    static const char *const timeFormats[] =
    {
        "%I:%M:%S %p",  // 12hour with AM/PM
        "%H:%M:%S",     // could be the same or 24 hour one so try it too
        "%I:%M %p",     // 12hour with AM/PM but without seconds
        "%H:%M",        // and a possibly 24 hour version without seconds
        "%I %p",        // just hour with AM/AM
        "%H",           // just hour in 24 hour version
        "%X",           // possibly something from above or maybe something
                        // completely different -- try it last

        // TODO: parse timezones
    };

    for ( size_t nFmt = 0; nFmt < WXSIZEOF(timeFormats); nFmt++ )
    {
        if ( ParseFormat(time, timeFormats[nFmt], end) )
            return true;
    }

    return false;
}

const char* wxDateTime::ParseTime(const char* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseTime(dateStr, &end) )
        return NULL;

    return date + dateStr.IterOffsetInMBStr(end);
}

const wchar_t* wxDateTime::ParseTime(const wchar_t* date)
{
    wxString::const_iterator end;
    wxString dateStr(date);
    if ( !ParseTime(dateStr, &end) )
        return NULL;

    return date + (end - dateStr.begin());
}

// ----------------------------------------------------------------------------
// Workdays and holidays support
// ----------------------------------------------------------------------------

bool wxDateTime::IsWorkDay(Country WXUNUSED(country)) const
{
    return !wxDateTimeHolidayAuthority::IsHoliday(*this);
}

// ============================================================================
// wxDateSpan
// ============================================================================

wxDateSpan WXDLLIMPEXP_BASE operator*(int n, const wxDateSpan& ds)
{
    wxDateSpan ds1(ds);
    return ds1.Multiply(n);
}

// ============================================================================
// wxTimeSpan
// ============================================================================

wxTimeSpan WXDLLIMPEXP_BASE operator*(int n, const wxTimeSpan& ts)
{
    return wxTimeSpan(ts).Multiply(n);
}

// this enum is only used in wxTimeSpan::Format() below but we can't declare
// it locally to the method as it provokes an internal compiler error in egcs
// 2.91.60 when building with -O2
enum TimeSpanPart
{
    Part_Week,
    Part_Day,
    Part_Hour,
    Part_Min,
    Part_Sec,
    Part_MSec
};

// not all strftime(3) format specifiers make sense here because, for example,
// a time span doesn't have a year nor a timezone
//
// Here are the ones which are supported (all of them are supported by strftime
// as well):
//  %H          hour in 24 hour format
//  %M          minute (00 - 59)
//  %S          second (00 - 59)
//  %%          percent sign
//
// Also, for MFC CTimeSpan compatibility, we support
//  %D          number of days
//
// And, to be better than MFC :-), we also have
//  %E          number of wEeks
//  %l          milliseconds (000 - 999)
wxString wxTimeSpan::Format(const wxString& format) const
{
    // we deal with only positive time spans here and just add the sign in
    // front for the negative ones
    if ( IsNegative() )
    {
        wxString str(Negate().Format(format));
        return "-" + str;
    }

    wxCHECK_MSG( !format.empty(), wxEmptyString,
                 wxT("NULL format in wxTimeSpan::Format") );

    wxString str;
    str.Alloc(format.length());

    // Suppose we have wxTimeSpan ts(1 /* hour */, 2 /* min */, 3 /* sec */)
    //
    // Then, of course, ts.Format("%H:%M:%S") must return "01:02:03", but the
    // question is what should ts.Format("%S") do? The code here returns "3273"
    // in this case (i.e. the total number of seconds, not just seconds % 60)
    // because, for me, this call means "give me entire time interval in
    // seconds" and not "give me the seconds part of the time interval"
    //
    // If we agree that it should behave like this, it is clear that the
    // interpretation of each format specifier depends on the presence of the
    // other format specs in the string: if there was "%H" before "%M", we
    // should use GetMinutes() % 60, otherwise just GetMinutes() &c

    // we remember the most important unit found so far
    TimeSpanPart partBiggest = Part_MSec;

    for ( wxString::const_iterator pch = format.begin(); pch != format.end(); ++pch )
    {
        wxChar ch = *pch;

        if ( ch == wxT('%') )
        {
            // the start of the format specification of the printf() below
            wxString fmtPrefix(wxT('%'));

            // the number
            long n;

            // the number of digits for the format string, 0 if unused
            unsigned digits = 0;

            ch = *++pch;    // get the format spec char
            switch ( ch )
            {
                default:
                    wxFAIL_MSG( wxT("invalid format character") );
                    // fall through

                case wxT('%'):
                    str += ch;

                    // skip the part below switch
                    continue;

                case wxT('D'):
                    n = GetDays();
                    if ( partBiggest < Part_Day )
                    {
                        n %= DAYS_PER_WEEK;
                    }
                    else
                    {
                        partBiggest = Part_Day;
                    }
                    break;

                case wxT('E'):
                    partBiggest = Part_Week;
                    n = GetWeeks();
                    break;

                case wxT('H'):
                    n = GetHours();
                    if ( partBiggest < Part_Hour )
                    {
                        n %= HOURS_PER_DAY;
                    }
                    else
                    {
                        partBiggest = Part_Hour;
                    }

                    digits = 2;
                    break;

                case wxT('l'):
                    n = GetMilliseconds().ToLong();
                    if ( partBiggest < Part_MSec )
                    {
                        n %= 1000;
                    }
                    //else: no need to reset partBiggest to Part_MSec, it is
                    //      the least significant one anyhow

                    digits = 3;
                    break;

                case wxT('M'):
                    n = GetMinutes();
                    if ( partBiggest < Part_Min )
                    {
                        n %= MIN_PER_HOUR;
                    }
                    else
                    {
                        partBiggest = Part_Min;
                    }

                    digits = 2;
                    break;

                case wxT('S'):
                    n = GetSeconds().ToLong();
                    if ( partBiggest < Part_Sec )
                    {
                        n %= SEC_PER_MIN;
                    }
                    else
                    {
                        partBiggest = Part_Sec;
                    }

                    digits = 2;
                    break;
            }

            if ( digits )
            {
                fmtPrefix << wxT("0") << digits;
            }

            str += wxString::Format(fmtPrefix + wxT("ld"), n);
        }
        else
        {
            // normal character, just copy
            str += ch;
        }
    }

    return str;
}

#endif // wxUSE_DATETIME
