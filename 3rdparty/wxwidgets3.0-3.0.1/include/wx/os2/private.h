/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWidgets itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_PRIVATE_H_
#define _WX_OS2_PRIVATE_H_

#define INCL_BASE
#define INCL_PM
#define INCL_GPI
#define INCL_WINSYS
#define INCL_GPIERRORS
#define INCL_DOS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_WINATOM
#define INCL_SHLERRORS

#include <os2.h>

#if wxONLY_WATCOM_EARLIER_THAN(1,4)
    inline HATOMTBL APIENTRY WinQuerySystemAtomTable(VOID){return NULL;}
    inline ULONG APIENTRY WinQueryAtomName(HATOMTBL,ATOM,PCSZ,ULONG){return 0;}
    inline LONG APIENTRY GpiPointArc(HPS,PPOINTL){return GPI_ERROR;}
    inline BOOL APIENTRY WinDrawPointer(HPS,LONG,LONG,HPOINTER,ULONG){return FALSE;}
    inline HPOINTER APIENTRY WinCreatePointerIndirect(HWND,PPOINTERINFO){return NULLHANDLE;}
    inline BOOL APIENTRY WinGetMaxPosition(HWND,PSWP){return FALSE;}
    inline BOOL APIENTRY WinGetMinPosition(HWND,PSWP,PPOINTL){return FALSE;}
#endif

#if defined(__WATCOMC__) && defined(__WXMOTIF__)
    #include <os2def.h>
    #define I_NEED_OS2_H
    #include <X11/Xmd.h>

    // include this header from here for many of the GUI related code
    #if wxUSE_GUI
        extern "C" {
            #include <Xm/VendorSP.h>
        }
    #endif

    // provide Unix-like pipe()
    #include <types.h>
    #include <tcpustd.h>
    #include <sys/time.h>
    // Use ::DosCreatePipe or ::DosCreateNPipe under OS/2
    // for more see http://posix2.sourceforge.net/guide.html
    inline int pipe( int WXUNUSED(filedes)[2] )
    {
        wxFAIL_MSG(wxT("Implement first"));
        return -1;
    }
#endif

#if defined (__EMX__) && !defined(USE_OS2_TOOLKIT_HEADERS) && !defined(HAVE_SPBCDATA)

    typedef struct _SPBCDATA {
        ULONG     cbSize;       /*  Size of control block. */
        ULONG     ulTextLimit;  /*  Entryfield text limit. */
        LONG      lLowerLimit;  /*  Spin lower limit (numeric only). */
        LONG      lUpperLimit;  /*  Spin upper limit (numeric only). */
        ULONG     idMasterSpb;  /*  ID of the servant's master spinbutton. */
        PVOID     pHWXCtlData;  /*  Handwriting control data structure flag. */
    } SPBCDATA;

    typedef SPBCDATA *PSPBCDATA;

#endif

#include "wx/dlimpexp.h"
#include "wx/fontenc.h"

class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_CORE wxBitmap;

// ---------------------------------------------------------------------------
// private constants
// ---------------------------------------------------------------------------

//
// Constant strings for control names and classes
//

//
// Controls
//
WXDLLIMPEXP_DATA_CORE(extern const char)   wxButtonNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxCheckBoxNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxChoiceNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxComboBoxNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxDialogNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxFrameNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxGaugeNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxStaticBoxNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxListBoxNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxStaticLineNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxStaticTextNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxStaticBitmapNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxPanelNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxRadioBoxNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxRadioButtonNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxBitmapRadioButtonNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxScrollBarNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxSliderNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxTextCtrlNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxToolBarNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxStatusLineNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxGetTextFromUserPromptStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxMessageBoxCaptionStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxFileSelectorPromptStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxFileSelectorDefaultWildcardStr[];
WXDLLIMPEXP_DATA_CORE(extern const wxChar*) wxInternalErrorStr;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*) wxFatalErrorStr;
WXDLLIMPEXP_DATA_CORE(extern const char)   wxTreeCtrlNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxDirDialogNameStr[];
WXDLLIMPEXP_DATA_CORE(extern const char)   wxDirDialogDefaultFolderStr[];

