/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/treectlg.cpp
// Purpose:     generic tree control implementation
// Author:      Robert Roebling
// Created:     01/02/97
// Modified:    22/10/98 - almost total rewrite, simpler interface (VZ)
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// =============================================================================
// declarations
// =============================================================================

// -----------------------------------------------------------------------------
// headers
// -----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TREECTRL

#include "wx/treectrl.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/timer.h"
    #include "wx/settings.h"
    #include "wx/listbox.h"
    #include "wx/textctrl.h"
#endif

#include "wx/generic/treectlg.h"
#include "wx/imaglist.h"

#include "wx/renderer.h"

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

// -----------------------------------------------------------------------------
// array types
// -----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxGenericTreeItem;

WX_DEFINE_ARRAY_PTR(wxGenericTreeItem *, wxArrayGenericTreeItems);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const int NO_IMAGE = -1;

static const int PIXELS_PER_UNIT = 10;

// the margin between the item state image and the item normal image
static const int MARGIN_BETWEEN_STATE_AND_IMAGE = 2;

// the margin between the item image and the item text
static const int MARGIN_BETWEEN_IMAGE_AND_TEXT = 4;

// -----------------------------------------------------------------------------
// private classes
// -----------------------------------------------------------------------------

// timer used for enabling in-place edit
class WXDLLEXPORT wxTreeRenameTimer: public wxTimer
{
public:
    // start editing the current item after half a second (if the mouse hasn't
    // been clicked/moved)
    enum { DELAY = 500 };

    wxTreeRenameTimer( wxGenericTreeCtrl *owner );

    virtual void Notify();

private:
    wxGenericTreeCtrl *m_owner;

    wxDECLARE_NO_COPY_CLASS(wxTreeRenameTimer);
};

// control used for in-place edit
class WXDLLEXPORT wxTreeTextCtrl: public wxTextCtrl
{
public:
    wxTreeTextCtrl(wxGenericTreeCtrl *owner, wxGenericTreeItem *item);

    void EndEdit( bool discardChanges );

    const wxGenericTreeItem* item() const { return m_itemEdited; }

protected:
    void OnChar( wxKeyEvent &event );
    void OnKeyUp( wxKeyEvent &event );
    void OnKillFocus( wxFocusEvent &event );

    bool AcceptChanges();
    void Finish( bool setfocus );

private:
    wxGenericTreeCtrl  *m_owner;
    wxGenericTreeItem  *m_itemEdited;
    wxString            m_startValue;
    bool                m_aboutToFinish;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxTreeTextCtrl);
};

// timer used to clear wxGenericTreeCtrl::m_findPrefix if no key was pressed
// for a sufficiently long time
class WXDLLEXPORT wxTreeFindTimer : public wxTimer
{
public:
    // reset the current prefix after half a second of inactivity
    enum { DELAY = 500 };

    wxTreeFindTimer( wxGenericTreeCtrl *owner ) { m_owner = owner; }

    virtual void Notify() { m_owner->ResetFindState(); }

private:
    wxGenericTreeCtrl *m_owner;

    wxDECLARE_NO_COPY_CLASS(wxTreeFindTimer);
};

// a tree item
class WXDLLEXPORT wxGenericTreeItem
{
public:
    // ctors & dtor
    wxGenericTreeItem()
    {
        m_data = NULL;
        m_widthText =
        m_heightText = -1;
    }

    wxGenericTreeItem( wxGenericTreeItem *parent,
                       const wxString& text,
                       int image,
                       int selImage,
                       wxTreeItemData *data );

    ~wxGenericTreeItem();

    // trivial accessors
    wxArrayGenericTreeItems& GetChildren() { return m_children; }

    const wxString& GetText() const { return m_text; }
    int GetImage(wxTreeItemIcon which = wxTreeItemIcon_Normal) const
        { return m_images[which]; }
    wxTreeItemData *GetData() const { return m_data; }
    int GetState() const { return m_state; }

    // returns the current image for the item (depending on its
    // selected/expanded/whatever state)
    int GetCurrentImage() const;

    void SetText(const wxString& text)
    {
        m_text = text;

        ResetTextSize();
    }

    void SetImage(int image, wxTreeItemIcon which)
    {
        m_images[which] = image;
        m_width = 0;
    }

    void SetData(wxTreeItemData *data) { m_data = data; }
    void SetState(int state) { m_state = state; m_width = 0; }

    void SetHasPlus(bool has = true) { m_hasPlus = has; }

    void SetBold(bool bold)
    {
        m_isBold = bold;

        ResetTextSize();
    }

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }

    void SetX(int x) { m_x = x; }
    void SetY(int y) { m_y = y; }

    int GetHeight() const { return m_height; }
    int GetWidth() const { return m_width; }

    int GetTextHeight() const
    {
        wxASSERT_MSG( m_heightText != -1, "must call CalculateSize() first" );

        return m_heightText;
    }

    int GetTextWidth() const
    {
        wxASSERT_MSG( m_widthText != -1, "must call CalculateSize() first" );

        return m_widthText;
    }

    wxGenericTreeItem *GetParent() const { return m_parent; }

    // sets the items font for the specified DC if it uses any special font or
    // simply returns false otherwise
    bool SetFont(wxGenericTreeCtrl *control, wxDC& dc) const
    {
        wxFont font;

        wxTreeItemAttr * const attr = GetAttributes();
        if ( attr && attr->HasFont() )
            font = attr->GetFont();
        else if ( IsBold() )
            font = control->m_boldFont;
        else
            return false;

        dc.SetFont(font);
        return true;
    }

    // operations

    // deletes all children notifying the treectrl about it
    void DeleteChildren(wxGenericTreeCtrl *tree);

    // get count of all children (and grand children if 'recursively')
    size_t GetChildrenCount(bool recursively = true) const;

    void Insert(wxGenericTreeItem *child, size_t index)
        { m_children.Insert(child, index); }

    // calculate and cache the item size using either the provided DC (which is
    // supposed to have wxGenericTreeCtrl::m_normalFont selected into it!) or a
    // wxClientDC on the control window
    void CalculateSize(wxGenericTreeCtrl *control, wxDC& dc)
        { DoCalculateSize(control, dc, true /* dc uses normal font */); }
    void CalculateSize(wxGenericTreeCtrl *control);

    void GetSize( int &x, int &y, const wxGenericTreeCtrl* );

    void ResetSize() { m_width = 0; }
    void ResetTextSize() { m_width = 0; m_widthText = -1; }
    void RecursiveResetSize();
    void RecursiveResetTextSize();

        // return the item at given position (or NULL if no item), onButton is
        // true if the point belongs to the item's button, otherwise it lies
        // on the item's label
    wxGenericTreeItem *HitTest( const wxPoint& point,
                                const wxGenericTreeCtrl *,
                                int &flags,
                                int level );

    void Expand() { m_isCollapsed = false; }
    void Collapse() { m_isCollapsed = true; }

    void SetHilight( bool set = true ) { m_hasHilight = set; }

    // status inquiries
    bool HasChildren() const { return !m_children.IsEmpty(); }
    bool IsSelected()  const { return m_hasHilight != 0; }
    bool IsExpanded()  const { return !m_isCollapsed; }
    bool HasPlus()     const { return m_hasPlus || HasChildren(); }
    bool IsBold()      const { return m_isBold != 0; }

    // attributes
        // get them - may be NULL
    wxTreeItemAttr *GetAttributes() const { return m_attr; }
        // get them ensuring that the pointer is not NULL
    wxTreeItemAttr& Attr()
    {
        if ( !m_attr )
        {
            m_attr = new wxTreeItemAttr;
            m_ownsAttr = true;
        }
        return *m_attr;
    }
        // set them
    void SetAttributes(wxTreeItemAttr *attr)
    {
        if ( m_ownsAttr ) delete m_attr;
        m_attr = attr;
        m_ownsAttr = false;
        m_width = 0;
        m_widthText = -1;
    }
        // set them and delete when done
    void AssignAttributes(wxTreeItemAttr *attr)
    {
        SetAttributes(attr);
        m_ownsAttr = true;
        m_width = 0;
        m_widthText = -1;
    }

private:
    // calculate the size of this item, i.e. set m_width, m_height and
    // m_widthText and m_heightText properly
    //
    // if dcUsesNormalFont is true, the current dc font must be the normal tree
    // control font
    void DoCalculateSize(wxGenericTreeCtrl *control,
                         wxDC& dc,
                         bool dcUsesNormalFont);

    // since there can be very many of these, we save size by chosing
    // the smallest representation for the elements and by ordering
    // the members to avoid padding.
    wxString            m_text;         // label to be rendered for item
    int                 m_widthText;
    int                 m_heightText;

    wxTreeItemData     *m_data;         // user-provided data

    int                 m_state;        // item state

    wxArrayGenericTreeItems m_children; // list of children
    wxGenericTreeItem  *m_parent;       // parent of this item

    wxTreeItemAttr     *m_attr;         // attributes???

    // tree ctrl images for the normal, selected, expanded and
    // expanded+selected states
    int                 m_images[wxTreeItemIcon_Max];

    wxCoord             m_x;            // (virtual) offset from top
    wxCoord             m_y;            // (virtual) offset from left
    int                 m_width;        // width of this item
    int                 m_height;       // height of this item

    // use bitfields to save size
    unsigned int        m_isCollapsed :1;
    unsigned int        m_hasHilight  :1; // same as focused
    unsigned int        m_hasPlus     :1; // used for item which doesn't have
                                          // children but has a [+] button
    unsigned int        m_isBold      :1; // render the label in bold font
    unsigned int        m_ownsAttr    :1; // delete attribute when done

    wxDECLARE_NO_COPY_CLASS(wxGenericTreeItem);
};

// =============================================================================
// implementation
// =============================================================================

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// translate the key or mouse event flags to the type of selection we're
// dealing with
static void EventFlagsToSelType(long style,
                                bool shiftDown,
                                bool ctrlDown,
                                bool &is_multiple,
                                bool &extended_select,
                                bool &unselect_others)
{
    is_multiple = (style & wxTR_MULTIPLE) != 0;
    extended_select = shiftDown && is_multiple;
    unselect_others = !(extended_select || (ctrlDown && is_multiple));
}

// check if the given item is under another one
static bool
IsDescendantOf(const wxGenericTreeItem *parent, const wxGenericTreeItem *item)
{
    while ( item )
    {
        if ( item == parent )
        {
            // item is a descendant of parent
            return true;
        }

        item = item->GetParent();
    }

    return false;
}

// -----------------------------------------------------------------------------
// wxTreeRenameTimer (internal)
// -----------------------------------------------------------------------------

wxTreeRenameTimer::wxTreeRenameTimer( wxGenericTreeCtrl *owner )
{
    m_owner = owner;
}

void wxTreeRenameTimer::Notify()
{
    m_owner->OnRenameTimer();
}

//-----------------------------------------------------------------------------
// wxTreeTextCtrl (internal)
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxTreeTextCtrl,wxTextCtrl)
    EVT_CHAR           (wxTreeTextCtrl::OnChar)
    EVT_KEY_UP         (wxTreeTextCtrl::OnKeyUp)
    EVT_KILL_FOCUS     (wxTreeTextCtrl::OnKillFocus)
END_EVENT_TABLE()

wxTreeTextCtrl::wxTreeTextCtrl(wxGenericTreeCtrl *owner,
                               wxGenericTreeItem *itm)
              : m_itemEdited(itm), m_startValue(itm->GetText())
{
    m_owner = owner;
    m_aboutToFinish = false;

    wxRect rect;
    m_owner->GetBoundingRect(m_itemEdited, rect, true);

    // corrects position and size for better appearance
#ifdef __WXMSW__
    rect.x -= 5;
    rect.width += 10;
#elif defined(__WXGTK__)
    rect.x -= 5;
    rect.y -= 2;
    rect.width  += 8;
    rect.height += 4;
#elif defined(wxOSX_USE_CARBON) && wxOSX_USE_CARBON
    int bestHeight = GetBestSize().y - 8;
    if ( rect.height > bestHeight )
    {
        int diff = rect.height - bestHeight;
        rect.height -= diff;
        rect.y += diff / 2;
    }
#endif // platforms

    (void)Create(m_owner, wxID_ANY, m_startValue,
                 rect.GetPosition(), rect.GetSize());

    SelectAll();
}

void wxTreeTextCtrl::EndEdit(bool discardChanges)
{
    if ( m_aboutToFinish )
    {
        // We already called Finish which cannot be called
        // more than once.
        return;
    }

    m_aboutToFinish = true;

    if ( discardChanges )
    {
        m_owner->OnRenameCancelled(m_itemEdited);

        Finish( true );
    }
    else
    {
        // Notify the owner about the changes
        AcceptChanges();

        // Even if vetoed, close the control (consistent with MSW)
        Finish( true );
    }
}

bool wxTreeTextCtrl::AcceptChanges()
{
    const wxString value = GetValue();

    if ( value == m_startValue )
    {
        // nothing changed, always accept
        // when an item remains unchanged, the owner
        // needs to be notified that the user decided
        // not to change the tree item label, and that
        // the edit has been cancelled

        m_owner->OnRenameCancelled(m_itemEdited);
        return true;
    }

    if ( !m_owner->OnRenameAccept(m_itemEdited, value) )
    {
        // vetoed by the user
        return false;
    }

    // accepted, do rename the item
    m_owner->SetItemText(m_itemEdited, value);

    return true;
}

void wxTreeTextCtrl::Finish( bool setfocus )
{
    m_owner->ResetTextControl();

#ifdef __WXMAC__
    // On wxMac, modal event loops avoid deleting pending objects.
    // Hide control so it does not remain visible e.g. if the tree
    // control is used in a dialog.
    Hide();
#endif

    wxPendingDelete.Append(this);

    if (setfocus)
        m_owner->SetFocus();
}

void wxTreeTextCtrl::OnChar( wxKeyEvent &event )
{
    switch ( event.m_keyCode )
    {
        case WXK_RETURN:
            EndEdit( false );
            break;

        case WXK_ESCAPE:
            EndEdit( true );
            break;

        default:
            event.Skip();
    }
}

