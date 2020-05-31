///////////////////////////////////////////////////////////////////////////////
// Name:        wx/buffer.h
// Purpose:     auto buffer classes: buffers which automatically free memory
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.04.99
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BUFFER_H
#define _WX_BUFFER_H

#include "wx/chartype.h"
#include "wx/wxcrtbase.h"

#include <stdlib.h>             // malloc() and free()

class WXDLLIMPEXP_FWD_BASE wxCStrData;

// ----------------------------------------------------------------------------
// Special classes for (wide) character strings: they use malloc/free instead
// of new/delete
// ----------------------------------------------------------------------------

// helpers used by wxCharTypeBuffer
namespace wxPrivate
{

struct UntypedBufferData
{
    enum Kind
    {
        Owned,
        NonOwned
    };

    UntypedBufferData(void *str, size_t len, Kind kind = Owned)
        : m_str(str), m_length(len), m_ref(1), m_owned(kind == Owned) {}

    ~UntypedBufferData()
    {
        if ( m_owned )
            free(m_str);
    }

    void *m_str;
    size_t m_length;

    // "short" to have sizeof(Data)=12 on 32bit archs
    unsigned short m_ref;

    bool m_owned;
};

// NB: this is defined in string.cpp and not the (non-existent) buffer.cpp
WXDLLIMPEXP_BASE UntypedBufferData * GetUntypedNullData();

} // namespace wxPrivate


// Reference-counted character buffer for storing string data. The buffer
// is only valid for as long as the "parent" object that provided the data
// is valid; see wxCharTypeBuffer<T> for persistent variant.
template <typename T>
class wxScopedCharTypeBuffer
{
public:
    typedef T CharType;

    wxScopedCharTypeBuffer()
    {
        m_data = GetNullData();
    }

    // Creates "non-owned" buffer, i.e. 'str' is not owned by the buffer
    // and doesn't get freed by dtor. Used e.g. to point to wxString's internal
    // storage.
    static
    const wxScopedCharTypeBuffer CreateNonOwned(const CharType *str,
                                                size_t len = wxNO_LEN)
    {
        if ( len == wxNO_LEN )
            len = wxStrlen(str);

        wxScopedCharTypeBuffer buf;
        if ( str )
            buf.m_data = new Data(const_cast<CharType*>(str), len, Data::NonOwned);
        return buf;
    }

    // Creates "owned" buffer, i.e. takes over ownership of 'str' and frees it
    // in dtor (if ref.count reaches 0).
    static
    const wxScopedCharTypeBuffer CreateOwned(CharType *str,
                                             size_t len = wxNO_LEN )
    {
        if ( len == wxNO_LEN )
            len = wxStrlen(str);

        wxScopedCharTypeBuffer buf;
        if ( str )
            buf.m_data = new Data(str, len);
        return buf;
    }

    wxScopedCharTypeBuffer(const wxScopedCharTypeBuffer& src)
    {
        m_data = src.m_data;
        IncRef();
    }

    wxScopedCharTypeBuffer& operator=(const wxScopedCharTypeBuffer& src)
    {
        if ( &src == this )
            return *this;

        DecRef();
        m_data = src.m_data;
        IncRef();

        return *this;
    }

    ~wxScopedCharTypeBuffer()
    {
        DecRef();
    }

    // NB: this method is only const for backward compatibility. It used to
    //     be needed for auto_ptr-like semantics of the copy ctor, but now
    //     that ref-counting is used, it's not really needed.
    CharType *release() const
    {
        if ( m_data == GetNullData() )
            return NULL;

        wxASSERT_MSG( m_data->m_owned, wxT("can't release non-owned buffer") );
        wxASSERT_MSG( m_data->m_ref == 1, wxT("can't release shared buffer") );

        CharType * const p = m_data->Get();

        wxScopedCharTypeBuffer *self = const_cast<wxScopedCharTypeBuffer*>(this);
        self->m_data->Set(NULL, 0);
        self->DecRef();

        return p;
    }

    void reset()
    {
        DecRef();
    }

    CharType *data() { return m_data->Get(); }
    const CharType *data() const { return  m_data->Get(); }
    operator const CharType *() const { return data(); }
    CharType operator[](size_t n) const { return data()[n]; }

    size_t length() const { return m_data->m_length; }

protected:
    // reference-counted data
    struct Data : public wxPrivate::UntypedBufferData
    {
        Data(CharType *str, size_t len, Kind kind = Owned)
            : wxPrivate::UntypedBufferData(str, len, kind)
        {
        }

