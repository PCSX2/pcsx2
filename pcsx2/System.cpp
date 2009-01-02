/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "PsxCommon.h"
#include "Threading.h"

#include "x86/ix86/ix86.h"

using namespace std;
using namespace Console;

bool sysInitialized = false;


// I can't believe I had to make my own version of trim.  C++'s STL is totally whack.
// And I still had to fix it too.  I found three samples of trim online and *all* three
// were buggy.  People really need to learn to code before they start posting trim
// functions in their blogs.  (air)

static void trim( string& line )
{
   if ( line.empty() )
      return;

   int string_size = line.length();
   int beginning_of_string = 0;
   int end_of_string = string_size - 1;
   
   bool encountered_characters = false;
   
   // find the start of chracters in the string
   while ( (beginning_of_string < string_size) && (!encountered_characters) )
   {
      if ( (line[ beginning_of_string ] != ' ') && (line[ beginning_of_string ] != '\t') )
         encountered_characters = true;
      else
         ++beginning_of_string;
   }

   // test if no characters were found in the string
   if ( beginning_of_string == string_size )
      return;
   
   encountered_characters = false;

   // find the character in the string
   while ( (end_of_string > beginning_of_string) && (!encountered_characters) )
   {
      // if a space or tab was found then ignore it
      if ( (line[ end_of_string ] != ' ') && (line[ end_of_string ] != '\t') )
         encountered_characters = true;
      else
         --end_of_string;
   }   
   
   // return the original string with all whitespace removed from its beginning and end
   // + 1 at the end to add the space for the string delimiter
   //line.substr( beginning_of_string, end_of_string - beginning_of_string + 1 );
   line.erase( end_of_string+1, string_size );
   line.erase( 0, beginning_of_string );
}


// This function should be called once during program execution.
void SysDetect()
{
	if( sysInitialized ) return;
	sysInitialized = true;

	Notice("PCSX2 " PCSX2_VERSION " - compiled on " __DATE__ );
	Notice("Savestate version: %x", g_SaveVersion);

	// fixme: what's the point of this line?  Anything? Or just to look "cool"? (air)
	DevCon::Notice("EE pc offset: 0x%x, IOP pc offset: 0x%x", (u32)&cpuRegs.pc - (u32)&cpuRegs, (u32)&psxRegs.pc - (u32)&psxRegs);

	cpudetectInit();

	string family( cpuinfo.x86Fam );
	trim( family );

	SetColor( Console::Color_White );

	MsgLn( "x86Init:" );
	WriteLn(
		"\tCPU vendor name =  %s\n"
		"\tFamilyID  =  %x\n"
		"\tx86Family =  %s\n"
		"\tCPU speed =  %d.%03d Ghz\n"
		"\tCores     =  %d physical [%d logical]\n"
		"\tx86PType  =  %s\n"
		"\tx86Flags  =  %8.8x %8.8x\n"
		"\tx86EFlags =  %8.8x\n",
		cpuinfo.x86ID, cpuinfo.x86StepID, family.c_str(), 
			cpuinfo.cpuspeed / 1000, cpuinfo.cpuspeed%1000,
			cpuinfo.PhysicalCores, cpuinfo.LogicalCores,
			cpuinfo.x86Type, cpuinfo.x86Flags, cpuinfo.x86Flags2,
			cpuinfo.x86EFlags
	);

	MsgLn( "Features:" );
	WriteLn(
		"\t%sDetected MMX\n"
		"\t%sDetected SSE\n"
		"\t%sDetected SSE2\n"
		"\t%sDetected SSE3\n"
		"\t%sDetected SSE4.1\n",
			cpucaps.hasMultimediaExtensions     ? "" : "Not ",
			cpucaps.hasStreamingSIMDExtensions  ? "" : "Not ",
			cpucaps.hasStreamingSIMD2Extensions ? "" : "Not ",
			cpucaps.hasStreamingSIMD3Extensions ? "" : "Not ",
			cpucaps.hasStreamingSIMD4Extensions ? "" : "Not "
	);

	if ( cpuinfo.x86ID[0] == 'A' ) //AMD cpu
	{
		MsgLn( " Extended AMD Features:" );
		WriteLn(
			"\t%sDetected MMX2\n"
			"\t%sDetected 3DNOW\n"
			"\t%sDetected 3DNOW2\n",
			cpucaps.hasMultimediaExtensionsExt       ? "" : "Not ",
			cpucaps.has3DNOWInstructionExtensions    ? "" : "Not ",
			cpucaps.has3DNOWInstructionExtensionsExt ? "" : "Not "
		);
	}

	Console::ClearColor();
}