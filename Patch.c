/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//
// Includes
//
#include <stdlib.h>
#include <string.h>

#include "PsxCommon.h"

#include "windows/cheats/cheats.h"

#include "patch.h"

//
// Variables
//
PatchTextTable commands[] =
{
   { "comment", 1, patchFunc_comment },
   { "gametitle", 2, patchFunc_gametitle },
   { "patch", 3, patchFunc_patch },
   { "fastmemory", 4, patchFunc_fastmemory }, // enable for faster but bugger mem (mvc2 is faster)
   { "roundmode", 5, patchFunc_roundmode }, // changes rounding mode for floating point
											// syntax: roundmode=X,Y
											// possible values for X,Y: NEAR, DOWN, UP, CHOP
											// X - EE rounding mode (default is NEAR)
											// Y - VU rounding mode (default is CHOP)
   { "", 0, NULL }
};

PatchTextTable dataType[] =
{
   { "byte", 1, NULL },
   { "short", 2, NULL },
   { "word", 3, NULL },
   { "double", 4, NULL },
   { "", 0, NULL }
};

PatchTextTable cpuCore[] =
{
   { "EE", 1, NULL },
   { "IOP", 2, NULL },
   { "", 0, NULL }
};

IniPatch patch[ MAX_PATCH ];
int patchnumber;


//
// Function Implementations
//

int PatchTableExecute( char * text1, char * text2, PatchTextTable * Table )
{
   int i = 0;

   while ( Table[ i ].text[ 0 ] )
   {
      if ( !strcmp( Table[ i ].text, text1 ) )
      {
         if ( Table[ i ].func )
         {
            Table[ i ].func( text1, text2 );
         }
         break;
      }
      i++;
   }

   return Table[ i ].code;
}

void _applypatch(int place, IniPatch *p) {
	if (p->placetopatch != place) return;

	if (p->enabled == 0) return;

	if (p->cpu == 1) { //EE
		if (p->type == 1) { //byte
			memWrite8(p->addr, (u8)p->data);
		} else
		if (p->type == 2) { //short
			memWrite16(p->addr, (u16)p->data);
		} else
		if (p->type == 3) { //word
			memWrite32(p->addr, (u32)p->data);
		} else
		if (p->type == 4) { //double
			memWrite64(p->addr, p->data);
		}
	} else
	if (p->cpu == 2) { //IOP
		if (p->type == 1) { //byte
			psxMemWrite8(p->addr, (u8)p->data);
		} else
		if (p->type == 2) { //short
			psxMemWrite16(p->addr, (u16)p->data);
		} else
		if (p->type == 3) { //word
			psxMemWrite32(p->addr, (u32)p->data);
		}
	}
}

//this is for apply patches directly to memory
void applypatch(int place) {
	int i;

	if (place == 0) {
		SysPrintf(" patchnumber: %d\n", patchnumber);
	}

	for ( i = 0; i < patchnumber; i++ ) {
		_applypatch(place, &patch[i]);
	}
}

void patchFunc_comment( char * text1, char * text2 )
{
   SysPrintf( "comment: %s \n", text2 );
}

char strgametitle[256] = {0};

void patchFunc_gametitle( char * text1, char * text2 )
{
	SysPrintf( "gametitle: %s \n", text2 );
#ifdef __WIN32__
	sprintf(strgametitle,"%s",text2);
	if (gApp.hConsole) SetConsoleTitle(strgametitle);
#endif
}

void patchFunc_patch( char * cmd, char * param )
{
   //patch=placetopatch,cpucore,address,type,data 
   char * pText;

   if ( patchnumber >= MAX_PATCH )
   {
      SysPrintf( "Patch ERROR: Maximum number of patches reached: %s=%s\n", cmd, param );
      return;
   }

   pText = strtok( param, "," );
   pText = param;
//   inifile_trim( pText );

   patch[ patchnumber ].placetopatch   = strtol( pText, (char **)NULL, 0 );

   pText = strtok( NULL, "," );
   inifile_trim( pText );
   patch[ patchnumber ].cpu = PatchTableExecute( pText, NULL, cpuCore );
	if ( patch[ patchnumber ].cpu == 0 ) 
   {
		SysPrintf( "Unrecognized patch '%s'\n", pText );
      return;
	}

   pText = strtok( NULL, "," );
   inifile_trim( pText );
   sscanf( pText, "%X", &patch[ patchnumber ].addr );

   pText = strtok( NULL, "," );
   inifile_trim( pText );
   patch[ patchnumber ].type = PatchTableExecute( pText, NULL, dataType );
	if ( patch[ patchnumber ].type == 0 ) 
   {
      SysPrintf( "Unrecognized patch '%s'\n", pText );
      return;
   }

   pText = strtok( NULL, "," );
   inifile_trim( pText );
   sscanf( pText, "%I64X", &patch[ patchnumber ].data );

   patch[ patchnumber ].enabled = 1;

   patchnumber++;
}