        CharType *Get() const { return static_cast<CharType *>(m_str); }
        void Set(CharType *str, size_t len)
        {
            m_str = str;
            m_length = len;
        }
    };

    // placeholder for NULL string, to simplify this code
    static Data *GetNullData()
    {
        return static_cast<Data *>(wxPrivate::GetUntypedNullData());
    }

    void IncRef()
    {
        if ( m_data == GetNullData() ) // exception, not ref-counted
            return;
        m_data->m_ref++;
    }

    void DecRef()
    {
        if ( m_data == GetNullData() ) // exception, not ref-counted
            return;
        if ( --m_data->m_ref == 0 )
            delete m_data;
        m_data = GetNullData();
    }

    // sets this object to a be copy of 'other'; if 'src' is non-owned,
    // a deep copy is made and 'this' will contain new instance of the data
    void MakeOwnedCopyOf(const wxScopedCharTypeBuffer& src)
    {
        this->DecRef();

        if ( src.m_data == this->GetNullData() )
        {
            this->m_data = this->GetNullData();
        }
        else if ( src.m_data->m_owned )
        {
            this->m_data = src.m_data;
            this->IncRef();
        }
        else
        {
            // if the scoped buffer had non-owned data, we have to make
            // a copy here, because src.m_data->m_str is valid only for as long
            // as 'src' exists
            this->m_data = new Data
                               (
                                   StrCopy(src.data(), src.length()),
                                   src.length()
                               );
        }
    }

    static CharType *StrCopy(const CharType *src, size_t len)
    {
        CharType *dst = (CharType*)malloc(sizeof(CharType) * (len + 1));
        if ( dst )
            memcpy(dst, src, sizeof(CharType) * (len + 1));
        return dst;
    }

protected:
    Data *m_data;
};

typedef wxScopedCharTypeBuffer<char> wxScopedCharBuffer;
typedef wxScopedCharTypeBuffer<wchar_t> wxScopedWCharBuffer;


// this buffer class always stores data in "owned" (persistent) manner
template <typename T>
class wxCharTypeBuffer : public wxScopedCharTypeBuffer<T>
{
protected:
    typedef typename wxScopedCharTypeBuffer<T>::Data Data;

public:
    typedef T CharType;

    wxCharTypeBuffer(const CharType *str = NULL, size_t len = wxNO_LEN)
    {
        if ( str )
        {
            if ( len == wxNO_LEN )
                len = wxStrlen(str);
            this->m_data = new Data(this->StrCopy(str, len), len);
        }
        else
        {
            this->m_data = this->GetNullData();
        }
    }

    wxCharTypeBuffer(size_t len)
    {
        CharType* const str = (CharType *)malloc((len + 1)*sizeof(CharType));
        if ( str )
        {
            str[len] = (CharType)0;

            // There is a potential memory leak here if new throws because it
            // fails to allocate Data, we ought to use new(nothrow) here, but
            // this might fail to compile under some platforms so until this
            // can be fully tested, just live with this (rather unlikely, as
            // Data is a small object) potential leak.
            this->m_data = new Data(str, len);
        }
        else
        {
            this->m_data = this->GetNullData();
        }
    }

    wxCharTypeBuffer(const wxCharTypeBuffer& src)
        : wxScopedCharTypeBuffer<T>(src) {}

    wxCharTypeBuffer& operator=(const CharType *str)
    {
        this->DecRef();

        if ( str )
            this->m_data = new Data(wxStrdup(str), wxStrlen(str));
        return *this;
    }

    wxCharTypeBuffer& operator=(const wxCharTypeBuffer& src)
    {
        wxScopedCharTypeBuffer<T>::operator=(src);
        return *this;
    }

    wxCharTypeBuffer(const wxScopedCharTypeBuffer<T>& src)
    {
        this->MakeOwnedCopyOf(src);
    }

    wxCharTypeBuffer& operator=(const wxScopedCharTypeBuffer<T>& src)
    {
        MakeOwnedCopyOf(src);
        return *this;
    }

    bool extend(size_t len)
    {
        wxASSERT_MSG( this->m_data->m_owned, "cannot extend non-owned buffer" );
        wxASSERT_MSG( this->m_data->m_ref == 1, "can't extend shared buffer" );

        CharType *str =
            (CharType *)realloc(this->data(), (len + 1) * sizeof(CharType));
        if ( !str )
            return false;

        // For consistency with the ctor taking just the length, NUL-terminate
        // the buffer.
        str[len] = (CharType)0;

        if ( this->m_data == this->GetNullData() )
        {
            this->m_data = new Data(str, len);
        }
        else
        {
            this->m_data->Set(str, len);
            this->m_data->m_owned = true;
        }

        return true;
    }

