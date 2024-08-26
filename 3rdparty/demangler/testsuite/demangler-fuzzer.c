/* Demangler fuzzer.

   Copyright (C) 2014-2023 Free Software Foundation, Inc.

   This file is part of GNU libiberty.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "demangle.h"

#define MAXLEN 253
#define ALPMIN 33
#define ALPMAX 127

static char *program_name;

#define DEFAULT_MAXCOUNT 7500000

static void
print_usage (FILE *fp, int exit_value)
{
  fprintf (fp, "Usage: %s [OPTION]...\n", program_name);
  fprintf (fp, "Options:\n");
  fprintf (fp, "  -h           Display this message.\n");
  fprintf (fp, "  -s SEED      Select the random seed to be used.\n");
  fprintf (fp, "               The default is to base one on the");
  fprintf (fp, " current time.\n");
  fprintf (fp, "  -m MAXCOUNT  Exit after MAXCOUNT symbols.\n");
  fprintf (fp, "               The default is %d.", DEFAULT_MAXCOUNT);
  fprintf (fp, " Set to `-1' for no limit.\n");

  exit (exit_value);
}

int
main (int argc, char *argv[])
{
  char symbol[2 + MAXLEN + 1] = "_Z";
  int seed = -1, seed_set = 0;
  int count = 0, maxcount = DEFAULT_MAXCOUNT;
  int optchr;

  program_name = argv[0];

  do
    {
      optchr = getopt (argc, argv, "hs:m:t:");
      switch (optchr)
	{
	case '?':  /* Unrecognized option.  */
	  print_usage (stderr, 1);
	  break;

	case 'h':
	  print_usage (stdout, 0);
	  break;

	case 's':
	  seed = atoi (optarg);
	  seed_set = 1;
	  break;

	case 'm':
	  maxcount = atoi (optarg);
	  break;
	}
    }
  while (optchr != -1);

  if (!seed_set)
    seed = time (NULL);
  srand (seed);
  printf ("%s: seed = %d\n", program_name, seed);

  while (maxcount < 0 || count < maxcount)
    {
      char *buffer = symbol + 2;
      int length, i;

      length = rand () % MAXLEN;
      for (i = 0; i < length; i++)
	*buffer++ = (rand () % (ALPMAX - ALPMIN)) + ALPMIN;

      *buffer++ = '\0';

      cplus_demangle (symbol, DMGL_AUTO | DMGL_ANSI | DMGL_PARAMS);

      count++;
    }

  printf ("%s: successfully demangled %d symbols\n", program_name, count);
  exit (0);
}
