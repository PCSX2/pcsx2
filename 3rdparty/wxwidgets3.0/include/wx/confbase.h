///////////////////////////////////////////////////////////////////////////////
// Name:        wx/confbase.h
// Purpose:     declaration of the base class of all config implementations
//              (see also: fileconf.h and msw/regconf.h and iniconf.h)
// Author:      Karsten Ballueder & Vadim Zeitlin
// Modified by:
// Created:     07.04.98 (adapted from appconf.h)
// Copyright:   (c) 1997 Karsten Ballueder   Ballueder@usa.net
//                       Vadim Zeitlin      <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONFBASE_H_
#define _WX_CONFBASE_H_

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/object.h"
#include "wx/base64.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

/// shall we be case sensitive in parsing variable names?
#ifndef wxCONFIG_CASE_SENSITIVE
  #define  wxCONFIG_CASE_SENSITIVE       0
#endif

/// separates group and entry names (probably shouldn't be changed)
#ifndef wxCONFIG_PATH_SEPARATOR
  #define   wxCONFIG_PATH_SEPARATOR     wxT('/')
#endif

/// introduces immutable entries
// (i.e. the ones which can't be changed from the local config file)
#ifndef wxCONFIG_IMMUTABLE_PREFIX
  #define   wxCONFIG_IMMUTABLE_PREFIX   wxT('!')
#endif

#if wxUSE_CONFIG

/// should we use registry instead of configuration files under Windows?
// (i.e. whether wxConfigBase::Create() will create a wxFileConfig (if it's
//  false) or wxRegConfig (if it's true and we're under Win32))
#ifndef   wxUSE_CONFIG_NATIVE
  #define wxUSE_CONFIG_NATIVE 1
#endif

// not all compilers can deal with template Read/Write() methods, define this
// symbol if the template functions are available
#if (!defined(__VISUALC__) || __VISUALC__ > 1200) && \
    !defined( __VMS ) && \
    !(defined(__HP_aCC) && defined(__hppa)) && \
    !defined (__DMC__)
    #define wxHAS_CONFIG_TEMPLATE_RW
#endif

// Style flags for constructor style parameter
enum
{
    wxCONFIG_USE_LOCAL_FILE = 1,
    wxCONFIG_USE_GLOBAL_FILE = 2,
    wxCONFIG_USE_RELATIVE_PATH = 4,
    wxCONFIG_USE_NO_ESCAPE_CHARACTERS = 8,
    wxCONFIG_USE_SUBDIR = 16
};

