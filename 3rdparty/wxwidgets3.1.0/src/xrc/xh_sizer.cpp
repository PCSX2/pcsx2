/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_sizer.cpp
// Purpose:     XRC resource for wxBoxSizer
// Author:      Vaclav Slavik
// Created:     2000/03/21
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC

#include "wx/xrc/xh_sizer.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/panel.h"
    #include "wx/statbox.h"
    #include "wx/sizer.h"
    #include "wx/frame.h"
    #include "wx/dialog.h"
    #include "wx/button.h"
    #include "wx/scrolwin.h"
#endif

#include "wx/gbsizer.h"
#include "wx/wrapsizer.h"
#include "wx/notebook.h"
#include "wx/tokenzr.h"

#include "wx/xml/xml.h"

//-----------------------------------------------------------------------------
// wxSizerXmlHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxSizerXmlHandler, wxXmlResourceHandler);

wxSizerXmlHandler::wxSizerXmlHandler()
                  :wxXmlResourceHandler(),
                   m_isInside(false),
                   m_isGBS(false),
                   m_parentSizer(NULL)
{
    XRC_ADD_STYLE(wxHORIZONTAL);
    XRC_ADD_STYLE(wxVERTICAL);

    // and flags
    XRC_ADD_STYLE(wxLEFT);
    XRC_ADD_STYLE(wxRIGHT);
    XRC_ADD_STYLE(wxTOP);
    XRC_ADD_STYLE(wxBOTTOM);
    XRC_ADD_STYLE(wxNORTH);
    XRC_ADD_STYLE(wxSOUTH);
    XRC_ADD_STYLE(wxEAST);
    XRC_ADD_STYLE(wxWEST);
    XRC_ADD_STYLE(wxALL);

    XRC_ADD_STYLE(wxGROW);
    XRC_ADD_STYLE(wxEXPAND);
    XRC_ADD_STYLE(wxSHAPED);
    XRC_ADD_STYLE(wxSTRETCH_NOT);

    XRC_ADD_STYLE(wxALIGN_CENTER);
    XRC_ADD_STYLE(wxALIGN_CENTRE);
    XRC_ADD_STYLE(wxALIGN_LEFT);
    XRC_ADD_STYLE(wxALIGN_TOP);
    XRC_ADD_STYLE(wxALIGN_RIGHT);
    XRC_ADD_STYLE(wxALIGN_BOTTOM);
    XRC_ADD_STYLE(wxALIGN_CENTER_HORIZONTAL);
    XRC_ADD_STYLE(wxALIGN_CENTRE_HORIZONTAL);
    XRC_ADD_STYLE(wxALIGN_CENTER_VERTICAL);
    XRC_ADD_STYLE(wxALIGN_CENTRE_VERTICAL);

    XRC_ADD_STYLE(wxFIXED_MINSIZE);
    XRC_ADD_STYLE(wxRESERVE_SPACE_EVEN_IF_HIDDEN);

    // this flag doesn't do anything any more but we can just ignore its
    // occurrences in the old resource files instead of raising a fuss because
    // of it
    AddStyle("wxADJUST_MINSIZE", 0);

    // wxWrapSizer-specific flags
    XRC_ADD_STYLE(wxEXTEND_LAST_ON_EACH_LINE);
    XRC_ADD_STYLE(wxREMOVE_LEADING_SPACES);
}



bool wxSizerXmlHandler::CanHandle(wxXmlNode *node)
{
    return ( (!m_isInside && IsSizerNode(node)) ||
             (m_isInside && IsOfClass(node, wxT("sizeritem"))) ||
             (m_isInside && IsOfClass(node, wxT("spacer")))
        );
}


wxObject* wxSizerXmlHandler::DoCreateResource()
{
    if (m_class == wxT("sizeritem"))
        return Handle_sizeritem();

    else if (m_class == wxT("spacer"))
        return Handle_spacer();

    else
        return Handle_sizer();
}


