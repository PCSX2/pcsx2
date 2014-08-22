/////////////////////////////////////////////////////////////////////////////
// Name:        src/propgrid/props.cpp
// Purpose:     Basic Property Classes
// Author:      Jaakko Salli
// Modified by:
// Created:     2005-05-14
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PROPGRID

#ifndef WX_PRECOMP
    #include "wx/defs.h"
    #include "wx/object.h"
    #include "wx/hash.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/event.h"
    #include "wx/window.h"
    #include "wx/panel.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/button.h"
    #include "wx/bmpbuttn.h"
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/cursor.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/choice.h"
    #include "wx/stattext.h"
    #include "wx/scrolwin.h"
    #include "wx/dirdlg.h"
    #include "wx/combobox.h"
    #include "wx/layout.h"
    #include "wx/sizer.h"
    #include "wx/textdlg.h"
    #include "wx/filedlg.h"
    #include "wx/intl.h"
#endif

#include "wx/filename.h"

#include "wx/propgrid/propgrid.h"

#define wxPG_CUSTOM_IMAGE_WIDTH     20 // for wxColourProperty etc.


// -----------------------------------------------------------------------
// wxStringProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxStringProperty,wxPGProperty,
                               wxString,const wxString&,TextCtrl)

wxStringProperty::wxStringProperty( const wxString& label,
                                    const wxString& name,
                                    const wxString& value )
    : wxPGProperty(label,name)
{
    SetValue(value);
}

void wxStringProperty::OnSetValue()
{
    if ( !m_value.IsNull() && m_value.GetString() == wxS("<composed>") )
        SetFlag(wxPG_PROP_COMPOSED_VALUE);

    if ( HasFlag(wxPG_PROP_COMPOSED_VALUE) )
    {
        wxString s;
        DoGenerateComposedValue(s);
        m_value = s;
    }
}

wxStringProperty::~wxStringProperty() { }

wxString wxStringProperty::ValueToString( wxVariant& value,
                                          int argFlags ) const
{
    wxString s = value.GetString();

    if ( GetChildCount() && HasFlag(wxPG_PROP_COMPOSED_VALUE) )
    {
        // Value stored in m_value is non-editable, non-full value
        if ( (argFlags & wxPG_FULL_VALUE) ||
             (argFlags & wxPG_EDITABLE_VALUE) ||
             !s.length() )
        {
            // Calling this under incorrect conditions will fail
            wxASSERT_MSG( argFlags & wxPG_VALUE_IS_CURRENT,
                          "Sorry, currently default wxPGProperty::ValueToString() "
                          "implementation only works if value is m_value." );

            DoGenerateComposedValue(s, argFlags);
        }

        return s;
    }

    // If string is password and value is for visual purposes,
    // then return asterisks instead the actual string.
    if ( (m_flags & wxPG_PROP_PASSWORD) && !(argFlags & (wxPG_FULL_VALUE|wxPG_EDITABLE_VALUE)) )
        return wxString(wxChar('*'), s.Length());

    return s;
}

bool wxStringProperty::StringToValue( wxVariant& variant, const wxString& text, int argFlags ) const
{
    if ( GetChildCount() && HasFlag(wxPG_PROP_COMPOSED_VALUE) )
        return wxPGProperty::StringToValue(variant, text, argFlags);

    if ( variant != text )
    {
        variant = text;
        return true;
    }

    return false;
}

bool wxStringProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_STRING_PASSWORD )
    {
        m_flags &= ~(wxPG_PROP_PASSWORD);
        if ( value.GetLong() ) m_flags |= wxPG_PROP_PASSWORD;
        RecreateEditor();
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------
// wxNumericPropertyValidator
// -----------------------------------------------------------------------

#if wxUSE_VALIDATORS

wxNumericPropertyValidator::
    wxNumericPropertyValidator( NumericType numericType, int base )
    : wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST)
{
    wxArrayString arr;
    arr.Add(wxS("0"));
    arr.Add(wxS("1"));
    arr.Add(wxS("2"));
    arr.Add(wxS("3"));
    arr.Add(wxS("4"));
    arr.Add(wxS("5"));
    arr.Add(wxS("6"));
    arr.Add(wxS("7"));

    if ( base >= 10 )
    {
        arr.Add(wxS("8"));
        arr.Add(wxS("9"));
        if ( base >= 16 )
        {
            arr.Add(wxS("a")); arr.Add(wxS("A"));
            arr.Add(wxS("b")); arr.Add(wxS("B"));
            arr.Add(wxS("c")); arr.Add(wxS("C"));
            arr.Add(wxS("d")); arr.Add(wxS("D"));
            arr.Add(wxS("e")); arr.Add(wxS("E"));
            arr.Add(wxS("f")); arr.Add(wxS("F"));
        }
    }

    if ( numericType == Signed )
    {
        arr.Add(wxS("+"));
        arr.Add(wxS("-"));
    }
    else if ( numericType == Float )
    {
        arr.Add(wxS("+"));
        arr.Add(wxS("-"));
        arr.Add(wxS("e"));

        // Use locale-specific decimal point
        arr.Add(wxString::Format("%g", 1.1)[1]);
    }

    SetIncludes(arr);
}

bool wxNumericPropertyValidator::Validate(wxWindow* parent)
{
    if ( !wxTextValidator::Validate(parent) )
        return false;

    wxWindow* wnd = GetWindow();
    if ( !wxDynamicCast(wnd, wxTextCtrl) )
        return true;

    // Do not allow zero-length string
    wxTextCtrl* tc = static_cast<wxTextCtrl*>(wnd);
    wxString text = tc->GetValue();

    if ( text.empty() )
        return false;

    return true;
}

#endif // wxUSE_VALIDATORS

// -----------------------------------------------------------------------
// wxIntProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxIntProperty,wxPGProperty,
                               long,long,TextCtrl)

wxIntProperty::wxIntProperty( const wxString& label, const wxString& name,
    long value ) : wxPGProperty(label,name)
{
    SetValue(value);
}

wxIntProperty::wxIntProperty( const wxString& label, const wxString& name,
    const wxLongLong& value ) : wxPGProperty(label,name)
{
    SetValue(WXVARIANT(value));
}

wxIntProperty::~wxIntProperty() { }

wxString wxIntProperty::ValueToString( wxVariant& value,
                                       int WXUNUSED(argFlags) ) const
{
    if ( value.GetType() == wxPG_VARIANT_TYPE_LONG )
    {
        return wxString::Format(wxS("%li"),value.GetLong());
    }
    else if ( value.GetType() == wxPG_VARIANT_TYPE_LONGLONG )
    {
        wxLongLong ll = value.GetLongLong();
        return ll.ToString();
    }

    return wxEmptyString;
}

bool wxIntProperty::StringToValue( wxVariant& variant, const wxString& text, int argFlags ) const
{
    wxString s;
    long value32;

    if ( text.empty() )
    {
        variant.MakeNull();
        return true;
    }

    // We know it is a number, but let's still check
    // the return value.
    if ( text.IsNumber() )
    {
        // Remove leading zeroes, so that the number is not interpreted as octal
        wxString::const_iterator i = text.begin();
        wxString::const_iterator iMax = text.end() - 1;  // Let's allow one, last zero though

        int firstNonZeroPos = 0;

        for ( ; i != iMax; ++i )
        {
            wxChar c = *i;
            if ( c != wxS('0') && c != wxS(' ') )
                break;
            firstNonZeroPos++;
        }

        wxString useText = text.substr(firstNonZeroPos, text.length() - firstNonZeroPos);

        wxString variantType = variant.GetType();
        bool isPrevLong = variantType == wxPG_VARIANT_TYPE_LONG;

        wxLongLong_t value64 = 0;

        if ( useText.ToLongLong(&value64, 10) &&
             ( value64 >= INT_MAX || value64 <= INT_MIN )
           )
        {
            bool doChangeValue = isPrevLong;

            if ( !isPrevLong && variantType == wxPG_VARIANT_TYPE_LONGLONG )
            {
                wxLongLong oldValue = variant.GetLongLong();
                if ( oldValue.GetValue() != value64 )
                    doChangeValue = true;
            }

            if ( doChangeValue )
            {
                wxLongLong ll(value64);
                variant = ll;
                return true;
            }
        }

        if ( useText.ToLong( &value32, 0 ) )
        {
            if ( !isPrevLong || variant != value32 )
            {
                variant = value32;
                return true;
            }
        }
    }
    else if ( argFlags & wxPG_REPORT_ERROR )
    {
    }
    return false;
}

bool wxIntProperty::IntToValue( wxVariant& variant, int value, int WXUNUSED(argFlags) ) const
{
    if ( variant.GetType() != wxPG_VARIANT_TYPE_LONG || variant != (long)value )
    {
        variant = (long)value;
        return true;
    }
    return false;
}

