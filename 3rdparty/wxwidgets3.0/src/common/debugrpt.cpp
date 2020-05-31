///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/debugrpt.cpp
// Purpose:     wxDebugReport and related classes implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-17
// Copyright:   (c) 2005 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/utils.h"
#endif // WX_PRECOMP

#if wxUSE_DEBUGREPORT && wxUSE_XML

#include "wx/debugrpt.h"
#if wxUSE_FFILE
    #include "wx/ffile.h"
#elif wxUSE_FILE
    #include "wx/file.h"
#endif

#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/dynlib.h"

#include "wx/xml/xml.h"

#if wxUSE_STACKWALKER
    #include "wx/stackwalk.h"
#endif

#if wxUSE_CRASHREPORT
    #include "wx/msw/crashrpt.h"
#endif

#if wxUSE_ZIPSTREAM
    #include "wx/wfstream.h"
    #include "wx/zipstrm.h"
#endif // wxUSE_ZIPSTREAM

WX_CHECK_BUILD_OPTIONS("wxQA")

// ----------------------------------------------------------------------------
// XmlStackWalker: stack walker specialization which dumps stack in XML
// ----------------------------------------------------------------------------

#if wxUSE_STACKWALKER

class XmlStackWalker : public wxStackWalker
{
public:
    XmlStackWalker(wxXmlNode *nodeStack)
    {
        m_isOk = false;
        m_nodeStack = nodeStack;
    }

    bool IsOk() const { return m_isOk; }

protected:
    virtual void OnStackFrame(const wxStackFrame& frame);

    wxXmlNode *m_nodeStack;
    bool m_isOk;
};

// ----------------------------------------------------------------------------
// local functions
// ----------------------------------------------------------------------------

static inline void
HexProperty(wxXmlNode *node, const wxChar *name, unsigned long value)
{
    node->AddAttribute(name, wxString::Format(wxT("%08lx"), value));
}

static inline void
NumProperty(wxXmlNode *node, const wxChar *name, unsigned long value)
{
    node->AddAttribute(name, wxString::Format(wxT("%lu"), value));
}

static inline void
TextElement(wxXmlNode *node, const wxChar *name, const wxString& value)
{
    wxXmlNode *nodeChild = new wxXmlNode(wxXML_ELEMENT_NODE, name);
    node->AddChild(nodeChild);
    nodeChild->AddChild(new wxXmlNode(wxXML_TEXT_NODE, wxEmptyString, value));
}

#if wxUSE_CRASHREPORT && defined(__INTEL__)

static inline void
HexElement(wxXmlNode *node, const wxChar *name, unsigned long value)
{
    TextElement(node, name, wxString::Format(wxT("%08lx"), value));
}

#endif // wxUSE_CRASHREPORT

// ============================================================================
// XmlStackWalker implementation
// ============================================================================

void XmlStackWalker::OnStackFrame(const wxStackFrame& frame)
{
    m_isOk = true;

    wxXmlNode *nodeFrame = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("frame"));
    m_nodeStack->AddChild(nodeFrame);

    NumProperty(nodeFrame, wxT("level"), frame.GetLevel());
    wxString func = frame.GetName();
    if ( !func.empty() )
    {
        nodeFrame->AddAttribute(wxT("function"), func);
        HexProperty(nodeFrame, wxT("offset"), frame.GetOffset());
    }

    if ( frame.HasSourceLocation() )
    {
        nodeFrame->AddAttribute(wxT("file"), frame.GetFileName());
        NumProperty(nodeFrame, wxT("line"), frame.GetLine());
    }

    const size_t nParams = frame.GetParamCount();
    if ( nParams )
    {
        wxXmlNode *nodeParams = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("parameters"));
        nodeFrame->AddChild(nodeParams);

        for ( size_t n = 0; n < nParams; n++ )
        {
            wxXmlNode *
                nodeParam = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("parameter"));
            nodeParams->AddChild(nodeParam);

            NumProperty(nodeParam, wxT("number"), n);

            wxString type, name, value;
            if ( !frame.GetParam(n, &type, &name, &value) )
                continue;

            if ( !type.empty() )
                TextElement(nodeParam, wxT("type"), type);

            if ( !name.empty() )
                TextElement(nodeParam, wxT("name"), name);

            if ( !value.empty() )
                TextElement(nodeParam, wxT("value"), value);
        }
    }
}

