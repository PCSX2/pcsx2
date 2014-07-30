/////////////////////////////////////////////////////////////////////////////
// Name:        wx/html/htmltag.h
// Purpose:     wxHtmlTag class (represents single tag)
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HTMLTAG_H_
#define _WX_HTMLTAG_H_

#include "wx/defs.h"

#if wxUSE_HTML

#include "wx/object.h"
#include "wx/arrstr.h"

class WXDLLIMPEXP_FWD_CORE wxColour;
class WXDLLIMPEXP_FWD_HTML wxHtmlEntitiesParser;

//-----------------------------------------------------------------------------
// wxHtmlTagsCache
//          - internal wxHTML class, do not use!
//-----------------------------------------------------------------------------

class wxHtmlTagsCacheData;

class WXDLLIMPEXP_HTML wxHtmlTagsCache
{
private:
    wxHtmlTagsCacheData *m_Cache;
    int m_CachePos;

    wxHtmlTagsCacheData& Cache() { return *m_Cache; }

public:
    wxHtmlTagsCache() {m_Cache = NULL;}
    wxHtmlTagsCache(const wxString& source);
    virtual ~wxHtmlTagsCache();

    // Finds parameters for tag starting at at and fills the variables
    void QueryTag(const wxString::const_iterator& at,
                  const wxString::const_iterator& inputEnd,
                  wxString::const_iterator *end1,
                  wxString::const_iterator *end2,
                  bool *hasEnding);

    wxDECLARE_NO_COPY_CLASS(wxHtmlTagsCache);
};


//--------------------------------------------------------------------------------
// wxHtmlTag
//                  This represents single tag. It is used as internal structure
//                  by wxHtmlParser.
//--------------------------------------------------------------------------------

class WXDLLIMPEXP_HTML wxHtmlTag
{
protected:
    // constructs wxHtmlTag object based on HTML tag.
    // The tag begins (with '<' character) at position pos in source
    // end_pos is position where parsing ends (usually end of document)
    wxHtmlTag(wxHtmlTag *parent,
              const wxString *source,
              const wxString::const_iterator& pos,
              const wxString::const_iterator& end_pos,
              wxHtmlTagsCache *cache,
              wxHtmlEntitiesParser *entParser);
    friend class wxHtmlParser;
public:
    ~wxHtmlTag();

    wxHtmlTag *GetParent() const {return m_Parent;}
    wxHtmlTag *GetFirstSibling() const;
    wxHtmlTag *GetLastSibling() const;
    wxHtmlTag *GetChildren() const { return m_FirstChild; }
    wxHtmlTag *GetPreviousSibling() const { return m_Prev; }
    wxHtmlTag *GetNextSibling() const {return m_Next; }
    // Return next tag, as if tree had been flattened
    wxHtmlTag *GetNextTag() const;

    // Returns tag's name in uppercase.
    inline wxString GetName() const {return m_Name;}

    // Returns true if the tag has given parameter. Parameter
    // should always be in uppercase.
    // Example : <IMG SRC="test.jpg"> HasParam("SRC") returns true
    bool HasParam(const wxString& par) const;

    // Returns value of the param. Value is in uppercase unless it is
    // enclosed with "
    // Example : <P align=right> GetParam("ALIGN") returns (RIGHT)
    //           <P IMG SRC="WhaT.jpg"> GetParam("SRC") returns (WhaT.jpg)
    //                           (or ("WhaT.jpg") if with_quotes == true)
    wxString GetParam(const wxString& par, bool with_quotes = false) const;

    // Return true if the string could be parsed as an HTML colour and false
    // otherwise.
    static bool ParseAsColour(const wxString& str, wxColour *clr);

    // Convenience functions:
    bool GetParamAsString(const wxString& par, wxString *str) const;
    bool GetParamAsColour(const wxString& par, wxColour *clr) const;
    bool GetParamAsInt(const wxString& par, int *clr) const;
    bool GetParamAsIntOrPercent(const wxString& param,
                                int* value, bool& isPercent) const;

    // Scans param like scanf() functions family does.
    // Example : ScanParam("COLOR", "\"#%X\"", &clr);
    // This is always with with_quotes=false
    // Returns number of scanned values
    // (like sscanf() does)
    // NOTE: unlike scanf family, this function only accepts
    //       *one* parameter !
    int ScanParam(const wxString& par, const char *format, void *param) const;
    int ScanParam(const wxString& par, const wchar_t *format, void *param) const;

    // Returns string containing all params.
    wxString GetAllParams() const;

    // return true if there is matching ending tag
    inline bool HasEnding() const {return m_hasEnding;}

    // returns beginning position of _internal_ block of text as iterator
    // into parser's source string (see wxHtmlParser::GetSource())
    // See explanation (returned value is marked with *):
    // bla bla bla <MYTAG>* bla bla intenal text</MYTAG> bla bla
    wxString::const_iterator GetBeginIter() const
        { return m_Begin; }
    // returns ending position of _internal_ block of text as iterator
    // into parser's source string (see wxHtmlParser::GetSource()):
    // bla bla bla <MYTAG> bla bla intenal text*</MYTAG> bla bla
    wxString::const_iterator GetEndIter1() const { return m_End1; }
    // returns end position 2 as iterator
    // into parser's source string (see wxHtmlParser::GetSource()):
    // bla bla bla <MYTAG> bla bla internal text</MYTAG>* bla bla
    wxString::const_iterator GetEndIter2() const { return m_End2; }

#if WXWIN_COMPATIBILITY_2_8
    // use GetBeginIter(), GetEndIter1() and GetEndIter2() instead
    wxDEPRECATED( inline int GetBeginPos() const );
    wxDEPRECATED( inline int GetEndPos1() const );
    wxDEPRECATED( inline int GetEndPos2() const );
#endif // WXWIN_COMPATIBILITY_2_8

private:
    wxString m_Name;
    bool m_hasEnding;
    wxString::const_iterator m_Begin, m_End1, m_End2;
    wxArrayString m_ParamNames, m_ParamValues;
#if WXWIN_COMPATIBILITY_2_8
    wxString::const_iterator m_sourceStart;
#endif

    // DOM tree relations:
    wxHtmlTag *m_Next;
    wxHtmlTag *m_Prev;
    wxHtmlTag *m_FirstChild, *m_LastChild;
    wxHtmlTag *m_Parent;

    wxDECLARE_NO_COPY_CLASS(wxHtmlTag);
};


#if WXWIN_COMPATIBILITY_2_8
inline int wxHtmlTag::GetBeginPos() const { return int(m_Begin - m_sourceStart); }
inline int wxHtmlTag::GetEndPos1() const { return int(m_End1 - m_sourceStart); }
inline int wxHtmlTag::GetEndPos2() const { return int(m_End2 - m_sourceStart); }
#endif // WXWIN_COMPATIBILITY_2_8




#endif // wxUSE_HTML

#endif // _WX_HTMLTAG_H_

