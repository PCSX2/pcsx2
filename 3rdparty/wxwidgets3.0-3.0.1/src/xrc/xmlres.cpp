/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xmlres.cpp
// Purpose:     XRC resources
// Author:      Vaclav Slavik
// Created:     2000/03/05
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC

#include "wx/xrc/xmlres.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/panel.h"
    #include "wx/frame.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/bitmap.h"
    #include "wx/image.h"
    #include "wx/module.h"
    #include "wx/wxcrtvararg.h"
#endif

#ifndef __WXWINCE__
    #include <locale.h>
#endif

#include "wx/vector.h"
#include "wx/wfstream.h"
#include "wx/filesys.h"
#include "wx/filename.h"
#include "wx/tokenzr.h"
#include "wx/fontenum.h"
#include "wx/fontmap.h"
#include "wx/artprov.h"
#include "wx/imaglist.h"
#include "wx/dir.h"
#include "wx/xml/xml.h"
#include "wx/hashset.h"
#include "wx/scopedptr.h"

namespace
{

// Helper function to get modification time of either a wxFileSystem URI or
// just a normal file name, depending on the build.
#if wxUSE_DATETIME

wxDateTime GetXRCFileModTime(const wxString& filename)
{
#if wxUSE_FILESYSTEM
    wxFileSystem fsys;
    wxScopedPtr<wxFSFile> file(fsys.OpenFile(filename));

    return file ? file->GetModificationTime() : wxDateTime();
#else // wxUSE_FILESYSTEM
    return wxDateTime(wxFileModificationTime(filename));
#endif // wxUSE_FILESYSTEM
}

#endif // wxUSE_DATETIME

} // anonymous namespace

// Assign the given value to the specified entry or add a new value with this
// name.
static void XRCID_Assign(const wxString& str_id, int value);

class wxXmlResourceDataRecord
{
public:
    // Ctor takes ownership of the document pointer.
    wxXmlResourceDataRecord(const wxString& File_,
                            wxXmlDocument *Doc_
                           )
        : File(File_), Doc(Doc_)
    {
#if wxUSE_DATETIME
        Time = GetXRCFileModTime(File);
#endif
    }

    ~wxXmlResourceDataRecord() {delete Doc;}

    wxString File;
    wxXmlDocument *Doc;
#if wxUSE_DATETIME
    wxDateTime Time;
#endif

    wxDECLARE_NO_COPY_CLASS(wxXmlResourceDataRecord);
};

class wxXmlResourceDataRecords : public wxVector<wxXmlResourceDataRecord*>
{
    // this is a class so that it can be forward-declared
};

WX_DECLARE_HASH_SET_PTR(int, wxIntegerHash, wxIntegerEqual, wxHashSetInt);

class wxIdRange // Holds data for a particular rangename
{
protected:
    wxIdRange(const wxXmlNode* node,
              const wxString& rname,
              const wxString& startno,
              const wxString& rsize);

    // Note the existence of an item within the range
    void NoteItem(const wxXmlNode* node, const wxString& item);

    // The manager is telling us that it's finished adding items
    void Finalise(const wxXmlNode* node);

    wxString GetName() const { return m_name; }
    bool IsFinalised() const { return m_finalised; }

    const wxString m_name;
    int m_start;
    int m_end;
    unsigned int m_size;
    bool m_item_end_found;
    bool m_finalised;
    wxHashSetInt m_indices;

    friend class wxIdRangeManager;
};

class wxIdRangeManager
{
public:
    ~wxIdRangeManager();
    // Gets the global resources object or creates one if none exists.
    static wxIdRangeManager *Get();

    // Sets the global resources object and returns a pointer to the previous
    // one (may be NULL).
    static wxIdRangeManager *Set(wxIdRangeManager *res);

    // Create a new IDrange from this node
    void AddRange(const wxXmlNode* node);
    // Tell the IdRange that this item exists, and should be pre-allocated an ID
    void NotifyRangeOfItem(const wxXmlNode* node, const wxString& item) const;
    // Tells all IDranges that they're now complete, and can create their IDs
    void FinaliseRanges(const wxXmlNode* node) const;
    // Searches for a known IdRange matching 'name', returning its index or -1
    int Find(const wxString& rangename) const;

protected:
    wxIdRange* FindRangeForItem(const wxXmlNode* node,
                                const wxString& item,
                                wxString& value) const;
    wxVector<wxIdRange*> m_IdRanges;

private:
    static wxIdRangeManager *ms_instance;
};

namespace
{

// helper used by DoFindResource() and elsewhere: returns true if this is an
// object or object_ref node
//
// node must be non-NULL
inline bool IsObjectNode(wxXmlNode *node)
{
    return node->GetType() == wxXML_ELEMENT_NODE &&
             (node->GetName() == wxS("object") ||
                node->GetName() == wxS("object_ref"));
}

// special XML attribute with name of input file, see GetFileNameFromNode()
const char *ATTR_INPUT_FILENAME = "__wx:filename";

// helper to get filename corresponding to an XML node
wxString
GetFileNameFromNode(const wxXmlNode *node, const wxXmlResourceDataRecords& files)
{
    // this loop does two things: it looks for ATTR_INPUT_FILENAME among
    // parents and if it isn't used, it finds the root of the XML tree 'node'
    // is in
    for ( ;; )
    {
        // in some rare cases (specifically, when an <object_ref> is used, see
        // wxXmlResource::CreateResFromNode() and MergeNodesOver()), we work
        // with XML nodes that are not rooted in any document from 'files'
        // (because a new node was created by CreateResFromNode() to merge the
        // content of <object_ref> and the referenced <object>); in that case,
        // we hack around the problem by putting the information about input
        // file into a custom attribute
        if ( node->HasAttribute(ATTR_INPUT_FILENAME) )
            return node->GetAttribute(ATTR_INPUT_FILENAME);

        if ( !node->GetParent() )
            break; // we found the root of this XML tree

        node = node->GetParent();
    }

    // NB: 'node' now points to the root of XML document

    for ( wxXmlResourceDataRecords::const_iterator i = files.begin();
          i != files.end(); ++i )
    {
        if ( (*i)->Doc->GetRoot() == node )
        {
            return (*i)->File;
        }
    }

    return wxEmptyString; // not found
}

} // anonymous namespace


wxXmlResource *wxXmlResource::ms_instance = NULL;

/*static*/ wxXmlResource *wxXmlResource::Get()
{
    if ( !ms_instance )
        ms_instance = new wxXmlResource;
    return ms_instance;
}

/*static*/ wxXmlResource *wxXmlResource::Set(wxXmlResource *res)
{
    wxXmlResource *old = ms_instance;
    ms_instance = res;
    return old;
}

wxXmlResource::wxXmlResource(int flags, const wxString& domain)
{
    m_flags = flags;
    m_version = -1;
    m_data = new wxXmlResourceDataRecords;
    SetDomain(domain);
}

wxXmlResource::wxXmlResource(const wxString& filemask, int flags, const wxString& domain)
{
    m_flags = flags;
    m_version = -1;
    m_data = new wxXmlResourceDataRecords;
    SetDomain(domain);
    Load(filemask);
}

wxXmlResource::~wxXmlResource()
{
    ClearHandlers();

    for ( wxXmlResourceDataRecords::iterator i = m_data->begin();
          i != m_data->end(); ++i )
    {
        delete *i;
    }
    delete m_data;
}

void wxXmlResource::SetDomain(const wxString& domain)
{
    m_domain = domain;
}


/* static */
wxString wxXmlResource::ConvertFileNameToURL(const wxString& filename)
{
    wxString fnd(filename);

    // NB: as Load() and Unload() accept both filenames and URLs (should
    //     probably be changed to filenames only, but embedded resources
    //     currently rely on its ability to handle URLs - FIXME) we need to
    //     determine whether found name is filename and not URL and this is the
    //     fastest/simplest way to do it
    if (wxFileName::FileExists(fnd))
    {
        // Make the name absolute filename, because the app may
        // change working directory later:
        wxFileName fn(fnd);
        if (fn.IsRelative())
        {
            fn.MakeAbsolute();
            fnd = fn.GetFullPath();
        }
#if wxUSE_FILESYSTEM
        fnd = wxFileSystem::FileNameToURL(fnd);
#endif
    }

    return fnd;
}

#if wxUSE_FILESYSTEM

/* static */
bool wxXmlResource::IsArchive(const wxString& filename)
{
    const wxString fnd = filename.Lower();

    return fnd.Matches(wxT("*.zip")) || fnd.Matches(wxT("*.xrs"));
}

#endif // wxUSE_FILESYSTEM

bool wxXmlResource::LoadFile(const wxFileName& file)
{
#if wxUSE_FILESYSTEM
    return Load(wxFileSystem::FileNameToURL(file));
#else
    return Load(file.GetFullPath());
#endif
}

bool wxXmlResource::LoadAllFiles(const wxString& dirname)
{
    bool ok = true;
    wxArrayString files;

    wxDir::GetAllFiles(dirname, &files, "*.xrc");

    for ( wxArrayString::const_iterator i = files.begin(); i != files.end(); ++i )
    {
        if ( !LoadFile(*i) )
            ok = false;
    }

    return ok;
}

bool wxXmlResource::Load(const wxString& filemask_)
{
    wxString filemask = ConvertFileNameToURL(filemask_);

    bool allOK = true;

#if wxUSE_FILESYSTEM
    wxFileSystem fsys;
#   define wxXmlFindFirst  fsys.FindFirst(filemask, wxFILE)
#   define wxXmlFindNext   fsys.FindNext()
#else
#   define wxXmlFindFirst  wxFindFirstFile(filemask, wxFILE)
#   define wxXmlFindNext   wxFindNextFile()
#endif
    wxString fnd = wxXmlFindFirst;
    if ( fnd.empty() )
    {
        wxLogError(_("Cannot load resources from '%s'."), filemask);
        return false;
    }

    while (!fnd.empty())
    {
#if wxUSE_FILESYSTEM
        if ( IsArchive(fnd) )
        {
            if ( !Load(fnd + wxT("#zip:*.xrc")) )
                allOK = false;
        }
        else // a single resource URL
#endif // wxUSE_FILESYSTEM
        {
            wxXmlDocument * const doc = DoLoadFile(fnd);
            if ( !doc )
                allOK = false;
            else
                Data().push_back(new wxXmlResourceDataRecord(fnd, doc));
        }

        fnd = wxXmlFindNext;
    }
#   undef wxXmlFindFirst
#   undef wxXmlFindNext

    return allOK;
}