// ----------------------------------------------------------------------------
// abstract base class wxConfigBase which defines the interface for derived
// classes
//
// wxConfig organizes the items in a tree-like structure (modelled after the
// Unix/Dos filesystem). There are groups (directories) and keys (files).
// There is always one current group given by the current path.
//
// Keys are pairs "key_name = value" where value may be of string or integer
// (long) type (TODO doubles and other types such as wxDate coming soon).
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConfigBase : public wxObject
{
public:
  // constants
    // the type of an entry
  enum EntryType
  {
    Type_Unknown,
    Type_String,
    Type_Boolean,
    Type_Integer,    // use Read(long *)
    Type_Float       // use Read(double *)
  };

  // static functions
    // sets the config object, returns the previous pointer
  static wxConfigBase *Set(wxConfigBase *pConfig);
    // get the config object, creates it on demand unless DontCreateOnDemand
    // was called
  static wxConfigBase *Get(bool createOnDemand = true)
       { if ( createOnDemand && (!ms_pConfig) ) Create(); return ms_pConfig; }
    // create a new config object: this function will create the "best"
    // implementation of wxConfig available for the current platform, see
    // comments near definition wxUSE_CONFIG_NATIVE for details. It returns
    // the created object and also sets it as ms_pConfig.
  static wxConfigBase *Create();
    // should Get() try to create a new log object if the current one is NULL?
  static void DontCreateOnDemand() { ms_bAutoCreate = false; }

  // ctor & virtual dtor
      // ctor (can be used as default ctor too)
      //
      // Not all args will always be used by derived classes, but including
      // them all in each class ensures compatibility. If appName is empty,
      // uses wxApp name
  wxConfigBase(const wxString& appName = wxEmptyString,
               const wxString& vendorName = wxEmptyString,
               const wxString& localFilename = wxEmptyString,
               const wxString& globalFilename = wxEmptyString,
               long style = 0);

    // empty but ensures that dtor of all derived classes is virtual
  virtual ~wxConfigBase();

  // path management
    // set current path: if the first character is '/', it's the absolute path,
    // otherwise it's a relative path. '..' is supported. If the strPath
    // doesn't exist it is created.
  virtual void SetPath(const wxString& strPath) = 0;
    // retrieve the current path (always as absolute path)
  virtual const wxString& GetPath() const = 0;

  // enumeration: all functions here return false when there are no more items.
  // you must pass the same lIndex to GetNext and GetFirst (don't modify it)
    // enumerate subgroups
  virtual bool GetFirstGroup(wxString& str, long& lIndex) const = 0;
  virtual bool GetNextGroup (wxString& str, long& lIndex) const = 0;
    // enumerate entries
  virtual bool GetFirstEntry(wxString& str, long& lIndex) const = 0;
  virtual bool GetNextEntry (wxString& str, long& lIndex) const = 0;
    // get number of entries/subgroups in the current group, with or without
    // it's subgroups
  virtual size_t GetNumberOfEntries(bool bRecursive = false) const = 0;
  virtual size_t GetNumberOfGroups(bool bRecursive = false) const = 0;

  // tests of existence
    // returns true if the group by this name exists
  virtual bool HasGroup(const wxString& strName) const = 0;
    // same as above, but for an entry
  virtual bool HasEntry(const wxString& strName) const = 0;
    // returns true if either a group or an entry with a given name exist
  bool Exists(const wxString& strName) const
    { return HasGroup(strName) || HasEntry(strName); }

    // get the entry type
  virtual EntryType GetEntryType(const wxString& name) const
  {
    // by default all entries are strings
    return HasEntry(name) ? Type_String : Type_Unknown;
  }

  // key access: returns true if value was really read, false if default used
  // (and if the key is not found the default value is returned.)

    // read a string from the key
  bool Read(const wxString& key, wxString *pStr) const;
  bool Read(const wxString& key, wxString *pStr, const wxString& defVal) const;

    // read a number (long)
  bool Read(const wxString& key, long *pl) const;
  bool Read(const wxString& key, long *pl, long defVal) const;

    // read an int (wrapper around `long' version)
  bool Read(const wxString& key, int *pi) const;
  bool Read(const wxString& key, int *pi, int defVal) const;

    // read a double
  bool Read(const wxString& key, double* val) const;
  bool Read(const wxString& key, double* val, double defVal) const;

    // read a float
  bool Read(const wxString& key, float* val) const;
  bool Read(const wxString& key, float* val, float defVal) const;

    // read a bool
  bool Read(const wxString& key, bool* val) const;
  bool Read(const wxString& key, bool* val, bool defVal) const;

#if wxUSE_BASE64
    // read a binary data block
  bool Read(const wxString& key, wxMemoryBuffer* data) const
    { return DoReadBinary(key, data); }
   // no default version since it does not make sense for binary data
#endif // wxUSE_BASE64

#ifdef wxHAS_CONFIG_TEMPLATE_RW
  // read other types, for which wxFromString is defined
  template <typename T>
  bool Read(const wxString& key, T* value) const
  {
      wxString s;
      if ( !Read(key, &s) )
          return false;
      return wxFromString(s, value);
  }

  template <typename T>
  bool Read(const wxString& key, T* value, const T& defVal) const
  {
      const bool found = Read(key, value);
      if ( !found )
      {
          if (IsRecordingDefaults())
              ((wxConfigBase *)this)->Write(key, defVal);
          *value = defVal;
      }
      return found;
  }
#endif // wxHAS_CONFIG_TEMPLATE_RW

  // convenience functions returning directly the value
  wxString Read(const wxString& key,
                const wxString& defVal = wxEmptyString) const
    { wxString s; (void)Read(key, &s, defVal); return s; }

  // we have to provide a separate version for C strings as otherwise the
  // template Read() would be used
  wxString Read(const wxString& key, const char* defVal) const
    { return Read(key, wxString(defVal)); }
  wxString Read(const wxString& key, const wchar_t* defVal) const
    { return Read(key, wxString(defVal)); }

  long ReadLong(const wxString& key, long defVal) const
    { long l; (void)Read(key, &l, defVal); return l; }

  double ReadDouble(const wxString& key, double defVal) const
    { double d; (void)Read(key, &d, defVal); return d; }

  bool ReadBool(const wxString& key, bool defVal) const
    { bool b; (void)Read(key, &b, defVal); return b; }

  template <typename T>
  T ReadObject(const wxString& key, T const& defVal) const
    { T t; (void)Read(key, &t, defVal); return t; }

  // for compatibility with wx 2.8
  long Read(const wxString& key, long defVal) const
    { return ReadLong(key, defVal); }


  // write the value (return true on success)
  bool Write(const wxString& key, const wxString& value)
    { return DoWriteString(key, value); }

  bool Write(const wxString& key, long value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, double value)
    { return DoWriteDouble(key, value); }

  bool Write(const wxString& key, bool value)
    { return DoWriteBool(key, value); }

#if wxUSE_BASE64
  bool Write(const wxString& key, const wxMemoryBuffer& buf)
    { return DoWriteBinary(key, buf); }
#endif // wxUSE_BASE64

  // we have to provide a separate version for C strings as otherwise they
  // would be converted to bool and not to wxString as expected!
  bool Write(const wxString& key, const char *value)
    { return Write(key, wxString(value)); }
  bool Write(const wxString& key, const unsigned char *value)
    { return Write(key, wxString(value)); }
  bool Write(const wxString& key, const wchar_t *value)
    { return Write(key, wxString(value)); }


  // we also have to provide specializations for other types which we want to
  // handle using the specialized DoWriteXXX() instead of the generic template
  // version below
  bool Write(const wxString& key, char value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, unsigned char value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, short value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, unsigned short value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, unsigned int value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, int value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, unsigned long value)
    { return DoWriteLong(key, value); }

  bool Write(const wxString& key, float value)
    { return DoWriteDouble(key, value); }

  // Causes ambiguities in VC++ 6 and OpenVMS (at least)
#if ( (!defined(__VISUALC__) || __VISUALC__ > 1200) && !defined( __VMS ) && !defined (__DMC__))
  // for other types, use wxToString()
  template <typename T>
  bool Write(const wxString& key, T const& value)
    { return Write(key, wxToString(value)); }
#endif

  // permanently writes all changes
  virtual bool Flush(bool bCurrentOnly = false) = 0;

  // renaming, all functions return false on failure (probably because the new
  // name is already taken by an existing entry)
    // rename an entry
  virtual bool RenameEntry(const wxString& oldName,
                           const wxString& newName) = 0;
    // rename a group
  virtual bool RenameGroup(const wxString& oldName,
                           const wxString& newName) = 0;

  // delete entries/groups
    // deletes the specified entry and the group it belongs to if
    // it was the last key in it and the second parameter is true
  virtual bool DeleteEntry(const wxString& key,
                           bool bDeleteGroupIfEmpty = true) = 0;
    // delete the group (with all subgroups)
  virtual bool DeleteGroup(const wxString& key) = 0;
    // delete the whole underlying object (disk file, registry key, ...)
    // primarily for use by uninstallation routine.
  virtual bool DeleteAll() = 0;

  // options
    // we can automatically expand environment variables in the config entries
    // (this option is on by default, you can turn it on/off at any time)
  bool IsExpandingEnvVars() const { return m_bExpandEnvVars; }
  void SetExpandEnvVars(bool bDoIt = true) { m_bExpandEnvVars = bDoIt; }
    // recording of default values
  void SetRecordDefaults(bool bDoIt = true) { m_bRecordDefaults = bDoIt; }
  bool IsRecordingDefaults() const { return m_bRecordDefaults; }
  // does expansion only if needed
  wxString ExpandEnvVars(const wxString& str) const;

    // misc accessors
  wxString GetAppName() const { return m_appName; }
  wxString GetVendorName() const { return m_vendorName; }

  // Used wxIniConfig to set members in constructor
  void SetAppName(const wxString& appName) { m_appName = appName; }
  void SetVendorName(const wxString& vendorName) { m_vendorName = vendorName; }

  void SetStyle(long style) { m_style = style; }
  long GetStyle() const { return m_style; }

protected:
  static bool IsImmutable(const wxString& key)
    { return !key.IsEmpty() && key[0] == wxCONFIG_IMMUTABLE_PREFIX; }

  // return the path without trailing separator, if any: this should be called
  // to sanitize paths referring to the group names before passing them to
  // wxConfigPathChanger as "/foo/bar/" should be the same as "/foo/bar" and it
  // isn't interpreted in the same way by it (and this can't be changed there
  // as it's not the same for the entries names)
  static wxString RemoveTrailingSeparator(const wxString& key);

  // do read/write the values of different types
  virtual bool DoReadString(const wxString& key, wxString *pStr) const = 0;
  virtual bool DoReadLong(const wxString& key, long *pl) const = 0;
  virtual bool DoReadDouble(const wxString& key, double* val) const;
  virtual bool DoReadBool(const wxString& key, bool* val) const;
#if wxUSE_BASE64
  virtual bool DoReadBinary(const wxString& key, wxMemoryBuffer* buf) const = 0;
#endif // wxUSE_BASE64

  virtual bool DoWriteString(const wxString& key, const wxString& value) = 0;
  virtual bool DoWriteLong(const wxString& key, long value) = 0;
  virtual bool DoWriteDouble(const wxString& key, double value);
  virtual bool DoWriteBool(const wxString& key, bool value);
#if wxUSE_BASE64
  virtual bool DoWriteBinary(const wxString& key, const wxMemoryBuffer& buf) = 0;
#endif // wxUSE_BASE64

private:
  // are we doing automatic environment variable expansion?
  bool m_bExpandEnvVars;
  // do we record default values?
  bool m_bRecordDefaults;

  // static variables
  static wxConfigBase *ms_pConfig;
  static bool          ms_bAutoCreate;

  // Application name and organisation name
  wxString          m_appName;
  wxString          m_vendorName;

  // Style flag
  long              m_style;

  DECLARE_ABSTRACT_CLASS(wxConfigBase)
};