    void shrink(size_t len)
    {
        wxASSERT_MSG( this->m_data->m_owned, "cannot shrink non-owned buffer" );
        wxASSERT_MSG( this->m_data->m_ref == 1, "can't shrink shared buffer" );

        wxASSERT( len <= this->length() );

        this->m_data->m_length = len;
        this->data()[len] = 0;
    }
};

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxScopedCharTypeBuffer<char> )
WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxCharTypeBuffer<char> )

class wxCharBuffer : public wxCharTypeBuffer<char>
{
public:
    typedef wxCharTypeBuffer<char> wxCharTypeBufferBase;
    typedef wxScopedCharTypeBuffer<char> wxScopedCharTypeBufferBase;

    wxCharBuffer(const wxCharTypeBufferBase& buf)
        : wxCharTypeBufferBase(buf) {}
    wxCharBuffer(const wxScopedCharTypeBufferBase& buf)
        : wxCharTypeBufferBase(buf) {}

    wxCharBuffer(const CharType *str = NULL) : wxCharTypeBufferBase(str) {}
    wxCharBuffer(size_t len) : wxCharTypeBufferBase(len) {}

    wxCharBuffer(const wxCStrData& cstr);
};

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxScopedCharTypeBuffer<wchar_t> )
WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxCharTypeBuffer<wchar_t> )

class wxWCharBuffer : public wxCharTypeBuffer<wchar_t>
{
public:
    typedef wxCharTypeBuffer<wchar_t> wxCharTypeBufferBase;
    typedef wxScopedCharTypeBuffer<wchar_t> wxScopedCharTypeBufferBase;

    wxWCharBuffer(const wxCharTypeBufferBase& buf)
        : wxCharTypeBufferBase(buf) {}
    wxWCharBuffer(const wxScopedCharTypeBufferBase& buf)
        : wxCharTypeBufferBase(buf) {}

    wxWCharBuffer(const CharType *str = NULL) : wxCharTypeBufferBase(str) {}
    wxWCharBuffer(size_t len) : wxCharTypeBufferBase(len) {}

    wxWCharBuffer(const wxCStrData& cstr);
};

// wxCharTypeBuffer<T> implicitly convertible to T*
template <typename T>
class wxWritableCharTypeBuffer : public wxCharTypeBuffer<T>
{
public:
    typedef typename wxScopedCharTypeBuffer<T>::CharType CharType;

    wxWritableCharTypeBuffer(const wxScopedCharTypeBuffer<T>& src)
        : wxCharTypeBuffer<T>(src) {}
    // FIXME-UTF8: this won't be needed after converting mb_str()/wc_str() to
    //             always return a buffer
    //             + we should derive this class from wxScopedCharTypeBuffer
    //               then
    wxWritableCharTypeBuffer(const CharType *str = NULL)
        : wxCharTypeBuffer<T>(str) {}

    operator CharType*() { return this->data(); }
};

typedef wxWritableCharTypeBuffer<char> wxWritableCharBuffer;
typedef wxWritableCharTypeBuffer<wchar_t> wxWritableWCharBuffer;


#if wxUSE_UNICODE
    #define wxWxCharBuffer wxWCharBuffer

    #define wxMB2WXbuf wxWCharBuffer
    #define wxWX2MBbuf wxCharBuffer
    #if wxUSE_UNICODE_WCHAR
        #define wxWC2WXbuf wxChar*
        #define wxWX2WCbuf wxChar*
    #elif wxUSE_UNICODE_UTF8
        #define wxWC2WXbuf wxWCharBuffer
        #define wxWX2WCbuf wxWCharBuffer
    #endif
#else // ANSI
    #define wxWxCharBuffer wxCharBuffer

    #define wxMB2WXbuf wxChar*
    #define wxWX2MBbuf wxChar*
    #define wxWC2WXbuf wxCharBuffer
    #define wxWX2WCbuf wxWCharBuffer
#endif // Unicode/ANSI

// ----------------------------------------------------------------------------
// A class for holding growable data buffers (not necessarily strings)
// ----------------------------------------------------------------------------

// This class manages the actual data buffer pointer and is ref-counted.
class wxMemoryBufferData
{
public:
    // the initial size and also the size added by ResizeIfNeeded()
    enum { DefBufSize = 1024 };

    friend class wxMemoryBuffer;

