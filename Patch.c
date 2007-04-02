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

//
// Defines
//
#define MAX_PATCH 1024 

#define IFIS(x,str) if(!strnicmp(x,str,sizeof(str)-1))

#define GETNEXT_PARAM() \
	while ( *param && ( *param != ',' ) ) param++; \
   if ( *param ) param++; \
	while ( *param && ( *param == ' ' ) ) param++; \
	if ( *param == 0 ) { SysPrintf( _( "Not enough params for inicommand\n" ) ); return; }

//
// Typedefs
//
typedef void (*PATCHTABLEFUNC)( char * text1, char * text2 );

typedef struct
{
   char           * text;
   int            code;
   PATCHTABLEFUNC func;
} PatchTextTable;

typedef struct
{
   int    type;
   int    cpu;
   int    placetopatch;
   u32    addr;
   u32    data;
} IniPatch;

//
// Function prototypes
//
void patchFunc_comment( char * text1, char * text2 );
void patchFunc_gametitle( char * text1, char * text2 );
void patchFunc_patch( char * text1, char * text2 );

void inifile_trim( char * buffer );

//
// Variables
//
PatchTextTable commands[] =
{
   { "comment", 1, patchFunc_comment },
   { "gametitle", 2, patchFunc_gametitle },
   { "patch", 3, patchFunc_patch },
   { "", 0, NULL }
};

PatchTextTable dataType[] =
{
   { "byte", 1, NULL },
   { "word", 2, NULL },
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

	if (p->cpu == 1) { //EE
		if (p->type == 1) { //byte
			memWrite8(p->addr, p->data);
		} else
		if (p->type == 2) { //word
			memWrite32(p->addr, p->data);
		}
	} else
	if (p->cpu == 2) { //IOP
		if (p->type == 1) { //byte
			psxMemWrite8(p->addr, p->data);
		} else
		if (p->type == 2) { //word
			psxMemWrite32(p->addr, p->data);
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

void patchFunc_gametitle( char * text1, char * text2 )
{
	char str2[256];
    SysPrintf( "gametitle: %s \n", text2 );
#ifdef __WIN32__
	sprintf(str2,"Running Game       %s",text2);
	if (gApp.hConsole) SetConsoleTitle(str2);
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
   sscanf( pText, "%X", &patch[ patchnumber ].data );

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
      SysPrintf( "Ini file ERROR: unknow line: %s \n", cmd );
      return;
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