//
// Common validation code to be called in ValidateValue()
// implementations.
//
// Note that 'value' is reference on purpose, so we can write
// back to it when mode is wxPG_PROPERTY_VALIDATION_SATURATE.
//
template<typename T>
bool NumericValidation( const wxPGProperty* property,
                        T& value,
                        wxPGValidationInfo* pValidationInfo,
                        int mode,
                        const wxString& strFmt )
{
    T min = (T) wxINT64_MIN;
    T max = (T) wxINT64_MAX;
    wxVariant variant;
    bool minOk = false;
    bool maxOk = false;

    variant = property->GetAttribute(wxPGGlobalVars->m_strMin);
    if ( !variant.IsNull() )
    {
        variant.Convert(&min);
        minOk = true;
    }

    variant = property->GetAttribute(wxPGGlobalVars->m_strMax);
    if ( !variant.IsNull() )
    {
        variant.Convert(&max);
        maxOk = true;
    }

    if ( minOk )
    {
        if ( value < min )
        {
            if ( mode == wxPG_PROPERTY_VALIDATION_ERROR_MESSAGE )
            {
                wxString msg;
                wxString smin = wxString::Format(strFmt, min);
                wxString smax = wxString::Format(strFmt, max);
                if ( !maxOk )
                    msg = wxString::Format(
                                _("Value must be %s or higher."),
                                smin.c_str());
                else
                    msg = wxString::Format(
                                _("Value must be between %s and %s."),
                                smin.c_str(), smax.c_str());
                pValidationInfo->SetFailureMessage(msg);
            }
            else if ( mode == wxPG_PROPERTY_VALIDATION_SATURATE )
                value = min;
            else
                value = max - (min - value);
            return false;
        }
    }

    if ( maxOk )
    {
        if ( value > max )
        {
            if ( mode == wxPG_PROPERTY_VALIDATION_ERROR_MESSAGE )
            {
                wxString msg;
                wxString smin = wxString::Format(strFmt, min);
                wxString smax = wxString::Format(strFmt, max);
                if ( !minOk )
                    msg = wxString::Format(
                                _("Value must be %s or less."),
                                smax.c_str());
                else
                    msg = wxString::Format(
                                _("Value must be between %s and %s."),
                                smin.c_str(), smax.c_str());
                pValidationInfo->SetFailureMessage(msg);
            }
            else if ( mode == wxPG_PROPERTY_VALIDATION_SATURATE )
                value = max;
            else
                value = min + (value - max);
            return false;
        }
    }
    return true;
}

bool wxIntProperty::DoValidation( const wxPGProperty* property,
                                  wxLongLong_t& value,
                                  wxPGValidationInfo* pValidationInfo,
                                  int mode )
{
    return NumericValidation<wxLongLong_t>(property,
                                           value,
                                           pValidationInfo,
                                           mode,
                                           wxS("%lld"));
}

bool wxIntProperty::ValidateValue( wxVariant& value,
                                   wxPGValidationInfo& validationInfo ) const
{
    wxLongLong_t ll = value.GetLongLong().GetValue();
    return DoValidation(this, ll, &validationInfo,
                        wxPG_PROPERTY_VALIDATION_ERROR_MESSAGE);
}

wxValidator* wxIntProperty::GetClassValidator()
{
#if wxUSE_VALIDATORS
    WX_PG_DOGETVALIDATOR_ENTRY()

    wxValidator* validator = new wxNumericPropertyValidator(
                                    wxNumericPropertyValidator::Signed);

    WX_PG_DOGETVALIDATOR_EXIT(validator)
#else
    return NULL;
#endif
}

wxValidator* wxIntProperty::DoGetValidator() const
{
    return GetClassValidator();
}

// -----------------------------------------------------------------------
// wxUIntProperty
// -----------------------------------------------------------------------

enum
{
    wxPG_UINT_HEX_LOWER,
    wxPG_UINT_HEX_LOWER_PREFIX,
    wxPG_UINT_HEX_LOWER_DOLLAR,
    wxPG_UINT_HEX_UPPER,
    wxPG_UINT_HEX_UPPER_PREFIX,
    wxPG_UINT_HEX_UPPER_DOLLAR,
    wxPG_UINT_DEC,
    wxPG_UINT_OCT,
    wxPG_UINT_TEMPLATE_MAX
};

static const wxChar* const gs_uintTemplates32[wxPG_UINT_TEMPLATE_MAX] = {
    wxT("%lx"),wxT("0x%lx"),wxT("$%lx"),
    wxT("%lX"),wxT("0x%lX"),wxT("$%lX"),
    wxT("%lu"),wxT("%lo")
};

static const char* const gs_uintTemplates64[wxPG_UINT_TEMPLATE_MAX] = {
    "%" wxLongLongFmtSpec "x",
    "0x%" wxLongLongFmtSpec "x",
    "$%" wxLongLongFmtSpec "x",
    "%" wxLongLongFmtSpec "X",
    "0x%" wxLongLongFmtSpec "X",
    "$%" wxLongLongFmtSpec "X",
    "%" wxLongLongFmtSpec "u",
    "%" wxLongLongFmtSpec "o"
};

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxUIntProperty,wxPGProperty,
                               long,unsigned long,TextCtrl)

void wxUIntProperty::Init()
{
    m_base = wxPG_UINT_DEC;
    m_realBase = 10;
    m_prefix = wxPG_PREFIX_NONE;
}

wxUIntProperty::wxUIntProperty( const wxString& label, const wxString& name,
    unsigned long value ) : wxPGProperty(label,name)
{
    Init();
    SetValue((long)value);
}

wxUIntProperty::wxUIntProperty( const wxString& label, const wxString& name,
    const wxULongLong& value ) : wxPGProperty(label,name)
{
    Init();
    SetValue(WXVARIANT(value));
}

wxUIntProperty::~wxUIntProperty() { }

wxString wxUIntProperty::ValueToString( wxVariant& value,
                                        int WXUNUSED(argFlags) ) const
{
    size_t index = m_base + m_prefix;
    if ( index >= wxPG_UINT_TEMPLATE_MAX )
        index = wxPG_UINT_DEC;

    if ( value.GetType() == wxPG_VARIANT_TYPE_LONG )
    {
        return wxString::Format(gs_uintTemplates32[index],
                                (unsigned long)value.GetLong());
    }

    wxULongLong ull = value.GetULongLong();

    return wxString::Format(gs_uintTemplates64[index], ull.GetValue());
}

bool wxUIntProperty::StringToValue( wxVariant& variant, const wxString& text, int WXUNUSED(argFlags) ) const
{
    wxString variantType = variant.GetType();
    bool isPrevLong = variantType == wxPG_VARIANT_TYPE_LONG;

    if ( text.empty() )
    {
        variant.MakeNull();
        return true;
    }

    size_t start = 0;
    if ( text[0] == wxS('$') )
        start++;

    wxULongLong_t value64 = 0;
    wxString s = text.substr(start, text.length() - start);

    if ( s.ToULongLong(&value64, (unsigned int)m_realBase) )
    {
        if ( value64 >= LONG_MAX )
        {
            bool doChangeValue = isPrevLong;

            if ( !isPrevLong && variantType == wxPG_VARIANT_TYPE_ULONGLONG )
            {
                wxULongLong oldValue = variant.GetULongLong();
                if ( oldValue.GetValue() != value64 )
                    doChangeValue = true;
            }

            if ( doChangeValue )
            {
                variant = wxULongLong(value64);
                return true;
            }
        }
        else
        {
            unsigned long value32 = wxLongLong(value64).GetLo();
            if ( !isPrevLong || m_value != (long)value32 )
            {
                variant = (long)value32;
                return true;
            }
        }

    }
    return false;
}

bool wxUIntProperty::IntToValue( wxVariant& variant, int number, int WXUNUSED(argFlags) ) const
{
    if ( variant != (long)number )
    {
        variant = (long)number;
        return true;
    }
    return false;
}

bool wxUIntProperty::ValidateValue( wxVariant& value, wxPGValidationInfo& validationInfo ) const
{
    wxULongLong_t uul = value.GetULongLong().GetValue();
    return
    NumericValidation<wxULongLong_t>(this,
                                     uul,
                                     &validationInfo,
                                     wxPG_PROPERTY_VALIDATION_ERROR_MESSAGE,
                                     wxS("%llu"));
}

wxValidator* wxUIntProperty::DoGetValidator() const
{
#if wxUSE_VALIDATORS
    WX_PG_DOGETVALIDATOR_ENTRY()

    wxValidator* validator = new wxNumericPropertyValidator(
                                    wxNumericPropertyValidator::Unsigned,
                                    m_realBase);

    WX_PG_DOGETVALIDATOR_EXIT(validator)
#else
    return NULL;
#endif
}

bool wxUIntProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_UINT_BASE )
    {
        int val = value.GetLong();

        m_realBase = (wxByte) val;
        if ( m_realBase > 16 )
            m_realBase = 16;

        //
        // Translate logical base to a template array index
        m_base = wxPG_UINT_OCT;
        if ( val == wxPG_BASE_HEX )
            m_base = wxPG_UINT_HEX_UPPER;
        else if ( val == wxPG_BASE_DEC )
            m_base = wxPG_UINT_DEC;
        else if ( val == wxPG_BASE_HEXL )
            m_base = wxPG_UINT_HEX_LOWER_DOLLAR;
        return true;
    }
    else if ( name == wxPG_UINT_PREFIX )
    {
        m_prefix = (wxByte) value.GetLong();
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxFloatProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxFloatProperty,wxPGProperty,
                               double,double,TextCtrl)

