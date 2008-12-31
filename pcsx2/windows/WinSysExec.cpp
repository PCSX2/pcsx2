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

// NOTE : This file was created to separate code that uses C++ exceptions from the
// WinMain procedure in WinMain.cpp.  Apparently MSVC handles not so well the idea
// of both __try/__except and try/catch in the same module.  (yup, I ran into a 
// dreaded compiler bug).

#include "PrecompiledHeader.h"
#include "win32.h"

#include <winnt.h>
#include <commctrl.h>

#include "Common.h"
#include "PsxCommon.h"

int UseGui = 1;
int nDisableSC = 0; // screensaver

MemoryAlloc<u8>* g_RecoveryState = NULL;

bool m_ReturnToGame = false;		// set to exit the RunGui message pump
bool g_GameInProgress = false;	// Set TRUE if a game is actively running.

// This instance is not modified by command line overrides so
// that command line plugins and stuff won't be saved into the
// user's conf file accidentally.
PcsxConfig winConfig;		// local storage of the configuration options.

HWND hStatusWnd = NULL;
AppData gApp;

const char* g_pRunGSState = NULL;


#define CmdSwitchIs( text ) ( stricmp( command, text ) == 0 )

// returns 1 if the user requested help (show help and exit)
// returns zero on success.
// returns -1 on failure (bad command line argument)
int ParseCommandLine( int tokenCount, TCHAR *const *const tokens )
{
	int tidx = 0;
	g_TestRun.efile = 0;

	while( tidx < tokenCount )
	{
		const TCHAR* command = tokens[tidx++];

		if( command[0] != '-' )
		{
			g_TestRun.ptitle = command;
            printf("opening file %s\n", command);
			continue;
		}

		// jump past the '-' switch char, and skip if this is a dud switch:
		command++;
		if( command[0] == 0 ) continue;

		if( CmdSwitchIs( "help" ) )
		{
			return -1;
		}
        else if( CmdSwitchIs( "nogui" ) ) {
			UseGui = 0;
		}
#ifdef PCSX2_DEVBUILD
			else if( CmdSwitchIs( "jpg" ) ) {
				g_TestRun.jpgcapture = 1;
			}
#endif
		else
		{
			const TCHAR* param;
			if( tidx >= tokenCount ) break;

			// None of the parameter-less toggles flagged.
			// Check for switches that require one or more parameters here:

			param = tokens[tidx++];

			if( CmdSwitchIs( "cfg" ) ) {
				g_CustomConfigFile = param;
			}

			else if( CmdSwitchIs( "efile" ) ) {
				g_TestRun.efile = !!atoi( param );
			}
			else if( CmdSwitchIs( "loadgs" ) ) {
				g_pRunGSState = param;
			}

			// Options to configure plugins:

			else if( CmdSwitchIs( "gs" ) ) {
				g_TestRun.pgsdll = param;
			}
			else if( CmdSwitchIs( "cdvd" ) ) {
				g_TestRun.pcdvddll = param;
			}
			else if( CmdSwitchIs( "spu" ) ) {
				g_TestRun.pspudll = param;
			}

#ifdef PCSX2_DEVBUILD
			else if( CmdSwitchIs( "image" ) ) {
				g_TestRun.pimagename = param;
			}
			else if( CmdSwitchIs( "log" ) ) {
				g_TestRun.plogname = param;
			}
			else if( CmdSwitchIs( "logopt" ) ) {
				if( param[0] == '0' && param[1] == 'x' ) param += 2;
				sscanf(param, "%x", &varLog);
			}
			else if( CmdSwitchIs( "frame" ) ) {
				g_TestRun.frame = atoi( param );
			}
			else if( CmdSwitchIs( "numimages" ) ) {
				g_TestRun.numimages = atoi( param );
			}
#endif
		}
	}
	return 0;
}

void ExecuteCpu()
{
	// This tells the WM_DELETE handler of our GUI that we don't want the
	// system and plugins shut down, thanks...
	if( UseGui )
        AccBreak = true;

	// ... and destroy the window.  Ugly thing.
	DestroyWindow(gApp.hWnd);
	gApp.hWnd = NULL;

	g_GameInProgress = true;
	Cpu->Execute();
	g_GameInProgress = false;
}

