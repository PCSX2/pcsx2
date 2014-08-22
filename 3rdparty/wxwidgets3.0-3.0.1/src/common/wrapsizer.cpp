/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/wrapsizer.cpp
// Purpose:     provides wxWrapSizer class for layout
// Author:      Arne Steinarson
// Created:     2008-05-08
// Copyright:   (c) Arne Steinarson
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/wrapsizer.h"
#include "wx/vector.h"

namespace
{

// ----------------------------------------------------------------------------
// helper local classes
// ----------------------------------------------------------------------------

// This object changes the item proportion to INT_MAX in its ctor and restores
// it back in the dtor.
class wxPropChanger : public wxObject
{
public:
    wxPropChanger(wxSizer& sizer, wxSizerItem& item)
        : m_sizer(sizer),
          m_item(item),
          m_propOld(item.GetProportion())
    {
        // ensure that this item expands more than all the other ones
        item.SetProportion(INT_MAX);
    }

    ~wxPropChanger()
    {
        // check if the sizer still has this item, it could have been removed
        if ( m_sizer.GetChildren().Find(&m_item) )
            m_item.SetProportion(m_propOld);
    }

private:
    wxSizer& m_sizer;
    wxSizerItem& m_item;
    const int m_propOld;

    wxDECLARE_NO_COPY_CLASS(wxPropChanger);
};

} // anonymous namespace

// ============================================================================
// wxWrapSizer implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxWrapSizer, wxBoxSizer)

wxWrapSizer::wxWrapSizer(int orient, int flags)
           : wxBoxSizer(orient),
             m_flags(flags),
             m_dirInform(0),
             m_availSize(-1),
             m_availableOtherDir(0),
             m_lastUsed(true),
             m_minSizeMinor(0),
             m_maxSizeMajor(0),
             m_minItemMajor(INT_MAX),
             m_rows(orient ^ wxBOTH)
{
}

wxWrapSizer::~wxWrapSizer()
{
    ClearRows();
}

void wxWrapSizer::ClearRows()
{
    // all elements of the row sizers are also elements of this one (we
    // directly add pointers to elements of our own m_children list to the row
    // sizers in RecalcSizes()), so we need to detach them from the row sizer
    // to avoid double deletion
    wxSizerItemList& rows = m_rows.GetChildren();
    for ( wxSizerItemList::iterator i = rows.begin(),
                                  end = rows.end();
          i != end;
          ++i )
    {
        wxSizerItem * const item = *i;
        wxSizer * const row = item->GetSizer();
        if ( !row )
        {
            wxFAIL_MSG( "all elements of m_rows must be sizers" );
            continue;
        }

        row->GetChildren().clear();

        wxPropChanger * const
            propChanger = static_cast<wxPropChanger *>(item->GetUserData());
        if ( propChanger )
        {
            // this deletes propChanger and so restores the old proportion
            item->SetUserData(NULL);
        }
    }
}

wxSizer *wxWrapSizer::GetRowSizer(size_t n)
{
    const wxSizerItemList& rows = m_rows.GetChildren();
    if ( n < rows.size() )
        return rows[n]->GetSizer();

    wxSizer * const sizer = new wxBoxSizer(GetOrientation());
    m_rows.Add(sizer, wxSizerFlags().Expand());
    return sizer;
}

bool wxWrapSizer::InformFirstDirection(int direction,
                                       int size,
                                       int availableOtherDir)
{
    if ( !direction )
        return false;

    // Store the values for later use
    m_availSize = size;
    m_availableOtherDir = availableOtherDir +
                            (direction == wxHORIZONTAL ? m_minSize.y
                                                       : m_minSize.x);
    m_dirInform = direction;
    m_lastUsed = false;
    return true;
}


void wxWrapSizer::AdjustLastRowItemProp(size_t n, wxSizerItem *itemLast)
{
    if ( !itemLast || !(m_flags & wxEXTEND_LAST_ON_EACH_LINE) )
    {
        // nothing to do
        return;
    }

    wxSizerItem * const item = m_rows.GetItem(n);
    wxCHECK_RET( item, "invalid sizer item" );

    // store the item we modified and its original proportion
    item->SetUserData(new wxPropChanger(*this, *itemLast));
}

