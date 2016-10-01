///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/cmdline.cpp
// Purpose:     wxCmdLineParser implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.01.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/app.h"
#endif //WX_PRECOMP

#include "wx/cmdline.h"

#if wxUSE_CMDLINE_PARSER

#include <ctype.h>
#include <locale.h>             // for LC_ALL

#include "wx/datetime.h"
#include "wx/msgout.h"
#include "wx/filename.h"
#include "wx/apptrait.h"
#include "wx/scopeguard.h"

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

static wxString GetTypeName(wxCmdLineParamType type);

static wxString GetOptionName(wxString::const_iterator p,
                              wxString::const_iterator end,
                              const wxChar *allowedChars);

static wxString GetShortOptionName(wxString::const_iterator p,
                                   wxString::const_iterator end);

static wxString GetLongOptionName(wxString::const_iterator p,
                                  wxString::const_iterator end);

// ----------------------------------------------------------------------------
// private structs
// ----------------------------------------------------------------------------

// an internal representation of an option
struct wxCmdLineOption
{
    wxCmdLineOption(wxCmdLineEntryType k,
                    const wxString& shrt,
                    const wxString& lng,
                    const wxString& desc,
                    wxCmdLineParamType typ,
                    int fl)
    {
        // wxCMD_LINE_USAGE_TEXT uses only description, shortName and longName is empty
        if ( k != wxCMD_LINE_USAGE_TEXT )
        {
            wxASSERT_MSG
            (
                !shrt.empty() || !lng.empty(),
                 wxT("option should have at least one name")
            );

            wxASSERT_MSG
            (
                GetShortOptionName(shrt.begin(), shrt.end()).Len() == shrt.Len(),
                wxT("Short option contains invalid characters")
            );

            wxASSERT_MSG
            (
                GetLongOptionName(lng.begin(), lng.end()).Len() == lng.Len(),
                wxT("Long option contains invalid characters")
            );
        }

        kind = k;

        shortName = shrt;
        longName = lng;
        description = desc;

        type = typ;
        flags = fl;

        Reset();
    }

    // can't use union easily here, so just store all possible data fields, we
    // don't waste much (might still use union later if the number of supported
    // types increases, so always use the accessor functions and don't access
    // the fields directly!)

    void Check(wxCmdLineParamType WXUNUSED_UNLESS_DEBUG(typ)) const
    {
        wxASSERT_MSG( type == typ, wxT("type mismatch in wxCmdLineOption") );
    }

    double GetDoubleVal() const
        { Check(wxCMD_LINE_VAL_DOUBLE); return m_doubleVal; }
    long GetLongVal() const
        { Check(wxCMD_LINE_VAL_NUMBER); return m_longVal; }
    const wxString& GetStrVal() const
        { Check(wxCMD_LINE_VAL_STRING); return m_strVal;  }
#if wxUSE_DATETIME
    const wxDateTime& GetDateVal() const
        { Check(wxCMD_LINE_VAL_DATE);   return m_dateVal; }
#endif // wxUSE_DATETIME

    void SetDoubleVal(double val)
        { Check(wxCMD_LINE_VAL_DOUBLE); m_doubleVal = val; m_hasVal = true; }
    void SetLongVal(long val)
        { Check(wxCMD_LINE_VAL_NUMBER); m_longVal = val; m_hasVal = true; }
    void SetStrVal(const wxString& val)
        { Check(wxCMD_LINE_VAL_STRING); m_strVal = val; m_hasVal = true; }
#if wxUSE_DATETIME
    void SetDateVal(const wxDateTime& val)
        { Check(wxCMD_LINE_VAL_DATE); m_dateVal = val; m_hasVal = true; }
#endif // wxUSE_DATETIME

    void SetHasValue() { m_hasVal = true; }
    bool HasValue() const { return m_hasVal; }

    void SetNegated() { m_isNegated = true; }
    bool IsNegated() const
    {
        wxASSERT_MSG( kind == wxCMD_LINE_SWITCH,
                      wxT("kind mismatch in wxCmdLineArg") );

        return m_isNegated;
    }

    // Reset to the initial state, called before parsing another command line.
    void Reset()
    {
        m_hasVal =
        m_isNegated = false;
    }

public:
    wxCmdLineEntryType kind;
    wxString shortName,
             longName,
             description;
    wxCmdLineParamType type;
    int flags;

private:
    bool m_hasVal;
    bool m_isNegated;

    double m_doubleVal;
    long m_longVal;
    wxString m_strVal;
#if wxUSE_DATETIME
    wxDateTime m_dateVal;
#endif // wxUSE_DATETIME
};

struct wxCmdLineParam
{
    wxCmdLineParam(const wxString& desc,
                   wxCmdLineParamType typ,
                   int fl)
        : description(desc)
    {
        type = typ;
        flags = fl;
    }

    wxString description;
    wxCmdLineParamType type;
    int flags;
};

WX_DECLARE_OBJARRAY(wxCmdLineOption, wxArrayOptions);
WX_DECLARE_OBJARRAY(wxCmdLineParam, wxArrayParams);

