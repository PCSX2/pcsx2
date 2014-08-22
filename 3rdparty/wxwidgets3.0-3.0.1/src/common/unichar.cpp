/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/unichar.cpp
// Purpose:     wxUniChar and wxUniCharRef classes
// Author:      Vaclav Slavik
// Created:     2007-03-19
// Copyright:   (c) 2007 REA Elektronik GmbH
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// headers
// ===========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/strconv.h"  // wxConvLibc
    #include "wx/log.h"
#endif

#include "wx/unichar.h"
#include "wx/string.h"

// ===========================================================================
// implementation
// ===========================================================================

// ---------------------------------------------------------------------------
// wxUniChar
// ---------------------------------------------------------------------------

/* static */
wxUniChar::value_type wxUniChar::FromHi8bit(char c)
{
#if wxUSE_UTF8_LOCALE_ONLY
    wxFAIL_MSG( "invalid UTF-8 character" );
    wxUnusedVar(c);

    return wxT('?'); // FIXME-UTF8: what to use as failure character?
#else
    char cbuf[2];
    cbuf[0] = c;
    cbuf[1] = '\0';
    wchar_t wbuf[2];
    if ( wxConvLibc.ToWChar(wbuf, 2, cbuf, 2) != 2 )
    {
        wxFAIL_MSG( "invalid multibyte character" );
        return wxT('?'); // FIXME-UTF8: what to use as failure character?
    }
    return wbuf[0];
#endif
}

/* static */
char wxUniChar::ToHi8bit(wxUniChar::value_type v)
{
    char c;
    if ( !GetAsHi8bit(v, &c) )
    {
        wxFAIL_MSG( "character cannot be converted to single byte" );
        c = '?'; // FIXME-UTF8: what to use as failure character?
    }

    return c;
}

/* static */
bool wxUniChar::GetAsHi8bit(value_type v, char *c)
{
    wchar_t wbuf[2];
    wbuf[0] = v;
    wbuf[1] = L'\0';
    char cbuf[2];
    if ( wxConvLibc.FromWChar(cbuf, 2, wbuf, 2) != 2 )
        return false;

    *c = cbuf[0];
    return true;
}

// ---------------------------------------------------------------------------
// wxUniCharRef
// ---------------------------------------------------------------------------

#if wxUSE_UNICODE_UTF8
wxUniChar wxUniCharRef::UniChar() const
{
    return wxStringOperations::DecodeChar(m_pos);
}

wxUniCharRef& wxUniCharRef::operator=(const wxUniChar& c)
{
    wxStringOperations::Utf8CharBuffer utf(wxStringOperations::EncodeChar(c));
    size_t lenOld = wxStringOperations::GetUtf8CharLength(*m_pos);
    size_t lenNew = wxStringOperations::GetUtf8CharLength(utf[0]);

    if ( lenNew == lenOld )
    {
        // this is the simpler case: if the new value's UTF-8 code has the
        // same length, we can just replace it:

        iterator pos(m_pos);
        for ( size_t i = 0; i < lenNew; ++i, ++pos )
            *pos = utf[i];
    }
    else // length of character encoding in UTF-8 changed
    {
        // the worse case is when the new value has either longer or shorter
        // code -- in that case, we have to use wxStringImpl::replace() and
        // this invalidates all iterators, so we have to update them too:

        wxStringImpl& strimpl = m_str.m_impl;

        int iterDiff = lenNew - lenOld;
        size_t posIdx = m_pos - strimpl.begin();

        // compute positions of outstanding iterators for this string after the
        // replacement is done (there is only a small number of iterators at
        // any time, so we use an array on the stack to avoid unneeded
        // allocation):
        static const size_t STATIC_SIZE = 32;
        size_t indexes_a[STATIC_SIZE];
        size_t *indexes = indexes_a;
        size_t iterNum = 0;
        wxStringIteratorNode *it;
        for ( it = m_str.m_iterators.ptr; it; it = it->m_next, ++iterNum )
        {
            wxASSERT( it->m_iter || it->m_citer );

            if ( iterNum == STATIC_SIZE )
            {
                wxLogTrace( wxT("utf8"), wxT("unexpectedly many iterators") );

                size_t total = iterNum + 1;
                for ( wxStringIteratorNode *it2 = it; it2; it2 = it2->m_next )
                    total++;
                indexes = new size_t[total];
                memcpy(indexes, indexes_a, sizeof(size_t) * STATIC_SIZE);
            }

            size_t idx = it->m_iter
                         ? (*it->m_iter - strimpl.begin())
                         : (*it->m_citer - strimpl.begin());

            if ( idx > posIdx )
                idx += iterDiff;

            indexes[iterNum] = idx;
        }

        // update the string:
        strimpl.replace(m_pos, m_pos + lenOld, utf, lenNew);

#if wxUSE_STRING_POS_CACHE
        m_str.InvalidateCache();
#endif // wxUSE_STRING_POS_CACHE

        // finally, set the iterators to valid values again (note that this
        // updates m_pos as well):
        size_t i;
        for ( i = 0, it = m_str.m_iterators.ptr; it; it = it->m_next, ++i )
        {
            wxASSERT( i < iterNum );
            wxASSERT( it->m_iter || it->m_citer );

            if ( it->m_iter )
                *it->m_iter = strimpl.begin() + indexes[i];
            else // it->m_citer
                *it->m_citer = strimpl.begin() + indexes[i];
        }

        if ( indexes != indexes_a )
            delete[] indexes;
    }

    return *this;
}
#endif // wxUSE_UNICODE_UTF8
