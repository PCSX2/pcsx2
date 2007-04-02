/*  Cpudetection lib 
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

#if defined (__WIN32__)

#include <windows.h>

#endif

#include <string.h>
#include <stdio.h>

#include "ix86.h"

#if defined (__VCNET2005__)

   void __cpuid(int* CPUInfo, int InfoType);
   unsigned __int64 __rdtsc();

   #pragma intrinsic(__cpuid)
   #pragma intrinsic(__rdtsc)

#endif

CAPABILITIES cpucaps;
CPUINFO cpuinfo;

static s32 iCpuId( u32 cmd, u32 *regs ) 
{
   int flag;

#if defined (__VCNET2005__)

   __cpuid( regs, cmd );

   return 0;

#elif defined (__MSCW32__) && !defined(__x86_64__)
   __asm 
   {
      push ebx;
      push edi;

      pushfd;
      pop eax;
      mov edx, eax;
      xor eax, 1 << 21;
      push eax;
      popfd;
      pushfd;
      pop eax;
      xor eax, edx;
      mov flag, eax;
   }
   if ( ! flag )
   {
      return -1;
   }

   __asm 
   {
      mov eax, cmd;
      cpuid;
      mov edi, [regs]
      mov [edi], eax;
      mov [edi+4], ebx;
      mov [edi+8], ecx;
      mov [edi+12], edx;

      pop edi;
      pop ebx;
   }

   return 0;


#else

   __asm__ __volatile__ (
#ifdef __x86_64__
	"sub $0x18, %%rsp\n"
#endif
      "pushf\n"
      "pop %%eax\n"
      "mov %%eax, %%edx\n"
      "xor $0x200000, %%eax\n"
      "push %%eax\n"
      "popf\n"
      "pushf\n"
      "pop %%eax\n"
      "xor %%edx, %%eax\n"
      "mov %%eax, %0\n"
#ifdef __x86_64__
	"add $0x18, %%rsp\n"
#endif
      : "=r"(flag) :
   );
   
   if ( ! flag )
   {
      return -1;
   }

   __asm__ __volatile__ (
      "mov %4, %%eax\n"
      "cpuid\n"
      "mov %%eax, %0\n"
      "mov %%ebx, %1\n"
      "mov %%ecx, %2\n"
      "mov %%edx, %3\n"
      : "=m" (regs[0]), "=m" (regs[1]),
        "=m" (regs[2]), "=m" (regs[3])
      : "m"(cmd)
      : "eax", "ebx", "ecx", "edx"
   );

   return 0;
#endif
}

u64 GetCPUTick( void ) 
{
#if defined (__VCNET2005__)

   return __rdtsc();

#elif defined(__MSCW32__) && !defined(__x86_64__)

   __asm rdtsc;

#else

   u32 _a, _d;
	__asm__ __volatile__ ("rdtsc" : "=a"(_a), "=d"(_d));
	return (u64)_a | ((u64)_d << 32);

#endif
}

#if defined __LINUX__

#include <sys/time.h>
#include <errno.h>

u32 timeGetTime( void ) 
{
	struct timeval tv;
	gettimeofday( &tv, 0 );
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#endif

s64 CPUSpeedHz( unsigned int time )
{
   s64 timeStart, 
            timeStop;
   s64 startTick, 
            endTick;
   s64 overhead;

   if( ! cpucaps.hasTimeStampCounter )
   {
      return 0; //check if function is supported
   }

	overhead = GetCPUTick() - GetCPUTick();
	
	timeStart = timeGetTime( );
	while( timeGetTime( ) == timeStart ) 
   {
      timeStart = timeGetTime( );
   }
	for(;;)
	{
		timeStop = timeGetTime( );
		if ( ( timeStop - timeStart ) > 1 )	
		{
			startTick = GetCPUTick( );
			break;
		}
	}

	timeStart = timeStop;
	for(;;)
	{
		timeStop = timeGetTime( );
		if ( ( timeStop - timeStart ) > time )	
		{
			endTick = GetCPUTick( );
			break;
		}
	}

	return (s64)( ( endTick - startTick ) + ( overhead ) );
}

////////////////////////////////////////////////////
void cpudetectInit( void ) 
{
   u32 regs[ 4 ];
   u32 cmds;
   u32 AMDspeed;
   s8 AMDspeedString[10];
   int cputype=0;            // Cpu type
   //AMD 64 STUFF
   u32 x86_64_8BITBRANDID;
   u32 x86_64_12BITBRANDID; 
   memset( cpuinfo.x86ID, 0, sizeof( cpuinfo.x86ID ) );
   cpuinfo.x86Family = 0;
   cpuinfo.x86Model  = 0;
   cpuinfo.x86PType  = 0;
   cpuinfo.x86StepID = 0;
   cpuinfo.x86Flags  = 0;
   cpuinfo.x86EFlags = 0;
   
   if ( iCpuId( 0, regs ) == -1 ) return;

   cmds = regs[ 0 ];
   ((u32*)cpuinfo.x86ID)[ 0 ] = regs[ 1 ];
   ((u32*)cpuinfo.x86ID)[ 1 ] = regs[ 3 ];
   ((u32*)cpuinfo.x86ID)[ 2 ] = regs[ 2 ];
   if ( cmds >= 0x00000001 ) 
   {
      if ( iCpuId( 0x00000001, regs ) != -1 )
      {
         cpuinfo.x86StepID =  regs[ 0 ]        & 0xf;
         cpuinfo.x86Model  = (regs[ 0 ] >>  4) & 0xf;
         cpuinfo.x86Family = (regs[ 0 ] >>  8) & 0xf;
         cpuinfo.x86PType  = (regs[ 0 ] >> 12) & 0x3;
         x86_64_8BITBRANDID = regs[1] & 0xff;
         cpuinfo.x86Flags  =  regs[ 3 ];
      }
   }
   if ( iCpuId( 0x80000000, regs ) != -1 )
   {
      cmds = regs[ 0 ];
      if ( cmds >= 0x80000001 ) 
      {
		 if ( iCpuId( 0x80000001, regs ) != -1 )
         {
			x86_64_12BITBRANDID = regs[1] & 0xfff;
            cpuinfo.x86EFlags = regs[ 3 ];
            
         }
      }
   }
   switch(cpuinfo.x86PType)
   {
      case 0:
         strcpy( cpuinfo.x86Type, "Standard OEM");
         break;
      case 1:
         strcpy( cpuinfo.x86Type, "Overdrive");
         break;
      case 2:
         strcpy( cpuinfo.x86Type, "Dual");
         break;
      case 3:
         strcpy( cpuinfo.x86Type, "Reserved");
         break;
      default:
         strcpy( cpuinfo.x86Type, "Unknown");
         break;
   }
   if ( cpuinfo.x86ID[ 0 ] == 'G' ){ cputype=0;}//trick lines but if you know a way better ;p
   if ( cpuinfo.x86ID[ 0 ] == 'A' ){ cputype=1;}
   
   if ( cputype == 0 ) //intel cpu
   {
      if( ( cpuinfo.x86Family >= 7 ) && ( cpuinfo.x86Family < 15 ) )
      {
         strcpy( cpuinfo.x86Fam, "Intel P6 family (Not PIV and Higher then PPro" );
      }
      else
      {
         switch( cpuinfo.x86Family )
         {     
            // Start at 486 because if it's below 486 there is no cpuid instruction
            case 4:
               strcpy( cpuinfo.x86Fam, "Intel 486" );
               break;
            case 5:     
               switch( cpuinfo.x86Model )
               {
               case 4:
               case 8:     // 0.25 µm
                  strcpy( cpuinfo.x86Fam, "Intel Pentium (MMX)");
                  break;
               default:
                  strcpy( cpuinfo.x86Fam, "Intel Pentium" );
               }
               break;
            case 6:     
               switch( cpuinfo.x86Model )
               {
               case 0:     // Pentium pro (P6 A-Step)
               case 1:     // Pentium pro
                  strcpy( cpuinfo.x86Fam, "Intel Pentium Pro" );
                  break;

               case 2:     // 66 MHz FSB
               case 5:     // Xeon/Celeron (0.25 µm)
               case 6:     // Internal L2 cache
                  strcpy( cpuinfo.x86Fam, "Intel Pentium II" );
                  break;

               case 7:     // Xeon external L2 cache
               case 8:     // Xeon/Celeron with 256 KB on-die L2 cache
               case 10:    // Xeon/Celeron with 1 or 2 MB on-die L2 cache
               case 11:    // Xeon/Celeron with Tualatin core, on-die cache
                  strcpy( cpuinfo.x86Fam, "Intel Pentium III" );
                  break;

               default:
                  strcpy( cpuinfo.x86Fam, "Intel Pentium Pro (Unknown)" );
               }
               break;
            case 15:
               switch( cpuinfo.x86Model )
               {
               case 0:     // Willamette (A-Step)
               case 1:     // Willamette 
                  strcpy( cpuinfo.x86Fam, "Willamette Intel Pentium IV" );
                  break;
               case 2:     // Northwood 
                  strcpy( cpuinfo.x86Fam, "Northwood Intel Pentium IV" );
                  break;

               default:
                  strcpy( cpuinfo.x86Fam, "Intel Pentium IV (Unknown)" );
                  break;
               }
               break;
            default:
               strcpy( cpuinfo.x86Fam, "Unknown Intel CPU" );
         }
      }
   }
   else if ( cputype == 1 ) //AMD cpu
   {
      if( cpuinfo.x86Family >= 7 )
      {
		  if((x86_64_12BITBRANDID !=0) || (x86_64_8BITBRANDID !=0))
		  {
		    if(x86_64_8BITBRANDID == 0 )
		    {
               switch((x86_64_12BITBRANDID >>6)& 0x3f)
			   {
			    case 4:
				 strcpy(cpuinfo.x86Fam,"AMD Athlon(tm) 64 Processor");
                 AMDspeed = 22 + (x86_64_12BITBRANDID & 0x1f);
				 //AMDspeedString = strtol(AMDspeed, (char**)NULL,10);
				 sprintf(AMDspeedString," %d",AMDspeed);
				 strcat(AMDspeedString,"00+");
				 strcat(cpuinfo.x86Fam,AMDspeedString);
				 break;
			    case 12: 
				 strcpy(cpuinfo.x86Fam,"AMD Opteron(tm) Processor");
				 break;
			    default:
				   strcpy(cpuinfo.x86Fam,"Unknown AMD 64 proccesor");
				   
			    }
		     }
		     else //8bit brand id is non zero
		     {
                strcpy(cpuinfo.x86Fam,"Unsupported yet AMD64 cpu");
		     }
		  }
		  else
		  {		 
			  strcpy( cpuinfo.x86Fam, "AMD K7+" );
		  }
      }
      else
      {
         switch ( cpuinfo.x86Family )
         {
            case 4:
               switch( cpuinfo.x86Model )
               {
               case 14: 
               case 15:       // Write-back enhanced
                  strcpy( cpuinfo.x86Fam, "AMD 5x86" );
                  break;

               case 3:        // DX2
               case 7:        // Write-back enhanced DX2
               case 8:        // DX4
               case 9:        // Write-back enhanced DX4
                  strcpy( cpuinfo.x86Fam, "AMD 486" );
                  break;

               default:
                  strcpy( cpuinfo.x86Fam, "AMD Unknown" );

               }
               break;

            case 5:     
               switch( cpuinfo.x86Model)
               {
               case 0:     // SSA 5 (75, 90 and 100 Mhz)
               case 1:     // 5k86 (PR 120 and 133 MHz)
               case 2:     // 5k86 (PR 166 MHz)
               case 3:     // K5 5k86 (PR 200 MHz)
                  strcpy( cpuinfo.x86Fam, "AMD K5" );
                  break;

               case 6:     
               case 7:     // (0.25 µm)
               case 8:     // K6-2
               case 9:     // K6-III
               case 14:    // K6-2+ / K6-III+
                  strcpy( cpuinfo.x86Fam, "AMD K6" );
                  break;

               default:
                  strcpy( cpuinfo.x86Fam, "AMD Unknown" );
               }
               break;
            case 6:     
               strcpy( cpuinfo.x86Fam, "AMD K7" );
               break;
            default:
               strcpy( cpuinfo.x86Fam, "Unknown AMD CPU" ); 
         }
      }
   }
   //capabilities
   cpucaps.hasFloatingPointUnit                         = ( cpuinfo.x86Flags >>  0 ) & 1;
   cpucaps.hasVirtual8086ModeEnhancements               = ( cpuinfo.x86Flags >>  1 ) & 1;
   cpucaps.hasDebuggingExtensions                       = ( cpuinfo.x86Flags >>  2 ) & 1;
   cpucaps.hasPageSizeExtensions                        = ( cpuinfo.x86Flags >>  3 ) & 1;
   cpucaps.hasTimeStampCounter                          = ( cpuinfo.x86Flags >>  4 ) & 1;
   cpucaps.hasModelSpecificRegisters                    = ( cpuinfo.x86Flags >>  5 ) & 1;
   cpucaps.hasPhysicalAddressExtension                  = ( cpuinfo.x86Flags >>  6 ) & 1;
   cpucaps.hasMachineCheckArchitecture                  = ( cpuinfo.x86Flags >>  7 ) & 1;
   cpucaps.hasCOMPXCHG8BInstruction                     = ( cpuinfo.x86Flags >>  8 ) & 1;
   cpucaps.hasAdvancedProgrammableInterruptController   = ( cpuinfo.x86Flags >>  9 ) & 1;
   cpucaps.hasSEPFastSystemCall                         = ( cpuinfo.x86Flags >> 11 ) & 1;
   cpucaps.hasMemoryTypeRangeRegisters                  = ( cpuinfo.x86Flags >> 12 ) & 1;
   cpucaps.hasPTEGlobalFlag                             = ( cpuinfo.x86Flags >> 13 ) & 1;
   cpucaps.hasMachineCheckArchitecture                  = ( cpuinfo.x86Flags >> 14 ) & 1;
   cpucaps.hasConditionalMoveAndCompareInstructions     = ( cpuinfo.x86Flags >> 15 ) & 1;
   cpucaps.hasFGPageAttributeTable                      = ( cpuinfo.x86Flags >> 16 ) & 1;
   cpucaps.has36bitPageSizeExtension                    = ( cpuinfo.x86Flags >> 17 ) & 1;
   cpucaps.hasProcessorSerialNumber                     = ( cpuinfo.x86Flags >> 18 ) & 1;
   cpucaps.hasCFLUSHInstruction                         = ( cpuinfo.x86Flags >> 19 ) & 1;
   cpucaps.hasDebugStore                                = ( cpuinfo.x86Flags >> 21 ) & 1;
   cpucaps.hasACPIThermalMonitorAndClockControl         = ( cpuinfo.x86Flags >> 22 ) & 1;
   cpucaps.hasMultimediaExtensions                      = ( cpuinfo.x86Flags >> 23 ) & 1; //mmx
   cpucaps.hasFastStreamingSIMDExtensionsSaveRestore    = ( cpuinfo.x86Flags >> 24 ) & 1;
   cpucaps.hasStreamingSIMDExtensions                   = ( cpuinfo.x86Flags >> 25 ) & 1; //sse
   cpucaps.hasStreamingSIMD2Extensions                  = ( cpuinfo.x86Flags >> 26 ) & 1; //sse2
   cpucaps.hasSelfSnoop                                 = ( cpuinfo.x86Flags >> 27 ) & 1;
   cpucaps.hasHyperThreading                            = ( cpuinfo.x86Flags >> 28 ) & 1;
   cpucaps.hasThermalMonitor                            = ( cpuinfo.x86Flags >> 29 ) & 1;
   cpucaps.hasIntel64BitArchitecture                    = ( cpuinfo.x86Flags >> 30 ) & 1;
    //that is only for AMDs
   cpucaps.hasMultimediaExtensionsExt                   = ( cpuinfo.x86EFlags >> 22 ) & 1; //mmx2
   cpucaps.hasAMD64BitArchitecture                      = ( cpuinfo.x86EFlags >> 29 ) & 1; //64bit cpu
   cpucaps.has3DNOWInstructionExtensionsExt             = ( cpuinfo.x86EFlags >> 30 ) & 1; //3dnow+
   cpucaps.has3DNOWInstructionExtensions                = ( cpuinfo.x86EFlags >> 31 ) & 1; //3dnow   
   cpuinfo.cpuspeed = (u32 )(CPUSpeedHz( 1000 ) / 1000000);
}