// Runs and ELF image directly (ISO or ELF program or BIN)
// Used by Run::FromCD and such
void RunExecute( const char* elf_file, bool use_bios )
{
	SetThreadPriority(GetCurrentThread(), Config.ThPriority);
	SetPriorityClass(GetCurrentProcess(), Config.ThPriority == THREAD_PRIORITY_HIGHEST ? ABOVE_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
    nDisableSC = 1;

	g_GameInProgress = false;

	try
	{
		cpuReset();
	}
	catch( std::exception& ex )
	{
		SysMessage( ex.what() );
		return;
	}

	if (OpenPlugins(g_TestRun.ptitle) == -1)
		return;

	if( elf_file == NULL )
	{
		if(g_RecoveryState != NULL)
		{
			try
			{
				memLoadingState( *g_RecoveryState ).FreezeAll();
			}
			catch( std::runtime_error& ex )
			{
				SysMessage(
					"Gamestate recovery failed.  Your game progress will be lost (sorry!)\n"
					"\nError: %s\n", ex.what() );

				// Take the user back to the GUI...
				safe_delete( g_RecoveryState );
				ClosePlugins();
				return;
			}
			safe_delete( g_RecoveryState );
		}
		else
		{
			// Not recovering a state, so need to execute the bios and load the ELF information.

			// Note: if the elf_file is null we use the CDVD elf file.
			// But if the elf_file is an empty string then we boot the bios instead.

			cpuExecuteBios();
			char ename[g_MaxPath];
			ename[0] = 0;
			if( !use_bios )
				GetPS2ElfName(ename);
			loadElfFile( ename );
		}
	}
	else
	{
		// Custom ELF specified (not using CDVD).
		// Run the BIOS and load the ELF.

		cpuExecuteBios();
		loadElfFile( elf_file );
	}

	// this needs to be called for every new game!
	// (note: sometimes launching games through bios will give a crc of 0)

	if( GSsetGameCRC != NULL )
		GSsetGameCRC(ElfCRC, g_ZeroGSOptions);

	ExecuteCpu();
}

void States_Load( const char* file, int num )
{
	struct stat buf;
	if( stat(file, &buf ) == -1 )
	{
		Console::Notice( "Saveslot %d is empty.", num );
		return;
	}

	try
	{
		char Text[128];
		gzLoadingState joe( file );		// this'll throw an UnsupportedStateVersion.

		// Make sure the cpu and plugins are ready to be state-ified!
		cpuReset();
		OpenPlugins( NULL );

		joe.FreezeAll();

		if( num != -1 )
			sprintf (Text, _("*PCSX2*: Loaded State %d"), num);
		else
			sprintf (Text, _("*PCSX2*: Loaded State %s"), file);

		StatusSet( Text );
	}
	catch( Exception::UnsupportedStateVersion& )
	{
		if( num != -1 )
			SysMessage( _( "Savestate slot %d is an unsupported version." ), num);
		else
			SysMessage( _( "%s : This is an unsupported savestate version." ), file);

		// At this point the cpu hasn't been reset, so we can return
		// control to the user safely...

		return;
	}
	catch( std::exception& ex )
	{
		if( num != -1 )
			Console::Error( _("Error occured while trying to load savestate slot %d"), num);
		else
			Console::Error( _("Error occured while trying to load savestate file: %d"), file);

		Console::Error( ex.what() );

		// The emulation state is ruined.  Might as well give them a popup and start the gui.

		SysMessage( _( 
			"An error occured while trying to load the savestate data.\n"
			"Pcsx2 emulation state has been reset."
		) );

		cpuShutdown();
		return;
	}

	// Start emulating!
	ExecuteCpu();
}

void States_Save( const char* file, int num )
{
	try
	{
		char Text[128];
		gzSavingState(file).FreezeAll();
		if( num != -1 )
			sprintf( Text, _( "State saved to slot %d" ), num );
		else
			sprintf( Text, _( "State saved to file: %s" ), file );

		StatusSet( Text );
	}
	catch( std::exception& ex )
	{
		if( num != -1 )
			SysMessage( _("An error occured while trying to save to slot %d"), num );
		else
			SysMessage( _("An error occured while trying to save to file: %s"), file );

		Console::Error( _( "Save state request failed with the following error:" ) );
		Console::Error( ex.what() );
	}
}

void RunGuiAndReturn() {
    MSG msg;

	m_ReturnToGame = false;
    while( !m_ReturnToGame ) {
		if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(10);
	}

	// re-init plugins before returning execution:

	OpenPlugins( NULL );
	AccBreak = true;
	DestroyWindow(gApp.hWnd);
}

void States_Load(int num)
{
	char Text[g_MaxPath];
	SaveState::GetFilename( Text, num );
	States_Load( Text, num );
}

void States_Save(int num)
{
	char Text[g_MaxPath];
	SaveState::GetFilename( Text, num );
	States_Save( Text, num );
}

void OnStates_LoadOther()
{
	OPENFILENAME ofn;
	char szFileName[g_MaxPath];
	char szFileTitle[g_MaxPath];
	char szFilter[g_MaxPath];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	strcpy(szFilter, _("PCSX2 State Format"));
	strcatz(szFilter, "*.*;*.*");

    ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= gApp.hWnd;
    ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
    ofn.nMaxCustFilter		= 0;
    ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= szFileName;
    ofn.nMaxFile			= g_MaxPath;
    ofn.lpstrInitialDir		= NULL;
    ofn.lpstrFileTitle		= szFileTitle;
    ofn.nMaxFileTitle		= g_MaxPath;
    ofn.lpstrTitle			= NULL;
    ofn.lpstrDefExt			= "EXE";
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) 
		States_Load( szFileName );
} 