void wxTreeTextCtrl::OnKeyUp( wxKeyEvent &event )
{
    if ( !m_aboutToFinish )
    {
        // auto-grow the textctrl:
        wxSize parentSize = m_owner->GetSize();
        wxPoint myPos = GetPosition();
        wxSize mySize = GetSize();
        int sx, sy;
        GetTextExtent(GetValue() + wxT("M"), &sx, &sy);
        if (myPos.x + sx > parentSize.x)
            sx = parentSize.x - myPos.x;
        if (mySize.x > sx)
            sx = mySize.x;
        SetSize(sx, wxDefaultCoord);
    }

    event.Skip();
}

void wxTreeTextCtrl::OnKillFocus( wxFocusEvent &event )
{
    if ( !m_aboutToFinish )
    {
        m_aboutToFinish = true;
        if ( !AcceptChanges() )
            m_owner->OnRenameCancelled( m_itemEdited );

        Finish( false );
    }

    // We should let the native text control handle focus, too.
    event.Skip();
}

// -----------------------------------------------------------------------------
// wxGenericTreeItem
// -----------------------------------------------------------------------------

wxGenericTreeItem::wxGenericTreeItem(wxGenericTreeItem *parent,
                                     const wxString& text,
                                     int image, int selImage,
                                     wxTreeItemData *data)
                 : m_text(text)
{
    m_images[wxTreeItemIcon_Normal] = image;
    m_images[wxTreeItemIcon_Selected] = selImage;
    m_images[wxTreeItemIcon_Expanded] = NO_IMAGE;
    m_images[wxTreeItemIcon_SelectedExpanded] = NO_IMAGE;

    m_data = data;
    m_state = wxTREE_ITEMSTATE_NONE;
    m_x = m_y = 0;

    m_isCollapsed = true;
    m_hasHilight = false;
    m_hasPlus = false;
    m_isBold = false;

    m_parent = parent;

    m_attr = NULL;
    m_ownsAttr = false;

    // We don't know the height here yet.
    m_width = 0;
    m_height = 0;

    m_widthText = -1;
    m_heightText = -1;
}

wxGenericTreeItem::~wxGenericTreeItem()
{
    delete m_data;

    if (m_ownsAttr) delete m_attr;

    wxASSERT_MSG( m_children.IsEmpty(),
                  "must call DeleteChildren() before deleting the item" );
}

void wxGenericTreeItem::DeleteChildren(wxGenericTreeCtrl *tree)
{
    size_t count = m_children.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGenericTreeItem *child = m_children[n];
        tree->SendDeleteEvent(child);

        child->DeleteChildren(tree);
        if ( child == tree->m_select_me )
            tree->m_select_me = NULL;
        delete child;
    }

    m_children.Empty();
}

size_t wxGenericTreeItem::GetChildrenCount(bool recursively) const
{
    size_t count = m_children.GetCount();
    if ( !recursively )
        return count;

    size_t total = count;
    for (size_t n = 0; n < count; ++n)
    {
        total += m_children[n]->GetChildrenCount();
    }

    return total;
}

void wxGenericTreeItem::GetSize( int &x, int &y,
                                 const wxGenericTreeCtrl *theButton )
{
    int bottomY=m_y+theButton->GetLineHeight(this);
    if ( y < bottomY ) y = bottomY;
    int width = m_x +  m_width;
    if ( x < width ) x = width;

    if (IsExpanded())
    {
        size_t count = m_children.GetCount();
        for ( size_t n = 0; n < count; ++n )
        {
            m_children[n]->GetSize( x, y, theButton );
        }
    }
}

wxGenericTreeItem *wxGenericTreeItem::HitTest(const wxPoint& point,
                                              const wxGenericTreeCtrl *theCtrl,
                                              int &flags,
                                              int level)
{
    // for a hidden root node, don't evaluate it, but do evaluate children
    if ( !(level == 0 && theCtrl->HasFlag(wxTR_HIDE_ROOT)) )
    {
        // evaluate the item
        int h = theCtrl->GetLineHeight(this);
        if ((point.y > m_y) && (point.y < m_y + h))
        {
            int y_mid = m_y + h/2;
            if (point.y < y_mid )
                flags |= wxTREE_HITTEST_ONITEMUPPERPART;
            else
                flags |= wxTREE_HITTEST_ONITEMLOWERPART;

            int xCross = m_x - theCtrl->GetSpacing();
#ifdef __WXMAC__
            // according to the drawing code the triangels are drawn
            // at -4 , -4  from the position up to +10/+10 max
            if ((point.x > xCross-4) && (point.x < xCross+10) &&
                (point.y > y_mid-4) && (point.y < y_mid+10) &&
                HasPlus() && theCtrl->HasButtons() )
#else
            // 5 is the size of the plus sign
            if ((point.x > xCross-6) && (point.x < xCross+6) &&
                (point.y > y_mid-6) && (point.y < y_mid+6) &&
                HasPlus() && theCtrl->HasButtons() )
#endif
            {
                flags |= wxTREE_HITTEST_ONITEMBUTTON;
                return this;
            }

            if ((point.x >= m_x) && (point.x <= m_x+m_width))
            {
                int image_w = -1;
                int image_h;

                // assuming every image (normal and selected) has the same size!
                if ( (GetImage() != NO_IMAGE) && theCtrl->m_imageListNormal )
                {
                    theCtrl->m_imageListNormal->GetSize(GetImage(),
                                                        image_w, image_h);
                }

                int state_w = -1;
                int state_h;

                if ( (GetState() != wxTREE_ITEMSTATE_NONE) &&
                        theCtrl->m_imageListState )
                {
                    theCtrl->m_imageListState->GetSize(GetState(),
                                                       state_w, state_h);
                }

                if ((state_w != -1) && (point.x <= m_x + state_w + 1))
                    flags |= wxTREE_HITTEST_ONITEMSTATEICON;
                else if ((image_w != -1) &&
                         (point.x <= m_x +
                            (state_w != -1 ? state_w +
                                                MARGIN_BETWEEN_STATE_AND_IMAGE
                                           : 0)
                                            + image_w + 1))
                    flags |= wxTREE_HITTEST_ONITEMICON;
                else
                    flags |= wxTREE_HITTEST_ONITEMLABEL;

                return this;
            }

            if (point.x < m_x)
                flags |= wxTREE_HITTEST_ONITEMINDENT;
            if (point.x > m_x+m_width)
                flags |= wxTREE_HITTEST_ONITEMRIGHT;

            return this;
        }

        // if children are expanded, fall through to evaluate them
        if (m_isCollapsed) return NULL;
    }

    // evaluate children
    size_t count = m_children.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGenericTreeItem *res = m_children[n]->HitTest( point,
                                                         theCtrl,
                                                         flags,
                                                         level + 1 );
        if ( res != NULL )
            return res;
    }

    return NULL;
}

int wxGenericTreeItem::GetCurrentImage() const
{
    int image = NO_IMAGE;
    if ( IsExpanded() )
    {
        if ( IsSelected() )
        {
            image = GetImage(wxTreeItemIcon_SelectedExpanded);
        }

        if ( image == NO_IMAGE )
        {
            // we usually fall back to the normal item, but try just the
            // expanded one (and not selected) first in this case
            image = GetImage(wxTreeItemIcon_Expanded);
        }
    }
    else // not expanded
    {
        if ( IsSelected() )
            image = GetImage(wxTreeItemIcon_Selected);
    }

    // maybe it doesn't have the specific image we want,
    // try the default one instead
    if ( image == NO_IMAGE ) image = GetImage();

    return image;
}

void wxGenericTreeItem::CalculateSize(wxGenericTreeCtrl* control)
{
    // check if we need to do anything before creating the DC
    if ( m_width != 0 )
        return;

    wxClientDC dc(control);
    DoCalculateSize(control, dc, false /* normal font not used */);
}

void
wxGenericTreeItem::DoCalculateSize(wxGenericTreeCtrl* control,
                                   wxDC& dc,
                                   bool dcUsesNormalFont)
{
    if ( m_width != 0 ) // Size known, nothing to do
        return;

    if ( m_widthText == -1 )
    {
        bool fontChanged;
        if ( SetFont(control, dc) )
        {
            fontChanged = true;
        }
        else // we have no special font
        {
           if ( !dcUsesNormalFont )
           {
               // but we do need to ensure that the normal font is used: notice
               // that this doesn't count as changing the font as we don't need
               // to restore it
               dc.SetFont(control->m_normalFont);
           }

           fontChanged = false;
        }

        dc.GetTextExtent( GetText(), &m_widthText, &m_heightText );

        // restore normal font if the DC used it previously and we changed it
        if ( fontChanged )
             dc.SetFont(control->m_normalFont);
    }

    int text_h = m_heightText + 2;

    int image_h = 0, image_w = 0;
    int image = GetCurrentImage();
    if ( image != NO_IMAGE && control->m_imageListNormal )
    {
        control->m_imageListNormal->GetSize(image, image_w, image_h);
        image_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
    }

    int state_h = 0, state_w = 0;
    int state = GetState();
    if ( state != wxTREE_ITEMSTATE_NONE && control->m_imageListState )
    {
        control->m_imageListState->GetSize(state, state_w, state_h);
        if ( image_w != 0 )
            state_w += MARGIN_BETWEEN_STATE_AND_IMAGE;
        else
            state_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
    }

    int img_h = wxMax(state_h, image_h);
    m_height = wxMax(img_h, text_h);

    if (m_height < 30)
        m_height += 2;            // at least 2 pixels
    else
        m_height += m_height / 10;   // otherwise 10% extra spacing

    if (m_height > control->m_lineHeight)
        control->m_lineHeight = m_height;

    m_width = state_w + image_w + m_widthText + 2;
}

void wxGenericTreeItem::RecursiveResetSize()
{
    m_width = 0;

    const size_t count = m_children.Count();
    for (size_t i = 0; i < count; i++ )
        m_children[i]->RecursiveResetSize();
}

void wxGenericTreeItem::RecursiveResetTextSize()
{
    m_width = 0;
    m_widthText = -1;

    const size_t count = m_children.Count();
    for (size_t i = 0; i < count; i++ )
        m_children[i]->RecursiveResetTextSize();
}

// -----------------------------------------------------------------------------
// wxGenericTreeCtrl implementation
// -----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericTreeCtrl, wxControl)

BEGIN_EVENT_TABLE(wxGenericTreeCtrl, wxTreeCtrlBase)
    EVT_PAINT          (wxGenericTreeCtrl::OnPaint)
    EVT_SIZE           (wxGenericTreeCtrl::OnSize)
    EVT_MOUSE_EVENTS   (wxGenericTreeCtrl::OnMouse)
    EVT_KEY_DOWN       (wxGenericTreeCtrl::OnKeyDown)
    EVT_CHAR           (wxGenericTreeCtrl::OnChar)
    EVT_SET_FOCUS      (wxGenericTreeCtrl::OnSetFocus)
    EVT_KILL_FOCUS     (wxGenericTreeCtrl::OnKillFocus)
    EVT_TREE_ITEM_GETTOOLTIP(wxID_ANY, wxGenericTreeCtrl::OnGetToolTip)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------
// construction/destruction
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::Init()
{
    m_current =
    m_key_current =
    m_anchor =
    m_select_me = NULL;
    m_hasFocus = false;
    m_dirty = false;

    m_lineHeight = 10;
    m_indent = 15;
    m_spacing = 18;

    m_hilightBrush = new wxBrush
                         (
                            wxSystemSettings::GetColour
                            (
                                wxSYS_COLOUR_HIGHLIGHT
                            ),
                            wxBRUSHSTYLE_SOLID
                         );

    m_hilightUnfocusedBrush = new wxBrush
                              (
                                 wxSystemSettings::GetColour
                                 (
                                     wxSYS_COLOUR_BTNSHADOW
                                 ),
                                 wxBRUSHSTYLE_SOLID
                              );

    m_imageListButtons = NULL;
    m_ownsImageListButtons = false;

    m_dragCount = 0;
    m_isDragging = false;
    m_dropTarget = m_oldSelection = NULL;
    m_underMouse = NULL;
    m_textCtrl = NULL;

    m_renameTimer = NULL;

    m_findTimer = NULL;
    m_findBell = 0;  // default is to not ring bell at all

    m_dropEffectAboveItem = false;

    m_dndEffect = NoEffect;
    m_dndEffectItem = NULL;

    m_lastOnSame = false;

#if defined( __WXMAC__ )
    m_normalFont = wxFont(wxOSX_SYSTEM_FONT_VIEWS);
#else
    m_normalFont = wxSystemSettings::GetFont( wxSYS_DEFAULT_GUI_FONT );
#endif
    m_boldFont = m_normalFont.Bold();
}

bool wxGenericTreeCtrl::Create(wxWindow *parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxValidator& validator,
                               const wxString& name )
{
#ifdef __WXMAC__
    int major, minor;
    wxGetOsVersion(&major, &minor);

    if (major < 10)
        style |= wxTR_ROW_LINES;

    if (style & wxTR_HAS_BUTTONS)
        style |= wxTR_NO_LINES;
#endif // __WXMAC__

#ifdef __WXGTK20__
    if (style & wxTR_HAS_BUTTONS)
        style |= wxTR_NO_LINES;
#endif

    if ( !wxControl::Create( parent, id, pos, size,
                             style|wxHSCROLL|wxVSCROLL|wxWANTS_CHARS,
                             validator,
                             name ) )
        return false;

    // If the tree display has no buttons, but does have
    // connecting lines, we can use a narrower layout.
    // It may not be a good idea to force this...
    if (!HasButtons() && !HasFlag(wxTR_NO_LINES))
    {
        m_indent= 10;
        m_spacing = 10;
    }

    wxVisualAttributes attr = GetDefaultAttributes();
    SetOwnForegroundColour( attr.colFg );
    SetOwnBackgroundColour( attr.colBg );
    if (!m_hasFont)
        SetOwnFont(attr.font);

    // this is a misnomer: it's called "dotted pen" but uses (default) wxSOLID
    // style because we apparently get performance problems when using dotted
    // pen for drawing in some ports -- but under MSW it seems to work fine
#ifdef __WXMSW__
    m_dottedPen = wxPen(*wxLIGHT_GREY, 0, wxPENSTYLE_DOT);
#else
    m_dottedPen = *wxGREY_PEN;
#endif

    SetInitialSize(size);

    return true;
}

