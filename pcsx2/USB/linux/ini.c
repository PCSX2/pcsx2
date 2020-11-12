/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h> // NULL
#include <stdio.h> // sprintf()
#include <stdarg.h> // va_start(), va_end(), vsprintf()
//#include "logfile.h"
#include "actualfile.h"
#include "ini.h"

//#define VERBOSE_FUNCTION_INI 1
const char INIext[] = ".ini";
const char INInewext[] = ".new";

// Returns: position where new extensions should be added.
int INIRemoveExt(const char *argname, char *tempname) {
  int i;
  int j;
  int k;

  i = 0;
  while((i <= INIMAXLEN) && (*(argname + i) != 0)) {
    *(tempname + i) = *(argname + i);
    i++;
  } // ENDWHILE- Copying the argument name into a temporary area;
  *(tempname + i) = 0; // And 0-terminate
  k = i;
  k--;

  j = 0;
  while((j <= INIMAXLEN) && (INIext[j] != 0)) j++;
  j--;

  while((j >= 0) && (*(tempname + k) == INIext[j])) {
    k--;
    j--;
  } // ENDWHILE- Comparing the ending characters to the INI ext.
  if(j < 0) {
    k++;
    i = k;
    *(tempname + i) = 0; // 0-terminate, cutting off ".ini"
  } // ENDIF- Do we have a match? Then remove the end chars.

  return(i);
} // END INIRemoveExt()


void INIAddInExt(char *tempname, int temppos) {
  int i;

  i = 0;
  while((i + temppos < INIMAXLEN) && (INIext[i] != 0)) {
    *(tempname + temppos + i) = INIext[i];
    i++;
  } // ENDWHILE- Attaching extenstion to filename
  *(tempname + temppos + i) = 0; // And 0-terminate
} // END INIAddInExt()


void INIAddOutExt(char *tempname, int temppos) {
  int i;

  i = 0;
  while((i + temppos < INIMAXLEN) && (INInewext[i] != 0)) {
    *(tempname + temppos + i) = INInewext[i];
    i++;
  } // ENDWHILE- Attaching extenstion to filename
  *(tempname + temppos + i) = 0; // And 0-terminate
} // END INIAddInExt()


// Returns number of bytes read to get line (0 means end-of-file)
int INIReadLine(ACTUALHANDLE infile, char *buffer) {
  int charcount;
  int i;
  char tempin[2];
  int retflag;
  int retval;

  charcount = 0;
  i = 0;
  tempin[1] = 0;
  retflag = 0;

  while((i < INIMAXLEN) && (retflag < 2)) {
    retval = ActualFileRead(infile, 1, tempin);
    charcount++;
    if(retval != 1) {
      retflag = 2;
      charcount--;

    } else if(tempin[0] == '\n') {
      retflag = 2;

    } else if(tempin[0] >= ' ') {
      *(buffer + i) = tempin[0];
      i++;
    } // ENDLONGIF- How do we react to the next character?
  } // ENDWHILE- Loading up on characters until an End-of-Line appears
  *(buffer + i) = 0; // And 0-terminate

  return(charcount);
} // END INIReadLine()
// Note: Do we need to back-skip a char if something other \n follows \r?


// Returns: number of bytes to get to start of section (or -1)
int INIFindSection(ACTUALHANDLE infile, const char *section) {
  int charcount;
  int i;
  int retflag;
  int retval;
  char scanbuffer[INIMAXLEN+1];


  charcount = 0;
  retflag = 0;

  while(retflag == 0) {
    retval = INIReadLine(infile, scanbuffer);
    if(retval == 0)  return(-1); // EOF? Stop here.

    if(scanbuffer[0] == '[') {
      i = 0;
      while((i < INIMAXLEN) &&
            (*(section + i) != 0) &&
            (*(section + i) == scanbuffer[i + 1]))  i++;
      if((i < INIMAXLEN - 2) && (*(section + i) == 0)) {
        if((scanbuffer[i + 1] == ']') && (scanbuffer[i + 2] == 0)) {
          retflag = 1;
        } // ENDIF- End marks look good? Return successful.
      } // ENDIF- Do we have a section match?
    } // ENDIF- Does this look like a section header?

    if(retflag == 0)  charcount += retval;
  } // ENDWHILE- Scanning lines for the correct [Section] header.

  return(charcount);
} // END INIFindSection()

