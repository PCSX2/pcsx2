/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/webview_missing.h
// Purpose:     Defintions / classes commonly missing used by wxWebViewIE
// Author:      Steven Lamerton
// Copyright:   (c) 2012 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/*
 * Classes and definitions used by wxWebViewIE vary in their
 * completeness between compilers and versions of compilers.
 * We implement our own versions here which should work
 * for all compilers. The definitions are taken from the
 * mingw-w64 headers which are public domain.
*/

/* urlmon.h */

struct IHTMLElement;
struct IHTMLDocument2;

#ifndef REFRESH_NORMAL
#define REFRESH_NORMAL 0
#endif

#ifndef REFRESH_COMPLETELY
#define REFRESH_COMPLETELY 3
#endif

typedef enum __wxMIDL_IBindStatusCallback_0006
{
    wxBSCF_FIRSTDATANOTIFICATION = 0x1,
    wxBSCF_INTERMEDIATEDATANOTIFICATION = 0x2,
    wxBSCF_LASTDATANOTIFICATION = 0x4,
    wxBSCF_DATAFULLYAVAILABLE = 0x8,
    wxBSCF_AVAILABLEDATASIZEUNKNOWN = 0x10
}   wxBSCF;

EXTERN_C const IID CLSID_FileProtocol;

typedef struct _tagwxBINDINFO
{
    ULONG cbSize;
    LPWSTR szExtraInfo;
    STGMEDIUM stgmedData;
    DWORD grfBindInfoF;
    DWORD dwBindVerb;
    LPWSTR szCustomVerb;
    DWORD cbstgmedData;
    DWORD dwOptions;
    DWORD dwOptionsFlags;
    DWORD dwCodePage;
    SECURITY_ATTRIBUTES securityAttributes;
    IID iid;
    IUnknown *pUnk;
    DWORD dwReserved;
}   wxBINDINFO;

typedef struct _tagwxPROTOCOLDATA
{
    DWORD grfFlags;
    DWORD dwState;
    LPVOID pData;
    ULONG cbData;
}   wxPROTOCOLDATA;

class wxIInternetBindInfo : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL GetBindInfo(DWORD *grfBINDF, wxBINDINFO *pbindinfo) = 0;
    virtual HRESULT wxSTDCALL GetBindString(ULONG ulStringType, LPOLESTR *ppwzStr,
                                            ULONG cEl, ULONG *pcElFetched) = 0;
};

class wxIInternetProtocolSink : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL Switch(wxPROTOCOLDATA *pProtocolData) = 0;
    virtual HRESULT wxSTDCALL ReportProgress(ULONG ulStatusCode,
            LPCWSTR szStatusText) = 0;
    virtual HRESULT wxSTDCALL ReportData(DWORD grfBSCF, ULONG ulProgress,
                                         ULONG ulProgressMax) = 0;
    virtual HRESULT wxSTDCALL ReportResult(HRESULT hrResult, DWORD dwError,
                                           LPCWSTR szResult) = 0;
};

class wxIInternetProtocolRoot : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL Start(LPCWSTR szUrl, wxIInternetProtocolSink *pOIProtSink,
                                    wxIInternetBindInfo *pOIBindInfo, DWORD grfPI,
                                    HANDLE_PTR dwReserved) = 0;
    virtual HRESULT wxSTDCALL Continue(wxPROTOCOLDATA *pProtocolData) = 0;
    virtual HRESULT wxSTDCALL Abort(HRESULT hrReason, DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL Terminate(DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL Suspend(void) = 0;
    virtual HRESULT wxSTDCALL Resume(void) = 0;
};


class wxIInternetProtocol : public wxIInternetProtocolRoot
{
public:
    virtual HRESULT wxSTDCALL Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
    virtual HRESULT wxSTDCALL Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                                   ULARGE_INTEGER *plibNewPosition) = 0;
    virtual HRESULT wxSTDCALL LockRequest(DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL UnlockRequest(void) = 0;
};


class wxIInternetSession : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL RegisterNameSpace(IClassFactory *pCF, REFCLSID rclsid,
            LPCWSTR pwzProtocol,
            ULONG cPatterns,
            const LPCWSTR *ppwzPatterns,
            DWORD dwReserved) = 0;
    virtual HRESULT wxSTDCALL UnregisterNameSpace(IClassFactory *pCF,
            LPCWSTR pszProtocol) = 0;
    virtual HRESULT wxSTDCALL RegisterMimeFilter(IClassFactory *pCF,
            REFCLSID rclsid,
            LPCWSTR pwzType) = 0;
    virtual HRESULT wxSTDCALL UnregisterMimeFilter(IClassFactory *pCF,
            LPCWSTR pwzType) = 0;
    virtual HRESULT wxSTDCALL CreateBinding(LPBC pBC, LPCWSTR szUrl,
                                            IUnknown *pUnkOuter, IUnknown **ppUnk,
                                            wxIInternetProtocol **ppOInetProt,
                                            DWORD dwOption) = 0;
    virtual HRESULT wxSTDCALL SetSessionOption(DWORD dwOption, LPVOID pBuffer,
            DWORD dwBufferLength,
            DWORD dwReserved) = 0;
    virtual HRESULT wxSTDCALL GetSessionOption(DWORD dwOption, LPVOID pBuffer,
            DWORD *pdwBufferLength,
            DWORD dwReserved) = 0;
};

/* end of urlmon.h */

/* mshtmhst.h */