wxGenericTreeCtrl::~wxGenericTreeCtrl()
{
    delete m_hilightBrush;
    delete m_hilightUnfocusedBrush;

    DeleteAllItems();

    delete m_renameTimer;
    delete m_findTimer;

    if (m_ownsImageListButtons)
        delete m_imageListButtons;
}

void wxGenericTreeCtrl::EnableBellOnNoMatch( bool on )
{
    m_findBell = on;
}

// -----------------------------------------------------------------------------
// accessors
// -----------------------------------------------------------------------------

unsigned int wxGenericTreeCtrl::GetCount() const
{
    if ( !m_anchor )
    {
        // the tree is empty
        return 0;
    }

    unsigned int count = m_anchor->GetChildrenCount();
    if ( !HasFlag(wxTR_HIDE_ROOT) )
    {
        // take the root itself into account
        count++;
    }

    return count;
}

void wxGenericTreeCtrl::SetIndent(unsigned int indent)
{
    m_indent = (unsigned short) indent;
    m_dirty = true;
}

size_t
wxGenericTreeCtrl::GetChildrenCount(const wxTreeItemId& item,
                                    bool recursively) const
{
    wxCHECK_MSG( item.IsOk(), 0u, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetChildrenCount(recursively);
}

void wxGenericTreeCtrl::SetWindowStyle(const long styles)
{
    // Do not try to expand the root node if it hasn't been created yet
    if (m_anchor && !HasFlag(wxTR_HIDE_ROOT) && (styles & wxTR_HIDE_ROOT))
    {
        // if we will hide the root, make sure children are visible
        m_anchor->SetHasPlus();
        m_anchor->Expand();
        CalculatePositions();
    }

    // right now, just sets the styles.  Eventually, we may
    // want to update the inherited styles, but right now
    // none of the parents has updatable styles
    m_windowStyle = styles;
    m_dirty = true;
}

// -----------------------------------------------------------------------------
// functions to work with tree items
// -----------------------------------------------------------------------------

wxString wxGenericTreeCtrl::GetItemText(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxEmptyString, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetText();
}

int wxGenericTreeCtrl::GetItemImage(const wxTreeItemId& item,
                             wxTreeItemIcon which) const
{
    wxCHECK_MSG( item.IsOk(), -1, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetImage(which);
}

wxTreeItemData *wxGenericTreeCtrl::GetItemData(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), NULL, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetData();
}

int wxGenericTreeCtrl::DoGetItemState(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTREE_ITEMSTATE_NONE, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->GetState();
}

wxColour wxGenericTreeCtrl::GetItemTextColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetTextColour();
}

wxColour
wxGenericTreeCtrl::GetItemBackgroundColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetBackgroundColour();
}

wxFont wxGenericTreeCtrl::GetItemFont(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullFont, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetFont();
}

void
wxGenericTreeCtrl::SetItemText(const wxTreeItemId& item, const wxString& text)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetText(text);
    pItem->CalculateSize(this);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemImage(const wxTreeItemId& item,
                              int image,
                              wxTreeItemIcon which)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetImage(image, which);
    pItem->CalculateSize(this);
    RefreshLine(pItem);
}

void
wxGenericTreeCtrl::SetItemData(const wxTreeItemId& item, wxTreeItemData *data)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    if (data)
        data->SetId( item );

    ((wxGenericTreeItem*) item.m_pItem)->SetData(data);
}

void wxGenericTreeCtrl::DoSetItemState(const wxTreeItemId& item, int state)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetState(state);
    pItem->CalculateSize(this);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemHasChildren(const wxTreeItemId& item, bool has)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetHasPlus(has);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemBold(const wxTreeItemId& item, bool bold)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    // avoid redrawing the tree if no real change
    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    if ( pItem->IsBold() != bold )
    {
        pItem->SetBold(bold);

        // recalculate the item size as bold and non bold fonts have different
        // widths
        pItem->CalculateSize(this);
        RefreshLine(pItem);
    }
}

void wxGenericTreeCtrl::SetItemDropHighlight(const wxTreeItemId& item,
                                             bool highlight)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxColour fg, bg;

    if (highlight)
    {
        bg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
        fg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
    }

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetTextColour(fg);
    pItem->Attr().SetBackgroundColour(bg);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemTextColour(const wxTreeItemId& item,
                                   const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetTextColour(col);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemBackgroundColour(const wxTreeItemId& item,
                                         const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetBackgroundColour(col);
    RefreshLine(pItem);
}

void
wxGenericTreeCtrl::SetItemFont(const wxTreeItemId& item, const wxFont& font)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetFont(font);
    pItem->ResetTextSize();
    pItem->CalculateSize(this);
    RefreshLine(pItem);
}

bool wxGenericTreeCtrl::SetFont( const wxFont &font )
{
    wxTreeCtrlBase::SetFont(font);

    m_normalFont = font;
    m_boldFont = m_normalFont.Bold();

    if (m_anchor)
        m_anchor->RecursiveResetTextSize();

    return true;
}


// -----------------------------------------------------------------------------
// item status inquiries
// -----------------------------------------------------------------------------

bool wxGenericTreeCtrl::IsVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    // Hidden root item is never visible.
    if ( item == GetRootItem() && HasFlag(wxTR_HIDE_ROOT) )
        return false;

    // An item is only visible if it's not a descendant of a collapsed item
    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem* parent = pItem->GetParent();
    while (parent)
    {
        if (!parent->IsExpanded())
            return false;
        parent = parent->GetParent();
    }

    int startX, startY;
    GetViewStart(& startX, & startY);

    wxSize clientSize = GetClientSize();

    wxRect rect;
    if (!GetBoundingRect(item, rect))
        return false;
    if (rect.GetWidth() == 0 || rect.GetHeight() == 0)
        return false;
    if (rect.GetBottom() < 0 || rect.GetTop() > clientSize.y)
        return false;
    if (rect.GetRight() < 0 || rect.GetLeft() > clientSize.x)
        return false;

    return true;
}

bool wxGenericTreeCtrl::ItemHasChildren(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    // consider that the item does have children if it has the "+" button: it
    // might not have them (if it had never been expanded yet) but then it
    // could have them as well and it's better to err on this side rather than
    // disabling some operations which are restricted to the items with
    // children for an item which does have them
    return ((wxGenericTreeItem*) item.m_pItem)->HasPlus();
}

bool wxGenericTreeCtrl::IsExpanded(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsExpanded();
}

bool wxGenericTreeCtrl::IsSelected(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsSelected();
}

bool wxGenericTreeCtrl::IsBold(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), false, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsBold();
}

// -----------------------------------------------------------------------------
// navigation
// -----------------------------------------------------------------------------

wxTreeItemId wxGenericTreeCtrl::GetItemParent(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetParent();
}

wxTreeItemId wxGenericTreeCtrl::GetFirstChild(const wxTreeItemId& item,
                                              wxTreeItemIdValue& cookie) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    cookie = 0;
    return GetNextChild(item, cookie);
}

wxTreeItemId wxGenericTreeCtrl::GetNextChild(const wxTreeItemId& item,
                                             wxTreeItemIdValue& cookie) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxArrayGenericTreeItems&
        children = ((wxGenericTreeItem*) item.m_pItem)->GetChildren();

    // it's ok to cast cookie to size_t, we never have indices big enough to
    // overflow "void *"
    size_t *pIndex = (size_t *)&cookie;
    if ( *pIndex < children.GetCount() )
    {
        return children.Item((*pIndex)++);
    }
    else
    {
        // there are no more of them
        return wxTreeItemId();
    }
}

wxTreeItemId wxGenericTreeCtrl::GetLastChild(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxArrayGenericTreeItems&
        children = ((wxGenericTreeItem*) item.m_pItem)->GetChildren();
    return children.IsEmpty() ? wxTreeItemId() : wxTreeItemId(children.Last());
}

wxTreeItemId wxGenericTreeCtrl::GetNextSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem *parent = i->GetParent();
    if ( parent == NULL )
    {
        // root item doesn't have any siblings
        return wxTreeItemId();
    }

    wxArrayGenericTreeItems& siblings = parent->GetChildren();
    int index = siblings.Index(i);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    size_t n = (size_t)(index + 1);
    return n == siblings.GetCount() ? wxTreeItemId()
                                    : wxTreeItemId(siblings[n]);
}

wxTreeItemId wxGenericTreeCtrl::GetPrevSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem *parent = i->GetParent();
    if ( parent == NULL )
    {
        // root item doesn't have any siblings
        return wxTreeItemId();
    }

    wxArrayGenericTreeItems& siblings = parent->GetChildren();
    int index = siblings.Index(i);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    return index == 0 ? wxTreeItemId()
                      : wxTreeItemId(siblings[(size_t)(index - 1)]);
}

// Only for internal use right now, but should probably be public
wxTreeItemId wxGenericTreeCtrl::GetNext(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    // First see if there are any children.
    wxArrayGenericTreeItems& children = i->GetChildren();
    if (children.GetCount() > 0)
    {
         return children.Item(0);
    }
    else
    {
         // Try a sibling of this or ancestor instead
         wxTreeItemId p = item;
         wxTreeItemId toFind;
         do
         {
              toFind = GetNextSibling(p);
              p = GetItemParent(p);
         } while (p.IsOk() && !toFind.IsOk());
         return toFind;
    }
}

wxTreeItemId wxGenericTreeCtrl::GetFirstVisibleItem() const
{
    wxTreeItemId itemid = GetRootItem();
    if (!itemid.IsOk())
        return itemid;

    do
    {
        if (IsVisible(itemid))
              return itemid;
        itemid = GetNext(itemid);
    } while (itemid.IsOk());

    return wxTreeItemId();
}

wxTreeItemId wxGenericTreeCtrl::GetNextVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    wxASSERT_MSG( IsVisible(item), wxT("this item itself should be visible") );

    wxTreeItemId id = item;
    if (id.IsOk())
    {
        while (id = GetNext(id), id.IsOk())
        {
            if (IsVisible(id))
                return id;
        }
    }
    return wxTreeItemId();
}

wxTreeItemId wxGenericTreeCtrl::GetPrevVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );
    wxASSERT_MSG( IsVisible(item), wxT("this item itself should be visible") );

    // find out the starting point
    wxTreeItemId prevItem = GetPrevSibling(item);
    if ( !prevItem.IsOk() )
    {
        prevItem = GetItemParent(item);
    }

    // find the first visible item after it
    while ( prevItem.IsOk() && !IsVisible(prevItem) )
    {
        prevItem = GetNext(prevItem);
        if ( !prevItem.IsOk() || prevItem == item )
        {
            // there are no visible items before item
            return wxTreeItemId();
        }
    }

    // from there we must be able to navigate until this item
    while ( prevItem.IsOk() )
    {
        const wxTreeItemId nextItem = GetNextVisible(prevItem);
        if ( !nextItem.IsOk() || nextItem == item )
            break;

        prevItem = nextItem;
    }

    return prevItem;
}

// called by wxTextTreeCtrl when it marks itself for deletion
void wxGenericTreeCtrl::ResetTextControl()
{
    m_textCtrl = NULL;
}

void wxGenericTreeCtrl::ResetFindState()
{
    m_findPrefix.clear();
    if ( m_findBell )
        m_findBell = 1;
}

// find the first item starting with the given prefix after the given item
wxTreeItemId wxGenericTreeCtrl::FindItem(const wxTreeItemId& idParent,
                                         const wxString& prefixOrig) const
{
    // match is case insensitive as this is more convenient to the user: having
    // to press Shift-letter to go to the item starting with a capital letter
    // would be too bothersome
    wxString prefix = prefixOrig.Lower();

    // determine the starting point: we shouldn't take the current item (this
    // allows to switch between two items starting with the same letter just by
    // pressing it) but we shouldn't jump to the next one if the user is
    // continuing to type as otherwise he might easily skip the item he wanted
    wxTreeItemId itemid = idParent;
    if ( prefix.length() == 1 )
    {
        itemid = GetNext(itemid);
    }

    // look for the item starting with the given prefix after it
    while ( itemid.IsOk() && !GetItemText(itemid).Lower().StartsWith(prefix) )
    {
        itemid = GetNext(itemid);
    }

    // if we haven't found anything...
    if ( !itemid.IsOk() )
    {
        // ... wrap to the beginning
        itemid = GetRootItem();
        if ( HasFlag(wxTR_HIDE_ROOT) )
        {
            // can't select virtual root
            itemid = GetNext(itemid);
        }

        // and try all the items (stop when we get to the one we started from)
        while ( itemid.IsOk() && itemid != idParent &&
                    !GetItemText(itemid).Lower().StartsWith(prefix) )
        {
            itemid = GetNext(itemid);
        }
        // If we haven't found the item but wrapped back to the one we started
        // from, id.IsOk() must be false
        if ( itemid == idParent )
        {
            itemid = wxTreeItemId();
        }
    }

    return itemid;
}

// -----------------------------------------------------------------------------
// operations
// -----------------------------------------------------------------------------

wxTreeItemId wxGenericTreeCtrl::DoInsertItem(const wxTreeItemId& parentId,
                                             size_t previous,
                                             const wxString& text,
                                             int image,
                                             int selImage,
                                             wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    m_dirty = true;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item =
        new wxGenericTreeItem( parent, text, image, selImage, data );

    if ( data != NULL )
    {
        data->m_pItem = item;
    }

    parent->Insert( item, previous == (size_t)-1 ? parent->GetChildren().size()
                                                 : previous );

    InvalidateBestSize();
    return item;
}