bool wxXmlResource::Unload(const wxString& filename)
{
    wxASSERT_MSG( !wxIsWild(filename),
                    wxT("wildcards not supported by wxXmlResource::Unload()") );

    wxString fnd = ConvertFileNameToURL(filename);
#if wxUSE_FILESYSTEM
    const bool isArchive = IsArchive(fnd);
    if ( isArchive )
        fnd += wxT("#zip:");
#endif // wxUSE_FILESYSTEM

    bool unloaded = false;
    for ( wxXmlResourceDataRecords::iterator i = Data().begin();
          i != Data().end(); ++i )
    {
#if wxUSE_FILESYSTEM
        if ( isArchive )
        {
            if ( (*i)->File.StartsWith(fnd) )
                unloaded = true;
            // don't break from the loop, we can have other matching files
        }
        else // a single resource URL
#endif // wxUSE_FILESYSTEM
        {
            if ( (*i)->File == fnd )
            {
                delete *i;
                Data().erase(i);
                unloaded = true;

                // no sense in continuing, there is only one file with this URL
                break;
            }
        }
    }

    return unloaded;
}


void wxXmlResource::AddHandler(wxXmlResourceHandler *handler)
{
    wxXmlResourceHandlerImpl *impl = new wxXmlResourceHandlerImpl(handler);
    handler->SetImpl(impl);
    m_handlers.push_back(handler);
    handler->SetParentResource(this);
}

void wxXmlResource::InsertHandler(wxXmlResourceHandler *handler)
{
    wxXmlResourceHandlerImpl *impl = new wxXmlResourceHandlerImpl(handler);
    handler->SetImpl(impl);
    m_handlers.insert(m_handlers.begin(), handler);
    handler->SetParentResource(this);
}



void wxXmlResource::ClearHandlers()
{
    for ( wxVector<wxXmlResourceHandler*>::iterator i = m_handlers.begin();
          i != m_handlers.end(); ++i )
        delete *i;
    m_handlers.clear();
}


wxMenu *wxXmlResource::LoadMenu(const wxString& name)
{
    return (wxMenu*)CreateResFromNode(FindResource(name, wxT("wxMenu")), NULL, NULL);
}



wxMenuBar *wxXmlResource::LoadMenuBar(wxWindow *parent, const wxString& name)
{
    return (wxMenuBar*)CreateResFromNode(FindResource(name, wxT("wxMenuBar")), parent, NULL);
}



#if wxUSE_TOOLBAR
wxToolBar *wxXmlResource::LoadToolBar(wxWindow *parent, const wxString& name)
{
    return (wxToolBar*)CreateResFromNode(FindResource(name, wxT("wxToolBar")), parent, NULL);
}
#endif


wxDialog *wxXmlResource::LoadDialog(wxWindow *parent, const wxString& name)
{
    return (wxDialog*)CreateResFromNode(FindResource(name, wxT("wxDialog")), parent, NULL);
}

bool wxXmlResource::LoadDialog(wxDialog *dlg, wxWindow *parent, const wxString& name)
{
    return CreateResFromNode(FindResource(name, wxT("wxDialog")), parent, dlg) != NULL;
}



wxPanel *wxXmlResource::LoadPanel(wxWindow *parent, const wxString& name)
{
    return (wxPanel*)CreateResFromNode(FindResource(name, wxT("wxPanel")), parent, NULL);
}

bool wxXmlResource::LoadPanel(wxPanel *panel, wxWindow *parent, const wxString& name)
{
    return CreateResFromNode(FindResource(name, wxT("wxPanel")), parent, panel) != NULL;
}

wxFrame *wxXmlResource::LoadFrame(wxWindow* parent, const wxString& name)
{
    return (wxFrame*)CreateResFromNode(FindResource(name, wxT("wxFrame")), parent, NULL);
}

bool wxXmlResource::LoadFrame(wxFrame* frame, wxWindow *parent, const wxString& name)
{
    return CreateResFromNode(FindResource(name, wxT("wxFrame")), parent, frame) != NULL;
}

wxBitmap wxXmlResource::LoadBitmap(const wxString& name)
{
    wxBitmap *bmp = (wxBitmap*)CreateResFromNode(
                               FindResource(name, wxT("wxBitmap")), NULL, NULL);
    wxBitmap rt;

    if (bmp) { rt = *bmp; delete bmp; }
    return rt;
}

wxIcon wxXmlResource::LoadIcon(const wxString& name)
{
    wxIcon *icon = (wxIcon*)CreateResFromNode(
                            FindResource(name, wxT("wxIcon")), NULL, NULL);
    wxIcon rt;

    if (icon) { rt = *icon; delete icon; }
    return rt;
}


wxObject *
wxXmlResource::DoLoadObject(wxWindow *parent,
                            const wxString& name,
                            const wxString& classname,
                            bool recursive)
{
    wxXmlNode * const node = FindResource(name, classname, recursive);

    return node ? DoCreateResFromNode(*node, parent, NULL) : NULL;
}

bool
wxXmlResource::DoLoadObject(wxObject *instance,
                            wxWindow *parent,
                            const wxString& name,
                            const wxString& classname,
                            bool recursive)
{
    wxXmlNode * const node = FindResource(name, classname, recursive);

    return node && DoCreateResFromNode(*node, parent, instance) != NULL;
}


bool wxXmlResource::AttachUnknownControl(const wxString& name,
                                         wxWindow *control, wxWindow *parent)
{
    if (parent == NULL)
        parent = control->GetParent();
    wxWindow *container = parent->FindWindow(name + wxT("_container"));
    if (!container)
    {
        wxLogError("Cannot find container for unknown control '%s'.", name);
        return false;
    }
    return control->Reparent(container);
}


static void ProcessPlatformProperty(wxXmlNode *node)
{
    wxString s;
    bool isok;

    wxXmlNode *c = node->GetChildren();
    while (c)
    {
        isok = false;
        if (!c->GetAttribute(wxT("platform"), &s))
            isok = true;
        else
        {
            wxStringTokenizer tkn(s, wxT(" |"));

            while (tkn.HasMoreTokens())
            {
                s = tkn.GetNextToken();
#ifdef __WINDOWS__
                if (s == wxT("win")) isok = true;
#endif
#if defined(__MAC__) || defined(__APPLE__)
                if (s == wxT("mac")) isok = true;
#elif defined(__UNIX__)
                if (s == wxT("unix")) isok = true;
#endif
#ifdef __OS2__
                if (s == wxT("os2")) isok = true;
#endif

                if (isok)
                    break;
            }
        }

        if (isok)
        {
            ProcessPlatformProperty(c);
            c = c->GetNext();
        }
        else
        {
            wxXmlNode *c2 = c->GetNext();
            node->RemoveChild(c);
            delete c;
            c = c2;
        }
    }
}

static void PreprocessForIdRanges(wxXmlNode *rootnode)
{
    // First go through the top level, looking for the names of ID ranges
    // as processing items is a lot easier if names are already known
    wxXmlNode *c = rootnode->GetChildren();
    while (c)
    {
        if (c->GetName() == wxT("ids-range"))
            wxIdRangeManager::Get()->AddRange(c);
        c = c->GetNext();
    }

    // Next, examine every 'name' for the '[' that denotes an ID in a range
    c = rootnode->GetChildren();
    while (c)
    {
        wxString name = c->GetAttribute(wxT("name"));
        if (name.find('[') != wxString::npos)
            wxIdRangeManager::Get()->NotifyRangeOfItem(rootnode, name);

        // Do any children by recursion, then proceed to the next sibling
        PreprocessForIdRanges(c);
        c = c->GetNext();
    }
}

bool wxXmlResource::UpdateResources()
{
    bool rt = true;

    for ( wxXmlResourceDataRecords::iterator i = Data().begin();
          i != Data().end(); ++i )
    {
        wxXmlResourceDataRecord* const rec = *i;

        // Check if we need to reload this one.

        // We never do it if this flag is specified.
        if ( m_flags & wxXRC_NO_RELOADING )
            continue;

        // Otherwise check its modification time if we can.
#if wxUSE_DATETIME
        const wxDateTime lastModTime = GetXRCFileModTime(rec->File);

        if ( lastModTime.IsValid() && lastModTime <= rec->Time )
#else // !wxUSE_DATETIME
        // Never reload the file contents: we can't know whether it changed or
        // not in this build configuration and it would be unexpected and
        // counter-productive to get a performance hit (due to constant
        // reloading of XRC files) in a minimal wx build which is presumably
        // used because of resource constraints of the current platform.
#endif // wxUSE_DATETIME/!wxUSE_DATETIME
        {
            // No need to reload, the file wasn't modified since we did it
            // last.
            continue;
        }

        wxXmlDocument * const doc = DoLoadFile(rec->File);
        if ( !doc )
        {
            // Notice that we keep the old XML document: it seems better to
            // preserve it instead of throwing it away if we have nothing to
            // replace it with.
            rt = false;
            continue;
        }

        // Replace the old resource contents with the new one.
        delete rec->Doc;
        rec->Doc = doc;

        // And, now that we loaded it successfully, update the last load time.
#if wxUSE_DATETIME
        rec->Time = lastModTime.IsValid() ? lastModTime : wxDateTime::Now();
#endif // wxUSE_DATETIME
    }

    return rt;
}

wxXmlDocument *wxXmlResource::DoLoadFile(const wxString& filename)
{
    wxLogTrace(wxT("xrc"), wxT("opening file '%s'"), filename);

    wxInputStream *stream = NULL;

#if wxUSE_FILESYSTEM
    wxFileSystem fsys;
    wxScopedPtr<wxFSFile> file(fsys.OpenFile(filename));
    if (file)
    {
        // Notice that we don't have ownership of the stream in this case, it
        // remains owned by wxFSFile.
        stream = file->GetStream();
    }
#else // !wxUSE_FILESYSTEM
    wxFileInputStream fstream(filename);
    stream = &fstream;
#endif // wxUSE_FILESYSTEM/!wxUSE_FILESYSTEM

    if ( !stream || !stream->IsOk() )
    {
        wxLogError(_("Cannot open resources file '%s'."), filename);
        return NULL;
    }

    wxString encoding(wxT("UTF-8"));
#if !wxUSE_UNICODE && wxUSE_INTL
    if ( (GetFlags() & wxXRC_USE_LOCALE) == 0 )
    {
        // In case we are not using wxLocale to translate strings, convert the
        // strings GUI's charset. This must not be done when wxXRC_USE_LOCALE
        // is on, because it could break wxGetTranslation lookup.
        encoding = wxLocale::GetSystemEncodingName();
    }
#endif

    wxScopedPtr<wxXmlDocument> doc(new wxXmlDocument);
    if (!doc->Load(*stream, encoding))
    {
        wxLogError(_("Cannot load resources from file '%s'."), filename);
        return NULL;
    }

    wxXmlNode * const root = doc->GetRoot();
    if (root->GetName() != wxT("resource"))
    {
        ReportError
        (
            root,
            "invalid XRC resource, doesn't have root node <resource>"
        );
        return NULL;
    }

    long version;
    int v1, v2, v3, v4;
    wxString verstr = root->GetAttribute(wxT("version"), wxT("0.0.0.0"));
    if (wxSscanf(verstr, wxT("%i.%i.%i.%i"), &v1, &v2, &v3, &v4) == 4)
        version = v1*256*256*256+v2*256*256+v3*256+v4;
    else
        version = 0;
    if (m_version == -1)
        m_version = version;
    if (m_version != version)
    {
        wxLogWarning("Resource files must have same version number.");
    }

    ProcessPlatformProperty(root);
    PreprocessForIdRanges(root);
    wxIdRangeManager::Get()->FinaliseRanges(root);

    return doc.release();
}

