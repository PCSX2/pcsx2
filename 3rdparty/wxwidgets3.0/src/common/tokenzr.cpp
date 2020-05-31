/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/tokenzr.cpp
// Purpose:     String tokenizer
// Author:      Guilhem Lavaux
// Modified by: Vadim Zeitlin (almost full rewrite)
// Created:     04/22/98
// Copyright:   (c) Guilhem Lavaux
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

#include "wx/tokenzr.h"

#ifndef WX_PRECOMP
    #include "wx/arrstr.h"
    #include "wx/crt.h"
#endif

// Required for wxIs... functions
#include <ctype.h>

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

static wxString::const_iterator
find_first_of(const wxChar *delims, size_t len,
              const wxString::const_iterator& from,
              const wxString::const_iterator& end)
{
    wxASSERT_MSG( from <= end,  wxT("invalid index") );

    for ( wxString::const_iterator i = from; i != end; ++i )
    {
        if ( wxTmemchr(delims, *i, len) )
            return i;
    }

    return end;
}

static wxString::const_iterator
find_first_not_of(const wxChar *delims, size_t len,
                  const wxString::const_iterator& from,
                  const wxString::const_iterator& end)
{
    wxASSERT_MSG( from <= end,  wxT("invalid index") );

    for ( wxString::const_iterator i = from; i != end; ++i )
    {
        if ( !wxTmemchr(delims, *i, len) )
            return i;
    }

    return end;
}

// ----------------------------------------------------------------------------
// wxStringTokenizer construction
// ----------------------------------------------------------------------------

wxStringTokenizer::wxStringTokenizer(const wxString& str,
                                     const wxString& delims,
                                     wxStringTokenizerMode mode)
{
    SetString(str, delims, mode);
}

void wxStringTokenizer::SetString(const wxString& str,
                                  const wxString& delims,
                                  wxStringTokenizerMode mode)
{
    if ( mode == wxTOKEN_DEFAULT )
    {
        // by default, we behave like strtok() if the delimiters are only
        // whitespace characters and as wxTOKEN_RET_EMPTY otherwise (for
        // whitespace delimiters, strtok() behaviour is better because we want
        // to count consecutive spaces as one delimiter)
        wxString::const_iterator p;
        for ( p = delims.begin(); p != delims.end(); ++p )
        {
            if ( !wxIsspace(*p) )
                break;
        }

        if ( p != delims.end() )
        {
            // not whitespace char in delims
            mode = wxTOKEN_RET_EMPTY;
        }
        else
        {
            // only whitespaces
            mode = wxTOKEN_STRTOK;
        }
    }

#if wxUSE_UNICODE // FIXME-UTF8: only wc_str()
    m_delims = delims.wc_str();
#else
    m_delims = delims.mb_str();
#endif
    m_delimsLen = delims.length();

    m_mode = mode;

    Reinit(str);
}

void wxStringTokenizer::Reinit(const wxString& str)
{
    wxASSERT_MSG( IsOk(), wxT("you should call SetString() first") );

    m_string = str;
    m_stringEnd = m_string.end();
    m_pos = m_string.begin();
    m_lastDelim = wxT('\0');
    m_hasMoreTokens = MoreTokens_Unknown;
}

// ----------------------------------------------------------------------------
// access to the tokens
// ----------------------------------------------------------------------------

// do we have more of them?
bool wxStringTokenizer::HasMoreTokens() const
{
    // GetNextToken() calls HasMoreTokens() and so HasMoreTokens() is called
    // twice in every interation in the following common usage patten:
    //     while ( HasMoreTokens() )
    //        GetNextToken();
    // We optimize this case by caching HasMoreTokens() return value here:
    if ( m_hasMoreTokens == MoreTokens_Unknown )
    {
        bool r = DoHasMoreTokens();
        wxConstCast(this, wxStringTokenizer)->m_hasMoreTokens =
            r ? MoreTokens_Yes : MoreTokens_No;
        return r;
    }
    else
        return m_hasMoreTokens == MoreTokens_Yes;
}

