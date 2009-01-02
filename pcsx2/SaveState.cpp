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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "PsxCommon.h"
#include "CDVDisodrv.h"
#include "VUmicro.h"

#include "VU.h"
#include "iCore.h"
#include "iVUzerorec.h"

#include "GS.h"
#include "COP0.h"
#include "Cache.h"

#include "Paths.h"

extern u32 s_iLastCOP0Cycle;
extern u32 s_iLastPERFCycle[2];
extern int g_psxWriteOk;

// STATES

static void PreLoadPrep()
{
#ifdef PCSX2_VIRTUAL_MEM
	DWORD OldProtect;
	// make sure can write
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READWRITE, &OldProtect);
#endif
}

static void PostLoadPrep()
{
#ifdef PCSX2_VIRTUAL_MEM
	DWORD OldProtect;
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READONLY, &OldProtect);
#endif

	memset(pCache,0,sizeof(_cacheS)*64);
	WriteCP0Status(cpuRegs.CP0.n.Status.val);
	for(int i=0; i<48; i++) MapTLB(i);
}

void SaveState::GetFilename( char* dest, int slot )
{
	char elfcrcText[72];
	sprintf( elfcrcText, "%8.8X.%3.3d", ElfCRC, slot );
	CombinePaths( dest, SSTATES_DIR, elfcrcText );
}

SaveState::SaveState( const char* msg, const char* destination ) :
 m_version( g_SaveVersion )
{
	Console::WriteLn( "%s %s", msg, destination );
}

void SaveState::FreezeAll()
{
	if( IsLoading() )
		PreLoadPrep();

	FreezeMem(PS2MEM_BASE, Ps2MemSize::Base);	// 32 MB main memory   
	FreezeMem(PS2MEM_ROM, Ps2MemSize::Rom);		// 4 mb rom memory
	FreezeMem(PS2MEM_ROM1, Ps2MemSize::Rom1);	// 256kb rom1 memory
	FreezeMem(PS2MEM_SCRATCH, Ps2MemSize::Scratch);	// scratch pad 
	FreezeMem(PS2MEM_HW, 0x00010000);			// hardware memory

	Freeze(cpuRegs);   // cpu regs + COP0
	Freeze(psxRegs);   // iop regs
	Freeze(fpuRegs);   // fpu regs
	Freeze(tlb);           // tlbs

	Freeze(EEsCycle);
	Freeze(EEoCycle);
	Freeze(psxRegs.cycle);		// used to be IOPoCycle.  This retains compatibility.
	Freeze(g_nextBranchCycle);
	Freeze(g_psxNextBranchCycle);

	Freeze(s_iLastCOP0Cycle);
	if( m_version >= 0x7a30000e )
		Freeze(s_iLastPERFCycle);

	Freeze(g_psxWriteOk);

	//hope didn't forgot any cpu....

	rcntFreeze();
	gsFreeze();
	vu0Freeze();
	vu1Freeze();
	vif0Freeze();
	vif1Freeze();
	sifFreeze();
	ipuFreeze();

	// iop now
	FreezeMem(psxM, 0x00200000);        // 2 MB main memory
	FreezeMem(psxH, 0x00010000);        // hardware memory
	//FreezeMem(psxS, 0x00010000);        // sif memory	

	sioFreeze();
	cdrFreeze();
	cdvdFreeze();
	psxRcntFreeze();
	sio2Freeze();

    if( mtgsThread != NULL ) {
        // have to call in thread, otherwise weird stuff will start happening
        mtgsThread->SendPointerPacket( GS_RINGTYPE_SAVE, 0, this );
        mtgsWaitGS();
    }
    else {
        FreezePlugin( "GS", GSfreeze );
    }
	FreezePlugin( "SPU2", SPU2freeze );
	FreezePlugin( "DEV9", DEV9freeze );
	FreezePlugin( "USB", USBfreeze );

	if( IsLoading() )
		PostLoadPrep();
}

/////////////////////////////////////////////////////////////////////////////
// gzipped to/from disk state saves implementation

gzBaseStateInfo::gzBaseStateInfo( const char* msg, const char* filename ) :
  SaveState( msg, filename )
, m_filename( filename )
, m_file( NULL )
{
}

gzBaseStateInfo::~gzBaseStateInfo()
{
	if( m_file != NULL )
	{
		gzclose( m_file );
		m_file = NULL;
	}
}


gzSavingState::gzSavingState( const char* filename ) :
  gzBaseStateInfo( _("Saving state to: "), filename )
{
	m_file = gzopen(filename, "wb");
	if( m_file == NULL )
		throw Exception::FileNotFound();

	Freeze( m_version );
}


gzLoadingState::gzLoadingState( const char* filename ) :
  gzBaseStateInfo( _("Loading state from: "), filename )
{
	m_file = gzopen(filename, "rb");
	if( m_file == NULL )
		throw Exception::FileNotFound();

	gzread( m_file, &m_version, 4 );

	if( m_version != g_SaveVersion )
	{
#ifdef PCSX2_VIRTUAL_MEM
		if( m_version >= 0x8b400000 )
		{
			Console::Error(
				"Savestate load aborted:\n"
				"\tVM edition cannot safely load savestates created by the VTLB edition."
			);
			throw Exception::UnsupportedStateVersion();
		}
		// pcsx2 vm supports opening these formats
		if( m_version < 0x7a30000d )
		{
			Console::WriteLn( "Unsupported or unrecognized savestate version: %x.", m_version );
			throw Exception::UnsupportedStateVersion();
		}
#else
		if( ( m_version >> 16 ) == 0x7a30 )
		{
			Console::Error(
				"Savestate load aborted:\n"
				"\tVTLB edition cannot safely load savestates created by the VM edition." );
			throw Exception::UnsupportedStateVersion();
		}
#endif
	}
}

