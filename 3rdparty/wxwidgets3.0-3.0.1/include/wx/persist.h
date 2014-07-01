///////////////////////////////////////////////////////////////////////////////
// Name:        wx/persist.h
// Purpose:     common classes for persistence support
// Author:      Vadim Zeitlin
// Created:     2009-01-18
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PERSIST_H_
#define _WX_PERSIST_H_

#include "wx/string.h"
#include "wx/hashmap.h"
#include "wx/confbase.h"

class wxPersistentObject;

WX_DECLARE_VOIDPTR_HASH_MAP(wxPersistentObject *, wxPersistentObjectsMap);

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

/*
   We do _not_ declare this function as doing this would force us to specialize
   it for the user classes deriving from the standard persistent classes.
   However we do define overloads of wxCreatePersistentObject() for all the wx
   classes which means that template wxPersistentObject::Restore() picks up the
   right overload to use provided that the header defining the correct overload
   is included before calling it. And a compilation error happens if this is
   not done.

template <class T>
wxPersistentObject *wxCreatePersistentObject(T *obj);

 */

// ----------------------------------------------------------------------------
// wxPersistenceManager: global aspects of persistent windows
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPersistenceManager
{
public:
    // Call this method to specify a non-default persistence manager to use.
    // This function should usually be called very early to affect creation of
    // all persistent controls and the object passed to it must have a lifetime
    // long enough to be still alive when the persistent controls are destroyed
    // and need it to save their state so typically this would be a global or a
    // wxApp member.
    static void Set(wxPersistenceManager& manager);

    // accessor to the unique persistence manager object
    static wxPersistenceManager& Get();

    // trivial but virtual dtor
    //
    // FIXME-VC6: this only needs to be public because of VC6 bug
    virtual ~wxPersistenceManager();


    // globally disable restoring or saving the persistent properties (both are
    // enabled by default)
    void DisableSaving() { m_doSave = false; }
    void DisableRestoring() { m_doRestore = false; }


    // register an object with the manager: when using the first overload,
    // wxCreatePersistentObject() must be specialized for this object class;
    // with the second one the persistent adapter is created by the caller
    //
    // the object shouldn't be already registered with us
    template <class T>
    wxPersistentObject *Register(T *obj)
    {
        return Register(obj, wxCreatePersistentObject(obj));
    }

    wxPersistentObject *Register(void *obj, wxPersistentObject *po);

    // check if the object is registered and return the associated
    // wxPersistentObject if it is or NULL otherwise
    wxPersistentObject *Find(void *obj) const;

    // unregister the object, this is called by wxPersistentObject itself so
    // there is usually no need to do it explicitly
    //
    // deletes the associated wxPersistentObject
    void Unregister(void *obj);


    // save/restore the state of an object
    //
    // these methods do nothing if DisableSaving/Restoring() was called
    //
    // Restore() returns true if the object state was really restored
    void Save(void *obj);
    bool Restore(void *obj);

    // combines both Save() and Unregister() calls
    void SaveAndUnregister(void *obj)
    {
        Save(obj);
        Unregister(obj);
    }

    // combines both Register() and Restore() calls
    template <class T>
    bool RegisterAndRestore(T *obj)
    {
        return Register(obj) && Restore(obj);
    }

    bool RegisterAndRestore(void *obj, wxPersistentObject *po)
    {
        return Register(obj, po) && Restore(obj);
    }


    // methods used by the persistent objects to save and restore the data
    //
    // currently these methods simply use wxConfig::Get() but they may be
    // overridden in the derived class (once we allow creating custom
    // persistent managers)
#define wxPERSIST_DECLARE_SAVE_RESTORE_FOR(Type)                              \
    virtual bool SaveValue(const wxPersistentObject& who,                     \
                           const wxString& name,                              \
                           Type value);                                       \
                                                                              \
    virtual bool                                                              \
    RestoreValue(const wxPersistentObject& who,                               \
                 const wxString& name,                                        \
                 Type *value)

    wxPERSIST_DECLARE_SAVE_RESTORE_FOR(bool);
    wxPERSIST_DECLARE_SAVE_RESTORE_FOR(int);
    wxPERSIST_DECLARE_SAVE_RESTORE_FOR(long);
    wxPERSIST_DECLARE_SAVE_RESTORE_FOR(wxString);

#undef wxPERSIST_DECLARE_SAVE_RESTORE_FOR

protected:
    // ctor is private, use Get()
    wxPersistenceManager()
    {
        m_doSave =
        m_doRestore = true;
    }


    // Return the config object to use, by default just the global one but a
    // different one could be used by the derived class if needed.
    virtual wxConfigBase *GetConfig() const { return wxConfigBase::Get(); }

    // Return the path to use for saving the setting with the given name for
    // the specified object (notice that the name is the name of the setting,
    // not the name of the object itself which can be retrieved with GetName()).
    virtual wxString GetKey(const wxPersistentObject& who,
                            const wxString& name) const;


private:
    // map with the registered objects as keys and associated
    // wxPersistentObjects as values
    wxPersistentObjectsMap m_persistentObjects;

    // true if we should restore/save the settings (it doesn't make much sense
    // to use this class when both of them are false but setting one of them to
    // false may make sense in some situations)
    bool m_doSave,
         m_doRestore;

    wxDECLARE_NO_COPY_CLASS(wxPersistenceManager);
};