wxTreeItemId wxGenericTreeCtrl::AddRoot(const wxString& text,
                                        int image,
                                        int selImage,
                                        wxTreeItemData *data)
{
    wxCHECK_MSG( !m_anchor, wxTreeItemId(), "tree can have only one root" );

    m_dirty = true;     // do this first so stuff below doesn't cause flicker

    m_anchor = new wxGenericTreeItem(NULL, text,
                                   image, selImage, data);
    if ( data != NULL )
    {
        data->m_pItem = m_anchor;
    }

    if (HasFlag(wxTR_HIDE_ROOT))
    {
        // if root is hidden, make sure we can navigate
        // into children
        m_anchor->SetHasPlus();
        m_anchor->Expand();
        CalculatePositions();
    }

    if (!HasFlag(wxTR_MULTIPLE))
    {
        m_current = m_key_current = m_anchor;
        m_current->SetHilight( true );
    }

    InvalidateBestSize();
    return m_anchor;
}

wxTreeItemId wxGenericTreeCtrl::DoInsertAfter(const wxTreeItemId& parentId,
                                              const wxTreeItemId& idPrevious,
                                              const wxString& text,
                                              int image, int selImage,
                                              wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    int index = -1;
    if (idPrevious.IsOk())
    {
        index = parent->GetChildren().Index(
                    (wxGenericTreeItem*) idPrevious.m_pItem);
        wxASSERT_MSG( index != wxNOT_FOUND,
                      "previous item in wxGenericTreeCtrl::InsertItem() "
                      "is not a sibling" );
    }

    return DoInsertItem(parentId, (size_t)++index, text, image, selImage, data);
}


void wxGenericTreeCtrl::SendDeleteEvent(wxGenericTreeItem *item)
{
    wxTreeEvent event(wxEVT_TREE_DELETE_ITEM, this, item);
    GetEventHandler()->ProcessEvent( event );
}

// Don't leave edit or selection on a child which is about to disappear
void wxGenericTreeCtrl::ChildrenClosing(wxGenericTreeItem* item)
{
    if ( m_textCtrl && item != m_textCtrl->item() &&
            IsDescendantOf(item, m_textCtrl->item()) )
    {
        m_textCtrl->EndEdit( true );
    }

    if ( item != m_key_current && IsDescendantOf(item, m_key_current) )
    {
        m_key_current = NULL;
    }

    if ( IsDescendantOf(item, m_select_me) )
    {
        m_select_me = item;
    }

    if ( item != m_current && IsDescendantOf(item, m_current) )
    {
        m_current->SetHilight( false );
        m_current = NULL;
        m_select_me = item;
    }
}

void wxGenericTreeCtrl::DeleteChildren(const wxTreeItemId& itemId)
{
    m_dirty = true;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;
    ChildrenClosing(item);
    item->DeleteChildren(this);
    InvalidateBestSize();
}

void wxGenericTreeCtrl::Delete(const wxTreeItemId& itemId)
{
    m_dirty = true;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    if (m_textCtrl != NULL && IsDescendantOf(item, m_textCtrl->item()))
    {
        // can't delete the item being edited, cancel editing it first
        m_textCtrl->EndEdit( true );
    }

    wxGenericTreeItem *parent = item->GetParent();

    // if the selected item will be deleted, select the parent ...
    wxGenericTreeItem *to_be_selected = parent;
    if (parent)
    {
        // .. unless there is a next sibling like wxMSW does it
        int pos = parent->GetChildren().Index( item );
        if ((int)(parent->GetChildren().GetCount()) > pos+1)
            to_be_selected = parent->GetChildren().Item( pos+1 );
    }

    // don't keep stale pointers around!
    if ( IsDescendantOf(item, m_key_current) )
    {
        // Don't silently change the selection:
        // do it properly in idle time, so event
        // handlers get called.

        // m_key_current = parent;
        m_key_current = NULL;
    }

    // m_select_me records whether we need to select
    // a different item, in idle time.
    if ( m_select_me && IsDescendantOf(item, m_select_me) )
    {
        m_select_me = to_be_selected;
    }

    if ( IsDescendantOf(item, m_current) )
    {
        // Don't silently change the selection:
        // do it properly in idle time, so event
        // handlers get called.

        // m_current = parent;
        m_current = NULL;
        m_select_me = to_be_selected;
    }

    // remove the item from the tree
    if ( parent )
    {
        parent->GetChildren().Remove( item );  // remove by value
    }
    else // deleting the root
    {
        // nothing will be left in the tree
        m_anchor = NULL;
    }

    // and delete all of its children and the item itself now
    item->DeleteChildren(this);
    SendDeleteEvent(item);

    if (item == m_select_me)
        m_select_me = NULL;

    delete item;

    InvalidateBestSize();
}

void wxGenericTreeCtrl::DeleteAllItems()
{
    if ( m_anchor )
    {
        Delete(m_anchor);
    }
}

void wxGenericTreeCtrl::Expand(const wxTreeItemId& itemId)
{
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    wxCHECK_RET( item, wxT("invalid item in wxGenericTreeCtrl::Expand") );
    wxCHECK_RET( !HasFlag(wxTR_HIDE_ROOT) || itemId != GetRootItem(),
                 wxT("can't expand hidden root") );

    if ( !item->HasPlus() )
        return;

    if ( item->IsExpanded() )
        return;

    wxTreeEvent event(wxEVT_TREE_ITEM_EXPANDING, this, item);

    if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
    {
        // cancelled by program
        return;
    }

    item->Expand();
    if ( !IsFrozen() )
    {
        CalculatePositions();

        RefreshSubtree(item);
    }
    else // frozen
    {
        m_dirty = true;
    }

    event.SetEventType(wxEVT_TREE_ITEM_EXPANDED);
    GetEventHandler()->ProcessEvent( event );
}

void wxGenericTreeCtrl::Collapse(const wxTreeItemId& itemId)
{
    wxCHECK_RET( !HasFlag(wxTR_HIDE_ROOT) || itemId != GetRootItem(),
                 wxT("can't collapse hidden root") );

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    if ( !item->IsExpanded() )
        return;

    wxTreeEvent event(wxEVT_TREE_ITEM_COLLAPSING, this, item);
    if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
    {
        // cancelled by program
        return;
    }

    ChildrenClosing(item);
    item->Collapse();

#if 0  // TODO why should items be collapsed recursively?
    wxArrayGenericTreeItems& children = item->GetChildren();
    size_t count = children.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        Collapse(children[n]);
    }
#endif

    CalculatePositions();

    RefreshSubtree(item);

    event.SetEventType(wxEVT_TREE_ITEM_COLLAPSED);
    GetEventHandler()->ProcessEvent( event );
}

void wxGenericTreeCtrl::CollapseAndReset(const wxTreeItemId& item)
{
    Collapse(item);
    DeleteChildren(item);
}

void wxGenericTreeCtrl::Toggle(const wxTreeItemId& itemId)
{
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    if (item->IsExpanded())
        Collapse(itemId);
    else
        Expand(itemId);
}

void wxGenericTreeCtrl::Unselect()
{
    if (m_current)
    {
        m_current->SetHilight( false );
        RefreshLine( m_current );

        m_current = NULL;
        m_select_me = NULL;
    }
}

void wxGenericTreeCtrl::ClearFocusedItem()
{
    wxTreeItemId item = GetFocusedItem();
    if ( item.IsOk() )
        SelectItem(item, false);
}

void wxGenericTreeCtrl::SetFocusedItem(const wxTreeItemId& item)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    SelectItem(item, true);
}

void wxGenericTreeCtrl::UnselectAllChildren(wxGenericTreeItem *item)
{
    if (item->IsSelected())
    {
        item->SetHilight(false);
        RefreshLine(item);
    }

    if (item->HasChildren())
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        size_t count = children.GetCount();
        for ( size_t n = 0; n < count; ++n )
        {
            UnselectAllChildren(children[n]);
        }
    }
}

void wxGenericTreeCtrl::UnselectAll()
{
    wxTreeItemId rootItem = GetRootItem();

    // the tree might not have the root item at all
    if ( rootItem )
    {
        UnselectAllChildren((wxGenericTreeItem*) rootItem.m_pItem);
    }
}

void wxGenericTreeCtrl::SelectChildren(const wxTreeItemId& parent)
{
    wxCHECK_RET( HasFlag(wxTR_MULTIPLE),
                 "this only works with multiple selection controls" );

    UnselectAll();

    if ( !HasChildren(parent) )
        return;


    wxArrayGenericTreeItems&
        children = ((wxGenericTreeItem*) parent.m_pItem)->GetChildren();
    size_t count = children.GetCount();

    wxGenericTreeItem *
        item = (wxGenericTreeItem*) ((wxTreeItemId)children[0]).m_pItem;
    wxTreeEvent event(wxEVT_TREE_SEL_CHANGING, this, item);
    event.m_itemOld = m_current;

    if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
        return;

    for ( size_t n = 0; n < count; ++n )
    {
        m_current = m_key_current = children[n];
        m_current->SetHilight(true);
        RefreshSelected();
    }


    event.SetEventType(wxEVT_TREE_SEL_CHANGED);
    GetEventHandler()->ProcessEvent( event );
}


// Recursive function !
// To stop we must have crt_item<last_item
// Algorithm :
// Tag all next children, when no more children,
// Move to parent (not to tag)
// Keep going... if we found last_item, we stop.
bool
wxGenericTreeCtrl::TagNextChildren(wxGenericTreeItem *crt_item,
                                   wxGenericTreeItem *last_item,
                                   bool select)
{
    wxGenericTreeItem *parent = crt_item->GetParent();

    if (parent == NULL) // This is root item
        return TagAllChildrenUntilLast(crt_item, last_item, select);

    wxArrayGenericTreeItems& children = parent->GetChildren();
    int index = children.Index(crt_item);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    size_t count = children.GetCount();
    for (size_t n=(size_t)(index+1); n<count; ++n)
    {
        if ( TagAllChildrenUntilLast(children[n], last_item, select) )
            return true;
    }

    return TagNextChildren(parent, last_item, select);
}

bool
wxGenericTreeCtrl::TagAllChildrenUntilLast(wxGenericTreeItem *crt_item,
                                           wxGenericTreeItem *last_item,
                                           bool select)
{
    crt_item->SetHilight(select);
    RefreshLine(crt_item);

    if (crt_item==last_item)
        return true;

    // We should leave the not shown children of collapsed items alone.
    if (crt_item->HasChildren() && crt_item->IsExpanded())
    {
        wxArrayGenericTreeItems& children = crt_item->GetChildren();
        size_t count = children.GetCount();
        for ( size_t n = 0; n < count; ++n )
        {
            if (TagAllChildrenUntilLast(children[n], last_item, select))
                return true;
        }
    }

  return false;
}

void
wxGenericTreeCtrl::SelectItemRange(wxGenericTreeItem *item1,
                                   wxGenericTreeItem *item2)
{
    m_select_me = NULL;

    // item2 is not necessary after item1
    // choice first' and 'last' between item1 and item2
    wxGenericTreeItem *first= (item1->GetY()<item2->GetY()) ? item1 : item2;
    wxGenericTreeItem *last = (item1->GetY()<item2->GetY()) ? item2 : item1;

    bool select = m_current->IsSelected();

    if ( TagAllChildrenUntilLast(first,last,select) )
        return;

    TagNextChildren(first,last,select);
}

void wxGenericTreeCtrl::DoSelectItem(const wxTreeItemId& itemId,
                                     bool unselect_others,
                                     bool extended_select)
{
    wxCHECK_RET( itemId.IsOk(), wxT("invalid tree item") );

    m_select_me = NULL;

    bool is_single=!(GetWindowStyleFlag() & wxTR_MULTIPLE);
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    //wxCHECK_RET( ( (!unselect_others) && is_single),
    //           wxT("this is a single selection tree") );

    // to keep going anyhow !!!
    if (is_single)
    {
        if (item->IsSelected())
            return; // nothing to do
        unselect_others = true;
        extended_select = false;
    }
    else if ( unselect_others && item->IsSelected() )
    {
        // selection change if there is more than one item currently selected
        wxArrayTreeItemIds selected_items;
        if ( GetSelections(selected_items) == 1 )
            return;
    }

    wxTreeEvent event(wxEVT_TREE_SEL_CHANGING, this, item);
    event.m_itemOld = m_current;
    // TODO : Here we don't send any selection mode yet !

    if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
        return;

    wxTreeItemId parent = GetItemParent( itemId );
    while (parent.IsOk())
    {
        if (!IsExpanded(parent))
            Expand( parent );

        parent = GetItemParent( parent );
    }

    // ctrl press
    if (unselect_others)
    {
        if (is_single) Unselect(); // to speed up thing
        else UnselectAll();
    }

    // shift press
    if (extended_select)
    {
        if ( !m_current )
        {
            m_current =
            m_key_current = (wxGenericTreeItem*) GetRootItem().m_pItem;
        }

        // don't change the mark (m_current)
        SelectItemRange(m_current, item);
    }
    else
    {
        bool select = true; // the default

        // Check if we need to toggle hilight (ctrl mode)
        if (!unselect_others)
            select=!item->IsSelected();

        m_current = m_key_current = item;
        m_current->SetHilight(select);
        RefreshLine( m_current );
    }

    // This can cause idle processing to select the root
    // if no item is selected, so it must be after the
    // selection is set
    EnsureVisible( itemId );

    event.SetEventType(wxEVT_TREE_SEL_CHANGED);
    GetEventHandler()->ProcessEvent( event );
}

void wxGenericTreeCtrl::SelectItem(const wxTreeItemId& itemId, bool select)
{
    wxGenericTreeItem * const item = (wxGenericTreeItem*) itemId.m_pItem;
    wxCHECK_RET( item, wxT("SelectItem(): invalid tree item") );

    if ( select )
    {
        if ( !item->IsSelected() )
            DoSelectItem(itemId, !HasFlag(wxTR_MULTIPLE));
    }
    else // deselect
    {
        wxTreeEvent event(wxEVT_TREE_SEL_CHANGING, this, item);
        if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
            return;

        item->SetHilight(false);
        RefreshLine(item);

        event.SetEventType(wxEVT_TREE_SEL_CHANGED);
        GetEventHandler()->ProcessEvent( event );
    }
}