wxXmlNode *wxXmlResource::DoFindResource(wxXmlNode *parent,
                                         const wxString& name,
                                         const wxString& classname,
                                         bool recursive) const
{
    wxXmlNode *node;

    // first search for match at the top-level nodes (as this is
    // where the resource is most commonly looked for):
    for (node = parent->GetChildren(); node; node = node->GetNext())
    {
        if ( IsObjectNode(node) && node->GetAttribute(wxS("name")) == name )
        {
            // empty class name matches everything
            if ( classname.empty() )
                return node;

            wxString cls(node->GetAttribute(wxS("class")));

            // object_ref may not have 'class' attribute:
            if (cls.empty() && node->GetName() == wxS("object_ref"))
            {
                wxString refName = node->GetAttribute(wxS("ref"));
                if (refName.empty())
                    continue;

                const wxXmlNode * const refNode = GetResourceNode(refName);
                if ( refNode )
                    cls = refNode->GetAttribute(wxS("class"));
            }

            if ( cls == classname )
                return node;
        }
    }

    // then recurse in child nodes
    if ( recursive )
    {
        for (node = parent->GetChildren(); node; node = node->GetNext())
        {
            if ( IsObjectNode(node) )
            {
                wxXmlNode* found = DoFindResource(node, name, classname, true);
                if ( found )
                    return found;
            }
        }
    }

    return NULL;
}

wxXmlNode *wxXmlResource::FindResource(const wxString& name,
                                       const wxString& classname,
                                       bool recursive)
{
    wxString path;
    wxXmlNode * const
        node = GetResourceNodeAndLocation(name, classname, recursive, &path);

    if ( !node )
    {
        ReportError
        (
            NULL,
            wxString::Format
            (
                "XRC resource \"%s\" (class \"%s\") not found",
                name, classname
            )
        );
    }
#if wxUSE_FILESYSTEM
    else // node was found
    {
        // ensure that relative paths work correctly when loading this node
        // (which should happen as soon as we return as FindResource() result
        // is always passed to CreateResFromNode())
        m_curFileSystem.ChangePathTo(path);
    }
#endif // wxUSE_FILESYSTEM

    return node;
}

wxXmlNode *
wxXmlResource::GetResourceNodeAndLocation(const wxString& name,
                                          const wxString& classname,
                                          bool recursive,
                                          wxString *path) const
{
    // ensure everything is up-to-date: this is needed to support on-demand
    // reloading of XRC files
    const_cast<wxXmlResource *>(this)->UpdateResources();

    for ( wxXmlResourceDataRecords::const_iterator f = Data().begin();
          f != Data().end(); ++f )
    {
        wxXmlResourceDataRecord *const rec = *f;
        wxXmlDocument * const doc = rec->Doc;
        if ( !doc || !doc->GetRoot() )
            continue;

        wxXmlNode * const
            found = DoFindResource(doc->GetRoot(), name, classname, recursive);
        if ( found )
        {
            if ( path )
                *path = rec->File;

            return found;
        }
    }

    return NULL;
}

static void MergeNodesOver(wxXmlNode& dest, wxXmlNode& overwriteWith,
                           const wxString& overwriteFilename)
{
    // Merge attributes:
    for ( wxXmlAttribute *attr = overwriteWith.GetAttributes();
          attr; attr = attr->GetNext() )
    {
        wxXmlAttribute *dattr;
        for (dattr = dest.GetAttributes(); dattr; dattr = dattr->GetNext())
        {

            if ( dattr->GetName() == attr->GetName() )
            {
                dattr->SetValue(attr->GetValue());
                break;
            }
        }

        if ( !dattr )
            dest.AddAttribute(attr->GetName(), attr->GetValue());
   }

    // Merge child nodes:
    for (wxXmlNode* node = overwriteWith.GetChildren(); node; node = node->GetNext())
    {
        wxString name = node->GetAttribute(wxT("name"), wxEmptyString);
        wxXmlNode *dnode;

        for (dnode = dest.GetChildren(); dnode; dnode = dnode->GetNext() )
        {
            if ( dnode->GetName() == node->GetName() &&
                 dnode->GetAttribute(wxT("name"), wxEmptyString) == name &&
                 dnode->GetType() == node->GetType() )
            {
                MergeNodesOver(*dnode, *node, overwriteFilename);
                break;
            }
        }

        if ( !dnode )
        {
            wxXmlNode *copyOfNode = new wxXmlNode(*node);
            // remember referenced object's file, see GetFileNameFromNode()
            copyOfNode->AddAttribute(ATTR_INPUT_FILENAME, overwriteFilename);

            static const wxChar *AT_END = wxT("end");
            wxString insert_pos = node->GetAttribute(wxT("insert_at"), AT_END);
            if ( insert_pos == AT_END )
            {
                dest.AddChild(copyOfNode);
            }
            else if ( insert_pos == wxT("begin") )
            {
                dest.InsertChild(copyOfNode, dest.GetChildren());
            }
        }
    }

    if ( dest.GetType() == wxXML_TEXT_NODE && overwriteWith.GetContent().length() )
         dest.SetContent(overwriteWith.GetContent());
}

wxObject *
wxXmlResource::DoCreateResFromNode(wxXmlNode& node,
                                   wxObject *parent,
                                   wxObject *instance,
                                   wxXmlResourceHandler *handlerToUse)
{
    // handling of referenced resource
    if ( node.GetName() == wxT("object_ref") )
    {
        wxString refName = node.GetAttribute(wxT("ref"), wxEmptyString);
        wxXmlNode* refNode = FindResource(refName, wxEmptyString, true);

        if ( !refNode )
        {
            ReportError
            (
                &node,
                wxString::Format
                (
                    "referenced object node with ref=\"%s\" not found",
                    refName
                )
            );
            return NULL;
        }

        const bool hasOnlyRefAttr = node.GetAttributes() != NULL &&
                                    node.GetAttributes()->GetNext() == NULL;

        if ( hasOnlyRefAttr && !node.GetChildren() )
        {
            // In the typical, simple case, <object_ref> is used to link
            // to another node and doesn't have any content of its own that
            // would overwrite linked object's properties. In this case,
            // we can simply create the resource from linked node.

            return DoCreateResFromNode(*refNode, parent, instance);
        }
        else
        {
            // In the more complicated (but rare) case, <object_ref> has
            // subnodes that partially overwrite content of the referenced
            // object. In this case, we need to merge both XML trees and
            // load the resource from result of the merge.

            wxXmlNode copy(*refNode);
            MergeNodesOver(copy, node, GetFileNameFromNode(&node, Data()));

            // remember referenced object's file, see GetFileNameFromNode()
            copy.AddAttribute(ATTR_INPUT_FILENAME,
                              GetFileNameFromNode(refNode, Data()));

            return DoCreateResFromNode(copy, parent, instance);
        }
    }

    if (handlerToUse)
    {
        if (handlerToUse->CanHandle(&node))
        {
            return handlerToUse->CreateResource(&node, parent, instance);
        }
    }
    else if (node.GetName() == wxT("object"))
    {
        for ( wxVector<wxXmlResourceHandler*>::iterator h = m_handlers.begin();
              h != m_handlers.end(); ++h )
        {
            wxXmlResourceHandler *handler = *h;
            if (handler->CanHandle(&node))
                return handler->CreateResource(&node, parent, instance);
        }
    }

    ReportError
    (
        &node,
        wxString::Format
        (
            "no handler found for XML node \"%s\" (class \"%s\")",
            node.GetName(),
            node.GetAttribute("class", wxEmptyString)
        )
    );
    return NULL;
}

wxIdRange::wxIdRange(const wxXmlNode* node,
                     const wxString& rname,
                     const wxString& startno,
                     const wxString& rsize)
    : m_name(rname),
      m_start(0),
      m_size(0),
      m_item_end_found(0),
      m_finalised(0)
{
    long l;
    if ( startno.ToLong(&l) )
    {
        if ( l >= 0 )
        {
            m_start = l;
        }
        else
        {
            wxXmlResource::Get()->ReportError
            (
                node,
                "a negative id-range start parameter was given"
            );
        }
    }
    else
    {
        wxXmlResource::Get()->ReportError
        (
            node,
            "the id-range start parameter was malformed"
        );
    }

    unsigned long ul;
    if ( rsize.ToULong(&ul) )
    {
        m_size = ul;
    }
    else
    {
        wxXmlResource::Get()->ReportError
        (
            node,
            "the id-range size parameter was malformed"
        );
    }
}

void wxIdRange::NoteItem(const wxXmlNode* node, const wxString& item)
{
    // Nothing gets added here, but the existence of each item is noted
    // thus getting an accurate count. 'item' will be either an integer e.g.
    // [0] [123]: will eventually create an XRCID as start+integer or [start]
    // or [end] which are synonyms for [0] or [range_size-1] respectively.
    wxString content(item.Mid(1, item.length()-2));

    // Check that basename+item wasn't foo[]
    if (content.empty())
    {
        wxXmlResource::Get()->ReportError(node, "an empty id-range item found");
        return;
    }

    if (content=="start")
    {
        // "start" means [0], so store that in the set
        if (m_indices.count(0) == 0)
        {
            m_indices.insert(0);
        }
        else
        {
            wxXmlResource::Get()->ReportError
            (
                node,
                "duplicate id-range item found"
            );
        }
    }
    else if (content=="end")
    {
        // We can't yet be certain which XRCID this will be equivalent to, so
        // just note that there's an item with this name, in case we need to
        // inc the range size
        m_item_end_found = true;
    }
    else
    {
        // Anything else will be an integer, or rubbish
        unsigned long l;
        if ( content.ToULong(&l) )
        {
            if (m_indices.count(l) == 0)
            {
                m_indices.insert(l);
                // Check that this item wouldn't fall outside the current range
                // extent
                if (l >= m_size)
                {
                    m_size = l + 1;
                }
            }
            else
            {
                wxXmlResource::Get()->ReportError
                (
                    node,
                    "duplicate id-range item found"
                );
            }

        }
        else
        {
            wxXmlResource::Get()->ReportError
            (
                node,
                "an id-range item had a malformed index"
            );
        }
    }
}

