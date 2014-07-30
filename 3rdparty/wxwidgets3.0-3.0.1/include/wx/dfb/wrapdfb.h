/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/wrapdfb.h
// Purpose:     wx wrappers for DirectFB interfaces
// Author:      Vaclav Slavik
// Created:     2006-08-23
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_WRAPDFB_H_
#define _WX_DFB_WRAPDFB_H_

#include "wx/dfb/dfbptr.h"
#include "wx/gdicmn.h"
#include "wx/vidmode.h"

#include <directfb.h>
#include <directfb_version.h>

// DFB < 1.0 didn't have u8 type, only __u8
#if DIRECTFB_MAJOR_VERSION == 0
typedef __u8 u8;
#endif


wxDFB_DECLARE_INTERFACE(IDirectFB);
wxDFB_DECLARE_INTERFACE(IDirectFBDisplayLayer);
wxDFB_DECLARE_INTERFACE(IDirectFBFont);
wxDFB_DECLARE_INTERFACE(IDirectFBWindow);
wxDFB_DECLARE_INTERFACE(IDirectFBSurface);
wxDFB_DECLARE_INTERFACE(IDirectFBPalette);
wxDFB_DECLARE_INTERFACE(IDirectFBEventBuffer);


/**
    Checks the @a code of a DirectFB call and returns true if it was
    successful and false if it failed, logging the errors as appropriate
    (asserts for programming errors, wxLogError for runtime failures).
 */
bool wxDfbCheckReturn(DFBResult code);

//-----------------------------------------------------------------------------
// wxDfbEvent
//-----------------------------------------------------------------------------

/**
    The struct defined by this macro is a thin wrapper around DFB*Event type.
    It is needed because DFB*Event are typedefs and so we can't forward declare
    them, but we need to pass them to methods declared in public headers where
    <directfb.h> cannot be included. So this struct just holds the event value,
    it's sole purpose is that it can be forward declared.
 */
#define WXDFB_DEFINE_EVENT_WRAPPER(T)                                       \
    struct wx##T                                                            \
    {                                                                       \
        wx##T() {}                                                          \
        wx##T(const T& event) : m_event(event) {}                           \
                                                                            \
        operator T&() { return m_event; }                                   \
        operator const T&() const { return m_event; }                       \
        T* operator&() { return &m_event; }                                 \
                                                                            \
        DFBEventClass GetClass() const { return m_event.clazz; }            \
                                                                            \
    private:                                                                \
        T m_event;                                                          \
    };

WXDFB_DEFINE_EVENT_WRAPPER(DFBEvent)
WXDFB_DEFINE_EVENT_WRAPPER(DFBWindowEvent)


//-----------------------------------------------------------------------------
// wxDfbWrapper<T>
//-----------------------------------------------------------------------------

/// Base class for wxDfbWrapper<T>
class wxDfbWrapperBase
{
public:
    /// Increases reference count of the object
    void AddRef()
    {
        m_refCnt++;
    }

    /// Decreases reference count and if it reaches zero, deletes the object
    void Release()
    {
        if ( --m_refCnt == 0 )
            delete this;
    }

    /// Returns result code of the last call
    DFBResult GetLastResult() const { return m_lastResult; }

protected:
    wxDfbWrapperBase() : m_refCnt(1), m_lastResult(DFB_OK) {}

    /// Dtor may only be called from Release()
    virtual ~wxDfbWrapperBase() {}

    /**
        Checks the @a result of a DirectFB call and returns true if it was
        successful and false if it failed. Also stores result of the call
        so that it can be obtained by calling GetLastResult().
     */
    bool Check(DFBResult result)
    {
        m_lastResult = result;
        return wxDfbCheckReturn(result);
    }

protected:
    /// Reference count
    unsigned m_refCnt;

    /// Result of the last DirectFB call
    DFBResult m_lastResult;
};

/**
    This template is base class for friendly C++ wrapper around DirectFB
    interface T.

    The wrapper provides same API as DirectFB, with a few exceptions:
     - methods return true/false instead of error code
     - methods that return or create another interface return pointer to the
       interface (or NULL on failure) instead of storing it in the last
       argument
     - interface arguments use wxFooPtr type instead of raw DirectFB pointer
     - methods taking flags use int type instead of an enum when the flags
       can be or-combination of enum elements (this is workaround for
       C++-unfriendly DirectFB API)
 */
template<typename T>
class wxDfbWrapper : public wxDfbWrapperBase
{
public:
    /// "Raw" DirectFB interface type
    typedef T DirectFBIface;

