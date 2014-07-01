/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/objc/objc_uniquifying.h
// Purpose:     Allows wxWidgets code to get a direct pointer to a compiled
//              Objective-C class and provides a method to fix up the
//              name to include a unique identifier (currently the address
//              of the objc_class structure).
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2007/05/15
// Copyright:   (c) 2007 Software 2000 Ltd.
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_OBJC_CLASS_H__
#define __WX_COCOA_OBJC_CLASS_H__

/* A note about this header:
Nothing in here is guaranteed to exist in future versions of wxCocoa. There
are other ways of adding Objective-C classes at runtime and a future wxCocoa
might use these instead of this ugly hack.  You may use this header file in
your own wxCocoa code if you need your own Objective-C classes to be
unqiuified.

You cannot turn this on for 64-bit mode. It will not compile due to opaque
Objective-C data structures and it is not needed because it is a workaround
for a bug that does not exist in the 64-bit runtime.

You should not use this when wxCocoa is built as a dynamic library.  This has
only been tested for the case when wxCocoa is built as a static library and
statically linked to user code to form a loadable bundle (e.g. a Cocoa plugin).
It forces each plugin (when multiple wxCocoa-using plugins are used) to use
its own internal Objective-C classes which is desirable when wxCocoa is
statically linked to the rest of the code.

Do not use uniquifying on your principal class.  That one should be named
differently for different bundles.
 */

#if wxUSE_OBJC_UNIQUIFYING

// objc_getClass and stuff
#include <objc/objc-runtime.h>

////////////// Objective-C uniquifying implementation //////////////

template <typename ObjcType>
class wxObjcClassInitializer;

template <typename ObjcType>
class UniquifiedName;

template <typename ObjcType>
class wxObjcCompilerInformation
{
    friend class wxObjcClassInitializer<ObjcType>;
    friend class UniquifiedName<ObjcType>;
private:
    // GetCompiledClass must be partially specialized for an ObjcType
    // If you're not using it, implement an inline returning NULL
    inline static struct objc_class * GetCompiledClass();

    // sm_theClassName must be partially specialized for each type
    static const char sm_theClassName[];

    // GetSuperclass must be specialized. Typically one of two ways:
    // 1. objc_getClass("SomeRealClassName")
    // 2. wxGetObjcClass_SomeWxClassName();
    inline static struct objc_class *GetSuperclass();
};


template <typename ObjcType>
struct UniquifiedName
{
    // We're going for OriginalClassName@ClassStructureAddress
    // Therefore our size is the sizeof the original class name constant string (which includes the terminating NULL)
    // plus the sizeof a pointer to struct objc_class times two (two hex digits for each byte) plus 3 for "@0x"
    typedef char Type[sizeof(wxObjcCompilerInformation<ObjcType>::sm_theClassName) + (sizeof(struct objc_class*)<<1) + 3];
    static void Init(Type m_theString, const objc_class *aClass)
    {
        snprintf(const_cast<char*>(m_theString), sizeof(Type), "%s@%p", wxObjcCompilerInformation<ObjcType>::sm_theClassName, aClass);
    }
};

/*! @function   HidePointerFromGC
    @abstract   Returns an l-value whose location the compiler cannot know.
    @discussion
    The compiler-generated Objective-C class structures are located in the static data area.
    They are by design Objective-C objects in their own right which makes the compiler issue
    write barriers as if they were located in the GC-managed heap as most Objective-C objects.

    By accepting and returning a reference to any pointer type we can set any i-var of an
    Objective-C object that is a pointer to another Objective-C object without the compiler
    generating an objc_assign_ivar write barrier.  It will instad generate an
    objc_assign_strongCast write barrier which is the appropriate write-barrier when assigning
    pointers to Objective-C objects located in unknown memory.

    For instance:
    Class *someClass = ...;
    HidePointerFromGC(someClass->isa) = ...;
 */
template <typename ObjcType>
inline ObjcType * & HidePointerFromGC(ObjcType * &p) __attribute__((always_inline));

template <typename ObjcType>
inline ObjcType * & HidePointerFromGC(ObjcType * &p)
{
    return p;
}

