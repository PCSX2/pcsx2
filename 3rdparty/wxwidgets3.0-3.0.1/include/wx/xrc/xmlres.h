/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xmlres.h
// Purpose:     XML resources
// Author:      Vaclav Slavik
// Created:     2000/03/05
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XMLRES_H_
#define _WX_XMLRES_H_

#include "wx/defs.h"

#if wxUSE_XRC

#include "wx/string.h"
#include "wx/dynarray.h"
#include "wx/arrstr.h"
#include "wx/datetime.h"
#include "wx/list.h"
#include "wx/gdicmn.h"
#include "wx/filesys.h"
#include "wx/bitmap.h"
#include "wx/icon.h"
#include "wx/artprov.h"
#include "wx/colour.h"
#include "wx/vector.h"

#include "wx/xrc/xmlreshandler.h"

class WXDLLIMPEXP_FWD_BASE wxFileName;

class WXDLLIMPEXP_FWD_CORE wxIconBundle;
class WXDLLIMPEXP_FWD_CORE wxImageList;
class WXDLLIMPEXP_FWD_CORE wxMenu;
class WXDLLIMPEXP_FWD_CORE wxMenuBar;
class WXDLLIMPEXP_FWD_CORE wxDialog;
class WXDLLIMPEXP_FWD_CORE wxPanel;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxToolBar;

class WXDLLIMPEXP_FWD_XML wxXmlDocument;
class WXDLLIMPEXP_FWD_XML wxXmlNode;
class WXDLLIMPEXP_FWD_XRC wxXmlSubclassFactory;
class wxXmlSubclassFactories;
class wxXmlResourceModule;
class wxXmlResourceDataRecords;

// These macros indicate current version of XML resources (this information is
// encoded in root node of XRC file as "version" property).
//
// Rules for increasing version number:
//   - change it only if you made incompatible change to the format. Addition
//     of new attribute to control handler is _not_ incompatible change, because
//     older versions of the library may ignore it.
//   - if you change version number, follow these steps:
//       - set major, minor and release numbers to respective version numbers of
//         the wxWidgets library (see wx/version.h)
//       - reset revision to 0 unless the first three are same as before,
//         in which case you should increase revision by one
#define WX_XMLRES_CURRENT_VERSION_MAJOR            2
#define WX_XMLRES_CURRENT_VERSION_MINOR            5
#define WX_XMLRES_CURRENT_VERSION_RELEASE          3
#define WX_XMLRES_CURRENT_VERSION_REVISION         0
#define WX_XMLRES_CURRENT_VERSION_STRING       wxT("2.5.3.0")

#define WX_XMLRES_CURRENT_VERSION \
                (WX_XMLRES_CURRENT_VERSION_MAJOR * 256*256*256 + \
                 WX_XMLRES_CURRENT_VERSION_MINOR * 256*256 + \
                 WX_XMLRES_CURRENT_VERSION_RELEASE * 256 + \
                 WX_XMLRES_CURRENT_VERSION_REVISION)

enum wxXmlResourceFlags
{
    wxXRC_USE_LOCALE     = 1,
    wxXRC_NO_SUBCLASSING = 2,
    wxXRC_NO_RELOADING   = 4
};

// This class holds XML resources from one or more .xml files
// (or derived forms, either binary or zipped -- see manual for
// details).
class WXDLLIMPEXP_XRC wxXmlResource : public wxObject
{
public:
    // Constructor.
    // Flags: wxXRC_USE_LOCALE
    //              translatable strings will be translated via _()
    //              using the given domain if specified
    //        wxXRC_NO_SUBCLASSING
    //              subclass property of object nodes will be ignored
    //              (useful for previews in XRC editors)
    //        wxXRC_NO_RELOADING
    //              don't check the modification time of the XRC files and
    //              reload them if they have changed on disk
    wxXmlResource(int flags = wxXRC_USE_LOCALE,
                  const wxString& domain = wxEmptyString);

    // Constructor.
    // Flags: wxXRC_USE_LOCALE
    //              translatable strings will be translated via _()
    //              using the given domain if specified
    //        wxXRC_NO_SUBCLASSING
    //              subclass property of object nodes will be ignored
    //              (useful for previews in XRC editors)
    wxXmlResource(const wxString& filemask, int flags = wxXRC_USE_LOCALE,
                  const wxString& domain = wxEmptyString);