wxSizer* wxSizerXmlHandler::DoCreateSizer(const wxString& name)
{
    if (name == wxT("wxBoxSizer"))
        return Handle_wxBoxSizer();
#if wxUSE_STATBOX
    else if (name == wxT("wxStaticBoxSizer"))
        return Handle_wxStaticBoxSizer();
#endif
    else if (name == wxT("wxGridSizer"))
    {
        if ( !ValidateGridSizerChildren() )
            return NULL;
        return Handle_wxGridSizer();
    }
    else if (name == wxT("wxFlexGridSizer"))
    {
        return Handle_wxFlexGridSizer();
    }
    else if (name == wxT("wxGridBagSizer"))
    {
        return Handle_wxGridBagSizer();
    }
    else if (name == wxT("wxWrapSizer"))
    {
        return Handle_wxWrapSizer();
    }

    ReportError(wxString::Format("unknown sizer class \"%s\"", name));
    return NULL;
}



bool wxSizerXmlHandler::IsSizerNode(wxXmlNode *node) const
{
    return (IsOfClass(node, wxT("wxBoxSizer"))) ||
           (IsOfClass(node, wxT("wxStaticBoxSizer"))) ||
           (IsOfClass(node, wxT("wxGridSizer"))) ||
           (IsOfClass(node, wxT("wxFlexGridSizer"))) ||
           (IsOfClass(node, wxT("wxGridBagSizer"))) ||
           (IsOfClass(node, wxT("wxWrapSizer")));
}


wxObject* wxSizerXmlHandler::Handle_sizeritem()
{
    // find the item to be managed by this sizeritem
    wxXmlNode *n = GetParamNode(wxT("object"));
    if ( !n )
        n = GetParamNode(wxT("object_ref"));

    // did we find one?
    if (n)
    {
        // create a sizer item for it
        wxSizerItem* sitem = MakeSizerItem();

        // now fetch the item to be managed
        bool old_gbs = m_isGBS;
        bool old_ins = m_isInside;
        wxSizer *old_par = m_parentSizer;
        m_isInside = false;
        if (!IsSizerNode(n)) m_parentSizer = NULL;
        wxObject *item = CreateResFromNode(n, m_parent, NULL);
        m_isInside = old_ins;
        m_parentSizer = old_par;
        m_isGBS = old_gbs;

        // and figure out what type it is
        wxSizer *sizer = wxDynamicCast(item, wxSizer);
        wxWindow *wnd = wxDynamicCast(item, wxWindow);

        if (sizer)
            sitem->AssignSizer(sizer);
        else if (wnd)
            sitem->AssignWindow(wnd);
        else
            ReportError(n, "unexpected item in sizer");

        // finally, set other wxSizerItem attributes
        SetSizerItemAttributes(sitem);

        AddSizerItem(sitem);
        return item;
    }
    else /*n == NULL*/
    {
        ReportError("no window/sizer/spacer within sizeritem object");
        return NULL;
    }
}


wxObject* wxSizerXmlHandler::Handle_spacer()
{
    if ( !m_parentSizer )
    {
        ReportError("spacer only allowed inside a sizer");
        return NULL;
    }

    wxSizerItem* sitem = MakeSizerItem();
    SetSizerItemAttributes(sitem);
    sitem->AssignSpacer(GetSize());
    AddSizerItem(sitem);
    return NULL;
}