typedef enum _tagwxDOCHOSTUIFLAG
{
    DOCHOSTUIFLAG_DIALOG = 0x1,
    DOCHOSTUIFLAG_DISABLE_HELP_MENU = 0x2,
    DOCHOSTUIFLAG_NO3DBORDER = 0x4,
    DOCHOSTUIFLAG_SCROLL_NO = 0x8,
    DOCHOSTUIFLAG_DISABLE_SCRIPT_INACTIVE = 0x10,
    DOCHOSTUIFLAG_OPENNEWWIN = 0x20,
    DOCHOSTUIFLAG_DISABLE_OFFSCREEN = 0x40,
    DOCHOSTUIFLAG_FLAT_SCROLLBAR = 0x80,
    DOCHOSTUIFLAG_DIV_BLOCKDEFAULT = 0x100,
    DOCHOSTUIFLAG_ACTIVATE_CLIENTHIT_ONLY = 0x200,
    DOCHOSTUIFLAG_OVERRIDEBEHAVIORFACTORY = 0x400,
    DOCHOSTUIFLAG_CODEPAGELINKEDFONTS = 0x800,
    DOCHOSTUIFLAG_URL_ENCODING_DISABLE_UTF8 = 0x1000,
    DOCHOSTUIFLAG_URL_ENCODING_ENABLE_UTF8 = 0x2000,
    DOCHOSTUIFLAG_ENABLE_FORMS_AUTOCOMPLETE = 0x4000,
    DOCHOSTUIFLAG_ENABLE_INPLACE_NAVIGATION = 0x10000,
    DOCHOSTUIFLAG_IME_ENABLE_RECONVERSION = 0x20000,
    DOCHOSTUIFLAG_THEME = 0x40000,
    DOCHOSTUIFLAG_NOTHEME = 0x80000,
    DOCHOSTUIFLAG_NOPICS = 0x100000,
    DOCHOSTUIFLAG_NO3DOUTERBORDER = 0x200000,
    DOCHOSTUIFLAG_DISABLE_EDIT_NS_FIXUP = 0x400000,
    DOCHOSTUIFLAG_LOCAL_MACHINE_ACCESS_CHECK = 0x800000,
    DOCHOSTUIFLAG_DISABLE_UNTRUSTEDPROTOCOL = 0x1000000
} DOCHOSTUIFLAG;

typedef struct _tagwxDOCHOSTUIINFO
{
    ULONG cbSize;
    DWORD dwFlags;
    DWORD dwDoubleClick;
    OLECHAR *pchHostCss;
    OLECHAR *pchHostNS;
} DOCHOSTUIINFO;

class wxIDocHostUIHandler : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL ShowContextMenu(DWORD dwID, POINT *ppt,
            IUnknown *pcmdtReserved,
            IDispatch *pdispReserved) = 0;

    virtual HRESULT wxSTDCALL GetHostInfo(DOCHOSTUIINFO *pInfo) = 0;

    virtual HRESULT wxSTDCALL ShowUI(DWORD dwID,
                                     IOleInPlaceActiveObject *pActiveObject,
                                     IOleCommandTarget *pCommandTarget,
                                     IOleInPlaceFrame *pFrame,
                                     IOleInPlaceUIWindow *pDoc) = 0;

    virtual HRESULT wxSTDCALL HideUI(void) = 0;

    virtual HRESULT wxSTDCALL UpdateUI(void) = 0;

    virtual HRESULT wxSTDCALL EnableModeless(BOOL fEnable) = 0;

    virtual HRESULT wxSTDCALL OnDocWindowActivate(BOOL fActivate) = 0;

    virtual HRESULT wxSTDCALL OnFrameWindowActivate(BOOL fActivate) = 0;

    virtual HRESULT wxSTDCALL ResizeBorder(LPCRECT prcBorder,
                                           IOleInPlaceUIWindow *pUIWindow,
                                           BOOL fRameWindow) = 0;

    virtual HRESULT wxSTDCALL TranslateAccelerator(LPMSG lpMsg,
            const GUID *pguidCmdGroup,
            DWORD nCmdID) = 0;

    virtual HRESULT wxSTDCALL GetOptionKeyPath(LPOLESTR *pchKey,
            DWORD dw) = 0;

    virtual HRESULT wxSTDCALL GetDropTarget(IDropTarget *pDropTarget,
                                            IDropTarget **ppDropTarget) = 0;

    virtual HRESULT wxSTDCALL GetExternal(IDispatch **ppDispatch) = 0;

    virtual HRESULT wxSTDCALL TranslateUrl(DWORD dwTranslate,
                                           OLECHAR *pchURLIn,
                                           OLECHAR **ppchURLOut) = 0;

    virtual HRESULT wxSTDCALL FilterDataObject(IDataObject *pDO,
            IDataObject **ppDORet) = 0;
};

/* end of mshtmhst.h */

/* mshtml.h */

typedef enum _tagwxPOINTER_GRAVITY
{
    wxPOINTER_GRAVITY_Left = 0,
    wxPOINTER_GRAVITY_Right = 1,
    wxPOINTER_GRAVITY_Max = 2147483647
} wxPOINTER_GRAVITY;

typedef enum _tagwxELEMENT_ADJACENCY
{
    wxELEM_ADJ_BeforeBegin = 0,
    wxELEM_ADJ_AfterBegin = 1,
    wxELEM_ADJ_BeforeEnd = 2,
    wxELEM_ADJ_AfterEnd = 3,
    wxELEMENT_ADJACENCY_Max = 2147483647
} wxELEMENT_ADJACENCY;

typedef enum _tagwxMARKUP_CONTEXT_TYPE
{
    wxCONTEXT_TYPE_None = 0,
    wxCONTEXT_TYPE_Text = 1,
    wxCONTEXT_TYPE_EnterScope = 2,
    wxCONTEXT_TYPE_ExitScope = 3,
    wxCONTEXT_TYPE_NoScope = 4,
    wxMARKUP_CONTEXT_TYPE_Max = 2147483647
} wxMARKUP_CONTEXT_TYPE;