// ----------------------------------------------------------------------------
// wxPersistentObject: ABC for anything persistent
// ----------------------------------------------------------------------------

class wxPersistentObject
{
public:
    // ctor associates us with the object whose options we save/restore
    wxPersistentObject(void *obj) : m_obj(obj) { }

    // trivial but virtual dtor
    virtual ~wxPersistentObject() { }


    // methods used by wxPersistenceManager
    // ------------------------------------

    // save/restore the corresponding objects settings
    //
    // these methods shouldn't be used directly as they don't respect the
    // global wxPersistenceManager::DisableSaving/Restoring() settings, use
    // wxPersistenceManager methods with the same name instead
    virtual void Save() const = 0;
    virtual bool Restore() = 0;


    // get the kind of the objects we correspond to, e.g. "Frame"
    virtual wxString GetKind() const = 0;

    // get the name of the object we correspond to, e.g. "Main"
    virtual wxString GetName() const = 0;


    // return the associated object
    void *GetObject() const { return m_obj; }

protected:
    // wrappers for wxPersistenceManager methods which don't require passing
    // "this" as the first parameter all the time
    template <typename T>
    bool SaveValue(const wxString& name, T value) const
    {
        return wxPersistenceManager::Get().SaveValue(*this, name, value);
    }

    template <typename T>
    bool RestoreValue(const wxString& name, T *value)
    {
        return wxPersistenceManager::Get().RestoreValue(*this, name, value);
    }

private:
    void * const m_obj;

    wxDECLARE_NO_COPY_CLASS(wxPersistentObject);
};

// FIXME-VC6: VC6 has troubles with template methods of DLL-exported classes,
//            apparently it believes they should be defined in the DLL (which
//            is, of course, impossible as the DLL doesn't know for which types
//            will they be instantiated) instead of compiling them when
//            building the main application itself. Because of this problem
//            (which only arises in debug build!) we can't use the usual
//            RegisterAndRestore(obj) with it and need to explicitly create the
//            persistence adapter. To hide this ugliness we define a global
//            function which does it for us.
template <typename T>
inline bool wxPersistentRegisterAndRestore(T *obj)
{
    wxPersistentObject * const pers = wxCreatePersistentObject(obj);

    return wxPersistenceManager::Get().RegisterAndRestore(obj, pers);

}

// A helper function which also sets the name for the (wxWindow-derived) object
// before registering and restoring it.
template <typename T>
inline bool wxPersistentRegisterAndRestore(T *obj, const wxString& name)
{
    obj->SetName(name);

    return wxPersistentRegisterAndRestore(obj);
}

#endif // _WX_PERSIST_H_

