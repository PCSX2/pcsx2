/////////////////////////////////////////////////////////////////////////////
// Name:        wx/html/htmlpars.h
// Purpose:     wxHtmlParser class (generic parser)
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HTMLPARS_H_
#define _WX_HTMLPARS_H_

#include "wx/defs.h"
#if wxUSE_HTML

#include "wx/html/htmltag.h"
#include "wx/filesys.h"
#include "wx/hashmap.h"
#include "wx/hashset.h"
#include "wx/vector.h"
#include "wx/fontenc.h"

class WXDLLIMPEXP_FWD_BASE wxMBConv;
class WXDLLIMPEXP_FWD_HTML wxHtmlParser;
class WXDLLIMPEXP_FWD_HTML wxHtmlTagHandler;
class WXDLLIMPEXP_FWD_HTML wxHtmlEntitiesParser;

class wxHtmlTextPieces;
class wxHtmlParserState;

WX_DECLARE_HASH_SET_WITH_DECL_PTR(wxHtmlTagHandler*,
                                  wxPointerHash, wxPointerEqual,
                                  wxHtmlTagHandlersSet,
                                  class WXDLLIMPEXP_HTML);
WX_DECLARE_STRING_HASH_MAP_WITH_DECL(wxHtmlTagHandler*,
                                     wxHtmlTagHandlersHash,
                                     class WXDLLIMPEXP_HTML);


enum wxHtmlURLType
{
    wxHTML_URL_PAGE,
    wxHTML_URL_IMAGE,
    wxHTML_URL_OTHER
};

// This class handles generic parsing of HTML document : it scans
// the document and divides it into blocks of tags (where one block
// consists of starting and ending tag and of text between these
// 2 tags.
class WXDLLIMPEXP_HTML wxHtmlParser : public wxObject
{
    DECLARE_ABSTRACT_CLASS(wxHtmlParser)

public:
    wxHtmlParser();
    virtual ~wxHtmlParser();

    // Sets the class which will be used for opening files
    void SetFS(wxFileSystem *fs) { m_FS = fs; }

    wxFileSystem* GetFS() const { return m_FS; }

    // Opens file if the parser is allowed to open given URL (may be forbidden
    // for security reasons)
    virtual wxFSFile *OpenURL(wxHtmlURLType type, const wxString& url) const;

    // You can simply call this method when you need parsed output.
    // This method does these things:
    // 1. call InitParser(source);
    // 2. call DoParsing();
    // 3. call GetProduct(); (its return value is then returned)
    // 4. call DoneParser();
    wxObject* Parse(const wxString& source);

    // Sets the source. This must be called before running Parse() method.
    virtual void InitParser(const wxString& source);
    // This must be called after Parse().
    virtual void DoneParser();

    // May be called during parsing to immediately return from Parse().
    virtual void StopParsing() { m_stopParsing = true; }

    // Parses the m_Source from begin_pos to end_pos-1.
    // (in noparams version it parses whole m_Source)
    void DoParsing(const wxString::const_iterator& begin_pos,
                   const wxString::const_iterator& end_pos);
    void DoParsing();

    // Returns pointer to the tag at parser's current position
    wxHtmlTag *GetCurrentTag() const { return m_CurTag; }

    // Returns product of parsing
    // Returned value is result of parsing of the part. The type of this result
    // depends on internal representation in derived parser
    // (see wxHtmlWinParser for details).
    virtual wxObject* GetProduct() = 0;

    // adds handler to the list & hash table of handlers.
    virtual void AddTagHandler(wxHtmlTagHandler *handler);

    // Forces the handler to handle additional tags (not returned by GetSupportedTags).
    // The handler should already be in use by this parser.
    // Example: you want to parse following pseudo-html structure:
    //   <myitems>
    //     <it name="one" value="1">
    //     <it name="two" value="2">
    //   </myitems>
    //   <it> This last it has different meaning, we don't want it to be parsed by myitems handler!
    // handler can handle only 'myitems' (e.g. its GetSupportedTags returns "MYITEMS")
    // you can call PushTagHandler(handler, "IT") when you find <myitems>
    // and call PopTagHandler() when you find </myitems>
    void PushTagHandler(wxHtmlTagHandler *handler, const wxString& tags);

    // Restores state before last call to PushTagHandler
    void PopTagHandler();

    const wxString* GetSource() {return m_Source;}
    void SetSource(const wxString& src);

    // Sets HTML source and remembers current parser's state so that it can
    // later be restored. This is useful for on-line modifications of
    // HTML source (for example, <pre> handler replaces spaces with &nbsp;
    // and newlines with <br>)
    virtual void SetSourceAndSaveState(const wxString& src);
    // Restores parser's state from stack or returns false if the stack is
    // empty
    virtual bool RestoreState();

    // Returns HTML source inside the element (i.e. between the starting
    // and ending tag)
    wxString GetInnerSource(const wxHtmlTag& tag);

    // Parses HTML string 'markup' and extracts charset info from <meta> tag
    // if present. Returns empty string if the tag is missing.
    // For wxHTML's internal use.
    static wxString ExtractCharsetInformation(const wxString& markup);

    // Returns entity parser object, used to substitute HTML &entities;
    wxHtmlEntitiesParser *GetEntitiesParser() const { return m_entitiesParser; }

    // Returns true if the tag starting at the given position is a comment tag
    //
    // p should point to '<' character and is modified to point to the closing
    // '>' of the end comment tag if this is indeed a comment
    static bool
    SkipCommentTag(wxString::const_iterator& p, wxString::const_iterator end);

protected:
    // DOM structure
    void CreateDOMTree();
    void DestroyDOMTree();
    void CreateDOMSubTree(wxHtmlTag *cur,
                          const wxString::const_iterator& begin_pos,
                          const wxString::const_iterator& end_pos,
                          wxHtmlTagsCache *cache);

