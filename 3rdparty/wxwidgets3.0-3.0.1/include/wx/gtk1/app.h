/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/app.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKAPPH__
#define __GTKAPPH__

#include "wx/frame.h"
#include "wx/icon.h"
#include "wx/strconv.h"

typedef struct _GdkVisual GdkVisual;

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxApp;
class WXDLLIMPEXP_FWD_BASE wxLog;

//-----------------------------------------------------------------------------
// wxApp
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxApp: public wxAppBase
{
public:
    wxApp();
    virtual ~wxApp();

    /* override for altering the way wxGTK intializes the GUI
     * (palette/visual/colorcube). under wxMSW, OnInitGui() does nothing by
     * default. when overriding this method, the code in it is likely to be
     * platform dependent, otherwise use OnInit(). */
    virtual bool OnInitGui();

    // override base class (pure) virtuals
    virtual void WakeUpIdle();

    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();

    static bool InitialzeVisual();

    virtual void OnAssertFailure(const wxChar *file,
                                 int line,
                                 const wxChar *func,
                                 const wxChar *cond,
                                 const wxChar *msg);

    bool IsInAssert() const { return m_isInAssert; }

    int             m_idleTag;
    void RemoveIdleTag();

    unsigned char  *m_colorCube;

    // Used by the wxGLApp and wxGLCanvas class for GL-based X visual
    // selection.
    void           *m_glVisualInfo; // this is actually an XVisualInfo*
    void           *m_glFBCInfo; // this is actually an GLXFBConfig*
    // This returns the current visual: either that used by wxRootWindow
    // or the XVisualInfo* for SGI.
    GdkVisual      *GetGdkVisual();

private:
    // true if we're inside an assert modal dialog
    bool m_isInAssert;

    DECLARE_DYNAMIC_CLASS(wxApp)
};

#endif // __GTKAPPH__
