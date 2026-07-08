# configure.ac

# Copyright (c) 2018-2024 Cosmin Truta
# Copyright (c) 2004-2016 Glenn Randers-Pehrson

# This code is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h

dnl Process this file with autoconf to produce a configure script.
dnl
dnl Minor upgrades (compatible ABI): increment the package version
dnl (third field in two places below) and set the PNGLIB_RELEASE
dnl variable.
dnl
dnl Major upgrades (incompatible ABI): increment the package major
dnl version (second field, or first if desired), set the minor
dnl to 0, set PNGLIB_MAJOR below *and* follow the instructions in
dnl Makefile.am to upgrade the package name.

dnl This is here to prevent earlier autoconf from being used, it
dnl should not be necessary to regenerate configure if the time
dnl stamps are correct
AC_PREREQ([2.68])

dnl Version number stuff here:

AC_INIT([libpng],[1.8.0.git],[png-mng-implement@lists.sourceforge.net])
AC_CONFIG_MACRO_DIR([scripts/autoconf])

# libpng does not follow GNU file name conventions (hence 'foreign')
# color-tests requires automake 1.11 or later
# silent-rules requires automake 1.11 or later
# dist-xz requires automake 1.11 or later
# 1.12.2 fixes a security issue in 1.11.2 and 1.12.1
# 1.13 is required for parallel tests
AM_INIT_AUTOMAKE([1.13 foreign dist-xz color-tests silent-rules subdir-objects])

dnl configure.ac and Makefile.am expect automake 1.11.2 or a compatible later
dnl version; aclocal.m4 will generate a failure if you use a prior version of
dnl automake, so the following is not necessary (and is not defined anyway):
dnl AM_PREREQ([1.11.2])
dnl stop configure from automagically running automake

PNGLIB_VERSION=1.8.0.git
PNGLIB_MAJOR=1
PNGLIB_MINOR=8
PNGLIB_RELEASE=0

dnl End of version number stuff

AC_CONFIG_SRCDIR([pngget.c])
AC_CONFIG_HEADERS([config.h])

# Check the basic programs.
AC_LANG([C])
AC_PROG_CC
AM_PROG_AS
LT_PATH_LD
AC_PROG_CPP
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl libtool/libtoolize; version 2.4.2 is the tested version. This or any
dnl compatible later version may be used
LT_INIT([win32-dll])
LT_PREREQ([2.4.2])

dnl Declare the AWK variable.
AC_ARG_VAR(AWK, [AWK language processor])

# Some awk implementations crash when confronted with pnglibconf.dfa.
# Run a test now, to make sure this doesn't happen.
AC_MSG_CHECKING([if awk ($AWK) works])
if ${AWK} -f "${srcdir}/scripts/pnglibconf/options.awk"\
   out="/dev/null" version=search\
   "${srcdir}/pngconf.h"\
   "${srcdir}/scripts/pnglibconf/pnglibconf.dfa"\
   "${srcdir}/pngusr.dfa" 1>&2
then
   AC_MSG_RESULT([yes])
else
   AC_MSG_FAILURE([no], 1)
fi

# This is a remnant of the old cc -E validation, where it may have been
# necessary to use a different preprocessor for .dfn files
DFNCPP="$CPP"
AC_SUBST(DFNCPP)

AC_ARG_ENABLE([tests],
   AS_HELP_STRING([--disable-tests],
      [do not build the test programs (default is to build)]),
      [enable_tests="$enableval"],
      [enable_tests=yes])

AM_CONDITIONAL([ENABLE_TESTS],
   [test "$enable_tests" != "no"])

AC_ARG_ENABLE([tools],
   AS_HELP_STRING([--disable-tools],
      [do not build the auxiliary tools (default is to build)]),
      [enable_tools="$enableval"],
      [enable_tools=yes])

AM_CONDITIONAL([ENABLE_TOOLS],
   [test "$enable_tools" != "no"])

# -Werror cannot be passed to GCC in CFLAGS because configure will fail
# (it checks the compiler with a program that generates a warning).
# Add the following option to deal with this:
AC_ARG_VAR(PNG_COPTS,
   [additional flags for the C compiler, to be used for options that would]
   [cause configure itself to fail])
AC_ARG_ENABLE(werror,
   AS_HELP_STRING([[[--enable-werror[=OPT]]]],
      [Pass -Werror or the given argument to the compiler if it is supported]),
   [test "$enable_werror" = "yes" && enable_werror="-Werror"
    if test "$enable_werror" != "no"; then
      sav_CFLAGS="$CFLAGS"
      CFLAGS="$enable_werror $CFLAGS"
      AC_MSG_CHECKING([if the compiler allows $enable_werror])
      AC_COMPILE_IFELSE(
         [AC_LANG_SOURCE([
            [int main(int argc, char **argv){]
            [return argv[argc-1][0];]
            [}]])],
         AC_MSG_RESULT(yes)
         PNG_COPTS="$PNG_COPTS $enable_werror",
         AC_MSG_RESULT(no))
      CFLAGS="$sav_CFLAGS"
    fi],)

