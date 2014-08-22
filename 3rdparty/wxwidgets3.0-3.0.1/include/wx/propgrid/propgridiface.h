/////////////////////////////////////////////////////////////////////////////
// Name:        wx/propgrid/propgridiface.h
// Purpose:     wxPropertyGridInterface class
// Author:      Jaakko Salli
// Modified by:
// Created:     2008-08-24
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_PROPGRID_PROPGRIDIFACE_H__
#define __WX_PROPGRID_PROPGRIDIFACE_H__

#include "wx/defs.h"

#if wxUSE_PROPGRID

#include "wx/propgrid/property.h"
#include "wx/propgrid/propgridpagestate.h"

// -----------------------------------------------------------------------

/** @section wxPGPropArgCls

    Most property grid functions have this type as their argument, as it can
    convey a property by either a pointer or name.
*/
class WXDLLIMPEXP_PROPGRID wxPGPropArgCls
{
public:
    wxPGPropArgCls( const wxPGProperty* property )
    {
        m_ptr.property = (wxPGProperty*) property;
        m_flags = IsProperty;
    }
    wxPGPropArgCls( const wxString& str )
    {
        m_ptr.stringName = &str;
        m_flags = IsWxString;
    }
    wxPGPropArgCls( const wxPGPropArgCls& id )
    {
        m_ptr = id.m_ptr;
        m_flags = id.m_flags;
    }
    // This is only needed for wxPython bindings
    wxPGPropArgCls( wxString* str, bool WXUNUSED(deallocPtr) )
    {
        m_ptr.stringName = str;
        m_flags = IsWxString | OwnsWxString;
    }
    ~wxPGPropArgCls()
    {
        if ( m_flags & OwnsWxString )
            delete m_ptr.stringName;
    }
    wxPGProperty* GetPtr() const
    {
        wxCHECK( m_flags == IsProperty, NULL );
        return m_ptr.property;
    }
    wxPGPropArgCls( const char* str )
    {
        m_ptr.charName = str;
        m_flags = IsCharPtr;
    }
    wxPGPropArgCls( const wchar_t* str )
    {
        m_ptr.wcharName = str;
        m_flags = IsWCharPtr;
    }
    /** This constructor is required for NULL. */
    wxPGPropArgCls( int )
    {
        m_ptr.property = NULL;
        m_flags = IsProperty;
    }
    wxPGProperty* GetPtr( wxPropertyGridInterface* iface ) const;
    wxPGProperty* GetPtr( const wxPropertyGridInterface* iface ) const
    {
        return GetPtr((wxPropertyGridInterface*)iface);
    }
    wxPGProperty* GetPtr0() const { return m_ptr.property; }
    bool HasName() const { return (m_flags != IsProperty); }
    const wxString& GetName() const { return *m_ptr.stringName; }
private:

    enum
    {
        IsProperty      = 0x00,
        IsWxString      = 0x01,
        IsCharPtr       = 0x02,
        IsWCharPtr      = 0x04,
        OwnsWxString    = 0x10
    };

    union
    {
        wxPGProperty* property;
        const char* charName;
        const wchar_t* wcharName;
        const wxString* stringName;
    } m_ptr;
    unsigned char m_flags;
};

typedef const wxPGPropArgCls& wxPGPropArg;

// -----------------------------------------------------------------------

WXDLLIMPEXP_PROPGRID
void wxPGTypeOperationFailed( const wxPGProperty* p,
                              const wxString& typestr,
                              const wxString& op );
WXDLLIMPEXP_PROPGRID
void wxPGGetFailed( const wxPGProperty* p, const wxString& typestr );

// -----------------------------------------------------------------------

// Helper macro that does necessary preparations when calling
// some wxPGProperty's member function.
#define wxPG_PROP_ARG_CALL_PROLOG_0(PROPERTY) \
    PROPERTY *p = (PROPERTY*)id.GetPtr(this); \
    if ( !p ) return;

#define wxPG_PROP_ARG_CALL_PROLOG_RETVAL_0(PROPERTY, RETVAL) \
    PROPERTY *p = (PROPERTY*)id.GetPtr(this); \
    if ( !p ) return RETVAL;

#define wxPG_PROP_ARG_CALL_PROLOG() \
    wxPG_PROP_ARG_CALL_PROLOG_0(wxPGProperty)

#define wxPG_PROP_ARG_CALL_PROLOG_RETVAL(RVAL) \
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL_0(wxPGProperty, RVAL)

#define wxPG_PROP_ID_CONST_CALL_PROLOG() \
    wxPG_PROP_ARG_CALL_PROLOG_0(const wxPGProperty)

#define wxPG_PROP_ID_CONST_CALL_PROLOG_RETVAL(RVAL) \
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL_0(const wxPGProperty, RVAL)

// -----------------------------------------------------------------------


