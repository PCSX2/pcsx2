#!/bin/sh

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2002-2011  PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License as published by the Free Software Found-
# ation, either version 3 of the License, or (at your option) any later version.
#
# PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with PCSX2.
# If not, see <http://www.gnu.org/licenses/>.

# This script is a small wrapper to the PCSX2 exectuable. The purpose is to
# 1/ launch PCSX2 from the same repository every times.
# Rationale: There is no guarantee on the directory when PCSX2 is launched from a shortcut.
#            This behavior trigger the first time wizards everytime...
# 2/ Change LD_LIBRARY_PATH to uses 3rdparty library
# Rationale: It is nearly impossible to have the same library version on all systems. So the
#            easiest solution it to ship library used during the build.
# 3/ Set __GL_THREADED_OPTIMIZATIONS variable for Nvidia Drivers (major speed boost)

set -e

current_script=$0
me=PCSX2-linux.sh

# We are already in the good directory. Allow to use "bash PCSX2-linux.sh"
if [ $current_script = $me ]
then
    if [ -e "./$me" ]
    then
        current_script="./$me"
    else
        current_script=`which $me`
    fi
fi

# Avoid to screw up the shell context
DIR=`dirname $current_script`
MY_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

if [ ! -x "$DIR/PCSX2" ]
then
    echo "Error PCSX2 not found in $DIR"
    echo "Maybe the script was directly 'called'"
    echo "Use either /absolute_path/$me or ./relative_path/$me"
    return 1 # warning exit will kill current terminal
fi

# Allow to ship .so library with the build to avoid version issue 
MY_LD_LIBRARY_PATH=${MY_LD_LIBRARY_PATH:+$MY_LD_LIBRARY_PATH:}$DIR/3rdPartyLibs

# openSUSE don't follow FHS !!!!
MY_LD_LIBRARY_PATH=${MY_LD_LIBRARY_PATH:+$MY_LD_LIBRARY_PATH:}/usr/lib/wx-2.8-stl

# And finally launch me
GDK_BACKEND=x11 LD_LIBRARY_PATH="$MY_LD_LIBRARY_PATH" __GL_THREADED_OPTIMIZATIONS=1 mesa_glthread=true MESA_NO_ERROR=1 $DIR/PCSX2 "$@"