    /// Returns raw DirectFB pointer
    T *GetRaw() const { return m_ptr; }

protected:
    /// To be called from ctor. Takes ownership of raw object.
    void Init(T *ptr) { m_ptr = ptr; }

    /// Dtor may only be used from Release
    ~wxDfbWrapper()
    {
        if ( m_ptr )
            m_ptr->Release(m_ptr);
    }

protected:
    // pointer to DirectFB object
    T *m_ptr;
};


//-----------------------------------------------------------------------------
// wxIDirectFBFont
//-----------------------------------------------------------------------------

struct wxIDirectFBFont : public wxDfbWrapper<IDirectFBFont>
{
    wxIDirectFBFont(IDirectFBFont *s) { Init(s); }

    bool GetStringWidth(const char *text, int bytes, int *w)
        { return Check(m_ptr->GetStringWidth(m_ptr, text, bytes, w)); }

    bool GetStringExtents(const char *text, int bytes,
                          DFBRectangle *logicalRect, DFBRectangle *inkRect)
    {
        return Check(m_ptr->GetStringExtents(m_ptr, text, bytes,
                                               logicalRect, inkRect));
    }

    bool GetHeight(int *h)
        { return Check(m_ptr->GetHeight(m_ptr, h)); }

    bool GetDescender(int *descender)
        { return Check(m_ptr->GetDescender(m_ptr, descender)); }
};


//-----------------------------------------------------------------------------
// wxIDirectFBPalette
//-----------------------------------------------------------------------------

struct wxIDirectFBPalette : public wxDfbWrapper<IDirectFBPalette>
{
    wxIDirectFBPalette(IDirectFBPalette *s) { Init(s); }
};


//-----------------------------------------------------------------------------
// wxIDirectFBSurface
//-----------------------------------------------------------------------------

struct wxIDirectFBSurface : public wxDfbWrapper<IDirectFBSurface>
{
    wxIDirectFBSurface(IDirectFBSurface *s) { Init(s); }

    bool GetSize(int *w, int *h)
        { return Check(m_ptr->GetSize(m_ptr, w, h)); }

    bool GetCapabilities(DFBSurfaceCapabilities *caps)
        { return Check(m_ptr->GetCapabilities(m_ptr, caps)); }

    bool GetPixelFormat(DFBSurfacePixelFormat *caps)
        { return Check(m_ptr->GetPixelFormat(m_ptr, caps)); }

    // convenience version of GetPixelFormat, returns DSPF_UNKNOWN if fails
    DFBSurfacePixelFormat GetPixelFormat();

    bool SetClip(const DFBRegion *clip)
        { return Check(m_ptr->SetClip(m_ptr, clip)); }

    bool SetColor(u8 r, u8 g, u8 b, u8 a)
        { return Check(m_ptr->SetColor(m_ptr, r, g, b, a)); }

    bool Clear(u8 r, u8 g, u8 b, u8 a)
        { return Check(m_ptr->Clear(m_ptr, r, g, b, a)); }

    bool DrawLine(int x1, int y1, int x2, int y2)
        { return Check(m_ptr->DrawLine(m_ptr, x1, y1, x2, y2)); }

    bool DrawRectangle(int x, int y, int w, int h)
        { return Check(m_ptr->DrawRectangle(m_ptr, x, y, w, h)); }

    bool FillRectangle(int x, int y, int w, int h)
        { return Check(m_ptr->FillRectangle(m_ptr, x, y, w, h)); }

    bool SetFont(const wxIDirectFBFontPtr& font)
        { return Check(m_ptr->SetFont(m_ptr, font->GetRaw())); }

    bool DrawString(const char *text, int bytes, int x, int y, int flags)
    {
        return Check(m_ptr->DrawString(m_ptr, text, bytes, x, y,
                                         (DFBSurfaceTextFlags)flags));
    }

    /**
        Updates the front buffer from the back buffer. If @a region is not
        NULL, only given rectangle is updated.
     */
    bool FlipToFront(const DFBRegion *region = NULL);

    wxIDirectFBSurfacePtr GetSubSurface(const DFBRectangle *rect)
    {
        IDirectFBSurface *s;
        if ( Check(m_ptr->GetSubSurface(m_ptr, rect, &s)) )
            return new wxIDirectFBSurface(s);
        else
            return NULL;
    }

    wxIDirectFBPalettePtr GetPalette()
    {
        IDirectFBPalette *s;
        if ( Check(m_ptr->GetPalette(m_ptr, &s)) )
            return new wxIDirectFBPalette(s);
        else
            return NULL;
    }