template <typename ObjcType>
class wxObjcClassInitializer
{
public:
    static struct objc_class* Get()
    {
        static wxObjcClassInitializer<ObjcType> s_theInstance;
        s_theInstance.noop(); // Make the compiler think we need this instance
        return wxObjcCompilerInformation<ObjcType>::GetCompiledClass();
    }
private:
    void noop()
    {}
    // This "constructor" operates solely on static data
    // It exists so that we can take advantage of a function-static
    // "instance" of this class to do the static data initialization.
    wxObjcClassInitializer()
    {
        // Objective-C class initialization occurs before C++ static initialization because the
        // libobjc.dylib gets notified directly by dyld on Tiger.
        // Therefore, even though we change the name, the class is still registered with the
        // original name.  We unfortunately can't change that.

        // The first time the class is loaded, Objective-C will already have fixed up the super_class
        // and isa->isa and isa->super_class variables so much of this won't do anything.  But
        // the next time the class is loaded, Objective-C will ignore it and thus we need to
        // initialize the data structures appropriately.

        // Ideally we'd have some sort of lock here, but we depend on the fact that we get called
        // just before the first time someone wants to send a class message so it should be
        // reasonably safe to do this without any locks.

        struct objc_class &theClassData = *wxObjcCompilerInformation<ObjcType>::GetCompiledClass();
        // Initialize the uniquified class name
        UniquifiedName<ObjcType>::Init(sm_theUniquifiedClassName, &theClassData);

        //////// Class Initialization ////////
        // Use objc_getClass to fix up the superclass pointer
        theClassData.super_class = wxObjcCompilerInformation<ObjcType>::GetSuperclass();
        // Fix up the compiler generated class struct to use the new name
        theClassData.name = sm_theUniquifiedClassName;

        //////// Meta-Class Initialization ////////
        // theClassData.isa is the metaclass pointer
        // Globals on Darwin use PC-relative access (slow) so it's quicker to use theClassData.isa

        // In any object hierarchy a metaclass's metaclass is always the root class's metaclass
        // Therefore, our superclass's metaclass's metaclass should already be the root class's metaclass
        HidePointerFromGC(theClassData.isa->isa) = theClassData.super_class->isa->isa;
        // A metaclass's superclass is always the superclass's metaclass.
        HidePointerFromGC(theClassData.isa->super_class) = theClassData.super_class->isa;
        // Fix up the compiler generated metaclass struct to use the new name
        theClassData.isa->name = sm_theUniquifiedClassName;

        // We need to set the initialized flag because after we change the name, Objective-C can't
        // look us up by name because we're only registered with the original name.
        theClassData.isa->info |= CLS_INITIALIZED;
    }
    wxObjcClassInitializer(const wxObjcClassInitializer&); // NO COPY
    wxObjcClassInitializer& operator =(const wxObjcClassInitializer&); // NO ASSIGN
    static typename UniquifiedName<ObjcType>::Type sm_theUniquifiedClassName;
};

template<typename ObjcType>
typename UniquifiedName<ObjcType>::Type wxObjcClassInitializer<ObjcType>::sm_theUniquifiedClassName;

// WX_DECLARE_GET_OBJC_CLASS
// Declares a function to get a direct pointer to an objective-C class.
// The class is guaranteed to be usable.
// When wxCocoa is built into a Mach-O bundle this function allows the wxCocoa
// code to get a reference to the Objective-C class structure located in the
// same bundle.  This allows a static wxCocoa library to be built into
// two different Mach-O bundles without having one bundle's Objective-C
// classes trample on the other's.
// Right now we toss the ObjcSuperClass parameter, but we might use it later.
#define WX_DECLARE_GET_OBJC_CLASS(ObjcClass,ObjcSuperClass) \
struct objc_class* wx_GetObjcClass_ ## ObjcClass();

// WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass)
// Provides an architecture-dependent way to get the direct pointer to the
// objc_class structure in the __OBJC segment.
// This takes advantage of the fact that the Objective-C compiler uses guessable
// local assembler labels for the class structures.
// Those class structures are only available on the Objective-C file containing the
// @implementation block.

#if 1
// Generic implementation - Tested on i386 and PPC.  Should work in all cases.
// This is a hack that depends on GCC asm symbol names.
// The static variable winds up being initialized with a direct reference to the appropriate
// L_OBJC_CLASS and no global symbol reference is generated because nothing uses the global symbol
// except for the static initializer which does it directly.
// The generated assembler for s_objc_class_ptr is basically like this:
// _s_objc_class_ptr_ObjcClass:
//     .long L_OBJC_CLASS_ObjcClass
// Once that static symbol is defined, the function implementation is easy for GCC to generate.
// Do note that return &s_objc_class_data_ObjcClass won't work.  The code is wrong in this case.
#define WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass) \
extern "C" objc_class s_objc_class_data_ ## ObjcClass asm("L_OBJC_CLASS_" #ObjcClass); \
static objc_class * s_objc_class_ptr_ ## ObjcClass = &s_objc_class_data_ ## ObjcClass; \
template<> \
inline objc_class * wxObjcCompilerInformation<ObjcClass>::GetCompiledClass() \
{ \
    return s_objc_class_ptr_## ObjcClass; \
}