wxObject* wxSizerXmlHandler::Handle_sizer()
{
    wxXmlNode *parentNode = m_node->GetParent();

    if ( !m_parentSizer &&
            (!parentNode || parentNode->GetType() != wxXML_ELEMENT_NODE ||
             !m_parentAsWindow) )
    {
        ReportError("sizer must have a window parent");
        return NULL;
    }

    // Create the sizer of the appropriate class.
    wxSizer * const sizer = DoCreateSizer(m_class);

    // creation of sizer failed for some (already reported) reason, so exit:
    if ( !sizer )
        return NULL;

    wxSize minsize = GetSize(wxT("minsize"));
    if (!(minsize == wxDefaultSize))
        sizer->SetMinSize(minsize);

    // save state
    wxSizer *old_par = m_parentSizer;
    bool old_ins = m_isInside;

    // set new state
    m_parentSizer = sizer;
    m_isInside = true;
    m_isGBS = (m_class == wxT("wxGridBagSizer"));

    wxObject* parent = m_parent;
#if wxUSE_STATBOX
    // wxStaticBoxSizer's child controls should be parented by the box itself,
    // not its parent.
    wxStaticBoxSizer* const stsizer = wxDynamicCast(sizer, wxStaticBoxSizer);
    if ( stsizer )
        parent = stsizer->GetStaticBox();
#endif // wxUSE_STATBOX

    CreateChildren(parent, true/*only this handler*/);

    // set growable rows and cols for sizers which support this
    if ( wxFlexGridSizer *flexsizer = wxDynamicCast(sizer, wxFlexGridSizer) )
    {
        SetFlexibleMode(flexsizer);
        SetGrowables(flexsizer, wxT("growablerows"), true);
        SetGrowables(flexsizer, wxT("growablecols"), false);
    }

    // restore state
    m_isInside = old_ins;
    m_parentSizer = old_par;

    if (m_parentSizer == NULL) // setup window:
    {
        m_parentAsWindow->SetSizer(sizer);

        wxXmlNode *nd = m_node;
        m_node = parentNode;
        if (GetSize() == wxDefaultSize)
        {
            if ( wxDynamicCast(m_parentAsWindow, wxScrolledWindow) != NULL )
            {
                sizer->FitInside(m_parentAsWindow);
            }
            else
            {
                sizer->Fit(m_parentAsWindow);
            }
        }
        m_node = nd;

        if (m_parentAsWindow->IsTopLevel())
        {
            sizer->SetSizeHints(m_parentAsWindow);
        }
    }

    return sizer;
}


wxSizer*  wxSizerXmlHandler::Handle_wxBoxSizer()
{
    return new wxBoxSizer(GetStyle(wxT("orient"), wxHORIZONTAL));
}

#if wxUSE_STATBOX
wxSizer*  wxSizerXmlHandler::Handle_wxStaticBoxSizer()
{
    return new wxStaticBoxSizer(
            new wxStaticBox(m_parentAsWindow,
                            GetID(),
                            GetText(wxT("label")),
                            wxDefaultPosition, wxDefaultSize,
                            0/*style*/,
                            GetName()),
            GetStyle(wxT("orient"), wxHORIZONTAL));
}
#endif // wxUSE_STATBOX

wxSizer*  wxSizerXmlHandler::Handle_wxGridSizer()
{
    return new wxGridSizer(GetLong(wxT("rows")), GetLong(wxT("cols")),
                           GetDimension(wxT("vgap")), GetDimension(wxT("hgap")));
}


wxFlexGridSizer* wxSizerXmlHandler::Handle_wxFlexGridSizer()
{
    if ( !ValidateGridSizerChildren() )
        return NULL;
    return new wxFlexGridSizer(GetLong(wxT("rows")), GetLong(wxT("cols")),
                               GetDimension(wxT("vgap")), GetDimension(wxT("hgap")));
}


wxGridBagSizer* wxSizerXmlHandler::Handle_wxGridBagSizer()
{
    if ( !ValidateGridSizerChildren() )
        return NULL;
    return new wxGridBagSizer(GetDimension(wxT("vgap")), GetDimension(wxT("hgap")));
}

wxSizer*  wxSizerXmlHandler::Handle_wxWrapSizer()
{
    wxWrapSizer *sizer = new wxWrapSizer(GetStyle("orient", wxHORIZONTAL), GetStyle("flag"));
    return sizer;
}


bool wxSizerXmlHandler::ValidateGridSizerChildren()
{
    int rows = GetLong("rows");
    int cols = GetLong("cols");

    if  ( rows && cols )
    {
        // fixed number of cells, need to verify children count
        int children = 0;
        for ( wxXmlNode *n = m_node->GetChildren(); n; n = n->GetNext() )
        {
            if ( n->GetType() == wxXML_ELEMENT_NODE &&
                 (n->GetName() == "object" || n->GetName() == "object_ref") )
            {
                children++;
            }
        }

        if ( children > rows * cols )
        {
            ReportError
            (
                wxString::Format
                (
                    "too many children in grid sizer: %d > %d x %d"
                    " (consider omitting the number of rows or columns)",
                    children,
                    cols,
                    rows
                )
            );
            return false;
        }
    }

    return true;
}