    // Destructor.
    virtual ~wxXmlResource();

    // Loads resources from XML files that match given filemask.
    // This method understands wxFileSystem URLs if wxUSE_FILESYS.
    bool Load(const wxString& filemask);

    // Loads resources from single XRC file.
    bool LoadFile(const wxFileName& file);

    // Loads all XRC files from a directory.
    bool LoadAllFiles(const wxString& dirname);

    // Unload resource from the given XML file (wildcards not allowed)
    bool Unload(const wxString& filename);

    // Initialize handlers for all supported controls/windows. This will
    // make the executable quite big because it forces linking against
    // most of the wxWidgets library.
    void InitAllHandlers();

    // Initialize only a specific handler (or custom handler). Convention says
    // that handler name is equal to the control's name plus 'XmlHandler', for
    // example wxTextCtrlXmlHandler, wxHtmlWindowXmlHandler. The XML resource
    // compiler (xmlres) can create include file that contains initialization
    // code for all controls used within the resource.
    void AddHandler(wxXmlResourceHandler *handler);

    // Add a new handler at the beginning of the handler list
    void InsertHandler(wxXmlResourceHandler *handler);

    // Removes all handlers
    void ClearHandlers();

    // Registers subclasses factory for use in XRC. This function is not meant
    // for public use, please see the comment above wxXmlSubclassFactory
    // definition.
    static void AddSubclassFactory(wxXmlSubclassFactory *factory);

    // Loads menu from resource. Returns NULL on failure.
    wxMenu *LoadMenu(const wxString& name);

    // Loads menubar from resource. Returns NULL on failure.
    wxMenuBar *LoadMenuBar(wxWindow *parent, const wxString& name);

    // Loads menubar from resource. Returns NULL on failure.
    wxMenuBar *LoadMenuBar(const wxString& name) { return LoadMenuBar(NULL, name); }

#if wxUSE_TOOLBAR
    // Loads a toolbar.
    wxToolBar *LoadToolBar(wxWindow *parent, const wxString& name);
#endif

    // Loads a dialog. dlg points to parent window (if any).
    wxDialog *LoadDialog(wxWindow *parent, const wxString& name);

    // Loads a dialog. dlg points to parent window (if any). This form
    // is used to finish creation of already existing instance (main reason
    // for this is that you may want to use derived class with new event table)
    // Example (typical usage):
    //      MyDialog dlg;
    //      wxTheXmlResource->LoadDialog(&dlg, mainFrame, "my_dialog");
    //      dlg->ShowModal();
    bool LoadDialog(wxDialog *dlg, wxWindow *parent, const wxString& name);

    // Loads a panel. panel points to parent window (if any).
    wxPanel *LoadPanel(wxWindow *parent, const wxString& name);

    // Loads a panel. panel points to parent window (if any). This form
    // is used to finish creation of already existing instance.
    bool LoadPanel(wxPanel *panel, wxWindow *parent, const wxString& name);

    // Loads a frame.
    wxFrame *LoadFrame(wxWindow* parent, const wxString& name);
    bool LoadFrame(wxFrame* frame, wxWindow *parent, const wxString& name);

    // Load an object from the resource specifying both the resource name and
    // the classname.  This lets you load nonstandard container windows.
    wxObject *LoadObject(wxWindow *parent, const wxString& name,
                         const wxString& classname)
    {
        return DoLoadObject(parent, name, classname, false /* !recursive */);
    }

    // Load an object from the resource specifying both the resource name and
    // the classname.  This form lets you finish the creation of an existing
    // instance.
    bool LoadObject(wxObject *instance,
                    wxWindow *parent,
                    const wxString& name,
                    const wxString& classname)
    {
        return DoLoadObject(instance, parent, name, classname, false);
    }