/** @class wxPropertyGridInterface

    Most of the shared property manipulation interface shared by wxPropertyGrid,
    wxPropertyGridPage, and wxPropertyGridManager is defined in this class.

    @remarks
    - In separate wxPropertyGrid component this class was known as
    wxPropertyContainerMethods.

    @library{wxpropgrid}
    @category{propgrid}
*/
class WXDLLIMPEXP_PROPGRID wxPropertyGridInterface
{
public:

    /** Destructor */
    virtual ~wxPropertyGridInterface() { }

    /**
        Appends property to the list.

        wxPropertyGrid assumes ownership of the object.
        Becomes child of most recently added category.
        @remarks
        - wxPropertyGrid takes the ownership of the property pointer.
        - If appending a category with name identical to a category already in
          the wxPropertyGrid, then newly created category is deleted, and most
          recently added category (under which properties are appended) is set
          to the one with same name. This allows easier adding of items to same
          categories in multiple passes.
        - Does not automatically redraw the control, so you may need to call
          Refresh when calling this function after control has been shown for
          the first time.
    */
    wxPGProperty* Append( wxPGProperty* property );

    wxPGProperty* AppendIn( wxPGPropArg id, wxPGProperty* newproperty );

    /**
        In order to add new items into a property with fixed children (for
        instance, wxFlagsProperty), you need to call this method. After
        populating has been finished, you need to call EndAddChildren.
    */
    void BeginAddChildren( wxPGPropArg id );

    /** Deletes all properties.
    */
    virtual void Clear() = 0;

    /**
        Clears current selection, if any.

        @param validation
            If set to @false, deselecting the property will always work,
            even if its editor had invalid value in it.

        @return Returns @true if successful or if there was no selection. May
                fail if validation was enabled and active editor had invalid
                value.

        @remarks In wxPropertyGrid 1.4, this member function used to send
                 wxPG_EVT_SELECTED. In wxWidgets 2.9 and later, it no longer
                 does that.
    */
    bool ClearSelection( bool validation = false );

    /** Resets modified status of all properties.
    */
    void ClearModifiedStatus();

    /** Collapses given category or property with children.
        Returns true if actually collapses.
    */
    bool Collapse( wxPGPropArg id );

    /** Collapses all items that can be collapsed.

        @return
        Return false if failed (may fail if editor value cannot be validated).
    */
    bool CollapseAll() { return ExpandAll(false); }

    /**
        Changes value of a property, as if from an editor.
        Use this instead of SetPropertyValue() if you need the value to run
        through validation process, and also send the property change event.

        @return
        Returns true if value was successfully changed.
    */
    bool ChangePropertyValue( wxPGPropArg id, wxVariant newValue );

    /**
        Removes and deletes a property and any children.

        @param id
            Pointer or name of a property.

        @remarks If you delete a property in a wxPropertyGrid event
                 handler, the actual deletion is postponed until the next
                 idle event.

                 This functions deselects selected property, if any.
                 Validation failure option wxPG_VFB_STAY_IN_PROPERTY is not
                 respected, ie. selection is cleared even if editor had
                 invalid value.
    */
    void DeleteProperty( wxPGPropArg id );

    /**
        Removes a property. Does not delete the property object, but
        instead returns it.

        @param id
            Pointer or name of a property.

        @remarks Removed property cannot have any children.

                 Also, if you remove property in a wxPropertyGrid event
                 handler, the actual removal is postponed until the next
                 idle event.
    */
    wxPGProperty* RemoveProperty( wxPGPropArg id );

    /**
        Disables a property.

        @see EnableProperty(), wxPGProperty::Enable()
    */
    bool DisableProperty( wxPGPropArg id ) { return EnableProperty(id,false); }

    /**
        Returns true if all property grid data changes have been committed.

        Usually only returns false if value in active editor has been
        invalidated by a wxValidator.
    */
    bool EditorValidate();

    /**
        Enables or disables property, depending on whether enable is true or
        false. Disabled property usually appears as having grey text.

        @param id
            Name or pointer to a property.
        @param enable
            If @false, property is disabled instead.

        @see wxPGProperty::Enable()
     */
    bool EnableProperty( wxPGPropArg id, bool enable = true );

    /** Called after population of property with fixed children has finished.
    */
    void EndAddChildren( wxPGPropArg id );

    /** Expands given category or property with children.
        Returns true if actually expands.
    */
    bool Expand( wxPGPropArg id );

    /** Expands all items that can be expanded.
    */
    bool ExpandAll( bool expand = true );

    /** Returns id of first child of given property.
        @remarks
        Does not return sub-properties!
    */
    wxPGProperty* GetFirstChild( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)

        if ( !p->GetChildCount() || p->HasFlag(wxPG_PROP_AGGREGATE) )
            return wxNullProperty;