    bool SetPalette(const wxIDirectFBPalettePtr& pal)
        { return Check(m_ptr->SetPalette(m_ptr, pal->GetRaw())); }

    bool SetBlittingFlags(int flags)
    {
        return Check(
            m_ptr->SetBlittingFlags(m_ptr, (DFBSurfaceBlittingFlags)flags));
    }

    bool Blit(const wxIDirectFBSurfacePtr& source,
              const DFBRectangle *source_rect,
              int x, int y)
        { return Blit(source->GetRaw(), source_rect, x, y); }

    bool Blit(IDirectFBSurface *source,
              const DFBRectangle *source_rect,
              int x, int y)
        { return Check(m_ptr->Blit(m_ptr, source, source_rect, x, y)); }

    bool StretchBlit(const wxIDirectFBSurfacePtr& source,
              const DFBRectangle *source_rect,
              const DFBRectangle *dest_rect)
    {
        return Check(m_ptr->StretchBlit(m_ptr, source->GetRaw(),
                                        source_rect, dest_rect));
    }

    /// Returns bit depth used by the surface or -1 on error
    int GetDepth();

    /**
        Creates a new surface by cloning this one. New surface will have same
        capabilities, pixel format and pixel data as the existing one.

        @see CreateCompatible
     */
    wxIDirectFBSurfacePtr Clone();

    /// Flags for CreateCompatible()
    enum CreateCompatibleFlags
    {
        /// Don't create double-buffered surface
        CreateCompatible_NoBackBuffer = 1
    };

    /**
        Creates a surface compatible with this one, i.e. surface with the same
        capabilities and pixel format, but with different and size.

        @param size  Size of the surface to create. If wxDefaultSize, use the
                     size of this surface.
        @param flags Or-combination of CreateCompatibleFlags values
     */
    wxIDirectFBSurfacePtr CreateCompatible(const wxSize& size = wxDefaultSize,
                                           int flags = 0);

    bool Lock(DFBSurfaceLockFlags flags, void **ret_ptr, int *ret_pitch)
        { return Check(m_ptr->Lock(m_ptr, flags, ret_ptr, ret_pitch)); }

    bool Unlock()
        { return Check(m_ptr->Unlock(m_ptr)); }

    /// Helper struct for safe locking & unlocking of surfaces
    struct Locked
    {
        Locked(const wxIDirectFBSurfacePtr& surface, DFBSurfaceLockFlags flags)
            : m_surface(surface)
        {
            if ( !surface->Lock(flags, &ptr, &pitch) )
                ptr = NULL;
        }

        ~Locked()
        {
            if ( ptr )
                m_surface->Unlock();
        }

        void *ptr;
        int pitch;

    private:
        wxIDirectFBSurfacePtr m_surface;
    };


private:
    // this is private because we want user code to use FlipToFront()
    bool Flip(const DFBRegion *region, int flags);
};


//-----------------------------------------------------------------------------
// wxIDirectFBEventBuffer
//-----------------------------------------------------------------------------

struct wxIDirectFBEventBuffer : public wxDfbWrapper<IDirectFBEventBuffer>
{
    wxIDirectFBEventBuffer(IDirectFBEventBuffer *s) { Init(s); }

    bool CreateFileDescriptor(int *ret_fd)
    {
        return Check(m_ptr->CreateFileDescriptor(m_ptr, ret_fd));
    }
};


//-----------------------------------------------------------------------------
// wxIDirectFBWindow
//-----------------------------------------------------------------------------

struct wxIDirectFBWindow : public wxDfbWrapper<IDirectFBWindow>
{
    wxIDirectFBWindow(IDirectFBWindow *s) { Init(s); }

    bool GetID(DFBWindowID *id)
        { return Check(m_ptr->GetID(m_ptr, id)); }

    bool GetPosition(int *x, int *y)
        { return Check(m_ptr->GetPosition(m_ptr, x, y)); }

    bool GetSize(int *w, int *h)
        { return Check(m_ptr->GetSize(m_ptr, w, h)); }

    bool MoveTo(int x, int y)
        { return Check(m_ptr->MoveTo(m_ptr, x, y)); }

    bool Resize(int w, int h)
        { return Check(m_ptr->Resize(m_ptr, w, h)); }

    bool SetOpacity(u8 opacity)
        { return Check(m_ptr->SetOpacity(m_ptr, opacity)); }

