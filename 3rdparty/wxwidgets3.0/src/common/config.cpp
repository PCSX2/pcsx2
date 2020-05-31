///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/config.cpp
// Purpose:     implementation of wxConfigBase class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07.04.98
// Copyright:   (c) 1997 Karsten Ballueder  Ballueder@usa.net
//                       Vadim Zeitlin      <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif  //__BORLANDC__

#ifndef wxUSE_CONFIG_NATIVE
    #define wxUSE_CONFIG_NATIVE 1
#endif

#include "wx/config.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/arrstr.h"
    #include "wx/math.h"
#endif //WX_PRECOMP

#if wxUSE_CONFIG && ((wxUSE_FILE && wxUSE_TEXTFILE) || wxUSE_CONFIG_NATIVE)

#include "wx/apptrait.h"
#include "wx/file.h"

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>     // for INT_MAX
#include <float.h>      // for FLT_MAX

// ----------------------------------------------------------------------------
// global and class static variables
// ----------------------------------------------------------------------------

wxConfigBase *wxConfigBase::ms_pConfig     = NULL;
bool          wxConfigBase::ms_bAutoCreate = true;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxAppTraitsBase
// ----------------------------------------------------------------------------

wxConfigBase *wxAppTraitsBase::CreateConfig()
{
    return new
    #if defined(__WINDOWS__) && wxUSE_CONFIG_NATIVE
        wxRegConfig(wxTheApp->GetAppName(), wxTheApp->GetVendorName());
    #else // either we're under Unix or wish to use files even under Windows
        wxFileConfig(wxTheApp->GetAppName());
    #endif
}

// ----------------------------------------------------------------------------
// wxConfigBase
// ----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_CLASS(wxConfigBase, wxObject)

// Not all args will always be used by derived classes, but including them all
// in each class ensures compatibility.
wxConfigBase::wxConfigBase(const wxString& appName,
                           const wxString& vendorName,
                           const wxString& WXUNUSED(localFilename),
                           const wxString& WXUNUSED(globalFilename),
                           long style)
            : m_appName(appName), m_vendorName(vendorName), m_style(style)
{
    m_bExpandEnvVars = true;
    m_bRecordDefaults = false;
}

wxConfigBase::~wxConfigBase()
{
    // required here for Darwin
}

wxConfigBase *wxConfigBase::Set(wxConfigBase *pConfig)
{
  wxConfigBase *pOld = ms_pConfig;
  ms_pConfig = pConfig;
  return pOld;
}

wxConfigBase *wxConfigBase::Create()
{
  if ( ms_bAutoCreate && ms_pConfig == NULL ) {
    wxAppTraits * const traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
    wxCHECK_MSG( traits, NULL, wxT("create wxApp before calling this") );

    ms_pConfig = traits->CreateConfig();
  }

  return ms_pConfig;
}

// ----------------------------------------------------------------------------
// wxConfigBase reading entries
// ----------------------------------------------------------------------------

