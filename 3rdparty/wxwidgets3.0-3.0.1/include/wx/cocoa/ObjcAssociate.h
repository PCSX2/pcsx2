/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/ObjcAssociate.h
// Purpose:     Associates an Objective-C class with a C++ class
// Author:      David Elliott
// Modified by:
// Created:     2002/12/03
// Copyright:   (c) 2002 David Elliott <dfe@cox.net>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_OBJC_ASSOCIATE_H__
#define __WX_COCOA_OBJC_ASSOCIATE_H__

/*-------------------------------------------------------------------------
Basic hashmap stuff, used by everything
-------------------------------------------------------------------------*/
#define WX_DECLARE_OBJC_HASHMAP(ObjcClass) \
class wxCocoa##ObjcClass; \
WX_DECLARE_HASH_MAP(WX_##ObjcClass,wxCocoa##ObjcClass*,wxPointerHash,wxPointerEqual,wxCocoa##ObjcClass##Hash)

#define WX_DECLARE_OBJC_INTERFACE_HASHMAP(ObjcClass) \
public: \
    static inline wxCocoa##ObjcClass* GetFromCocoa(WX_##ObjcClass cocoaObjcClass) \
    { \
        wxCocoa##ObjcClass##Hash::iterator iter = sm_cocoaHash.find(cocoaObjcClass); \
        if(iter!=sm_cocoaHash.end()) \
        { \
            return iter->second; \
        } \
        return NULL; \
    } \
protected: \
    static wxCocoa##ObjcClass##Hash sm_cocoaHash;

#define WX_IMPLEMENT_OBJC_INTERFACE_HASHMAP(ObjcClass) \
wxCocoa##ObjcClass##Hash wxCocoa##ObjcClass::sm_cocoaHash;


/*-------------------------------------------------------------------------
The entire interface, including some boilerplate stuff
-------------------------------------------------------------------------*/
#define WX_DECLARE_OBJC_INTERFACE(ObjcClass) \
WX_DECLARE_OBJC_INTERFACE_HASHMAP(ObjcClass) \
public: \
    inline void Associate##ObjcClass(WX_##ObjcClass cocoaObjcClass) \
    { \
        if(cocoaObjcClass) \
            sm_cocoaHash.insert(wxCocoa##ObjcClass##Hash::value_type(cocoaObjcClass,this)); \
    } \
    inline void Disassociate##ObjcClass(WX_##ObjcClass cocoaObjcClass) \
    { \
        if(cocoaObjcClass) \
            sm_cocoaHash.erase(cocoaObjcClass); \
    }

#define WX_IMPLEMENT_OBJC_INTERFACE(ObjcClass) \
WX_IMPLEMENT_OBJC_INTERFACE_HASHMAP(ObjcClass)

/*-------------------------------------------------------------------------
Stuff to be used by the wxWidgets class (not the Cocoa interface)
-------------------------------------------------------------------------*/
#define WX_DECLARE_COCOA_OWNER(ObjcClass,ObjcBase,ObjcRoot) \
public: \
    inline WX_##ObjcClass Get##ObjcClass() { return (WX_##ObjcClass)m_cocoa##ObjcRoot; } \
    inline const WX_##ObjcClass Get##ObjcClass() const { return (WX_##ObjcClass)m_cocoa##ObjcRoot; } \
protected: \
    void Set##ObjcClass(WX_##ObjcClass cocoaObjcClass);

#define WX_IMPLEMENT_COCOA_OWNER(wxClass,ObjcClass,ObjcBase,ObjcRoot) \
void wxClass::Set##ObjcClass(WX_##ObjcClass cocoaObjcClass) \
{ \
    Disassociate##ObjcClass((WX_##ObjcClass)m_cocoa##ObjcRoot); \
    Set##ObjcBase(cocoaObjcClass); \
    Associate##ObjcClass((WX_##ObjcClass)m_cocoa##ObjcRoot); \
}

#endif // __WX_COCOA_OBJC_ASSOCIATE_H__