//
// Class names
//
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxFrameClassName;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxFrameClassNameNoRedraw;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxMDIFrameClassName;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxMDIFrameClassNameNoRedraw;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxMDIChildFrameClassName;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxMDIChildFrameClassNameNoRedraw;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxPanelClassName;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxPanelClassNameNR;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxCanvasClassName;
WXDLLIMPEXP_DATA_CORE(extern const wxChar*)  wxCanvasClassNameNR;

// ---------------------------------------------------------------------------
// standard icons from the resources
// ---------------------------------------------------------------------------

#ifdef __WXPM__

WXDLLIMPEXP_DATA_CORE(extern HICON) wxSTD_FRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HICON) wxSTD_MDIPARENTFRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HICON) wxSTD_MDICHILDFRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HICON) wxDEFAULT_FRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HICON) wxDEFAULT_MDIPARENTFRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HICON) wxDEFAULT_MDICHILDFRAME_ICON;
WXDLLIMPEXP_DATA_CORE(extern HFONT) wxSTATUS_LINE_FONT;

#endif

// ---------------------------------------------------------------------------
// this defines a CASTWNDPROC macro which casts a pointer to the type of a
// window proc for PM.
// MPARAM is a void * but is really a 32-bit value
// ---------------------------------------------------------------------------

typedef MRESULT (APIENTRY * WndProcCast) (HWND, ULONG, MPARAM, MPARAM);
#define CASTWNDPROC (WndProcCast)

/*
 * Decide what window classes we're going to use
 * for this combination of CTl3D/FAFA settings
 */

#define STATIC_CLASS     wxT("STATIC")
#define STATIC_FLAGS     (SS_TEXT|DT_LEFT|SS_LEFT|WS_VISIBLE)
#define CHECK_CLASS      wxT("BUTTON")
#define CHECK_FLAGS      (BS_AUTOCHECKBOX|WS_TABSTOP)
#define CHECK_IS_FAFA    FALSE
#define RADIO_CLASS      wxT("BUTTON" )
#define RADIO_FLAGS      (BS_AUTORADIOBUTTON|WS_VISIBLE)
#define RADIO_SIZE       20
#define RADIO_IS_FAFA    FALSE
#define PURE_WINDOWS
/*  PM has no group box button style
#define GROUP_CLASS      "BUTTON"
#define GROUP_FLAGS      (BS_GROUPBOX|WS_CHILD|WS_VISIBLE)
*/

/*
#define BITCHECK_FLAGS   (FB_BITMAP|FC_BUTTONDRAW|FC_DEFAULT|WS_VISIBLE)
#define BITRADIO_FLAGS   (FC_BUTTONDRAW|FB_BITMAP|FC_RADIO|WS_CHILD|WS_VISIBLE)
*/

// ---------------------------------------------------------------------------
// misc macros
// ---------------------------------------------------------------------------

#define MEANING_CHARACTER '0'
#define DEFAULT_ITEM_WIDTH  200
#define DEFAULT_ITEM_HEIGHT 80

// Scale font to get edit control height
#define EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy)    (3*(cy)/2)

#ifdef __WXPM__

// Generic subclass proc, for panel item moving/sizing and intercept
// EDIT control VK_RETURN messages
extern LONG APIENTRY wxSubclassedGenericControlProc(WXHWND hWnd, WXDWORD message, WXWPARAM wParam, WXLPARAM lParam);

#endif

// ---------------------------------------------------------------------------
// constants which might miss from some compilers' headers
// ---------------------------------------------------------------------------

#if !defined(WS_EX_CLIENTEDGE)
    #define WS_EX_CLIENTEDGE 0x00000200L
#endif

#ifndef ENDSESSION_LOGOFF
    #define ENDSESSION_LOGOFF    0x80000000
#endif

#ifndef PMERR_INVALID_PARM
    #define PMERR_INVALID_PARM 0x1303
#endif

#ifndef PMERR_INVALID_PARAMETERS
    #define PMERR_INVALID_PARAMETERS 0x1208