        return p->Item(0);
    }

    //@{
    /** Returns iterator class instance.
        @param flags
        See @ref propgrid_iterator_flags. Value wxPG_ITERATE_DEFAULT causes
        iteration over everything except private child properties.
        @param firstProp
            Property to start iteration from. If NULL, then first child of root
            is used.
        @param startPos
            Either wxTOP or wxBOTTOM. wxTOP will indicate that iterations start
            from the first property from the top, and wxBOTTOM means that the
            iteration will instead begin from bottommost valid item.
    */
    wxPropertyGridIterator GetIterator( int flags = wxPG_ITERATE_DEFAULT,
                                        wxPGProperty* firstProp = NULL )
    {
        return wxPropertyGridIterator( m_pState, flags, firstProp );
    }

    wxPropertyGridConstIterator
    GetIterator( int flags = wxPG_ITERATE_DEFAULT,
                 wxPGProperty* firstProp = NULL ) const
    {
        return wxPropertyGridConstIterator( m_pState, flags, firstProp );
    }

    wxPropertyGridIterator GetIterator( int flags, int startPos )
    {
        return wxPropertyGridIterator( m_pState, flags, startPos );
    }

    wxPropertyGridConstIterator GetIterator( int flags, int startPos ) const
    {
        return wxPropertyGridConstIterator( m_pState, flags, startPos );
    }
    //@}

    /** Returns id of first item, whether it is a category or property.
        @param flags
        @link iteratorflags List of iterator flags@endlink
    */
    wxPGProperty* GetFirst( int flags = wxPG_ITERATE_ALL )
    {
        wxPropertyGridIterator it( m_pState, flags, wxNullProperty, 1 );
        return *it;
    }

    const wxPGProperty* GetFirst( int flags = wxPG_ITERATE_ALL ) const
    {
        return ((wxPropertyGridInterface*)this)->GetFirst(flags);
    }

    /**
        Returns pointer to a property with given name (case-sensitive).
        If there is no property with such name, @NULL pointer is returned.

        @remarks Properties which have non-category, non-root parent
                 cannot be accessed globally by their name. Instead, use
                 "<property>.<subproperty>" instead of "<subproperty>".
    */
    wxPGProperty* GetProperty( const wxString& name ) const
    {
        return GetPropertyByName(name);
    }

    /** Returns map-like storage of property's attributes.
        @remarks
        Note that if extra style wxPG_EX_WRITEONLY_BUILTIN_ATTRIBUTES is set,
        then builtin-attributes are not included in the storage.
    */
    const wxPGAttributeStorage& GetPropertyAttributes( wxPGPropArg id ) const
    {
        // If 'id' refers to invalid property, then we will return dummy
        // attributes (ie. root property's attributes, which contents should
        // should always be empty and of no consequence).
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(m_pState->DoGetRoot()->GetAttributes());
        return p->GetAttributes();
    }

    /** Adds to 'targetArr' pointers to properties that have given
        flags 'flags' set. However, if 'inverse' is set to true, then
        only properties without given flags are stored.
        @param flags
        Property flags to use.
        @param iterFlags
        Iterator flags to use. Default is everything expect private children.
    */
    void GetPropertiesWithFlag( wxArrayPGProperty* targetArr,
                                wxPGProperty::FlagType flags,
                                bool inverse = false,
                                int iterFlags = wxPG_ITERATE_PROPERTIES |
                                                wxPG_ITERATE_HIDDEN |
                                                wxPG_ITERATE_CATEGORIES) const;

    /** Returns value of given attribute. If none found, returns NULL-variant.
    */
    wxVariant GetPropertyAttribute( wxPGPropArg id,
                                    const wxString& attrName ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullVariant)
        return p->GetAttribute(attrName);
    }

    /** Returns pointer of property's nearest parent category. If no category
        found, returns NULL.
    */
    wxPropertyCategory* GetPropertyCategory( wxPGPropArg id ) const
    {
        wxPG_PROP_ID_CONST_CALL_PROLOG_RETVAL(NULL)
        return m_pState->GetPropertyCategory(p);
    }

    /** Returns client data (void*) of a property. */
    void* GetPropertyClientData( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(NULL)
        return p->GetClientData();
    }

    /**
        Returns first property which label matches given string.

        NULL if none found. Note that this operation is extremely slow when
        compared to GetPropertyByName().
    */
    wxPGProperty* GetPropertyByLabel( const wxString& label ) const;

    /** Returns property with given name. NULL if none found.
    */
    wxPGProperty* GetPropertyByName( const wxString& name ) const;

    /** Returns child property 'subname' of property 'name'. Same as
        calling GetPropertyByName("name.subname"), albeit slightly faster.
    */
    wxPGProperty* GetPropertyByName( const wxString& name,
                                     const wxString& subname ) const;

    /** Returns property's editor. */
    const wxPGEditor* GetPropertyEditor( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(NULL)
        return p->GetEditorClass();
    }

    /** Returns help string associated with a property. */
    wxString GetPropertyHelpString( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(m_emptyString)
        return p->GetHelpString();
    }

    /** Returns property's custom value image (NULL of none). */
    wxBitmap* GetPropertyImage( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(NULL)
        return p->GetValueImage();
    }

    /** Returns label of a property. */
    const wxString& GetPropertyLabel( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(m_emptyString)
        return p->GetLabel();
    }

    /** Returns name of a property, by which it is globally accessible. */
    wxString GetPropertyName( wxPGProperty* property )
    {
        return property->GetName();
    }

    /** Returns parent item of a property. */
    wxPGProperty* GetPropertyParent( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)
        return p->GetParent();
    }