# For GCC 5 the default mode for C is -std=gnu11 instead of -std=gnu89.
# In pngpriv.h we request just the POSIX 1003.1 and C89 APIs by defining
# _POSIX_SOURCE to 1. This is incompatible with the new default mode, so
# we test for that and force the "-std=c89" compiler option:
AC_MSG_CHECKING([if we need to force back C standard to C89])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([
      [#define _POSIX_SOURCE 1]
      [#include <stdio.h>]
   ])],
   AC_MSG_RESULT(no),[
      if test "x$GCC" != "xyes"; then
         AC_MSG_ERROR(
            [Forcing back to C89 is required but the flags are only known for GCC])
      fi
   AC_MSG_RESULT(yes)
   CFLAGS="$CFLAGS -std=c89"
])

# Checks for structures and compiler characteristics.
AC_STRUCT_TM
AC_C_RESTRICT

# Checks for library functions.
AC_CHECK_FUNCS([pow], ,
  [AC_CHECK_LIB([m], [pow], , [AC_MSG_ERROR([cannot find pow])])])

# Some later POSIX 1003.1 functions are required for test programs, failure
# here is soft (the corresponding test program is not built).
AC_CHECK_FUNC([clock_gettime], , [AC_MSG_WARN([not building timepng])])
AM_CONDITIONAL([HAVE_CLOCK_GETTIME], [test "$ac_cv_func_clock_gettime" = "yes"])

AC_ARG_WITH(zlib-prefix,
   AS_HELP_STRING([[[--with-zlib-prefix]]],
      [prefix that may have been used in installed zlib]),
      [ZPREFIX=${withval}],
      [ZPREFIX='z_'])
AC_CHECK_LIB([z], [zlibVersion], ,
  [AC_CHECK_LIB([z], [${ZPREFIX}zlibVersion], ,
     [AC_MSG_ERROR([zlib not installed])])])

AC_MSG_CHECKING([zlib version])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([
#include <zlib.h>
#if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1280
#error zlib version 1.2.8 or later is required
#endif
int main(void) { return 0; }
])],
  [AC_MSG_RESULT([acceptable])],
  [AC_MSG_RESULT([too old or ZLIB_VERNUM not defined])
   AC_MSG_ERROR([zlib version 1.2.8 or later is required])])

# The following is for pngvalid, to ensure it catches FP errors even on
# platforms that don't enable FP exceptions, the function appears in the math
# library (typically), it's not an error if it is not found.
AC_CHECK_LIB([m], [feenableexcept])
AC_CHECK_FUNCS([feenableexcept])

AC_MSG_CHECKING([if using Solaris linker])
SLD=`$LD --version 2>&1 | grep Solaris`
if test "$SLD"; then
    have_solaris_ld=yes
    AC_MSG_RESULT(yes)
else
    have_solaris_ld=no
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(HAVE_SOLARIS_LD, test "$have_solaris_ld" = "yes")

AC_MSG_CHECKING([if libraries can be versioned])
# Special case for PE/COFF platforms: ld reports
# support for version-script, but doesn't actually
# DO anything with it.
case $host in
*cygwin* | *mingw32* | *interix* )
    have_ld_version_script=no
    AC_MSG_RESULT(no)
;;
* )

if test "$have_solaris_ld" = "yes"; then
    GLD=`$LD --help < /dev/null 2>&1 | grep 'M mapfile'`
else
    GLD=`$LD --help < /dev/null 2>/dev/null | grep version-script`
fi

if test "$GLD"; then
    have_ld_version_script=yes
    AC_MSG_RESULT(yes)
else
    have_ld_version_script=no
    AC_MSG_RESULT(no)
    AC_MSG_WARN(*** You have not enabled versioned symbols.)
fi
;;
esac

AM_CONDITIONAL(HAVE_LD_VERSION_SCRIPT, test "$have_ld_version_script" = "yes")

if test "$have_ld_version_script" = "yes"; then
    AC_MSG_CHECKING([for symbol prefix])
    SYMBOL_PREFIX=`echo "PREFIX=__USER_LABEL_PREFIX__" \
                  | ${CPP:-${CC:-gcc} -E} - 2>&1 \
                  | ${EGREP:-grep} "^PREFIX=" \
                  | ${SED:-sed} -e "s:^PREFIX=::" -e "s:__USER_LABEL_PREFIX__::"`
    AC_SUBST(SYMBOL_PREFIX)
    AC_MSG_RESULT($SYMBOL_PREFIX)
fi