#include "wx/arrimpl.cpp"

WX_DEFINE_OBJARRAY(wxArrayOptions)
WX_DEFINE_OBJARRAY(wxArrayParams)

// the parser internal state
struct wxCmdLineParserData
{
    // options
    wxString m_switchChars;     // characters which may start an option
    bool m_enableLongOptions;   // true if long options are enabled
    wxString m_logo;            // some extra text to show in Usage()

    // cmd line data
    wxArrayString m_arguments;  // == argv, argc == m_arguments.GetCount()
    wxArrayOptions m_options;   // all possible options and switches
    wxArrayParams m_paramDesc;  // description of all possible params
    wxArrayString m_parameters; // all params found

    // methods
    wxCmdLineParserData();
    void SetArguments(int argc, char **argv);
#if wxUSE_UNICODE
    void SetArguments(int argc, wxChar **argv);
    void SetArguments(int argc, const wxCmdLineArgsArray& argv);
#endif // wxUSE_UNICODE
    void SetArguments(const wxString& cmdline);

    int FindOption(const wxString& name);
    int FindOptionByLongName(const wxString& name);

    // Find the option by either its short or long name.
    //
    // Asserts and returns NULL if option with this name is not found.
    const wxCmdLineOption* FindOptionByAnyName(const wxString& name);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxCmdLineParserData
// ----------------------------------------------------------------------------

wxCmdLineParserData::wxCmdLineParserData()
{
    m_enableLongOptions = true;
#ifdef __UNIX_LIKE__
    m_switchChars = wxT("-");
#else // !Unix
    m_switchChars = wxT("/-");
#endif
}

namespace
{

// Small helper function setting locale for all categories.
//
// We define it because wxSetlocale() can't be easily used with wxScopeGuard as
// it has several overloads -- while this one can.
inline char *SetAllLocaleFacets(const char *loc)
{
    return wxSetlocale(LC_ALL, loc);
}

} // private namespace

void wxCmdLineParserData::SetArguments(int argc, char **argv)
{
    m_arguments.clear();

    // Command-line arguments are supposed to be in the user locale encoding
    // (what else?) but wxLocale probably wasn't initialized yet as we're
    // called early during the program startup and so our locale might not have
    // been set from the environment yet. To work around this problem we
    // temporarily change the locale here. The only drawback is that changing
    // the locale is thread-unsafe but precisely because we're called so early
    // it's hopefully safe to assume that no other threads had been created yet.
    char * const locOld = SetAllLocaleFacets(NULL);
    SetAllLocaleFacets("");
    wxON_BLOCK_EXIT1( SetAllLocaleFacets, locOld );

    for ( int n = 0; n < argc; n++ )
    {
        // try to interpret the string as being in the current locale
        wxString arg(argv[n]);

        // but just in case we guessed wrongly and the conversion failed, do
        // try to salvage at least something
        if ( arg.empty() && argv[n][0] != '\0' )
            arg = wxString(argv[n], wxConvISO8859_1);

        m_arguments.push_back(arg);
    }
}

#if wxUSE_UNICODE

void wxCmdLineParserData::SetArguments(int argc, wxChar **argv)
{
    m_arguments.clear();

    for ( int n = 0; n < argc; n++ )
    {
        m_arguments.push_back(argv[n]);
    }
}

void wxCmdLineParserData::SetArguments(int WXUNUSED(argc),
                                       const wxCmdLineArgsArray& argv)
{
    m_arguments = argv.GetArguments();
}

#endif // wxUSE_UNICODE

void wxCmdLineParserData::SetArguments(const wxString& cmdLine)
{
    m_arguments.clear();

    if(wxTheApp && wxTheApp->argc > 0)
        m_arguments.push_back(wxTheApp->argv[0]);
    else
        m_arguments.push_back(wxEmptyString);

    wxArrayString args = wxCmdLineParser::ConvertStringToArgs(cmdLine);

    WX_APPEND_ARRAY(m_arguments, args);
}

int wxCmdLineParserData::FindOption(const wxString& name)
{
    if ( !name.empty() )
    {
        size_t count = m_options.GetCount();
        for ( size_t n = 0; n < count; n++ )
        {
            if ( m_options[n].shortName == name )
            {
                // found
                return n;
            }
        }
    }

    return wxNOT_FOUND;
}

int wxCmdLineParserData::FindOptionByLongName(const wxString& name)
{
    size_t count = m_options.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( m_options[n].longName == name )
        {
            // found
            return n;
        }
    }

    return wxNOT_FOUND;
}

const wxCmdLineOption*
wxCmdLineParserData::FindOptionByAnyName(const wxString& name)
{
    int i = FindOption(name);
    if ( i == wxNOT_FOUND )
    {
        i = FindOptionByLongName(name);

        if ( i == wxNOT_FOUND )
        {
            wxFAIL_MSG( wxS("Unknown option ") + name );
            return NULL;
        }
    }

    return &m_options[(size_t)i];
}

// ----------------------------------------------------------------------------
// construction and destruction
// ----------------------------------------------------------------------------