gzLoadingState::~gzLoadingState() { }


void gzSavingState::FreezeMem( void* data, int size )
{
	gzwrite( m_file, data, size );
}

void gzLoadingState::FreezeMem( void* data, int size )
{
	gzread( m_file, data, size );
	if( gzeof( m_file ) )
		throw Exception::BadSavedState( m_filename );
}

void gzSavingState::FreezePlugin( const char* name, s32 (CALLBACK *freezer)(int mode, freezeData *data) )
{
	Console::WriteLn( "\tSaving %s", name );
	freezeData fP = { 0, NULL };

	if (freezer(FREEZE_SIZE, &fP) == -1)
		throw Exception::FreezePluginFailure( name, "saving" );

	gzwrite(m_file, &fP.size, sizeof(fP.size));
	if( fP.size == 0 ) return;

	fP.data = (s8*)malloc(fP.size);
	if (fP.data == NULL)
		throw Exception::OutOfMemory();

	if(freezer(FREEZE_SAVE, &fP) == -1)
		throw Exception::FreezePluginFailure( name, "saving" );

	if (fP.size)
	{
		gzwrite(m_file, fP.data, fP.size);
		free(fP.data);
	}
}

void gzLoadingState::FreezePlugin( const char* name, s32 (CALLBACK *freezer)(int mode, freezeData *data) )
{
	freezeData fP = { 0, NULL };
	Console::WriteLn( "\tLoading %s", name );

	gzread(m_file, &fP.size, sizeof(fP.size));
	if( fP.size == 0 ) return;

	fP.data = (s8*)malloc(fP.size);
	if (fP.data == NULL)
		throw Exception::OutOfMemory();
	gzread(m_file, fP.data, fP.size);

	if( gzeof( m_file ) )
		throw Exception::BadSavedState( m_filename );

	if(freezer(FREEZE_LOAD, &fP) == -1)
		throw Exception::FreezePluginFailure( name, "loading" );

	if (fP.size) free(fP.data);
}

//////////////////////////////////////////////////////////////////////////////////
// uncompressed to/from memory state saves implementation

memBaseStateInfo::memBaseStateInfo( MemoryAlloc<u8>& memblock, const char* msg ) :
  SaveState( msg, "Memory" )
, m_memory( memblock )
, m_idx( 0 )
{
}

memSavingState::memSavingState( MemoryAlloc<u8>& save_to ) : memBaseStateInfo( save_to, _("Saving state to: ") )
{
	save_to.ChunkSize = ReallocThreshold;
	save_to.MakeRoomFor( MemoryBaseAllocSize );
}

// Saving of state data to a memory buffer
void memSavingState::FreezeMem( void* data, int size )
{
	const int end = m_idx+size;
	m_memory.MakeRoomFor( end );

	u8* dest = (u8*)m_memory.GetPtr();
	const u8* src = (u8*)data;

	for( ; m_idx<end; ++m_idx, ++src )
		dest[m_idx] = *src;
}

memLoadingState::memLoadingState(MemoryAlloc<u8>& load_from ) : 
	memBaseStateInfo( load_from, _("Loading state from: ") )
{
}

memLoadingState::~memLoadingState() { }

// Loading of state data from a memory buffer...
void memLoadingState::FreezeMem( void* data, int size )
{
	const int end = m_idx+size;
	const u8* src = (u8*)m_memory.GetPtr();
	u8* dest = (u8*)data;

	for( ; m_idx<end; ++m_idx, ++dest )
		*dest = src[m_idx];
}

void memSavingState::FreezePlugin( const char* name, s32 (CALLBACK *freezer)(int mode, freezeData *data) )
{
	freezeData fP = { 0, NULL };
	Console::WriteLn( "\tSaving %s", name );

	if( freezer(FREEZE_SIZE, &fP) == -1 )
		throw Exception::FreezePluginFailure( name, "saving" );

	Freeze( fP.size );
	if( fP.size == 0 ) return;

	const int end = m_idx+fP.size;
	m_memory.MakeRoomFor( end );

	fP.data = ((s8*)m_memory.GetPtr()) + m_idx;
	if(freezer(FREEZE_SAVE, &fP) == -1)
		throw Exception::FreezePluginFailure( name, "saving" );

	m_idx += fP.size;
}

void memLoadingState::FreezePlugin( const char* name, s32 (CALLBACK *freezer)(int mode, freezeData *data) )
{
	freezeData fP;
	Console::WriteLn( "\tLoading %s", name );

	Freeze( fP.size );
	if( fP.size == 0 ) return;

	if( ( fP.size + m_idx ) > m_memory.GetSizeInBytes() )
	{
		assert(0);
		throw Exception::BadSavedState( "memory" );
	}

	fP.data = ((s8*)m_memory.GetPtr()) + m_idx;
	if(freezer(FREEZE_LOAD, &fP) == -1)
		throw Exception::FreezePluginFailure( name, "loading" );

	m_idx += fP.size;
}