// Returns: number of bytes to get to start of keyword (or -1)
int INIFindKeyword(ACTUALHANDLE infile, const char *keyword, char *buffer) {
  int charcount;
  int i;
  int j;
  int retflag;
  int retval;
  char scanbuffer[INIMAXLEN+1];

  charcount = 0;
  retflag = 0;

  while(retflag == 0) {
    retval = INIReadLine(infile, scanbuffer);
    if(retval == 0)  return(-1); // EOF? Stop here.
    if(scanbuffer[0] == '[')  return(-1); // New section? Stop here.

    i = 0;
    while((i < INIMAXLEN) &&
          (*(keyword + i) != 0) &&
          (*(keyword + i) == scanbuffer[i]))  i++;
    if((i < INIMAXLEN - 2) && (*(keyword + i) == 0)) {
      if(scanbuffer[i] == '=') {
        retflag = 1;
        if(buffer != NULL) {
          i++;
          j = 0;
          while((i < INIMAXLEN) && (scanbuffer[i] != 0)) {
            *(buffer + j) = scanbuffer[i];
            i++;
            j++;
          } // ENDWHILE- Copying the value out to the outbound buffer.
          *(buffer + j) = 0; // And 0-terminate.
        } // ENDIF- Return the value as well?
      } // ENDIF- End marks look good? Return successful.
    } // ENDIF- Do we have a section match?

    if(retflag == 0)  charcount += retval;
  } // ENDWHILE- Scanning lines for the correct [Section] header.

  return(charcount);
} // END INIFindKeyWord()


// Returns: number of bytes left to write... (from charcount back)
int INICopy(ACTUALHANDLE infile, ACTUALHANDLE outfile, int charcount) {
  char buffer[4096];
  int i;
  int chunk;
  int retval;

  i = charcount;
  chunk = 4096;
  if(i < chunk)  chunk = i;
  while(chunk > 0) {
    retval = ActualFileRead(infile, chunk, buffer);
    if(retval <= 0)  return(i); // Trouble? Stop here.
    if(retval < chunk)  chunk = retval; // Short block? Note it.

    retval = ActualFileWrite(outfile, chunk, buffer);
    if(retval <= 0)  return(i); // Trouble? Stop here.
    i -= retval;
    if(retval < chunk)  return(i); // Short block written? Stop here.

    chunk = 4096;
    if(i < chunk)  chunk = i;
  } // ENDWHILE- Copying a section of file across, one chunk at a time.

  return(0);
} // END INICopyToPos()


