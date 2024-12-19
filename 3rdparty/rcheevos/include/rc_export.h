#ifndef RC_EXPORT_H
#define RC_EXPORT_H

/* These macros control how callbacks and public functions are defined */

/* RC_SHARED should be defined when building rcheevos as a shared library (e.g. dll/dylib/so). External code should not define this macro. */
/* RC_STATIC should be defined when building rcheevos as a static library. External code should also define this macro. */
/* RC_IMPORT should be defined for external code using rcheevos as a shared library. */

/* For compatibility, if none of these three macros are defined, then the build is assumed to be RC_STATIC */

#if !defined(RC_SHARED) && !defined(RC_STATIC) && !defined(RC_IMPORT)
  #define RC_STATIC
#endif

#if (defined(RC_SHARED) && defined(RC_STATIC)) || (defined(RC_SHARED) && defined(RC_IMPORT)) || (defined(RC_STATIC) && defined(RC_IMPORT))
  #error RC_SHARED, RC_STATIC, and RC_IMPORT are mutually exclusive
#endif

/* RC_BEGIN_C_DECLS and RC_END_C_DECLS should be used for all headers, to enforce C linkage and the C calling convention */
/* RC_BEGIN_C_DECLS should be placed after #include's and before header declarations */
/* RC_END_C_DECLS should be placed after header declarations */

/* example usage
 *
 * #ifndef RC_HEADER_H
 * #define RC_HEADER_H
 *
 * #include <stdint.h>
 *
 * RC_BEGIN_C_DECLS
 *
 * uint8_t rc_function(void);
 *
 * RC_END_C_DECLS
 *
 * #endif
 */

#ifdef __cplusplus
  #define RC_BEGIN_C_DECLS extern "C" {
  #define RC_END_C_DECLS }
#else
  #define RC_BEGIN_C_DECLS
  #define RC_END_C_DECLS
#endif

/* RC_CCONV should be used for public functions and callbacks, to enforce the cdecl calling convention, if applicable */
/* RC_CCONV should be placed after the return type, and between the ( and * for callbacks */

/* example usage */
/* void RC_CCONV rc_function(void) */
/* void (RC_CCONV *rc_callback)(void) */

#if defined(_WIN32)
  /* Windows compilers will ignore __cdecl when not applicable */
  #define RC_CCONV __cdecl
#elif defined(__GNUC__) && defined(__i386__)
  /* GNU C compilers will warn if cdecl is defined on an unsupported platform */
  #define RC_CCONV __attribute__((cdecl))
#else
  #define RC_CCONV
#endif

/* RC_EXPORT should be used for public functions */
/* RC_EXPORT will provide necessary hints for shared library usage, if applicable */
/* RC_EXPORT should be placed before the return type */

/* example usage */
/* RC_EXPORT void rc_function(void) */

#ifdef RC_SHARED
  #if defined(_WIN32)
    #define RC_EXPORT __declspec(dllexport)
  #elif defined(__GNUC__) && __GNUC__ >= 4
    #define RC_EXPORT __attribute__((visibility("default")))
  #else
    #define RC_EXPORT
  #endif
#endif

#ifdef RC_IMPORT
  #if defined(_WIN32)
    #define RC_EXPORT __declspec(dllimport)
  #elif defined(__GNUC__) && __GNUC__ >= 4
    #define RC_EXPORT __attribute__((visibility("default")))
  #else
    #define RC_EXPORT
  #endif
#endif

#ifdef RC_STATIC
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define RC_EXPORT __attribute__((visibility("default")))
  #else
    #define RC_EXPORT
  #endif
#endif

#endif /* RC_EXPORT_H */