#if wxUSE_VALIDATORS
    /** Returns validator of a property as a reference, which you
        can pass to any number of SetPropertyValidator.
    */
    wxValidator* GetPropertyValidator( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(NULL)
        return p->GetValidator();
    }
#endif

    /** Returns value as wxVariant. To get wxObject pointer from it,
        you will have to use WX_PG_VARIANT_TO_WXOBJECT(VARIANT,CLASSNAME) macro.

        If property value is unspecified, Null variant is returned.
    */
    wxVariant GetPropertyValue( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxVariant())
        return p->GetValue();
    }

    wxString GetPropertyValueAsString( wxPGPropArg id ) const;
    long GetPropertyValueAsLong( wxPGPropArg id ) const;
    unsigned long GetPropertyValueAsULong( wxPGPropArg id ) const
    {
        return (unsigned long) GetPropertyValueAsLong(id);
    }
    int GetPropertyValueAsInt( wxPGPropArg id ) const
        { return (int)GetPropertyValueAsLong(id); }
    bool GetPropertyValueAsBool( wxPGPropArg id ) const;
    double GetPropertyValueAsDouble( wxPGPropArg id ) const;

#define wxPG_PROP_ID_GETPROPVAL_CALL_PROLOG_RETVAL(TYPENAME, DEFVAL) \
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(DEFVAL) \
    wxString typeName(wxS(TYPENAME)); \
    wxVariant value = p->GetValue(); \
    if ( value.GetType() != typeName ) \
    { \
        wxPGGetFailed(p, typeName); \
        return DEFVAL; \
    }

#define wxPG_PROP_ID_GETPROPVAL_CALL_PROLOG_RETVAL_WFALLBACK(TYPENAME, DEFVAL) \
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(DEFVAL) \
    wxVariant value = p->GetValue(); \
    if ( value.GetType() != wxS(TYPENAME) ) \
        return DEFVAL; \

    wxArrayString GetPropertyValueAsArrayString( wxPGPropArg id ) const
    {
        wxPG_PROP_ID_GETPROPVAL_CALL_PROLOG_RETVAL("arrstring",
                                                   wxArrayString())
        return value.GetArrayString();
    }

#ifdef wxLongLong_t
    wxLongLong_t GetPropertyValueAsLongLong( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(0)
        return p->GetValue().GetLongLong().GetValue();
    }

    wxULongLong_t GetPropertyValueAsULongLong( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(0)
        return p->GetValue().GetULongLong().GetValue();
    }
#endif

    wxArrayInt GetPropertyValueAsArrayInt( wxPGPropArg id ) const
    {
        wxPG_PROP_ID_GETPROPVAL_CALL_PROLOG_RETVAL("wxArrayInt",
                                                   wxArrayInt())
        wxArrayInt arr;
        arr << value;
        return arr;
    }

#if wxUSE_DATETIME
    wxDateTime GetPropertyValueAsDateTime( wxPGPropArg id ) const
    {
        wxPG_PROP_ID_GETPROPVAL_CALL_PROLOG_RETVAL("datetime",
                                                   wxDateTime())
        return value.GetDateTime();
    }
#endif

    /** Returns a wxVariant list containing wxVariant versions of all
        property values. Order is not guaranteed.
        @param flags
        Use wxPG_KEEP_STRUCTURE to retain category structure; each sub
        category will be its own wxVariantList of wxVariant.
        Use wxPG_INC_ATTRIBUTES to include property attributes as well.
        Each attribute will be stored as list variant named
        "@@<propname>@@attr."
        @remarks
    */
    wxVariant GetPropertyValues( const wxString& listname = wxEmptyString,
        wxPGProperty* baseparent = NULL, long flags = 0 ) const
    {
        return m_pState->DoGetPropertyValues(listname, baseparent, flags);
    }

    /**
        Returns currently selected property. NULL if none.

        @remarks When wxPG_EX_MULTIPLE_SELECTION extra style is used, this
                 member function returns the focused property, that is the
                 one which can have active editor.
    */
    wxPGProperty* GetSelection() const;

    /**
        Returns list of currently selected properties.

        @remarks wxArrayPGProperty should be compatible with std::vector API.
    */
    const wxArrayPGProperty& GetSelectedProperties() const
    {
        return m_pState->m_selection;
    }

    wxPropertyGridPageState* GetState() const { return m_pState; }

    /** Similar to GetIterator(), but instead returns wxPGVIterator instance,
        which can be useful for forward-iterating through arbitrary property
        containers.

        @param flags
        See @ref propgrid_iterator_flags.
    */
    virtual wxPGVIterator GetVIterator( int flags ) const;

    /** Hides or reveals a property.
        @param hide
            If true, hides property, otherwise reveals it.
        @param flags
            By default changes are applied recursively. Set this paramter
            wxPG_DONT_RECURSE to prevent this.
    */
    bool HideProperty( wxPGPropArg id,
                       bool hide = true,
                       int flags = wxPG_RECURSE );