// a handy little class which changes current path to the path of given entry
// and restores it in dtor: so if you declare a local variable of this type,
// you work in the entry directory and the path is automatically restored
// when the function returns
// Taken out of wxConfig since not all compilers can cope with nested classes.
class WXDLLIMPEXP_BASE wxConfigPathChanger
{
public:
  // ctor/dtor do path changing/restoring of the path
  wxConfigPathChanger(const wxConfigBase *pContainer, const wxString& strEntry);
 ~wxConfigPathChanger();

  // get the key name
  const wxString& Name() const { return m_strName; }

  // this method must be called if the original path (i.e. the current path at
  // the moment of creation of this object) could have been deleted to prevent
  // us from restoring the not existing (any more) path
  //
  // if the original path doesn't exist any more, the path will be restored to
  // the deepest still existing component of the old path
  void UpdateIfDeleted();

private:
  wxConfigBase *m_pContainer;   // object we live in
  wxString      m_strName,      // name of entry (i.e. name only)
                m_strOldPath;   // saved path
  bool          m_bChanged;     // was the path changed?

  wxDECLARE_NO_COPY_CLASS(wxConfigPathChanger);
};


#endif // wxUSE_CONFIG

/*
  Replace environment variables ($SOMETHING) with their values. The format is
  $VARNAME or ${VARNAME} where VARNAME contains alphanumeric characters and
  '_' only. '$' must be escaped ('\$') in order to be taken literally.
*/

WXDLLIMPEXP_BASE wxString wxExpandEnvVars(const wxString &sz);

/*
  Split path into parts removing '..' in progress
 */
WXDLLIMPEXP_BASE void wxSplitPath(wxArrayString& aParts, const wxString& path);

#endif // _WX_CONFBASE_H_

