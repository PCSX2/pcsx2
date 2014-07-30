/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/notebook.h
// Purpose:     MSW/GTK compatible notebook (a.k.a. property sheet)
// Author:      Stefan Csomor
// Modified by:
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTEBOOK_H_
#define _WX_NOTEBOOK_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#include "wx/event.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// fwd declarations
class WXDLLIMPEXP_FWD_CORE wxImageList;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// wxNotebook
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNotebook : public wxNotebookBase
{
public:
  // ctors
  // -----
    // default for dynamic class
  wxNotebook() { }
    // the same arguments as for wxControl (@@@ any special styles?)
  wxNotebook(wxWindow *parent,
             wxWindowID id,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxString& name = wxNotebookNameStr)
    { Create( parent, id, pos, size, style, name ); }
    // Create() function
  bool Create(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = wxNotebookNameStr);
    // dtor
  virtual ~wxNotebook();

  // accessors
  // ---------
    // set the currently selected page, return the index of the previously
    // selected one (or wxNOT_FOUND on error)
    // NB: this function will _not_ generate wxEVT_NOTEBOOK_PAGE_xxx events
  int SetSelection(size_t nPage) { return DoSetSelection(nPage, SetSelection_SendEvent); }

    // changes selected page without sending events
  int ChangeSelection(size_t nPage) { return DoSetSelection(nPage); }

    // set/get the title of a page
  bool SetPageText(size_t nPage, const wxString& strText);
  wxString GetPageText(size_t nPage) const;

    // sets/returns item's image index in the current image list
  int  GetPageImage(size_t nPage) const;
  bool SetPageImage(size_t nPage, int nImage);

  // control the appearance of the notebook pages
    // set the size (the same for all pages)
  virtual void SetPageSize(const wxSize& size);
    // set the padding between tabs (in pixels)
  virtual void SetPadding(const wxSize& padding);
    // sets the size of the tabs (assumes all tabs are the same size)
  virtual void SetTabSize(const wxSize& sz);

  // hit test
  virtual int HitTest(const wxPoint& pt, long *flags = NULL) const;

  // calculate size for wxNotebookSizer
  wxSize CalcSizeFromPage(const wxSize& sizePage) const;
  wxRect GetPageRect() const ;

  // operations
  // ----------
    // remove all pages
  bool DeleteAllPages();
    // the same as AddPage(), but adds it at the specified position
  bool InsertPage(size_t nPage,
                  wxNotebookPage *pPage,
                  const wxString& strText,
                  bool bSelect = false,
                  int imageId = NO_IMAGE);

  // callbacks
  // ---------
  void OnSize(wxSizeEvent& event);
  void OnSelChange(wxBookCtrlEvent& event);
  void OnSetFocus(wxFocusEvent& event);
  void OnNavigationKey(wxNavigationKeyEvent& event);

    // implementation
    // --------------

#if wxUSE_CONSTRAINTS
  virtual void SetConstraintSizes(bool recurse = true);
  virtual bool DoPhase(int nPhase);

#endif

  // base class virtuals
  // -------------------
  virtual void Command(wxCommandEvent& event);
    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

protected:
  virtual wxNotebookPage *DoRemovePage(size_t page) ;
  // common part of all ctors
  void Init();

  // helper functions
  void ChangePage(int nOldSel, int nSel); // change pages
  void MacSetupTabs();

  int DoSetSelection(size_t nPage, int flags = 0);

  // the icon indices
  wxArrayInt m_images;

  DECLARE_DYNAMIC_CLASS(wxNotebook)
  DECLARE_EVENT_TABLE()
};


#endif // _WX_NOTEBOOK_H_