    bool SetStackingClass(DFBWindowStackingClass klass)
        { return Check(m_ptr->SetStackingClass(m_ptr, klass)); }

    bool RaiseToTop()
        { return Check(m_ptr->RaiseToTop(m_ptr)); }

    bool LowerToBottom()
        { return Check(m_ptr->LowerToBottom(m_ptr)); }

    wxIDirectFBSurfacePtr GetSurface()
    {
        IDirectFBSurface *s;
        if ( Check(m_ptr->GetSurface(m_ptr, &s)) )
            return new wxIDirectFBSurface(s);
        else
            return NULL;
    }

    bool AttachEventBuffer(const wxIDirectFBEventBufferPtr& buffer)
        { return Check(m_ptr->AttachEventBuffer(m_ptr, buffer->GetRaw())); }

    bool RequestFocus()
        { return Check(m_ptr->RequestFocus(m_ptr)); }

    bool Destroy()
        { return Check(m_ptr->Destroy(m_ptr)); }
};


//-----------------------------------------------------------------------------
// wxIDirectFBDisplayLayer
//-----------------------------------------------------------------------------

struct wxIDirectFBDisplayLayer : public wxDfbWrapper<IDirectFBDisplayLayer>
{
    wxIDirectFBDisplayLayer(IDirectFBDisplayLayer *s) { Init(s); }

    wxIDirectFBWindowPtr CreateWindow(const DFBWindowDescription *desc)
    {
        IDirectFBWindow *w;
        if ( Check(m_ptr->CreateWindow(m_ptr, desc, &w)) )
            return new wxIDirectFBWindow(w);
        else
            return NULL;
    }

    bool GetConfiguration(DFBDisplayLayerConfig *config)
        { return Check(m_ptr->GetConfiguration(m_ptr, config)); }

    wxVideoMode GetVideoMode();

    bool GetCursorPosition(int *x, int *y)
        { return Check(m_ptr->GetCursorPosition(m_ptr, x, y)); }

    bool WarpCursor(int x, int y)
        { return Check(m_ptr->WarpCursor(m_ptr, x, y)); }
};


//-----------------------------------------------------------------------------
// wxIDirectFB
//-----------------------------------------------------------------------------

struct wxIDirectFB : public wxDfbWrapper<IDirectFB>
{
    /**
        Returns pointer to DirectFB singleton object, it never returns NULL
        after wxApp was initialized. The object is cached, so calling this
        method is cheap.
     */
    static wxIDirectFBPtr Get()
    {
        if ( !ms_ptr ) CreateDirectFB();
        return ms_ptr;
    }

    bool SetVideoMode(int w, int h, int bpp)
        { return Check(m_ptr->SetVideoMode(m_ptr, w, h, bpp)); }

    wxIDirectFBSurfacePtr CreateSurface(const DFBSurfaceDescription *desc)
    {
        IDirectFBSurface *s;
        if ( Check(m_ptr->CreateSurface(m_ptr, desc, &s)) )
            return new wxIDirectFBSurface(s);
        else
            return NULL;
    }

    wxIDirectFBEventBufferPtr CreateEventBuffer()
    {
        IDirectFBEventBuffer *b;
        if ( Check(m_ptr->CreateEventBuffer(m_ptr, &b)) )
            return new wxIDirectFBEventBuffer(b);
        else
            return NULL;
    }

    wxIDirectFBFontPtr CreateFont(const char *filename,
                                  const DFBFontDescription *desc)
    {
        IDirectFBFont *f;
        if ( Check(m_ptr->CreateFont(m_ptr, filename, desc, &f)) )
            return new wxIDirectFBFont(f);
        else
            return NULL;
    }

    wxIDirectFBDisplayLayerPtr
    GetDisplayLayer(DFBDisplayLayerID id = DLID_PRIMARY)
    {
        IDirectFBDisplayLayer *l;
        if ( Check(m_ptr->GetDisplayLayer(m_ptr, id, &l)) )
            return new wxIDirectFBDisplayLayer(l);
        else
            return NULL;
    }

    /// Returns primary surface
    wxIDirectFBSurfacePtr GetPrimarySurface();

private:
    wxIDirectFB(IDirectFB *ptr) { Init(ptr); }

    // creates ms_ptr instance
    static void CreateDirectFB();

    static void CleanUp();
    friend class wxApp; // calls CleanUp

    // pointer to the singleton IDirectFB object
    static wxIDirectFBPtr ms_ptr;
};

#endif // _WX_DFB_WRAPDFB_H_