#endif // wxUSE_STACKWALKER

// ============================================================================
// wxDebugReport implementation
// ============================================================================

// ----------------------------------------------------------------------------
// initialization and cleanup
// ----------------------------------------------------------------------------

wxDebugReport::wxDebugReport()
{
    // get a temporary directory name
    wxString appname = GetReportName();

    // we can't use CreateTempFileName() because it creates a file, not a
    // directory, so do our best to create a unique name ourselves
    //
    // of course, this doesn't protect us against malicious users...
#if wxUSE_DATETIME
    m_dir.Printf(wxT("%s%c%s_dbgrpt-%lu-%s"),
                 wxFileName::GetTempDir(), wxFILE_SEP_PATH, appname,
                 wxGetProcessId(),
                 wxDateTime::Now().Format(wxT("%Y%m%dT%H%M%S")));
#else
    m_dir.Printf(wxT("%s%c%s_dbgrpt-%lu"),
                 wxFileName::GetTempDir(), wxFILE_SEP_PATH, appname,
                 wxGetProcessId());
#endif

    // as we are going to save the process state there use restrictive
    // permissions
    if ( !wxMkdir(m_dir, 0700) )
    {
        wxLogSysError(_("Failed to create directory \"%s\""), m_dir.c_str());
        wxLogError(_("Debug report couldn't be created."));

        Reset();
    }
}

wxDebugReport::~wxDebugReport()
{
    if ( !m_dir.empty() )
    {
        // remove all files in this directory
        wxDir dir(m_dir);
        wxString file;
        for ( bool cont = dir.GetFirst(&file); cont; cont = dir.GetNext(&file) )
        {
            if ( wxRemove(wxFileName(m_dir, file).GetFullPath()) != 0 )
            {
                wxLogSysError(_("Failed to remove debug report file \"%s\""),
                              file.c_str());
                m_dir.clear();
                break;
            }
        }
    }

    if ( !m_dir.empty() )
    {
        // Temp fix: what should this be? eVC++ doesn't like wxRmDir
#ifdef __WXWINCE__
        if ( wxRmdir(m_dir.fn_str()) != 0 )
#else
        if ( wxRmDir(m_dir.fn_str()) != 0 )
#endif
        {
            wxLogSysError(_("Failed to clean up debug report directory \"%s\""),
                          m_dir.c_str());
        }
    }
}

// ----------------------------------------------------------------------------
// various helpers
// ----------------------------------------------------------------------------

wxString wxDebugReport::GetReportName() const
{
    if ( wxTheApp )
        return wxTheApp->GetAppName();

    return wxT("wx");
}

void
wxDebugReport::AddFile(const wxString& filename, const wxString& description)
{
    wxString name;
    wxFileName fn(filename);
    if ( fn.IsAbsolute() )
    {
        // we need to copy the file to the debug report directory: give it the
        // same name there
        name = fn.GetFullName();

        if (!wxCopyFile(fn.GetFullPath(),
                        wxFileName(GetDirectory(), name).GetFullPath()))
           return;
    }
    else // file relative to the report directory
    {
        name = filename;

        wxASSERT_MSG( wxFileName(GetDirectory(), name).FileExists(),
                      wxT("file should exist in debug report directory") );
    }

    m_files.Add(name);
    m_descriptions.Add(description);
}

bool
wxDebugReport::AddText(const wxString& filename,
                       const wxString& text,
                       const wxString& description)
{
#if wxUSE_FFILE || wxUSE_FILE
    wxASSERT_MSG( !wxFileName(filename).IsAbsolute(),
                  wxT("filename should be relative to debug report directory") );

    const wxString fullPath = wxFileName(GetDirectory(), filename).GetFullPath();
#if wxUSE_FFILE
    wxFFile file(fullPath, wxT("w"));
#elif wxUSE_FILE
    wxFile file(fullPath, wxFile::write);
#endif
    if ( !file.IsOpened() || !file.Write(text, wxConvAuto()) )
        return false;

    AddFile(filename, description);

    return true;
#else // !wxUSE_FFILE && !wxUSE_FILE
    return false;
#endif
}

void wxDebugReport::RemoveFile(const wxString& name)
{
    const int n = m_files.Index(name);
    wxCHECK_RET( n != wxNOT_FOUND, wxT("No such file in wxDebugReport") );

    m_files.RemoveAt(n);
    m_descriptions.RemoveAt(n);

    wxRemove(wxFileName(GetDirectory(), name).GetFullPath());
}