// implement both Read() overloads for the given type in terms of DoRead()
#define IMPLEMENT_READ_FOR_TYPE(name, type, deftype, extra)                 \
    bool wxConfigBase::Read(const wxString& key, type *val) const           \
    {                                                                       \
        wxCHECK_MSG( val, false, wxT("wxConfig::Read(): NULL parameter") );  \
                                                                            \
        if ( !DoRead##name(key, val) )                                      \
            return false;                                                   \
                                                                            \
        *val = extra(*val);                                                 \
                                                                            \
        return true;                                                        \
    }                                                                       \
                                                                            \
    bool wxConfigBase::Read(const wxString& key,                            \
                            type *val,                                      \
                            deftype defVal) const                           \
    {                                                                       \
        wxCHECK_MSG( val, false, wxT("wxConfig::Read(): NULL parameter") );  \
                                                                            \
        bool read = DoRead##name(key, val);                                 \
        if ( !read )                                                        \
        {                                                                   \
            if ( IsRecordingDefaults() )                                    \
            {                                                               \
                ((wxConfigBase *)this)->DoWrite##name(key, defVal);         \
            }                                                               \
                                                                            \
            *val = defVal;                                                  \
        }                                                                   \
                                                                            \
        *val = extra(*val);                                                 \
                                                                            \
        return read;                                                        \
    }


IMPLEMENT_READ_FOR_TYPE(String, wxString, const wxString&, ExpandEnvVars)
IMPLEMENT_READ_FOR_TYPE(Long, long, long, long)
IMPLEMENT_READ_FOR_TYPE(Double, double, double, double)
IMPLEMENT_READ_FOR_TYPE(Bool, bool, bool, bool)

#undef IMPLEMENT_READ_FOR_TYPE

// int is stored as long
bool wxConfigBase::Read(const wxString& key, int *pi) const
{
    long l = *pi;
    bool r = Read(key, &l);
    wxASSERT_MSG( l < INT_MAX, wxT("int overflow in wxConfig::Read") );
    *pi = (int)l;
    return r;
}

bool wxConfigBase::Read(const wxString& key, int *pi, int defVal) const
{
    long l = *pi;
    bool r = Read(key, &l, defVal);
    wxASSERT_MSG( l < INT_MAX, wxT("int overflow in wxConfig::Read") );
    *pi = (int)l;
    return r;
}

// Read floats as doubles then just type cast it down.
bool wxConfigBase::Read(const wxString& key, float* val) const
{
    wxCHECK_MSG( val, false, wxT("wxConfig::Read(): NULL parameter") );

    double temp;
    if ( !Read(key, &temp) )
        return false;

    wxCHECK_MSG( fabs(temp) <= FLT_MAX, false,
                     wxT("float overflow in wxConfig::Read") );
    wxCHECK_MSG( (temp == 0.0) || (fabs(temp) >= FLT_MIN), false,
                     wxT("float underflow in wxConfig::Read") );

    *val = static_cast<float>(temp);

    return true;
}

bool wxConfigBase::Read(const wxString& key, float* val, float defVal) const
{
    wxCHECK_MSG( val, false, wxT("wxConfig::Read(): NULL parameter") );

    if ( Read(key, val) )
        return true;

    *val = defVal;
    return false;
}

// the DoReadXXX() for the other types have implementation in the base class
// but can be overridden in the derived ones
bool wxConfigBase::DoReadBool(const wxString& key, bool* val) const
{
    wxCHECK_MSG( val, false, wxT("wxConfig::Read(): NULL parameter") );

    long l;
    if ( !DoReadLong(key, &l) )
        return false;

    if ( l != 0 && l != 1 )
    {
        // Don't assert here as this could happen in the result of user editing
        // the file directly and this not indicate a bug in the program but
        // still complain that something is wrong.
        wxLogWarning(_("Invalid value %ld for a boolean key \"%s\" in "
                       "config file."),
                     l, key);
    }

    *val = l != 0;

    return true;
}

bool wxConfigBase::DoReadDouble(const wxString& key, double* val) const
{
    wxString str;
    if ( Read(key, &str) )
    {
        if ( str.ToCDouble(val) )
            return true;

        // Previous versions of wxFileConfig wrote the numbers out using the
        // current locale and not the C one as now, so attempt to parse the
        // string as a number in the current locale too, for compatibility.
        if ( str.ToDouble(val) )
            return true;
    }

    return false;
}

// string reading helper
wxString wxConfigBase::ExpandEnvVars(const wxString& str) const
{
    wxString tmp; // Required for BC++
    if (IsExpandingEnvVars())
        tmp = wxExpandEnvVars(str);
    else
        tmp = str;
    return tmp;
}

// ----------------------------------------------------------------------------
// wxConfigBase writing
// ----------------------------------------------------------------------------

bool wxConfigBase::DoWriteDouble(const wxString& key, double val)
{
    // Notice that we always write out the numbers in C locale and not the
    // current one. This makes the config files portable between machines using
    // different locales.
    return DoWriteString(key, wxString::FromCDouble(val));
}

bool wxConfigBase::DoWriteBool(const wxString& key, bool value)
{
    return DoWriteLong(key, value ? 1l : 0l);
}

// ----------------------------------------------------------------------------
// wxConfigPathChanger
// ----------------------------------------------------------------------------

wxConfigPathChanger::wxConfigPathChanger(const wxConfigBase *pContainer,
                                         const wxString& strEntry)
{
  m_bChanged = false;
  m_pContainer = const_cast<wxConfigBase *>(pContainer);

  // the path is everything which precedes the last slash and the name is
  // everything after it -- and this works correctly if there is no slash too
  wxString strPath = strEntry.BeforeLast(wxCONFIG_PATH_SEPARATOR, &m_strName);

  // except in the special case of "/keyname" when there is nothing before "/"
  if ( strPath.empty() &&
       ((!strEntry.empty()) && strEntry[0] == wxCONFIG_PATH_SEPARATOR) )
  {
    strPath = wxCONFIG_PATH_SEPARATOR;
  }

  if ( !strPath.empty() )
  {
    if ( m_pContainer->GetPath() != strPath )
    {
        // we do change the path so restore it later
        m_bChanged = true;

        /* JACS: work around a memory bug that causes an assert
           when using wxRegConfig, related to reference-counting.
           Can be reproduced by removing .wc_str() below and
           adding the following code to the config sample OnInit under
           Windows:

           pConfig->SetPath(wxT("MySettings"));
           pConfig->SetPath(wxT(".."));
           int value;
           pConfig->Read(wxT("MainWindowX"), & value);
        */
        m_strOldPath = m_pContainer->GetPath().wc_str();
        if ( *m_strOldPath.c_str() != wxCONFIG_PATH_SEPARATOR )
          m_strOldPath += wxCONFIG_PATH_SEPARATOR;
        m_pContainer->SetPath(strPath);
    }
  }
}

void wxConfigPathChanger::UpdateIfDeleted()
{
    // we don't have to do anything at all if we didn't change the path
    if ( !m_bChanged )
        return;

    // find the deepest still existing parent path of the original path
    while ( !m_pContainer->HasGroup(m_strOldPath) )
    {
        m_strOldPath = m_strOldPath.BeforeLast(wxCONFIG_PATH_SEPARATOR);
        if ( m_strOldPath.empty() )
            m_strOldPath = wxCONFIG_PATH_SEPARATOR;
    }
}

wxConfigPathChanger::~wxConfigPathChanger()
{
  // only restore path if it was changed
  if ( m_bChanged ) {
    m_pContainer->SetPath(m_strOldPath);
  }
}

// this is a wxConfig method but it's mainly used with wxConfigPathChanger
/* static */
wxString wxConfigBase::RemoveTrailingSeparator(const wxString& key)
{
    wxString path(key);

    // don't remove the only separator from a root group path!
    while ( path.length() > 1 )
    {
        if ( *path.rbegin() != wxCONFIG_PATH_SEPARATOR )
            break;

        path.erase(path.end() - 1);
    }

    return path;
}

#endif // wxUSE_CONFIG

// ----------------------------------------------------------------------------
// static & global functions
// ----------------------------------------------------------------------------

// understands both Unix and Windows (but only under Windows) environment
// variables expansion: i.e. $var, $(var) and ${var} are always understood
// and in addition under Windows %var% is also.

// don't change the values the enum elements: they must be equal
// to the matching [closing] delimiter.
enum Bracket
{
  Bracket_None,
  Bracket_Normal  = ')',
  Bracket_Curly   = '}',
#ifdef  __WINDOWS__
  Bracket_Windows = '%',    // yeah, Windows people are a bit strange ;-)
#endif
  Bracket_Max
};

wxString wxExpandEnvVars(const wxString& str)
{
  wxString strResult;
  strResult.Alloc(str.length());

  size_t m;
  for ( size_t n = 0; n < str.length(); n++ ) {
    switch ( str[n].GetValue() ) {
#ifdef __WINDOWS__
      case wxT('%'):
#endif // __WINDOWS__
      case wxT('$'):
        {
          Bracket bracket;
          #ifdef __WINDOWS__
            if ( str[n] == wxT('%') )
              bracket = Bracket_Windows;
            else
          #endif // __WINDOWS__
          if ( n == str.length() - 1 ) {
            bracket = Bracket_None;
          }
          else {
            switch ( str[n + 1].GetValue() ) {
              case wxT('('):
                bracket = Bracket_Normal;
                n++;                   // skip the bracket
                break;

              case wxT('{'):
                bracket = Bracket_Curly;
                n++;                   // skip the bracket
                break;

              default:
                bracket = Bracket_None;
            }
          }

          m = n + 1;

          while ( m < str.length() && (wxIsalnum(str[m]) || str[m] == wxT('_')) )
            m++;

          wxString strVarName(str.c_str() + n + 1, m - n - 1);

#ifdef __WXWINCE__
          const bool expanded = false;
#else
          // NB: use wxGetEnv instead of wxGetenv as otherwise variables
          //     set through wxSetEnv may not be read correctly!
          bool expanded = false;
          wxString tmp;
          if (wxGetEnv(strVarName, &tmp))
          {
              strResult += tmp;
              expanded = true;
          }
          else
#endif
          {
            // variable doesn't exist => don't change anything
            #ifdef  __WINDOWS__
              if ( bracket != Bracket_Windows )
            #endif
                if ( bracket != Bracket_None )
                  strResult << str[n - 1];
            strResult << str[n] << strVarName;
          }

          // check the closing bracket
          if ( bracket != Bracket_None ) {
            if ( m == str.length() || str[m] != (wxChar)bracket ) {
              // under MSW it's common to have '%' characters in the registry
              // and it's annoying to have warnings about them each time, so
              // ignroe them silently if they are not used for env vars
              //
              // under Unix, OTOH, this warning could be useful for the user to
              // understand why isn't the variable expanded as intended
              #ifndef __WINDOWS__
                wxLogWarning(_("Environment variables expansion failed: missing '%c' at position %u in '%s'."),
                             (char)bracket, (unsigned int) (m + 1), str.c_str());
              #endif // __WINDOWS__
            }
            else {
              // skip closing bracket unless the variables wasn't expanded
              if ( !expanded )
                strResult << (wxChar)bracket;
              m++;
            }
          }

          n = m - 1;  // skip variable name
        }
        break;

      case wxT('\\'):
        // backslash can be used to suppress special meaning of % and $
        if ( n != str.length() - 1 &&
                (str[n + 1] == wxT('%') || str[n + 1] == wxT('$')) ) {
          strResult += str[++n];

          break;
        }
        //else: fall through

      default:
        strResult += str[n];
    }
  }

  return strResult;
}

// this function is used to properly interpret '..' in path
void wxSplitPath(wxArrayString& aParts, const wxString& path)
{
  aParts.clear();

  wxString strCurrent;
  wxString::const_iterator pc = path.begin();
  for ( ;; ) {
    if ( pc == path.end() || *pc == wxCONFIG_PATH_SEPARATOR ) {
      if ( strCurrent == wxT(".") ) {
        // ignore
      }
      else if ( strCurrent == wxT("..") ) {
        // go up one level
        if ( aParts.size() == 0 )
        {
          wxLogWarning(_("'%s' has extra '..', ignored."), path);
        }
        else
        {
          aParts.erase(aParts.end() - 1);
        }

        strCurrent.Empty();
      }
      else if ( !strCurrent.empty() ) {
        aParts.push_back(strCurrent);
        strCurrent.Empty();
      }
      //else:
        // could log an error here, but we prefer to ignore extra '/'

      if ( pc == path.end() )
        break;
    }
    else
      strCurrent += *pc;

    ++pc;
  }
}