wxSize wxWrapSizer::CalcMin()
{
    if ( m_children.empty() )
        return wxSize();

    // We come here to calculate min size in two different situations:
    // 1 - Immediately after InformFirstDirection, then we find a min size that
    //     uses one dimension maximally and the other direction minimally.
    // 2 - Ordinary case, get a sensible min size value using the current line
    //     layout, trying to maintain the possibility to re-arrange lines by
    //     sizing

    if ( !m_lastUsed )
    {
        // Case 1 above: InformFirstDirection() has just been called
        m_lastUsed = true;

        // There are two different algorithms for finding a useful min size for
        // a wrap sizer, depending on whether the first reported size component
        // is the opposite as our own orientation (the simpler case) or the same
        // one (more complicated).
        if ( m_dirInform == m_orient )
            CalcMinFromMajor(m_availSize);
        else
            CalcMinFromMinor(m_availSize);
    }
    else // Case 2 above: not immediately after InformFirstDirection()
    {
        if ( m_availSize > 0 )
        {
            wxSize szAvail;    // Keep track of boundary so we don't overflow
            if ( m_dirInform == m_orient )
                szAvail = SizeFromMajorMinor(m_availSize, m_availableOtherDir);
            else
                szAvail = SizeFromMajorMinor(m_availableOtherDir, m_availSize);

            CalcMinFittingSize(szAvail);
        }
        else // Initial calculation, before we have size available to us
        {
            CalcMaxSingleItemSize();
        }
    }

    return m_minSize;
}

void wxWrapSizer::CalcMinFittingSize(const wxSize& szBoundary)
{
    // Min size based on current line layout. It is important to
    // provide a smaller size when possible to allow for resizing with
    // the help of re-arranging the lines.
    wxSize sizeMin = SizeFromMajorMinor(m_maxSizeMajor, m_minSizeMinor);
    if ( m_minSizeMinor < SizeInMinorDir(m_size) &&
            m_maxSizeMajor < SizeInMajorDir(m_size) )
    {
        m_minSize = sizeMin;
    }
    else
    {
        // Try making it a bit more narrow
        bool done = false;
        if ( m_minItemMajor != INT_MAX && m_maxSizeMajor > 0 )
        {
            // We try to present a lower min value by removing an item in
            // the major direction (and preserving current minor min size).
            CalcMinFromMajor(m_maxSizeMajor - m_minItemMajor);
            if ( m_minSize.x <= szBoundary.x && m_minSize.y <= szBoundary.y )
            {
                SizeInMinorDir(m_minSize) = SizeInMinorDir(sizeMin);
                done = true;
            }
        }

        if ( !done )
        {
            // If failed finding little smaller area, go back to what we had
            m_minSize = sizeMin;
        }
    }
}

void wxWrapSizer::CalcMaxSingleItemSize()
{
    // Find max item size in each direction
    int maxMajor = 0;    // Widest item
    int maxMinor = 0;    // Line height
    for ( wxSizerItemList::const_iterator i = m_children.begin();
          i != m_children.end();
          ++i )
    {
        wxSizerItem * const item = *i;
        if ( item->IsShown() )
        {
            wxSize sz = item->CalcMin();
            if ( SizeInMajorDir(sz) > maxMajor )
                maxMajor = SizeInMajorDir(sz);
            if ( SizeInMinorDir(sz) > maxMinor )
                maxMinor = SizeInMinorDir(sz);
        }
    }

    // This is, of course, not our real minimal size but if we return more
    // than this it would be impossible to shrink us to one row/column so
    // we have to pretend that this is all we need for now.
    m_minSize = SizeFromMajorMinor(maxMajor, maxMinor);
}

void wxWrapSizer::CalcMinFromMajor(int totMajor)
{
    // Algorithm for calculating min size: (assuming horizontal orientation)
    // This is the simpler case (known major size)
    // X: Given, totMajor
    // Y: Based on X, calculate how many lines needed

    int maxTotalMajor = 0;      // max of rowTotalMajor over all rows
    int minorSum = 0;           // sum of sizes of all rows in minor direction
    int maxRowMinor = 0;        // max of item minor sizes in this row
    int rowTotalMajor = 0;      // sum of major sizes of items in this row

    // pack the items in each row until we reach totMajor, then start a new row
    for ( wxSizerItemList::const_iterator i = m_children.begin();
          i != m_children.end();
          ++i )
    {
        wxSizerItem * const item = *i;
        if ( !item->IsShown() )
            continue;

        wxSize minItemSize = item->CalcMin();
        const int itemMajor = SizeInMajorDir(minItemSize);
        const int itemMinor = SizeInMinorDir(minItemSize);

        // check if this is the first item in a new row: if so, we have to put
        // it in it, whether it fits or not, as it would never fit better
        // anyhow
        //
        // otherwise check if we have enough space left for this item here
        if ( !rowTotalMajor || rowTotalMajor + itemMajor <= totMajor )
        {
            // continue this row
            rowTotalMajor += itemMajor;
            if ( itemMinor > maxRowMinor )
                maxRowMinor = itemMinor;
        }
        else // start a new row
        {
            // minor size of the row is the max of minor sizes of its items
            minorSum += maxRowMinor;
            if ( rowTotalMajor > maxTotalMajor )
                maxTotalMajor = rowTotalMajor;
            maxRowMinor = itemMinor;
            rowTotalMajor = itemMajor;
        }
    }

    // account for the last (unfinished) row too
    minorSum += maxRowMinor;
    if ( rowTotalMajor > maxTotalMajor )
        maxTotalMajor = rowTotalMajor;

    m_minSize = SizeFromMajorMinor(maxTotalMajor, minorSum);
}