typedef enum _tagwxFINDTEXT_FLAGS
{
    wxFINDTEXT_BACKWARDS = 0x1,
    wxFINDTEXT_WHOLEWORD = 0x2,
    wxFINDTEXT_MATCHCASE = 0x4,
    wxFINDTEXT_RAW = 0x20000,
    wxFINDTEXT_MATCHDIAC = 0x20000000,
    wxFINDTEXT_MATCHKASHIDA = 0x40000000,
    wxFINDTEXT_MATCHALEFHAMZA = 0x80000000,
    wxFINDTEXT_FLAGS_Max = 2147483647
} wxFINDTEXT_FLAGS;

typedef enum _tagwxMOVEUNIT_ACTION
{
    wxMOVEUNIT_PREVCHAR = 0,
    wxMOVEUNIT_NEXTCHAR = 1,
    wxMOVEUNIT_PREVCLUSTERBEGIN = 2,
    wxMOVEUNIT_NEXTCLUSTERBEGIN = 3,
    wxMOVEUNIT_PREVCLUSTEREND = 4,
    wxMOVEUNIT_NEXTCLUSTEREND = 5,
    wxMOVEUNIT_PREVWORDBEGIN = 6,
    wxMOVEUNIT_NEXTWORDBEGIN = 7,
    wxMOVEUNIT_PREVWORDEND = 8,
    wxMOVEUNIT_NEXTWORDEND = 9,
    wxMOVEUNIT_PREVPROOFWORD = 10,
    wxMOVEUNIT_NEXTPROOFWORD = 11,
    wxMOVEUNIT_NEXTURLBEGIN = 12,
    wxMOVEUNIT_PREVURLBEGIN = 13,
    wxMOVEUNIT_NEXTURLEND = 14,
    wxMOVEUNIT_PREVURLEND = 15,
    wxMOVEUNIT_PREVSENTENCE = 16,
    wxMOVEUNIT_NEXTSENTENCE = 17,
    wxMOVEUNIT_PREVBLOCK = 18,
    wxMOVEUNIT_NEXTBLOCK = 19,
    wxMOVEUNIT_ACTION_Max = 2147483647
} wxMOVEUNIT_ACTION;

typedef enum _tagwxELEMENT_TAG_ID
{
    wxTAGID_NULL = 0, wxTAGID_UNKNOWN = 1, wxTAGID_A = 2, wxTAGID_ACRONYM = 3,
    wxTAGID_ADDRESS = 4, wxTAGID_APPLET = 5, wxTAGID_AREA = 6, wxTAGID_B = 7,
    wxTAGID_BASE = 8, wxTAGID_BASEFONT = 9, wxTAGID_BDO = 10, 
    wxTAGID_BGSOUND = 11, wxTAGID_BIG = 12, wxTAGID_BLINK = 13, 
    wxTAGID_BLOCKQUOTE = 14, wxTAGID_BODY = 15, wxTAGID_BR = 16,
    wxTAGID_BUTTON = 17, wxTAGID_CAPTION = 18, wxTAGID_CENTER = 19,
    wxTAGID_CITE = 20, wxTAGID_CODE = 21, wxTAGID_COL = 22,
    wxTAGID_COLGROUP = 23, wxTAGID_COMMENT = 24, wxTAGID_COMMENT_RAW = 25,
    wxTAGID_DD = 26, wxTAGID_DEL = 27, wxTAGID_DFN = 28, wxTAGID_DIR = 29,
    wxTAGID_DIV = 30, wxTAGID_DL = 31, wxTAGID_DT = 32, wxTAGID_EM = 33,
    wxTAGID_EMBED = 34, wxTAGID_FIELDSET = 35, wxTAGID_FONT = 36,
    wxTAGID_FORM = 37, wxTAGID_FRAME = 38, wxTAGID_FRAMESET = 39,
    wxTAGID_GENERIC = 40, wxTAGID_H1 = 41, wxTAGID_H2 = 42, wxTAGID_H3 = 43,
    wxTAGID_H4 = 44, wxTAGID_H5 = 45, wxTAGID_H6 = 46, wxTAGID_HEAD = 47,
    wxTAGID_HR = 48, wxTAGID_HTML = 49, wxTAGID_I = 50, wxTAGID_IFRAME = 51,
    wxTAGID_IMG = 52, wxTAGID_INPUT = 53, wxTAGID_INS = 54, wxTAGID_KBD = 55,
    wxTAGID_LABEL = 56, wxTAGID_LEGEND = 57, wxTAGID_LI = 58, wxTAGID_LINK = 59,
    wxTAGID_LISTING = 60, wxTAGID_MAP = 61, wxTAGID_MARQUEE = 62,
    wxTAGID_MENU = 63, wxTAGID_META = 64, wxTAGID_NEXTID = 65,
    wxTAGID_NOBR = 66, wxTAGID_NOEMBED = 67, wxTAGID_NOFRAMES = 68,
    wxTAGID_NOSCRIPT = 69, wxTAGID_OBJECT = 70, wxTAGID_OL = 71,
    wxTAGID_OPTION = 72, wxTAGID_P = 73, wxTAGID_PARAM = 74,
    wxTAGID_PLAINTEXT = 75, wxTAGID_PRE = 76, wxTAGID_Q = 77, wxTAGID_RP = 78,
    wxTAGID_RT = 79, wxTAGID_RUBY = 80, wxTAGID_S = 81, wxTAGID_SAMP = 82,
    wxTAGID_SCRIPT = 83, wxTAGID_SELECT = 84, wxTAGID_SMALL = 85,
    wxTAGID_SPAN = 86, wxTAGID_STRIKE = 87, wxTAGID_STRONG = 88,
    wxTAGID_STYLE = 89, wxTAGID_SUB = 90, wxTAGID_SUP = 91, wxTAGID_TABLE = 92,
    wxTAGID_TBODY = 93, wxTAGID_TC = 94, wxTAGID_TD = 95, wxTAGID_TEXTAREA = 96,
    wxTAGID_TFOOT = 97, wxTAGID_TH = 98, wxTAGID_THEAD = 99,
    wxTAGID_TITLE = 100, wxTAGID_TR = 101, wxTAGID_TT = 102, wxTAGID_U = 103,
    wxTAGID_UL = 104, wxTAGID_VAR = 105, wxTAGID_WBR = 106, wxTAGID_XMP = 107,
    wxTAGID_ROOT = 108, wxTAGID_OPTGROUP = 109, wxTAGID_COUNT = 110,
    wxTAGID_LAST_PREDEFINED = 10000, wxELEMENT_TAG_ID_Max = 2147483647
} wxELEMENT_TAG_ID;