#if wxPG_INCLUDE_ADVPROPS
    /** Initializes *all* property types. Causes references to most object
        files in the library, so calling this may cause significant increase
        in executable size when linking with static library.
    */
    static void InitAllTypeHandlers();
#else
    static void InitAllTypeHandlers() { }
#endif

    //@{
    /** Inserts property to the property container.

        @param priorThis
        New property is inserted just prior to this. Available only
        in the first variant. There are two versions of this function
        to allow this parameter to be either an id or name to
        a property.

        @param newproperty
        Pointer to the inserted property. wxPropertyGrid will take
        ownership of this object.

        @param parent
        New property is inserted under this category. Available only
        in the second variant. There are two versions of this function
        to allow this parameter to be either an id or name to
        a property.

        @param index
        Index under category. Available only in the second variant.
        If index is < 0, property is appended in category.

        @return
        Returns id for the property,

        @remarks

        - wxPropertyGrid takes the ownership of the property pointer.

        - While Append may be faster way to add items, make note that when
          both types of data storage (categoric and
          non-categoric) are active, Insert becomes even more slow. This is
          especially true if current mode is non-categoric.

        Example of use:

        @code

            // append category
            wxPGProperty* my_cat_id = propertygrid->Append(
                new wxPropertyCategory("My Category") );

            ...

            // insert into category - using second variant
            wxPGProperty* my_item_id_1 = propertygrid->Insert(
                my_cat_id, 0, new wxStringProperty("My String 1") );

            // insert before to first item - using first variant
            wxPGProperty* my_item_id_2 = propertygrid->Insert(
                my_item_id, new wxStringProperty("My String 2") );

        @endcode

    */
    wxPGProperty* Insert( wxPGPropArg priorThis, wxPGProperty* newproperty );
    wxPGProperty* Insert( wxPGPropArg parent,
                          int index,
                          wxPGProperty* newproperty );
    //@}

    /** Returns true if property is a category. */
    bool IsPropertyCategory( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return p->IsCategory();
    }

    /** Returns true if property is enabled. */
    bool IsPropertyEnabled( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return (!(p->GetFlags() & wxPG_PROP_DISABLED))?true:false;
    }

    /**
        Returns true if given property is expanded.

        Naturally, always returns false for properties that cannot be expanded.
    */
    bool IsPropertyExpanded( wxPGPropArg id ) const;

    /**
        Returns true if property has been modified after value set or modify
        flag clear by software.
    */
    bool IsPropertyModified( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return ( (p->GetFlags() & wxPG_PROP_MODIFIED) ? true : false );
    }

    /**
        Returns true if property is selected.
    */
    bool IsPropertySelected( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return m_pState->DoIsPropertySelected(p);
    }

    /**
        Returns true if property is shown (ie hideproperty with true not
        called for it).
    */
    bool IsPropertyShown( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return (!(p->GetFlags() & wxPG_PROP_HIDDEN))?true:false;
    }

    /** Returns true if property value is set to unspecified.
    */
    bool IsPropertyValueUnspecified( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return p->IsValueUnspecified();
    }

    /**
        Disables (limit = true) or enables (limit = false) wxTextCtrl editor of
        a property, if it is not the sole mean to edit the value.
    */
    void LimitPropertyEditing( wxPGPropArg id, bool limit = true );

    /** If state is shown in it's grid, refresh it now.
    */
    virtual void RefreshGrid( wxPropertyGridPageState* state = NULL );

#if wxPG_INCLUDE_ADVPROPS
    /**
        Initializes additional property editors (SpinCtrl etc.). Causes
        references to most object files in the library, so calling this may
        cause significant increase in executable size when linking with static
        library.
    */
    static void RegisterAdditionalEditors();
#else
    static void RegisterAdditionalEditors() { }
