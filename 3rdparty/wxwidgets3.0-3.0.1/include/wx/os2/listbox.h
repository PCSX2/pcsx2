/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/listbox.h
// Purpose:     wxListBox class
// Author:      David Webster
// Modified by:
// Created:     10/09/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LISTBOX_H_
#define _WX_LISTBOX_H_

// ----------------------------------------------------------------------------
// simple types
// ----------------------------------------------------------------------------

#if wxUSE_OWNER_DRAWN
    class WXDLLIMPEXP_FWD_CORE wxOwnerDrawn;

    // define the array of list box items
    #include  "wx/dynarray.h"

    WX_DEFINE_EXPORTED_ARRAY_PTR(wxOwnerDrawn *, wxListBoxItemsArray);
#endif // wxUSE_OWNER_DRAWN

// forward decl for GetSelections()
class wxArrayInt;

// ----------------------------------------------------------------------------
// List box control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox();
    wxListBox( wxWindow*          pParent
              ,wxWindowID         vId
              ,const wxPoint&     rPos = wxDefaultPosition
              ,const wxSize&      rSize = wxDefaultSize
              ,int                n = 0
              ,const wxString     asChoices[] = NULL
              ,long               lStyle = 0
              ,const wxValidator& rValidator = wxDefaultValidator
              ,const wxString&    rsName = wxListBoxNameStr)
    {
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,n
               ,asChoices
               ,lStyle
               ,rValidator
               ,rsName
              );
    }
    wxListBox( wxWindow*            pParent
              ,wxWindowID           vId
              ,const wxPoint&       rPos
              ,const wxSize&        rSize
              ,const wxArrayString& asChoices
              ,long                 lStyle = 0
              ,const wxValidator&   rValidator = wxDefaultValidator
              ,const wxString&      rsName = wxListBoxNameStr)
    {
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,asChoices
               ,lStyle
               ,rValidator
               ,rsName
              );
    }

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,int                n = 0
                ,const wxString     asChoices[] = NULL
                ,long               lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxListBoxNameStr
               );
    bool Create( wxWindow*            pParent
                ,wxWindowID           vId
                ,const wxPoint&       rPos
                ,const wxSize&        rSize
                ,const wxArrayString& asChoices
                ,long                 lStyle = 0
                ,const wxValidator&   rValidator = wxDefaultValidator
                ,const wxString&      rsName = wxListBoxNameStr
               );

    virtual ~wxListBox();

    //
    // Implement base class pure virtuals
    //
    virtual void          DoClear(void);
    virtual void          DoDeleteOneItem(unsigned int n);

    virtual unsigned int  GetCount() const;
    virtual wxString      GetString(unsigned int n) const;
    virtual void          SetString(unsigned int n, const wxString& rsString);

    virtual bool          IsSelected(int n) const;
    virtual void          DoSetSelection(int  n, bool bSelect);
    virtual int           GetSelection(void) const;
    virtual int           GetSelections(wxArrayInt& raSelections) const;

    virtual void          DoSetFirstItem(int n);

    virtual void          DoSetItemClientData(unsigned int n, void* pClientData);
    virtual void*         DoGetItemClientData(unsigned int n) const;

    //
    // wxCheckListBox support
    //
#if wxUSE_OWNER_DRAWN
    long                  OS2OnMeasure(WXMEASUREITEMSTRUCT *item);
    bool                  OS2OnDraw(WXDRAWITEMSTRUCT *item);

    virtual wxOwnerDrawn* CreateItem(size_t n);
    wxOwnerDrawn*         GetItem(size_t n) const { return m_aItems[n]; }
    int                   GetItemIndex(wxOwnerDrawn *item) const { return m_aItems.Index(item); }
#endif // wxUSE_OWNER_DRAWN

    bool                  OS2Command( WXUINT uParam
                                     ,WXWORD wId
                                    );
    virtual void          SetupColours(void);

protected:

    bool                  HasMultipleSelection(void) const;
    virtual wxSize        DoGetBestSize(void) const;

    unsigned int          m_nNumItems;
    int                   m_nSelected;

#if wxUSE_OWNER_DRAWN
    //
    // Control items
    //
    wxListBoxItemsArray             m_aItems;
#endif

    //
    // Implement base wxItemContainer virtuals
    //
    virtual int           DoInsertItems(const wxArrayStringsAdapter& items,
                                        unsigned int pos,
                                        void **clientData,
                                        wxClientDataType type);

    DECLARE_DYNAMIC_CLASS(wxListBox)
}; // end of wxListBox

#endif // _WX_LISTBOX_H_