# Substitutions for .in files
AC_SUBST(PNGLIB_VERSION)
AC_SUBST(PNGLIB_MAJOR)
AC_SUBST(PNGLIB_MINOR)
AC_SUBST(PNGLIB_RELEASE)

# Additional arguments (and substitutions)
# Allow the pkg-config directory to be set
AC_ARG_WITH(pkgconfigdir,
   AS_HELP_STRING([[[--with-pkgconfigdir]]],
      [Use the specified pkgconfig dir (default is libdir/pkgconfig)]),
   [pkgconfigdir=${withval}],
   [pkgconfigdir='${libdir}/pkgconfig'])

AC_SUBST([pkgconfigdir])
AC_MSG_NOTICE([[pkgconfig directory is ${pkgconfigdir}]])

# Make the *-config binary config scripts optional
AC_ARG_WITH(binconfigs,
   AS_HELP_STRING([[[--with-binconfigs]]],
      [Generate shell libpng-config scripts as well as pkg-config data]
      [@<:@default=yes@:>@]),
   [if test "${withval}" = no; then
      binconfigs=
      AC_MSG_NOTICE([[libpng-config scripts will not be built]])
    else
      binconfigs='${binconfigs}'
    fi],
   [binconfigs='${binconfigs}'])
AC_SUBST([binconfigs])

# Support for prefixes to the API function names; this will generate defines
# at the start of the build to rename exported library functions
AC_ARG_WITH(libpng-prefix,
   AS_HELP_STRING([[[--with-libpng-prefix]]],
      [prefix libpng exported function (API) names with the given value]),
   [if test "${withval:-no}" != "no"; then
      AC_SUBST([PNG_PREFIX], [${withval}])
    fi])
AM_CONDITIONAL([DO_PNG_PREFIX], [test "${with_libpng_prefix:-no}" != "no"])

# Control over what links are made for installed files.  Versioned files are
# always installed, when the following options are turned on corresponding
# unversioned links are also created (normally as symbolic links):
AC_ARG_ENABLE([unversioned-links],
   AS_HELP_STRING([[[--enable-unversioned-links]]],
      [Installed libpng header files are placed in a versioned subdirectory]
      [and installed libpng library (including DLL) files are versioned.]
      [If this option is enabled unversioned links will be created pointing to]
      [the corresponding installed files.  If you use libpng.pc or]
      [libpng-config for all builds you do not need these links, but if you]
      [compile programs directly they will typically #include <png.h> and]
      [link with -lpng; in that case you need the links.]
      [The links can be installed manually using 'make install-header-links']
      [and 'make install-library-links' and can be removed using the]
      [corresponding uninstall- targets.  If you do enable this option every]
      [libpng 'make install' will recreate the links to point to the just]
      [installed version of libpng.  The default is to create the links;]
      [use --disable-unversioned-links to change this]))

# The AM_CONDITIONAL test is written so that the default is enabled;
# --disable-unversioned-links must be given to turn the option off.
AM_CONDITIONAL([DO_INSTALL_LINKS],[test "$enable_unversioned_links" != "no"])

AC_ARG_ENABLE([unversioned-libpng-pc],
   AS_HELP_STRING([[[--enable-unversioned-libpng-pc]]],
      [Install the configuration file 'libpng.pc' as a link to the versioned]
      [version.  This is done by default - use --disable-unversioned-libpng-pc]
      [to change this.]))
AM_CONDITIONAL([DO_INSTALL_LIBPNG_PC],
   [test "$enable_unversioned_libpng_pc" != "no"])

AC_ARG_ENABLE([unversioned-libpng-config],
   AS_HELP_STRING([[[--enable-unversioned-libpng-config]]],
      [Install the configuration file 'libpng-config' as a link to the]
      [versioned version.  This is done by default - use]
      [--disable-unversioned-libpng-config to change this.]))
AM_CONDITIONAL([DO_INSTALL_LIBPNG_CONFIG],
   [test "$enable_unversioned_libpng_config" != "no"])

# HOST-SPECIFIC OPTIONS
# =====================
#
# DEFAULT
# =======
#
AC_ARG_ENABLE([hardware-optimizations],
   AS_HELP_STRING([[[--enable-hardware-optimizations]]],
      [Enable hardware optimizations: no/off, =yes/on.]),
   [case "$enableval" in
      no|off)
         # disable hardware optimization on all systems
         AC_DEFINE([PNG_NO_HARDWARE], [1],
                   [Disable hardware specific optimizations])
         ;;
      *)
         ;;
   esac])

AC_MSG_NOTICE([[Extra options for compiler: $PNG_COPTS]])

# Config files, substituting as above
AC_CONFIG_FILES([Makefile libpng.pc:libpng.pc.in])
AC_CONFIG_FILES([libpng-config:libpng-config.in],
   [chmod +x libpng-config])

AC_OUTPUT
