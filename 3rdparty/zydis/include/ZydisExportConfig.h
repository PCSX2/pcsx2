
#ifndef ZYDIS_EXPORT_H
#define ZYDIS_EXPORT_H

#ifdef ZYDIS_STATIC_DEFINE
#  define ZYDIS_EXPORT
#  define ZYDIS_NO_EXPORT
#else
#  ifndef ZYDIS_EXPORT
#    ifdef Zydis_EXPORTS
        /* We are building this library */
#      define ZYDIS_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define ZYDIS_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef ZYDIS_NO_EXPORT
#    define ZYDIS_NO_EXPORT 
#  endif
#endif

#ifndef ZYDIS_DEPRECATED
#  ifdef _MSC_VER
#    define ZYDIS_DEPRECATED __declspec(deprecated)
#  else
#    define ZYDIS_DEPRECATED
#  endif
#endif

#ifndef ZYDIS_DEPRECATED_EXPORT
#  define ZYDIS_DEPRECATED_EXPORT ZYDIS_EXPORT ZYDIS_DEPRECATED
#endif

#ifndef ZYDIS_DEPRECATED_NO_EXPORT
#  define ZYDIS_DEPRECATED_NO_EXPORT ZYDIS_NO_EXPORT ZYDIS_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef ZYDIS_NO_DEPRECATED
#    define ZYDIS_NO_DEPRECATED
#  endif
#endif

#endif /* ZYDIS_EXPORT_H */
