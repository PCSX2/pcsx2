/////////////////////////////////////////////////////////////////////////////
// Name:        wx/meta/int2type.h
// Purpose:     Generate a unique type from a constant integer
// Author:      Arne Steinarson
// Created:     2008-01-10
// Copyright:   (c) 2008 Arne Steinarson
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_META_INT2TYPE_H_
#define _WX_META_INT2TYPE_H_

template <int N>
struct wxInt2Type { enum { value=N }; };

#endif // _WX_META_INT2TYPE_H_
