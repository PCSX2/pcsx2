/* Declare the environ system variable.
   Copyright (C) 2015-2023 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* On OSX, the environ variable can be used directly in the code of an
   executable, but cannot be used in the code of a shared library (such as
   GCC's liblto_plugin, which links in libiberty code).  Instead, the
   function _NSGetEnviron can be called to get the address of environ.  */

#ifndef HAVE_ENVIRON_DECL
#  ifdef __APPLE__
#     include <crt_externs.h>
#     define environ (*_NSGetEnviron ())
#  else
#    ifndef environ
extern char **environ;
#    endif
#  endif
#  define HAVE_ENVIRON_DECL
#endif