void wxGenericTreeCtrl::FillArray(wxGenericTreeItem *item,
                                  wxArrayTreeItemIds &array) const
{
    if ( item->IsSelected() )
        array.Add(wxTreeItemId(item));

    if ( item->HasChildren() )
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        size_t count = children.GetCount();
        for ( size_t n = 0; n < count; ++n )
            FillArray(children[n], array);
    }
}

size_t wxGenericTreeCtrl::GetSelections(wxArrayTreeItemIds &array) const
{
    array.Empty();
    wxTreeItemId idRoot = GetRootItem();
    if ( idRoot.IsOk() )
    {
        FillArray((wxGenericTreeItem*) idRoot.m_pItem, array);
    }
    //else: the tree is empty, so no selections

    return array.GetCount();
}

void wxGenericTreeCtrl::EnsureVisible(const wxTreeItemId& item)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    if (!item.IsOk()) return;

    wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;

    // first expand all parent branches
    wxGenericTreeItem *parent = gitem->GetParent();

    if ( HasFlag(wxTR_HIDE_ROOT) )
    {
        while ( parent && parent != m_anchor )
        {
            Expand(parent);
            parent = parent->GetParent();
        }
    }
    else
    {
        while ( parent )
        {
            Expand(parent);
            parent = parent->GetParent();
        }
    }

    //if (parent) CalculatePositions();

    ScrollTo(item);
}

void wxGenericTreeCtrl::ScrollTo(const wxTreeItemId &item)
{
    if (!item.IsOk())
        return;

    // update the control before scrolling it
    if (m_dirty)
    {
#if defined( __WXMSW__ ) 
        Update();
#elif defined(__WXMAC__)
        Update();
        DoDirtyProcessing();
#else
        DoDirtyProcessing();
#endif
    }
        
    wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;

    int itemY = gitem->GetY();

    int start_x = 0;
    int start_y = 0;
    GetViewStart( &start_x, &start_y );

    const int clientHeight = GetClientSize().y;

    const int itemHeight = GetLineHeight(gitem) + 2;

    if ( itemY + itemHeight > start_y*PIXELS_PER_UNIT + clientHeight )
    {
        // need to scroll down by enough to show this item fully
        itemY += itemHeight - clientHeight;

        // because itemY below will be divided by PIXELS_PER_UNIT it may
        // be rounded down, with the result of the item still only being 
        // partially visible, so make sure we are rounding up
        itemY += PIXELS_PER_UNIT - 1;
    }
    else if ( itemY > start_y*PIXELS_PER_UNIT )
    {
        // item is already fully visible, don't do anything
        return;
    }
    //else: scroll up to make this item the top one displayed

    Scroll(-1, itemY/PIXELS_PER_UNIT);
}

// FIXME: tree sorting functions are not reentrant and not MT-safe!
static wxGenericTreeCtrl *s_treeBeingSorted = NULL;

static int LINKAGEMODE tree_ctrl_compare_func(wxGenericTreeItem **item1,
                                  wxGenericTreeItem **item2)
{
    wxCHECK_MSG( s_treeBeingSorted, 0,
                 "bug in wxGenericTreeCtrl::SortChildren()" );

    return s_treeBeingSorted->OnCompareItems(*item1, *item2);
}

void wxGenericTreeCtrl::SortChildren(const wxTreeItemId& itemId)
{
    wxCHECK_RET( itemId.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    wxCHECK_RET( !s_treeBeingSorted,
                 wxT("wxGenericTreeCtrl::SortChildren is not reentrant") );

    wxArrayGenericTreeItems& children = item->GetChildren();
    if ( children.GetCount() > 1 )
    {
        m_dirty = true;

        s_treeBeingSorted = this;
        children.Sort(tree_ctrl_compare_func);
        s_treeBeingSorted = NULL;
    }
    //else: don't make the tree dirty as nothing changed
}

void wxGenericTreeCtrl::CalculateLineHeight()
{
    wxClientDC dc(this);
    m_lineHeight = (int)(dc.GetCharHeight() + 4);

    if ( m_imageListNormal )
    {
        // Calculate a m_lineHeight value from the normal Image sizes.
        // May be toggle off. Then wxGenericTreeCtrl will spread when
        // necessary (which might look ugly).
        int n = m_imageListNormal->GetImageCount();
        for (int i = 0; i < n ; i++)
        {
            int width = 0, height = 0;
            m_imageListNormal->GetSize(i, width, height);
            if (height > m_lineHeight) m_lineHeight = height;
        }
    }

    if ( m_imageListState )
    {
        // Calculate a m_lineHeight value from the state Image sizes.
        // May be toggle off. Then wxGenericTreeCtrl will spread when
        // necessary (which might look ugly).
        int n = m_imageListState->GetImageCount();
        for (int i = 0; i < n ; i++)
        {
            int width = 0, height = 0;
            m_imageListState->GetSize(i, width, height);
            if (height > m_lineHeight) m_lineHeight = height;
        }
    }

    if (m_imageListButtons)
    {
        // Calculate a m_lineHeight value from the Button image sizes.
        // May be toggle off. Then wxGenericTreeCtrl will spread when
        // necessary (which might look ugly).
        int n = m_imageListButtons->GetImageCount();
        for (int i = 0; i < n ; i++)
        {
            int width = 0, height = 0;
            m_imageListButtons->GetSize(i, width, height);
            if (height > m_lineHeight) m_lineHeight = height;
        }
    }

    if (m_lineHeight < 30)
        m_lineHeight += 2;                 // at least 2 pixels
    else
        m_lineHeight += m_lineHeight/10;   // otherwise 10% extra spacing
}

void wxGenericTreeCtrl::SetImageList(wxImageList *imageList)
{
    if (m_ownsImageListNormal) delete m_imageListNormal;
    m_imageListNormal = imageList;
    m_ownsImageListNormal = false;
    m_dirty = true;

    if (m_anchor)
        m_anchor->RecursiveResetSize();

    // Don't do any drawing if we're setting the list to NULL,
    // since we may be in the process of deleting the tree control.
    if (imageList)
        CalculateLineHeight();
}

void wxGenericTreeCtrl::SetStateImageList(wxImageList *imageList)
{
    if (m_ownsImageListState) delete m_imageListState;
    m_imageListState = imageList;
    m_ownsImageListState = false;
    m_dirty = true;

    if (m_anchor)
        m_anchor->RecursiveResetSize();

    // Don't do any drawing if we're setting the list to NULL,
    // since we may be in the process of deleting the tree control.
    if (imageList)
        CalculateLineHeight();
}

void wxGenericTreeCtrl::SetButtonsImageList(wxImageList *imageList)
{
    if (m_ownsImageListButtons) delete m_imageListButtons;
    m_imageListButtons = imageList;
    m_ownsImageListButtons = false;
    m_dirty = true;

    if (m_anchor)
        m_anchor->RecursiveResetSize();

    CalculateLineHeight();
}

void wxGenericTreeCtrl::AssignButtonsImageList(wxImageList *imageList)
{
    SetButtonsImageList(imageList);
    m_ownsImageListButtons = true;
}

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::AdjustMyScrollbars()
{
    if (m_anchor)
    {
        int x = 0, y = 0;
        m_anchor->GetSize( x, y, this );
        y += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        x += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        int x_pos = GetScrollPos( wxHORIZONTAL );
        int y_pos = GetScrollPos( wxVERTICAL );
        SetScrollbars( PIXELS_PER_UNIT, PIXELS_PER_UNIT,
                       x/PIXELS_PER_UNIT, y/PIXELS_PER_UNIT,
                       x_pos, y_pos );
    }
    else
    {
        SetScrollbars( 0, 0, 0, 0 );
    }
}

int wxGenericTreeCtrl::GetLineHeight(wxGenericTreeItem *item) const
{
    if (GetWindowStyleFlag() & wxTR_HAS_VARIABLE_ROW_HEIGHT)
        return item->GetHeight();
    else
        return m_lineHeight;
}

void wxGenericTreeCtrl::PaintItem(wxGenericTreeItem *item, wxDC& dc)
{
    item->SetFont(this, dc);
    item->CalculateSize(this, dc);

    wxCoord text_h = item->GetTextHeight();

    int image_h = 0, image_w = 0;
    int image = item->GetCurrentImage();
    if ( image != NO_IMAGE )
    {
        if ( m_imageListNormal )
        {
            m_imageListNormal->GetSize(image, image_w, image_h);
            image_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
        }
        else
        {
            image = NO_IMAGE;
        }
    }

    int state_h = 0, state_w = 0;
    int state = item->GetState();
    if ( state != wxTREE_ITEMSTATE_NONE )
    {
        if ( m_imageListState )
        {
            m_imageListState->GetSize(state, state_w, state_h);
            if ( image_w != 0 )
                state_w += MARGIN_BETWEEN_STATE_AND_IMAGE;
            else
                state_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
        }
        else
        {
            state = wxTREE_ITEMSTATE_NONE;
        }
    }

    int total_h = GetLineHeight(item);
    bool drawItemBackground = false,
         hasBgColour = false;

    if ( item->IsSelected() )
    {
        dc.SetBrush(*(m_hasFocus ? m_hilightBrush : m_hilightUnfocusedBrush));
        drawItemBackground = true;
    }
    else
    {
        wxColour colBg;
        wxTreeItemAttr * const attr = item->GetAttributes();
        if ( attr && attr->HasBackgroundColour() )
        {
            drawItemBackground =
            hasBgColour = true;
            colBg = attr->GetBackgroundColour();
        }
        else
        {
            colBg = GetBackgroundColour();
        }
        dc.SetBrush(wxBrush(colBg, wxBRUSHSTYLE_SOLID));
    }

    int offset = HasFlag(wxTR_ROW_LINES) ? 1 : 0;

    if ( HasFlag(wxTR_FULL_ROW_HIGHLIGHT) )
    {
        int x, w, h;
        x=0;
        GetVirtualSize(&w, &h);
        wxRect rect( x, item->GetY()+offset, w, total_h-offset);
        if (!item->IsSelected())
        {
            dc.DrawRectangle(rect);
        }
        else
        {
            int flags = wxCONTROL_SELECTED;
            if (m_hasFocus
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__) && wxOSX_USE_CARBON // TODO CS
                && IsControlActive( (ControlRef)GetHandle() )
#endif
            )
                flags |= wxCONTROL_FOCUSED;
            if ((item == m_current) && (m_hasFocus))
                flags |= wxCONTROL_CURRENT;

            wxRendererNative::Get().
                DrawItemSelectionRect(this, dc, rect, flags);
        }
    }
    else // no full row highlight
    {
        if ( item->IsSelected() &&
                (state != wxTREE_ITEMSTATE_NONE || image != NO_IMAGE) )
        {
            // If it's selected, and there's an state image or normal image,
            // then we should take care to leave the area under the image
            // painted in the background colour.
            wxRect rect( item->GetX() + state_w + image_w - 2,
                         item->GetY() + offset,
                         item->GetWidth() - state_w - image_w + 2,
                         total_h - offset );
#if !defined(__WXGTK20__) && !defined(__WXMAC__)
            dc.DrawRectangle( rect );
#else
            rect.x -= 1;
            rect.width += 2;

            int flags = wxCONTROL_SELECTED;
            if (m_hasFocus)
                flags |= wxCONTROL_FOCUSED;
            if ((item == m_current) && (m_hasFocus))
                flags |= wxCONTROL_CURRENT;
            wxRendererNative::Get().
                DrawItemSelectionRect(this, dc, rect, flags);
#endif
        }
        // On GTK+ 2, drawing a 'normal' background is wrong for themes that
        // don't allow backgrounds to be customized. Not drawing the background,
        // except for custom item backgrounds, works for both kinds of theme.
        else if (drawItemBackground)
        {
            wxRect rect( item->GetX() + state_w + image_w - 2,
                         item->GetY() + offset,
                         item->GetWidth() - state_w - image_w + 2,
                         total_h - offset );
            if ( hasBgColour )
            {
                dc.DrawRectangle( rect );
            }
            else // no specific background colour
            {
                rect.x -= 1;
                rect.width += 2;

                int flags = wxCONTROL_SELECTED;
                if (m_hasFocus)
                    flags |= wxCONTROL_FOCUSED;
                if ((item == m_current) && (m_hasFocus))
                    flags |= wxCONTROL_CURRENT;
                wxRendererNative::Get().
                    DrawItemSelectionRect(this, dc, rect, flags);
            }
        }
    }

    if ( state != wxTREE_ITEMSTATE_NONE )
    {
        dc.SetClippingRegion( item->GetX(), item->GetY(), state_w, total_h );
        m_imageListState->Draw( state, dc,
                                item->GetX(),
                                item->GetY() +
                                    (total_h > state_h ? (total_h-state_h)/2
                                                       : 0),
                                wxIMAGELIST_DRAW_TRANSPARENT );
        dc.DestroyClippingRegion();
    }

    if ( image != NO_IMAGE )
    {
        dc.SetClippingRegion(item->GetX() + state_w, item->GetY(),
                             image_w, total_h);
        m_imageListNormal->Draw( image, dc,
                                 item->GetX() + state_w,
                                 item->GetY() +
                                    (total_h > image_h ? (total_h-image_h)/2
                                                       : 0),
                                 wxIMAGELIST_DRAW_TRANSPARENT );
        dc.DestroyClippingRegion();
    }

    dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
    int extraH = (total_h > text_h) ? (total_h - text_h)/2 : 0;
    dc.DrawText( item->GetText(),
                 (wxCoord)(state_w + image_w + item->GetX()),
                 (wxCoord)(item->GetY() + extraH));

    // restore normal font
    dc.SetFont( m_normalFont );

    if (item == m_dndEffectItem)
    {
        dc.SetPen( *wxBLACK_PEN );
        // DnD visual effects
        switch (m_dndEffect)
        {
            case BorderEffect:
            {
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                int w = item->GetWidth() + 2;
                int h = total_h + 2;
                dc.DrawRectangle( item->GetX() - 1, item->GetY() - 1, w, h);
                break;
            }
            case AboveEffect:
            {
                int x = item->GetX(),
                    y = item->GetY();
                dc.DrawLine( x, y, x + item->GetWidth(), y);
                break;
            }
            case BelowEffect:
            {
                int x = item->GetX(),
                    y = item->GetY();
                y += total_h - 1;
                dc.DrawLine( x, y, x + item->GetWidth(), y);
                break;
            }
            case NoEffect:
                break;
        }
    }
}