#endif

    /** Replaces property with id with newly created property. For example,
        this code replaces existing property named "Flags" with one that
        will have different set of items:
        @code
            pg->ReplaceProperty("Flags",
                wxFlagsProperty("Flags", wxPG_LABEL, newItems))
        @endcode
        For more info, see wxPropertyGrid::Insert.
    */
    wxPGProperty* ReplaceProperty( wxPGPropArg id, wxPGProperty* property );

    /** @anchor propgridinterface_editablestate_flags

        Flags for wxPropertyGridInterface::SaveEditableState() and
        wxPropertyGridInterface::RestoreEditableState().
    */
    enum EditableStateFlags
    {
        /** Include selected property. */
        SelectionState   = 0x01,
        /** Include expanded/collapsed property information. */
        ExpandedState    = 0x02,
        /** Include scrolled position. */
        ScrollPosState   = 0x04,
        /** Include selected page information.
            Only applies to wxPropertyGridManager. */
        PageState        = 0x08,
        /** Include splitter position. Stored for each page. */
        SplitterPosState = 0x10,
        /** Include description box size.
            Only applies to wxPropertyGridManager. */
        DescBoxState     = 0x20,

        /**
            Include all supported user editable state information.
            This is usually the default value. */
        AllStates        = SelectionState |
                           ExpandedState |
                           ScrollPosState |
                           PageState |
                           SplitterPosState |
                           DescBoxState
    };

    /**
        Restores user-editable state.

        See also wxPropertyGridInterface::SaveEditableState().

        @param src
            String generated by SaveEditableState.

        @param restoreStates
            Which parts to restore from source string. See @ref
            propgridinterface_editablestate_flags "list of editable state
            flags".

        @return
            False if there was problem reading the string.

        @remarks
        If some parts of state (such as scrolled or splitter position) fail to
        restore correctly, please make sure that you call this function after
        wxPropertyGrid size has been set (this may sometimes be tricky when
        sizers are used).
    */
    bool RestoreEditableState( const wxString& src,
                               int restoreStates = AllStates );

    /**
        Used to acquire user-editable state (selected property, expanded
        properties, scrolled position, splitter positions).

        @param includedStates
        Which parts of state to include. See @ref
        propgridinterface_editablestate_flags "list of editable state flags".
    */
    wxString SaveEditableState( int includedStates = AllStates ) const;

    /**
        Lets user set the strings listed in the choice dropdown of a
        wxBoolProperty. Defaults are "True" and "False", so changing them to,
        say, "Yes" and "No" may be useful in some less technical applications.
    */
    static void SetBoolChoices( const wxString& trueChoice,
                                const wxString& falseChoice );

    /**
        Set proportion of a auto-stretchable column. wxPG_SPLITTER_AUTO_CENTER
        window style needs to be used to indicate that columns are auto-
        resizable.

        @returns Returns @false on failure.

        @remarks You should call this for individual pages of
                 wxPropertyGridManager (if used).

        @see GetColumnProportion()
    */
    bool SetColumnProportion( unsigned int column, int proportion );

    /**
        Returns auto-resize proportion of the given column.

        @see SetColumnProportion()
    */
    int GetColumnProportion( unsigned int column ) const
    {
        return m_pState->DoGetColumnProportion(column);
    }

    /** Sets an attribute for this property.
        @param name
            Text identifier of attribute. See @ref propgrid_property_attributes.
        @param value
            Value of attribute.
        @param argFlags
            Optional. Use wxPG_RECURSE to set the attribute to child properties
            recursively.
    */
    void SetPropertyAttribute( wxPGPropArg id,
                               const wxString& attrName,
                               wxVariant value,
                               long argFlags = 0 )
    {
        DoSetPropertyAttribute(id,attrName,value,argFlags);
    }

    /** Sets property attribute for all applicapple properties.
        Be sure to use this method only after all properties have been
        added to the grid.
    */
    void SetPropertyAttributeAll( const wxString& attrName, wxVariant value );

    /**
        Sets background colour of a property.

        @param id
            Property name or pointer.

        @param colour
            New background colour.

        @param flags
            Default is wxPG_RECURSE which causes colour to be set recursively.
            Omit this flag to only set colour for the property in question
            and not any of its children.
    */
    void SetPropertyBackgroundColour( wxPGPropArg id,
                                      const wxColour& colour,
                                      int flags = wxPG_RECURSE );

    /** Resets text and background colours of given property.
    */
    void SetPropertyColoursToDefault( wxPGPropArg id );

    /**
        Sets text colour of a property.

        @param id
            Property name or pointer.

        @param colour
            New background colour.

        @param flags
            Default is wxPG_RECURSE which causes colour to be set recursively.
            Omit this flag to only set colour for the property in question
            and not any of its children.
    */
    void SetPropertyTextColour( wxPGPropArg id,
                                const wxColour& col,
                                int flags = wxPG_RECURSE );

    /**
        Returns background colour of first cell of a property.
    */
    wxColour GetPropertyBackgroundColour( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxColour())
        return p->GetCell(0).GetBgCol();
    }

    /**
        Returns text colour of first cell of a property.
    */
    wxColour GetPropertyTextColour( wxPGPropArg id ) const
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxColour())
        return p->GetCell(0).GetFgCol();
    }

    /** Sets text, bitmap, and colours for given column's cell.

        @remarks
        - You can set label cell by setting column to 0.
        - You can use wxPG_LABEL as text to use default text for column.
    */
    void SetPropertyCell( wxPGPropArg id,
                          int column,
                          const wxString& text = wxEmptyString,
                          const wxBitmap& bitmap = wxNullBitmap,
                          const wxColour& fgCol = wxNullColour,
                          const wxColour& bgCol = wxNullColour );

    /** Sets client data (void*) of a property.
        @remarks
        This untyped client data has to be deleted manually.
    */
    void SetPropertyClientData( wxPGPropArg id, void* clientData )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        p->SetClientData(clientData);
    }

    /** Sets editor for a property.

        @param editor
        For builtin editors, use wxPGEditor_X, where X is builtin editor's
        name (TextCtrl, Choice, etc. see wxPGEditor documentation for full
        list).

        For custom editors, use pointer you received from
        wxPropertyGrid::RegisterEditorClass().
    */
    void SetPropertyEditor( wxPGPropArg id, const wxPGEditor* editor )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        wxCHECK_RET( editor, wxT("unknown/NULL editor") );
        p->SetEditor(editor);
        RefreshProperty(p);
    }

    /** Sets editor control of a property. As editor argument, use
        editor name string, such as "TextCtrl" or "Choice".
    */
    void SetPropertyEditor( wxPGPropArg id, const wxString& editorName )
    {
        SetPropertyEditor(id,GetEditorByName(editorName));
    }

    /** Sets label of a property.
    */
    void SetPropertyLabel( wxPGPropArg id, const wxString& newproplabel );

    /**
        Sets name of a property.

        @param id
            Name or pointer of property which name to change.

        @param newName
            New name for property.
    */
    void SetPropertyName( wxPGPropArg id, const wxString& newName )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        m_pState->DoSetPropertyName( p, newName );
    }

    /**
        Sets property (and, recursively, its children) to have read-only value.
        In other words, user cannot change the value in the editor, but they
        can still copy it.
        @remarks
        This is mainly for use with textctrl editor. Not all other editors fully
        support it.
        @param flags
        By default changes are applied recursively. Set this paramter
        wxPG_DONT_RECURSE to prevent this.
    */
    void SetPropertyReadOnly( wxPGPropArg id,
                              bool set = true,
                              int flags = wxPG_RECURSE )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        if ( flags & wxPG_RECURSE )
            p->SetFlagRecursively(wxPG_PROP_READONLY, set);
        else
            p->ChangeFlag(wxPG_PROP_READONLY, set);
    }

    /** Sets property's value to unspecified.
        If it has children (it may be category), then the same thing is done to
        them.
    */
    void SetPropertyValueUnspecified( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        p->SetValueToUnspecified();
    }

    /**
        Sets property values from a list of wxVariants.
    */
    void SetPropertyValues( const wxVariantList& list,
                            wxPGPropArg defaultCategory = wxNullProperty )
    {
        wxPGProperty *p;
        if ( defaultCategory.HasName() ) p = defaultCategory.GetPtr(this);
        else p = defaultCategory.GetPtr0();
        m_pState->DoSetPropertyValues(list, p);
    }

    /**
        Sets property values from a list of wxVariants.
    */
    void SetPropertyValues( const wxVariant& list,
                            wxPGPropArg defaultCategory = wxNullProperty )
    {
        SetPropertyValues(list.GetList(),defaultCategory);
    }

    /** Associates the help string with property.
        @remarks
        By default, text is shown either in the manager's "description"
        text box or in the status bar. If extra window style
        wxPG_EX_HELP_AS_TOOLTIPS is used, then the text will appear as a
        tooltip.
    */
    void SetPropertyHelpString( wxPGPropArg id, const wxString& helpString )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        p->SetHelpString(helpString);
    }

    /** Set wxBitmap in front of the value.
        @remarks
        - Bitmap will be scaled to a size returned by
        wxPropertyGrid::GetImageSize();
    */
    void SetPropertyImage( wxPGPropArg id, wxBitmap& bmp )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        p->SetValueImage(bmp);
        RefreshProperty(p);
    }

    /** Sets max length of property's text.
    */
    bool SetPropertyMaxLength( wxPGPropArg id, int maxLen );