void wxIdRange::Finalise(const wxXmlNode* node)
{
    wxCHECK_RET( !IsFinalised(),
                 "Trying to finalise an already-finalised range" );

    // Now we know about all the items, we can get an accurate range size
    // Expand any requested range-size if there were more items than would fit
    m_size = wxMax(m_size, m_indices.size());

    // If an item is explicitly called foo[end], ensure it won't clash with
    // another item
    if ( m_item_end_found && m_indices.count(m_size-1) )
        ++m_size;
    if (m_size == 0)
    {
        // This will happen if someone creates a range but no items in this xrc
        // file Report the error and abort, but don't finalise, in case items
        // appear later
        wxXmlResource::Get()->ReportError
        (
            node,
            "trying to create an empty id-range"
        );
        return;
    }

    if (m_start==0)
    {
        // This is the usual case, where the user didn't specify a start ID
        // So get the range using NewControlId().
        //
        // NB: negative numbers, but NewControlId already returns the most
        //     negative
        m_start = wxWindow::NewControlId(m_size);
        wxCHECK_RET( m_start != wxID_NONE,
                     "insufficient IDs available to create range" );
        m_end = m_start + m_size - 1;
    }
    else
    {
        // The user already specified a start value, which must be positive
        m_end = m_start + m_size - 1;
    }

    // Create the XRCIDs
    for (int i=m_start; i <= m_end; ++i)
    {
        // Ensure that we overwrite any existing value as otherwise
        // wxXmlResource::Unload() followed by Load() wouldn't work correctly.
        XRCID_Assign(m_name + wxString::Format("[%i]", i-m_start), i);

        wxLogTrace("xrcrange",
                   "integer = %i %s now returns %i",
                   i,
                   m_name + wxString::Format("[%i]", i-m_start),
                   XRCID((m_name + wxString::Format("[%i]", i-m_start)).mb_str()));
    }
    // and these special ones
    XRCID_Assign(m_name + "[start]", m_start);
    XRCID_Assign(m_name + "[end]", m_end);
    wxLogTrace("xrcrange","%s[start] = %i  %s[end] = %i",
            m_name.mb_str(),XRCID(wxString(m_name+"[start]").mb_str()),
                m_name.mb_str(),XRCID(wxString(m_name+"[end]").mb_str()));

    m_finalised = true;
}

wxIdRangeManager *wxIdRangeManager::ms_instance = NULL;

/*static*/ wxIdRangeManager *wxIdRangeManager::Get()
{
    if ( !ms_instance )
        ms_instance = new wxIdRangeManager;
    return ms_instance;
}

/*static*/ wxIdRangeManager *wxIdRangeManager::Set(wxIdRangeManager *res)
{
    wxIdRangeManager *old = ms_instance;
    ms_instance = res;
    return old;
}

wxIdRangeManager::~wxIdRangeManager()
{
    for ( wxVector<wxIdRange*>::iterator i = m_IdRanges.begin();
          i != m_IdRanges.end(); ++i )
    {
        delete *i;
    }
    m_IdRanges.clear();

    delete ms_instance;
}

void wxIdRangeManager::AddRange(const wxXmlNode* node)
{
    wxString name = node->GetAttribute("name");
    wxString start = node->GetAttribute("start", "0");
    wxString size = node->GetAttribute("size", "0");
    if (name.empty())
    {
        wxXmlResource::Get()->ReportError
        (
            node,
            "xrc file contains an id-range without a name"
        );
        return;
    }

    int index = Find(name);
    if (index == wxNOT_FOUND)
    {
        wxLogTrace("xrcrange",
                   "Adding ID range, name=%s start=%s size=%s",
                   name, start, size);

        m_IdRanges.push_back(new wxIdRange(node, name, start, size));
    }
    else
    {
        // There was already a range with this name. Let's hope this is
        // from an Unload()/(re)Load(), not an unintentional duplication
        wxLogTrace("xrcrange",
                   "Replacing ID range, name=%s start=%s size=%s",
                   name, start, size);

        wxIdRange* oldrange = m_IdRanges.at(index);
        m_IdRanges.at(index) = new wxIdRange(node, name, start, size);
        delete oldrange;
    }
}

wxIdRange *
wxIdRangeManager::FindRangeForItem(const wxXmlNode* node,
                                   const wxString& item,
                                   wxString& value) const
{
    wxString basename = item.BeforeFirst('[');
    wxCHECK_MSG( !basename.empty(), NULL,
                 "an id-range item without a range name" );

    int index = Find(basename);
    if (index == wxNOT_FOUND)
    {
        // Don't assert just because we've found an unexpected foo[123]
        // Someone might just want such a name, nothing to do with ranges
        return NULL;
    }

    value = item.Mid(basename.Len());
    if (value.at(value.length()-1)==']')
    {
        return m_IdRanges.at(index);
    }
    wxXmlResource::Get()->ReportError(node, "a malformed id-range item");
    return NULL;
}

void
wxIdRangeManager::NotifyRangeOfItem(const wxXmlNode* node,
                                    const wxString& item) const
{
    wxString value;
    wxIdRange* range = FindRangeForItem(node, item, value);
    if (range)
        range->NoteItem(node, value);
}

int wxIdRangeManager::Find(const wxString& rangename) const
{
    for ( int i=0; i < (int)m_IdRanges.size(); ++i )
    {
        if (m_IdRanges.at(i)->GetName() == rangename)
            return i;
    }

    return wxNOT_FOUND;
}

void wxIdRangeManager::FinaliseRanges(const wxXmlNode* node) const
{
    for ( wxVector<wxIdRange*>::const_iterator i = m_IdRanges.begin();
          i != m_IdRanges.end(); ++i )
    {
        // Check if this range has already been finalised. Quite possible,
        // as  FinaliseRanges() gets called for each .xrc file loaded
        if (!(*i)->IsFinalised())
        {
            wxLogTrace("xrcrange", "Finalising ID range %s", (*i)->GetName());
            (*i)->Finalise(node);
        }
    }
}


class wxXmlSubclassFactories : public wxVector<wxXmlSubclassFactory*>
{
    // this is a class so that it can be forward-declared
};

wxXmlSubclassFactories *wxXmlResource::ms_subclassFactories = NULL;

/*static*/ void wxXmlResource::AddSubclassFactory(wxXmlSubclassFactory *factory)
{
    if (!ms_subclassFactories)
    {
        ms_subclassFactories = new wxXmlSubclassFactories;
    }
    ms_subclassFactories->push_back(factory);
}

class wxXmlSubclassFactoryCXX : public wxXmlSubclassFactory
{
public:
    ~wxXmlSubclassFactoryCXX() {}

    wxObject *Create(const wxString& className)
    {
        wxClassInfo* classInfo = wxClassInfo::FindClass(className);

        if (classInfo)
            return classInfo->CreateObject();
        else
            return NULL;
    }
};




wxXmlResourceHandlerImpl::wxXmlResourceHandlerImpl(wxXmlResourceHandler *handler)
                         :wxXmlResourceHandlerImplBase(handler)
{
}

wxObject *wxXmlResourceHandlerImpl::CreateResFromNode(wxXmlNode *node,
                            wxObject *parent, wxObject *instance)
{
    return m_handler->m_resource->CreateResFromNode(node, parent, instance);
}

#if wxUSE_FILESYSTEM
wxFileSystem& wxXmlResourceHandlerImpl::GetCurFileSystem()
{
    return m_handler->m_resource->GetCurFileSystem();
}
#endif


wxObject *wxXmlResourceHandlerImpl::CreateResource(wxXmlNode *node, wxObject *parent, wxObject *instance)
{
    wxXmlNode *myNode = m_handler->m_node;
    wxString myClass = m_handler->m_class;
    wxObject *myParent = m_handler->m_parent, *myInstance = m_handler->m_instance;
    wxWindow *myParentAW = m_handler->m_parentAsWindow;

    m_handler->m_instance = instance;
    if (!m_handler->m_instance && node->HasAttribute(wxT("subclass")) &&
        !(m_handler->m_resource->GetFlags() & wxXRC_NO_SUBCLASSING))
    {
        wxString subclass = node->GetAttribute(wxT("subclass"), wxEmptyString);
        if (!subclass.empty())
        {
            for (wxXmlSubclassFactories::iterator i = wxXmlResource::ms_subclassFactories->begin();
                 i != wxXmlResource::ms_subclassFactories->end(); ++i)
            {
                m_handler->m_instance = (*i)->Create(subclass);
                if (m_handler->m_instance)
                    break;
            }

            if (!m_handler->m_instance)
            {
                wxString name = node->GetAttribute(wxT("name"), wxEmptyString);
                ReportError
                (
                    node,
                    wxString::Format
                    (
                        "subclass \"%s\" not found for resource \"%s\", not subclassing",
                        subclass, name
                    )
                );
            }
        }
    }

    m_handler->m_node = node;
    m_handler->m_class = node->GetAttribute(wxT("class"), wxEmptyString);
    m_handler->m_parent = parent;
    m_handler->m_parentAsWindow = wxDynamicCast(m_handler->m_parent, wxWindow);

    wxObject *returned = GetHandler()->DoCreateResource();

    m_handler->m_node = myNode;
    m_handler->m_class = myClass;
    m_handler->m_parent = myParent; m_handler->m_parentAsWindow = myParentAW;
    m_handler->m_instance = myInstance;

    return returned;
}

bool wxXmlResourceHandlerImpl::HasParam(const wxString& param)
{
    return (GetParamNode(param) != NULL);
}