wxFloatProperty::wxFloatProperty( const wxString& label,
                                            const wxString& name,
                                            double value )
    : wxPGProperty(label,name)
{
    m_precision = -1;
    SetValue(value);
}

wxFloatProperty::~wxFloatProperty() { }

// This helper method provides standard way for floating point-using
// properties to convert values to string.
const wxString& wxPropertyGrid::DoubleToString(wxString& target,
                                               double value,
                                               int precision,
                                               bool removeZeroes,
                                               wxString* precTemplate)
{
    if ( precision >= 0 )
    {
        wxString text1;
        if (!precTemplate)
            precTemplate = &text1;

        if ( precTemplate->empty() )
        {
            *precTemplate = wxS("%.");
            *precTemplate << wxString::Format( wxS("%i"), precision );
            *precTemplate << wxS('f');
        }

        target.Printf( precTemplate->c_str(), value );
    }
    else
    {
        target.Printf( wxS("%f"), value );
    }

    if ( removeZeroes && precision != 0 && !target.empty() )
    {
        // Remove excess zeroes (do not remove this code just yet,
        // since sprintf can't do the same consistently across platforms).
        wxString::const_iterator i = target.end() - 1;
        size_t new_len = target.length() - 1;

        for ( ; i != target.begin(); --i )
        {
            if ( *i != wxS('0') )
                break;
            new_len--;
        }

        wxChar cur_char = *i;
        if ( cur_char != wxS('.') && cur_char != wxS(',') )
            new_len++;

        if ( new_len != target.length() )
            target.resize(new_len);
    }

    // Remove sign from zero
    if ( target.length() >= 2 && target[0] == wxS('-') )
    {
        bool isZero = true;
        wxString::const_iterator i = target.begin() + 1;

        for ( ; i != target.end(); i++ )
        {
            if ( *i != wxS('0') && *i != wxS('.') && *i != wxS(',') )
            {
                isZero = false;
                break;
            }
        }

        if ( isZero )
            target.erase(target.begin());
    }

    return target;
}

wxString wxFloatProperty::ValueToString( wxVariant& value,
                                         int argFlags ) const
{
    wxString text;
    if ( !value.IsNull() )
    {
        wxPropertyGrid::DoubleToString(text,
                                       value,
                                       m_precision,
                                       !(argFlags & wxPG_FULL_VALUE),
                                       NULL);
    }
    return text;
}

bool wxFloatProperty::StringToValue( wxVariant& variant, const wxString& text, int argFlags ) const
{
    wxString s;
    double value;

    if ( text.empty() )
    {
        variant.MakeNull();
        return true;
    }

    bool res = text.ToDouble(&value);
    if ( res )
    {
        if ( variant != value )
        {
            variant = value;
            return true;
        }
    }
    else if ( argFlags & wxPG_REPORT_ERROR )
    {
    }
    return false;
}

bool wxFloatProperty::DoValidation( const wxPGProperty* property,
                                    double& value,
                                    wxPGValidationInfo* pValidationInfo,
                                    int mode )
{
    return NumericValidation<double>(property,
                                     value,
                                     pValidationInfo,
                                     mode,
                                     wxS("%g"));
}

bool
wxFloatProperty::ValidateValue( wxVariant& value,
                                wxPGValidationInfo& validationInfo ) const
{
    double fpv = value.GetDouble();
    return DoValidation(this, fpv, &validationInfo,
                        wxPG_PROPERTY_VALIDATION_ERROR_MESSAGE);
}

bool wxFloatProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_FLOAT_PRECISION )
    {
        m_precision = value.GetLong();
        return true;
    }
    return false;
}

wxValidator*
wxFloatProperty::GetClassValidator()
{
#if wxUSE_VALIDATORS
    WX_PG_DOGETVALIDATOR_ENTRY()

    wxValidator* validator = new wxNumericPropertyValidator(
                                    wxNumericPropertyValidator::Float);

    WX_PG_DOGETVALIDATOR_EXIT(validator)
#else
    return NULL;
#endif
}

wxValidator* wxFloatProperty::DoGetValidator() const
{
    return GetClassValidator();
}

// -----------------------------------------------------------------------
// wxBoolProperty
// -----------------------------------------------------------------------

// We cannot use standard WX_PG_IMPLEMENT_PROPERTY_CLASS macro, since
// there is a custom GetEditorClass.

IMPLEMENT_DYNAMIC_CLASS(wxBoolProperty, wxPGProperty)

const wxPGEditor* wxBoolProperty::DoGetEditorClass() const
{
    // Select correct editor control.
#if wxPG_INCLUDE_CHECKBOX
    if ( !(m_flags & wxPG_PROP_USE_CHECKBOX) )
        return wxPGEditor_Choice;
    return wxPGEditor_CheckBox;
#else
    return wxPGEditor_Choice;
#endif
}

wxBoolProperty::wxBoolProperty( const wxString& label, const wxString& name, bool value ) :
    wxPGProperty(label,name)
{
    m_choices.Assign(wxPGGlobalVars->m_boolChoices);

    SetValue(wxPGVariant_Bool(value));

    m_flags |= wxPG_PROP_USE_DCC;
}

wxBoolProperty::~wxBoolProperty() { }

wxString wxBoolProperty::ValueToString( wxVariant& value,
                                        int argFlags ) const
{
    bool boolValue = value.GetBool();

    // As a fragment of composite string value,
    // make it a little more readable.
    if ( argFlags & wxPG_COMPOSITE_FRAGMENT )
    {
        if ( boolValue )
        {
            return m_label;
        }
        else
        {
            if ( argFlags & wxPG_UNEDITABLE_COMPOSITE_FRAGMENT )
                return wxEmptyString;

            wxString notFmt;
            if ( wxPGGlobalVars->m_autoGetTranslation )
                notFmt = _("Not %s");
            else
                notFmt = wxS("Not %s");

            return wxString::Format(notFmt.c_str(), m_label.c_str());
        }
    }

    if ( !(argFlags & wxPG_FULL_VALUE) )
    {
        return wxPGGlobalVars->m_boolChoices[boolValue?1:0].GetText();
    }

    wxString text;

    if ( boolValue ) text = wxS("true");
    else text = wxS("false");

    return text;
}

bool wxBoolProperty::StringToValue( wxVariant& variant, const wxString& text, int WXUNUSED(argFlags) ) const
{
    bool boolValue = false;
    if ( text.CmpNoCase(wxPGGlobalVars->m_boolChoices[1].GetText()) == 0 ||
         text.CmpNoCase(wxS("true")) == 0 ||
         text.CmpNoCase(m_label) == 0 )
        boolValue = true;

    if ( text.empty() )
    {
        variant.MakeNull();
        return true;
    }

    if ( variant != boolValue )
    {
        variant = wxPGVariant_Bool(boolValue);
        return true;
    }
    return false;
}

bool wxBoolProperty::IntToValue( wxVariant& variant, int value, int ) const
{
    bool boolValue = value ? true : false;

    if ( variant != boolValue )
    {
        variant = wxPGVariant_Bool(boolValue);
        return true;
    }
    return false;
}

bool wxBoolProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
#if wxPG_INCLUDE_CHECKBOX
    if ( name == wxPG_BOOL_USE_CHECKBOX )
    {
        if ( value.GetLong() )
            m_flags |= wxPG_PROP_USE_CHECKBOX;
        else
            m_flags &= ~(wxPG_PROP_USE_CHECKBOX);
        return true;
    }
