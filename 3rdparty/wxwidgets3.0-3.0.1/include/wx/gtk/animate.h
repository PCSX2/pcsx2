/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/animate.h
// Purpose:     Animation classes
// Author:      Julian Smart and Guillermo Rodriguez Garcia
// Modified by: Francesco Montorsi
// Created:     13/8/99
// Copyright:   (c) Julian Smart and Guillermo Rodriguez Garcia
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKANIMATEH__
#define _WX_GTKANIMATEH__

typedef struct _GdkPixbufAnimation GdkPixbufAnimation;
typedef struct _GdkPixbufAnimationIter GdkPixbufAnimationIter;

// ----------------------------------------------------------------------------
// wxAnimation
// Unlike the generic wxAnimation object (see generic\animate.cpp), we won't
// use directly wxAnimationHandlers as gdk-pixbuf already provides the
// concept of handler and will automatically use the available handlers.
// Like generic wxAnimation object, this implementation of wxAnimation is
// refcounted so that assignment is very fast
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxAnimation : public wxAnimationBase
{
public:
    wxAnimation(const wxString &name, wxAnimationType type = wxANIMATION_TYPE_ANY)
        : m_pixbuf(NULL) { LoadFile(name, type); }
    wxAnimation(GdkPixbufAnimation *p = NULL);
    wxAnimation(const wxAnimation&);
    ~wxAnimation() { UnRef(); }

    wxAnimation& operator= (const wxAnimation&);

    virtual bool IsOk() const
        { return m_pixbuf != NULL; }


    // unfortunately GdkPixbufAnimation does not expose these info:

    virtual unsigned int GetFrameCount() const { return 0; }
    virtual wxImage GetFrame(unsigned int frame) const;

    // we can retrieve the delay for a frame only after building
    // a GdkPixbufAnimationIter...
    virtual int GetDelay(unsigned int WXUNUSED(frame)) const { return 0; }

    virtual wxSize GetSize() const;

    virtual bool LoadFile(const wxString &name, wxAnimationType type = wxANIMATION_TYPE_ANY);
    virtual bool Load(wxInputStream &stream, wxAnimationType type = wxANIMATION_TYPE_ANY);

    // Implementation
public:     // used by GTK callbacks

    GdkPixbufAnimation *GetPixbuf() const
        { return m_pixbuf; }
    void SetPixbuf(GdkPixbufAnimation* p);

protected:
    GdkPixbufAnimation *m_pixbuf;

private:
    void UnRef();

    typedef wxAnimationBase base_type;
    DECLARE_DYNAMIC_CLASS(wxAnimation)
};


// ----------------------------------------------------------------------------
// wxAnimationCtrl
// ----------------------------------------------------------------------------

// Resize to animation size if this is set
#define wxAN_FIT_ANIMATION       0x0010

class WXDLLIMPEXP_ADV wxAnimationCtrl: public wxAnimationCtrlBase
{
public:
    wxAnimationCtrl() { Init(); }
    wxAnimationCtrl(wxWindow *parent,
                        wxWindowID id,
                        const wxAnimation& anim = wxNullAnimation,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxAC_DEFAULT_STYLE,
                        const wxString& name = wxAnimationCtrlNameStr)
    {
        Init();

        Create(parent, id, anim, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxAnimation& anim = wxNullAnimation,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxAC_DEFAULT_STYLE,
                const wxString& name = wxAnimationCtrlNameStr);

    ~wxAnimationCtrl();

public:     // event handler

    void OnTimer(wxTimerEvent &);

public:     // public API

    virtual bool LoadFile(const wxString& filename, wxAnimationType type = wxANIMATION_TYPE_ANY);
    virtual bool Load(wxInputStream& stream, wxAnimationType type = wxANIMATION_TYPE_ANY);

    virtual void SetAnimation(const wxAnimation &anim);
    virtual wxAnimation GetAnimation() const
        { return wxAnimation(m_anim); }

    virtual bool Play();
    virtual void Stop();

    virtual bool IsPlaying() const;

    bool SetBackgroundColour( const wxColour &colour );

protected:

    virtual void DisplayStaticImage();
    virtual wxSize DoGetBestSize() const;
    void FitToAnimation();
    void ClearToBackgroundColour();

    void ResetAnim();
    void ResetIter();

protected:      // internal vars

    GdkPixbufAnimation *m_anim;
    GdkPixbufAnimationIter *m_iter;

    wxTimer m_timer;
    bool m_bPlaying;

private:
    typedef wxAnimationCtrlBase base_type;

    void Init();

    DECLARE_DYNAMIC_CLASS(wxAnimationCtrl)
    DECLARE_EVENT_TABLE()
};

#endif // _WX_GTKANIMATEH__