int wxXmlResourceHandlerImpl::GetStyle(const wxString& param, int defaults)
{
    wxString s = GetParamValue(param);

    if (!s) return defaults;

    wxStringTokenizer tkn(s, wxT("| \t\n"), wxTOKEN_STRTOK);
    int style = 0;
    int index;
    wxString fl;
    while (tkn.HasMoreTokens())
    {
        fl = tkn.GetNextToken();
        index = m_handler->m_styleNames.Index(fl);
        if (index != wxNOT_FOUND)
        {
            style |= m_handler->m_styleValues[index];
        }
        else
        {
            ReportParamError
            (
                param,
                wxString::Format("unknown style flag \"%s\"", fl)
            );
        }
    }
    return style;
}



wxString wxXmlResourceHandlerImpl::GetText(const wxString& param, bool translate)
{
    wxXmlNode *parNode = GetParamNode(param);
    wxString str1(GetNodeContent(parNode));
    wxString str2;

    // "\\" wasn't translated to "\" prior to 2.5.3.0:
    const bool escapeBackslash = (m_handler->m_resource->CompareVersion(2,5,3,0) >= 0);

    // VS: First version of XRC resources used $ instead of & (which is
    //     illegal in XML), but later I realized that '_' fits this purpose
    //     much better (because &File means "File with F underlined").
    const wxChar amp_char = (m_handler->m_resource->CompareVersion(2,3,0,1) < 0)
                            ? '$' : '_';

    for ( wxString::const_iterator dt = str1.begin(); dt != str1.end(); ++dt )
    {
        // Remap amp_char to &, map double amp_char to amp_char (for things
        // like "&File..." -- this is illegal in XML, so we use "_File..."):
        if ( *dt == amp_char )
        {
            if ( dt+1 == str1.end() || *(++dt) == amp_char )
                str2 << amp_char;
            else
                str2 << wxT('&') << *dt;
        }
        // Remap \n to CR, \r to LF, \t to TAB, \\ to \:
        else if ( *dt == wxT('\\') )
        {
            switch ( (*(++dt)).GetValue() )
            {
                case wxT('n'):
                    str2 << wxT('\n');
                    break;

                case wxT('t'):
                    str2 << wxT('\t');
                    break;

                case wxT('r'):
                    str2 << wxT('\r');
                    break;

                case wxT('\\') :
                    // "\\" wasn't translated to "\" prior to 2.5.3.0:
                    if ( escapeBackslash )
                    {
                        str2 << wxT('\\');
                        break;
                    }
                    // else fall-through to default: branch below

                default:
                    str2 << wxT('\\') << *dt;
                    break;
            }
        }
        else
        {
            str2 << *dt;
        }
    }

    if (m_handler->m_resource->GetFlags() & wxXRC_USE_LOCALE)
    {
        if (translate && parNode &&
            parNode->GetAttribute(wxT("translate"), wxEmptyString) != wxT("0"))
        {
            return wxGetTranslation(str2, m_handler->m_resource->GetDomain());
        }
        else
        {
#if wxUSE_UNICODE
            return str2;
#else
            // The string is internally stored as UTF-8, we have to convert
            // it into system's default encoding so that it can be displayed:
            return wxString(str2.wc_str(wxConvUTF8), wxConvLocal);
#endif
        }
    }

    // If wxXRC_USE_LOCALE is not set, then the string is already in
    // system's default encoding in ANSI build, so we don't have to
    // do anything special here.
    return str2;
}



long wxXmlResourceHandlerImpl::GetLong(const wxString& param, long defaultv)
{
    long value = defaultv;
    wxString str1 = GetParamValue(param);

    if (!str1.empty())
    {
        if (!str1.ToLong(&value))
        {
            ReportParamError
            (
                param,
                wxString::Format("invalid long specification \"%s\"", str1)
            );
        }
    }

    return value;
}

float wxXmlResourceHandlerImpl::GetFloat(const wxString& param, float defaultv)
{
    wxString str = GetParamValue(param);

    // strings in XRC always use C locale so make sure to use the
    // locale-independent wxString::ToCDouble() and not ToDouble() which uses
    // the current locale with a potentially different decimal point character
    double value = defaultv;
    if (!str.empty())
    {
        if (!str.ToCDouble(&value))
        {
            ReportParamError
            (
                param,
                wxString::Format("invalid float specification \"%s\"", str)
            );
        }
    }

    return wx_truncate_cast(float, value);
}


int wxXmlResourceHandlerImpl::GetID()
{
    return wxXmlResource::GetXRCID(GetName());
}



wxString wxXmlResourceHandlerImpl::GetName()
{
    return m_handler->m_node->GetAttribute(wxT("name"), wxT("-1"));
}



bool wxXmlResourceHandlerImpl::GetBoolAttr(const wxString& attr, bool defaultv)
{
    wxString v;
    return m_handler->m_node->GetAttribute(attr, &v) ? v == '1' : defaultv;
}

bool wxXmlResourceHandlerImpl::GetBool(const wxString& param, bool defaultv)
{
    const wxString v = GetParamValue(param);

    return v.empty() ? defaultv : (v == '1');
}