void wxSizerXmlHandler::SetFlexibleMode(wxFlexGridSizer* fsizer)
{
    if (HasParam(wxT("flexibledirection")))
    {
        wxString dir = GetParamValue(wxT("flexibledirection"));

        if (dir == wxT("wxVERTICAL"))
            fsizer->SetFlexibleDirection(wxVERTICAL);
        else if (dir == wxT("wxHORIZONTAL"))
            fsizer->SetFlexibleDirection(wxHORIZONTAL);
        else if (dir == wxT("wxBOTH"))
            fsizer->SetFlexibleDirection(wxBOTH);
        else
        {
            ReportParamError
            (
                wxT("flexibledirection"),
                wxString::Format("unknown direction \"%s\"", dir)
            );
        }
    }

    if (HasParam(wxT("nonflexiblegrowmode")))
    {
        wxString mode = GetParamValue(wxT("nonflexiblegrowmode"));

        if (mode == wxT("wxFLEX_GROWMODE_NONE"))
            fsizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_NONE);
        else if (mode == wxT("wxFLEX_GROWMODE_SPECIFIED"))
            fsizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        else if (mode == wxT("wxFLEX_GROWMODE_ALL"))
            fsizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_ALL);
        else
        {
            ReportParamError
            (
                wxT("nonflexiblegrowmode"),
                wxString::Format("unknown grow mode \"%s\"", mode)
            );
        }
    }
}


void wxSizerXmlHandler::SetGrowables(wxFlexGridSizer* sizer,
                                     const wxChar* param,
                                     bool rows)
{
    int nrows, ncols;
    sizer->CalcRowsCols(nrows, ncols);
    const int nslots = rows ? nrows : ncols;

    wxStringTokenizer tkn;
    tkn.SetString(GetParamValue(param), wxT(","));

    while (tkn.HasMoreTokens())
    {
        wxString propStr;
        wxString idxStr = tkn.GetNextToken().BeforeFirst(wxT(':'), &propStr);

        unsigned long li;
        if (!idxStr.ToULong(&li))
        {
            ReportParamError
            (
                param,
                "value must be a comma-separated list of numbers"
            );
            break;
        }

        unsigned long lp = 0;
        if (!propStr.empty())
        {
            if (!propStr.ToULong(&lp))
            {
                ReportParamError
                (
                    param,
                    "value must be a comma-separated list of numbers"
                );
                break;
            }
        }

        const int n = static_cast<int>(li);
        if ( n >= nslots )
        {
            ReportParamError
            (
                param,
                wxString::Format
                (
                    "invalid %s index %d: must be less than %d",
                    rows ? "row" : "column",
                    n,
                    nslots
                )
            );

            // ignore incorrect value, still try to process the rest
            continue;
        }

        if (rows)
            sizer->AddGrowableRow(n, static_cast<int>(lp));
        else
            sizer->AddGrowableCol(n, static_cast<int>(lp));
    }
}


wxGBPosition wxSizerXmlHandler::GetGBPos(const wxString& param)
{
    wxSize sz = GetSize(param);
    if (sz.x < 0) sz.x = 0;
    if (sz.y < 0) sz.y = 0;
    return wxGBPosition(sz.x, sz.y);
}

wxGBSpan wxSizerXmlHandler::GetGBSpan(const wxString& param)
{
    wxSize sz = GetSize(param);
    if (sz.x < 1) sz.x = 1;
    if (sz.y < 1) sz.y = 1;
    return wxGBSpan(sz.x, sz.y);
}



wxSizerItem* wxSizerXmlHandler::MakeSizerItem()
{
    if (m_isGBS)
        return new wxGBSizerItem();
    else
        return new wxSizerItem();
}