struct wxIHTMLStyle : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL put_fontFamily(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_fontFamily(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_fontStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_fontStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_fontVariant(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_fontVariant(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_fontWeight(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_fontWeight(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_fontSize(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_fontSize(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_font(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_font(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_color(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_color(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_background(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_background(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundColor(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundImage(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundImage(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundRepeat(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundRepeat(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundAttachment(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundAttachment(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundPosition(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundPosition(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundPositionX(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundPositionX(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_backgroundPositionY(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_backgroundPositionY(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_wordSpacing(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_wordSpacing(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_letterSpacing(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_letterSpacing(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecoration(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_textDecoration(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecorationNone(VARIANT_BOOL v) = 0;
    virtual HRESULT wxSTDCALL get_textDecorationNone(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecorationUnderline(VARIANT_BOOL v) = 0;
    virtual HRESULT wxSTDCALL get_textDecorationUnderline(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecorationOverline(VARIANT_BOOL v) = 0;
    virtual HRESULT wxSTDCALL get_textDecorationOverline(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecorationLineThrough(VARIANT_BOOL v) = 0;
    virtual HRESULT wxSTDCALL get_textDecorationLineThrough(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL put_textDecorationBlink(VARIANT_BOOL v) = 0;
    virtual HRESULT wxSTDCALL get_textDecorationBlink(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL put_verticalAlign(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_verticalAlign(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_textTransform(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_textTransform(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_textAlign(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_textAlign(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_textIndent(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_textIndent(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_lineHeight(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_lineHeight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_marginTop(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_marginTop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_marginRight(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_marginRight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_marginBottom(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_marginBottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_marginLeft(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_marginLeft(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_margin(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_margin(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_paddingTop(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_paddingTop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_paddingRight(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_paddingRight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_paddingBottom(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_paddingBottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_paddingLeft(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_paddingLeft(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_padding(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_padding(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_border(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_border(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderTop(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderTop(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderRight(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderRight(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderBottom(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderBottom(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderLeft(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderLeft(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderColor(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderColor(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderTopColor(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderTopColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderRightColor(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderRightColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderBottomColor(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderLeftColor(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderWidth(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderWidth(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderTopWidth(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderTopWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderRightWidth(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderRightWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderBottomWidth(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderLeftWidth(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_borderStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderTopStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderTopStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderRightStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderRightStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderBottomStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_borderLeftStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_width(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_width(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_height(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_height(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_styleFloat(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_styleFloat(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_clear(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_clear(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_display(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_display(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_visibility(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_visibility(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_listStyleType(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_listStyleType(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_listStylePosition(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_listStylePosition(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_listStyleImage(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_listStyleImage(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_listStyle(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_listStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_whiteSpace(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_whiteSpace(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_top(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_top(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_left(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_left(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_position(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_zIndex(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_zIndex(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_overflow(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_overflow(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_pageBreakBefore(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_pageBreakBefore(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_pageBreakAfter(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_pageBreakAfter(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_cssText(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_cssText(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_pixelTop(long v) = 0;
    virtual HRESULT wxSTDCALL get_pixelTop(long *p) = 0;
    virtual HRESULT wxSTDCALL put_pixelLeft(long v) = 0;
    virtual HRESULT wxSTDCALL get_pixelLeft(long *p) = 0;
    virtual HRESULT wxSTDCALL put_pixelWidth(long v) = 0;
    virtual HRESULT wxSTDCALL get_pixelWidth(long *p) = 0;
    virtual HRESULT wxSTDCALL put_pixelHeight(long v) = 0;
    virtual HRESULT wxSTDCALL get_pixelHeight(long *p) = 0;
    virtual HRESULT wxSTDCALL put_posTop(float v) = 0;
    virtual HRESULT wxSTDCALL get_posTop(float *p) = 0;
    virtual HRESULT wxSTDCALL put_posLeft(float v) = 0;
    virtual HRESULT wxSTDCALL get_posLeft(float *p) = 0;
    virtual HRESULT wxSTDCALL put_posWidth(float v) = 0;
    virtual HRESULT wxSTDCALL get_posWidth(float *p) = 0;
    virtual HRESULT wxSTDCALL put_posHeight(float v) = 0;
    virtual HRESULT wxSTDCALL get_posHeight(float *p) = 0;
    virtual HRESULT wxSTDCALL put_cursor(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_cursor(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_clip(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_clip(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_filter(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_filter(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL setAttribute(BSTR strAttributeName, VARIANT AttributeValue, LONG lFlags = 1) = 0;
    virtual HRESULT wxSTDCALL getAttribute(BSTR strAttributeName, LONG lFlags, VARIANT *AttributeValue) = 0;
    virtual HRESULT wxSTDCALL removeAttribute(BSTR strAttributeName, LONG lFlags, VARIANT_BOOL *pfSuccess) = 0;
    virtual HRESULT wxSTDCALL toString(BSTR *String) = 0;
};

struct wxIHTMLCurrentStyle : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL get_position(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_styleFloat(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_color(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_fontFamily(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_fontStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_fontVariant(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_fontWeight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_fontSize(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundImage(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundPositionX(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundPositionY(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundRepeat(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderTopColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderRightColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomColor(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderTopStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderRightStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderTopWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderRightWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderBottomWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_borderLeftWidth(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_left(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_top(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_width(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_height(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_paddingLeft(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_paddingTop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_paddingRight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_paddingBottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_textAlign(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textDecoration(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_display(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_visibility(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_zIndex(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_letterSpacing(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_lineHeight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_textIndent(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_verticalAlign(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_backgroundAttachment(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_marginTop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_marginRight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_marginBottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_marginLeft(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_clear(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_listStyleType(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_listStylePosition(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_listStyleImage(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_clipTop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_clipRight(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_clipBottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_clipLeft(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_overflow(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_pageBreakBefore(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_pageBreakAfter(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_cursor(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_tableLayout(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderCollapse(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_direction(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_behavior(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL getAttribute(BSTR strAttributeName, LONG lFlags, VARIANT *AttributeValue) = 0;
    virtual HRESULT wxSTDCALL get_unicodeBidi(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_right(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_bottom(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_imeMode(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_rubyAlign(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_rubyPosition(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_rubyOverhang(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textAutospace(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_lineBreak(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_wordBreak(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textJustify(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textJustifyTrim(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textKashida(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_blockDirection(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_layoutGridChar(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_layoutGridLine(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_layoutGridMode(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_layoutGridType(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderStyle(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderColor(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_borderWidth(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_padding(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_margin(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_accelerator(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_overflowX(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_overflowY(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL get_textTransform(BSTR *p) = 0;
};


struct wxIHTMLRect : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL put_left(long v) = 0;
    virtual HRESULT wxSTDCALL get_left(long *p) = 0;
    virtual HRESULT wxSTDCALL put_top(long v) = 0;
    virtual HRESULT wxSTDCALL get_top(long *p) = 0;
    virtual HRESULT wxSTDCALL put_right(long v) = 0;
    virtual HRESULT wxSTDCALL get_right(long *p) = 0;
    virtual HRESULT wxSTDCALL put_bottom(long v) = 0;
    virtual HRESULT wxSTDCALL get_bottom(long *p) = 0;
};

struct wxIHTMLRectCollection : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL get_length(long *p) = 0;
    virtual HRESULT wxSTDCALL get__newEnum(IUnknown **p) = 0;
    virtual HRESULT wxSTDCALL item(VARIANT *pvarIndex, VARIANT *pvarResult) = 0;
};

struct wxIHTMLFiltersCollection : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL get_length(long *p) = 0;
    virtual HRESULT wxSTDCALL get__newEnum(IUnknown **p) = 0;
    virtual HRESULT wxSTDCALL item(VARIANT *pvarIndex, VARIANT *pvarResult) = 0;
};

struct wxIHTMLElementCollection : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL toString(BSTR *String) = 0;
    virtual HRESULT wxSTDCALL put_length(long v) = 0;
    virtual HRESULT wxSTDCALL get_length(long *p) = 0;
    virtual HRESULT wxSTDCALL get__newEnum(IUnknown **p) = 0;
    virtual HRESULT wxSTDCALL item(VARIANT name, VARIANT index, IDispatch **pdisp) = 0;
    virtual HRESULT wxSTDCALL tags(VARIANT tagName, IDispatch **pdisp) = 0;
};

struct wxIHTMLElement2 : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL get_scopeName(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL setCapture(VARIANT_BOOL containerCapture = -1) = 0;
    virtual HRESULT wxSTDCALL releaseCapture(void) = 0;
    virtual HRESULT wxSTDCALL put_onlosecapture(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onlosecapture(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL componentFromPoint(long x, long y, BSTR *component) = 0;
    virtual HRESULT wxSTDCALL doScroll(VARIANT component) = 0;
    virtual HRESULT wxSTDCALL put_onscroll(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onscroll(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondrag(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondrag(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondragend(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondragend(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondragenter(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondragenter(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondragover(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondragover(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondragleave(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondragleave(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_ondrop(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_ondrop(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onbeforecut(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onbeforecut(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_oncut(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_oncut(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onbeforecopy(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onbeforecopy(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_oncopy(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_oncopy(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onbeforepaste(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onbeforepaste(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onpaste(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onpaste(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_currentStyle(wxIHTMLCurrentStyle **p) = 0;
    virtual HRESULT wxSTDCALL put_onpropertychange(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onpropertychange(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL getClientRects(wxIHTMLRectCollection **pRectCol) = 0;
    virtual HRESULT wxSTDCALL getBoundingClientRect(wxIHTMLRect **pRect) = 0;
    virtual HRESULT wxSTDCALL setExpression(BSTR propname, BSTR expression, BSTR language = L"") = 0;
    virtual HRESULT wxSTDCALL getExpression(BSTR propname, VARIANT *expression) = 0;
    virtual HRESULT wxSTDCALL removeExpression(BSTR propname, VARIANT_BOOL *pfSuccess) = 0;
    virtual HRESULT wxSTDCALL put_tabIndex(short v) = 0;
    virtual HRESULT wxSTDCALL get_tabIndex(short *p) = 0;
    virtual HRESULT wxSTDCALL focus(void) = 0;
    virtual HRESULT wxSTDCALL put_accessKey(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_accessKey(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_onblur(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onblur(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onfocus(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onfocus(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onresize(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onresize(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL blur(void) = 0;
    virtual HRESULT wxSTDCALL addFilter(IUnknown *pUnk) = 0;
    virtual HRESULT wxSTDCALL removeFilter(IUnknown *pUnk) = 0;
    virtual HRESULT wxSTDCALL get_clientHeight(long *p) = 0;
    virtual HRESULT wxSTDCALL get_clientWidth(long *p) = 0;
    virtual HRESULT wxSTDCALL get_clientTop(long *p) = 0;
    virtual HRESULT wxSTDCALL get_clientLeft(long *p) = 0;
    virtual HRESULT wxSTDCALL attachEvent(BSTR event, IDispatch *pDisp, VARIANT_BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL detachEvent(BSTR event, IDispatch *pDisp) = 0;
    virtual HRESULT wxSTDCALL get_readyState(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onreadystatechange(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onreadystatechange(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onrowsdelete(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onrowsdelete(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_onrowsinserted(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onrowsinserted(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_oncellchange(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_oncellchange(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL put_dir(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_dir(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL createControlRange(IDispatch **range) = 0;
    virtual HRESULT wxSTDCALL get_scrollHeight(long *p) = 0;
    virtual HRESULT wxSTDCALL get_scrollWidth(long *p) = 0;
    virtual HRESULT wxSTDCALL put_scrollTop(long v) = 0;
    virtual HRESULT wxSTDCALL get_scrollTop(long *p) = 0;
    virtual HRESULT wxSTDCALL put_scrollLeft(long v) = 0;
    virtual HRESULT wxSTDCALL get_scrollLeft(long *p) = 0;
    virtual HRESULT wxSTDCALL clearAttributes(void) = 0;
    virtual HRESULT wxSTDCALL mergeAttributes(IHTMLElement *mergeThis) = 0;
    virtual HRESULT wxSTDCALL put_oncontextmenu(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_oncontextmenu(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL insertAdjacentElement(BSTR where, IHTMLElement *insertedElement, IHTMLElement **inserted) = 0;
    virtual HRESULT wxSTDCALL applyElement(IHTMLElement *apply, BSTR where, IHTMLElement **applied) = 0;
    virtual HRESULT wxSTDCALL getAdjacentText(BSTR where, BSTR *text) = 0;
    virtual HRESULT wxSTDCALL replaceAdjacentText(BSTR where, BSTR newText, BSTR *oldText) = 0;
    virtual HRESULT wxSTDCALL get_canHaveChildren(VARIANT_BOOL *p) = 0;
    virtual HRESULT wxSTDCALL addBehavior(BSTR bstrUrl, VARIANT *pvarFactory, long *pCookie) = 0;
    virtual HRESULT wxSTDCALL removeBehavior(long cookie, VARIANT_BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL get_runtimeStyle(wxIHTMLStyle **p) = 0;
    virtual HRESULT wxSTDCALL get_behaviorUrns(IDispatch **p) = 0;
    virtual HRESULT wxSTDCALL put_tagUrn(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_tagUrn(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_onbeforeeditfocus(VARIANT v) = 0;
    virtual HRESULT wxSTDCALL get_onbeforeeditfocus(VARIANT *p) = 0;
    virtual HRESULT wxSTDCALL get_readyStateValue(long *p) = 0;
    virtual HRESULT wxSTDCALL getElementsByTagName(BSTR v,
            wxIHTMLElementCollection **pelColl) = 0;
};

struct wxIHTMLTxtRange : public IDispatch
{
public:
    virtual HRESULT wxSTDCALL get_htmlText(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL put_text(BSTR v) = 0;
    virtual HRESULT wxSTDCALL get_text(BSTR *p) = 0;
    virtual HRESULT wxSTDCALL parentElement(IHTMLElement **parent) = 0;
    virtual HRESULT wxSTDCALL duplicate(wxIHTMLTxtRange **Duplicate) = 0;
    virtual HRESULT wxSTDCALL inRange(wxIHTMLTxtRange *Range, VARIANT_BOOL *InRange) = 0;
    virtual HRESULT wxSTDCALL isEqual(wxIHTMLTxtRange *Range, VARIANT_BOOL *IsEqual) = 0;
    virtual HRESULT wxSTDCALL scrollIntoView(VARIANT_BOOL fStart = -1) = 0;
    virtual HRESULT wxSTDCALL collapse(VARIANT_BOOL Start = -1) = 0;
    virtual HRESULT wxSTDCALL expand(BSTR Unit, VARIANT_BOOL *Success) = 0;
    virtual HRESULT wxSTDCALL move(BSTR Unit, long Count, long *ActualCount) = 0;
    virtual HRESULT wxSTDCALL moveStart(BSTR Unit, long Count, long *ActualCount) = 0;
    virtual HRESULT wxSTDCALL moveEnd(BSTR Unit, long Count, long *ActualCount) = 0;
    virtual HRESULT wxSTDCALL select(void) = 0;
    virtual HRESULT wxSTDCALL pasteHTML(BSTR html) = 0;
    virtual HRESULT wxSTDCALL moveToElementText(IHTMLElement *element) = 0;
    virtual HRESULT wxSTDCALL setEndPoint(BSTR how, wxIHTMLTxtRange *SourceRange) = 0;
    virtual HRESULT wxSTDCALL compareEndPoints(BSTR how, wxIHTMLTxtRange *SourceRange, long *ret) = 0;
    virtual HRESULT wxSTDCALL findText(BSTR String, long count, long Flags, VARIANT_BOOL *Success) = 0;
    virtual HRESULT wxSTDCALL moveToPoint(long x, long y) = 0;
    virtual HRESULT wxSTDCALL getBookmark(BSTR *Boolmark) = 0;
    virtual HRESULT wxSTDCALL moveToBookmark(BSTR Bookmark, VARIANT_BOOL *Success) = 0;
    virtual HRESULT wxSTDCALL queryCommandSupported(BSTR cmdID, VARIANT_BOOL *pfRet) = 0;
    virtual HRESULT wxSTDCALL queryCommandEnabled(BSTR cmdID, VARIANT_BOOL *pfRet) = 0;
    virtual HRESULT wxSTDCALL queryCommandState(BSTR cmdID, VARIANT_BOOL *pfRet) = 0;
    virtual HRESULT wxSTDCALL queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL *pfRet) = 0;
    virtual HRESULT wxSTDCALL queryCommandText(BSTR cmdID, BSTR *pcmdText) = 0;
    virtual HRESULT wxSTDCALL queryCommandValue(BSTR cmdID, VARIANT *pcmdValue) = 0;
    virtual HRESULT wxSTDCALL execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL *pfRet) = 0;
    virtual HRESULT wxSTDCALL execCommandShowHelp(BSTR cmdID, VARIANT_BOOL *pfRet) = 0;
};

struct wxIMarkupContainer : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL OwningDoc(IHTMLDocument2 **ppDoc) = 0;
};

struct wxIMarkupPointer : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL OwningDoc(IHTMLDocument2 **ppDoc) = 0;
    virtual HRESULT wxSTDCALL Gravity(wxPOINTER_GRAVITY *pGravity) = 0;
    virtual HRESULT wxSTDCALL SetGravity(wxPOINTER_GRAVITY Gravity) = 0;
    virtual HRESULT wxSTDCALL Cling(BOOL *pfCling) = 0;
    virtual HRESULT wxSTDCALL SetCling(BOOL fCLing) = 0;
    virtual HRESULT wxSTDCALL Unposition(void) = 0;
    virtual HRESULT wxSTDCALL IsPositioned(BOOL *pfPositioned) = 0;
    virtual HRESULT wxSTDCALL GetContainer(wxIMarkupContainer **ppContainer) = 0;
    virtual HRESULT wxSTDCALL MoveAdjacentToElement(IHTMLElement *pElement, wxELEMENT_ADJACENCY eAdj) = 0;
    virtual HRESULT wxSTDCALL MoveToPointer(wxIMarkupPointer *pPointer) = 0;
    virtual HRESULT wxSTDCALL MoveToContainer(wxIMarkupContainer *pContainer, BOOL fAtStart) = 0;
    virtual HRESULT wxSTDCALL Left(BOOL fMove, wxMARKUP_CONTEXT_TYPE *pContext, IHTMLElement **ppElement, long *pcch, OLECHAR *pchText) = 0;
    virtual HRESULT wxSTDCALL Right(BOOL fMove, wxMARKUP_CONTEXT_TYPE *pContext, IHTMLElement **ppElement, long *pcch, OLECHAR *pchText) = 0;
    virtual HRESULT wxSTDCALL CurrentScope(IHTMLElement **ppElemCurrent) = 0;
    virtual HRESULT wxSTDCALL IsLeftOf(wxIMarkupPointer *pPointerThat, BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL IsLeftOfOrEqualTo(wxIMarkupPointer *pPointerThat, BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL IsRightOf(wxIMarkupPointer *pPointerThat, BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL IsRightOfOrEqualTo(wxIMarkupPointer *pPointerThat, BOOL *pfResult) = 0;
    virtual HRESULT wxSTDCALL IsEqualTo(wxIMarkupPointer *pPointerThat, BOOL *pfAreEqual) = 0;
    virtual HRESULT wxSTDCALL MoveUnit(wxMOVEUNIT_ACTION muAction) = 0;
    virtual HRESULT wxSTDCALL FindText(OLECHAR *pchFindText, DWORD dwFlags, wxIMarkupPointer *pIEndMatch, wxIMarkupPointer *pIEndSearch) = 0;
};

struct wxIMarkupServices : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL CreateMarkupPointer(wxIMarkupPointer **ppPointer) = 0;
    virtual HRESULT wxSTDCALL CreateMarkupContainer(wxIMarkupContainer **ppMarkupContainer) = 0;
    virtual HRESULT wxSTDCALL CreateElement(wxELEMENT_TAG_ID tagID, OLECHAR *pchAttributes, IHTMLElement **ppElement) = 0;
    virtual HRESULT wxSTDCALL CloneElement(IHTMLElement *pElemCloneThis, IHTMLElement **ppElementTheClone) = 0;
    virtual HRESULT wxSTDCALL InsertElement(IHTMLElement *pElementInsert, wxIMarkupPointer *pPointerStart, wxIMarkupPointer *pPointerFinish) = 0;
    virtual HRESULT wxSTDCALL RemoveElement(IHTMLElement *pElementRemove) = 0;
    virtual HRESULT wxSTDCALL Remove(wxIMarkupPointer *pPointerStart, wxIMarkupPointer *pPointerFinish) = 0;
    virtual HRESULT wxSTDCALL Copy(wxIMarkupPointer *pPointerSourceStart, wxIMarkupPointer *pPointerSourceFinish, wxIMarkupPointer *pPointerTarget) = 0;
    virtual HRESULT wxSTDCALL Move(wxIMarkupPointer *pPointerSourceStart, wxIMarkupPointer *pPointerSourceFinish, wxIMarkupPointer *pPointerTarget) = 0;
    virtual HRESULT wxSTDCALL InsertText(OLECHAR *pchText, long cch, wxIMarkupPointer *pPointerTarget) = 0;
    virtual HRESULT wxSTDCALL ParseString(OLECHAR *pchHTML, DWORD dwFlags, wxIMarkupContainer **ppContainerResult, wxIMarkupPointer *ppPointerStart, wxIMarkupPointer *ppPointerFinish) = 0;
    virtual HRESULT wxSTDCALL ParseGlobal(HGLOBAL hglobalHTML, DWORD dwFlags, wxIMarkupContainer **ppContainerResult, wxIMarkupPointer *pPointerStart, wxIMarkupPointer *pPointerFinish) = 0;
    virtual HRESULT wxSTDCALL IsScopedElement(IHTMLElement *pElement, BOOL *pfScoped) = 0;
    virtual HRESULT wxSTDCALL GetElementTagId(IHTMLElement *pElement, wxELEMENT_TAG_ID *ptagId) = 0;
    virtual HRESULT wxSTDCALL GetTagIDForName(BSTR bstrName, wxELEMENT_TAG_ID *ptagId) = 0;
    virtual HRESULT wxSTDCALL GetNameForTagID(wxELEMENT_TAG_ID tagId, BSTR *pbstrName) = 0;
    virtual HRESULT wxSTDCALL MovePointersToRange(wxIHTMLTxtRange *pIRange, wxIMarkupPointer *pPointerStart, wxIMarkupPointer *pPointerFinish) = 0;
    virtual HRESULT wxSTDCALL MoveRangeToPointers(wxIMarkupPointer *pPointerStart, wxIMarkupPointer *pPointerFinish, wxIHTMLTxtRange *pIRange) = 0;
    virtual HRESULT wxSTDCALL BeginUndoUnit(OLECHAR *pchTitle) = 0;
    virtual HRESULT wxSTDCALL EndUndoUnit(void) = 0;
};

/* end of mshtml.h */

/* WinInet.h */

#ifndef HTTP_STATUS_BAD_REQUEST
#define HTTP_STATUS_BAD_REQUEST 400
#endif

#ifndef HTTP_STATUS_DENIED
#define HTTP_STATUS_DENIED 401
#endif

#ifndef HTTP_STATUS_PAYMENT_REQ
#define HTTP_STATUS_PAYMENT_REQ 402
#endif

#ifndef HTTP_STATUS_FORBIDDEN
#define HTTP_STATUS_FORBIDDEN 403
#endif

#ifndef HTTP_STATUS_NOT_FOUND
#define HTTP_STATUS_NOT_FOUND 404
#endif

#ifndef HTTP_STATUS_BAD_METHOD
#define HTTP_STATUS_BAD_METHOD 405
#endif

#ifndef HTTP_STATUS_NONE_ACCEPTABLE
#define HTTP_STATUS_NONE_ACCEPTABLE 406
#endif

#ifndef HTTP_STATUS_PROXY_AUTH_REQ
#define HTTP_STATUS_PROXY_AUTH_REQ 407
#endif

#ifndef HTTP_STATUS_REQUEST_TIMEOUT
#define HTTP_STATUS_REQUEST_TIMEOUT 408
#endif

#ifndef HTTP_STATUS_CONFLICT
#define HTTP_STATUS_CONFLICT 409
#endif

#ifndef HTTP_STATUS_GONE
#define HTTP_STATUS_GONE 410
#endif

#ifndef HTTP_STATUS_LENGTH_REQUIRED
#define HTTP_STATUS_LENGTH_REQUIRED 411
#endif

#ifndef HTTP_STATUS_PRECOND_FAILED
#define HTTP_STATUS_PRECOND_FAILED 412
#endif

#ifndef HTTP_STATUS_REQUEST_TOO_LARGE
#define HTTP_STATUS_REQUEST_TOO_LARGE 413
#endif

#ifndef HTTP_STATUS_URI_TOO_LONG
#define HTTP_STATUS_URI_TOO_LONG 414
#endif

#ifndef HTTP_STATUS_UNSUPPORTED_MEDIA
#define HTTP_STATUS_UNSUPPORTED_MEDIA 415
#endif

#ifndef HTTP_STATUS_RETRY_WITH
#define HTTP_STATUS_RETRY_WITH 449
#endif

#ifndef HTTP_STATUS_SERVER_ERROR
#define HTTP_STATUS_SERVER_ERROR 500
#endif

#ifndef HTTP_STATUS_NOT_SUPPORTED
#define HTTP_STATUS_NOT_SUPPORTED 501
#endif

#ifndef HTTP_STATUS_BAD_GATEWAY
#define HTTP_STATUS_BAD_GATEWAY 502
#endif

#ifndef HTTP_STATUS_SERVICE_UNAVAIL
#define HTTP_STATUS_SERVICE_UNAVAIL 503
#endif

#ifndef HTTP_STATUS_GATEWAY_TIMEOUT
#define HTTP_STATUS_GATEWAY_TIMEOUT 504
#endif

#ifndef HTTP_STATUS_VERSION_NOT_SUP
#define HTTP_STATUS_VERSION_NOT_SUP 505
#endif

/* end of WinInet.h */

