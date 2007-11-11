// stop compiling if NORECBUILD build (only for Visual Studio)
#if !(defined(_MSC_VER) && defined(PCSX2_NORECBUILD))

#include "ix86.h"

/**********************/
/* 3DNOW instructions */
/**********************/

/* femms */
void FEMMS( void ) 
{
	write16( 0x0E0F );
}

void PFCMPEQMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0xB0 );
}

void PFCMPGTMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0xA0 );
}

void PFCMPGEMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x90 );
}

void PFADDMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x9E );
}

void PFADDRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from );
	write8( 0x9E );
}

void PFSUBMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x9A );
}

void PFSUBRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0x9A );
}

void PFMULMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0xB4 );
}

void PFMULRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0xB4 );
}

void PFRCPMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x96 );
}

void PFRCPRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0x96 );
}

void PFRCPIT1RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0xA6 );
}

void PFRCPIT2RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0xB6 );
}

void PFRSQRTRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0x97 );
}

void PFRSQIT1RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0xA7 );
}

void PF2IDMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x1D );
}

void PF2IDRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0x1D );
}

void PI2FDMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x0D );
}

void PI2FDRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0x0D );
}

void PFMAXMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0xA4 );
}

void PFMAXRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from ); 
	write8( 0xA4 );
}

void PFMINMtoR( x86IntRegType to, uptr from )
{
	write16( 0x0F0F );
	ModRM( 0, to, DISP32 ); 
	write32( from ); 
	write8( 0x94 );
}

void PFMINRtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0F0F );
	ModRM( 3, to, from );
	write8( 0x94 );
}

#endif