void
wxGenericTreeCtrl::PaintLevel(wxGenericTreeItem *item,
                              wxDC &dc,
                              int level,
                              int &y)
{
    int x = level*m_indent;
    if (!HasFlag(wxTR_HIDE_ROOT))
    {
        x += m_indent;
    }
    else if (level == 0)
    {
        // always expand hidden root
        int origY = y;
        wxArrayGenericTreeItems& children = item->GetChildren();
        int count = children.GetCount();
        if (count > 0)
        {
            int n = 0, oldY;
            do {
                oldY = y;
                PaintLevel(children[n], dc, 1, y);
            } while (++n < count);

            if ( !HasFlag(wxTR_NO_LINES) && HasFlag(wxTR_LINES_AT_ROOT)
                    && count > 0 )
            {
                // draw line down to last child
                origY += GetLineHeight(children[0])>>1;
                oldY += GetLineHeight(children[n-1])>>1;
                dc.DrawLine(3, origY, 3, oldY);
            }
        }
        return;
    }

    item->SetX(x+m_spacing);
    item->SetY(y);

    int h = GetLineHeight(item);
    int y_top = y;
    int y_mid = y_top + (h>>1);
    y += h;

    int exposed_x = dc.LogicalToDeviceX(0);
    int exposed_y = dc.LogicalToDeviceY(y_top);

    if (IsExposed(exposed_x, exposed_y, 10000, h))  // 10000 = very much
    {
        const wxPen *pen =
#ifndef __WXMAC__
            // don't draw rect outline if we already have the
            // background color under Mac
            (item->IsSelected() && m_hasFocus) ? wxBLACK_PEN :
#endif // !__WXMAC__
            wxTRANSPARENT_PEN;

        wxColour colText;
        if ( item->IsSelected()
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__) && wxOSX_USE_CARBON // TODO CS
            // On wxMac, if the tree doesn't have the focus we draw an empty
            // rectangle, so we want to make sure that the text is visible
            // against the normal background, not the highlightbackground, so
            // don't use the highlight text colour unless we have the focus.
             && m_hasFocus && IsControlActive( (ControlRef)GetHandle() )
#endif
            )
        {
#ifdef __WXMAC__
            colText = *wxWHITE;
#else
            if (m_hasFocus)
                colText = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
            else
                colText = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT);
#endif
        }
        else
        {
            wxTreeItemAttr *attr = item->GetAttributes();
            if (attr && attr->HasTextColour())
                colText = attr->GetTextColour();
            else
                colText = GetForegroundColour();
        }

        // prepare to draw
        dc.SetTextForeground(colText);
        dc.SetPen(*pen);

        // draw
        PaintItem(item, dc);

        if (HasFlag(wxTR_ROW_LINES))
        {
            // if the background colour is white, choose a
            // contrasting color for the lines
            dc.SetPen(*((GetBackgroundColour() == *wxWHITE)
                         ? wxMEDIUM_GREY_PEN : wxWHITE_PEN));
            dc.DrawLine(0, y_top, 10000, y_top);
            dc.DrawLine(0, y, 10000, y);
        }

        // restore DC objects
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.SetPen(m_dottedPen);
        dc.SetTextForeground(*wxBLACK);

        if ( !HasFlag(wxTR_NO_LINES) )
        {
            // draw the horizontal line here
            int x_start = x;
            if (x > (signed)m_indent)
                x_start -= m_indent;
            else if (HasFlag(wxTR_LINES_AT_ROOT))
                x_start = 3;
            dc.DrawLine(x_start, y_mid, x + m_spacing, y_mid);
        }

        // should the item show a button?
        if ( item->HasPlus() && HasButtons() )
        {
            if ( m_imageListButtons )
            {
                // draw the image button here
                int image_h = 0,
                    image_w = 0;
                int image = item->IsExpanded() ? wxTreeItemIcon_Expanded
                                               : wxTreeItemIcon_Normal;
                if ( item->IsSelected() )
                    image += wxTreeItemIcon_Selected - wxTreeItemIcon_Normal;

                m_imageListButtons->GetSize(image, image_w, image_h);
                int xx = x - image_w/2;
                int yy = y_mid - image_h/2;

                wxDCClipper clip(dc, xx, yy, image_w, image_h);
                m_imageListButtons->Draw(image, dc, xx, yy,
                                         wxIMAGELIST_DRAW_TRANSPARENT);
            }
            else // no custom buttons
            {
                static const int wImage = 9;
                static const int hImage = 9;

                int flag = 0;
                if (item->IsExpanded())
                    flag |= wxCONTROL_EXPANDED;
                if (item == m_underMouse)
                    flag |= wxCONTROL_CURRENT;

                wxRendererNative::Get().DrawTreeItemButton
                                        (
                                            this,
                                            dc,
                                            wxRect(x - wImage/2,
                                                   y_mid - hImage/2,
                                                   wImage, hImage),
                                            flag
                                        );
            }
        }
    }

    if (item->IsExpanded())
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        int count = children.GetCount();
        if (count > 0)
        {
            int n = 0, oldY;
            ++level;
            do {
                oldY = y;
                PaintLevel(children[n], dc, level, y);
            } while (++n < count);

            if (!HasFlag(wxTR_NO_LINES) && count > 0)
            {
                // draw line down to last child
                oldY += GetLineHeight(children[n-1])>>1;
                if (HasButtons()) y_mid += 5;

                // Only draw the portion of the line that is visible, in case
                // it is huge
                wxCoord xOrigin=0, yOrigin=0, width, height;
                dc.GetDeviceOrigin(&xOrigin, &yOrigin);
                yOrigin = abs(yOrigin);
                GetClientSize(&width, &height);

                // Move end points to the beginning/end of the view?
                if (y_mid < yOrigin)
                    y_mid = yOrigin;
                if (oldY > yOrigin + height)
                    oldY = yOrigin + height;

                // after the adjustments if y_mid is larger than oldY then the
                // line isn't visible at all so don't draw anything
                if (y_mid < oldY)
                    dc.DrawLine(x, y_mid, x, oldY);
            }
        }
    }
}

void wxGenericTreeCtrl::DrawDropEffect(wxGenericTreeItem *item)
{
    if ( item )
    {
        if ( item->HasPlus() )
        {
            // it's a folder, indicate it by a border
            DrawBorder(item);
        }
        else
        {
            // draw a line under the drop target because the item will be
            // dropped there
            DrawLine(item, !m_dropEffectAboveItem );
        }

        SetCursor(*wxSTANDARD_CURSOR);
    }
    else
    {
        // can't drop here
        SetCursor(wxCURSOR_NO_ENTRY);
    }
}

void wxGenericTreeCtrl::DrawBorder(const wxTreeItemId &item)
{
    wxCHECK_RET( item.IsOk(), "invalid item in wxGenericTreeCtrl::DrawLine" );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    if (m_dndEffect == NoEffect)
    {
        m_dndEffect = BorderEffect;
        m_dndEffectItem = i;
    }
    else
    {
        m_dndEffect = NoEffect;
        m_dndEffectItem = NULL;
    }

    wxRect rect( i->GetX()-1, i->GetY()-1, i->GetWidth()+2, GetLineHeight(i)+2 );
    CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );
    RefreshRect( rect );
}

void wxGenericTreeCtrl::DrawLine(const wxTreeItemId &item, bool below)
{
    wxCHECK_RET( item.IsOk(), "invalid item in wxGenericTreeCtrl::DrawLine" );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    if (m_dndEffect == NoEffect)
    {
        if (below)
            m_dndEffect = BelowEffect;
        else
            m_dndEffect = AboveEffect;
        m_dndEffectItem = i;
    }
    else
    {
        m_dndEffect = NoEffect;
        m_dndEffectItem = NULL;
    }

    wxRect rect( i->GetX()-1, i->GetY()-1, i->GetWidth()+2, GetLineHeight(i)+2 );
    CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );
    RefreshRect( rect );
}

// -----------------------------------------------------------------------------
// wxWidgets callbacks
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::OnSize( wxSizeEvent &event )
{
#ifdef __WXGTK__
    if (HasFlag( wxTR_FULL_ROW_HIGHLIGHT) && m_current)
        RefreshLine( m_current );
#endif

    event.Skip(true);
}

void wxGenericTreeCtrl::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
    wxPaintDC dc(this);
    PrepareDC( dc );

    if ( !m_anchor)
        return;

    dc.SetFont( m_normalFont );
    dc.SetPen( m_dottedPen );

    // this is now done dynamically
    //if(GetImageList() == NULL)
    // m_lineHeight = (int)(dc.GetCharHeight() + 4);

    int y = 2;
    PaintLevel( m_anchor, dc, 0, y );
}

void wxGenericTreeCtrl::OnSetFocus( wxFocusEvent &event )
{
    m_hasFocus = true;

    RefreshSelected();

    event.Skip();
}

void wxGenericTreeCtrl::OnKillFocus( wxFocusEvent &event )
{
    m_hasFocus = false;

    RefreshSelected();

    event.Skip();
}

void wxGenericTreeCtrl::OnKeyDown( wxKeyEvent &event )
{
    // send a tree event
    wxTreeEvent te( wxEVT_TREE_KEY_DOWN, this);
    te.m_evtKey = event;
    if ( GetEventHandler()->ProcessEvent( te ) )
        return;

    event.Skip();
}