#elif defined(__i386__)
// Not used because the generic implementation seems to work fine.
// But this is here since it was written beforehand and it also works.

// This is based on the code GCC generates for accessing file-static data on i386.
// The i386 PC-relative addressing happens in this manner
// 1. The program counter is placed into ecx using the code that GCC should have
//    already generated.
// 2. A label is placed directly after the call to get the program counter.
// 3. The Load Effective Address instruction is used to add the offset of the
//    local assembler label we're interested in minus the local assembler label
//    from step 2 to the program counter register in ecx and place the result
//    into the result register (typically eax if not inlined).
#define WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass) \
template<> \
inline objc_class * wxObjcCompilerInformation<ObjcClass>::GetCompiledClass() \
{ \
    register struct objc_class *retval; \
    asm \
    (   "call ___i686.get_pc_thunk.cx\n" \
        "\"LPC_FOR_GET_CLASS_" #ObjcClass "\":\n\t" \
        "leal L_OBJC_CLASS_" #ObjcClass "-\"LPC_FOR_GET_CLASS_" #ObjcClass "\"(%%ecx), %0" \
    :   "=r"(retval) \
    :    \
    :   "ecx" \
    ); \
    return retval; \
}

#elif defined(__ppc__)
// Not used because the generic implementation seems to work fine.
// But this is here since it was written beforehand and it also works.

// This is based on the code GCC generates for accessing file-static data on PPC.
// The PowerPC PC-relative addressing happens in this manner
// 1. The link register is saved (mflr) to a temporary (we re-use the output register for this)
// 2. An unconditional branch instruction (bcl) "branches" to the following address (labeled)
// 3. The link register (filled in by bcl) is saved to r10 (a temporary)
// 4. The previous link register is restored (mtlr) (from the output register we were using as a temporary)
// 5. The address of the LPC label as executed is added to the high 16 bits of the offset between that label and the static data we want
//    and stored in a temporary register (r2)
// 6. That temporary register plus the low 16 bits of the offset are stored into the result register.
#define WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass) \
template<> \
inline objc_class * wxObjcCompilerInformation<ObjcClass>::GetCompiledClass() \
{ \
    register struct objc_class *retval; \
    asm \
    (   "mflr %0" \
        "\n\tbcl 20, 31, \"LPC_FOR_GET_CLASS_" #ObjcClass "\"" \
        "\n\"LPC_FOR_GET_CLASS_" #ObjcClass "\":" \
        "\n\tmflr r10" \
        "\n\tmtlr %0" \
        "\n\taddis r2,r10,ha16(L_OBJC_CLASS_" #ObjcClass "-\"LPC_FOR_GET_CLASS_" #ObjcClass "\")" \
        "\n\tla %0,lo16(L_OBJC_CLASS_" #ObjcClass "-\"LPC_FOR_GET_CLASS_" #ObjcClass "\")(r2)" \
    :   "=r" (retval) \
    : \
    :   "r10","r2" \
    ); \
    return retval; \
}

// TODO: __x86_64__, __ppc64__
#else // Can't write inline asm to bust into __OBJC segment
// This won't be used since the generic implementation takes precedence.

#warning "Don't know how to implement wxObjcCompilerInformation<ObjcClass>::GetCompiledClass on this platform"

#endif // platforms