void wxCmdLineParser::Init()
{
    m_data = new wxCmdLineParserData;
}

void wxCmdLineParser::SetCmdLine(int argc, char **argv)
{
    m_data->SetArguments(argc, argv);
}

#if wxUSE_UNICODE

void wxCmdLineParser::SetCmdLine(int argc, wxChar **argv)
{
    m_data->SetArguments(argc, argv);
}

void wxCmdLineParser::SetCmdLine(int argc, const wxCmdLineArgsArray& argv)
{
    m_data->SetArguments(argc, argv);
}

#endif // wxUSE_UNICODE

void wxCmdLineParser::SetCmdLine(const wxString& cmdline)
{
    m_data->SetArguments(cmdline);
}

wxCmdLineParser::~wxCmdLineParser()
{
    delete m_data;
}

// ----------------------------------------------------------------------------
// options
// ----------------------------------------------------------------------------

void wxCmdLineParser::SetSwitchChars(const wxString& switchChars)
{
    m_data->m_switchChars = switchChars;
}

void wxCmdLineParser::EnableLongOptions(bool enable)
{
    m_data->m_enableLongOptions = enable;
}

bool wxCmdLineParser::AreLongOptionsEnabled() const
{
    return m_data->m_enableLongOptions;
}

void wxCmdLineParser::SetLogo(const wxString& logo)
{
    m_data->m_logo = logo;
}

// ----------------------------------------------------------------------------
// command line construction
// ----------------------------------------------------------------------------

void wxCmdLineParser::SetDesc(const wxCmdLineEntryDesc *desc)
{
    for ( ;; desc++ )
    {
        switch ( desc->kind )
        {
            case wxCMD_LINE_SWITCH:
                AddSwitch(desc->shortName, desc->longName,
                          wxGetTranslation(desc->description),
                          desc->flags);
                break;

            case wxCMD_LINE_OPTION:
                AddOption(desc->shortName, desc->longName,
                          wxGetTranslation(desc->description),
                          desc->type, desc->flags);
                break;

            case wxCMD_LINE_PARAM:
                AddParam(wxGetTranslation(desc->description),
                         desc->type, desc->flags);
                break;

            case wxCMD_LINE_USAGE_TEXT:
                AddUsageText(wxGetTranslation(desc->description));
                break;

            default:
                wxFAIL_MSG( wxT("unknown command line entry type") );
                // still fall through

            case wxCMD_LINE_NONE:
                return;
        }
    }
}

void wxCmdLineParser::AddSwitch(const wxString& shortName,
                                const wxString& longName,
                                const wxString& desc,
                                int flags)
{
    wxASSERT_MSG( m_data->FindOption(shortName) == wxNOT_FOUND,
                  wxT("duplicate switch") );

    wxCmdLineOption *option = new wxCmdLineOption(wxCMD_LINE_SWITCH,
                                                  shortName, longName, desc,
                                                  wxCMD_LINE_VAL_NONE, flags);

    m_data->m_options.Add(option);
}

void wxCmdLineParser::AddOption(const wxString& shortName,
                                const wxString& longName,
                                const wxString& desc,
                                wxCmdLineParamType type,
                                int flags)
{
    wxASSERT_MSG( m_data->FindOption(shortName) == wxNOT_FOUND,
                  wxT("duplicate option") );

    wxCmdLineOption *option = new wxCmdLineOption(wxCMD_LINE_OPTION,
                                                  shortName, longName, desc,
                                                  type, flags);

    m_data->m_options.Add(option);
}

void wxCmdLineParser::AddParam(const wxString& desc,
                               wxCmdLineParamType type,
                               int flags)
{
    // do some consistency checks: a required parameter can't follow an
    // optional one and nothing should follow a parameter with MULTIPLE flag
#if wxDEBUG_LEVEL
    if ( !m_data->m_paramDesc.IsEmpty() )
    {
        wxCmdLineParam& param = m_data->m_paramDesc.Last();

        wxASSERT_MSG( !(param.flags & wxCMD_LINE_PARAM_MULTIPLE),
                      wxT("all parameters after the one with wxCMD_LINE_PARAM_MULTIPLE style will be ignored") );

        if ( !(flags & wxCMD_LINE_PARAM_OPTIONAL) )
        {
            wxASSERT_MSG( !(param.flags & wxCMD_LINE_PARAM_OPTIONAL),
                          wxT("a required parameter can't follow an optional one") );
        }
    }
#endif // wxDEBUG_LEVEL

    wxCmdLineParam *param = new wxCmdLineParam(desc, type, flags);

    m_data->m_paramDesc.Add(param);
}

void wxCmdLineParser::AddUsageText(const wxString& text)
{
    wxASSERT_MSG( !text.empty(), wxT("text can't be empty") );

    wxCmdLineOption *option = new wxCmdLineOption(wxCMD_LINE_USAGE_TEXT,
                                                  wxEmptyString, wxEmptyString,
                                                  text, wxCMD_LINE_VAL_NONE, 0);

    m_data->m_options.Add(option);
}

// ----------------------------------------------------------------------------
// access to parse command line
// ----------------------------------------------------------------------------