#endif

#ifndef BOOKERR_INVALID_PARAMETERS
    #define BOOKERR_INVALID_PARAMETERS -1
#endif

#ifndef DLGC_ENTRYFIELD
    #define DLGC_ENTRYFIELD  0x0001
#endif

#ifndef DLGC_BUTTON
    #define DLGC_BUTTON      0x0002
#endif

#ifndef DLGC_MLE
    #define DLGC_MLE         0x0400
#endif

#ifndef DP_NORMAL
    #define DP_NORMAL 0
#endif

// ---------------------------------------------------------------------------
// debug messages -- OS/2 has no native debug output system
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// macros to make casting between WXFOO and FOO a bit easier: the GetFoo()
// returns Foo cast to the Windows type for oruselves, while GetFooOf() takes
// an argument which should be a pointer or reference to the object of the
// corresponding class (this depends on the macro)
// ---------------------------------------------------------------------------

#define GetHwnd()               ((HWND)GetHWND())
#define GetHwndOf(win)          ((HWND)((win)->GetHWND()))
// old name
#define GetWinHwnd              GetHwndOf

#define GetHdc()                ((HDC)GetHDC())
#define GetHdcOf(dc)            ((HDC)(dc).GetHDC())

#define GetHbitmap()            ((HBITMAP)GetHBITMAP())
#define GetHbitmapOf(bmp)       ((HBITMAP)(bmp).GetHBITMAP())

#define GetHicon()              ((HICON)GetHICON())
#define GetHiconOf(icon)        ((HICON)(icon).GetHICON())

#define GetHaccel()             ((HACCEL)GetHACCEL())
#define GetHaccelOf(table)      ((HACCEL)((table).GetHACCEL()))

#define GetHmenu()              ((HMENU)GetHMenu())
#define GetHmenuOf(menu)        ((HMENU)menu->GetHMenu())

#define GetHcursor()            ((HCURSOR)GetHCURSOR())
#define GetHcursorOf(cursor)    ((HCURSOR)(cursor).GetHCURSOR())

#define GetHfont()              ((HFONT)GetHFONT())
#define GetHfontOf(font)        ((HFONT)(font).GetHFONT())

// OS/2 convention of the mask is opposed to the wxWidgets one, so we need
// to invert the mask each time we pass one/get one to/from Windows
extern HBITMAP wxInvertMask(HBITMAP hbmpMask, int w = 0, int h = 0);
extern HBITMAP wxCopyBmp(HBITMAP hbmp, bool flip=false, int w=0, int h=0);

// ---------------------------------------------------------------------------
// global data
// ---------------------------------------------------------------------------

#ifdef __WXPM__
// The MakeProcInstance version of the function wxSubclassedGenericControlProc
WXDLLIMPEXP_DATA_CORE(extern int) wxGenericControlSubClassProc;
WXDLLIMPEXP_DATA_CORE(extern wxChar*) wxBuffer;
WXDLLIMPEXP_DATA_CORE(extern HINSTANCE) wxhInstance;
#endif

// ---------------------------------------------------------------------------
// global functions
// ---------------------------------------------------------------------------

#ifdef __WXPM__
extern "C"
{
WXDLLIMPEXP_CORE HINSTANCE wxGetInstance();
}

WXDLLIMPEXP_CORE void wxSetInstance(HINSTANCE hInst);
#endif

#include "wx/thread.h"
static inline MRESULT MySendMsg(HWND hwnd, ULONG ulMsgid,
                                MPARAM mpParam1, MPARAM mpParam2)
{
    MRESULT vRes;
    vRes = ::WinSendMsg(hwnd, ulMsgid, mpParam1, mpParam2);
#if wxUSE_THREADS
    if (!wxThread::IsMain())
        ::WinPostMsg(hwnd, ulMsgid, mpParam1, mpParam2);
#endif
    return vRes;
}
#define WinSendMsg MySendMsg

#ifdef __WXPM__

WXDLLIMPEXP_CORE void wxDrawBorder( HPS     hPS
                              ,RECTL&  rRect
                              ,WXDWORD dwStyle
                             );