bool wxDebugReport::GetFile(size_t n, wxString *name, wxString *desc) const
{
    if ( n >= m_files.GetCount() )
        return false;

    if ( name )
        *name = m_files[n];
    if ( desc )
        *desc = m_descriptions[n];

    return true;
}

void wxDebugReport::AddAll(Context context)
{
#if wxUSE_STACKWALKER
    AddContext(context);
#endif // wxUSE_STACKWALKER

#if wxUSE_CRASHREPORT
    AddDump(context);
#endif // wxUSE_CRASHREPORT

#if !wxUSE_STACKWALKER && !wxUSE_CRASHREPORT
    wxUnusedVar(context);
#endif
}

// ----------------------------------------------------------------------------
// adding basic text information about current context
// ----------------------------------------------------------------------------

#if wxUSE_STACKWALKER

bool wxDebugReport::DoAddSystemInfo(wxXmlNode *nodeSystemInfo)
{
    nodeSystemInfo->AddAttribute(wxT("description"), wxGetOsDescription());

    return true;
}

bool wxDebugReport::DoAddLoadedModules(wxXmlNode *nodeModules)
{
    wxDynamicLibraryDetailsArray modules(wxDynamicLibrary::ListLoaded());
    const size_t count = modules.GetCount();
    if ( !count )
        return false;

    for ( size_t n = 0; n < count; n++ )
    {
        const wxDynamicLibraryDetails& info = modules[n];

        wxXmlNode *nodeModule = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("module"));
        nodeModules->AddChild(nodeModule);

        wxString path = info.GetPath();
        if ( path.empty() )
            path = info.GetName();
        if ( !path.empty() )
            nodeModule->AddAttribute(wxT("path"), path);

        void *addr = NULL;
        size_t len = 0;
        if ( info.GetAddress(&addr, &len) )
        {
            HexProperty(nodeModule, wxT("address"), wxPtrToUInt(addr));
            HexProperty(nodeModule, wxT("size"), len);
        }

        wxString ver = info.GetVersion();
        if ( !ver.empty() )
        {
            nodeModule->AddAttribute(wxT("version"), ver);
        }
    }

    return true;
}

bool wxDebugReport::DoAddExceptionInfo(wxXmlNode *nodeContext)
{
#if wxUSE_CRASHREPORT
    wxCrashContext c;
    if ( !c.code )
        return false;

    wxXmlNode *nodeExc = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("exception"));
    nodeContext->AddChild(nodeExc);

    HexProperty(nodeExc, wxT("code"), c.code);
    nodeExc->AddAttribute(wxT("name"), c.GetExceptionString());
    HexProperty(nodeExc, wxT("address"), wxPtrToUInt(c.addr));

#ifdef __INTEL__
    wxXmlNode *nodeRegs = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("registers"));
    nodeContext->AddChild(nodeRegs);
    HexElement(nodeRegs, wxT("eax"), c.regs.eax);
    HexElement(nodeRegs, wxT("ebx"), c.regs.ebx);
    HexElement(nodeRegs, wxT("ecx"), c.regs.edx);
    HexElement(nodeRegs, wxT("edx"), c.regs.edx);
    HexElement(nodeRegs, wxT("esi"), c.regs.esi);
    HexElement(nodeRegs, wxT("edi"), c.regs.edi);

    HexElement(nodeRegs, wxT("ebp"), c.regs.ebp);
    HexElement(nodeRegs, wxT("esp"), c.regs.esp);
    HexElement(nodeRegs, wxT("eip"), c.regs.eip);

    HexElement(nodeRegs, wxT("cs"), c.regs.cs);
    HexElement(nodeRegs, wxT("ds"), c.regs.ds);
    HexElement(nodeRegs, wxT("es"), c.regs.es);
    HexElement(nodeRegs, wxT("fs"), c.regs.fs);
    HexElement(nodeRegs, wxT("gs"), c.regs.gs);
    HexElement(nodeRegs, wxT("ss"), c.regs.ss);

    HexElement(nodeRegs, wxT("flags"), c.regs.flags);
#endif // __INTEL__

    return true;
#else // !wxUSE_CRASHREPORT
    wxUnusedVar(nodeContext);

    return false;