    // These versions of LoadObject() look for the object with the given name
    // recursively (breadth first) and can be used to instantiate an individual
    // control defined anywhere in an XRC file. No check is done that the name
    // is unique, it's up to the caller to ensure this.
    wxObject *LoadObjectRecursively(wxWindow *parent,
                                    const wxString& name,
                                    const wxString& classname)
    {
        return DoLoadObject(parent, name, classname, true /* recursive */);
    }

    bool LoadObjectRecursively(wxObject *instance,
                               wxWindow *parent,
                               const wxString& name,
                               const wxString& classname)
    {
        return DoLoadObject(instance, parent, name, classname, true);
    }

    // Loads a bitmap resource from a file.
    wxBitmap LoadBitmap(const wxString& name);

    // Loads an icon resource from a file.
    wxIcon LoadIcon(const wxString& name);

    // Attaches an unknown control to the given panel/window/dialog.
    // Unknown controls are used in conjunction with <object class="unknown">.
    bool AttachUnknownControl(const wxString& name, wxWindow *control,
                              wxWindow *parent = NULL);

    // Returns a numeric ID that is equivalent to the string ID used in an XML
    // resource. If an unknown str_id is requested (i.e. other than wxID_XXX
    // or integer), a new record is created which associates the given string
    // with a number. If value_if_not_found == wxID_NONE, the number is obtained via
    // wxWindow::NewControlId(). Otherwise value_if_not_found is used.
    // Macro XRCID(name) is provided for convenient use in event tables.
    static int GetXRCID(const wxString& str_id, int value_if_not_found = wxID_NONE)
        { return DoGetXRCID(str_id.mb_str(), value_if_not_found); }

    // version for internal use only
    static int DoGetXRCID(const char *str_id, int value_if_not_found = wxID_NONE);


    // Find the string ID with the given numeric value, returns an empty string
    // if no such ID is found.
    //
    // Notice that unlike GetXRCID(), which is fast, this operation is slow as
    // it checks all the IDs used in XRC.
    static wxString FindXRCIDById(int numId);


    // Returns version information (a.b.c.d = d+ 256*c + 256^2*b + 256^3*a).
    long GetVersion() const { return m_version; }

    // Compares resources version to argument. Returns -1 if resources version
    // is less than the argument, +1 if greater and 0 if they equal.
    int CompareVersion(int major, int minor, int release, int revision) const
    {
        long diff = GetVersion() -
                    (major*256*256*256 + minor*256*256 + release*256 + revision);
        if ( diff < 0 )
            return -1;
        else if ( diff > 0 )
            return +1;
        else
            return 0;
    }

    //// Singleton accessors.

    // Gets the global resources object or creates one if none exists.
    static wxXmlResource *Get();

    // Sets the global resources object and returns a pointer to the previous one (may be NULL).
    static wxXmlResource *Set(wxXmlResource *res);

    // Returns flags, which may be a bitlist of wxXRC_USE_LOCALE and wxXRC_NO_SUBCLASSING.
    int GetFlags() const { return m_flags; }
    // Set flags after construction.
    void SetFlags(int flags) { m_flags = flags; }

    // Get/Set the domain to be passed to the translation functions, defaults
    // to empty string (no domain).
    const wxString& GetDomain() const { return m_domain; }
    void SetDomain(const wxString& domain);


    // This function returns the wxXmlNode containing the definition of the
    // object with the given name or NULL.
    //
    // It can be used to access additional information defined in the XRC file
    // and not used by wxXmlResource itself.
    const wxXmlNode *GetResourceNode(const wxString& name) const
        { return GetResourceNodeAndLocation(name, wxString(), true); }

protected:
    // reports input error at position 'context'
    void ReportError(const wxXmlNode *context, const wxString& message);

    // override this in derived class to customize errors reporting
    virtual void DoReportError(const wxString& xrcFile, const wxXmlNode *position,
                               const wxString& message);

    // Load the contents of a single file and returns its contents as a new
    // wxXmlDocument (which will be owned by caller) on success or NULL.
    wxXmlDocument *DoLoadFile(const wxString& file);

    // Scans the resources list for unloaded files and loads them. Also reloads
    // files that have been modified since last loading.
    bool UpdateResources();


