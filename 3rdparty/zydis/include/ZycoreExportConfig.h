
#ifndef ZYCORE_EXPORT_H
#define ZYCORE_EXPORT_H

#ifdef ZYCORE_STATIC_DEFINE
#  define ZYCORE_EXPORT
#  define ZYCORE_NO_EXPORT
#else
#  ifndef ZYCORE_EXPORT
#    ifdef Zycore_EXPORTS
        /* We are building this library */
#      define ZYCORE_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define ZYCORE_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef ZYCORE_NO_EXPORT
#    define ZYCORE_NO_EXPORT 
#  endif
#endif

#ifndef ZYCORE_DEPRECATED
#  ifdef _MSC_VER
#    define ZYCORE_DEPRECATED __declspec(deprecated)
#  else
#    define ZYCORE_DEPRECATED
#  endif
#endif

#ifndef ZYCORE_DEPRECATED_EXPORT
#  define ZYCORE_DEPRECATED_EXPORT ZYCORE_EXPORT ZYCORE_DEPRECATED
#endif

#ifndef ZYCORE_DEPRECATED_NO_EXPORT
#  define ZYCORE_DEPRECATED_NO_EXPORT ZYCORE_NO_EXPORT ZYCORE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef ZYCORE_NO_DEPRECATED
#    define ZYCORE_NO_DEPRECATED
#  endif
#endif

#endif /* ZYCORE_EXPORT_H */