    // everything is private as it can only be used by wxMemoryBuffer
private:
    wxMemoryBufferData(size_t size = wxMemoryBufferData::DefBufSize)
        : m_data(size ? malloc(size) : NULL), m_size(size), m_len(0), m_ref(0)
    {
    }
    ~wxMemoryBufferData() { free(m_data); }


    void ResizeIfNeeded(size_t newSize)
    {
        if (newSize > m_size)
        {
            void *dataOld = m_data;
            m_data = realloc(m_data, newSize + wxMemoryBufferData::DefBufSize);
            if ( !m_data )
            {
                free(dataOld);
            }

            m_size = newSize + wxMemoryBufferData::DefBufSize;
        }
    }

    void IncRef() { m_ref += 1; }
    void DecRef()
    {
        m_ref -= 1;
        if (m_ref == 0)  // are there no more references?
            delete this;
    }

    void *release()
    {
        if ( m_data == NULL )
            return NULL;

        wxASSERT_MSG( m_ref == 1, "can't release shared buffer" );

        void *p = m_data;
        m_data = NULL;
        m_len =
        m_size = 0;

        return p;
    }


    // the buffer containing the data
    void  *m_data;

    // the size of the buffer
    size_t m_size;

    // the amount of data currently in the buffer
    size_t m_len;

    // the reference count
    size_t m_ref;

    wxDECLARE_NO_COPY_CLASS(wxMemoryBufferData);
};


class wxMemoryBuffer
{
public:
    // ctor and dtor
    wxMemoryBuffer(size_t size = wxMemoryBufferData::DefBufSize)
    {
        m_bufdata = new wxMemoryBufferData(size);
        m_bufdata->IncRef();
    }

    ~wxMemoryBuffer() { m_bufdata->DecRef(); }


    // copy and assignment
    wxMemoryBuffer(const wxMemoryBuffer& src)
        : m_bufdata(src.m_bufdata)
    {
        m_bufdata->IncRef();
    }

    wxMemoryBuffer& operator=(const wxMemoryBuffer& src)
    {
        if (&src != this)
        {
            m_bufdata->DecRef();
            m_bufdata = src.m_bufdata;
            m_bufdata->IncRef();
        }
        return *this;
    }


    // Accessors
    void  *GetData() const    { return m_bufdata->m_data; }
    size_t GetBufSize() const { return m_bufdata->m_size; }
    size_t GetDataLen() const { return m_bufdata->m_len; }

    bool IsEmpty() const { return GetDataLen() == 0; }

    void   SetBufSize(size_t size) { m_bufdata->ResizeIfNeeded(size); }
    void   SetDataLen(size_t len)
    {
        wxASSERT(len <= m_bufdata->m_size);
        m_bufdata->m_len = len;
    }

    void Clear() { SetDataLen(0); }

    // Ensure the buffer is big enough and return a pointer to it
    void *GetWriteBuf(size_t sizeNeeded)
    {
        m_bufdata->ResizeIfNeeded(sizeNeeded);
        return m_bufdata->m_data;
    }

    // Update the length after the write
    void  UngetWriteBuf(size_t sizeUsed) { SetDataLen(sizeUsed); }

    // Like the above, but appends to the buffer
    void *GetAppendBuf(size_t sizeNeeded)
    {
        m_bufdata->ResizeIfNeeded(m_bufdata->m_len + sizeNeeded);
        return (char*)m_bufdata->m_data + m_bufdata->m_len;
    }

    // Update the length after the append
    void  UngetAppendBuf(size_t sizeUsed)
    {
        SetDataLen(m_bufdata->m_len + sizeUsed);
    }

    // Other ways to append to the buffer
    void  AppendByte(char data)
    {
        wxCHECK_RET( m_bufdata->m_data, wxT("invalid wxMemoryBuffer") );

        m_bufdata->ResizeIfNeeded(m_bufdata->m_len + 1);
        *(((char*)m_bufdata->m_data) + m_bufdata->m_len) = data;
        m_bufdata->m_len += 1;
    }

    void  AppendData(const void *data, size_t len)
    {
        memcpy(GetAppendBuf(len), data, len);
        UngetAppendBuf(len);
    }

    operator const char *() const { return (const char*)GetData(); }

    // gives up ownership of data, returns the pointer; after this call,
    // data isn't freed by the buffer and its content is resent to empty
    void *release()
    {
        return m_bufdata->release();
    }

private:
    wxMemoryBufferData*  m_bufdata;
};

// ----------------------------------------------------------------------------
// template class for any kind of data
// ----------------------------------------------------------------------------

// TODO

#endif // _WX_BUFFER_H