bool wxCmdLineParser::Found(const wxString& name) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    return opt && opt->HasValue();
}

wxCmdLineSwitchState wxCmdLineParser::FoundSwitch(const wxString& name) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    if ( !opt || !opt->HasValue() )
        return wxCMD_SWITCH_NOT_FOUND;

    return opt->IsNegated() ? wxCMD_SWITCH_OFF : wxCMD_SWITCH_ON;
}

bool wxCmdLineParser::Found(const wxString& name, wxString *value) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    if ( !opt || !opt->HasValue() )
        return false;

    wxCHECK_MSG( value, false, wxT("NULL pointer in wxCmdLineOption::Found") );

    *value = opt->GetStrVal();

    return true;
}

bool wxCmdLineParser::Found(const wxString& name, long *value) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    if ( !opt || !opt->HasValue() )
        return false;

    wxCHECK_MSG( value, false, wxT("NULL pointer in wxCmdLineOption::Found") );

    *value = opt->GetLongVal();

    return true;
}

bool wxCmdLineParser::Found(const wxString& name, double *value) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    if ( !opt || !opt->HasValue() )
        return false;

    wxCHECK_MSG( value, false, wxT("NULL pointer in wxCmdLineOption::Found") );

    *value = opt->GetDoubleVal();

    return true;
}

#if wxUSE_DATETIME
bool wxCmdLineParser::Found(const wxString& name, wxDateTime *value) const
{
    const wxCmdLineOption* const opt = m_data->FindOptionByAnyName(name);

    if ( !opt || !opt->HasValue() )
        return false;

    wxCHECK_MSG( value, false, wxT("NULL pointer in wxCmdLineOption::Found") );

    *value = opt->GetDateVal();

    return true;
}
#endif // wxUSE_DATETIME

size_t wxCmdLineParser::GetParamCount() const
{
    return m_data->m_parameters.size();
}

wxString wxCmdLineParser::GetParam(size_t n) const
{
    wxCHECK_MSG( n < GetParamCount(), wxEmptyString, wxT("invalid param index") );

    return m_data->m_parameters[n];
}

// Resets switches and options
void wxCmdLineParser::Reset()
{
    for ( size_t i = 0; i < m_data->m_options.GetCount(); i++ )
    {
        m_data->m_options[i].Reset();
    }
}


// ----------------------------------------------------------------------------
// the real work is done here
// ----------------------------------------------------------------------------

