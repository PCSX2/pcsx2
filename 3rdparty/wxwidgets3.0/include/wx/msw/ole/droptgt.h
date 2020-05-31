///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/droptgt.h
// Purpose:     declaration of the wxDropTarget class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.03.98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_OLEDROPTGT_H
#define   _WX_OLEDROPTGT_H

#if wxUSE_DRAG_AND_DROP

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class  wxIDropTarget;
struct wxIDropTargetHelper;
struct IDataObject;

// ----------------------------------------------------------------------------
// An instance of the class wxDropTarget may be associated with any wxWindow
// derived object via SetDropTarget() function. If this is done, the virtual
// methods of wxDropTarget are called when something is dropped on the window.
//
// Note that wxDropTarget is an abstract base class (ABC) and you should derive
// your own class from it implementing pure virtual function in order to use it
// (all of them, including protected ones which are called by the class itself)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDropTarget : public wxDropTargetBase
{
public:
    // ctor & dtor
    wxDropTarget(wxDataObject *dataObject = NULL);
    virtual ~wxDropTarget();

    // normally called by wxWindow on window creation/destruction, but might be
    // called `manually' as well. Register() returns true on success.
    bool Register(WXHWND hwnd);
    void Revoke(WXHWND hwnd);

    // provide default implementation for base class pure virtuals
    virtual bool OnDrop(wxCoord x, wxCoord y);
    virtual bool GetData();

    // Can only be called during OnXXX methods.
    wxDataFormat GetMatchingPair();

    // implementation only from now on
    // -------------------------------

    // do we accept this kind of data?
    bool MSWIsAcceptedData(IDataObject *pIDataSource) const;

    // give us the data source from IDropTarget::Drop() - this is later used by
    // GetData() when it's called from inside OnData()
    void MSWSetDataSource(IDataObject *pIDataSource);

    // These functions take care of all things necessary to support native drag
    // images.
    //
    // {Init,End}DragImageSupport() are called during Register/Revoke,
    // UpdateDragImageOnXXX() functions are called on the corresponding drop
    // target events.
    void MSWInitDragImageSupport();
    void MSWEndDragImageSupport();
    void MSWUpdateDragImageOnData(wxCoord x, wxCoord y, wxDragResult res);
    void MSWUpdateDragImageOnDragOver(wxCoord x, wxCoord y, wxDragResult res);
    void MSWUpdateDragImageOnEnter(wxCoord x, wxCoord y, wxDragResult res);
    void MSWUpdateDragImageOnLeave();

private:
    // helper used by IsAcceptedData() and GetData()
    wxDataFormat MSWGetSupportedFormat(IDataObject *pIDataSource) const;

    wxIDropTarget     *m_pIDropTarget; // the pointer to our COM interface
    IDataObject       *m_pIDataSource; // the pointer to the source data object
    wxIDropTargetHelper *m_dropTargetHelper; // the drop target helper

    wxDECLARE_NO_COPY_CLASS(wxDropTarget);
};

#endif  //wxUSE_DRAG_AND_DROP

#endif  //_WX_OLEDROPTGT_H