#endif
    if ( name == wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING )
    {
        if ( value.GetLong() )
            m_flags |= wxPG_PROP_USE_DCC;
        else
            m_flags &= ~(wxPG_PROP_USE_DCC);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxEnumProperty
// -----------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxEnumProperty, wxPGProperty)

WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(wxEnumProperty,long,Choice)

wxEnumProperty::wxEnumProperty( const wxString& label, const wxString& name, const wxChar* const* labels,
    const long* values, int value ) : wxPGProperty(label,name)
{
    SetIndex(0);

    if ( labels )
    {
        m_choices.Add(labels,values);

        if ( GetItemCount() )
            SetValue( (long)value );
    }
}

wxEnumProperty::wxEnumProperty( const wxString& label, const wxString& name, const wxChar* const* labels,
    const long* values, wxPGChoices* choicesCache, int value )
    : wxPGProperty(label,name)
{
    SetIndex(0);

    wxASSERT( choicesCache );

    if ( choicesCache->IsOk() )
    {
        m_choices.Assign( *choicesCache );
        m_value = wxPGVariant_Zero;
    }
    else if ( labels )
    {
        m_choices.Add(labels,values);

        if ( GetItemCount() )
            SetValue( (long)value );
    }
}

wxEnumProperty::wxEnumProperty( const wxString& label, const wxString& name,
    const wxArrayString& labels, const wxArrayInt& values, int value )
    : wxPGProperty(label,name)
{
    SetIndex(0);

    if ( &labels && labels.size() )
    {
        m_choices.Set(labels, values);

        if ( GetItemCount() )
            SetValue( (long)value );
    }
}

wxEnumProperty::wxEnumProperty( const wxString& label, const wxString& name,
    wxPGChoices& choices, int value )
    : wxPGProperty(label,name)
{
    m_choices.Assign( choices );

    if ( GetItemCount() )
        SetValue( (long)value );
}

int wxEnumProperty::GetIndexForValue( int value ) const
{
    if ( !m_choices.IsOk() )
        return -1;

    int intVal = m_choices.Index(value);
    if ( intVal >= 0 )
        return intVal;

    return value;
}

wxEnumProperty::~wxEnumProperty ()
{
}

int wxEnumProperty::ms_nextIndex = -2;

void wxEnumProperty::OnSetValue()
{
    wxString variantType = m_value.GetType();

    if ( variantType == wxPG_VARIANT_TYPE_LONG )
    {
        ValueFromInt_( m_value, m_value.GetLong(), wxPG_FULL_VALUE );
    }
    else if ( variantType == wxPG_VARIANT_TYPE_STRING )
    {
        ValueFromString_( m_value, m_value.GetString(), 0 );
    }
    else
    {
        wxFAIL;
    }

    if ( ms_nextIndex != -2 )
    {
        m_index = ms_nextIndex;
        ms_nextIndex = -2;
    }
}

bool wxEnumProperty::ValidateValue( wxVariant& value, wxPGValidationInfo& WXUNUSED(validationInfo) ) const
{
    // Make sure string value is in the list,
    // unless property has string as preferred value type
    // To reduce code size, use conversion here as well
    if ( value.GetType() == wxPG_VARIANT_TYPE_STRING &&
         !wxDynamicCastThis(wxEditEnumProperty) )
        return ValueFromString_( value, value.GetString(), wxPG_PROPERTY_SPECIFIC );

    return true;
}

wxString wxEnumProperty::ValueToString( wxVariant& value,
                                            int WXUNUSED(argFlags) ) const
{
    if ( value.GetType() == wxPG_VARIANT_TYPE_STRING )
        return value.GetString();

    int index = m_choices.Index(value.GetLong());
    if ( index < 0 )
        return wxEmptyString;

    return m_choices.GetLabel(index);
}

bool wxEnumProperty::StringToValue( wxVariant& variant, const wxString& text, int argFlags ) const
{
    return ValueFromString_( variant, text, argFlags );
}

bool wxEnumProperty::IntToValue( wxVariant& variant, int intVal, int argFlags ) const
{
    return ValueFromInt_( variant, intVal, argFlags );
}

bool wxEnumProperty::ValueFromString_( wxVariant& value, const wxString& text, int argFlags ) const
{
    int useIndex = -1;
    long useValue = 0;

    for ( unsigned int i=0; i<m_choices.GetCount(); i++ )
    {
        const wxString& entryLabel = m_choices.GetLabel(i);
        if ( text.CmpNoCase(entryLabel) == 0 )
        {
            useIndex = (int)i;
            useValue = m_choices.GetValue(i);
            break;
        }
    }

    bool asText = false;

    bool isEdit = this->IsKindOf(wxCLASSINFO(wxEditEnumProperty));

    // If text not any of the choices, store as text instead
    // (but only if we are wxEditEnumProperty)
    if ( useIndex == -1 && isEdit )
    {
        asText = true;
    }

    int setAsNextIndex = -2;

    if ( asText )
    {
        setAsNextIndex = -1;
        value = text;
    }
    else if ( useIndex != GetIndex() )
    {
        if ( useIndex != -1 )
        {
            setAsNextIndex = useIndex;
            value = (long)useValue;
        }
        else
        {
            setAsNextIndex = -1;
            value = wxPGVariant_MinusOne;
        }
    }

    if ( setAsNextIndex != -2 )
    {
        // If wxPG_PROPERTY_SPECIFIC is set, then this is done for
        // validation purposes only, and index must not be changed
        if ( !(argFlags & wxPG_PROPERTY_SPECIFIC) )
            ms_nextIndex = setAsNextIndex;

        if ( isEdit || setAsNextIndex != -1 )
            return true;
        else
            return false;
    }
    return false;
}

bool wxEnumProperty::ValueFromInt_( wxVariant& variant, int intVal, int argFlags ) const
{
    // If wxPG_FULL_VALUE is *not* in argFlags, then intVal is index from combo box.
    //
    int setAsNextIndex = -2;

    if ( argFlags & wxPG_FULL_VALUE )
    {
        setAsNextIndex = GetIndexForValue( intVal );
    }
    else
    {
        if ( intVal != GetIndex() )
        {
           setAsNextIndex = intVal;
        }
    }

    if ( setAsNextIndex != -2 )
    {
        // If wxPG_PROPERTY_SPECIFIC is set, then this is done for
        // validation or fetching a data purposes only, and index must not be changed.
        if ( !(argFlags & wxPG_PROPERTY_SPECIFIC) )
            ms_nextIndex = setAsNextIndex;

        if ( !(argFlags & wxPG_FULL_VALUE) )
            intVal = m_choices.GetValue(intVal);

        variant = (long)intVal;

        return true;
    }

    return false;
}

void
wxEnumProperty::OnValidationFailure( wxVariant& WXUNUSED(pendingValue) )
{
    // Revert index
    ResetNextIndex();
}

void wxEnumProperty::SetIndex( int index )
{
    ms_nextIndex = -2;
    m_index = index;
}

int wxEnumProperty::GetIndex() const
{
    if ( m_value.IsNull() )
        return -1;

    if ( ms_nextIndex != -2 )
        return ms_nextIndex;

    return m_index;
}

// -----------------------------------------------------------------------
// wxEditEnumProperty
// -----------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxEditEnumProperty, wxPGProperty)

WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(wxEditEnumProperty,wxString,ComboBox)

wxEditEnumProperty::wxEditEnumProperty( const wxString& label, const wxString& name, const wxChar* const* labels,
    const long* values, const wxString& value )
    : wxEnumProperty(label,name,labels,values,0)
{
    SetValue( value );
}

wxEditEnumProperty::wxEditEnumProperty( const wxString& label, const wxString& name, const wxChar* const* labels,
    const long* values, wxPGChoices* choicesCache, const wxString& value )
    : wxEnumProperty(label,name,labels,values,choicesCache,0)
{
    SetValue( value );
}

wxEditEnumProperty::wxEditEnumProperty( const wxString& label, const wxString& name,
    const wxArrayString& labels, const wxArrayInt& values, const wxString& value )
    : wxEnumProperty(label,name,labels,values,0)
{
    SetValue( value );
}

wxEditEnumProperty::wxEditEnumProperty( const wxString& label, const wxString& name,
    wxPGChoices& choices, const wxString& value )
    : wxEnumProperty(label,name,choices,0)
{
    SetValue( value );
}

wxEditEnumProperty::~wxEditEnumProperty()
{
}

// -----------------------------------------------------------------------
// wxFlagsProperty
// -----------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFlagsProperty,wxPGProperty)

WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(wxFlagsProperty,long,TextCtrl)

void wxFlagsProperty::Init()
{
    long value = m_value;

    //
    // Generate children
    //
    unsigned int i;

    unsigned int prevChildCount = m_children.size();

    int oldSel = -1;
    if ( prevChildCount )
    {
        wxPropertyGridPageState* state = GetParentState();

        // State safety check (it may be NULL in immediate parent)
        wxASSERT( state );

        if ( state )
        {
            wxPGProperty* selected = state->GetSelection();
            if ( selected )
            {
                if ( selected->GetParent() == this )
                    oldSel = selected->GetIndexInParent();
                else if ( selected == this )
                    oldSel = -2;
            }
        }
        state->DoClearSelection();
    }

    // Delete old children
    for ( i=0; i<prevChildCount; i++ )
        delete m_children[i];

    m_children.clear();

    // Relay wxPG_BOOL_USE_CHECKBOX and wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING
    // to child bool property controls.
    long attrUseCheckBox = GetAttributeAsLong(wxPG_BOOL_USE_CHECKBOX, 0);
    long attrUseDCC = GetAttributeAsLong(wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING,
                                         0);

    if ( m_choices.IsOk() )
    {
        const wxPGChoices& choices = m_choices;

        for ( i=0; i<GetItemCount(); i++ )
        {
            bool child_val;
            child_val = ( value & choices.GetValue(i) )?true:false;

            wxPGProperty* boolProp;
            wxString label = GetLabel(i);

        #if wxUSE_INTL
            if ( wxPGGlobalVars->m_autoGetTranslation )
            {
                boolProp = new wxBoolProperty( ::wxGetTranslation(label), label, child_val );
            }
            else
        #endif
            {
                boolProp = new wxBoolProperty( label, label, child_val );
            }
            if ( attrUseCheckBox )
                boolProp->SetAttribute(wxPG_BOOL_USE_CHECKBOX,
                                       true);
            if ( attrUseDCC )
                boolProp->SetAttribute(wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING,
                                       true);
            AddPrivateChild(boolProp);
        }

        m_oldChoicesData = m_choices.GetDataPtr();
    }

    m_oldValue = m_value;

    if ( prevChildCount )
        SubPropsChanged(oldSel);
}

wxFlagsProperty::wxFlagsProperty( const wxString& label, const wxString& name,
    const wxChar* const* labels, const long* values, long value ) : wxPGProperty(label,name)
{
    m_oldChoicesData = NULL;

    if ( labels )
    {
        m_choices.Set(labels,values);

        wxASSERT( GetItemCount() );

        SetValue( value );
    }
    else
    {
        m_value = wxPGVariant_Zero;
    }
}

wxFlagsProperty::wxFlagsProperty( const wxString& label, const wxString& name,
        const wxArrayString& labels, const wxArrayInt& values, int value )
    : wxPGProperty(label,name)
{
    m_oldChoicesData = NULL;

    if ( &labels && labels.size() )
    {
        m_choices.Set(labels,values);

        wxASSERT( GetItemCount() );

        SetValue( (long)value );
    }
    else
    {
        m_value = wxPGVariant_Zero;
    }
}

wxFlagsProperty::wxFlagsProperty( const wxString& label, const wxString& name,
    wxPGChoices& choices, long value )
    : wxPGProperty(label,name)
{
    m_oldChoicesData = NULL;

    if ( choices.IsOk() )
    {
        m_choices.Assign(choices);

        wxASSERT( GetItemCount() );

        SetValue( value );
    }
    else
    {
        m_value = wxPGVariant_Zero;
    }
}

wxFlagsProperty::~wxFlagsProperty()
{
}

void wxFlagsProperty::OnSetValue()
{
    if ( !m_choices.IsOk() || !GetItemCount() )
    {
        m_value = wxPGVariant_Zero;
    }
    else
    {
        long val = m_value.GetLong();

        long fullFlags = 0;

        // normalize the value (i.e. remove extra flags)
        unsigned int i;
        const wxPGChoices& choices = m_choices;
        for ( i = 0; i < GetItemCount(); i++ )
        {
            fullFlags |= choices.GetValue(i);
        }

        val &= fullFlags;

        m_value = val;

        // Need to (re)init now?
        if ( GetChildCount() != GetItemCount() ||
             m_choices.GetDataPtr() != m_oldChoicesData )
        {
            Init();
        }
    }

    long newFlags = m_value;

    if ( newFlags != m_oldValue )
    {
        // Set child modified states
        unsigned int i;
        const wxPGChoices& choices = m_choices;
        for ( i = 0; i<GetItemCount(); i++ )
        {
            int flag;

            flag = choices.GetValue(i);

            if ( (newFlags & flag) != (m_oldValue & flag) )
                Item(i)->ChangeFlag( wxPG_PROP_MODIFIED, true );
        }

        m_oldValue = newFlags;
    }
}

wxString wxFlagsProperty::ValueToString( wxVariant& value,
                                         int WXUNUSED(argFlags) ) const
{
    wxString text;

    if ( !m_choices.IsOk() )
        return text;

    long flags = value;
    unsigned int i;
    const wxPGChoices& choices = m_choices;

    for ( i = 0; i < GetItemCount(); i++ )
    {
        int doAdd;
        doAdd = ( (flags & choices.GetValue(i)) == choices.GetValue(i) );

        if ( doAdd )
        {
            text += choices.GetLabel(i);
            text += wxS(", ");
        }
    }

    // remove last comma
    if ( text.Len() > 1 )
        text.Truncate ( text.Len() - 2 );

    return text;
}

// Translate string into flag tokens
bool wxFlagsProperty::StringToValue( wxVariant& variant, const wxString& text, int ) const
{
    if ( !m_choices.IsOk() )
        return false;

    long newFlags = 0;

    // semicolons are no longer valid delimeters
    WX_PG_TOKENIZER1_BEGIN(text,wxS(','))

        if ( !token.empty() )
        {
            // Determine which one it is
            long bit = IdToBit( token );

            if ( bit != -1 )
            {
                // Changed?
                newFlags |= bit;
            }
            else
            {
                break;
            }
        }

    WX_PG_TOKENIZER1_END()

    if ( variant != (long)newFlags )
    {
        variant = (long)newFlags;
        return true;
    }

    return false;
}

// Converts string id to a relevant bit.
long wxFlagsProperty::IdToBit( const wxString& id ) const
{
    unsigned int i;
    for ( i = 0; i < GetItemCount(); i++ )
    {
        if ( id == GetLabel(i) )
        {
            return m_choices.GetValue(i);
        }
    }
    return -1;
}

void wxFlagsProperty::RefreshChildren()
{
    if ( !m_choices.IsOk() || !GetChildCount() ) return;

    int flags = m_value.GetLong();

    const wxPGChoices& choices = m_choices;
    unsigned int i;
    for ( i = 0; i < GetItemCount(); i++ )
    {
        long flag;

        flag = choices.GetValue(i);

        long subVal = flags & flag;
        wxPGProperty* p = Item(i);

        if ( subVal != (m_oldValue & flag) )
            p->ChangeFlag( wxPG_PROP_MODIFIED, true );

        p->SetValue( subVal == flag?true:false );
    }

    m_oldValue = flags;
}

wxVariant wxFlagsProperty::ChildChanged( wxVariant& thisValue,
                                         int childIndex,
                                         wxVariant& childValue ) const
{
    long oldValue = thisValue.GetLong();
    long val = childValue.GetLong();
    unsigned long vi = m_choices.GetValue(childIndex);

    if ( val )
        return (long) (oldValue | vi);

    return (long) (oldValue & ~(vi));
}

bool wxFlagsProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_BOOL_USE_CHECKBOX ||
         name == wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING )
    {
        for ( size_t i=0; i<GetChildCount(); i++ )
        {
            Item(i)->SetAttribute(name, value);
        }
        // Must return false so that the attribute is stored in
        // flag property's actual property storage
        return false;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxDirProperty
// -----------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDirProperty, wxLongStringProperty)

wxDirProperty::wxDirProperty( const wxString& name, const wxString& label, const wxString& value )
  : wxLongStringProperty(name,label,value)
{
    m_flags |= wxPG_PROP_NO_ESCAPE;
}

wxDirProperty::~wxDirProperty() { }

wxValidator* wxDirProperty::DoGetValidator() const
{
    return wxFileProperty::GetClassValidator();
}

bool wxDirProperty::OnButtonClick( wxPropertyGrid* propGrid, wxString& value )
{
    // Update property value from editor, if necessary
    wxSize dlg_sz(300,400);

    wxString dlgMessage(m_dlgMessage);
    if ( dlgMessage.empty() )
        dlgMessage = _("Choose a directory:");
    wxDirDialog dlg( propGrid,
                     dlgMessage,
                     value,
                     0,
#if !wxPG_SMALL_SCREEN
                     propGrid->GetGoodEditorDialogPosition(this,dlg_sz),
                     dlg_sz
#else
                     wxDefaultPosition,
                     wxDefaultSize
#endif
                    );

    if ( dlg.ShowModal() == wxID_OK )
    {
        value = dlg.GetPath();
        return true;
    }
    return false;
}

bool wxDirProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_DIR_DIALOG_MESSAGE )
    {
        m_dlgMessage = value.GetString();
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxPGFileDialogAdapter
// -----------------------------------------------------------------------

bool wxPGFileDialogAdapter::DoShowDialog( wxPropertyGrid* propGrid, wxPGProperty* property )
{
    wxFileProperty* fileProp = NULL;
    wxString path;
    int indFilter = -1;

    if ( wxDynamicCast(property, wxFileProperty) )
    {
        fileProp = ((wxFileProperty*)property);
        wxFileName filename = fileProp->GetValue().GetString();
        path = filename.GetPath();
        indFilter = fileProp->m_indFilter;

        if ( path.empty() && !fileProp->m_basePath.empty() )
            path = fileProp->m_basePath;
    }
    else
    {
        wxFileName fn(property->GetValue().GetString());
        path = fn.GetPath();
    }

    wxFileDialog dlg( propGrid->GetPanel(),
                      property->GetAttribute(wxS("DialogTitle"), _("Choose a file")),
                      property->GetAttribute(wxS("InitialPath"), path),
                      wxEmptyString,
                      property->GetAttribute(wxPG_FILE_WILDCARD, wxALL_FILES),
                      property->GetAttributeAsLong(wxPG_FILE_DIALOG_STYLE, 0),
                      wxDefaultPosition );

    if ( indFilter >= 0 )
        dlg.SetFilterIndex( indFilter );

    if ( dlg.ShowModal() == wxID_OK )
    {
        if ( fileProp )
            fileProp->m_indFilter = dlg.GetFilterIndex();
        SetValue( dlg.GetPath() );
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxFileProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxFileProperty,wxPGProperty,
                               wxString,const wxString&,TextCtrlAndButton)

wxFileProperty::wxFileProperty( const wxString& label, const wxString& name,
    const wxString& value ) : wxPGProperty(label,name)
{
    m_flags |= wxPG_PROP_SHOW_FULL_FILENAME;
    m_indFilter = -1;
    SetAttribute( wxPG_FILE_WILDCARD, wxALL_FILES);

    SetValue(value);
}

wxFileProperty::~wxFileProperty() {}

wxValidator* wxFileProperty::GetClassValidator()
{
#if wxUSE_VALIDATORS
    WX_PG_DOGETVALIDATOR_ENTRY()

    // Atleast wxPython 2.6.2.1 required that the string argument is given
    static wxString v;
    wxTextValidator* validator = new wxTextValidator(wxFILTER_EXCLUDE_CHAR_LIST,&v);

    wxArrayString exChars;
    exChars.Add(wxS("?"));
    exChars.Add(wxS("*"));
    exChars.Add(wxS("|"));
    exChars.Add(wxS("<"));
    exChars.Add(wxS(">"));
    exChars.Add(wxS("\""));

    validator->SetExcludes(exChars);

    WX_PG_DOGETVALIDATOR_EXIT(validator)
#else
    return NULL;
#endif
}

wxValidator* wxFileProperty::DoGetValidator() const
{
    return GetClassValidator();
}

void wxFileProperty::OnSetValue()
{
    const wxString& fnstr = m_value.GetString();

    wxFileName filename = fnstr;

    if ( !filename.HasName() )
    {
        m_value = wxPGVariant_EmptyString;
    }

    // Find index for extension.
    if ( m_indFilter < 0 && !fnstr.empty() )
    {
        wxString ext = filename.GetExt();
        int curind = 0;
        size_t pos = 0;
        size_t len = m_wildcard.length();

        pos = m_wildcard.find(wxS("|"), pos);
        while ( pos != wxString::npos && pos < (len-3) )
        {
            size_t ext_begin = pos + 3;

            pos = m_wildcard.find(wxS("|"), ext_begin);
            if ( pos == wxString::npos )
                pos = len;
            wxString found_ext = m_wildcard.substr(ext_begin, pos-ext_begin);

            if ( !found_ext.empty() )
            {
                if ( found_ext[0] == wxS('*') )
                {
                    m_indFilter = curind;
                    break;
                }
                if ( ext.CmpNoCase(found_ext) == 0 )
                {
                    m_indFilter = curind;
                    break;
                }
            }

            if ( pos != len )
                pos = m_wildcard.find(wxS("|"), pos+1);

            curind++;
        }
    }
}

wxFileName wxFileProperty::GetFileName() const
{
    wxFileName filename;

    if ( !m_value.IsNull() )
        filename = m_value.GetString();

    return filename;
}

wxString wxFileProperty::ValueToString( wxVariant& value,
                                        int argFlags ) const
{
    wxFileName filename = value.GetString();

    if ( !filename.HasName() )
        return wxEmptyString;

    wxString fullName = filename.GetFullName();
    if ( fullName.empty() )
        return wxEmptyString;

    if ( argFlags & wxPG_FULL_VALUE )
    {
        return filename.GetFullPath();
    }
    else if ( m_flags & wxPG_PROP_SHOW_FULL_FILENAME )
    {
        if ( !m_basePath.empty() )
        {
            wxFileName fn2(filename);
            fn2.MakeRelativeTo(m_basePath);
            return fn2.GetFullPath();
        }
        return filename.GetFullPath();
    }

    return filename.GetFullName();
}

wxPGEditorDialogAdapter* wxFileProperty::GetEditorDialog() const
{
    return new wxPGFileDialogAdapter();
}

bool wxFileProperty::StringToValue( wxVariant& variant, const wxString& text, int argFlags ) const
{
    wxFileName filename = variant.GetString();

    if ( (m_flags & wxPG_PROP_SHOW_FULL_FILENAME) || (argFlags & wxPG_FULL_VALUE) )
    {
        if ( filename != text )
        {
            variant = text;
            return true;
        }
    }
    else
    {
        if ( filename.GetFullName() != text )
        {
            wxFileName fn = filename;
            fn.SetFullName(text);
            variant = fn.GetFullPath();
            return true;
        }
    }

    return false;
}

bool wxFileProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    // Return false on some occasions to make sure those attribs will get
    // stored in m_attributes.
    if ( name == wxPG_FILE_SHOW_FULL_PATH )
    {
        if ( value.GetLong() )
            m_flags |= wxPG_PROP_SHOW_FULL_FILENAME;
        else
            m_flags &= ~(wxPG_PROP_SHOW_FULL_FILENAME);
        return true;
    }
    else if ( name == wxPG_FILE_WILDCARD )
    {
        m_wildcard = value.GetString();
    }
    else if ( name == wxPG_FILE_SHOW_RELATIVE_PATH )
    {
        m_basePath = value.GetString();

        // Make sure wxPG_FILE_SHOW_FULL_PATH is also set
        m_flags |= wxPG_PROP_SHOW_FULL_FILENAME;
    }
    else if ( name == wxPG_FILE_INITIAL_PATH )
    {
        m_initialPath = value.GetString();
        return true;
    }
    else if ( name == wxPG_FILE_DIALOG_TITLE )
    {
        m_dlgTitle = value.GetString();
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// wxPGLongStringDialogAdapter
// -----------------------------------------------------------------------

bool wxPGLongStringDialogAdapter::DoShowDialog( wxPropertyGrid* propGrid, wxPGProperty* property )
{
    wxString val1 = property->GetValueAsString(0);
    wxString val_orig = val1;

    wxString value;
    if ( !property->HasFlag(wxPG_PROP_NO_ESCAPE) )
        wxPropertyGrid::ExpandEscapeSequences(value, val1);
    else
        value = wxString(val1);

    // Run editor dialog.
    if ( wxLongStringProperty::DisplayEditorDialog(property, propGrid, value) )
    {
        if ( !property->HasFlag(wxPG_PROP_NO_ESCAPE) )
            wxPropertyGrid::CreateEscapeSequences(val1,value);
        else
            val1 = value;

        if ( val1 != val_orig )
        {
            SetValue( val1 );
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------
// wxLongStringProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxLongStringProperty,wxPGProperty,
                               wxString,const wxString&,TextCtrlAndButton)

wxLongStringProperty::wxLongStringProperty( const wxString& label, const wxString& name,
    const wxString& value ) : wxPGProperty(label,name)
{
    SetValue(value);
}

wxLongStringProperty::~wxLongStringProperty() {}

wxString wxLongStringProperty::ValueToString( wxVariant& value,
                                              int WXUNUSED(argFlags) ) const
{
    return value;
}

bool wxLongStringProperty::OnEvent( wxPropertyGrid* propGrid, wxWindow* WXUNUSED(primary),
                                    wxEvent& event )
{
    if ( propGrid->IsMainButtonEvent(event) )
    {
        // Update the value
        wxVariant useValue = propGrid->GetUncommittedPropertyValue();

        wxString val1 = useValue.GetString();
        wxString val_orig = val1;

        wxString value;
        if ( !(m_flags & wxPG_PROP_NO_ESCAPE) )
            wxPropertyGrid::ExpandEscapeSequences(value,val1);
        else
            value = wxString(val1);

        // Run editor dialog.
        if ( OnButtonClick(propGrid,value) )
        {
            if ( !(m_flags & wxPG_PROP_NO_ESCAPE) )
                wxPropertyGrid::CreateEscapeSequences(val1,value);
            else
                val1 = value;

            if ( val1 != val_orig )
            {
                SetValueInEvent( val1 );
                return true;
            }
        }
    }
    return false;
}

bool wxLongStringProperty::OnButtonClick( wxPropertyGrid* propGrid, wxString& value )
{
    return DisplayEditorDialog(this, propGrid, value);
}

bool wxLongStringProperty::DisplayEditorDialog( wxPGProperty* prop, wxPropertyGrid* propGrid, wxString& value )

{
    // launch editor dialog
    wxDialog* dlg = new wxDialog(propGrid,-1,prop->GetLabel(),wxDefaultPosition,wxDefaultSize,
                                 wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxCLIP_CHILDREN);

    dlg->SetFont(propGrid->GetFont()); // To allow entering chars of the same set as the propGrid

    // Multi-line text editor dialog.
#if !wxPG_SMALL_SCREEN
    const int spacing = 8;
#else
    const int spacing = 4;
#endif
    wxBoxSizer* topsizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer* rowsizer = new wxBoxSizer( wxHORIZONTAL );
    wxTextCtrl* ed = new wxTextCtrl(dlg,11,value,
        wxDefaultPosition,wxDefaultSize,wxTE_MULTILINE);

    rowsizer->Add( ed, 1, wxEXPAND|wxALL, spacing );
    topsizer->Add( rowsizer, 1, wxEXPAND, 0 );

    wxStdDialogButtonSizer* buttonSizer = new wxStdDialogButtonSizer();
    buttonSizer->AddButton(new wxButton(dlg, wxID_OK));
    buttonSizer->AddButton(new wxButton(dlg, wxID_CANCEL));
    buttonSizer->Realize();
    topsizer->Add( buttonSizer, 0,
                   wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL|wxBOTTOM|wxRIGHT,
                   spacing );

    dlg->SetSizer( topsizer );
    topsizer->SetSizeHints( dlg );

#if !wxPG_SMALL_SCREEN
    dlg->SetSize(400,300);

    dlg->Move( propGrid->GetGoodEditorDialogPosition(prop,dlg->GetSize()) );
#endif

    int res = dlg->ShowModal();

    if ( res == wxID_OK )
    {
        value = ed->GetValue();
        dlg->Destroy();
        return true;
    }
    dlg->Destroy();
    return false;
}

bool wxLongStringProperty::StringToValue( wxVariant& variant, const wxString& text, int ) const
{
    if ( variant != text )
    {
        variant = text;
        return true;
    }
    return false;
}

#if wxUSE_EDITABLELISTBOX

// -----------------------------------------------------------------------
// wxPGArrayEditorDialog
// -----------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxPGArrayEditorDialog, wxDialog)
    EVT_IDLE(wxPGArrayEditorDialog::OnIdle)
END_EVENT_TABLE()

IMPLEMENT_ABSTRACT_CLASS(wxPGArrayEditorDialog, wxDialog)

#include "wx/editlbox.h"
#include "wx/listctrl.h"

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnIdle(wxIdleEvent& event)
{
    // Repair focus - wxEditableListBox has bitmap buttons, which
    // get focus, and lose focus (into the oblivion) when they
    // become disabled due to change in control state.

    wxWindow* lastFocused = m_lastFocused;
    wxWindow* focus = ::wxWindow::FindFocus();

    // If last focused control became disabled, set focus back to
    // wxEditableListBox
    if ( lastFocused && focus != lastFocused &&
         lastFocused->GetParent() == m_elbSubPanel &&
         !lastFocused->IsEnabled() )
    {
        m_elb->GetListCtrl()->SetFocus();
    }

    m_lastFocused = focus;

    event.Skip();
}

// -----------------------------------------------------------------------

wxPGArrayEditorDialog::wxPGArrayEditorDialog()
    : wxDialog()
{
    Init();
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::Init()
{
    m_lastFocused = NULL;
    m_hasCustomNewAction = false;
    m_itemPendingAtIndex = -1;
}

// -----------------------------------------------------------------------

wxPGArrayEditorDialog::wxPGArrayEditorDialog( wxWindow *parent,
                                          const wxString& message,
                                          const wxString& caption,
                                          long style,
                                          const wxPoint& pos,
                                          const wxSize& sz )
    : wxDialog()
{
    Init();
    Create(parent,message,caption,style,pos,sz);
}

// -----------------------------------------------------------------------

bool wxPGArrayEditorDialog::Create( wxWindow *parent,
                                  const wxString& message,
                                  const wxString& caption,
                                  long style,
                                  const wxPoint& pos,
                                  const wxSize& sz )
{
    // On wxMAC the dialog shows incorrectly if style is not exactly wxCAPTION
    // FIXME: This should be only a temporary fix.
#ifdef __WXMAC__
    wxUnusedVar(style);
    int useStyle = wxCAPTION;
#else
    int useStyle = style;
#endif

    bool res = wxDialog::Create(parent, wxID_ANY, caption, pos, sz, useStyle);

    SetFont(parent->GetFont()); // To allow entering chars of the same set as the propGrid

#if !wxPG_SMALL_SCREEN
    const int spacing = 4;
#else
    const int spacing = 3;
#endif

    m_modified = false;

    wxBoxSizer* topsizer = new wxBoxSizer( wxVERTICAL );

    // Message
    if ( !message.empty() )
        topsizer->Add( new wxStaticText(this,-1,message),
            0, wxALIGN_LEFT|wxALIGN_CENTRE_VERTICAL|wxALL, spacing );

    m_elb = new wxEditableListBox(this, wxID_ANY, message,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxEL_ALLOW_NEW |
                                  wxEL_ALLOW_EDIT |
                                  wxEL_ALLOW_DELETE);

    // Populate the list box
    wxArrayString arr;
    for ( unsigned int i=0; i<ArrayGetCount(); i++ )
        arr.push_back(ArrayGet(i));
    m_elb->SetStrings(arr);

    // Connect event handlers
    wxButton* but;
    wxListCtrl* lc = m_elb->GetListCtrl();

    but = m_elb->GetNewButton();
    m_elbSubPanel = but->GetParent();
    but->Connect(but->GetId(), wxEVT_BUTTON,
        wxCommandEventHandler(wxPGArrayEditorDialog::OnAddClick),
        NULL, this);

    but = m_elb->GetDelButton();
    but->Connect(but->GetId(), wxEVT_BUTTON,
        wxCommandEventHandler(wxPGArrayEditorDialog::OnDeleteClick),
        NULL, this);

    but = m_elb->GetUpButton();
    but->Connect(but->GetId(), wxEVT_BUTTON,
        wxCommandEventHandler(wxPGArrayEditorDialog::OnUpClick),
        NULL, this);

    but = m_elb->GetDownButton();
    but->Connect(but->GetId(), wxEVT_BUTTON,
        wxCommandEventHandler(wxPGArrayEditorDialog::OnDownClick),
        NULL, this);

    lc->Connect(lc->GetId(), wxEVT_LIST_END_LABEL_EDIT,
        wxListEventHandler(wxPGArrayEditorDialog::OnEndLabelEdit),
        NULL, this);

    topsizer->Add( m_elb, 1, wxEXPAND, spacing );

    // Standard dialog buttons
    wxStdDialogButtonSizer* buttonSizer = new wxStdDialogButtonSizer();
    buttonSizer->AddButton(new wxButton(this, wxID_OK));
    buttonSizer->AddButton(new wxButton(this, wxID_CANCEL));
    buttonSizer->Realize();
    topsizer->Add( buttonSizer, 0,
                   wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL|wxALL,
                   spacing );

    m_elb->SetFocus();

    SetSizer( topsizer );
    topsizer->SetSizeHints( this );

#if !wxPG_SMALL_SCREEN
    if ( sz.x == wxDefaultSize.x &&
         sz.y == wxDefaultSize.y )
        SetSize( wxSize(275,360) );
    else
        SetSize(sz);
#endif

    return res;
}

// -----------------------------------------------------------------------

int wxPGArrayEditorDialog::GetSelection() const
{
    wxListCtrl* lc = m_elb->GetListCtrl();

    int index = lc->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if ( index == -1 )
        return wxNOT_FOUND;

    return index;
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnAddClick(wxCommandEvent& event)
{
    wxListCtrl* lc = m_elb->GetListCtrl();
    int newItemIndex = lc->GetItemCount() - 1;

    if ( m_hasCustomNewAction )
    {
        wxString str;
        if ( OnCustomNewAction(&str) )
        {
            if ( ArrayInsert(str, newItemIndex) )
            {
                lc->InsertItem(newItemIndex, str);
                m_modified = true;
            }
        }

        // Do *not* skip the event! We do not want the wxEditableListBox
        // to do anything.
    }
    else
    {
        m_itemPendingAtIndex = newItemIndex;

        event.Skip();
    }
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnDeleteClick(wxCommandEvent& event)
{
    int index = GetSelection();
    if ( index >= 0 )
    {
        ArrayRemoveAt( index );
        m_modified = true;
    }

    event.Skip();
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnUpClick(wxCommandEvent& event)
{
    int index = GetSelection();
    if ( index > 0 )
    {
        ArraySwap(index-1,index);
        m_modified = true;
    }

    event.Skip();
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnDownClick(wxCommandEvent& event)
{
    wxListCtrl* lc = m_elb->GetListCtrl();
    int index = GetSelection();
    int lastStringIndex = lc->GetItemCount() - 1;
    if ( index >= 0 && index < lastStringIndex )
    {
        ArraySwap(index, index+1);
        m_modified = true;
    }

    event.Skip();
}

// -----------------------------------------------------------------------

void wxPGArrayEditorDialog::OnEndLabelEdit(wxListEvent& event)
{
    wxString str = event.GetLabel();

    if ( m_itemPendingAtIndex >= 0 )
    {
        // Add a new item
        if ( ArrayInsert(str, m_itemPendingAtIndex) )
        {
            m_modified = true;
        }
        else
        {
            // Editable list box doesn't really respect Veto(), but
            // it recognizes if no text was added, so we simulate
            // Veto() using it.
            event.m_item.SetText(wxEmptyString);
            m_elb->GetListCtrl()->SetItemText(m_itemPendingAtIndex,
                                              wxEmptyString);

            event.Veto();
        }
    }
    else
    {
        // Change an existing item
        int index = GetSelection();
        wxASSERT( index != wxNOT_FOUND );
        if ( ArraySet(index, str) )
            m_modified = true;
        else
            event.Veto();
    }

    event.Skip();
}

#endif // wxUSE_EDITABLELISTBOX

// -----------------------------------------------------------------------
// wxPGArrayStringEditorDialog
// -----------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPGArrayStringEditorDialog, wxPGArrayEditorDialog)

BEGIN_EVENT_TABLE(wxPGArrayStringEditorDialog, wxPGArrayEditorDialog)
END_EVENT_TABLE()

// -----------------------------------------------------------------------

wxString wxPGArrayStringEditorDialog::ArrayGet( size_t index )
{
    return m_array[index];
}

size_t wxPGArrayStringEditorDialog::ArrayGetCount()
{
    return m_array.size();
}

bool wxPGArrayStringEditorDialog::ArrayInsert( const wxString& str, int index )
{
    if (index<0)
        m_array.Add(str);
    else
        m_array.Insert(str,index);
    return true;
}

bool wxPGArrayStringEditorDialog::ArraySet( size_t index, const wxString& str )
{
    m_array[index] = str;
    return true;
}

void wxPGArrayStringEditorDialog::ArrayRemoveAt( int index )
{
    m_array.RemoveAt(index);
}

void wxPGArrayStringEditorDialog::ArraySwap( size_t first, size_t second )
{
    wxString old_str = m_array[first];
    wxString new_str = m_array[second];
    m_array[first] = new_str;
    m_array[second] = old_str;
}

wxPGArrayStringEditorDialog::wxPGArrayStringEditorDialog()
    : wxPGArrayEditorDialog()
{
    Init();
}

void wxPGArrayStringEditorDialog::Init()
{
    m_pCallingClass = NULL;
}

bool
wxPGArrayStringEditorDialog::OnCustomNewAction(wxString* resString)
{
    return m_pCallingClass->OnCustomStringEdit(m_parent, *resString);
}

// -----------------------------------------------------------------------
// wxArrayStringProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS(wxArrayStringProperty,  // Property name
                               wxPGProperty,  // Property we inherit from
                               wxArrayString,  // Value type name
                               const wxArrayString&,  // Value type, as given in constructor
                               TextCtrlAndButton)  // Initial editor

wxArrayStringProperty::wxArrayStringProperty( const wxString& label,
                                                        const wxString& name,
                                                        const wxArrayString& array )
    : wxPGProperty(label,name)
{
    m_delimiter = ',';
    SetValue( array );
}

wxArrayStringProperty::~wxArrayStringProperty() { }

void wxArrayStringProperty::OnSetValue()
{
    GenerateValueAsString();
}

void
wxArrayStringProperty::ConvertArrayToString(const wxArrayString& arr,
                                            wxString* pString,
                                            const wxUniChar& delimiter) const
{
    if ( delimiter == '"' || delimiter == '\'' )
    {
        // Quoted strings
        ArrayStringToString(*pString,
                            arr,
                            delimiter,
                            Escape | QuoteStrings);
    }
    else
    {
        // Regular delimiter
        ArrayStringToString(*pString,
                            arr,
                            delimiter,
                            0);
    }
}

wxString wxArrayStringProperty::ValueToString( wxVariant& WXUNUSED(value),
                                               int argFlags ) const
{
    //
    // If this is called from GetValueAsString(), return cached string
    if ( argFlags & wxPG_VALUE_IS_CURRENT )
    {
        return m_display;
    }

    wxArrayString arr = m_value.GetArrayString();
    wxString s;
    ConvertArrayToString(arr, &s, m_delimiter);
    return s;
}

// Converts wxArrayString to a string separated by delimeters and spaces.
// preDelim is useful for "str1" "str2" style. Set flags to 1 to do slash
// conversion.
void
wxArrayStringProperty::ArrayStringToString( wxString& dst,
                                            const wxArrayString& src,
                                            wxUniChar delimiter, int flags )
{
    wxString pdr;
    wxString preas;

    unsigned int i;
    unsigned int itemCount = src.size();

    dst.Empty();

    if ( flags & Escape )
    {
        preas = delimiter;
        pdr = wxS("\\");
        pdr += delimiter;
    }

    if ( itemCount )
        dst.append( preas );

    wxString delimStr(delimiter);

    for ( i = 0; i < itemCount; i++ )
    {
        wxString str( src.Item(i) );

        // Do some character conversion.
        // Converts \ to \\ and $delimiter to \$delimiter
        // Useful when quoting.
        if ( flags & Escape )
        {
            str.Replace( wxS("\\"), wxS("\\\\"), true );
            if ( !pdr.empty() )
                str.Replace( preas, pdr, true );
        }

        dst.append( str );

        if ( i < (itemCount-1) )
        {
            dst.append( delimStr );
            dst.append( wxS(" ") );
            dst.append( preas );
        }
        else if ( flags & QuoteStrings )
            dst.append( delimStr );
    }
}

void wxArrayStringProperty::GenerateValueAsString()
{
    wxArrayString arr = m_value.GetArrayString();
    ConvertArrayToString(arr, &m_display, m_delimiter);
}

// Default implementation doesn't do anything.
bool wxArrayStringProperty::OnCustomStringEdit( wxWindow*, wxString& )
{
    return false;
}

wxPGArrayEditorDialog* wxArrayStringProperty::CreateEditorDialog()
{
    return new wxPGArrayStringEditorDialog();
}

bool wxArrayStringProperty::OnButtonClick( wxPropertyGrid* propGrid,
                                           wxWindow* WXUNUSED(primaryCtrl),
                                           const wxChar* cbt )
{
    // Update the value
    wxVariant useValue = propGrid->GetUncommittedPropertyValue();

    if ( !propGrid->EditorValidate() )
        return false;

    // Create editor dialog.
    wxPGArrayEditorDialog* dlg = CreateEditorDialog();
#if wxUSE_VALIDATORS
    wxValidator* validator = GetValidator();
    wxPGInDialogValidator dialogValidator;
#endif

    wxPGArrayStringEditorDialog* strEdDlg = wxDynamicCast(dlg, wxPGArrayStringEditorDialog);

    if ( strEdDlg )
        strEdDlg->SetCustomButton(cbt, this);

    dlg->SetDialogValue( useValue );
    dlg->Create(propGrid, wxEmptyString, m_label);

#if !wxPG_SMALL_SCREEN
    dlg->Move( propGrid->GetGoodEditorDialogPosition(this,dlg->GetSize()) );
#endif

    bool retVal;

    for (;;)
    {
        retVal = false;

        int res = dlg->ShowModal();

        if ( res == wxID_OK && dlg->IsModified() )
        {
            wxVariant value = dlg->GetDialogValue();
            if ( !value.IsNull() )
            {
                wxArrayString actualValue = value.GetArrayString();
                wxString tempStr;
                ConvertArrayToString(actualValue, &tempStr, m_delimiter);
            #if wxUSE_VALIDATORS
                if ( dialogValidator.DoValidate(propGrid, validator,
                                                tempStr) )
            #endif
                {
                    SetValueInEvent( actualValue );
                    retVal = true;
                    break;
                }
            }
            else
                break;
        }
        else
            break;
    }

    delete dlg;

    return retVal;
}

bool wxArrayStringProperty::OnEvent( wxPropertyGrid* propGrid,
                                     wxWindow* primary,
                                     wxEvent& event )
{
    if ( propGrid->IsMainButtonEvent(event) )
        return OnButtonClick(propGrid,primary,(const wxChar*) NULL);
    return false;
}

bool wxArrayStringProperty::StringToValue( wxVariant& variant,
                                           const wxString& text, int ) const
{
    wxArrayString arr;

    if ( m_delimiter == '"' || m_delimiter == '\'' )
    {
        // Quoted strings
        WX_PG_TOKENIZER2_BEGIN(text, m_delimiter)

            // Need to replace backslashes with empty characters
            // (opposite what is done in ConvertArrayToString()).
            token.Replace ( wxS("\\\\"), wxS("\\"), true );

            arr.Add( token );

        WX_PG_TOKENIZER2_END()
    }
    else
    {
        // Regular delimiter
        WX_PG_TOKENIZER1_BEGIN(text, m_delimiter)
            arr.Add( token );
        WX_PG_TOKENIZER1_END()
    }

    variant = arr;

    return true;
}

bool wxArrayStringProperty::DoSetAttribute( const wxString& name, wxVariant& value )
{
    if ( name == wxPG_ARRAY_DELIMITER )
    {
        m_delimiter = value.GetChar();
        GenerateValueAsString();
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------
// wxPGInDialogValidator
// -----------------------------------------------------------------------

#if wxUSE_VALIDATORS
bool wxPGInDialogValidator::DoValidate( wxPropertyGrid* propGrid,
                                        wxValidator* validator,
                                        const wxString& value )
{
    if ( !validator )
        return true;

    wxTextCtrl* tc = m_textCtrl;

    if ( !tc )
    {
        {
            tc = new wxTextCtrl( propGrid, wxPG_SUBID_TEMP1, wxEmptyString,
                                 wxPoint(30000,30000));
            tc->Hide();
        }

        m_textCtrl = tc;
    }

    tc->SetValue(value);

    validator->SetWindow(tc);
    bool res = validator->Validate(propGrid);

    return res;
}
#else
bool wxPGInDialogValidator::DoValidate( wxPropertyGrid* WXUNUSED(propGrid),
                                        wxValidator* WXUNUSED(validator),
                                        const wxString& WXUNUSED(value) )
{
    return true;
}
#endif

// -----------------------------------------------------------------------

#endif  // wxUSE_PROPGRID