int wxCmdLineParser::Parse(bool showUsage)
{
    bool maybeOption = true;    // can the following arg be an option?
    bool ok = true;             // true until an error is detected
    bool helpRequested = false; // true if "-h" was given
    bool hadRepeatableParam = false; // true if found param with MULTIPLE flag

    size_t currentParam = 0;    // the index in m_paramDesc

    size_t countParam = m_data->m_paramDesc.GetCount();
    wxString errorMsg;

    Reset();

    // parse everything
    m_data->m_parameters.clear();
    wxString arg;
    size_t count = m_data->m_arguments.size();
    for ( size_t n = 1; ok && (n < count); n++ )    // 0 is program name
    {
        arg = m_data->m_arguments[n];

        // special case: "--" should be discarded and all following arguments
        // should be considered as parameters, even if they start with '-' and
        // not like options (this is POSIX-like)
        if ( arg == wxT("--") )
        {
            maybeOption = false;

            continue;
        }
#ifdef __WXOSX__
        if ( arg == wxS("-ApplePersistenceIgnoreState") ||
             arg == wxS("-AppleTextDirection") ||
             arg == wxS("-AppleLocale") ||
             arg == wxS("-AppleLanguages") )
        {
            maybeOption = false;
            n++;
            
            continue;
        }
#endif
        
        // empty argument or just '-' is not an option but a parameter
        if ( maybeOption && arg.length() > 1 &&
                // FIXME-UTF8: use wc_str() after removing ANSI build
                wxStrchr(m_data->m_switchChars.c_str(), arg[0u]) )
        {
            bool isLong;
            wxString name;
            int optInd = wxNOT_FOUND;   // init to suppress warnings

            // an option or a switch: find whether it's a long or a short one
            if ( arg.length() >= 3 && arg[0u] == wxT('-') && arg[1u] == wxT('-') )
            {
                // a long one
                isLong = true;

                // Skip leading "--"
                wxString::const_iterator p = arg.begin() + 2;

                bool longOptionsEnabled = AreLongOptionsEnabled();

                name = GetLongOptionName(p, arg.end());

                if (longOptionsEnabled)
                {
                    wxString errorOpt;

                    optInd = m_data->FindOptionByLongName(name);
                    if ( optInd == wxNOT_FOUND )
                    {
                        // Check if this could be a negatable long option.
                        if ( name.Last() == '-' )
                        {
                            name.RemoveLast();

                            optInd = m_data->FindOptionByLongName(name);
                            if ( optInd != wxNOT_FOUND )
                            {
                                if ( !(m_data->m_options[optInd].flags &
                                        wxCMD_LINE_SWITCH_NEGATABLE) )
                                {
                                    errorOpt.Printf
                                             (
                                              _("Option '%s' can't be negated"),
                                              name
                                             );
                                    optInd = wxNOT_FOUND;
                                }
                            }
                        }

                        if ( optInd == wxNOT_FOUND )
                        {
                            if ( errorOpt.empty() )
                            {
                                errorOpt.Printf
                                         (
                                          _("Unknown long option '%s'"),
                                          name
                                         );
                            }

                            errorMsg << errorOpt << wxT('\n');
                        }
                    }
                }
                else
                {
                    optInd = wxNOT_FOUND; // Sanity check

                    // Print the argument including leading "--"
                    name.Prepend( wxT("--") );
                    errorMsg << wxString::Format(_("Unknown option '%s'"), name.c_str())
                             << wxT('\n');
                }

            }
            else // not a long option
            {
                isLong = false;

                // a short one: as they can be cumulated, we try to find the
                // longest substring which is a valid option
                wxString::const_iterator p = arg.begin() + 1;

                name = GetShortOptionName(p, arg.end());

                size_t len = name.length();
                do
                {
                    if ( len == 0 )
                    {
                        // we couldn't find a valid option name in the
                        // beginning of this string
                        errorMsg << wxString::Format(_("Unknown option '%s'"), name.c_str())
                                 << wxT('\n');

                        break;
                    }
                    else
                    {
                        optInd = m_data->FindOption(name.Left(len));

                        // will try with one character less the next time
                        len--;
                    }
                }
                while ( optInd == wxNOT_FOUND );

                len++;  // compensates extra len-- above
                if ( (optInd != wxNOT_FOUND) && (len != name.length()) )
                {
                    // first of all, the option name is only part of this
                    // string
                    name = name.Left(len);

                    // our option is only part of this argument, there is
                    // something else in it - it is either the value of this
                    // option or other switches if it is a switch
                    if ( m_data->m_options[(size_t)optInd].kind
                            == wxCMD_LINE_SWITCH )
                    {
                        // if the switch is negatable and it is just followed
                        // by '-' the '-' is considered to be part of this
                        // switch
                        if ( (m_data->m_options[(size_t)optInd].flags &
                                    wxCMD_LINE_SWITCH_NEGATABLE) &&
                                arg[len] == '-' )
                            ++len;

                        // pretend that all the rest of the argument is the
                        // next argument, in fact
                        wxString arg2 = arg[0u];
                        arg2 += arg.Mid(len + 1); // +1 for leading '-'

                        m_data->m_arguments.insert
                            (m_data->m_arguments.begin() + n + 1, arg2);
                        count++;

                        // only leave the part which wasn't extracted into the
                        // next argument in this one
                        arg = arg.Left(len + 1);
                    }
                    //else: it's our value, we'll deal with it below
                }
            }

            if ( optInd == wxNOT_FOUND )
            {
                ok = false;

                continue;   // will break, in fact
            }

            // look at what follows:

            // +1 for leading '-'
            wxString::const_iterator p = arg.begin() + 1 + name.length();
            wxString::const_iterator end = arg.end();

            if ( isLong )
                ++p;    // for another leading '-'

            wxCmdLineOption& opt = m_data->m_options[(size_t)optInd];
            if ( opt.kind == wxCMD_LINE_SWITCH )
            {
                // we must check that there is no value following the switch
                bool negated = (opt.flags & wxCMD_LINE_SWITCH_NEGATABLE) &&
                                    p != arg.end() && *p == '-';

                if ( !negated && p != arg.end() )
                {
                    errorMsg << wxString::Format(_("Unexpected characters following option '%s'."), name.c_str())
                             << wxT('\n');
                    ok = false;
                }
                else // no value, as expected
                {
                    // nothing more to do
                    opt.SetHasValue();
                    if ( negated )
                        opt.SetNegated();

                    if ( opt.flags & wxCMD_LINE_OPTION_HELP )
                    {
                        helpRequested = true;

                        // it's not an error, but we still stop here
                        ok = false;
                    }
                }
            }
            else // it's an option. not a switch
            {
                switch ( p == end ? '\0' : (*p).GetValue() )
                {
                    case '=':
                    case ':':
                        // the value follows
                        ++p;
                        break;

                    case '\0':
                        // the value is in the next argument
                        if ( ++n == count )
                        {
                            // ... but there is none
                            errorMsg << wxString::Format(_("Option '%s' requires a value."),
                                                         name.c_str())
                                     << wxT('\n');

                            ok = false;
                        }
                        else
                        {
                            // ... take it from there
                            p = m_data->m_arguments[n].begin();
                            end = m_data->m_arguments[n].end();
                        }
                        break;

                    default:
                        // the value is right here: this may be legal or
                        // not depending on the option style
                        if ( opt.flags & wxCMD_LINE_NEEDS_SEPARATOR )
                        {
                            errorMsg << wxString::Format(_("Separator expected after the option '%s'."),
                                                         name.c_str())
                                    << wxT('\n');

                            ok = false;
                        }
                }

                if ( ok )
                {
                    wxString value(p, end);
                    switch ( opt.type )
                    {
                        default:
                            wxFAIL_MSG( wxT("unknown option type") );
                            // still fall through

                        case wxCMD_LINE_VAL_STRING:
                            opt.SetStrVal(value);
                            break;

                        case wxCMD_LINE_VAL_NUMBER:
                            {
                                long val;
                                if ( value.ToLong(&val) )
                                {
                                    opt.SetLongVal(val);
                                }
                                else
                                {
                                    errorMsg << wxString::Format(_("'%s' is not a correct numeric value for option '%s'."),
                                                                 value.c_str(), name.c_str())
                                             << wxT('\n');

                                    ok = false;
                                }
                            }
                            break;

                        case wxCMD_LINE_VAL_DOUBLE:
                            {
                                double val;
                                if ( value.ToDouble(&val) )
                                {
                                    opt.SetDoubleVal(val);
                                }
                                else
                                {
                                    errorMsg << wxString::Format(_("'%s' is not a correct numeric value for option '%s'."),
                                                                 value.c_str(), name.c_str())
                                             << wxT('\n');

                                    ok = false;
                                }
                            }
                            break;

#if wxUSE_DATETIME
                        case wxCMD_LINE_VAL_DATE:
                            {
                                wxDateTime dt;
                                wxString::const_iterator endDate;
                                if ( !dt.ParseDate(value, &endDate) || endDate != value.end() )
                                {
                                    errorMsg << wxString::Format(_("Option '%s': '%s' cannot be converted to a date."),
                                                                 name.c_str(), value.c_str())
                                             << wxT('\n');

                                    ok = false;
                                }
                                else
                                {
                                    opt.SetDateVal(dt);
                                }
                            }
                            break;
#endif // wxUSE_DATETIME
                    }
                }
            }
        }
        else // not an option, must be a parameter
        {
            if ( currentParam < countParam )
            {
                wxCmdLineParam& param = m_data->m_paramDesc[currentParam];

                // TODO check the param type

                m_data->m_parameters.push_back(arg);

                if ( !(param.flags & wxCMD_LINE_PARAM_MULTIPLE) )
                {
                    currentParam++;
                }
                else
                {
                    wxASSERT_MSG( currentParam == countParam - 1,
                                  wxT("all parameters after the one with wxCMD_LINE_PARAM_MULTIPLE style are ignored") );

                    // remember that we did have this last repeatable parameter
                    hadRepeatableParam = true;
                }
            }
            else
            {
                errorMsg << wxString::Format(_("Unexpected parameter '%s'"), arg.c_str())
                         << wxT('\n');

                ok = false;
            }
        }
    }

    // verify that all mandatory options were given
    if ( ok )
    {
        size_t countOpt = m_data->m_options.GetCount();
        for ( size_t n = 0; ok && (n < countOpt); n++ )
        {
            wxCmdLineOption& opt = m_data->m_options[n];
            if ( (opt.flags & wxCMD_LINE_OPTION_MANDATORY) && !opt.HasValue() )
            {
                wxString optName;
                if ( !opt.longName )
                {
                    optName = opt.shortName;
                }
                else
                {
                    if ( AreLongOptionsEnabled() )
                    {
                        optName.Printf( _("%s (or %s)"),
                                        opt.shortName.c_str(),
                                        opt.longName.c_str() );
                    }
                    else
                    {
                        optName.Printf( wxT("%s"),
                                        opt.shortName.c_str() );
                    }
                }

                errorMsg << wxString::Format(_("The value for the option '%s' must be specified."),
                                             optName.c_str())
                         << wxT('\n');

                ok = false;
            }
        }

        for ( ; ok && (currentParam < countParam); currentParam++ )
        {
            wxCmdLineParam& param = m_data->m_paramDesc[currentParam];
            if ( (currentParam == countParam - 1) &&
                 (param.flags & wxCMD_LINE_PARAM_MULTIPLE) &&
                 hadRepeatableParam )
            {
                // special case: currentParam wasn't incremented, but we did
                // have it, so don't give error
                continue;
            }

            if ( !(param.flags & wxCMD_LINE_PARAM_OPTIONAL) )
            {
                errorMsg << wxString::Format(_("The required parameter '%s' was not specified."),
                                             param.description.c_str())
                         << wxT('\n');

                ok = false;
            }
        }
    }

    // if there was an error during parsing the command line, show this error
    // and also the usage message if it had been requested
    if ( !ok && (!errorMsg.empty() || (helpRequested && showUsage)) )
    {
        wxMessageOutput* msgOut = wxMessageOutput::Get();
        if ( msgOut )
        {
            wxString usage;
            if ( showUsage )
                usage = GetUsageString();

            msgOut->Printf( wxT("%s%s"), usage.c_str(), errorMsg.c_str() );
        }
        else
        {
            wxFAIL_MSG( wxT("no wxMessageOutput object?") );
        }
    }

    return ok ? 0 : helpRequested ? -1 : 1;
}