//this routine is for execute the commands of the ini file
void inifile_command( char * cmd )
{
   int code;
   char command[ 256 ];
   char parameter[ 256 ];

   // extract param part (after '=')
   char * pEqual = strchr( cmd, '=' );

   if ( ! pEqual )
   {
	   // fastmemory doesn't have =
	   pEqual = cmd+strlen(cmd);
//      SysPrintf( "Ini file ERROR: unknow line: %s \n", cmd );
//      return;
   }

   memset( command, 0, sizeof( command ) );
   memset( parameter, 0, sizeof( parameter ) );
      
   strncpy( command, cmd, pEqual - cmd );
   strncpy( parameter, pEqual + 1, sizeof( parameter ) );

   inifile_trim( command );
   inifile_trim( parameter );

   code = PatchTableExecute( command, parameter, commands );
}

void inifile_trim( char * buffer )
{
   char * pInit = buffer;
   char * pEnd = NULL;

   while ( ( *pInit == ' ' ) || ( *pInit == '\t' ) ) //skip space
   {
      pInit++;
   }
   if ( ( pInit[ 0 ] == '/' ) && ( pInit[ 1 ] == '/' ) ) //remove comment
   {
      buffer[ 0 ] = '\0';
      return;
   }
   pEnd = pInit + strlen( pInit ) - 1;
   if ( pEnd <= pInit )
   {
      buffer[ 0 ] = '\0';
      return;
   }
   while ( ( *pEnd == '\r' ) || ( *pEnd == '\n' ) ||
           ( *pEnd == ' ' ) || ( *pEnd == '\t' ) )
   {
      pEnd--;
   }
   if ( pEnd <= pInit )
   {
      buffer[ 0 ] = '\0';
      return;
   }
   memmove( buffer, pInit, pEnd - pInit + 1 );
   buffer[ pEnd - pInit + 1 ] = '\0';
}

void inisection_process( FILE * f1 )
{
   char buffer[ 1024 ];
   while( fgets( buffer, sizeof( buffer ), f1 ) )
   {
      inifile_trim( buffer );
      if ( buffer[ 0 ] )
      {
         inifile_command( buffer );
      }
   }
}

//this routine is for reading the ini file

void inifile_read( char * name )
{
   FILE * f1;
   char buffer[ 1024 ];

   patchnumber = 0;
#ifdef __WIN32__
   sprintf( buffer, "patches\\%s.pnach", name );
#else
   sprintf( buffer, "patches/%s.pnach", name );
#endif

   f1 = fopen( buffer, "rt" );
   if( !f1 )
   {
      SysPrintf( _( "patch file for this game not found. Can't apply any patches\n" ) );
      return;
   }

   inisection_process( f1 );

   fclose( f1 );
}

void resetpatch( void )
{
   patchnumber = 0;
}

int AddPatch(int Mode, int Place, int Address, int Size, u64 data)
{

	if ( patchnumber >= MAX_PATCH )
	{
		SysPrintf( "Patch ERROR: Maximum number of patches reached.\n");
		return -1;
	}

	patch[patchnumber].placetopatch = Mode;
	patch[patchnumber].cpu = Place;
	patch[patchnumber].addr=Address;
	patch[patchnumber].type=Size;
	patch[patchnumber].data = data;
	return patchnumber++;
}

void patchFunc_fastmemory( char * cmd, char * param )
{
	SetFastMemory(1);
}

void patchFunc_roundmode( char * cmd, char * param )
{
	//roundmode = X,Y
	int index;
	char * pText;

	u32 eetype=0x0000;
	u32 vutype=0x6000;
	
	index = 0;
	pText = strtok( param, ", " );
	while(pText != NULL) {
		u32 type = 0xffff;
		if( stricmp(pText, "near") == 0 ) {
			type = 0x0000;
		}
		else if( stricmp(pText, "down") == 0 ) {
			type = 0x2000;
		}
		else if( stricmp(pText, "up") == 0 ) {
			type = 0x4000;
		}
		else if( stricmp(pText, "chop") == 0 ) {
			type = 0x6000;
		}

		if( type == 0xffff ) {
			printf("bad argument (%s) to round mode! skipping...\n", pText);
			break;
		}

		if( index == 0 ) eetype=type;
		else			 vutype=type;

		if( index == 1 )
			break;

		index++;
		pText = strtok(NULL, ", ");
	}

	SetRoundMode(eetype,vutype);
}

void SetRoundMode(u32 ee, u32 vu)
{
	g_sseMXCSR = 0x9f80|ee;
	g_sseVUMXCSR = 0x9f80|vu;

	SetCPUState();
}
