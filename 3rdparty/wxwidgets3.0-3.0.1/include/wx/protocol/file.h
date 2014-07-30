/////////////////////////////////////////////////////////////////////////////
// Name:        wx/protocol/file.h
// Purpose:     File protocol
// Author:      Guilhem Lavaux
// Modified by:
// Created:     1997
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_PROTO_FILE_H__
#define __WX_PROTO_FILE_H__

#include "wx/defs.h"

#if wxUSE_PROTOCOL_FILE

#include "wx/protocol/protocol.h"

class WXDLLIMPEXP_NET wxFileProto: public wxProtocol
{
public:
    wxFileProto();
    virtual ~wxFileProto();

    bool Abort() { return true; }
    wxString GetContentType() const { return wxEmptyString; }

    wxInputStream *GetInputStream(const wxString& path);

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxFileProto)
    DECLARE_PROTOCOL(wxFileProto)
};

#endif // wxUSE_PROTOCOL_FILE

#endif // __WX_PROTO_FILE_H__