void wxGenericTreeCtrl::OnChar( wxKeyEvent &event )
{
    if ( (m_current == 0) || (m_key_current == 0) )
    {
        event.Skip();
        return;
    }

    // how should the selection work for this event?
    bool is_multiple, extended_select, unselect_others;
    EventFlagsToSelType(GetWindowStyleFlag(),
                        event.ShiftDown(),
                        event.CmdDown(),
                        is_multiple, extended_select, unselect_others);

    if (GetLayoutDirection() == wxLayout_RightToLeft)
    {
        if (event.GetKeyCode() == WXK_RIGHT)
            event.m_keyCode = WXK_LEFT;
        else if (event.GetKeyCode() == WXK_LEFT)
            event.m_keyCode = WXK_RIGHT;
    }

    // + : Expand
    // - : Collaspe
    // * : Expand all/Collapse all
    // ' ' | return : activate
    // up    : go up (not last children!)
    // down  : go down
    // left  : go to parent
    // right : open if parent and go next
    // home  : go to root
    // end   : go to last item without opening parents
    // alnum : start or continue searching for the item with this prefix
    int keyCode = event.GetKeyCode();

#ifdef __WXOSX__
    // Make the keys work as they do in the native control:
    // right => expand
    // left => collapse if current item is expanded
    if (keyCode == WXK_RIGHT)
    {
        keyCode = '+';
    }
    else if (keyCode == WXK_LEFT && IsExpanded(m_current))
    {
        keyCode = '-';
    }
#endif // __WXOSX__

    switch ( keyCode )
    {
        case '+':
        case WXK_ADD:
            if (m_current->HasPlus() && !IsExpanded(m_current))
            {
                Expand(m_current);
            }
            break;

        case '*':
        case WXK_MULTIPLY:
            if ( !IsExpanded(m_current) )
            {
                // expand all
                ExpandAllChildren(m_current);
                break;
            }
            //else: fall through to Collapse() it

        case '-':
        case WXK_SUBTRACT:
            if (IsExpanded(m_current))
            {
                Collapse(m_current);
            }
            break;

        case WXK_MENU:
            {
                // Use the item's bounding rectangle to determine position for
                // the event
                wxRect ItemRect;
                GetBoundingRect(m_current, ItemRect, true);

                wxTreeEvent
                    eventMenu(wxEVT_TREE_ITEM_MENU, this, m_current);
                // Use the left edge, vertical middle
                eventMenu.m_pointDrag = wxPoint(ItemRect.GetX(),
                                                ItemRect.GetY() +
                                                    ItemRect.GetHeight() / 2);
                GetEventHandler()->ProcessEvent( eventMenu );
            }
            break;

        case ' ':
        case WXK_RETURN:
            if ( !event.HasModifiers() )
            {
                wxTreeEvent
                   eventAct(wxEVT_TREE_ITEM_ACTIVATED, this, m_current);
                GetEventHandler()->ProcessEvent( eventAct );
            }

            // in any case, also generate the normal key event for this key,
            // even if we generated the ACTIVATED event above: this is what
            // wxMSW does and it makes sense because you might not want to
            // process ACTIVATED event at all and handle Space and Return
            // directly (and differently) which would be impossible otherwise
            event.Skip();
            break;

            // up goes to the previous sibling or to the last
            // of its children if it's expanded
        case WXK_UP:
            {
                wxTreeItemId prev = GetPrevSibling( m_key_current );
                if (!prev)
                {
                    prev = GetItemParent( m_key_current );
                    if ((prev == GetRootItem()) && HasFlag(wxTR_HIDE_ROOT))
                    {
                        break;  // don't go to root if it is hidden
                    }
                    if (prev)
                    {
                        wxTreeItemIdValue cookie;
                        wxTreeItemId current = m_key_current;
                        // TODO: Huh?  If we get here, we'd better be the first
                        // child of our parent.  How else could it be?
                        if (current == GetFirstChild( prev, cookie ))
                        {
                            // otherwise we return to where we came from
                            DoSelectItem(prev,
                                         unselect_others,
                                         extended_select);
                            m_key_current = (wxGenericTreeItem*) prev.m_pItem;
                            break;
                        }
                    }
                }
                if (prev)
                {
                    while ( IsExpanded(prev) && HasChildren(prev) )
                    {
                        wxTreeItemId child = GetLastChild(prev);
                        if ( child )
                        {
                            prev = child;
                        }
                    }

                    DoSelectItem( prev, unselect_others, extended_select );
                    m_key_current=(wxGenericTreeItem*) prev.m_pItem;
                }
            }
            break;

            // left arrow goes to the parent
        case WXK_LEFT:
            {
                wxTreeItemId prev = GetItemParent( m_current );
                if ((prev == GetRootItem()) && HasFlag(wxTR_HIDE_ROOT))
                {
                    // don't go to root if it is hidden
                    prev = GetPrevSibling( m_current );
                }
                if (prev)
                {
                    DoSelectItem( prev, unselect_others, extended_select );
                }
            }
            break;

        case WXK_RIGHT:
            // this works the same as the down arrow except that we
            // also expand the item if it wasn't expanded yet
            if (m_current != GetRootItem().m_pItem || !HasFlag(wxTR_HIDE_ROOT))
                Expand(m_current);
            //else: don't try to expand hidden root item (which can be the
            //      current one when the tree is empty)

            // fall through

        case WXK_DOWN:
            {
                if (IsExpanded(m_key_current) && HasChildren(m_key_current))
                {
                    wxTreeItemIdValue cookie;
                    wxTreeItemId child = GetFirstChild( m_key_current, cookie );
                    if ( !child )
                        break;

                    DoSelectItem( child, unselect_others, extended_select );
                    m_key_current=(wxGenericTreeItem*) child.m_pItem;
                }
                else
                {
                    wxTreeItemId next = GetNextSibling( m_key_current );
                    if (!next)
                    {
                        wxTreeItemId current = m_key_current;
                        while (current.IsOk() && !next)
                        {
                            current = GetItemParent( current );
                            if (current) next = GetNextSibling( current );
                        }
                    }
                    if (next)
                    {
                        DoSelectItem( next, unselect_others, extended_select );
                        m_key_current=(wxGenericTreeItem*) next.m_pItem;
                    }
                }
            }
            break;

            // <End> selects the last visible tree item
        case WXK_END:
            {
                wxTreeItemId last = GetRootItem();

                while ( last.IsOk() && IsExpanded(last) )
                {
                    wxTreeItemId lastChild = GetLastChild(last);

                    // it may happen if the item was expanded but then all of
                    // its children have been deleted - so IsExpanded() returned
                    // true, but GetLastChild() returned invalid item
                    if ( !lastChild )
                        break;

                    last = lastChild;
                }

                if ( last.IsOk() )
                {
                    DoSelectItem( last, unselect_others, extended_select );
                }
            }
            break;

            // <Home> selects the root item
        case WXK_HOME:
            {
                wxTreeItemId prev = GetRootItem();
                if (!prev)
                    break;

                if ( HasFlag(wxTR_HIDE_ROOT) )
                {
                    wxTreeItemIdValue cookie;
                    prev = GetFirstChild(prev, cookie);
                    if (!prev)
                        break;
                }

                DoSelectItem( prev, unselect_others, extended_select );
            }
            break;

        default:
            // do not use wxIsalnum() here
            if ( !event.HasModifiers() &&
                 ((keyCode >= '0' && keyCode <= '9') ||
                  (keyCode >= 'a' && keyCode <= 'z') ||
                  (keyCode >= 'A' && keyCode <= 'Z') ||
                  (keyCode == '_')))
            {
                // find the next item starting with the given prefix
                wxChar ch = (wxChar)keyCode;
                wxTreeItemId id;

                // if the same character is typed multiple times then go to the
                // next entry starting with that character instead of searching
                // for an item starting with multiple copies of this character,
                // this is more useful and is how it works under Windows.
                if ( m_findPrefix.length() == 1 && m_findPrefix[0] == ch )
                {
                    id = FindItem(m_current, ch);
                }
                else
                {
                    const wxString newPrefix(m_findPrefix + ch);
                    id = FindItem(m_current, newPrefix);
                    if ( id.IsOk() )
                        m_findPrefix = newPrefix;
                }

                // also start the timer to reset the current prefix if the user
                // doesn't press any more alnum keys soon -- we wouldn't want
                // to use this prefix for a new item search
                if ( !m_findTimer )
                {
                    m_findTimer = new wxTreeFindTimer(this);
                }

                // Notice that we should start the timer even if we didn't find
                // anything to make sure we reset the search state later.
                m_findTimer->Start(wxTreeFindTimer::DELAY, wxTIMER_ONE_SHOT);

                if ( id.IsOk() )
                {
                    SelectItem(id);

                    // Reset the bell flag if it had been temporarily disabled
                    // before.
                    if ( m_findBell )
                        m_findBell = 1;
                }
                else // No such item
                {
                    // Signal it with a bell if enabled.
                    if ( m_findBell == 1 )
                    {
                        ::wxBell();

                        // Disable it for the next unsuccessful match, we only
                        // beep once, this is usually enough and continuing to
                        // do it would be annoying.
                        m_findBell = -1;
                    }
                }
            }
            else
            {
                event.Skip();
            }
    }
}

wxTreeItemId
wxGenericTreeCtrl::DoTreeHitTest(const wxPoint& point, int& flags) const
{
    int w, h;
    GetSize(&w, &h);
    flags=0;
    if (point.x<0) flags |= wxTREE_HITTEST_TOLEFT;
    if (point.x>w) flags |= wxTREE_HITTEST_TORIGHT;
    if (point.y<0) flags |= wxTREE_HITTEST_ABOVE;
    if (point.y>h) flags |= wxTREE_HITTEST_BELOW;
    if (flags) return wxTreeItemId();

    if (m_anchor == NULL)
    {
        flags = wxTREE_HITTEST_NOWHERE;
        return wxTreeItemId();
    }

    wxGenericTreeItem *hit =  m_anchor->HitTest(CalcUnscrolledPosition(point),
                                                this, flags, 0);
    if (hit == NULL)
    {
        flags = wxTREE_HITTEST_NOWHERE;
        return wxTreeItemId();
    }
    return hit;
}

// get the bounding rectangle of the item (or of its label only)
bool wxGenericTreeCtrl::GetBoundingRect(const wxTreeItemId& item,
                                        wxRect& rect,
                                        bool textOnly) const
{
    wxCHECK_MSG( item.IsOk(), false,
                 "invalid item in wxGenericTreeCtrl::GetBoundingRect" );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    if ( textOnly )
    {
        int image_h = 0, image_w = 0;
        int image = ((wxGenericTreeItem*) item.m_pItem)->GetCurrentImage();
        if ( image != NO_IMAGE && m_imageListNormal )
        {
            m_imageListNormal->GetSize( image, image_w, image_h );
            image_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
        }

        int state_h = 0, state_w = 0;
        int state = ((wxGenericTreeItem*) item.m_pItem)->GetState();
        if ( state != wxTREE_ITEMSTATE_NONE && m_imageListState )
        {
            m_imageListState->GetSize( state, state_w, state_h );
            if ( image_w != 0 )
                state_w += MARGIN_BETWEEN_STATE_AND_IMAGE;
            else
                state_w += MARGIN_BETWEEN_IMAGE_AND_TEXT;
        }

        rect.x = i->GetX() + state_w + image_w;
        rect.width = i->GetWidth() - state_w - image_w;

    }
    else // the entire line
    {
        rect.x = 0;
        rect.width = GetClientSize().x;
    }

    rect.y = i->GetY();
    rect.height = GetLineHeight(i);

    // we have to return the logical coordinates, not physical ones
    rect.SetTopLeft(CalcScrolledPosition(rect.GetTopLeft()));

    return true;
}

wxTextCtrl *wxGenericTreeCtrl::EditLabel(const wxTreeItemId& item,
                                  wxClassInfo * WXUNUSED(textCtrlClass))
{
    wxCHECK_MSG( item.IsOk(), NULL, wxT("can't edit an invalid item") );

    wxGenericTreeItem *itemEdit = (wxGenericTreeItem *)item.m_pItem;

    wxTreeEvent te(wxEVT_TREE_BEGIN_LABEL_EDIT, this, itemEdit);
    if ( GetEventHandler()->ProcessEvent( te ) && !te.IsAllowed() )
    {
        // vetoed by user
        return NULL;
    }

    // We have to call this here because the label in
    // question might just have been added and no screen
    // update taken place.
    if ( m_dirty )
        DoDirtyProcessing();

    // TODO: use textCtrlClass here to create the control of correct class
    m_textCtrl = new wxTreeTextCtrl(this, itemEdit);

    m_textCtrl->SetFocus();

    return m_textCtrl;
}

// returns a pointer to the text edit control if the item is being
// edited, NULL otherwise (it's assumed that no more than one item may
// be edited simultaneously)
wxTextCtrl* wxGenericTreeCtrl::GetEditControl() const
{
    return m_textCtrl;
}

void wxGenericTreeCtrl::EndEditLabel(const wxTreeItemId& WXUNUSED(item),
                                     bool discardChanges)
{
    if (m_textCtrl)
    {
        m_textCtrl->EndEdit(discardChanges);
    }
}

bool wxGenericTreeCtrl::OnRenameAccept(wxGenericTreeItem *item,
                                       const wxString& value)
{
    wxTreeEvent le(wxEVT_TREE_END_LABEL_EDIT, this, item);
    le.m_label = value;
    le.m_editCancelled = false;

    return !GetEventHandler()->ProcessEvent( le ) || le.IsAllowed();
}

void wxGenericTreeCtrl::OnRenameCancelled(wxGenericTreeItem *item)
{
    // let owner know that the edit was cancelled
    wxTreeEvent le(wxEVT_TREE_END_LABEL_EDIT, this, item);
    le.m_label = wxEmptyString;
    le.m_editCancelled = true;

    GetEventHandler()->ProcessEvent( le );
}

void wxGenericTreeCtrl::OnRenameTimer()
{
    EditLabel( m_current );
}