#endif // wxUSE_CRASHREPORT/!wxUSE_CRASHREPORT
}

bool wxDebugReport::AddContext(wxDebugReport::Context ctx)
{
    wxCHECK_MSG( IsOk(), false, wxT("use IsOk() first") );

    // create XML dump of current context
    wxXmlDocument xmldoc;
    wxXmlNode *nodeRoot = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("report"));
    xmldoc.SetRoot(nodeRoot);
    nodeRoot->AddAttribute(wxT("version"), wxT("1.0"));
    nodeRoot->AddAttribute(wxT("kind"), ctx == Context_Current ? wxT("user")
                                                             : wxT("exception"));

    // add system information
    wxXmlNode *nodeSystemInfo = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("system"));
    if ( DoAddSystemInfo(nodeSystemInfo) )
        nodeRoot->AddChild(nodeSystemInfo);
    else
        delete nodeSystemInfo;

    // add information about the loaded modules
    wxXmlNode *nodeModules = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("modules"));
    if ( DoAddLoadedModules(nodeModules) )
        nodeRoot->AddChild(nodeModules);
    else
        delete nodeModules;

    // add CPU context information: this only makes sense for exceptions as our
    // current context is not very interesting otherwise
    if ( ctx == Context_Exception )
    {
        wxXmlNode *nodeContext = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("context"));
        if ( DoAddExceptionInfo(nodeContext) )
            nodeRoot->AddChild(nodeContext);
        else
            delete nodeContext;
    }

    // add stack traceback
#if wxUSE_STACKWALKER
    wxXmlNode *nodeStack = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("stack"));
    XmlStackWalker sw(nodeStack);
#if wxUSE_ON_FATAL_EXCEPTION
    if ( ctx == Context_Exception )
    {
        sw.WalkFromException();
    }
    else // Context_Current
#endif // wxUSE_ON_FATAL_EXCEPTION
    {
        sw.Walk();
    }

    if ( sw.IsOk() )
        nodeRoot->AddChild(nodeStack);
    else
        delete nodeStack;
#endif // wxUSE_STACKWALKER

    // finally let the user add any extra information he needs
    DoAddCustomContext(nodeRoot);


    // save the entire context dump in a file
    wxFileName fn(m_dir, GetReportName(), wxT("xml"));

    if ( !xmldoc.Save(fn.GetFullPath()) )
        return false;

    AddFile(fn.GetFullName(), _("process context description"));

    return true;
}

#endif // wxUSE_STACKWALKER

// ----------------------------------------------------------------------------
// adding core dump
// ----------------------------------------------------------------------------

#if wxUSE_CRASHREPORT

bool wxDebugReport::AddDump(Context ctx)
{
    wxCHECK_MSG( IsOk(), false, wxT("use IsOk() first") );

    wxFileName fn(m_dir, GetReportName(), wxT("dmp"));
    wxCrashReport::SetFileName(fn.GetFullPath());

    if ( !(ctx == Context_Exception ? wxCrashReport::Generate()
                                    : wxCrashReport::GenerateNow()) )
            return false;

    AddFile(fn.GetFullName(), _("dump of the process state (binary)"));

    return true;
}

#endif // wxUSE_CRASHREPORT

// ----------------------------------------------------------------------------
// report processing
// ----------------------------------------------------------------------------

bool wxDebugReport::Process()
{
    if ( !GetFilesCount() )
    {
        wxLogError(_("Debug report generation has failed."));

        return false;
    }

    if ( !DoProcess() )
    {
        wxLogError(_("Processing debug report has failed, leaving the files in \"%s\" directory."),
                   GetDirectory().c_str());

        Reset();

        return false;
    }

    return true;
}

bool wxDebugReport::DoProcess()
{
    wxString msg(_("A debug report has been generated. It can be found in"));
    msg << wxT("\n")
           wxT("\t") << GetDirectory() << wxT("\n\n")
        << _("And includes the following files:\n");

    wxString name, desc;
    const size_t count = GetFilesCount();
    for ( size_t n = 0; n < count; n++ )
    {
        GetFile(n, &name, &desc);
        msg += wxString::Format("\t%s: %s\n", name, desc);
    }

    msg += _("\nPlease send this report to the program maintainer, thank you!\n");

    wxLogMessage(wxT("%s"), msg.c_str());

    // we have to do this or the report would be deleted, and we don't even
    // have any way to ask the user if he wants to keep it from here
    Reset();

    return true;
}