static wxColour GetSystemColour(const wxString& name)
{
    if (!name.empty())
    {
        #define SYSCLR(clr) \
            if (name == wxT(#clr)) return wxSystemSettings::GetColour(clr);
        SYSCLR(wxSYS_COLOUR_SCROLLBAR)
        SYSCLR(wxSYS_COLOUR_BACKGROUND)
        SYSCLR(wxSYS_COLOUR_DESKTOP)
        SYSCLR(wxSYS_COLOUR_ACTIVECAPTION)
        SYSCLR(wxSYS_COLOUR_INACTIVECAPTION)
        SYSCLR(wxSYS_COLOUR_MENU)
        SYSCLR(wxSYS_COLOUR_WINDOW)
        SYSCLR(wxSYS_COLOUR_WINDOWFRAME)
        SYSCLR(wxSYS_COLOUR_MENUTEXT)
        SYSCLR(wxSYS_COLOUR_WINDOWTEXT)
        SYSCLR(wxSYS_COLOUR_CAPTIONTEXT)
        SYSCLR(wxSYS_COLOUR_ACTIVEBORDER)
        SYSCLR(wxSYS_COLOUR_INACTIVEBORDER)
        SYSCLR(wxSYS_COLOUR_APPWORKSPACE)
        SYSCLR(wxSYS_COLOUR_HIGHLIGHT)
        SYSCLR(wxSYS_COLOUR_HIGHLIGHTTEXT)
        SYSCLR(wxSYS_COLOUR_BTNFACE)
        SYSCLR(wxSYS_COLOUR_3DFACE)
        SYSCLR(wxSYS_COLOUR_BTNSHADOW)
        SYSCLR(wxSYS_COLOUR_3DSHADOW)
        SYSCLR(wxSYS_COLOUR_GRAYTEXT)
        SYSCLR(wxSYS_COLOUR_BTNTEXT)
        SYSCLR(wxSYS_COLOUR_INACTIVECAPTIONTEXT)
        SYSCLR(wxSYS_COLOUR_BTNHIGHLIGHT)
        SYSCLR(wxSYS_COLOUR_BTNHILIGHT)
        SYSCLR(wxSYS_COLOUR_3DHIGHLIGHT)
        SYSCLR(wxSYS_COLOUR_3DHILIGHT)
        SYSCLR(wxSYS_COLOUR_3DDKSHADOW)
        SYSCLR(wxSYS_COLOUR_3DLIGHT)
        SYSCLR(wxSYS_COLOUR_INFOTEXT)
        SYSCLR(wxSYS_COLOUR_INFOBK)
        SYSCLR(wxSYS_COLOUR_LISTBOX)
        SYSCLR(wxSYS_COLOUR_HOTLIGHT)
        SYSCLR(wxSYS_COLOUR_GRADIENTACTIVECAPTION)
        SYSCLR(wxSYS_COLOUR_GRADIENTINACTIVECAPTION)
        SYSCLR(wxSYS_COLOUR_MENUHILIGHT)
        SYSCLR(wxSYS_COLOUR_MENUBAR)
        #undef SYSCLR
    }

    return wxNullColour;
}

wxColour wxXmlResourceHandlerImpl::GetColour(const wxString& param, const wxColour& defaultv)
{
    wxString v = GetParamValue(param);

    if ( v.empty() )
        return defaultv;

    wxColour clr;

    // wxString -> wxColour conversion
    if (!clr.Set(v))
    {
        // the colour doesn't use #RRGGBB format, check if it is symbolic
        // colour name:
        clr = GetSystemColour(v);
        if (clr.IsOk())
            return clr;

        ReportParamError
        (
            param,
            wxString::Format("incorrect colour specification \"%s\"", v)
        );
        return wxNullColour;
    }

    return clr;
}

namespace
{

// if 'param' has stock_id/stock_client, extracts them and returns true
bool GetStockArtAttrs(const wxXmlNode *paramNode,
                      const wxString& defaultArtClient,
                      wxString& art_id, wxString& art_client)
{
    if ( paramNode )
    {
        art_id = paramNode->GetAttribute("stock_id", "");

        if ( !art_id.empty() )
        {
            art_id = wxART_MAKE_ART_ID_FROM_STR(art_id);

            art_client = paramNode->GetAttribute("stock_client", "");
            if ( art_client.empty() )
                art_client = defaultArtClient;
            else
                art_client = wxART_MAKE_CLIENT_ID_FROM_STR(art_client);

            return true;
        }
    }

    return false;
}

} // anonymous namespace

wxBitmap wxXmlResourceHandlerImpl::GetBitmap(const wxString& param,
                                         const wxArtClient& defaultArtClient,
                                         wxSize size)
{
    // it used to be possible to pass an empty string here to indicate that the
    // bitmap name should be read from this node itself but this is not
    // supported any more because GetBitmap(m_node) can be used directly
    // instead
    wxASSERT_MSG( !param.empty(), "bitmap parameter name can't be empty" );

    const wxXmlNode* const node = GetParamNode(param);

    if ( !node )
    {
        // this is not an error as bitmap parameter could be optional
        return wxNullBitmap;
    }

    return GetBitmap(node, defaultArtClient, size);
}

wxBitmap wxXmlResourceHandlerImpl::GetBitmap(const wxXmlNode* node,
                                         const wxArtClient& defaultArtClient,
                                         wxSize size)
{
    wxCHECK_MSG( node, wxNullBitmap, "bitmap node can't be NULL" );

    /* If the bitmap is specified as stock item, query wxArtProvider for it: */
    wxString art_id, art_client;
    if ( GetStockArtAttrs(node, defaultArtClient,
                          art_id, art_client) )
    {
        wxBitmap stockArt(wxArtProvider::GetBitmap(art_id, art_client, size));
        if ( stockArt.IsOk() )
            return stockArt;
    }

    /* ...or load the bitmap from file: */
    wxString name = GetParamValue(node);
    if (name.empty()) return wxNullBitmap;
#if wxUSE_FILESYSTEM
    wxFSFile *fsfile = GetCurFileSystem().OpenFile(name, wxFS_READ | wxFS_SEEKABLE);
    if (fsfile == NULL)
    {
        ReportParamError
        (
            node->GetName(),
            wxString::Format("cannot open bitmap resource \"%s\"", name)
        );
        return wxNullBitmap;
    }
    wxImage img(*(fsfile->GetStream()));
    delete fsfile;
#else
    wxImage img(name);
#endif

    if (!img.IsOk())
    {
        ReportParamError
        (
            node->GetName(),
            wxString::Format("cannot create bitmap from \"%s\"", name)
        );
        return wxNullBitmap;
    }
    if (!(size == wxDefaultSize)) img.Rescale(size.x, size.y);
    return wxBitmap(img);
}


wxIcon wxXmlResourceHandlerImpl::GetIcon(const wxString& param,
                                     const wxArtClient& defaultArtClient,
                                     wxSize size)
{
    // see comment in GetBitmap(wxString) overload
    wxASSERT_MSG( !param.empty(), "icon parameter name can't be empty" );

    const wxXmlNode* const node = GetParamNode(param);

    if ( !node )
    {
        // this is not an error as icon parameter could be optional
        return wxIcon();
    }

    return GetIcon(node, defaultArtClient, size);
}

wxIcon wxXmlResourceHandlerImpl::GetIcon(const wxXmlNode* node,
                                     const wxArtClient& defaultArtClient,
                                     wxSize size)
{
    wxIcon icon;
    icon.CopyFromBitmap(GetBitmap(node, defaultArtClient, size));
    return icon;
}


wxIconBundle wxXmlResourceHandlerImpl::GetIconBundle(const wxString& param,
                                                 const wxArtClient& defaultArtClient)
{
    wxString art_id, art_client;
    if ( GetStockArtAttrs(GetParamNode(param), defaultArtClient,
                          art_id, art_client) )
    {
        wxIconBundle stockArt(wxArtProvider::GetIconBundle(art_id, art_client));
        if ( stockArt.IsOk() )
            return stockArt;
    }

    const wxString name = GetParamValue(param);
    if ( name.empty() )
        return wxNullIconBundle;

#if wxUSE_FILESYSTEM
    wxFSFile *fsfile = GetCurFileSystem().OpenFile(name, wxFS_READ | wxFS_SEEKABLE);
    if ( fsfile == NULL )
    {
        ReportParamError
        (
            param,
            wxString::Format("cannot open icon resource \"%s\"", name)
        );
        return wxNullIconBundle;
    }

    wxIconBundle bundle(*(fsfile->GetStream()));
    delete fsfile;
#else
    wxIconBundle bundle(name);
#endif

    if ( !bundle.IsOk() )
    {
        ReportParamError
        (
            param,
            wxString::Format("cannot create icon from \"%s\"", name)
        );
        return wxNullIconBundle;
    }

    return bundle;
}


wxImageList *wxXmlResourceHandlerImpl::GetImageList(const wxString& param)
{
    wxXmlNode * const imagelist_node = GetParamNode(param);
    if ( !imagelist_node )
        return NULL;

    wxXmlNode * const oldnode = m_handler->m_node;
    m_handler->m_node = imagelist_node;

    // Get the size if we have it, otherwise we will use the size of the first
    // list element.
    wxSize size = GetSize();

    // Start adding images, we'll create the image list when adding the first
    // one.
    wxImageList * imagelist = NULL;
    wxString parambitmap = wxT("bitmap");
    if ( HasParam(parambitmap) )
    {
        wxXmlNode *n = m_handler->m_node->GetChildren();
        while (n)
        {
            if (n->GetType() == wxXML_ELEMENT_NODE && n->GetName() == parambitmap)
            {
                wxIcon icon = GetIcon(n, wxART_OTHER, size);
                if ( !imagelist )
                {
                    // We need the real image list size to create it.
                    if ( size == wxDefaultSize )
                        size = icon.GetSize();

                    // We use the mask by default.
                    bool mask = GetBool(wxS("mask"), true);

                    imagelist = new wxImageList(size.x, size.y, mask);
                }

                // add icon instead of bitmap to keep the bitmap mask
                imagelist->Add(icon);
            }
            n = n->GetNext();
        }
    }

    m_handler->m_node = oldnode;
    return imagelist;
}

wxXmlNode *wxXmlResourceHandlerImpl::GetParamNode(const wxString& param)
{
    wxCHECK_MSG(m_handler->m_node, NULL, wxT("You can't access handler data before it was initialized!"));

    wxXmlNode *n = m_handler->m_node->GetChildren();

    while (n)
    {
        if (n->GetType() == wxXML_ELEMENT_NODE && n->GetName() == param)
        {
            // TODO: check that there are no other properties/parameters with
            //       the same name and log an error if there are (can't do this
            //       right now as I'm not sure if it's not going to break code
            //       using this function in unintentional way (i.e. for
            //       accessing other things than properties), for example
            //       wxBitmapComboBoxXmlHandler almost surely does
            return n;
        }
        n = n->GetNext();
    }
    return NULL;
}

bool wxXmlResourceHandlerImpl::IsOfClass(wxXmlNode *node, const wxString& classname) const
{
    return node->GetAttribute(wxT("class")) == classname;
}



wxString wxXmlResourceHandlerImpl::GetNodeContent(const wxXmlNode *node)
{
    const wxXmlNode *n = node;
    if (n == NULL) return wxEmptyString;
    n = n->GetChildren();

    while (n)
    {
        if (n->GetType() == wxXML_TEXT_NODE ||
            n->GetType() == wxXML_CDATA_SECTION_NODE)
            return n->GetContent();
        n = n->GetNext();
    }
    return wxEmptyString;
}



wxString wxXmlResourceHandlerImpl::GetParamValue(const wxString& param)
{
    if (param.empty())
        return GetNodeContent(m_handler->m_node);
    else
        return GetNodeContent(GetParamNode(param));
}

wxString wxXmlResourceHandlerImpl::GetParamValue(const wxXmlNode* node)
{
    return GetNodeContent(node);
}


wxSize wxXmlResourceHandlerImpl::GetSize(const wxString& param,
                                     wxWindow *windowToUse)
{
    wxString s = GetParamValue(param);
    if (s.empty()) s = wxT("-1,-1");
    bool is_dlg;
    long sx, sy = 0;

    is_dlg = s[s.length()-1] == wxT('d');
    if (is_dlg) s.RemoveLast();

    if (!s.BeforeFirst(wxT(',')).ToLong(&sx) ||
        !s.AfterLast(wxT(',')).ToLong(&sy))
    {
        ReportParamError
        (
            param,
            wxString::Format("cannot parse coordinates value \"%s\"", s)
        );
        return wxDefaultSize;
    }

    if (is_dlg)
    {
        if (windowToUse)
        {
            return wxDLG_UNIT(windowToUse, wxSize(sx, sy));
        }
        else if (m_handler->m_parentAsWindow)
        {
            return wxDLG_UNIT(m_handler->m_parentAsWindow, wxSize(sx, sy));
        }
        else
        {
            ReportParamError
            (
                param,
                "cannot convert dialog units: dialog unknown"
            );
            return wxDefaultSize;
        }
    }

    return wxSize(sx, sy);
}



wxPoint wxXmlResourceHandlerImpl::GetPosition(const wxString& param)
{
    wxSize sz = GetSize(param);
    return wxPoint(sz.x, sz.y);
}



wxCoord wxXmlResourceHandlerImpl::GetDimension(const wxString& param,
                                           wxCoord defaultv,
                                           wxWindow *windowToUse)
{
    wxString s = GetParamValue(param);
    if (s.empty()) return defaultv;
    bool is_dlg;
    long sx;

    is_dlg = s[s.length()-1] == wxT('d');
    if (is_dlg) s.RemoveLast();

    if (!s.ToLong(&sx))
    {
        ReportParamError
        (
            param,
            wxString::Format("cannot parse dimension value \"%s\"", s)
        );
        return defaultv;
    }

    if (is_dlg)
    {
        if (windowToUse)
        {
            return wxDLG_UNIT(windowToUse, wxSize(sx, 0)).x;
        }
        else if (m_handler->m_parentAsWindow)
        {
            return wxDLG_UNIT(m_handler->m_parentAsWindow, wxSize(sx, 0)).x;
        }
        else
        {
            ReportParamError
            (
                param,
                "cannot convert dialog units: dialog unknown"
            );
            return defaultv;
        }
    }

    return sx;
}

wxDirection
wxXmlResourceHandlerImpl::GetDirection(const wxString& param, wxDirection dirDefault)
{
    wxDirection dir;

    const wxString dirstr = GetParamValue(param);
    if ( dirstr.empty() )
        dir = dirDefault;
    else if ( dirstr == "wxLEFT" )
        dir = wxLEFT;
    else if ( dirstr == "wxRIGHT" )
        dir = wxRIGHT;
    else if ( dirstr == "wxTOP" )
        dir = wxTOP;
    else if ( dirstr == "wxBOTTOM" )
        dir = wxBOTTOM;
    else
    {
        ReportError
        (
            GetParamNode(param),
            wxString::Format
            (
                "Invalid direction \"%s\": must be one of "
                "wxLEFT|wxRIGHT|wxTOP|wxBOTTOM.",
                dirstr
            )
        );

        dir = dirDefault;
    }

    return dir;
}

// Get system font index using indexname
static wxFont GetSystemFont(const wxString& name)
{
    if (!name.empty())
    {
        #define SYSFNT(fnt) \
            if (name == wxT(#fnt)) return wxSystemSettings::GetFont(fnt);
        SYSFNT(wxSYS_OEM_FIXED_FONT)
        SYSFNT(wxSYS_ANSI_FIXED_FONT)
        SYSFNT(wxSYS_ANSI_VAR_FONT)
        SYSFNT(wxSYS_SYSTEM_FONT)
        SYSFNT(wxSYS_DEVICE_DEFAULT_FONT)
        SYSFNT(wxSYS_SYSTEM_FIXED_FONT)
        SYSFNT(wxSYS_DEFAULT_GUI_FONT)
        #undef SYSFNT
    }

    return wxNullFont;
}

wxFont wxXmlResourceHandlerImpl::GetFont(const wxString& param, wxWindow* parent)
{
    wxXmlNode *font_node = GetParamNode(param);
    if (font_node == NULL)
    {
        ReportError(
            wxString::Format("cannot find font node \"%s\"", param));
        return wxNullFont;
    }

    wxXmlNode *oldnode = m_handler->m_node;
    m_handler->m_node = font_node;

    // font attributes:

    // size
    int isize = -1;
    bool hasSize = HasParam(wxT("size"));
    if (hasSize)
        isize = GetLong(wxT("size"), -1);

    // style
    int istyle = wxNORMAL;
    bool hasStyle = HasParam(wxT("style"));
    if (hasStyle)
    {
        wxString style = GetParamValue(wxT("style"));
        if (style == wxT("italic"))
            istyle = wxITALIC;
        else if (style == wxT("slant"))
            istyle = wxSLANT;
        else if (style != wxT("normal"))
        {
            ReportParamError
            (
                param,
                wxString::Format("unknown font style \"%s\"", style)
            );
        }
    }

    // weight
    int iweight = wxNORMAL;
    bool hasWeight = HasParam(wxT("weight"));
    if (hasWeight)
    {
        wxString weight = GetParamValue(wxT("weight"));
        if (weight == wxT("bold"))
            iweight = wxBOLD;
        else if (weight == wxT("light"))
            iweight = wxLIGHT;
        else if (weight != wxT("normal"))
        {
            ReportParamError
            (
                param,
                wxString::Format("unknown font weight \"%s\"", weight)
            );
        }
    }

    // underline
    bool hasUnderlined = HasParam(wxT("underlined"));
    bool underlined = hasUnderlined ? GetBool(wxT("underlined"), false) : false;

    // family and facename
    int ifamily = wxDEFAULT;
    bool hasFamily = HasParam(wxT("family"));
    if (hasFamily)
    {
        wxString family = GetParamValue(wxT("family"));
             if (family == wxT("decorative")) ifamily = wxDECORATIVE;
        else if (family == wxT("roman")) ifamily = wxROMAN;
        else if (family == wxT("script")) ifamily = wxSCRIPT;
        else if (family == wxT("swiss")) ifamily = wxSWISS;
        else if (family == wxT("modern")) ifamily = wxMODERN;
        else if (family == wxT("teletype")) ifamily = wxTELETYPE;
        else
        {
            ReportParamError
            (
                param,
                wxString::Format("unknown font family \"%s\"", family)
            );
        }
    }


    wxString facename;
    bool hasFacename = HasParam(wxT("face"));
    if (hasFacename)
    {
        wxString faces = GetParamValue(wxT("face"));
        wxStringTokenizer tk(faces, wxT(","));
#if wxUSE_FONTENUM
        wxArrayString facenames(wxFontEnumerator::GetFacenames());
        while (tk.HasMoreTokens())
        {
            int index = facenames.Index(tk.GetNextToken(), false);
            if (index != wxNOT_FOUND)
            {
                facename = facenames[index];
                break;
            }
        }
#else // !wxUSE_FONTENUM
        // just use the first face name if we can't check its availability:
        if (tk.HasMoreTokens())
            facename = tk.GetNextToken();
#endif // wxUSE_FONTENUM/!wxUSE_FONTENUM
    }

    // encoding
    wxFontEncoding enc = wxFONTENCODING_DEFAULT;
    bool hasEncoding = HasParam(wxT("encoding"));
#if wxUSE_FONTMAP
    if (hasEncoding)
    {
        wxString encoding = GetParamValue(wxT("encoding"));
        wxFontMapper mapper;
        if (!encoding.empty())
            enc = mapper.CharsetToEncoding(encoding);
        if (enc == wxFONTENCODING_SYSTEM)
            enc = wxFONTENCODING_DEFAULT;
    }
#endif // wxUSE_FONTMAP

    wxFont font;

    // is this font based on a system font?
    if (HasParam(wxT("sysfont")))
    {
        font = GetSystemFont(GetParamValue(wxT("sysfont")));
        if (HasParam(wxT("inherit")))
        {
            ReportParamError
            (
                param,
                "double specification of \"sysfont\" and \"inherit\""
            );
        }
    }
    // or should the font of the widget be used?
    else if (GetBool(wxT("inherit"), false))
    {
        if (parent)
            font = parent->GetFont();
        else
        {
            ReportParamError
            (
                param,
                "no parent window specified to derive the font from"
            );
        }
    }

    if (font.IsOk())
    {
        if (hasSize && isize != -1)
        {
            font.SetPointSize(isize);
            if (HasParam(wxT("relativesize")))
            {
                ReportParamError
                (
                    param,
                    "double specification of \"size\" and \"relativesize\""
                );
            }
        }
        else if (HasParam(wxT("relativesize")))
            font.SetPointSize(int(font.GetPointSize() *
                                     GetFloat(wxT("relativesize"))));

        if (hasStyle)
            font.SetStyle(istyle);
        if (hasWeight)
            font.SetWeight(iweight);
        if (hasUnderlined)
            font.SetUnderlined(underlined);
        if (hasFamily)
            font.SetFamily(ifamily);
        if (hasFacename)
            font.SetFaceName(facename);
        if (hasEncoding)
            font.SetDefaultEncoding(enc);
    }
    else // not based on system font
    {
        font = wxFont(isize == -1 ? wxNORMAL_FONT->GetPointSize() : isize,
                      ifamily, istyle, iweight,
                      underlined, facename, enc);
    }

    m_handler->m_node = oldnode;
    return font;
}


void wxXmlResourceHandlerImpl::SetupWindow(wxWindow *wnd)
{
    //FIXME : add cursor

    if (HasParam(wxT("exstyle")))
        // Have to OR it with existing style, since
        // some implementations (e.g. wxGTK) use the extra style
        // during creation
        wnd->SetExtraStyle(wnd->GetExtraStyle() | GetStyle(wxT("exstyle")));
    if (HasParam(wxT("bg")))
        wnd->SetBackgroundColour(GetColour(wxT("bg")));
    if (HasParam(wxT("ownbg")))
        wnd->SetOwnBackgroundColour(GetColour(wxT("ownbg")));
    if (HasParam(wxT("fg")))
        wnd->SetForegroundColour(GetColour(wxT("fg")));
    if (HasParam(wxT("ownfg")))
        wnd->SetOwnForegroundColour(GetColour(wxT("ownfg")));
    if (GetBool(wxT("enabled"), 1) == 0)
        wnd->Enable(false);
    if (GetBool(wxT("focused"), 0) == 1)
        wnd->SetFocus();
    if (GetBool(wxT("hidden"), 0) == 1)
        wnd->Show(false);
#if wxUSE_TOOLTIPS
    if (HasParam(wxT("tooltip")))
        wnd->SetToolTip(GetText(wxT("tooltip")));
#endif
    if (HasParam(wxT("font")))
        wnd->SetFont(GetFont(wxT("font"), wnd));
    if (HasParam(wxT("ownfont")))
        wnd->SetOwnFont(GetFont(wxT("ownfont"), wnd));
    if (HasParam(wxT("help")))
        wnd->SetHelpText(GetText(wxT("help")));
}


void wxXmlResourceHandlerImpl::CreateChildren(wxObject *parent, bool this_hnd_only)
{
    for ( wxXmlNode *n = m_handler->m_node->GetChildren(); n; n = n->GetNext() )
    {
        if ( IsObjectNode(n) )
        {
            m_handler->m_resource->DoCreateResFromNode(*n, parent, NULL,
                                            this_hnd_only ? this->GetHandler() : NULL);
       }
    }
}


void wxXmlResourceHandlerImpl::CreateChildrenPrivately(wxObject *parent, wxXmlNode *rootnode)
{
    wxXmlNode *root;
    if (rootnode == NULL) root = m_handler->m_node; else root = rootnode;
    wxXmlNode *n = root->GetChildren();

    while (n)
    {
        if (n->GetType() == wxXML_ELEMENT_NODE && GetHandler()->CanHandle(n))
        {
            CreateResource(n, parent, NULL);
        }
        n = n->GetNext();
    }
}


//-----------------------------------------------------------------------------
// errors reporting
//-----------------------------------------------------------------------------

void wxXmlResourceHandlerImpl::ReportError(const wxString& message)
{
    m_handler->m_resource->ReportError(m_handler->m_node, message);
}

void wxXmlResourceHandlerImpl::ReportError(wxXmlNode *context,
                                       const wxString& message)
{
    m_handler->m_resource->ReportError(context ? context : m_handler->m_node, message);
}

void wxXmlResourceHandlerImpl::ReportParamError(const wxString& param,
                                            const wxString& message)
{
    m_handler->m_resource->ReportError(GetParamNode(param), message);
}

void wxXmlResource::ReportError(const wxXmlNode *context, const wxString& message)
{
    if ( !context )
    {
        DoReportError("", NULL, message);
        return;
    }

    // We need to find out the file that 'context' is part of. Performance of
    // this code is not critical, so we simply find the root XML node and
    // compare it with all loaded XRC files.
    const wxString filename = GetFileNameFromNode(context, Data());

    DoReportError(filename, context, message);
}

void wxXmlResource::DoReportError(const wxString& xrcFile, const wxXmlNode *position,
                                  const wxString& message)
{
    const int line = position ? position->GetLineNumber() : -1;

    wxString loc;
    if ( !xrcFile.empty() )
        loc = xrcFile + ':';
    if ( line != -1 )
        loc += wxString::Format("%d:", line);
    if ( !loc.empty() )
        loc += ' ';

    wxLogError("XRC error: %s%s", loc, message);
}


//-----------------------------------------------------------------------------
// XRCID implementation
//-----------------------------------------------------------------------------

#define XRCID_TABLE_SIZE     1024


struct XRCID_record
{
    /* Hold the id so that once an id is allocated for a name, it
       does not get created again by NewControlId at least
       until we are done with it */
    wxWindowIDRef id;
    char *key;
    XRCID_record *next;
};

static XRCID_record *XRCID_Records[XRCID_TABLE_SIZE] = {NULL};

// Extremely simplistic hash function which probably ought to be replaced with
// wxStringHash::stringHash().
static inline unsigned XRCIdHash(const char *str_id)
{
    unsigned index = 0;

    for (const char *c = str_id; *c != '\0'; c++) index += (unsigned int)*c;
    index %= XRCID_TABLE_SIZE;

    return index;
}

static void XRCID_Assign(const wxString& str_id, int value)
{
    const wxCharBuffer buf_id(str_id.mb_str());
    const unsigned index = XRCIdHash(buf_id);


    XRCID_record *oldrec = NULL;
    for (XRCID_record *rec = XRCID_Records[index]; rec; rec = rec->next)
    {
        if (wxStrcmp(rec->key, buf_id) == 0)
        {
            rec->id = value;
            return;
        }
        oldrec = rec;
    }

    XRCID_record **rec_var = (oldrec == NULL) ?
                              &XRCID_Records[index] : &oldrec->next;
    *rec_var = new XRCID_record;
    (*rec_var)->key = wxStrdup(str_id);
    (*rec_var)->id = value;
    (*rec_var)->next = NULL;
}

static int XRCID_Lookup(const char *str_id, int value_if_not_found = wxID_NONE)
{
    const unsigned index = XRCIdHash(str_id);


    XRCID_record *oldrec = NULL;
    for (XRCID_record *rec = XRCID_Records[index]; rec; rec = rec->next)
    {
        if (wxStrcmp(rec->key, str_id) == 0)
        {
            return rec->id;
        }
        oldrec = rec;
    }

    XRCID_record **rec_var = (oldrec == NULL) ?
                              &XRCID_Records[index] : &oldrec->next;
    *rec_var = new XRCID_record;
    (*rec_var)->key = wxStrdup(str_id);
    (*rec_var)->next = NULL;

    char *end;
    if (value_if_not_found != wxID_NONE)
        (*rec_var)->id = value_if_not_found;
    else
    {
        int asint = wxStrtol(str_id, &end, 10);
        if (*str_id && *end == 0)
        {
            // if str_id was integer, keep it verbosely:
            (*rec_var)->id = asint;
        }
        else
        {
            (*rec_var)->id = wxWindowBase::NewControlId();
        }
    }

    return (*rec_var)->id;
}

namespace
{

// flag indicating whether standard XRC ids were already initialized
static bool gs_stdIDsAdded = false;

void AddStdXRCID_Records()
{
#define stdID(id) XRCID_Lookup(#id, id)
    stdID(-1);

    stdID(wxID_ANY);
    stdID(wxID_SEPARATOR);

    stdID(wxID_OPEN);
    stdID(wxID_CLOSE);
    stdID(wxID_NEW);
    stdID(wxID_SAVE);
    stdID(wxID_SAVEAS);
    stdID(wxID_REVERT);
    stdID(wxID_EXIT);
    stdID(wxID_UNDO);
    stdID(wxID_REDO);
    stdID(wxID_HELP);
    stdID(wxID_PRINT);
    stdID(wxID_PRINT_SETUP);
    stdID(wxID_PAGE_SETUP);
    stdID(wxID_PREVIEW);
    stdID(wxID_ABOUT);
    stdID(wxID_HELP_CONTENTS);
    stdID(wxID_HELP_INDEX),
    stdID(wxID_HELP_SEARCH),
    stdID(wxID_HELP_COMMANDS);
    stdID(wxID_HELP_PROCEDURES);
    stdID(wxID_HELP_CONTEXT);
    stdID(wxID_CLOSE_ALL);
    stdID(wxID_PREFERENCES);

    stdID(wxID_EDIT);
    stdID(wxID_CUT);
    stdID(wxID_COPY);
    stdID(wxID_PASTE);
    stdID(wxID_CLEAR);
    stdID(wxID_FIND);
    stdID(wxID_DUPLICATE);
    stdID(wxID_SELECTALL);
    stdID(wxID_DELETE);
    stdID(wxID_REPLACE);
    stdID(wxID_REPLACE_ALL);
    stdID(wxID_PROPERTIES);

    stdID(wxID_VIEW_DETAILS);
    stdID(wxID_VIEW_LARGEICONS);
    stdID(wxID_VIEW_SMALLICONS);
    stdID(wxID_VIEW_LIST);
    stdID(wxID_VIEW_SORTDATE);
    stdID(wxID_VIEW_SORTNAME);
    stdID(wxID_VIEW_SORTSIZE);
    stdID(wxID_VIEW_SORTTYPE);


    stdID(wxID_FILE1);
    stdID(wxID_FILE2);
    stdID(wxID_FILE3);
    stdID(wxID_FILE4);
    stdID(wxID_FILE5);
    stdID(wxID_FILE6);
    stdID(wxID_FILE7);
    stdID(wxID_FILE8);
    stdID(wxID_FILE9);


    stdID(wxID_OK);
    stdID(wxID_CANCEL);
    stdID(wxID_APPLY);
    stdID(wxID_YES);
    stdID(wxID_NO);
    stdID(wxID_STATIC);
    stdID(wxID_FORWARD);
    stdID(wxID_BACKWARD);
    stdID(wxID_DEFAULT);
    stdID(wxID_MORE);
    stdID(wxID_SETUP);
    stdID(wxID_RESET);
    stdID(wxID_CONTEXT_HELP);
    stdID(wxID_YESTOALL);
    stdID(wxID_NOTOALL);
    stdID(wxID_ABORT);
    stdID(wxID_RETRY);
    stdID(wxID_IGNORE);
    stdID(wxID_ADD);
    stdID(wxID_REMOVE);

    stdID(wxID_UP);
    stdID(wxID_DOWN);
    stdID(wxID_HOME);
    stdID(wxID_REFRESH);
    stdID(wxID_STOP);
    stdID(wxID_INDEX);

    stdID(wxID_BOLD);
    stdID(wxID_ITALIC);
    stdID(wxID_JUSTIFY_CENTER);
    stdID(wxID_JUSTIFY_FILL);
    stdID(wxID_JUSTIFY_RIGHT);
    stdID(wxID_JUSTIFY_LEFT);
    stdID(wxID_UNDERLINE);
    stdID(wxID_INDENT);
    stdID(wxID_UNINDENT);
    stdID(wxID_ZOOM_100);
    stdID(wxID_ZOOM_FIT);
    stdID(wxID_ZOOM_IN);
    stdID(wxID_ZOOM_OUT);
    stdID(wxID_UNDELETE);
    stdID(wxID_REVERT_TO_SAVED);
    stdID(wxID_CDROM);
    stdID(wxID_CONVERT);
    stdID(wxID_EXECUTE);
    stdID(wxID_FLOPPY);
    stdID(wxID_HARDDISK);
    stdID(wxID_BOTTOM);
    stdID(wxID_FIRST);
    stdID(wxID_LAST);
    stdID(wxID_TOP);
    stdID(wxID_INFO);
    stdID(wxID_JUMP_TO);
    stdID(wxID_NETWORK);
    stdID(wxID_SELECT_COLOR);
    stdID(wxID_SELECT_FONT);
    stdID(wxID_SORT_ASCENDING);
    stdID(wxID_SORT_DESCENDING);
    stdID(wxID_SPELL_CHECK);
    stdID(wxID_STRIKETHROUGH);


    stdID(wxID_SYSTEM_MENU);
    stdID(wxID_CLOSE_FRAME);
    stdID(wxID_MOVE_FRAME);
    stdID(wxID_RESIZE_FRAME);
    stdID(wxID_MAXIMIZE_FRAME);
    stdID(wxID_ICONIZE_FRAME);
    stdID(wxID_RESTORE_FRAME);



    stdID(wxID_MDI_WINDOW_CASCADE);
    stdID(wxID_MDI_WINDOW_TILE_HORZ);
    stdID(wxID_MDI_WINDOW_TILE_VERT);
    stdID(wxID_MDI_WINDOW_ARRANGE_ICONS);
    stdID(wxID_MDI_WINDOW_PREV);
    stdID(wxID_MDI_WINDOW_NEXT);
#undef stdID
}

} // anonymous namespace


/*static*/
int wxXmlResource::DoGetXRCID(const char *str_id, int value_if_not_found)
{
    if ( !gs_stdIDsAdded )
    {
        gs_stdIDsAdded = true;
        AddStdXRCID_Records();
    }

    return XRCID_Lookup(str_id, value_if_not_found);
}

/* static */
wxString wxXmlResource::FindXRCIDById(int numId)
{
    for ( int i = 0; i < XRCID_TABLE_SIZE; i++ )
    {
        for ( XRCID_record *rec = XRCID_Records[i]; rec; rec = rec->next )
        {
            if ( rec->id == numId )
                return wxString(rec->key);
        }
    }

    return wxString();
}

static void CleanXRCID_Record(XRCID_record *rec)
{
    if (rec)
    {
        CleanXRCID_Record(rec->next);

        free(rec->key);
        delete rec;
    }
}

static void CleanXRCID_Records()
{
    for (int i = 0; i < XRCID_TABLE_SIZE; i++)
    {
        CleanXRCID_Record(XRCID_Records[i]);
        XRCID_Records[i] = NULL;
    }

    gs_stdIDsAdded = false;
}


//-----------------------------------------------------------------------------
// module and globals
//-----------------------------------------------------------------------------

// normally we would do the cleanup from wxXmlResourceModule::OnExit() but it
// can happen that some XRC records have been created because of the use of
// XRCID() in event tables, which happens during static objects initialization,
// but then the application initialization failed and so the wx modules were
// neither initialized nor cleaned up -- this static object does the cleanup in
// this case
static struct wxXRCStaticCleanup
{
    ~wxXRCStaticCleanup() { CleanXRCID_Records(); }
} s_staticCleanup;

class wxXmlResourceModule: public wxModule
{
DECLARE_DYNAMIC_CLASS(wxXmlResourceModule)
public:
    wxXmlResourceModule() {}
    bool OnInit()
    {
        wxXmlResource::AddSubclassFactory(new wxXmlSubclassFactoryCXX);
        return true;
    }
    void OnExit()
    {
        delete wxXmlResource::Set(NULL);
        delete wxIdRangeManager::Set(NULL);
        if(wxXmlResource::ms_subclassFactories)
        {
            for ( wxXmlSubclassFactories::iterator i = wxXmlResource::ms_subclassFactories->begin();
                  i != wxXmlResource::ms_subclassFactories->end(); ++i )
            {
                delete *i;
            }
            wxDELETE(wxXmlResource::ms_subclassFactories);
        }
        CleanXRCID_Records();
    }
};

IMPLEMENT_DYNAMIC_CLASS(wxXmlResourceModule, wxModule)


// When wxXml is loaded dynamically after the application is already running
// then the built-in module system won't pick this one up.  Add it manually.
void wxXmlInitResourceModule()
{
    wxModule* module = new wxXmlResourceModule;
    wxModule::RegisterModule(module);
    wxModule::InitializeModules();
}

#endif // wxUSE_XRC