    // Common implementation of GetResourceNode() and FindResource(): searches
    // all top-level or all (if recursive == true) nodes if all loaded XRC
    // files and returns the node, if found, as well as the path of the file it
    // was found in if path is non-NULL
    wxXmlNode *GetResourceNodeAndLocation(const wxString& name,
                                          const wxString& classname,
                                          bool recursive = false,
                                          wxString *path = NULL) const;


    // Note that these functions are used outside of wxWidgets itself, e.g.
    // there are several known cases of inheriting from wxXmlResource just to
    // be able to call FindResource() so we keep them for compatibility even if
    // their names are not really consistent with GetResourceNode() public
    // function and FindResource() is also non-const because it changes the
    // current path of m_curFileSystem to ensure that relative paths work
    // correctly when CreateResFromNode() is called immediately afterwards
    // (something const public function intentionally does not do)

    // Returns the node containing the resource with the given name and class
    // name unless it's empty (then any class matches) or NULL if not found.
    wxXmlNode *FindResource(const wxString& name, const wxString& classname,
                            bool recursive = false);

    // Helper function used by FindResource() to look under the given node.
    wxXmlNode *DoFindResource(wxXmlNode *parent, const wxString& name,
                              const wxString& classname, bool recursive) const;

    // Creates a resource from information in the given node
    // (Uses only 'handlerToUse' if != NULL)
    wxObject *CreateResFromNode(wxXmlNode *node, wxObject *parent,
                                wxObject *instance = NULL,
                                wxXmlResourceHandler *handlerToUse = NULL)
    {
        return node ? DoCreateResFromNode(*node, parent, instance, handlerToUse)
                    : NULL;
    }

    // Helper of Load() and Unload(): returns the URL corresponding to the
    // given file if it's indeed a file, otherwise returns the original string
    // unmodified
    static wxString ConvertFileNameToURL(const wxString& filename);

    // loading resources from archives is impossible without wxFileSystem
#if wxUSE_FILESYSTEM
    // Another helper: detect if the filename is a ZIP or XRS file
    static bool IsArchive(const wxString& filename);
#endif // wxUSE_FILESYSTEM

private:
    wxXmlResourceDataRecords& Data() { return *m_data; }
    const wxXmlResourceDataRecords& Data() const { return *m_data; }

    // the real implementation of CreateResFromNode(): this should be only
    // called if node is non-NULL
    wxObject *DoCreateResFromNode(wxXmlNode& node,
                                  wxObject *parent,
                                  wxObject *instance,
                                  wxXmlResourceHandler *handlerToUse = NULL);

    // common part of LoadObject() and LoadObjectRecursively()
    wxObject *DoLoadObject(wxWindow *parent,
                           const wxString& name,
                           const wxString& classname,
                           bool recursive);
    bool DoLoadObject(wxObject *instance,
                      wxWindow *parent,
                      const wxString& name,
                      const wxString& classname,
                      bool recursive);

private:
    long m_version;

    int m_flags;
    wxVector<wxXmlResourceHandler*> m_handlers;
    wxXmlResourceDataRecords *m_data;
#if wxUSE_FILESYSTEM
    wxFileSystem m_curFileSystem;
    wxFileSystem& GetCurFileSystem() { return m_curFileSystem; }
#endif

    // domain to pass to translation functions, if any.
    wxString m_domain;

    friend class wxXmlResourceHandlerImpl;
    friend class wxXmlResourceModule;
    friend class wxIdRangeManager;
    friend class wxIdRange;

    static wxXmlSubclassFactories *ms_subclassFactories;

    // singleton instance:
    static wxXmlResource *ms_instance;
};


// This macro translates string identifier (as used in XML resource,
// e.g. <menuitem id="my_menu">...</menuitem>) to integer id that is needed by
// wxWidgets event tables.
// Example:
//    BEGIN_EVENT_TABLE(MyFrame, wxFrame)
//       EVT_MENU(XRCID("quit"), MyFrame::OnQuit)
//       EVT_MENU(XRCID("about"), MyFrame::OnAbout)
//       EVT_MENU(XRCID("new"), MyFrame::OnNew)
//       EVT_MENU(XRCID("open"), MyFrame::OnOpen)
//    END_EVENT_TABLE()