int INISaveString(const char *file, const char *section, const char *keyword, const char *value) {
  char inname[INIMAXLEN+1];
  char outname[INIMAXLEN+1];
  int filepos;
  ACTUALHANDLE infile;
  ACTUALHANDLE outfile;
  int i;
  int retval;
  char templine[INIMAXLEN+1];

  if(file == NULL)  return(-1);
  if(section == NULL)  return(-1);
  if(keyword == NULL)  return(-1);
  if(value == NULL)  return(-1);

  filepos = INIRemoveExt(file, inname);
  for(i = 0; i <= filepos; i++)  outname[i] = inname[i];
  INIAddInExt(inname, filepos);
  INIAddOutExt(outname, filepos);

  filepos = 0;
  infile = ActualFileOpenForRead(inname);
  if(infile == ACTUALHANDLENULL) {
    outfile = ActualFileOpenForWrite(inname);
    if(outfile == ACTUALHANDLENULL)  return(-1); // Just a bad name? Abort.

    sprintf(templine, "[%s]\r\n", section);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    if(retval < i) {
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(inname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.

    sprintf(templine, "%s=%s\r\n", keyword, value);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    ActualFileClose(outfile);
    outfile = ACTUALHANDLENULL;
    if(retval < i) {
      ActualFileDelete(inname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.
    return(0);
  } // ENDIF- No input file? Create a brand new .ini file then.

  retval = INIFindSection(infile, section);
  if(retval < 0) {
    outfile = ActualFileOpenForWrite(outname);
    if(outfile == ACTUALHANDLENULL) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't open a temp file? Abort

    ActualFileSeek(infile, 0); // Move ini to beginning of file...
    INICopy(infile, outfile, 0x0FFFFFFF); // Copy the whole file out...

    sprintf(templine, "\r\n[%s]\r\n", section);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    if(retval < i) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.

    sprintf(templine, "%s=%s\r\n", keyword, value);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    ActualFileClose(infile);
    infile = ACTUALHANDLENULL;
    ActualFileClose(outfile);
    outfile = ACTUALHANDLENULL;
    if(retval < i) {
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.

    ActualFileDelete(inname);
    ActualFileRename(outname, inname);
    return(0);
  } // ENDIF- Couldn't find the section? Make a new one!

  filepos = retval;
  ActualFileSeek(infile, filepos);
  filepos += INIReadLine(infile, templine); // Get section line's byte count

  retval = INIFindKeyword(infile, keyword, NULL);
  if(retval < 0) {
    ActualFileSeek(infile, filepos);
    retval = INIReadLine(infile, templine);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0) && (templine[i] != '='))  i++;
    while((retval > 0) && (templine[i] == '=')) {
      filepos += retval;
      retval = INIReadLine(infile, templine);
      i = 0;
      while((i < INIMAXLEN) && (templine[i] != 0) && (templine[i] != '='))  i++;
    } // ENDWHILE- skimming to the bottom of the section

    outfile = ActualFileOpenForWrite(outname);
    if(outfile == ACTUALHANDLENULL) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't open a temp file? Abort

    ActualFileSeek(infile, 0);
    retval = INICopy(infile, outfile, filepos);
    if(retval > 0) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing everything up to keyword? Abort.

    sprintf(templine, "%s=%s\r\n", keyword, value);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    if(retval < i) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.

  } else {
    filepos += retval; // Position just before old version of keyword

    outfile = ActualFileOpenForWrite(outname);
    if(outfile == ACTUALHANDLENULL) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't open a temp file? Abort

    ActualFileSeek(infile, 0);
    retval = INICopy(infile, outfile, filepos);
    if(retval > 0) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing everything up to keyword? Abort.

    INIReadLine(infile, templine); // Read past old keyword/value...

    // Replace with new value
    sprintf(templine, "%s=%s\r\n", keyword, value);
    i = 0;
    while((i < INIMAXLEN) && (templine[i] != 0)) i++;
    retval = ActualFileWrite(outfile, i, templine);
    if(retval < i) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing it out? Abort.
  } // ENDIF- Need to add a new keyword?

  INICopy(infile, outfile, 0xFFFFFFF); // Write out rest of file
  ActualFileClose(infile);
  infile = ACTUALHANDLENULL;
  ActualFileClose(outfile);
  outfile = ACTUALHANDLENULL;
  ActualFileDelete(inname);
  ActualFileRename(outname, inname);
  return(0);
} // END INISaveString()


int INILoadString(const char *file, const char *section, const char *keyword, char *buffer) {
  char inname[INIMAXLEN+1];
  int filepos;
  ACTUALHANDLE infile;
  int retval;

  if(file == NULL)  return(-1);
  if(section == NULL)  return(-1);
  if(keyword == NULL)  return(-1);
  if(buffer == NULL)  return(-1);

  filepos = INIRemoveExt(file, inname);
  INIAddInExt(inname, filepos);

  filepos = 0;
  infile = ActualFileOpenForRead(inname);
  if(infile == ACTUALHANDLENULL)  return(-1);

  retval = INIFindSection(infile, section);
  if(retval < 0) {
    ActualFileClose(infile);
    infile = ACTUALHANDLENULL;
    return(-1);
  } // ENDIF- Didn't find it? Abort.

  retval = INIFindKeyword(infile, keyword, buffer);
  if(retval < 0) {
    ActualFileClose(infile);
    infile = ACTUALHANDLENULL;
    return(-1);
  } // ENDIF- Didn't find it? Abort.

  ActualFileClose(infile);
  infile = ACTUALHANDLENULL;
  return(0);
} // END INILoadString()


int INIRemove(const char *file, const char *section, const char *keyword) {
  char inname[INIMAXLEN+1];
  char outname[INIMAXLEN+1];
  int filepos;
  ACTUALHANDLE infile;
  ACTUALHANDLE outfile;
  char templine[INIMAXLEN+1];
  int i;
  int retval;

  if(file == NULL)  return(-1);
  if(section == NULL)  return(-1);


  filepos = INIRemoveExt(file, inname);
  for(i = 0; i <= filepos; i++)  outname[i] = inname[i];
  INIAddInExt(inname, filepos);
  INIAddOutExt(outname, filepos);

  infile = ActualFileOpenForRead(inname);
  if(infile == ACTUALHANDLENULL)  return(-1);

  retval = INIFindSection(infile, section);
  if(retval == -1) {
    ActualFileClose(infile);
    infile = ACTUALHANDLENULL;
    return(-1);
  } // ENDIF- Couldn't even find the section? Abort

  filepos = retval;
  if(keyword == NULL) {
    outfile = ActualFileOpenForWrite(outname);
    if(outfile == ACTUALHANDLENULL) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't open a temp file? Abort

    ActualFileSeek(infile, 0);
    retval = INICopy(infile, outfile, filepos);
    if(retval > 0) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing everything up to the section? Abort.

    templine[0] = 0;
    retval = 1;
    while((retval > 0) && (templine[0] != '[')) {
      retval = INIReadLine(infile, templine);
    } // ENDWHILE- Read to the start of the next section... or EOF.

    if(templine[0] == '[') {
      i = 0;
      while((i < INIMAXLEN) && (templine[i] != 0)) i++;
      retval = ActualFileWrite(outfile, i, templine);
      if(retval < i) {
        ActualFileClose(infile);
        infile = ACTUALHANDLENULL;
        ActualFileClose(outfile);
        outfile = ACTUALHANDLENULL;
        ActualFileDelete(outname);
        return(-1);
      } // ENDIF- Trouble writing it out? Abort.
    } // ENDIF- Are there other sections after this one? Save them then.

  } else {
    filepos = retval;
    ActualFileSeek(infile, filepos);
    filepos += INIReadLine(infile, templine); // Get section line's byte count

    retval = INIFindKeyword(infile, keyword, NULL);
    if(retval == -1) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't find the keyword? Abort
    filepos += retval;

    outfile = ActualFileOpenForWrite(outname);
    if(outfile == ACTUALHANDLENULL) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      return(-1);
    } // ENDIF- Couldn't open a temp file? Abort

    ActualFileSeek(infile, 0);
    retval = INICopy(infile, outfile, filepos);
    if(retval > 0) {
      ActualFileClose(infile);
      infile = ACTUALHANDLENULL;
      ActualFileClose(outfile);
      outfile = ACTUALHANDLENULL;
      ActualFileDelete(outname);
      return(-1);
    } // ENDIF- Trouble writing everything up to keyword? Abort.

    INIReadLine(infile, templine); // Read (and discard) the keyword line
  } // ENDIF- Wipe out the whole section? Or just a keyword?

  INICopy(infile, outfile, 0xFFFFFFF); // Write out rest of file
  ActualFileClose(infile);
  infile = ACTUALHANDLENULL;
  ActualFileClose(outfile);
  outfile = ACTUALHANDLENULL;
  ActualFileDelete(inname);
  ActualFileRename(outname, inname);
  return(0);
} // END INIRemove()


int INISaveUInt(const char *file, const char *section, const char *keyword, unsigned int value) {
  char numvalue[INIMAXLEN+1];

  sprintf(numvalue, "%u", value);
  return(INISaveString(file, section, keyword, numvalue));
} // END INISaveUInt()


int INILoadUInt(const char *file, const char *section, const char *keyword, unsigned int *buffer) {
  char numvalue[INIMAXLEN+1];
  int retval;
  unsigned int value;
  // unsigned int sign; // Not needed in unsigned numbers
  int pos;

  if(buffer == NULL)  return(-1);
  *(buffer) = 0;

  retval = INILoadString(file, section, keyword, numvalue);
  if(retval < 0)  return(retval);

  value = 0;
  // sign = 1; // Start positive
  pos = 0;

  // Note: skip leading spaces? (Shouldn't have to, I hope)

  // if(numvalue[pos] == '-') {
  //   pos++;
  //   sign = -1;
  // } // ENDIF- Negative sign check

  while((pos < INIMAXLEN) && (numvalue[pos] != 0)) {
    if(value > (0xFFFFFFFF / 10))  return(-1); // Overflow?

    if((numvalue[pos] >= '0') && (numvalue[pos] <= '9')) {
      value *= 10;
      value += numvalue[pos] - '0';
      pos++;
    } else {
      numvalue[pos] = 0;
    } // ENDIF- Add a digit in? Or stop searching for digits?
  } // ENDWHILE- Adding digits of info to our ever-increasing value

  // value *= sign
  *(buffer) = value;
  return(0);
} // END INILoadUInt()
