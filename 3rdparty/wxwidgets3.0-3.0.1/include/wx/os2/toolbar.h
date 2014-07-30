/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/toolbar.h
// Purpose:     wxToolBar class
// Author:      David Webster
// Modified by:
// Created:     10/17/98
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBAR_H_
#define _WX_TOOLBAR_H_

#if wxUSE_TOOLBAR
#include "wx/timer.h"
#include "wx/tbarbase.h"

#define ID_TOOLTIMER                100
#define ID_TOOLEXPTIMER             101

class WXDLLIMPEXP_CORE wxToolBar: public wxToolBarBase
{
public:
    /*
     * Public interface
     */

    wxToolBar()
    : m_vToolTimer(this, ID_TOOLTIMER)
    , m_vToolExpTimer(this, ID_TOOLEXPTIMER)
    { Init(); }

    inline wxToolBar( wxWindow*       pParent
                     ,wxWindowID      vId
                     ,const wxPoint&  rPos = wxDefaultPosition
                     ,const wxSize&   rSize = wxDefaultSize
                     ,long            lStyle = wxTB_HORIZONTAL
                     ,const wxString& rName = wxToolBarNameStr
                    ) : m_vToolTimer(this, ID_TOOLTIMER)
                      , m_vToolExpTimer(this, ID_TOOLEXPTIMER)
    {
        Init();
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,lStyle
               ,rName
              );
    }
    virtual ~wxToolBar();

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = wxTB_HORIZONTAL
                ,const wxString& rName = wxToolBarNameStr
               );


    //
    // Override/implement base class virtuals
    //
    virtual wxToolBarToolBase* FindToolForPosition( wxCoord vX
                                                   ,wxCoord vY
                                                  ) const;
    virtual bool               Realize(void);
    virtual void               SetRows(int nRows);

    //
    // Special overrides for OS/2
    //
    virtual wxToolBarToolBase* InsertControl( size_t     nPos
                                             ,wxControl* pControl
                                            );
    virtual wxToolBarToolBase* InsertSeparator(size_t nPos);
    virtual wxToolBarToolBase* InsertTool( size_t          nPos
                                          ,int             nId
                                          ,const wxString& rsLabel
                                          ,const wxBitmap& rBitmap
                                          ,const wxBitmap& rBmpDisabled = wxNullBitmap
                                          ,wxItemKind      eKind = wxITEM_NORMAL
                                          ,const wxString& rsShortHelp = wxEmptyString
                                          ,const wxString& rsLongHelp = wxEmptyString
                                          ,wxObject*       pClientData = NULL
                                         );
    wxToolBarToolBase*         InsertTool( size_t          nPos
                                          ,int             nId
                                          ,const wxBitmap& rBitmap
                                          ,const wxBitmap& rBmpDisabled = wxNullBitmap
                                          ,bool            bToggle = FALSE
                                          ,wxObject*       pClientData = NULL
                                          ,const wxString& rsShortHelp = wxEmptyString
                                          ,const wxString& rsLongHelp = wxEmptyString
                                         )
    {
        return InsertTool( nPos
                          ,nId
                          ,wxEmptyString
                          ,rBitmap
                          ,rBmpDisabled
                          ,bToggle ? wxITEM_CHECK : wxITEM_NORMAL
                          ,rsShortHelp
                          ,rsLongHelp
                          ,pClientData
                         );
    }
    virtual bool               DeleteTool(int nId);
    virtual bool               DeleteToolByPos(size_t nPos);

    //
    // Event handlers
    //
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    void OnKillFocus(wxFocusEvent& event);

protected:
    //
    // Common part of all ctors
    //
    void Init();

    //
    // Implement base class pure virtuals
    //
    virtual wxToolBarToolBase* DoAddTool( int id
                                         ,const wxString& label
                                         ,const wxBitmap& bitmap
                                         ,const wxBitmap& bmpDisabled
                                         ,wxItemKind kind
                                         ,const wxString& shortHelp = wxEmptyString
                                         ,const wxString& longHelp = wxEmptyString
                                         ,wxObject *clientData = NULL
                                         ,wxCoord xPos = -1
                                         ,wxCoord yPos = -1
                                        );

    virtual bool DoInsertTool( size_t             nPos
                              ,wxToolBarToolBase* pTool
                             );
    virtual bool DoDeleteTool( size_t              nPos
                              , wxToolBarToolBase* pTool
                             );

    virtual void DoEnableTool( wxToolBarToolBase* pTool
                              ,bool               bEnable
                             );
    virtual void DoToggleTool( wxToolBarToolBase* pTool
                              ,bool               bToggle
                             );
    virtual void DoSetToggle( wxToolBarToolBase* pTool
                             ,bool               bToggle
                            );

    virtual wxToolBarToolBase* CreateTool( int             vId
                                          ,const wxString& rsLabel
                                          ,const wxBitmap& rBmpNormal
                                          ,const wxBitmap& rBmpDisabled
                                          ,wxItemKind      eKind
                                          ,wxObject*       pClientData
                                          ,const wxString& rsShortHelp
                                          ,const wxString& rsLongHelp
                                         );
    virtual wxToolBarToolBase* CreateTool(wxControl* pControl,
                                          const wxString& label);

    //
    // Helpers
    //
    void         DrawTool(wxToolBarToolBase *tool);
    virtual void DrawTool( wxDC&              rDC
                          ,wxToolBarToolBase* pTool
                         );
    virtual void SpringUpButton(int nIndex);

    int                             m_nCurrentRowsOrColumns;
    int                             m_nPressedTool;
    int                             m_nCurrentTool;
    wxCoord                         m_vLastX;
    wxCoord                         m_vLastY;
    wxCoord                         m_vMaxWidth;
    wxCoord                         m_vMaxHeight;
    wxCoord                         m_vXPos;
    wxCoord                         m_vYPos;
    wxCoord                         m_vTextX;
    wxCoord                         m_vTextY;

private:
    void LowerTool( wxToolBarToolBase* pTool
                   ,bool               bLower = TRUE
                  );
    void RaiseTool( wxToolBarToolBase* pTool
                   ,bool               bRaise = TRUE
                  );
    void OnTimer(wxTimerEvent& rEvent);

    static bool                     m_bInitialized;

    wxTimer                         m_vToolTimer;
    wxTimer                         m_vToolExpTimer;
    wxToolTip*                      m_pToolTip;
    wxCoord                         m_vXMouse;
    wxCoord                         m_vYMouse;

    //
    // Virtual function hiding supression
    virtual wxToolBarToolBase *InsertTool (size_t nPos, wxToolBarToolBase* pTool)
    {
        return( wxToolBarBase::InsertTool( nPos
                                          ,pTool
                                         ));
    }

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxToolBar)
};

#endif // wxUSE_TOOLBAR

#endif
    // _WX_TOOLBAR_H_