// ============================================================================
// wxDebugReport-derived classes
// ============================================================================

#if wxUSE_ZIPSTREAM

// ----------------------------------------------------------------------------
// wxDebugReportCompress
// ----------------------------------------------------------------------------

void wxDebugReportCompress::SetCompressedFileDirectory(const wxString& dir)
{
    wxASSERT_MSG( m_zipfile.empty(), "Too late: call this before Process()" );

    m_zipDir = dir;
}

void wxDebugReportCompress::SetCompressedFileBaseName(const wxString& name)
{
    wxASSERT_MSG( m_zipfile.empty(), "Too late: call this before Process()" );

    m_zipName = name;
}

bool wxDebugReportCompress::DoProcess()
{
#define HAS_FILE_STREAMS (wxUSE_STREAMS && (wxUSE_FILE || wxUSE_FFILE))
#if HAS_FILE_STREAMS
    const size_t count = GetFilesCount();
    if ( !count )
        return false;

    // create the compressed report file outside of the directory with the
    // report files as it will be deleted by wxDebugReport dtor but we want to
    // keep this one: for this we simply treat the directory name as the name
    // of the file so that its last component becomes our base name
    wxFileName fn(GetDirectory());
    if ( !m_zipDir.empty() )
        fn.SetPath(m_zipDir);
    if ( !m_zipName.empty() )
        fn.SetName(m_zipName);
    fn.SetExt("zip");

    // create the streams
    const wxString ofullPath = fn.GetFullPath();
#if wxUSE_FFILE
    wxFFileOutputStream os(ofullPath, wxT("wb"));
#elif wxUSE_FILE
    wxFileOutputStream os(ofullPath);
#endif
    if ( !os.IsOk() )
        return false;
    wxZipOutputStream zos(os, 9);

    // add all files to the ZIP one
    wxString name, desc;
    for ( size_t n = 0; n < count; n++ )
    {
        GetFile(n, &name, &desc);

        wxZipEntry *ze = new wxZipEntry(name);
        ze->SetComment(desc);

        if ( !zos.PutNextEntry(ze) )
            return false;

        const wxString ifullPath = wxFileName(GetDirectory(), name).GetFullPath();
#if wxUSE_FFILE
        wxFFileInputStream is(ifullPath);
#elif wxUSE_FILE
        wxFileInputStream is(ifullPath);
#endif
        if ( !is.IsOk() || !zos.Write(is).IsOk() )
            return false;
    }

    if ( !zos.Close() )
        return false;

    m_zipfile = ofullPath;

    return true;
#else
    return false;
#endif // HAS_FILE_STREAMS
}

// ----------------------------------------------------------------------------
// wxDebugReportUpload
// ----------------------------------------------------------------------------

wxDebugReportUpload::wxDebugReportUpload(const wxString& url,
                                         const wxString& input,
                                         const wxString& action,
                                         const wxString& curl)
                   : m_uploadURL(url),
                     m_inputField(input),
                     m_curlCmd(curl)
{
    if ( m_uploadURL.Last() != wxT('/') )
        m_uploadURL += wxT('/');
    m_uploadURL += action;
}

bool wxDebugReportUpload::DoProcess()
{
    if ( !wxDebugReportCompress::DoProcess() )
        return false;


    wxArrayString output, errors;
    int rc = wxExecute(wxString::Format
                       (
                            wxT("%s -F \"%s=@%s\" %s"),
                            m_curlCmd.c_str(),
                            m_inputField.c_str(),
                            GetCompressedFileName().c_str(),
                            m_uploadURL.c_str()
                       ),
                       output,
                       errors);
    if ( rc == -1 )
    {
        wxLogError(_("Failed to execute curl, please install it in PATH."));
    }
    else if ( rc != 0 )
    {
        const size_t count = errors.GetCount();
        if ( count )
        {
            for ( size_t n = 0; n < count; n++ )
            {
                wxLogWarning(wxT("%s"), errors[n].c_str());
            }
        }

        wxLogError(_("Failed to upload the debug report (error code %d)."), rc);
    }
    else // rc == 0
    {
        if ( OnServerReply(output) )
            return true;
    }

    return false;
}

#endif // wxUSE_ZIPSTREAM

#endif // wxUSE_DEBUGREPORT