// ----------------------------------------------------------------------------
// give the usage message
// ----------------------------------------------------------------------------

void wxCmdLineParser::Usage() const
{
    wxMessageOutput* msgOut = wxMessageOutput::Get();
    if ( msgOut )
    {
        msgOut->Printf( wxT("%s"), GetUsageString().c_str() );
    }
    else
    {
        wxFAIL_MSG( wxT("no wxMessageOutput object?") );
    }
}

wxString wxCmdLineParser::GetUsageString() const
{
    wxString appname;
    if ( m_data->m_arguments.empty() )
    {
        if ( wxTheApp )
            appname = wxTheApp->GetAppName();
    }
    else // use argv[0]
    {
        appname = wxFileName(m_data->m_arguments[0]).GetName();
    }

    // we construct the brief cmd line desc on the fly, but not the detailed
    // help message below because we want to align the options descriptions
    // and for this we must first know the longest one of them
    wxString usage;
    wxArrayString namesOptions, descOptions;

    if ( !m_data->m_logo.empty() )
    {
        usage << m_data->m_logo << wxT('\n');
    }

    usage << wxString::Format(_("Usage: %s"), appname.c_str());

    // the switch char is usually '-' but this can be changed with
    // SetSwitchChars() and then the first one of possible chars is used
    wxChar chSwitch = !m_data->m_switchChars ? wxT('-')
                                             : m_data->m_switchChars[0u];

    bool areLongOptionsEnabled = AreLongOptionsEnabled();
    size_t n, count = m_data->m_options.GetCount();
    for ( n = 0; n < count; n++ )
    {
        wxCmdLineOption& opt = m_data->m_options[n];
        wxString option, negator;

        if ( opt.kind != wxCMD_LINE_USAGE_TEXT )
        {
            usage << wxT(' ');
            if ( !(opt.flags & wxCMD_LINE_OPTION_MANDATORY) )
            {
                usage << wxT('[');
            }

            if ( opt.flags & wxCMD_LINE_SWITCH_NEGATABLE )
                negator = wxT("[-]");

            if ( !opt.shortName.empty() )
            {
                usage << chSwitch << opt.shortName << negator;
            }
            else if ( areLongOptionsEnabled && !opt.longName.empty() )
            {
                usage << wxT("--") << opt.longName << negator;
            }
            else
            {
                if (!opt.longName.empty())
                {
                    wxFAIL_MSG( wxT("option with only a long name while long ")
                                wxT("options are disabled") );
                }
                else
                {
                    wxFAIL_MSG( wxT("option without neither short nor long name") );
                }
            }

            if ( !opt.shortName.empty() )
            {
                option << wxT("  ") << chSwitch << opt.shortName;
            }

            if ( areLongOptionsEnabled && !opt.longName.empty() )
            {
                option << (option.empty() ? wxT("  ") : wxT(", "))
                       << wxT("--") << opt.longName;
            }

            if ( opt.kind != wxCMD_LINE_SWITCH )
            {
                wxString val;
                val << wxT('<') << GetTypeName(opt.type) << wxT('>');
                usage << wxT(' ') << val;
                option << (!opt.longName ? wxT(':') : wxT('=')) << val;
            }

            if ( !(opt.flags & wxCMD_LINE_OPTION_MANDATORY) )
            {
                usage << wxT(']');
            }
        }

        namesOptions.push_back(option);
        descOptions.push_back(opt.description);
    }

    count = m_data->m_paramDesc.GetCount();
    for ( n = 0; n < count; n++ )
    {
        wxCmdLineParam& param = m_data->m_paramDesc[n];

        usage << wxT(' ');
        if ( param.flags & wxCMD_LINE_PARAM_OPTIONAL )
        {
            usage << wxT('[');
        }

        usage << param.description;

        if ( param.flags & wxCMD_LINE_PARAM_MULTIPLE )
        {
            usage << wxT("...");
        }

        if ( param.flags & wxCMD_LINE_PARAM_OPTIONAL )
        {
            usage << wxT(']');
        }
    }

    usage << wxT('\n');

    // set to number of our own options, not counting the standard ones
    count = namesOptions.size();

    // get option names & descriptions for standard options, if any:
    wxAppTraits *traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
    wxString stdDesc;
    if ( traits )
        stdDesc = traits->GetStandardCmdLineOptions(namesOptions, descOptions);

    // now construct the detailed help message
    size_t len, lenMax = 0;
    for ( n = 0; n < namesOptions.size(); n++ )
    {
        len = namesOptions[n].length();
        if ( len > lenMax )
            lenMax = len;
    }

    for ( n = 0; n < namesOptions.size(); n++ )
    {
        if ( n == count )
            usage << wxT('\n') << stdDesc;

        len = namesOptions[n].length();
        // desc contains text if name is empty
        if (len == 0)
        {
            usage << descOptions[n] << wxT('\n');
        }
        else
        {
            usage << namesOptions[n]
                  << wxString(wxT(' '), lenMax - len) << wxT('\t')
                  << descOptions[n]
                  << wxT('\n');
        }
    }

    return usage;
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

static wxString GetTypeName(wxCmdLineParamType type)
{
    wxString s;
    switch ( type )
    {
        default:
            wxFAIL_MSG( wxT("unknown option type") );
            // still fall through

        case wxCMD_LINE_VAL_STRING:
            s = _("str");
            break;

        case wxCMD_LINE_VAL_NUMBER:
            s = _("num");
            break;

        case wxCMD_LINE_VAL_DOUBLE:
            s = _("double");
            break;

        case wxCMD_LINE_VAL_DATE:
            s = _("date");
            break;
    }

    return s;
}

/*
Returns a string which is equal to the string pointed to by p, but up to the
point where p contains an character that's not allowed.
Allowable characters are letters and numbers, and characters pointed to by
the parameter allowedChars.

For example, if p points to "abcde-@-_", and allowedChars is "-_",
this function returns "abcde-".
*/
static wxString GetOptionName(wxString::const_iterator p,
                              wxString::const_iterator end,
                              const wxChar *allowedChars)
{
    wxString argName;

    while ( p != end && (wxIsalnum(*p) || wxStrchr(allowedChars, *p)) )
    {
        argName += *p++;
    }

    return argName;
}

// Besides alphanumeric characters, short and long options can
// have other characters.

// A short option additionally can have these
#define wxCMD_LINE_CHARS_ALLOWED_BY_SHORT_OPTION wxT("_?")

// A long option can have the same characters as a short option and a '-'.
#define wxCMD_LINE_CHARS_ALLOWED_BY_LONG_OPTION \
    wxCMD_LINE_CHARS_ALLOWED_BY_SHORT_OPTION wxT("-")

static wxString GetShortOptionName(wxString::const_iterator p,
                                  wxString::const_iterator end)
{
    return GetOptionName(p, end, wxCMD_LINE_CHARS_ALLOWED_BY_SHORT_OPTION);
}

static wxString GetLongOptionName(wxString::const_iterator p,
                                  wxString::const_iterator end)
{
    return GetOptionName(p, end, wxCMD_LINE_CHARS_ALLOWED_BY_LONG_OPTION);
}

#endif // wxUSE_CMDLINE_PARSER

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

/*
   This function is mainly used under Windows (as under Unix we always get the
   command line arguments as argc/argv anyhow) and so it tries to follow
   Windows conventions for the command line handling, not Unix ones. For
   instance, backslash is not special except when it precedes double quote when
   it does quote it.

   TODO: Rewrite this to follow the even more complicated rule used by Windows
         CommandLineToArgv():

    * A string of backslashes not followed by a quotation mark has no special
      meaning.
    * An even number of backslashes followed by a quotation mark is treated as
      pairs of protected backslashes, followed by a word terminator.
    * An odd number of backslashes followed by a quotation mark is treated as
      pairs of protected backslashes, followed by a protected quotation mark.

    See http://blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx

    It could also be useful to provide a converse function which is also
    non-trivial, see

    http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
 */

/* static */
wxArrayString
wxCmdLineParser::ConvertStringToArgs(const wxString& cmdline,
                                     wxCmdLineSplitType type)
{
    wxArrayString args;

    wxString arg;
    arg.reserve(1024);

    const wxString::const_iterator end = cmdline.end();
    wxString::const_iterator p = cmdline.begin();

    for ( ;; )
    {
        // skip white space
        while ( p != end && (*p == ' ' || *p == '\t') )
            ++p;

        // anything left?
        if ( p == end )
            break;

        // parse this parameter
        bool lastBS = false,
             isInsideQuotes = false;
        wxChar chDelim = '\0';
        for ( arg.clear(); p != end; ++p )
        {
            const wxChar ch = *p;

            if ( type == wxCMD_LINE_SPLIT_DOS )
            {
                if ( ch == '"' )
                {
                    if ( !lastBS )
                    {
                        isInsideQuotes = !isInsideQuotes;

                        // don't put quote in arg
                        continue;
                    }
                    //else: quote has no special meaning but the backslash
                    //      still remains -- makes no sense but this is what
                    //      Windows does
                }
                // note that backslash does *not* quote the space, only quotes do
                else if ( !isInsideQuotes && (ch == ' ' || ch == '\t') )
                {
                    ++p;    // skip this space anyhow
                    break;
                }

                lastBS = !lastBS && ch == '\\';
            }
            else // type == wxCMD_LINE_SPLIT_UNIX
            {
                if ( !lastBS )
                {
                    if ( isInsideQuotes )
                    {
                        if ( ch == chDelim )
                        {
                            isInsideQuotes = false;

                            continue;   // don't use the quote itself
                        }
                    }
                    else // not in quotes and not escaped
                    {
                        if ( ch == '\'' || ch == '"' )
                        {
                            isInsideQuotes = true;
                            chDelim = ch;

                            continue;   // don't use the quote itself
                        }

                        if ( ch == ' ' || ch == '\t' )
                        {
                            ++p;    // skip this space anyhow
                            break;
                        }
                    }

                    lastBS = ch == '\\';
                    if ( lastBS )
                        continue;
                }
                else // escaped by backslash, just use as is
                {
                    lastBS = false;
                }
            }

            arg += ch;
        }

        args.push_back(arg);
    }

    return args;
}