int wxSizerXmlHandler::GetSizerFlags()
{
    const wxString s = GetParamValue(wxS("flag"));
    if ( s.empty() )
        return 0;

    // Parse flags keeping track of invalid combinations. This is somewhat
    // redundant with the checks performed in wxSizer subclasses themselves but
    // doing it here allows us to give the exact line number at which the
    // offending line numbers are given, which is very valuable.
    //
    // We also can detect invalid flags combinations involving wxALIGN_LEFT and
    // wxALIGN_TOP here, while this is impossible at wxSizer level as both of
    // these flags have value of 0.


    // As the logic is exactly the same in horizontal and vertical
    // orientations, use arrays and loops to avoid duplicating the code.

    enum Orient
    {
        Orient_Horz,
        Orient_Vert,
        Orient_Max
    };

    const char* const orientName[] = { "horizontal", "vertical" };

    // The already seen alignment flag in the given orientation or empty if
    // none have been seen yet.
    wxString alignFlagIn[] = { wxString(), wxString() };

    // Either "wxEXPAND" or "wxGROW" depending on the string used in the input,
    // or empty string if none is specified.
    wxString expandFlag;

    // Either "wxALIGN_CENTRE" or "wxALIGN_CENTER" if either flag was found or
    // empty string.
    wxString centreFlag;

    // Indicates whether we can use alignment in the given orientation at all.
    bool alignAllowedIn[] = { true, true };

    // Find out the sizer orientation: it is the principal/major size direction
    // for the 1D sizers and undefined/invalid for the 2D ones.
    Orient orientSizer;
    if ( wxBoxSizer* const boxSizer = wxDynamicCast(m_parentSizer, wxBoxSizer) )
    {
        orientSizer = boxSizer->GetOrientation() == wxHORIZONTAL
                        ? Orient_Horz
                        : Orient_Vert;

        // Alignment can be only used in the transversal/minor direction.
        alignAllowedIn[orientSizer] = false;
    }
    else
    {
        orientSizer = Orient_Max;
    }

    int flags = 0;

    wxStringTokenizer tkn(s, wxS("| \t\n"), wxTOKEN_STRTOK);
    while ( tkn.HasMoreTokens() )
    {
        const wxString flagName = tkn.GetNextToken();
        const int n = m_styleNames.Index(flagName);
        if ( n == wxNOT_FOUND )
        {
            ReportParamError
            (
                "flag",
                wxString::Format("unknown sizer flag \"%s\"", flagName)
            );
            continue;
        }

        // Flag description is the string that appears in the error messages,
        // the main difference from the flag name is that it can indicate that
        // wxALIGN_CENTRE_XXX flag could have been encountered as part of
        // wxALIGN_CENTRE which should make the error message more clear as
        // seeing references to e.g. wxALIGN_CENTRE_VERTICAL when it's never
        // used could be confusing.
        wxString flagDesc = wxS('"') + flagName + wxS('"');

        int flag = m_styleValues[n];

        bool flagSpecifiesAlignIn[] = { false, false };

        switch ( flag )
        {
            case wxALIGN_CENTRE_HORIZONTAL:
            case wxALIGN_RIGHT:
                flagSpecifiesAlignIn[Orient_Horz] = true;
                break;

            case wxALIGN_CENTRE_VERTICAL:
            case wxALIGN_BOTTOM:
                flagSpecifiesAlignIn[Orient_Vert] = true;
                break;

            case wxEXPAND:
                expandFlag = flagName;
                break;

            case wxALIGN_CENTRE:
                // wxALIGN_CENTRE is a combination of wxALIGN_CENTRE_HORIZONTAL
                // and wxALIGN_CENTRE_VERTICAL but we also handle it as just
                // one of those flags if alignment in the other direction is
                // not allowed for both compatibility and convenience reasons.
                switch ( orientSizer )
                {
                    case Orient_Horz:
                        flagSpecifiesAlignIn[Orient_Vert] = true;
                        flagDesc.Printf
                        (
                             "\"wxALIGN_CENTRE_VERTICAL\" (as part of %s)",
                             flagName
                        );
                        flag = wxALIGN_CENTRE_VERTICAL;
                        break;

                    case Orient_Vert:
                        flagSpecifiesAlignIn[Orient_Horz] = true;
                        flagDesc.Printf
                        (
                            "\"wxALIGN_CENTRE_HORIZONTAL\" (as part of %s)",
                            flagName
                        );
                        flag = wxALIGN_CENTRE_HORIZONTAL;
                        break;

                    case Orient_Max:
                        // For 2D sizers we need to deal with this flag at the
                        // end, so just remember that we had it for now.
                        centreFlag = flagName;
                        flag = 0;
                        break;
                }
                break;

            case 0:
                // This is a special case: both wxALIGN_LEFT and wxALIGN_TOP
                // have value of 0, so we need to examine the name of the flag
                // and not just its value.
                if ( flagName == wxS("wxALIGN_LEFT") )
                    flagSpecifiesAlignIn[Orient_Horz] = true;
                else if ( flagName == wxS("wxALIGN_TOP") )
                    flagSpecifiesAlignIn[Orient_Vert] = true;
                break;
        }

        for ( int orient = 0; orient < Orient_Max; orient++ )
        {
            if ( !flagSpecifiesAlignIn[orient] )
                continue;

            if ( !alignAllowedIn[orient] )
            {
                ReportParamError
                (
                    "flag",
                    wxString::Format
                    (
                        "%s alignment flag %s has no effect inside "
                        "a %s box sizer, remove it and consider inserting "
                        "a spacer instead",
                        orientName[orient],
                        flagDesc,
                        orientName[orient]
                    )
                );

                // Notice that we take care to not add this invalid flag to the
                // flags we will actually use with wxSizer: they would just
                // trigger an assert there which wouldn't be very useful as
                // we've already given an error about this.
                flag = 0;
            }
            else if ( alignFlagIn[orient].empty() )
            {
                alignFlagIn[orient] = flagDesc;
            }
            else
            {
                ReportParamError
                (
                    "flag",
                    wxString::Format
                    (
                        "both %s and %s specify %s alignment "
                        "and can't be used together",
                        alignFlagIn[orient],
                        flagDesc,
                        orientName[orient]
                    )
                );

                flag = 0;
            }
        }

        flags |= flag;
    }

    // Now that we know all the alignment flags we can interpret wxALIGN_CENTRE
    // for the 2D sizers ("centreFlag" is only set in the 2D case).
    if ( !centreFlag.empty() )
    {
        if ( !expandFlag.empty() )
        {
            ReportParamError
            (
                "flag",
                wxString::Format
                (
                    "\"%s\" has no effect when combined with \"%s\"",
                    centreFlag,
                    expandFlag
                )
            );
        }
        else // !wxEXPAND
        {
            int flagsCentre = 0;

            if ( alignFlagIn[Orient_Horz].empty() )
                flagsCentre |= wxALIGN_CENTRE_HORIZONTAL;

            if ( alignFlagIn[Orient_Vert].empty() )
                flagsCentre |= wxALIGN_CENTRE_VERTICAL;

            if ( !flagsCentre )
            {
                ReportParamError
                (
                    "flag",
                    wxString::Format
                    (
                        "\"%s\" flag has no effect when combined "
                        "with both %s and %s horizontal and "
                        "vertical alignment flags",
                        centreFlag,
                        alignFlagIn[Orient_Horz],
                        alignFlagIn[Orient_Vert]
                    )
                );
            }

            flags |= flagsCentre;
        }
    }

    // Finally check that the alignment flags are compatible with wxEXPAND.
    if ( !expandFlag.empty() )
    {
        if ( orientSizer != Orient_Max )
        {
            const Orient orientOther = orientSizer == Orient_Horz
                                            ? Orient_Vert
                                            : Orient_Horz;

            if ( !alignFlagIn[orientOther].empty() )
            {
                ReportParamError
                (
                    "flag",
                    wxString::Format
                    (
                        "\"%s\" is incompatible with %s alignment flag "
                        "\"%s\" in a %s box sizer",
                        expandFlag,
                        orientName[orientOther],
                        alignFlagIn[orientOther],
                        orientName[orientSizer]
                    )
                );

                // Just as with the alignment flags above, ignore wxEXPAND
                // completely to avoid asserts from wxSizer code.
                flags &= ~wxEXPAND;
            }
        }
        else // 2D sizer
        {
            if ( !alignFlagIn[Orient_Horz].empty() &&
                    !alignFlagIn[Orient_Vert].empty() )
            {
                ReportParamError
                (
                    "flag",
                    wxString::Format
                    (
                        "\"%s\" flag has no effect when combined "
                        "with both %s and %s horizontal and "
                        "vertical alignment flags",
                        expandFlag,
                        alignFlagIn[Orient_Horz],
                        alignFlagIn[Orient_Vert]
                    )
                );

                flags &= ~wxEXPAND;
            }
        }
    }

    return flags;
}

