AC_INIT(pcsx2,0.9.2,zerofrog@gmail.com)

AM_INIT_AUTOMAKE(pcsx2,0.9.2)

AC_PROG_CC([gcc g++ cl KCC CC cxx cc++ xlC aCC c++])
AC_PROG_CXX([gcc g++ cl KCC CC cxx cc++ xlC aCC c++])
AC_PROG_CPP([gcc g++ cl KCC CC cxx cc++ xlC aCC c++])

AC_PROG_RANLIB

dnl Check for debug build
AC_MSG_CHECKING(debug build)
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [debug build]),
 debug=$enableval,debug=no)
if test "x$debug" == xyes
then
 AC_DEFINE(_DEBUG,1,[_DEBUG])
 CFLAGS="-g "
 CPPFLAGS="-g "
 CXXFLAGS="-g "
else
 CFLAGS="-O2 -fomit-frame-pointer "
 CPPFLAGS="-O2 -fomit-frame-pointer "
 CXXFLAGS="-O2 -fomit-frame-pointer "
fi
AC_MSG_RESULT($debug)

dnl CFLAGS+="-masm=intel "
dnl CPPFLAGS="-masm=intel "
dnl CXXFLAGS="-masm=intel "

AC_CHECK_FUNCS([ _aligned_malloc _aligned_free ], AC_DEFINE(HAVE_ALIGNED_MALLOC))

dnl Check for dev build
AC_MSG_CHECKING(for development build)
AC_ARG_ENABLE(devbuild, AC_HELP_STRING([--enable-devbuild], [Special Build for developers that simplifies testing and adds extra checks]),
 devbuild=$enableval,devbuild=no)
if test "x$devbuild" == xyes
then
 AC_DEFINE(PCSX2_DEVBUILD,1,[PCSX2_DEVBUILD])
fi
AC_MSG_RESULT($devbuild)

dnl Check for 64bit CPU
AC_MSG_CHECKING(for a x86-64 CPU)
dnl if test "$build_os" == "target_os"
dnl then
AC_TRY_RUN([
int main()
{
int a = 0;
int*pa = &a;
asm("mov %%rax, %0\n"
    "mov %%eax, [%%rax]\n"
    : : "r"(pa) : "%rax");
return 0;
}
],cpu64=yes,cpu64=no,)
dnl else
dnl cpu64=no
dnl fi
if test "x$cpu64" == xyes
then
AC_DEFINE(__x86_64__,1,[__x86_64__])
fi
AC_MSG_RESULT($cpu64)
AM_CONDITIONAL(X86_64, test x$cpu64 = xyes)

AC_OUTPUT([
 Makefile
 DebugTools/Makefile
 Linux/Makefile
 IPU/Makefile
 IPU/mpeg2lib/Makefile
 pcl/Makefile
 RDebug/Makefile
 tinyxml/Makefile
 x86/Makefile
 x86/ix86/Makefile
 x86/ix86-32/Makefile
 x86/ix86-64/Makefile
 zlib/Makefile
 ])

bindir = pcsx2exe

echo "Configuration:"
echo "  Target system type:    $target"
echo "  x86-64 build?	       $cpu64"
echo "  Debug build?	       $debug"
echo "  Dev build?	       $devbuild"