#define XRCID(str_id) \
    wxXmlResource::DoGetXRCID(str_id)


// This macro returns pointer to particular control in dialog
// created using XML resources. You can use it to set/get values from
// controls.
// Example:
//    wxDialog dlg;
//    wxXmlResource::Get()->LoadDialog(&dlg, mainFrame, "my_dialog");
//    XRCCTRL(dlg, "my_textctrl", wxTextCtrl)->SetValue(wxT("default value"));

#define XRCCTRL(window, id, type) \
    (wxStaticCast((window).FindWindow(XRCID(id)), type))

// This macro returns pointer to sizer item
// Example:
//
// <object class="spacer" name="area">
//   <size>400, 300</size>
// </object>
//
// wxSizerItem* item = XRCSIZERITEM(*this, "area")

#define XRCSIZERITEM(window, id) \
    ((window).GetSizer() ? (window).GetSizer()->GetItemById(XRCID(id)) : NULL)


// wxXmlResourceHandlerImpl is the back-end of the wxXmlResourceHander class to
// really implementing all its functionality. It is defined in the "xrc"
// library unlike wxXmlResourceHandler itself which is defined in "core" to
// allow inheriting from it in the code from the other libraries too.

class WXDLLIMPEXP_XRC wxXmlResourceHandlerImpl : public wxXmlResourceHandlerImplBase
{
public:
    // Constructor.
    wxXmlResourceHandlerImpl(wxXmlResourceHandler *handler);

    // Destructor.
    virtual ~wxXmlResourceHandlerImpl() {}

    // Creates an object (menu, dialog, control, ...) from an XML node.
    // Should check for validity.
    // parent is a higher-level object (usually window, dialog or panel)
    // that is often necessary to create the resource.
    // If instance is non-NULL it should not create a new instance via 'new' but
    // should rather use this one, and call its Create method.
    wxObject *CreateResource(wxXmlNode *node, wxObject *parent,
                             wxObject *instance);


    // --- Handy methods:

    // Returns true if the node has a property class equal to classname,
    // e.g. <object class="wxDialog">.
    bool IsOfClass(wxXmlNode *node, const wxString& classname) const;

    // Gets node content from wxXML_ENTITY_NODE
    // The problem is, <tag>content<tag> is represented as
    // wxXML_ENTITY_NODE name="tag", content=""
    //    |-- wxXML_TEXT_NODE or
    //        wxXML_CDATA_SECTION_NODE name="" content="content"
    wxString GetNodeContent(const wxXmlNode *node);

    // Check to see if a parameter exists.
    bool HasParam(const wxString& param);

    // Finds the node or returns NULL.
    wxXmlNode *GetParamNode(const wxString& param);

    // Finds the parameter value or returns the empty string.
    wxString GetParamValue(const wxString& param);

    // Returns the parameter value from given node.
    wxString GetParamValue(const wxXmlNode* node);

    // Gets style flags from text in form "flag | flag2| flag3 |..."
    // Only understands flags added with AddStyle
    int GetStyle(const wxString& param = wxT("style"), int defaults = 0);

    // Gets text from param and does some conversions:
    // - replaces \n, \r, \t by respective chars (according to C syntax)
    // - replaces _ by & and __ by _ (needed for _File => &File because of XML)
    // - calls wxGetTranslations (unless disabled in wxXmlResource)
    wxString GetText(const wxString& param, bool translate = true);

    // Returns the XRCID.
    int GetID();

    // Returns the resource name.
    wxString GetName();

    // Gets a bool flag (1, t, yes, on, true are true, everything else is false).
    bool GetBool(const wxString& param, bool defaultv = false);

    // Gets an integer value from the parameter.
    long GetLong(const wxString& param, long defaultv = 0);

    // Gets a float value from the parameter.
    float GetFloat(const wxString& param, float defaultv = 0);

    // Gets colour in HTML syntax (#RRGGBB).
    wxColour GetColour(const wxString& param, const wxColour& defaultv = wxNullColour);

    // Gets the size (may be in dialog units).
    wxSize GetSize(const wxString& param = wxT("size"),
                   wxWindow *windowToUse = NULL);