#if wxUSE_VALIDATORS
    /** Sets validator of a property.
    */
    void SetPropertyValidator( wxPGPropArg id, const wxValidator& validator )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        p->SetValidator(validator);
    }
#endif

    /** Sets value (long integer) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, long value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }

    /** Sets value (integer) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, int value )
    {
        wxVariant v((long)value);
        SetPropVal( id, v );
    }
    /** Sets value (floating point) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, double value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }
    /** Sets value (bool) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, bool value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }
    void SetPropertyValue( wxPGPropArg id, const wchar_t* value )
    {
        SetPropertyValueString( id, wxString(value) );
    }
    void SetPropertyValue( wxPGPropArg id, const char* value )
    {
        SetPropertyValueString( id, wxString(value) );
    }
    void SetPropertyValue( wxPGPropArg id, const wxString& value )
    {
        SetPropertyValueString( id, value );
    }

    /** Sets value (wxArrayString) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, const wxArrayString& value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }

#if wxUSE_DATETIME
    void SetPropertyValue( wxPGPropArg id, const wxDateTime& value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }
#endif

    /** Sets value (wxObject*) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, wxObject* value )
    {
        wxVariant v(value);
        SetPropVal( id, v );
    }

    void SetPropertyValue( wxPGPropArg id, wxObject& value )
    {
        wxVariant v(&value);
        SetPropVal( id, v );
    }

#ifdef wxLongLong_t
    /** Sets value (wxLongLong&) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, wxLongLong_t value )
    {
        wxVariant v = WXVARIANT(wxLongLong(value));
        SetPropVal( id, v );
    }
    /** Sets value (wxULongLong&) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, wxULongLong_t value )
    {
        wxVariant v = WXVARIANT(wxULongLong(value));
        SetPropVal( id, v );
    }
#endif

    /** Sets value (wxArrayInt&) of a property.
    */
    void SetPropertyValue( wxPGPropArg id, const wxArrayInt& value )
    {
        wxVariant v = WXVARIANT(value);
        SetPropVal( id, v );
    }

    /** Sets value (wxString) of a property.

        @remarks
        This method uses wxPGProperty::SetValueFromString, which all properties
        should implement. This means that there should not be a type error,
        and instead the string is converted to property's actual value type.
    */
    void SetPropertyValueString( wxPGPropArg id, const wxString& value );

    /** Sets value (wxVariant&) of a property.

        @remarks
        Use wxPropertyGrid::ChangePropertyValue() instead if you need to run
        through validation process and send property change event.
    */
    void SetPropertyValue( wxPGPropArg id, wxVariant value )
    {
        SetPropVal( id, value );
    }

    /** Sets value (wxVariant&) of a property. Same as SetPropertyValue, but
        accepts reference. */
    void SetPropVal( wxPGPropArg id, wxVariant& value );

    /** Adjusts how wxPropertyGrid behaves when invalid value is entered
        in a property.
        @param vfbFlags
        See @link vfbflags list of valid flags values@endlink
    */
    void SetValidationFailureBehavior( int vfbFlags );

    /**
        Sorts all properties recursively.

        @param flags
            This can contain any of the following options:
              wxPG_SORT_TOP_LEVEL_ONLY: Only sort categories and their
                immediate children. Sorting done by wxPG_AUTO_SORT option
                uses this.

        @see SortChildren, wxPropertyGrid::SetSortFunction
    */
    void Sort( int flags = 0 );

    /**
        Sorts children of a property.

        @param id
            Name or pointer to a property.

        @param flags
            This can contain any of the following options:
              wxPG_RECURSE: Sorts recursively.

        @see Sort, wxPropertyGrid::SetSortFunction
    */
    void SortChildren( wxPGPropArg id, int flags = 0 )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        m_pState->DoSortChildren(p, flags);
    }

    // GetPropertyByName With nice assertion error message.
    wxPGProperty* GetPropertyByNameA( const wxString& name ) const;

    static wxPGEditor* GetEditorByName( const wxString& editorName );

    // NOTE: This function reselects the property and may cause
    //       excess flicker, so to just call Refresh() on a rect
    //       of single property, call DrawItem() instead.
    virtual void RefreshProperty( wxPGProperty* p ) = 0;