WXDLLIMPEXP_CORE wxWindow* wxFindWinFromHandle(WXHWND hWnd);

WXDLLIMPEXP_CORE void   wxGetCharSize(WXHWND wnd, int *x, int *y,wxFont *the_font);

WXDLLIMPEXP_CORE void   wxConvertVectorFontSize( FIXED   fxPointSize
                                           ,PFATTRS pFattrs
                                          );
WXDLLIMPEXP_CORE void   wxFillLogFont( LOGFONT*      pLogFont
                                 ,PFACENAMEDESC pFaceName
                                 ,HPS*          phPS
                                 ,bool*         pbInternalPS
                                 ,long*         pflId
                                 ,wxString&     sFaceName
                                 ,wxFont*       pFont
                                );
WXDLLIMPEXP_CORE wxFontEncoding wxGetFontEncFromCharSet(int nCharSet);
WXDLLIMPEXP_CORE void   wxOS2SelectMatchingFontByName( PFATTRS       vFattrs
                                                 ,PFACENAMEDESC pFaceName
                                                 ,PFONTMETRICS  pFM
                                                 ,int           nNumFonts
                                                 ,const wxFont* pFont
                                                );
WXDLLIMPEXP_CORE wxFont wxCreateFontFromLogFont( LOGFONT*      pLogFont
                                           ,PFONTMETRICS  pFM
                                           ,PFACENAMEDESC pFace
                                          );
WXDLLIMPEXP_CORE int    wxGpiStrcmp(wxChar* s0, wxChar* s1);

WXDLLIMPEXP_CORE void wxSliderEvent(WXHWND control, WXWORD wParam, WXWORD pos);
WXDLLIMPEXP_CORE void wxScrollBarEvent(WXHWND hbar, WXWORD wParam, WXWORD pos);

// Find maximum size of window/rectangle
WXDLLIMPEXP_CORE extern void wxFindMaxSize(WXHWND hwnd, RECT *rect);

WXDLLIMPEXP_CORE wxWindow* wxFindControlFromHandle(WXHWND hWnd);
WXDLLIMPEXP_CORE void wxAddControlHandle(WXHWND hWnd, wxWindow *item);

// Safely get the window text (i.e. without using fixed size buffer)
WXDLLIMPEXP_CORE extern wxString wxGetWindowText(WXHWND hWnd);

// get the window class name
WXDLLIMPEXP_CORE extern wxString wxGetWindowClass(WXHWND hWnd);

// get the window id (should be unsigned, hence this is not wxWindowID which
// is, for mainly historical reasons, signed)
WXDLLIMPEXP_CORE extern WXWORD wxGetWindowId(WXHWND hWnd);

// Convert a PM Error code to a string
WXDLLIMPEXP_BASE extern wxString wxPMErrorToStr(ERRORID vError);

// Does this window style specify any border?
inline bool wxStyleHasBorder(long style)
{
  return (style & (wxSIMPLE_BORDER | wxRAISED_BORDER |
                   wxSUNKEN_BORDER | wxDOUBLE_BORDER)) != 0;
}

inline RECTL wxGetWindowRect(HWND hWnd)
{
    RECTL                           vRect;

    ::WinQueryWindowRect(hWnd, &vRect);
    return vRect;
} // end of wxGetWindowRect

WXDLLIMPEXP_CORE extern void wxOS2SetFont( HWND          hWnd
                                     ,const wxFont& rFont
                                    );


WXDLLIMPEXP_CORE extern bool wxCheckWindowWndProc( WXHWND    hWnd
                                             ,WXFARPROC fnWndProc
                                            );
WXDLLIMPEXP_CORE extern wxBitmap wxDisableBitmap( const wxBitmap& rBmp
                                            ,long            lColor
                                           );
#if wxUSE_GUI
class wxColour;
WXDLLIMPEXP_CORE extern COLORREF wxColourToRGB(const wxColour& rColor);
#endif

#endif // __WXPM__

#endif // _WX_OS2_PRIVATE_H_
