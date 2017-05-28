/////////////////////////////////////////////////////////////////////////////
// Name:        wx/file.h
// Purpose:     wxFile - encapsulates low-level "file descriptor"
//              wxTempFile - safely replace the old file
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEH__
#define _WX_FILEH__

#include  "wx/defs.h"

#if wxUSE_FILE

#include  "wx/string.h"
#include  "wx/filefn.h"
#include  "wx/convauto.h"

// ----------------------------------------------------------------------------
// class wxFile: raw file IO
//
// NB: for space efficiency this class has no virtual functions, including
//     dtor which is _not_ virtual, so it shouldn't be used as a base class.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxFile
{
public:
  // more file constants
  // -------------------
    // opening mode
  enum OpenMode { read, write, read_write, write_append, write_excl };
    // standard values for file descriptor
  enum { fd_invalid = -1, fd_stdin, fd_stdout, fd_stderr };

  // static functions
  // ----------------
    // check whether a regular file by this name exists
  static bool Exists(const wxString& name);
    // check whether we can access the given file in given mode
    // (only read and write make sense here)
  static bool Access(const wxString& name, OpenMode mode);

  // ctors
  // -----
    // def ctor
  wxFile() { m_fd = fd_invalid; m_lasterror = 0; }
    // open specified file (may fail, use IsOpened())
  wxFile(const wxString& fileName, OpenMode mode = read);
    // attach to (already opened) file
  wxFile(int lfd) { m_fd = lfd; m_lasterror = 0; }

  // open/close
    // create a new file (with the default value of bOverwrite, it will fail if
    // the file already exists, otherwise it will overwrite it and succeed)
  bool Create(const wxString& fileName, bool bOverwrite = false,
              int access = wxS_DEFAULT);
  bool Open(const wxString& fileName, OpenMode mode = read,
            int access = wxS_DEFAULT);
  bool Close();  // Close is a NOP if not opened

  // assign an existing file descriptor and get it back from wxFile object
  void Attach(int lfd) { Close(); m_fd = lfd; m_lasterror = 0; }
  int  Detach() { int fdOld = m_fd; m_fd = fd_invalid; return fdOld; }
  int  fd() const { return m_fd; }

  // read/write (unbuffered)
    // read all data from the file into a string (useful for text files)
  bool ReadAll(wxString *str, const wxMBConv& conv = wxConvAuto());
    // returns number of bytes read or wxInvalidOffset on error
  ssize_t Read(void *pBuf, size_t nCount);
    // returns the number of bytes written
  size_t Write(const void *pBuf, size_t nCount);
    // returns true on success
  bool Write(const wxString& s, const wxMBConv& conv = wxConvAuto());
    // flush data not yet written
  bool Flush();

  // file pointer operations (return wxInvalidOffset on failure)
    // move ptr ofs bytes related to start/current offset/end of file
  wxFileOffset Seek(wxFileOffset ofs, wxSeekMode mode = wxFromStart);
    // move ptr to ofs bytes before the end
  wxFileOffset SeekEnd(wxFileOffset ofs = 0) { return Seek(ofs, wxFromEnd); }
    // get current offset
  wxFileOffset Tell() const;
    // get current file length
  wxFileOffset Length() const;

  // simple accessors
    // is file opened?
  bool IsOpened() const { return m_fd != fd_invalid; }
    // is end of file reached?
  bool Eof() const;
    // has an error occurred?
  bool Error() const { return m_lasterror != 0; }
    // get last errno
  int GetLastError() const { return m_lasterror; }
    // reset error state
  void ClearLastError() { m_lasterror = 0; }
    // type such as disk or pipe
  wxFileKind GetKind() const { return wxGetFileKind(m_fd); }


  // dtor closes the file if opened
  ~wxFile() { Close(); }

private:
  // copy ctor and assignment operator are private because
  // it doesn't make sense to copy files this way:
  // attempt to do it will provoke a compile-time error.
  wxFile(const wxFile&);
  wxFile& operator=(const wxFile&);

  // Copy the value of errno into m_lasterror if rc == -1 and return true in
  // this case (indicating that we've got an error). Otherwise return false.
  //
  // Notice that we use the possibly 64 bit wxFileOffset instead of int here so
  // that it works for checking the result of functions such as tell() too.
  bool CheckForError(wxFileOffset rc) const;


  int m_fd; // file descriptor or INVALID_FD if not opened
  int m_lasterror; // errno value of last error
};

// ----------------------------------------------------------------------------
// class wxTempFile: if you want to replace another file, create an instance
// of wxTempFile passing the name of the file to be replaced to the ctor. Then
// you can write to wxTempFile and call Commit() function to replace the old
// file (and close this one) or call Discard() to cancel the modification. If
// you call neither of them, dtor will call Discard().
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxTempFile
{
public:
  // ctors
    // default
  wxTempFile() { }
    // associates the temp file with the file to be replaced and opens it
  wxTempFile(const wxString& strName);

  // open the temp file (strName is the name of file to be replaced)
  bool Open(const wxString& strName);

  // is the file opened?
  bool IsOpened() const { return m_file.IsOpened(); }
    // get current file length
  wxFileOffset Length() const { return m_file.Length(); }
    // move ptr ofs bytes related to start/current offset/end of file
  wxFileOffset Seek(wxFileOffset ofs, wxSeekMode mode = wxFromStart)
    { return m_file.Seek(ofs, mode); }
    // get current offset
  wxFileOffset Tell() const { return m_file.Tell(); }

  // I/O (both functions return true on success, false on failure)
  bool Write(const void *p, size_t n) { return m_file.Write(p, n) == n; }
  bool Write(const wxString& str, const wxMBConv& conv = wxMBConvUTF8())
    { return m_file.Write(str, conv); }

  // flush data: can be called before closing file to ensure that data was
  // correctly written out
  bool Flush() { return m_file.Flush(); }

  // different ways to close the file
    // validate changes and delete the old file of name m_strName
  bool Commit();
    // discard changes
  void Discard();

  // dtor calls Discard() if file is still opened
 ~wxTempFile();

private:
  // no copy ctor/assignment operator
  wxTempFile(const wxTempFile&);
  wxTempFile& operator=(const wxTempFile&);

  wxString  m_strName,  // name of the file to replace in Commit()
            m_strTemp;  // temporary file name
  wxFile    m_file;     // the temporary file
};

#endif // wxUSE_FILE

#endif // _WX_FILEH__