// Helper struct for CalcMinFromMinor
struct wxWrapLine
{
    wxWrapLine() : m_first(NULL), m_width(0) { }
    wxSizerItem *m_first;
    int m_width;        // Width of line
};

void wxWrapSizer::CalcMinFromMinor(int totMinor)
{
    // Algorithm for calculating min size:
    // This is the more complex case (known minor size)

    // First step, find total sum of all items in primary direction
    // and max item size in secondary direction, that gives initial
    // estimate of the minimum number of lines.

    int totMajor = 0;    // Sum of widths
    int maxMinor = 0;    // Line height
    int maxMajor = 0;    // Widest item
    int itemCount = 0;
    wxSizerItemList::compatibility_iterator node = m_children.GetFirst();
    wxSize sz;
    while (node)
    {
        wxSizerItem *item = node->GetData();
        if ( item->IsShown() )
        {
            sz = item->CalcMin();
            totMajor += SizeInMajorDir(sz);
            if ( SizeInMinorDir(sz)>maxMinor )
                maxMinor = SizeInMinorDir(sz);
            if ( SizeInMajorDir(sz)>maxMinor )
                maxMajor = SizeInMajorDir(sz);
            itemCount++;
        }
        node = node->GetNext();
    }

    // The trivial case
    if ( !itemCount || totMajor==0 || maxMinor==0 )
    {
        m_minSize = wxSize(0,0);
        return;
    }

    // First attempt, use lines of average size:
    int nrLines = totMinor / maxMinor; // Rounding down is right here
    if ( nrLines<=1 )
    {
        // Another simple case, everything fits on one line
        m_minSize = SizeFromMajorMinor(totMajor,maxMinor);
        return;
    }

    int lineSize = totMajor / nrLines;
    if ( lineSize<maxMajor )     // At least as wide as the widest element
        lineSize = maxMajor;

    // The algorithm is as follows (horz case):
    // 1 - Vertical (minor) size is known.
    // 2 - We have a reasonable estimated width from above
    // 3 - Loop
    // 3a - Do layout with suggested width
    // 3b - See how much we spill over in minor dir
    // 3c - If no spill, we're done
    // 3d - Otherwise increase width by known smallest item
    //      and redo loop

    // First algo step: put items on lines of known max width
    wxVector<wxWrapLine*> lines;

    int sumMinor;       // Sum of all minor sizes (height of all lines)

    // While we still have items 'spilling over' extend the tested line width
    for ( ;; )
    {
        wxWrapLine *line = new wxWrapLine;
        lines.push_back( line );

        int tailSize = 0;   // Width of what exceeds nrLines
        maxMinor = 0;
        sumMinor = 0;
        for ( node=m_children.GetFirst(); node; node=node->GetNext() )
        {
            wxSizerItem *item = node->GetData();
            if ( item->IsShown() )
            {
                sz = item->GetMinSizeWithBorder();
                if ( line->m_width+SizeInMajorDir(sz)>lineSize )
                {
                    line = new wxWrapLine;
                    lines.push_back(line);
                    sumMinor += maxMinor;
                    maxMinor = 0;
                }
                line->m_width += SizeInMajorDir(sz);
                if ( line->m_width && !line->m_first )
                    line->m_first = item;
                if ( SizeInMinorDir(sz)>maxMinor )
                    maxMinor = SizeInMinorDir(sz);
                if ( sumMinor+maxMinor>totMinor )
                {
                    // Keep track of widest tail item
                    if ( SizeInMajorDir(sz)>tailSize )
                        tailSize = SizeInMajorDir(sz);
                }
            }
        }

        if ( tailSize )
        {
            // Now look how much we need to extend our size
            // We know we must have at least one more line than nrLines
            // (otherwise no tail size).
            int bestExtSize = 0; // Minimum extension width for current tailSize
            for ( int ix=0; ix<nrLines; ix++ )
            {
                // Take what is not used on this line, see how much extension we get
                // by adding first item on next line.
                int size = lineSize-lines[ix]->m_width; // Left over at end of this line
                int extSize = GetSizeInMajorDir(lines[ix+1]->m_first->GetMinSizeWithBorder()) - size;
                if ( (extSize>=tailSize && (extSize<bestExtSize || bestExtSize<tailSize)) ||
                    (extSize>bestExtSize && bestExtSize<tailSize) )
                    bestExtSize = extSize;
            }
            // Have an extension size, ready to redo line layout
            lineSize += bestExtSize;
        }

        // Clear helper items
        for ( wxVector<wxWrapLine*>::iterator it=lines.begin(); it<lines.end(); ++it )
            delete *it;
        lines.clear();

        // No spill over?
        if ( !tailSize )
            break;
    }

    // Now have min size in the opposite direction
    m_minSize = SizeFromMajorMinor(lineSize,sumMinor);
}

