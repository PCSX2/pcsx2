///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dnd.h
// Purpose:     declaration of the wxDropTarget class
// Author:      David Webster
// Modified by:
// Created:     10/21/99
// Copyright:   (c) 1999 David Webster
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#ifndef __OS2DNDH__
#define __OS2DNDH__

#if !wxUSE_DRAG_AND_DROP
    #error  "You should #define wxUSE_DRAG_AND_DROP to 1 to compile this file!"
#endif  //WX_DRAG_DROP

#define INCL_WINSTDDRAG
#include <os2.h>
#ifndef __EMX__
#include <pmstddlg.h>
#endif

class CIDropTarget;

//-------------------------------------------------------------------------
// wxDropSource
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDropSource: public wxDropSourceBase
{
public:
    /* constructor. set data later with SetData() */
    wxDropSource(wxWindow* pWin);

    /* constructor for setting one data object */
    wxDropSource( wxDataObject& rData,
                  wxWindow*     pWin
                );
    virtual ~wxDropSource();

    /* start drag action */
    virtual wxDragResult DoDragDrop(int flags = wxDrag_CopyOnly);
    virtual bool         GiveFeedback(wxDragResult eEffect);

protected:
    void Init(void);

    ULONG                           m_ulItems;
    PDRAGINFO                       m_pDragInfo;
    DRAGIMAGE                       m_vDragImage;
    PDRAGITEM                       m_pDragItem;
    wxWindow*                       m_pWindow;
}; // end of CLASS wxDropSource

//-------------------------------------------------------------------------
// wxDropTarget
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDropTarget : public wxDropTargetBase
{
public:
    wxDropTarget(wxDataObject* pDataObject = NULL);
    virtual ~wxDropTarget();

    //
    // These functions are called when data is moved over position (x, y) and
    // may return either wxDragCopy, wxDragMove or wxDragNone depending on
    // what would happen if the data were dropped here.
    //
    // The last parameter is what would happen by default and is determined by
    // the platform-specific logic (for example, under Windows it's wxDragCopy
    // if Ctrl key is pressed and wxDragMove otherwise) except that it will
    // always be wxDragNone if the carried data is in an unsupported format.
    //
    // OnData must be implemented and other should be overridden by derived classes
    //
    virtual wxDragResult OnData( wxCoord      vX
                                ,wxCoord      vY
                                ,wxDragResult eResult
                               );
    virtual bool         OnDrop( wxCoord vX
                                ,wxCoord vY
                               );
            bool         IsAcceptedData(PDRAGINFO pDataSource) const;

protected:
    virtual bool         GetData(void);
            wxDataFormat GetSupportedFormat(PDRAGINFO pDataSource) const;
            void         Release(void);

private:
    CIDropTarget*                   m_pDropTarget;
}; // end of CLASS wxDropTarget

#endif //__OS2DNDH__