void OnStates_SaveOther()
{
	OPENFILENAME ofn;
	char szFileName[g_MaxPath];
	char szFileTitle[g_MaxPath];
	char szFilter[g_MaxPath];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	strcpy(szFilter, _("PCSX2 State Format"));
	strcatz(szFilter, "*.*;*.*");

    ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= gApp.hWnd;
    ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
    ofn.nMaxCustFilter		= 0;
    ofn.nFilterIndex		= 1;
    ofn.lpstrFile			= szFileName;
    ofn.nMaxFile			= g_MaxPath;
    ofn.lpstrInitialDir		= NULL;
    ofn.lpstrFileTitle		= szFileTitle;
    ofn.nMaxFileTitle		= g_MaxPath;
    ofn.lpstrTitle			= NULL;
    ofn.lpstrDefExt			= "EXE";
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn))
		States_Save( szFileName );
}

static int shiftkey = 0;
void CALLBACK KeyEvent(keyEvent* ev)
{
	if (ev == NULL) return;
	if (ev->evt == KEYRELEASE) {
		switch (ev->key) {
		case VK_SHIFT: shiftkey = 0; break;
		}
		GSkeyEvent(ev); return;
	}
	if (ev->evt != KEYPRESS)
        return;

    //some pad plugins don't give a key released event for shift, so this is needed
    //shiftkey = GetKeyState(VK_SHIFT)&0x8000;
    //Well SSXPad breaks with your code, thats why my code worked and your makes reg dumping impossible
	//So i suggest you fix the plugins that dont.
    
	switch (ev->key) {
		case VK_SHIFT: shiftkey = 1; break;

		case VK_F1: case VK_F2:  case VK_F3:  case VK_F4:
		case VK_F5: case VK_F6:  case VK_F7:  case VK_F8:
		case VK_F9: case VK_F10: case VK_F11: case VK_F12:
			try
			{
				ProcessFKeys(ev->key-VK_F1 + 1, shiftkey);
			}
			catch( Exception::CpuStateShutdown& )
			{
				// Woops!  Something was unrecoverable.  Bummer.
				// Let's give the user a RunGui!

				g_GameInProgress = false;
				CreateMainWindow( SW_SHOWNORMAL );
				RunGui();	// ah the beauty of perpetual stack recursion! (air)
			}
		break;

		/*case VK_NUMPAD0:
			Config.Hacks ^= 2;
			if (Config.Hacks & 2) {SysPrintf( "Overflow Check OFF\n" );} else {SysPrintf( "Overflow Check ON\n" );}
			break;*/

		case VK_ESCAPE:
#ifdef PCSX2_DEVBUILD
			if( g_SaveGSStream >= 3 ) {
				// gs state
				g_SaveGSStream = 4;
				break;
			}
#endif

			if (CHECK_ESCAPE_HACK) {
				PostMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
				WinClose();
			}
			else {
				ClosePlugins();

				if( !UseGui ) {
					// not using GUI and user just quit, so exit
					WinClose();
				}

				CreateMainWindow(SW_SHOWNORMAL);
				nDisableSC = 0;
				RunGuiAndReturn();
			}
			break;

		default:
			GSkeyEvent(ev);
			break;
	}
}