void wxWrapSizer::FinishRow(size_t n,
                            int rowMajor, int rowMinor,
                            wxSizerItem *itemLast)
{
    // Account for the finished row size.
    m_minSizeMinor += rowMinor;
    if ( rowMajor > m_maxSizeMajor )
        m_maxSizeMajor = rowMajor;

    // And adjust proportion of its last item if necessary.
    AdjustLastRowItemProp(n, itemLast);
}

void wxWrapSizer::RecalcSizes()
{
    // First restore any proportions we may have changed and remove the old rows
    ClearRows();

    if ( m_children.empty() )
        return;

    // Put all our items into as many row box sizers as needed.
    const int majorSize = SizeInMajorDir(m_size);   // max size of each row
    int rowTotalMajor = 0;                          // running row major size
    int maxRowMinor = 0;

    m_minSizeMinor = 0;
    m_minItemMajor = INT_MAX;
    m_maxSizeMajor = 0;

    // We need at least one row
    size_t nRow = 0;
    wxSizer *sizer = GetRowSizer(nRow);

    wxSizerItem *itemLast = NULL,   // last item processed in this row
                *itemSpace = NULL;  // spacer which we delayed adding

    // Now put our child items into child sizers instead
    for ( wxSizerItemList::iterator i = m_children.begin();
          i != m_children.end();
          ++i )
    {
        wxSizerItem * const item = *i;
        if ( !item->IsShown() )
            continue;

        wxSize minItemSize = item->GetMinSizeWithBorder();
        const int itemMajor = SizeInMajorDir(minItemSize);
        const int itemMinor = SizeInMinorDir(minItemSize);
        if ( itemMajor > 0 && itemMajor < m_minItemMajor )
            m_minItemMajor = itemMajor;

        // Is there more space on this line? Notice that if this is the first
        // item we add it unconditionally as it wouldn't fit in the next line
        // any better than in this one.
        if ( !rowTotalMajor || rowTotalMajor + itemMajor <= majorSize )
        {
            // There is enough space here
            rowTotalMajor += itemMajor;
            if ( itemMinor > maxRowMinor )
                maxRowMinor = itemMinor;
        }
        else // Start a new row
        {
            FinishRow(nRow, rowTotalMajor, maxRowMinor, itemLast);

            rowTotalMajor = itemMajor;
            maxRowMinor = itemMinor;

            // Get a new empty sizer to insert into
            sizer = GetRowSizer(++nRow);

            itemLast =
            itemSpace = NULL;
        }

        // Only remove first/last spaces if that flag is set
        if ( (m_flags & wxREMOVE_LEADING_SPACES) && IsSpaceItem(item) )
        {
            // Remember space only if we have a first item
            if ( itemLast )
                itemSpace = item;
        }
        else // not a space
        {
            if ( itemLast && itemSpace )
            {
                // We had a spacer after a real item and now that we add
                // another real item to the same row we need to add the spacer
                // between them two.
                sizer->Add(itemSpace);
            }

            // Notice that we reuse a pointer to our own sizer item here, so we
            // must remember to remove it by calling ClearRows() to avoid
            // double deletion later
            sizer->Add(item);

            itemLast = item;
            itemSpace = NULL;
        }

        // If item is a window, it now has a pointer to the child sizer,
        // which is wrong. Set it to point to us.
        if ( wxWindow *win = item->GetWindow() )
            win->SetContainingSizer(this);
    }

    FinishRow(nRow, rowTotalMajor, maxRowMinor, itemLast);

    // Now do layout on row sizer
    m_rows.SetDimension(m_position, m_size);
}