// The WX_IMPLEMENT_OBJC_GET_SUPERCLASS macro implements the template specialization
// to get the superclass.  This only works if it's a real superclass.  If you are
// deriving from a class that's already being uniquified then you'd need to
// implement the specialization to call the appropriate get method instead.
#define WX_IMPLEMENT_OBJC_GET_SUPERCLASS(ObjcClass,ObjcSuperClass) \
    template <> \
    inline objc_class* wxObjcCompilerInformation<ObjcClass>::GetSuperclass() \
    { \
        return objc_getClass(#ObjcSuperClass); \
    }

// The WX_IMPLEMENT_OBJC_GET_UNIQUIFIED_SUPERCLASS macro implements the template
// specialization to get the superclass when the superclass is another uniquified
// Objective-C class.
#define WX_IMPLEMENT_OBJC_GET_UNIQUIFIED_SUPERCLASS(ObjcClass,ObjcSuperClass) \
    template <> \
    inline objc_class* wxObjcCompilerInformation<ObjcClass>::GetSuperclass() \
    { \
        return wx_GetObjcClass_ ## ObjcSuperClass(); \
    }

// The WX_IMPLEMENT_OBJC_CLASS_NAME macro implements the template specialization
// of the sm_theClassName constant.  As soon as this specialization is in place
// sizeof(sm_theClassName) will return the number of bytes at compile time.
#define WX_IMPLEMENT_OBJC_CLASS_NAME(ObjcClass) \
    template <> \
    const char wxObjcCompilerInformation<ObjcClass>::sm_theClassName[] = #ObjcClass;

// The WX_IMPLEMENT_OBJC_GET_OBJC_CLASS macro is the final one that actually provides
// the wx_GetObjcClass_XXX function that will be called in lieu of asking the Objective-C
// runtime for the class.  All the others are really machinery to make this happen.
#define WX_IMPLEMENT_OBJC_GET_OBJC_CLASS(ObjcClass) \
    objc_class* wx_GetObjcClass_ ## ObjcClass() \
    { \
        return wxObjcClassInitializer<ObjcClass>::Get(); \
    }

// The WX_IMPLEMENT_GET_OBJC_CLASS macro combines all of these together
// for the case when the superclass is a non-uniquified class.
#define WX_IMPLEMENT_GET_OBJC_CLASS(ObjcClass,ObjcSuperClass) \
    WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass) \
    WX_IMPLEMENT_OBJC_GET_SUPERCLASS(ObjcClass,ObjcSuperClass) \
    WX_IMPLEMENT_OBJC_CLASS_NAME(ObjcClass) \
    WX_IMPLEMENT_OBJC_GET_OBJC_CLASS(ObjcClass)

// The WX_IMPLEMENT_GET_OBJC_CLASS_WITH_UNIQUIFIED_SUPERCLASS macro combines all
// of these together for the case when the superclass is another uniquified class.
#define WX_IMPLEMENT_GET_OBJC_CLASS_WITH_UNIQUIFIED_SUPERCLASS(ObjcClass,ObjcSuperClass) \
    WX_IMPLEMENT_OBJC_GET_COMPILED_CLASS(ObjcClass) \
    WX_IMPLEMENT_OBJC_GET_UNIQUIFIED_SUPERCLASS(ObjcClass,ObjcSuperClass) \
    WX_IMPLEMENT_OBJC_CLASS_NAME(ObjcClass) \
    WX_IMPLEMENT_OBJC_GET_OBJC_CLASS(ObjcClass)

// The WX_GET_OBJC_CLASS macro is intended to wrap the class name when the class
// is used as a message receiver (e.g. for calling class methods).  When
// class name uniquifying is used, this calls the global function implemented
// in the Objective-C file containing the class @implementation.
#define WX_GET_OBJC_CLASS(ObjcClass) wx_GetObjcClass_ ## ObjcClass()

#else // wxUSE_OBJC_UNIQUIFYING

// Define WX_DECLARE_GET_OBJC_CLASS as nothing
#define WX_DECLARE_GET_OBJC_CLASS(ObjcClass,ObjcSuperClass)
// Define WX_IMPLEMENT_GET_OBJC_CLASS as nothing
#define WX_IMPLEMENT_GET_OBJC_CLASS(ObjcClass,ObjcSuperClass)
// Define WX_IMPLEMENT_GET_OBJC_CLASS_WITH_UNIQUIFIED_SUPERCLASS as nothing
#define WX_IMPLEMENT_GET_OBJC_CLASS_WITH_UNIQUIFIED_SUPERCLASS(ObjcClass,ObjcSuperClass)

// Define WX_GET_OBJC_CLASS macro to output the class name and let the compiler do the normal thing
// The WX_GET_OBJC_CLASS macro is intended to wrap the class name when the class
// is used as a message receiver (e.g. for calling class methods).  When
// class name uniquifying is not used, this is simply defined to be the class
// name which will allow the compiler to do the normal thing.
#define WX_GET_OBJC_CLASS(ObjcClass) ObjcClass

#endif // wxUSE_OBJC_UNIQUIFYING

#endif //ndef __WX_COCOA_OBJC_CLASS_H__