static bool sinit=false;

void SysRestorableReset()
{
	// already reset? and saved?
	if( !g_GameInProgress ) return;
	if( g_RecoveryState != NULL ) return;

	try
	{
		g_RecoveryState = new MemoryAlloc<u8>( "Memory Savestate Recovery" );
		memSavingState( *g_RecoveryState ).FreezeAll();
		cpuShutdown();
		g_GameInProgress = false;
	}
	catch( std::runtime_error& ex )
	{
		SysMessage(
			"Pcsx2 gamestate recovery failed. Some options may have been reverted to protect your game's state.\n"
			"Error: %s", ex.what() );
		safe_delete( g_RecoveryState );
	}
}

void SysReset()
{
	if (!sinit) return;

	StatusSet(_("Resetting..."));

	g_GameInProgress = false;
	safe_free( g_RecoveryState );

	ResetPlugins();

	StatusSet(_("Ready"));
}

bool SysInit()
{
	if( sinit )
		SysClose();

	CreateDirectory(MEMCARDS_DIR, NULL);
	CreateDirectory(SSTATES_DIR, NULL);
	CreateDirectory(LOGS_DIR, NULL);

	if( IsDevBuild && emuLog == NULL && g_TestRun.plogname != NULL )
		emuLog = fopen(g_TestRun.plogname, "w");

	if( emuLog == NULL )
		emuLog = fopen(LOGS_DIR "\\emuLog.txt","w");

	SysDetect();

	while (LoadPlugins() == -1) {
		if (Pcsx2Configure(NULL) == FALSE) {
			return false;		// user cancelled.
		}
	}

	if( !cpuInit() )
		return false;

	sinit = true;
	return true;
}

// completely shuts down the emulator's cpu state, and unloads all plugins from memory.
void SysClose() {
	if (!sinit) return;
	cpuShutdown();
	ClosePlugins();
	ReleasePlugins();
	sinit=false;
}

void SysPrintf(const char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list,fmt);
	vsprintf_s(msg,fmt,list);
	msg[511] = '\0';
	va_end(list);

	Console::Write( msg );
}

void SysMessage(const char *fmt, ...)
{
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf_s(tmp,fmt,list);
	tmp[511] = '\0';
	va_end(list);
	MessageBox(0, tmp, _("Pcsx2 Msg"), 0);
}

void SysUpdate() {

    KeyEvent(PAD1keyEvent()); //Only need 1 as its used for windows keys only
	KeyEvent(PAD2keyEvent());
}

void SysRunGui() {
	RunGui();
}

static const char *err = N_("Error Loading Symbol");
static int errval;

void *SysLoadLibrary(const char *lib) {
	return LoadLibrary(lib);
}

void *SysLoadSym(void *lib, const char *sym) {
	void *tmp = GetProcAddress((HINSTANCE)lib, sym);
	if (tmp == NULL) errval = GetLastError();
	else errval = 0;
	return tmp;
}

const char *SysLibError() {
	if( errval ) 
	{ 
		static char perr[4096];

		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,NULL,GetLastError(),NULL,perr,4095,NULL);

		errval = 0;
		return _(perr); 
	}
	return NULL;
}

void SysCloseLibrary(void *lib) {
	FreeLibrary((HINSTANCE)lib);
}

void *SysMmap(uptr base, u32 size) {
	void *mem;

	mem = VirtualAlloc((void*)base, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	//mem = VirtualAlloc((void*)mem,  size, MEM_COMMIT , PAGE_EXECUTE_READWRITE);
	return mem;
}

void SysMunmap(uptr base, u32 size) {
	VirtualFree((void*)base, size, MEM_DECOMMIT);
	VirtualFree((void*)base, 0, MEM_RELEASE);
}