void wxGenericTreeCtrl::OnMouse( wxMouseEvent &event )
{
    if ( !m_anchor )return;

    wxPoint pt = CalcUnscrolledPosition(event.GetPosition());

    // Is the mouse over a tree item button?
    int flags = 0;
    wxGenericTreeItem *thisItem = m_anchor->HitTest(pt, this, flags, 0);
    wxGenericTreeItem *underMouse = thisItem;
#if wxUSE_TOOLTIPS
    bool underMouseChanged = (underMouse != m_underMouse) ;
#endif // wxUSE_TOOLTIPS

    if ((underMouse) &&
        (flags & wxTREE_HITTEST_ONITEMBUTTON) &&
        (!event.LeftIsDown()) &&
        (!m_isDragging) &&
        (!m_renameTimer || !m_renameTimer->IsRunning()))
    {
    }
    else
    {
        underMouse = NULL;
    }

    if (underMouse != m_underMouse)
    {
         if (m_underMouse)
         {
            // unhighlight old item
            wxGenericTreeItem *tmp = m_underMouse;
            m_underMouse = NULL;
            RefreshLine( tmp );
         }

         m_underMouse = underMouse;
         if (m_underMouse)
            RefreshLine( m_underMouse );
    }

#if wxUSE_TOOLTIPS
    // Determines what item we are hovering over and need a tooltip for
    wxTreeItemId hoverItem = thisItem;

    // We do not want a tooltip if we are dragging, or if the rename timer is
    // running
    if ( underMouseChanged &&
            hoverItem.IsOk() &&
              !m_isDragging &&
                (!m_renameTimer || !m_renameTimer->IsRunning()) )
    {
        // Ask the tree control what tooltip (if any) should be shown
        wxTreeEvent
            hevent(wxEVT_TREE_ITEM_GETTOOLTIP,  this, hoverItem);

        // setting a tooltip upon leaving a view is getting the tooltip displayed
        // on the neighbouring view ...
#ifdef __WXOSX__
        if ( event.Leaving() )
            SetToolTip(NULL);
        else
#endif
        if ( GetEventHandler()->ProcessEvent(hevent) )
        {
            // If the user permitted the tooltip change, update it, otherwise
            // remove any old tooltip we might have.
            if ( hevent.IsAllowed() )
                SetToolTip(hevent.m_label);
            else
                SetToolTip(NULL);
        }
    }
#endif

    // we process left mouse up event (enables in-place edit), middle/right down
    // (pass to the user code), left dbl click (activate item) and
    // dragging/moving events for items drag-and-drop
    if ( !(event.LeftDown() ||
           event.LeftUp() ||
           event.MiddleDown() ||
           event.RightDown() ||
           event.LeftDClick() ||
           event.Dragging() ||
           ((event.Moving() || event.RightUp()) && m_isDragging)) )
    {
        event.Skip();

        return;
    }


    flags = 0;
    wxGenericTreeItem *item = m_anchor->HitTest(pt, this, flags, 0);

    if ( event.Dragging() && !m_isDragging )
    {
        if (m_dragCount == 0)
            m_dragStart = pt;

        m_dragCount++;

        if (m_dragCount != 3)
        {
            // wait until user drags a bit further...
            return;
        }

        wxEventType command = event.RightIsDown()
                              ? wxEVT_TREE_BEGIN_RDRAG
                              : wxEVT_TREE_BEGIN_DRAG;

        wxTreeEvent nevent(command,  this, m_current);
        nevent.SetPoint(CalcScrolledPosition(pt));

        // by default the dragging is not supported, the user code must
        // explicitly allow the event for it to take place
        nevent.Veto();

        if ( GetEventHandler()->ProcessEvent(nevent) && nevent.IsAllowed() )
        {
            // we're going to drag this item
            m_isDragging = true;

            // remember the old cursor because we will change it while
            // dragging
            m_oldCursor = m_cursor;

            // in a single selection control, hide the selection temporarily
            if ( !(GetWindowStyleFlag() & wxTR_MULTIPLE) )
            {
                m_oldSelection = (wxGenericTreeItem*) GetSelection().m_pItem;

                if ( m_oldSelection )
                {
                    m_oldSelection->SetHilight(false);
                    RefreshLine(m_oldSelection);
                }
            }

            CaptureMouse();
        }
    }
    else if ( event.Dragging() )
    {
        if ( item != m_dropTarget )
        {
            // unhighlight the previous drop target
            DrawDropEffect(m_dropTarget);

            m_dropTarget = item;

            // highlight the current drop target if any
            DrawDropEffect(m_dropTarget);

#if defined(__WXMSW__) || defined(__WXMAC__) || defined(__WXGTK20__)
            Update();
#else
            // TODO: remove this call or use wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI)
            //       instead (needs to be tested!)
            wxYieldIfNeeded();
#endif
        }
    }
    else if ( (event.LeftUp() || event.RightUp()) && m_isDragging )
    {
        ReleaseMouse();

        // erase the highlighting
        DrawDropEffect(m_dropTarget);

        if ( m_oldSelection )
        {
            m_oldSelection->SetHilight(true);
            RefreshLine(m_oldSelection);
            m_oldSelection = NULL;
        }

        // generate the drag end event
        wxTreeEvent eventEndDrag(wxEVT_TREE_END_DRAG,  this, item);

        eventEndDrag.m_pointDrag = CalcScrolledPosition(pt);

        (void)GetEventHandler()->ProcessEvent(eventEndDrag);

        m_isDragging = false;
        m_dropTarget = NULL;

        SetCursor(m_oldCursor);

#if defined( __WXMSW__ ) || defined(__WXMAC__) || defined(__WXGTK20__)
        Update();
#else
        // TODO: remove this call or use wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI)
        //       instead (needs to be tested!)
        wxYieldIfNeeded();
#endif
    }
    else
    {
        // If we got to this point, we are not dragging or moving the mouse.
        // Because the code in carbon/toplevel.cpp will only set focus to the
        // tree if we skip for EVT_LEFT_DOWN, we MUST skip this event here for
        // focus to work.
        // We skip even if we didn't hit an item because we still should
        // restore focus to the tree control even if we didn't exactly hit an
        // item.
        if ( event.LeftDown() )
        {
            event.Skip();
        }

        // here we process only the messages which happen on tree items

        m_dragCount = 0;

        if (item == NULL) return;  /* we hit the blank area */

        if ( event.RightDown() )
        {
            // If the item is already selected, do not update the selection.
            // Multi-selections should not be cleared if a selected item is
            // clicked.
            if (!IsSelected(item))
            {
                DoSelectItem(item, true, false);
            }

            wxTreeEvent
                nevent(wxEVT_TREE_ITEM_RIGHT_CLICK,  this, item);
            nevent.m_pointDrag = CalcScrolledPosition(pt);
            event.Skip(!GetEventHandler()->ProcessEvent(nevent));

            // Consistent with MSW (for now), send the ITEM_MENU *after*
            // the RIGHT_CLICK event. TODO: This behaviour may change.
            wxTreeEvent nevent2(wxEVT_TREE_ITEM_MENU,  this, item);
            nevent2.m_pointDrag = CalcScrolledPosition(pt);
            GetEventHandler()->ProcessEvent(nevent2);
        }
        else if ( event.MiddleDown() )
        {
            wxTreeEvent
                nevent(wxEVT_TREE_ITEM_MIDDLE_CLICK,  this, item);
            nevent.m_pointDrag = CalcScrolledPosition(pt);
            event.Skip(!GetEventHandler()->ProcessEvent(nevent));
        }
        else if ( event.LeftUp() )
        {
            if (flags & wxTREE_HITTEST_ONITEMSTATEICON)
            {
                wxTreeEvent
                    nevent(wxEVT_TREE_STATE_IMAGE_CLICK, this, item);
                GetEventHandler()->ProcessEvent(nevent);
            }

            // this facilitates multiple-item drag-and-drop

            if ( /* item && */ HasFlag(wxTR_MULTIPLE))
            {
                wxArrayTreeItemIds selections;
                size_t count = GetSelections(selections);

                if (count > 1 &&
                    !event.CmdDown() &&
                    !event.ShiftDown())
                {
                    DoSelectItem(item, true, false);
                }
            }

            if ( m_lastOnSame )
            {
                if ( (item == m_current) &&
                     (flags & wxTREE_HITTEST_ONITEMLABEL) &&
                     HasFlag(wxTR_EDIT_LABELS) )
                {
                    if ( m_renameTimer )
                    {
                        if ( m_renameTimer->IsRunning() )
                            m_renameTimer->Stop();
                    }
                    else
                    {
                        m_renameTimer = new wxTreeRenameTimer( this );
                    }

                    m_renameTimer->Start( wxTreeRenameTimer::DELAY, true );
                }

                m_lastOnSame = false;
            }
        }
        else // !RightDown() && !MiddleDown() && !LeftUp()
        {
            // ==> LeftDown() || LeftDClick()
            if ( event.LeftDown() )
            {
                // If we click on an already selected item but do it to return
                // the focus to the control, it shouldn't start editing the
                // item label because it's too easy to start editing
                // accidentally (and also because nobody else does it like
                // this). So only set this flag, used to decide whether we
                // should start editing the label later, if we already have
                // focus.
                m_lastOnSame = item == m_current && HasFocus();
            }

            if ( flags & wxTREE_HITTEST_ONITEMBUTTON )
            {
                // only toggle the item for a single click, double click on
                // the button doesn't do anything (it toggles the item twice)
                if ( event.LeftDown() )
                {
                    Toggle( item );
                }

                // don't select the item if the button was clicked
                return;
            }


            // clear the previously selected items, if the
            // user clicked outside of the present selection.
            // otherwise, perform the deselection on mouse-up.
            // this allows multiple drag and drop to work.
            // but if Cmd is down, toggle selection of the clicked item
            if (!IsSelected(item) || event.CmdDown())
            {
                // how should the selection work for this event?
                bool is_multiple, extended_select, unselect_others;
                EventFlagsToSelType(GetWindowStyleFlag(),
                                    event.ShiftDown(),
                                    event.CmdDown(),
                                    is_multiple,
                                    extended_select,
                                    unselect_others);

                DoSelectItem(item, unselect_others, extended_select);
            }


            // For some reason, Windows isn't recognizing a left double-click,
            // so we need to simulate it here.  Allow 200 milliseconds for now.
            if ( event.LeftDClick() )
            {
                // double clicking should not start editing the item label
                if ( m_renameTimer )
                    m_renameTimer->Stop();

                m_lastOnSame = false;

                // send activate event first
                wxTreeEvent
                    nevent(wxEVT_TREE_ITEM_ACTIVATED,  this, item);
                nevent.m_pointDrag = CalcScrolledPosition(pt);
                if ( !GetEventHandler()->ProcessEvent( nevent ) )
                {
                    // if the user code didn't process the activate event,
                    // handle it ourselves by toggling the item when it is
                    // double clicked
                    if ( item->HasPlus() )
                    {
                        Toggle(item);
                    }
                }
            }
        }
    }
}

void wxGenericTreeCtrl::OnInternalIdle()
{
    wxWindow::OnInternalIdle();

    // Check if we need to select the root item
    // because nothing else has been selected.
    // Delaying it means that we can invoke event handlers
    // as required, when a first item is selected.
    if (!HasFlag(wxTR_MULTIPLE) && !GetSelection().IsOk())
    {
        if (m_select_me)
            SelectItem(m_select_me);
        else if (GetRootItem().IsOk())
            SelectItem(GetRootItem());
    }

    // after all changes have been done to the tree control,
    // actually redraw the tree when everything is over
    if (m_dirty)
        DoDirtyProcessing();
}

void
wxGenericTreeCtrl::CalculateLevel(wxGenericTreeItem *item,
                                  wxDC &dc,
                                  int level,
                                  int &y )
{
    int x = level*m_indent;
    if (!HasFlag(wxTR_HIDE_ROOT))
    {
        x += m_indent;
    }
    else if (level == 0)
    {
        // a hidden root is not evaluated, but its
        // children are always calculated
        goto Recurse;
    }

    item->CalculateSize(this, dc);

    // set its position
    item->SetX( x+m_spacing );
    item->SetY( y );
    y += GetLineHeight(item);

    if ( !item->IsExpanded() )
    {
        // we don't need to calculate collapsed branches
        return;
    }

  Recurse:
    wxArrayGenericTreeItems& children = item->GetChildren();
    size_t n, count = children.GetCount();
    ++level;
    for (n = 0; n < count; ++n )
        CalculateLevel( children[n], dc, level, y );  // recurse
}

void wxGenericTreeCtrl::CalculatePositions()
{
    if ( !m_anchor ) return;

    wxClientDC dc(this);
    PrepareDC( dc );

    dc.SetFont( m_normalFont );

    dc.SetPen( m_dottedPen );
    //if(GetImageList() == NULL)
    // m_lineHeight = (int)(dc.GetCharHeight() + 4);

    int y = 2;
    CalculateLevel( m_anchor, dc, 0, y ); // start recursion
}

void wxGenericTreeCtrl::Refresh(bool eraseBackground, const wxRect *rect)
{
    if ( !IsFrozen() )
        wxTreeCtrlBase::Refresh(eraseBackground, rect);
}

void wxGenericTreeCtrl::RefreshSubtree(wxGenericTreeItem *item)
{
    if (m_dirty || IsFrozen() )
        return;

    wxSize client = GetClientSize();

    wxRect rect;
    CalcScrolledPosition(0, item->GetY(), NULL, &rect.y);
    rect.width = client.x;
    rect.height = client.y;

    Refresh(true, &rect);

    AdjustMyScrollbars();
}

void wxGenericTreeCtrl::RefreshLine( wxGenericTreeItem *item )
{
    if (m_dirty || IsFrozen() )
        return;

    wxRect rect;
    CalcScrolledPosition(0, item->GetY(), NULL, &rect.y);
    rect.width = GetClientSize().x;
    rect.height = GetLineHeight(item); //dc.GetCharHeight() + 6;

    Refresh(true, &rect);
}

void wxGenericTreeCtrl::RefreshSelected()
{
    if (IsFrozen())
        return;

    // TODO: this is awfully inefficient, we should keep the list of all
    //       selected items internally, should be much faster
    if ( m_anchor )
        RefreshSelectedUnder(m_anchor);
}

void wxGenericTreeCtrl::RefreshSelectedUnder(wxGenericTreeItem *item)
{
    if (IsFrozen())
        return;

    if ( item->IsSelected() )
        RefreshLine(item);

    const wxArrayGenericTreeItems& children = item->GetChildren();
    size_t count = children.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        RefreshSelectedUnder(children[n]);
    }
}

void wxGenericTreeCtrl::DoThaw()
{
    wxTreeCtrlBase::DoThaw();

    if ( m_dirty )
        DoDirtyProcessing();
    else
        Refresh();
}

// ----------------------------------------------------------------------------
// changing colours: we need to refresh the tree control
// ----------------------------------------------------------------------------

bool wxGenericTreeCtrl::SetBackgroundColour(const wxColour& colour)
{
    if ( !wxWindow::SetBackgroundColour(colour) )
        return false;

    Refresh();

    return true;
}

bool wxGenericTreeCtrl::SetForegroundColour(const wxColour& colour)
{
    if ( !wxWindow::SetForegroundColour(colour) )
        return false;

    Refresh();

    return true;
}

void wxGenericTreeCtrl::OnGetToolTip( wxTreeEvent &event )
{
#if wxUSE_TOOLTIPS
    wxTreeItemId itemId = event.GetItem();
    const wxGenericTreeItem* const pItem = (wxGenericTreeItem*)itemId.m_pItem;

    // Check if the item fits into the client area:
    if ( pItem->GetX() + pItem->GetWidth() > GetClientSize().x )
    {
        // If it doesn't, show its full text in the tooltip.
        event.SetLabel(pItem->GetText());
    }
    else
#endif // wxUSE_TOOLTIPS
    {
        // veto processing the event, nixing any tooltip
        event.Veto();
    }
}


// NOTE: If using the wxListBox visual attributes works everywhere then this can
// be removed, as well as the #else case below.
#define _USE_VISATTR 0

//static
wxVisualAttributes
#if _USE_VISATTR
wxGenericTreeCtrl::GetClassDefaultAttributes(wxWindowVariant variant)
#else
wxGenericTreeCtrl::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
#endif
{
#if _USE_VISATTR
    // Use the same color scheme as wxListBox
    return wxListBox::GetClassDefaultAttributes(variant);
#else
    wxVisualAttributes attr;
    attr.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
    attr.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
    attr.font  = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    return attr;
#endif
}

void wxGenericTreeCtrl::DoDirtyProcessing()
{
    if (IsFrozen())
        return;

    m_dirty = false;

    CalculatePositions();
    Refresh();
    AdjustMyScrollbars();
}

wxSize wxGenericTreeCtrl::DoGetBestSize() const
{
    // make sure all positions are calculated as normally this only done during
    // idle time but we need them for base class DoGetBestSize() to return the
    // correct result
    wxConstCast(this, wxGenericTreeCtrl)->CalculatePositions();

    wxSize size = wxTreeCtrlBase::DoGetBestSize();

    // there seems to be an implicit extra border around the items, although
    // I'm not really sure where does it come from -- but without this, the
    // scrollbars appear in a tree with default/best size
    size.IncBy(4, 4);

    // and the border has to be rounded up to a multiple of PIXELS_PER_UNIT or
    // scrollbars still appear
    const wxSize& borderSize = GetWindowBorderSize();

    int dx = (size.x - borderSize.x) % PIXELS_PER_UNIT;
    if ( dx )
        size.x += PIXELS_PER_UNIT - dx;
    int dy = (size.y - borderSize.y) % PIXELS_PER_UNIT;
    if ( dy )
        size.y += PIXELS_PER_UNIT - dy;

    // we need to update the cache too as the base class cached its own value
    CacheBestSize(size);

    return size;
}

#endif // wxUSE_TREECTRL
