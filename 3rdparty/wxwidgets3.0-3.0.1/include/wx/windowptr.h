/////////////////////////////////////////////////////////////////////////////
// Name:        wx/windowptr.h
// Purpose:     smart pointer for holding wxWindow instances
// Author:      Vaclav Slavik
// Created:     2013-09-01
// Copyright:   (c) 2013 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WINDOWPTR_H_
#define _WX_WINDOWPTR_H_

#include "wx/sharedptr.h"

// ----------------------------------------------------------------------------
// wxWindowPtr: A smart pointer with correct wxWindow destruction.
// ----------------------------------------------------------------------------

namespace wxPrivate
{

struct wxWindowDeleter
{
    void operator()(wxWindow *win)
    {
        win->Destroy();
    }
};

} // namespace wxPrivate

template<typename T>
class wxWindowPtr : public wxSharedPtr<T>
{
public:
    typedef T element_type;

    wxEXPLICIT wxWindowPtr(element_type* win)
        : wxSharedPtr<T>(win, wxPrivate::wxWindowDeleter())
    {
    }

    wxWindowPtr() {}
    wxWindowPtr(const wxWindowPtr& tocopy) : wxSharedPtr<T>(tocopy) {}

    wxWindowPtr& operator=(const wxWindowPtr& tocopy)
    {
        wxSharedPtr<T>::operator=(tocopy);
        return *this;
    }

    wxWindowPtr& operator=(element_type* win)
    {
        return operator=(wxWindowPtr(win));
    }

    void reset(T* ptr = NULL)
    {
        wxSharedPtr<T>::reset(ptr, wxPrivate::wxWindowDeleter());
    }
};

#endif // _WX_WINDOWPTR_H_