    // Adds text to the output.
    // This is called from Parse() and must be overridden in derived classes.
    // txt is not guaranteed to be only one word. It is largest continuous part
    // of text (= not broken by tags)
    virtual void AddText(const wxString& txt) = 0;

    // Adds tag and proceeds it. Parse() may (and usually is) called from this method.
    // This is called from Parse() and may be overridden.
    // Default behaviour is that it looks for proper handler in m_Handlers. The tag is
    // ignored if no hander is found.
    // Derived class is *responsible* for filling in m_Handlers table.
    virtual void AddTag(const wxHtmlTag& tag);

protected:
    // DOM tree:
    wxHtmlTag *m_CurTag;
    wxHtmlTag *m_Tags;
    wxHtmlTextPieces *m_TextPieces;
    size_t m_CurTextPiece;

    const wxString *m_Source;

    wxHtmlParserState *m_SavedStates;

    // handlers that handle particular tags. The table is accessed by
    // key = tag's name.
    // This attribute MUST be filled by derived class otherwise it would
    // be empty and no tags would be recognized
    // (see wxHtmlWinParser for details about filling it)
    // m_HandlersHash is for random access based on knowledge of tag name (BR, P, etc.)
    //      it may (and often does) contain more references to one object
    // m_HandlersList is list of all handlers and it is guaranteed to contain
    //      only one reference to each handler instance.
    wxHtmlTagHandlersSet m_HandlersSet;
    wxHtmlTagHandlersHash m_HandlersHash;

    wxDECLARE_NO_COPY_CLASS(wxHtmlParser);

    // class for opening files (file system)
    wxFileSystem *m_FS;
    // handlers stack used by PushTagHandler and PopTagHandler
    wxVector<wxHtmlTagHandlersHash*> m_HandlersStack;

    // entity parse
    wxHtmlEntitiesParser *m_entitiesParser;

    // flag indicating that the parser should stop
    bool m_stopParsing;
};



// This class (and derived classes) cooperates with wxHtmlParser.
// Each recognized tag is passed to handler which is capable
// of handling it. Each tag is handled in 3 steps:
// 1. Handler will modifies state of parser
//    (using its public methods)
// 2. Parser parses source between starting and ending tag
// 3. Handler restores original state of the parser
class WXDLLIMPEXP_HTML wxHtmlTagHandler : public wxObject
{
    DECLARE_ABSTRACT_CLASS(wxHtmlTagHandler)

public:
    wxHtmlTagHandler() : wxObject () { m_Parser = NULL; }

    // Sets the parser.
    // NOTE : each _instance_ of handler is guaranteed to be called
    // only by one parser. This means you don't have to care about
    // reentrancy.
    virtual void SetParser(wxHtmlParser *parser)
        { m_Parser = parser; }

    // Get the parser associated with this tag handler.
    wxHtmlParser* GetParser() const { return m_Parser; }

    // Returns list of supported tags. The list is in uppercase and
    // tags are delimited by ','.
    // Example : "I,B,FONT,P"
    //   is capable of handling italic, bold, font and paragraph tags
    virtual wxString GetSupportedTags() = 0;

    // This is hadling core method. It does all the Steps 1-3.
    // To process step 2, you can call ParseInner()
    // returned value : true if it called ParseInner(),
    //                  false etherwise
    virtual bool HandleTag(const wxHtmlTag& tag) = 0;

protected:
    // parses input between beginning and ending tag.
    // m_Parser must be set.
    void ParseInner(const wxHtmlTag& tag)
        { m_Parser->DoParsing(tag.GetBeginIter(), tag.GetEndIter1()); }

    // Parses given source as if it was tag's inner code (see
    // wxHtmlParser::GetInnerSource).  Unlike ParseInner(), this method lets
    // you specify the source code to parse. This is useful when you need to
    // modify the inner text before parsing.
    void ParseInnerSource(const wxString& source);

    wxHtmlParser *m_Parser;

    wxDECLARE_NO_COPY_CLASS(wxHtmlTagHandler);
};


// This class is used to parse HTML entities in strings. It can handle
// both named entities and &#xxxx entries where xxxx is Unicode code.
class WXDLLIMPEXP_HTML wxHtmlEntitiesParser : public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxHtmlEntitiesParser)

public:
    wxHtmlEntitiesParser();
    virtual ~wxHtmlEntitiesParser();

    // Sets encoding of output string.
    // Has no effect if wxUSE_UNICODE==1
#if wxUSE_UNICODE
    void SetEncoding(wxFontEncoding WXUNUSED(encoding)) {}
#else
    void SetEncoding(wxFontEncoding encoding);
#endif

    // Parses entities in input and replaces them with respective characters
    // (with respect to output encoding)
    wxString Parse(const wxString& input) const;

    // Returns character for given entity or 0 if the enity is unknown
    wxChar GetEntityChar(const wxString& entity) const;

    // Returns character that represents given Unicode code
#if wxUSE_UNICODE
    wxChar GetCharForCode(unsigned code) const { return (wxChar)code; }
#else
    wxChar GetCharForCode(unsigned code) const;
#endif

protected:
#if !wxUSE_UNICODE
    wxMBConv *m_conv;
    wxFontEncoding m_encoding;
#endif

    wxDECLARE_NO_COPY_CLASS(wxHtmlEntitiesParser);
};


#endif

#endif // _WX_HTMLPARS_H_