void wxSizerXmlHandler::SetSizerItemAttributes(wxSizerItem* sitem)
{
    sitem->SetProportion(GetLong(wxT("option")));  // Should this check for "proportion" too?
    sitem->SetFlag(GetSizerFlags());
    sitem->SetBorder(GetDimension(wxT("border")));
    wxSize sz = GetSize(wxT("minsize"));
    if (!(sz == wxDefaultSize))
        sitem->SetMinSize(sz);
    sz = GetSize(wxT("ratio"));
    if (!(sz == wxDefaultSize))
        sitem->SetRatio(sz);

    if (m_isGBS)
    {
        wxGBSizerItem* gbsitem = (wxGBSizerItem*)sitem;
        gbsitem->SetPos(GetGBPos(wxT("cellpos")));
        gbsitem->SetSpan(GetGBSpan(wxT("cellspan")));
    }

    // record the id of the item, if any, for use by XRCSIZERITEM()
    sitem->SetId(GetID());
}

void wxSizerXmlHandler::AddSizerItem(wxSizerItem* sitem)
{
    if (m_isGBS)
        ((wxGridBagSizer*)m_parentSizer)->Add((wxGBSizerItem*)sitem);
    else
        m_parentSizer->Add(sitem);
}



//-----------------------------------------------------------------------------
// wxStdDialogButtonSizerXmlHandler
//-----------------------------------------------------------------------------
#if wxUSE_BUTTON