    // Gets the position (may be in dialog units).
    wxPoint GetPosition(const wxString& param = wxT("pos"));

    // Gets a dimension (may be in dialog units).
    wxCoord GetDimension(const wxString& param, wxCoord defaultv = 0,
                         wxWindow *windowToUse = NULL);

    // Gets a direction, complains if the value is invalid.
    wxDirection GetDirection(const wxString& param, wxDirection dirDefault = wxLEFT);

    // Gets a bitmap.
    wxBitmap GetBitmap(const wxString& param = wxT("bitmap"),
                       const wxArtClient& defaultArtClient = wxART_OTHER,
                       wxSize size = wxDefaultSize);

    // Gets a bitmap from an XmlNode.
    wxBitmap GetBitmap(const wxXmlNode* node,
                       const wxArtClient& defaultArtClient = wxART_OTHER,
                       wxSize size = wxDefaultSize);

    // Gets an icon.
    wxIcon GetIcon(const wxString& param = wxT("icon"),
                   const wxArtClient& defaultArtClient = wxART_OTHER,
                   wxSize size = wxDefaultSize);

    // Gets an icon from an XmlNode.
    wxIcon GetIcon(const wxXmlNode* node,
                   const wxArtClient& defaultArtClient = wxART_OTHER,
                   wxSize size = wxDefaultSize);

    // Gets an icon bundle.
    wxIconBundle GetIconBundle(const wxString& param,
                               const wxArtClient& defaultArtClient = wxART_OTHER);

    // Gets an image list.
    wxImageList *GetImageList(const wxString& param = wxT("imagelist"));

#if wxUSE_ANIMATIONCTRL
    // Gets an animation.
    wxAnimation* GetAnimation(const wxString& param = wxT("animation"));
#endif

    // Gets a font.
    wxFont GetFont(const wxString& param = wxT("font"), wxWindow* parent = NULL);

    // Gets the value of a boolean attribute (only "0" and "1" are valid values)
    bool GetBoolAttr(const wxString& attr, bool defaultv);


    // Sets common window options.
    void SetupWindow(wxWindow *wnd);

    // Creates children.
    void CreateChildren(wxObject *parent, bool this_hnd_only = false);

    // Helper function.
    void CreateChildrenPrivately(wxObject *parent, wxXmlNode *rootnode = NULL);

    // Creates a resource from a node.
    wxObject *CreateResFromNode(wxXmlNode *node,
                                wxObject *parent, wxObject *instance = NULL);

    // helper
#if wxUSE_FILESYSTEM
    wxFileSystem& GetCurFileSystem();
#endif

    // reports input error at position 'context'
    void ReportError(wxXmlNode *context, const wxString& message);
    // reports input error at m_node
    void ReportError(const wxString& message);
    // reports input error when parsing parameter with given name
    void ReportParamError(const wxString& param, const wxString& message);
};


// Programmer-friendly macros for writing XRC handlers:

#define XRC_MAKE_INSTANCE(variable, classname) \
   classname *variable = NULL; \
   if (m_instance) \
       variable = wxStaticCast(m_instance, classname); \
   if (!variable) \
       variable = new classname;


// FIXME -- remove this $%^#$%#$@# as soon as Ron checks his changes in!!
WXDLLIMPEXP_XRC void wxXmlInitResourceModule();


// This class is used to create instances of XRC "object" nodes with "subclass"
// property. It is _not_ supposed to be used by XRC users, you should instead
// register your subclasses via wxWidgets' RTTI mechanism. This class is useful
// only for language bindings developer who need a way to implement subclassing
// in wxWidgets ports that don't support wxRTTI (e.g. wxPython).
class WXDLLIMPEXP_XRC wxXmlSubclassFactory
{
public:
    // Try to create instance of given class and return it, return NULL on
    // failure:
    virtual wxObject *Create(const wxString& className) = 0;
    virtual ~wxXmlSubclassFactory() {}
};


/* -------------------------------------------------------------------------
   Backward compatibility macros. Do *NOT* use, they may disappear in future
   versions of the XRC library!
   ------------------------------------------------------------------------- */

#endif // wxUSE_XRC

#endif // _WX_XMLRES_H_