protected:

    bool DoClearSelection( bool validation = false,
                           int selFlags = 0 );

    /**
        In derived class, implement to set editable state component with
        given name to given value.
    */
    virtual bool SetEditableStateItem( const wxString& name, wxVariant value )
    {
        wxUnusedVar(name);
        wxUnusedVar(value);
        return false;
    }

    /**
        In derived class, implement to return editable state component with
        given name.
    */
    virtual wxVariant GetEditableStateItem( const wxString& name ) const
    {
        wxUnusedVar(name);
        return wxNullVariant;
    }

    // Returns page state data for given (sub) page (-1 means current page).
    virtual wxPropertyGridPageState* GetPageState( int pageIndex ) const
    {
        if ( pageIndex <= 0 )
            return m_pState;
        return NULL;
    }

    virtual bool DoSelectPage( int WXUNUSED(index) ) { return true; }

    // Default call's m_pState's BaseGetPropertyByName
    virtual wxPGProperty* DoGetPropertyByName( const wxString& name ) const;

    // Deriving classes must set this (it must be only or current page).
    wxPropertyGridPageState*         m_pState;

    // Intermediate version needed due to wxVariant copying inefficiency
    void DoSetPropertyAttribute( wxPGPropArg id,
                                 const wxString& name,
                                 wxVariant& value, long argFlags );

    // Empty string object to return from member functions returning const
    // wxString&.
    wxString                    m_emptyString;

private:
    // Cannot be GetGrid() due to ambiguity issues.
    wxPropertyGrid* GetPropertyGrid()
    {
        if ( !m_pState )
            return NULL;
        return m_pState->GetGrid();
    }

    // Cannot be GetGrid() due to ambiguity issues.
    const wxPropertyGrid* GetPropertyGrid() const
    {
        if ( !m_pState )
            return NULL;

        return m_pState->GetGrid();
    }

    friend class wxPropertyGrid;
    friend class wxPropertyGridManager;
};

#endif // wxUSE_PROPGRID

#endif // __WX_PROPGRID_PROPGRIDIFACE_H__