wxIMPLEMENT_DYNAMIC_CLASS(wxStdDialogButtonSizerXmlHandler, wxXmlResourceHandler);

wxStdDialogButtonSizerXmlHandler::wxStdDialogButtonSizerXmlHandler()
    : m_isInside(false), m_parentSizer(NULL)
{
}

wxObject *wxStdDialogButtonSizerXmlHandler::DoCreateResource()
{
    if (m_class == wxT("wxStdDialogButtonSizer"))
    {
        wxASSERT( !m_parentSizer );

        wxSizer *s = m_parentSizer = new wxStdDialogButtonSizer;
        m_isInside = true;

        CreateChildren(m_parent, true/*only this handler*/);

        m_parentSizer->Realize();

        m_isInside = false;
        m_parentSizer = NULL;

        return s;
    }
    else // m_class == "button"
    {
        wxASSERT( m_parentSizer );

        // find the item to be managed by this sizeritem
        wxXmlNode *n = GetParamNode(wxT("object"));
        if ( !n )
            n = GetParamNode(wxT("object_ref"));

        // did we find one?
        if (n)
        {
            wxObject *item = CreateResFromNode(n, m_parent, NULL);
            wxButton *button = wxDynamicCast(item, wxButton);

            if (button)
                m_parentSizer->AddButton(button);
            else
                ReportError(n, "expected wxButton");

            return item;
        }
        else /*n == NULL*/
        {
            ReportError("no button within wxStdDialogButtonSizer");
            return NULL;
        }
    }
}

bool wxStdDialogButtonSizerXmlHandler::CanHandle(wxXmlNode *node)
{
    return (!m_isInside && IsOfClass(node, wxT("wxStdDialogButtonSizer"))) ||
           (m_isInside && IsOfClass(node, wxT("button")));
}
#endif // wxUSE_BUTTON

#endif // wxUSE_XRC