bool wxStringTokenizer::DoHasMoreTokens() const
{
    wxCHECK_MSG( IsOk(), false, wxT("you should call SetString() first") );

    if ( find_first_not_of(m_delims, m_delimsLen, m_pos, m_stringEnd)
         != m_stringEnd )
    {
        // there are non delimiter characters left, so we do have more tokens
        return true;
    }

    switch ( m_mode )
    {
        case wxTOKEN_RET_EMPTY:
        case wxTOKEN_RET_DELIMS:
            // special hack for wxTOKEN_RET_EMPTY: we should return the initial
            // empty token even if there are only delimiters after it
            return !m_string.empty() && m_pos == m_string.begin();

        case wxTOKEN_RET_EMPTY_ALL:
            // special hack for wxTOKEN_RET_EMPTY_ALL: we can know if we had
            // already returned the trailing empty token after the last
            // delimiter by examining m_lastDelim: it is set to NUL if we run
            // up to the end of the string in GetNextToken(), but if it is not
            // NUL yet we still have this last token to return even if m_pos is
            // already at m_string.length()
            return m_pos < m_stringEnd || m_lastDelim != wxT('\0');

        case wxTOKEN_INVALID:
        case wxTOKEN_DEFAULT:
            wxFAIL_MSG( wxT("unexpected tokenizer mode") );
            // fall through

        case wxTOKEN_STRTOK:
            // never return empty delimiters
            break;
    }

    return false;
}

// count the number of (remaining) tokens in the string
size_t wxStringTokenizer::CountTokens() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("you should call SetString() first") );

    // VZ: this function is IMHO not very useful, so it's probably not very
    //     important if its implementation here is not as efficient as it
    //     could be -- but OTOH like this we're sure to get the correct answer
    //     in all modes
    wxStringTokenizer tkz(wxString(m_pos, m_stringEnd), m_delims, m_mode);

    size_t count = 0;
    while ( tkz.HasMoreTokens() )
    {
        count++;

        (void)tkz.GetNextToken();
    }

    return count;
}

// ----------------------------------------------------------------------------
// token extraction
// ----------------------------------------------------------------------------

wxString wxStringTokenizer::GetNextToken()
{
    wxString token;
    do
    {
        if ( !HasMoreTokens() )
        {
            break;
        }

        m_hasMoreTokens = MoreTokens_Unknown;

        // find the end of this token
        wxString::const_iterator pos =
            find_first_of(m_delims, m_delimsLen, m_pos, m_stringEnd);

        // and the start of the next one
        if ( pos == m_stringEnd )
        {
            // no more delimiters, the token is everything till the end of
            // string
            token.assign(m_pos, m_stringEnd);

            // skip the token
            m_pos = m_stringEnd;

            // it wasn't terminated
            m_lastDelim = wxT('\0');
        }
        else // we found a delimiter at pos
        {
            // in wxTOKEN_RET_DELIMS mode we return the delimiter character
            // with token, otherwise leave it out
            wxString::const_iterator tokenEnd(pos);
            if ( m_mode == wxTOKEN_RET_DELIMS )
                ++tokenEnd;

            token.assign(m_pos, tokenEnd);

            // skip the token and the trailing delimiter
            m_pos = pos + 1;

            m_lastDelim = (pos == m_stringEnd) ? wxT('\0') : (wxChar)*pos;
        }
    }
    while ( !AllowEmpty() && token.empty() );

    return token;
}

// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

wxArrayString wxStringTokenize(const wxString& str,
                               const wxString& delims,
                               wxStringTokenizerMode mode)
{
    wxArrayString tokens;
    wxStringTokenizer tk(str, delims, mode);
    while ( tk.HasMoreTokens() )
    {
        tokens.Add(tk.GetNextToken());
    }

    return tokens;
}
