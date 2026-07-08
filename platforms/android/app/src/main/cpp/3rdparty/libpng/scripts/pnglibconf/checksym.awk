#!/bin/awk -f

# checksym.awk - check a list of symbols against the master definition list
#
# Copyright (c) 2025 Cosmin Truta
# Copyright (c) 2010 Glenn Randers-Pehrson
# Originally written by John Bowler, 2010
#
# Use, modification and distribution are subject to
# the same licensing terms and conditions as libpng.
# Please see the copyright notice in png.h or visit
# http://libpng.org/pub/png/src/libpng-LICENSE.txt
#
# SPDX-License-Identifier: libpng-2.0

# Usage:
# awk -f checksym.awk official-def list-to-check
#
# Output is a file in the current directory called "symbols.new",
# the value of the awk variable "of" (which can be changed on the
# command line if required.)  stdout holds error messages.  Exit
# code indicates success or failure.

BEGIN {
   err = 0
   master = ""        # master file
   official[""] = ""  # defined symbols from master file
   symbol[""] = ""    # defined symbols from png.h
   removed[""] = ""   # removed symbols from png.h
   missing = "error"  # log an error on missing symbols
   of = "symbols.new" # default to a fixed name
}

# Read existing definitions from the master file (the first
# file on the command line.)  This is a Windows .def file
# which must NOT have definitions of the form "symbol @ordinal".
master == "" {
   master = FILENAME
}
FILENAME == master && $1 == ";missing" && NF == 2 {
   # This allows the master file to control how missing symbols
   # are handled; symbols that aren't in either the master or
   # the new file.  Valid values are 'ignore', 'warning' and
   # 'error'
   missing = $2
}
FILENAME == master {
   next
}

# Read new definitions, these are free form but the lines must
# just be symbol definitions.  Lines will be commented out for
# 'removed' symbols, introduced in png.h using PNG_REMOVED rather
# than PNG_EXPORT.  Use symbols.dfn to generate the input file.
#
#  symbol   # one field, exported symbol
#  ; symbol # two fields, removed symbol
#
NF == 0 {
   # empty line
   next
}
NF == 1 && $1 ~ /^[A-Za-z_][A-Za-z_0-9]*$/ {
   # exported symbol (one field)
   symbol[$1] = $1
   next
}
NF == 2 && $1 == ";" && $2 ~ /^[A-Za-z_][A-Za-z_0-9]*$/ {
   # removed symbol (two fields)
   removed[$1] = $1
   next
}
{
   print "error: invalid line:", $0
   err = 1
   next
}

END {
   # Write the standard header to "symbols.new":
   print ";Version INSERT-VERSION-HERE" >of
   print ";--------------------------------------------------------------" >of
   print "; LIBPNG symbol list as a Win32 DEF file" >of
   print "; Contains all the symbols that can be exported from libpng" >of
   print ";--------------------------------------------------------------" >of
   print "LIBRARY" >of
   print "" >of
   print "EXPORTS" >of

   # Collect all unique symbols into a plain array for sorting.
   i = 1
   for (sym in symbol) {
      if (sym != "") sorted_symbols[i++] = sym
   }

   # Sort and print the symbols.
   num = do_sort(sorted_symbols)
   for (i = 1; i <= num; i++) {
      print " " sorted_symbols[i] >of
   }

   if (err != 0) {
      print "*** A new list is in", of, "***"
      exit 1
   }

   # TODO: Check for symbols that are both defined and removed.
}

# Portable replacement for the gawk-specific asort function.
function do_sort(arr) {
   # Perform insertion sort, which should be fast enough for our use case.
   num = length(arr)
   for (i = 2; i <= num; i++) {
      key = arr[i]
      j = i - 1
      while (j > 0 && arr[j] > key) {
         arr[j + 1] = arr[j]
         j = j - 1
      }
      arr[j + 1] = key
   }
   # Return the array size, as in the asort function.
   return num
}
