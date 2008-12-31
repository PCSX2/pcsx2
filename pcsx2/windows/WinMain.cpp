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
#include "win32.h"

#include <winnt.h>
#include <commctrl.h>
#include <direct.h>

#include <ntsecapi.h>

#include "Common.h"
#include "PsxCommon.h"
#include "debugger.h"
#include "rdebugger.h"
#include "AboutDlg.h"
#include "McdsDlg.h"

#include "VU.h"
#include "iCore.h"
#include "iVUzerorec.h"

#include "Patch.h"
#include "cheats/cheats.h"

#include "Paths.h"
#include "SamplProf.h"

#include "implement.h"		// pthreads-win32 defines for startup/shutdown

#define COMPILEDATE         __DATE__

static bool AccBreak = false;
static unsigned int langsMax;
static bool m_ReturnToGame = false;		// set to exit the RunGui message pump

bool g_GameInProgress = false;	// Set TRUE if a game is actively running.

// This instance is not modified by command line overrides so
// that command line plugins and stuff won't be saved into the
// user's conf file accidentally.
PcsxConfig winConfig;		// local storage of the configuration options.

HWND hStatusWnd;
AppData gApp;

extern int g_SaveGSStream;

struct _langs {
	TCHAR lang[256];
};

_langs *langs = NULL;

static int UseGui = 1;
static int nDisableSC = 0; // screensaver

void strcatz(char *dst, char *src) {
	int len = strlen(dst) + 1;
	strcpy(dst + len, src);
}

static MemoryAlloc<u8>* g_RecoveryState = NULL;

//2002-09-20 (Florin)
BOOL APIENTRY CmdlineProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);//forward def
//-------------------

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
void RunExecute( const char* elf_file )
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
	}

	if (OpenPlugins(g_TestRun.ptitle) == -1)
		return;

	if( elf_file == NULL || elf_file[0] == 0)
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
			GetPS2ElfName(ename);
			loadElfFile( (elf_file == NULL) ? ename : "");
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

int Slots[5] = { -1, -1, -1, -1, -1 };

void ResetMenuSlots() {
	int i;

	for (i=0; i<5; i++) {
		if (Slots[i] == -1)
			EnableMenuItem(GetSubMenu(gApp.hMenu, 0), ID_FILE_STATES_LOAD_SLOT1+i, MF_GRAYED);
		else 
			EnableMenuItem(GetSubMenu(gApp.hMenu, 0), ID_FILE_STATES_LOAD_SLOT1+i, MF_ENABLED);
	}
}

// fixme - this looks like the beginnings of a dynamic "list of valid saveslots"
// feature.  Too bad it's never called and CheckState was old/dead code.
/*void UpdateMenuSlots() {
	char str[g_MaxPath];
	int i;

	for (i=0; i<5; i++) {
		sprintf_s (str, g_MaxPath, "sstates\\%8.8X.%3.3d", ElfCRC, i);
		Slots[i] = CheckState(str);
	}
}*/

static void States_Load( const char* file, int num=-1 )
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

static void States_Save( const char* file, int num=-1 )
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

static void States_Load(int num)
{
	char Text[g_MaxPath];
	SaveState::GetFilename( Text, num );
	States_Load( Text, num );
}

static void States_Save(int num)
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
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) 
		States_Load( szFileName );
} 

const char* g_pRunGSState = NULL;

void OnStates_SaveOther() {
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
    ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn))
		States_Save( szFileName );
}


TESTRUNARGS g_TestRun;

#define CmdSwitchIs( text ) ( stricmp( command, text ) == 0 )

static const char* phelpmsg = 
    "pcsx2 [options] [file]\n\n"
    "-cfg [file] {configuration file}\n"
    "-efile [efile] {0 - reset, 1 - runcd (default), 2 - loadelf}\n"
    "-help {display this help file}\n"
    "-nogui {Don't use gui when launching}\n"
	"-loadgs [file] {Loads a gsstate}\n"
    "\n"

#ifdef PCSX2_DEVBUILD
    "Testing Options: \n"
    "\t-frame [frame] {game will run up to this frame before exiting}\n"
	"\t-image [name] {path and base name of image (do not include the .ext)}\n"
    "\t-jpg {save images to jpg format}\n"
	"\t-log [name] {log path to save log file in}\n"
	"\t-logopt [hex] {log options in hex (see debug.h) }\n"
	"\t-numimages [num] {after hitting frame, this many images will be captures every 20 frames}\n"
    "\t-test {Triggers testing mode (only for dev builds)}\n"
    "\n"
#endif

    "Load Plugins:\n"
    "\t-cdvd [dllpath] {specify the dll load path of the CDVD plugin}\n"
    "\t-gs [dllpath] {specify the dll load path of the GS plugin}\n"
    "\t-spu [dllpath] {specify the dll load path of the SPU2 plugin}\n"
    "\n";

/// This code is courtesy of http://alter.org.ua/en/docs/win/args/
static PTCHAR* _CommandLineToArgv( const TCHAR *CmdLine, int* _argc )
{
    PTCHAR* argv;
    PTCHAR  _argv;
    ULONG   len;
    ULONG   argc;
    TCHAR   a;
    ULONG   i, j;

    BOOLEAN  in_QM;
    BOOLEAN  in_TEXT;
    BOOLEAN  in_SPACE;

	len = _tcslen( CmdLine );
    i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

    argv = (PTCHAR*)GlobalAlloc(GMEM_FIXED,
        i + (len+2)*sizeof(a));

    _argv = (PTCHAR)(((PUCHAR)argv)+i);

    argc = 0;
    argv[argc] = _argv;
    in_QM = FALSE;
    in_TEXT = FALSE;
    in_SPACE = TRUE;
    i = 0;
    j = 0;

    while( a = CmdLine[i] ) {
        if(in_QM) {
            if(a == '\"') {
                in_QM = FALSE;
            } else {
                _argv[j] = a;
                j++;
            }
        } else {
            switch(a) {
            case '\"':
                in_QM = TRUE;
                in_TEXT = TRUE;
                if(in_SPACE) {
                    argv[argc] = _argv+j;
                    argc++;
                }
                in_SPACE = FALSE;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                if(in_TEXT) {
                    _argv[j] = '\0';
                    j++;
                }
                in_TEXT = FALSE;
                in_SPACE = TRUE;
                break;
            default:
                in_TEXT = TRUE;
                if(in_SPACE) {
                    argv[argc] = _argv+j;
                    argc++;
                }
                _argv[j] = a;
                j++;
                in_SPACE = FALSE;
                break;
            }
        }
        i++;
    }
    _argv[j] = '\0';
    argv[argc] = NULL;

    (*_argc) = argc;
    return argv;
}

// returns 1 if the user requested help (show help and exit)
// returns zero on success.
// returns -1 on failure (bad command line argument)
static int ParseCommandLine( int tokenCount, TCHAR *const *const tokens )
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

static void WinClose()
{
	SysClose();

	// Don't check Config.Profiler here -- the Profiler will know if it's running or not.
	ProfilerTerm();
	timeEndPeriod( 1 );

	ReleasePlugins();
	Console::Close();

	pthread_win32_process_detach_np();

#ifdef PCSX2_VIRTUAL_MEM
	VirtualFree(PS2MEM_BASE, 0, MEM_RELEASE);
#endif

	exit(0);
}

BOOL SysLoggedSetLockPagesPrivilege ( HANDLE hProcess, BOOL bEnable);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	char *lang;
	int i;

#ifdef PCSX2_VIRTUAL_MEM
	LPVOID lpMemReserved;

	if( !SysLoggedSetLockPagesPrivilege( GetCurrentProcess(), TRUE ) )
		return -1;

	lpMemReserved = VirtualAlloc(PS2MEM_BASE, 0x40000000, MEM_RESERVE, PAGE_NOACCESS);

	if( lpMemReserved == NULL || lpMemReserved!= PS2MEM_BASE ) {
		char str[255];
		sprintf(str, "Cannot allocate mem addresses %x-%x, err: %d", PS2MEM_BASE, PS2MEM_BASE+0x40000000, GetLastError());
		MessageBox(NULL, str, "SysError", MB_OK);
		return -1;
	}
#endif

	InitCommonControls();
	pInstance=hInstance;
	FirstShow=true;			// this is used by cheats.cpp to search for stuff (broken?)

	pthread_win32_process_attach_np();

	__try 
	{

	gApp.hInstance = hInstance;
	gApp.hMenu = NULL;
	gApp.hWnd = NULL;

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, "Langs\\");
	textdomain(PACKAGE);
#endif

	memset(&g_TestRun, 0, sizeof(g_TestRun));

	_getcwd( g_WorkingFolder, g_MaxPath );

	{
		int argc;
		TCHAR *const *const argv = _CommandLineToArgv( lpCmdLine, &argc );

		if( argv == NULL )
		{
			SysMessage( "A fatal error occured while attempting to parse the command line.\n" );
			return 2;
		}

		switch( ParseCommandLine( argc, argv ) )
		{
			case 1:		// display help and exit:
				printf( "%s", phelpmsg );
				MessageBox( NULL, phelpmsg, "Pcsx2 Help", MB_OK);

			case -1:	// exit...
			return 0;
		}

		switch( LoadConfig() )
		{
			case 0:	break;	// everything worked!
			case 1:	
				// configure some defaults, notify the user, and the quit.
				memset(&Config, 0, sizeof(Config));
				//strcpy(Config.Bios, "HLE");
				strcpy(Config.BiosDir,    "Bios\\");
				strcpy(Config.PluginsDir, "Plugins\\");
				Config.Patch = 1;
				Config.Options = PCSX2_EEREC|PCSX2_VU0REC|PCSX2_VU1REC|PCSX2_COP2REC;
				Config.sseMXCSR = DEFAULT_sseMXCSR;
				Config.sseVUMXCSR = DEFAULT_sseVUMXCSR;
				Config.eeOptions = DEFAULT_eeOptions;
				Config.vuOptions = DEFAULT_vuOptions;
				Config.GameFixes = 0;
				Config.Hacks = 0;

				SysMessage(_("Pcsx2 needs to be configured"));
				Pcsx2Configure(NULL);

			case -1:		// Error occured.  Quit.
			return 0;
		}

		if( g_Error_PathTooLong ) return 3;
	}

	if (Config.Lang[0] == 0) {
		strcpy(Config.Lang, "en_US");
	}

	langs = (_langs*)malloc(sizeof(_langs));
	strcpy(langs[0].lang, "en_US");
	InitLanguages(); i=1;
	while ((lang = GetLanguageNext()) != NULL) {
		langs = (_langs*)realloc(langs, sizeof(_langs)*(i+1));
		strcpy(langs[i].lang, lang);
		i++;
	}
	CloseLanguages();
	langsMax = i;

	if( winConfig.PsxOut )
	{
		Console::Open();

		//if( lpCmdLine == NULL || *lpCmdLine == 0 )
		//	Console::WriteLn("-help to see arguments");
	}

	// Load the command line overrides for plugins:

	memcpy( &Config, &winConfig, sizeof( PcsxConfig ) );

	if( g_TestRun.pgsdll )
	{
		_tcscpy_s( Config.GS, g_MaxPath, g_TestRun.pgsdll );
		Console::Notice( "* GS plugin override: \n\t%s\n", Config.GS );
	}
	if( g_TestRun.pcdvddll )
	{
		_tcscpy_s( Config.CDVD, g_MaxPath, g_TestRun.pcdvddll );
		Console::Notice( "* CDVD plugin override: \n\t%s\n", Config.CDVD );
	}
	if( g_TestRun.pspudll )
	{
		_tcscpy_s( Config.SPU2, g_MaxPath, g_TestRun.pspudll );
		Console::Notice( "* SPU2 plugin override: \n\t%s\n", Config.SPU2 );
	}

	// [TODO] : Add the other plugin overrides here...

#ifndef _DEBUG
	if( Config.Profiler )
		ProfilerInit();
#endif

	// This makes sure the Windows Kernel is using high resolution
	// timeslices for Sleep calls.
	// (may not make much difference on most desktops but can improve performance
	//  a lot on laptops).

	timeBeginPeriod( 1 );
	InitCPUTicks();
	
	if( !SysInit() )
		WinClose();

#ifdef PCSX2_DEVBUILD
    if( g_TestRun.enabled || g_TestRun.ptitle != NULL ) {
		// run without ui
        UseGui = 0;
		SysReset();
		RunExecute( g_TestRun.efile ? g_TestRun.ptitle : NULL );
		WinClose();
		return 0; // success!
	}

	if( g_pRunGSState ) {
		LoadGSState(g_pRunGSState);
		WinClose();
		return 0;
	}
#endif

	CreateMainWindow( nCmdShow );

    if( Config.PsxOut )
	{
	    // output the help commands
		Console::SetColor( Console::Color_White );

		Console::WriteLn( "Hotkeys:" );

		Console::WriteLn(
			"\tF1  - save state\n"
			"\t(Shift +) F2 - cycle states\n"
			"\tF3  - load state"
		);

	    DevCon::WriteLn(
			"\tF10 - dump performance counters\n"
			"\tF11 - save GS state\n"
			"\tF12 - dump hardware registers"
		);
		Console::ClearColor();
    }

	LoadPatch("default");
	RunGui();

	}
	__except(SysPageFaultExceptionFilter(GetExceptionInformation()))
	{
	}

	// Note : Because of how the GUI and recompiler function, this area of
	// the code is effectively unreachable.  Program termination is handled
	// by a call to WinClose instead. (above)

	return 0;
}


void RunGui() {
    MSG msg;

    for (;;) {
		if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(10);
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

BOOL Open_File_Proc( std::string& outstr )
{
	OPENFILENAME ofn;
	char szFileName[ g_MaxPath ];
	char szFileTitle[ g_MaxPath ];
	char * filter = "ELF Files (*.ELF)\0*.ELF\0ALL Files (*.*)\0*.*\0";

	memset( &szFileName, 0, sizeof( szFileName ) );
	memset( &szFileTitle, 0, sizeof( szFileTitle ) );

	ofn.lStructSize			= sizeof( OPENFILENAME );
	ofn.hwndOwner			= gApp.hWnd;
	ofn.lpstrFilter			= filter;
	ofn.lpstrCustomFilter   = NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= g_MaxPath;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= g_MaxPath;
	ofn.lpstrTitle			= NULL;
	ofn.lpstrDefExt			= "ELF";
	ofn.Flags				= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	 
	if (GetOpenFileName(&ofn)) {
		struct stat buf;

		if (stat(szFileName, &buf) != 0) {
			return FALSE;
		}

		outstr.assign( szFileName );
		return TRUE;
	}

	return FALSE;
}

//2002-09-20 (Florin)
BOOL APIENTRY CmdlineProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowText(hDlg, _("Program arguments"));

			Button_SetText(GetDlgItem(hDlg, IDOK), _("OK"));
			Button_SetText(GetDlgItem(hDlg, IDCANCEL), _("Cancel"));
			Static_SetText(GetDlgItem(hDlg, IDC_TEXT), _("Fill in the command line arguments for opened program:"));
			Static_SetText(GetDlgItem(hDlg, IDC_TIP), _("Tip: If you don't know what to write\nleave it blank"));

            SetDlgItemText(hDlg, IDC_CMDLINE, args);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
				char tmp[256];

				GetDlgItemText(hDlg, IDC_CMDLINE, tmp, 256);

				strcpy_s(args, 256, tmp);
                
                EndDialog(hDlg, TRUE);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, TRUE);
            }
            return TRUE;
    }

    return FALSE;
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

#ifdef PCSX2_DEVBUILD

BOOL APIENTRY LogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	int i;
    switch (message) {
        case WM_INITDIALOG:
			for (i=0; i<32; i++)
				if (varLog & (1<<i))
					CheckDlgButton(hDlg, IDC_CPULOG+i, TRUE);

            return TRUE;

        case WM_COMMAND:

            if (LOWORD(wParam) == IDOK) {
				for (i=0; i<32; i++) {
	 			    int ret = Button_GetCheck(GetDlgItem(hDlg, IDC_CPULOG+i));
					if (ret) varLog|= 1<<i;
					else varLog&=~(1<<i);
				}

				SaveConfig();              

                EndDialog(hDlg, TRUE);
				return FALSE;
            } 
            return TRUE;
    }

    return FALSE;
}

#endif

BOOL APIENTRY GameFixes(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
			if(Config.GameFixes & 0x2) CheckDlgButton(hDlg, IDC_GAMEFIX2, TRUE);
			if(Config.GameFixes & 0x4) CheckDlgButton(hDlg, IDC_GAMEFIX3, TRUE);
			if(Config.GameFixes & 0x8) CheckDlgButton(hDlg, IDC_GAMEFIX4, TRUE);
		return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
			{
				uint newfixes = 0;
				newfixes |= IsDlgButtonChecked(hDlg, IDC_GAMEFIX2) ? 0x2 : 0;
				newfixes |= IsDlgButtonChecked(hDlg, IDC_GAMEFIX3) ? 0x4 : 0;
				newfixes |= IsDlgButtonChecked(hDlg, IDC_GAMEFIX4) ? 0x8 : 0;
				
				EndDialog(hDlg, TRUE);

				if( newfixes != Config.GameFixes )
				{
					Config.GameFixes = newfixes;
					SysRestorableReset();
					SaveConfig();
				}
				return FALSE;
            } 
			else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, TRUE);
				return FALSE;
            }
		return TRUE;
    }

    return FALSE;
}

BOOL APIENTRY HacksProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
			if(Config.Hacks & 0x1) CheckDlgButton(hDlg, IDC_SYNCHACK, TRUE);
			if(Config.Hacks & 0x10) CheckDlgButton(hDlg, IDC_SYNCHACK2, TRUE);
			if(Config.Hacks & 0x20) CheckDlgButton(hDlg, IDC_SYNCHACK3, TRUE);
			if(Config.Hacks & 0x400) CheckDlgButton(hDlg, IDC_ESCHACK, TRUE);
			return TRUE;

        case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					uint newhacks = 0;
					newhacks |= IsDlgButtonChecked(hDlg, IDC_SYNCHACK) ? 0x1 : 0;
					newhacks |= IsDlgButtonChecked(hDlg, IDC_SYNCHACK2) ? 0x10 : 0;
					newhacks |= IsDlgButtonChecked(hDlg, IDC_SYNCHACK3) ? 0x20 : 0;
					newhacks |= IsDlgButtonChecked(hDlg, IDC_ESCHACK) ? 0x400 : 0;

					EndDialog(hDlg, TRUE);

					if( newhacks != Config.Hacks )
					{
						SysRestorableReset();
						Config.Hacks = newhacks;
						SaveConfig();
					}
				}
				return FALSE;

				case IDCANCEL:
					EndDialog(hDlg, FALSE);
				return FALSE;
			}
		return TRUE;
    }

    return FALSE;
}

HBITMAP hbitmap_background;//the background image

LRESULT WINAPI MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
        case WM_CREATE:
	        return TRUE;

		case WM_PAINT:
		{
			BITMAP bm;
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(gApp.hWnd, &ps);

			HDC hdcMem = CreateCompatibleDC(hdc);
			HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbitmap_background);

			GetObject(hbitmap_background, sizeof(bm), &bm);
		//			BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);
			BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right-ps.rcPaint.left+1,
						ps.rcPaint.bottom-ps.rcPaint.top+1,
						hdcMem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

			SelectObject(hdcMem, hbmOld);
			DeleteDC(hdcMem);
			EndPaint(gApp.hWnd, &ps);
		 }
		 return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
			case ID_GAMEFIXES:
				 DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_GAMEFIXES), hWnd, (DLGPROC)GameFixes);
				 return FALSE;

			case ID_HACKS:
				 DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_HACKS), hWnd, (DLGPROC)HacksProc);
				 return FALSE;

			case ID_ADVANCED_OPTIONS:
				 DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_ADVANCED_OPTIONS), hWnd, (DLGPROC)AdvancedOptionsProc);
				 return FALSE;

			case ID_CHEAT_FINDER_SHOW:
				ShowFinder(pInstance,hWnd);
				return FALSE;

			case ID_CHEAT_BROWSER_SHOW:
				ShowCheats(pInstance,hWnd);
				return FALSE;

			case ID_FILE_EXIT:
				DestroyWindow( hWnd );
				// WM_DESTROY will do the shutdown work for us.
				return FALSE;

			case ID_FILEOPEN:
			{
				std::string outstr;
				if( Open_File_Proc( outstr ) )
					RunExecute( outstr.c_str() );
			}
			return FALSE;

			case ID_RUN_EXECUTE:
				if( g_GameInProgress )
					m_ReturnToGame = 1;
				else
					RunExecute( "" );	// boots bios if no savestate is to be recovered
			return FALSE;

			case ID_FILE_RUNCD:
				safe_free( g_RecoveryState );
				ResetPlugins();
				RunExecute( NULL );
			return FALSE;

			case ID_RUN_RESET:
				safe_free( g_RecoveryState );
				SysReset();
			return FALSE;

			//2002-09-20 (Florin)
			case ID_RUN_CMDLINE:
				DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CMDLINE), hWnd, (DLGPROC)CmdlineProc);
				return FALSE;
			//-------------------
           	case ID_PATCHBROWSER:
                DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_PATCHBROWSER), hWnd, (DLGPROC)PatchBDlgProc);
				return FALSE;
			case ID_CONFIG_CONFIGURE:
				Pcsx2Configure(hWnd);
				ReleasePlugins();
				LoadPlugins();
				return FALSE;

			case ID_CONFIG_GRAPHICS:
				if (GSconfigure) GSconfigure();
				return FALSE;
         

			case ID_CONFIG_CONTROLLERS:
				if (PAD1configure) PAD1configure();
				if (PAD2configure) {
					if (strcmp(Config.PAD1, Config.PAD2))PAD2configure();
				}
				return FALSE;

			case ID_CONFIG_SOUND:
				if (SPU2configure) SPU2configure();
				return FALSE;

			case ID_CONFIG_CDVDROM:
				if (CDVDconfigure) CDVDconfigure();
				return FALSE;

			case ID_CONFIG_DEV9:
				if (DEV9configure) DEV9configure();
				return FALSE;

			case ID_CONFIG_USB:
				if (USBconfigure) USBconfigure();
				return FALSE;
  
			case ID_CONFIG_FW:
				if (FWconfigure) FWconfigure();
				return FALSE;

			case ID_FILE_STATES_LOAD_SLOT1: 
			case ID_FILE_STATES_LOAD_SLOT2: 
			case ID_FILE_STATES_LOAD_SLOT3: 
			case ID_FILE_STATES_LOAD_SLOT4: 
			case ID_FILE_STATES_LOAD_SLOT5:
				States_Load(LOWORD(wParam) - ID_FILE_STATES_LOAD_SLOT1);
			return FALSE;

			case ID_FILE_STATES_LOAD_OTHER:
				OnStates_LoadOther();
			return FALSE;

			case ID_FILE_STATES_SAVE_SLOT1: 
			case ID_FILE_STATES_SAVE_SLOT2: 
			case ID_FILE_STATES_SAVE_SLOT3: 
			case ID_FILE_STATES_SAVE_SLOT4: 
			case ID_FILE_STATES_SAVE_SLOT5: 
				States_Save(LOWORD(wParam) - ID_FILE_STATES_SAVE_SLOT1);
			return FALSE;

			case ID_FILE_STATES_SAVE_OTHER:
				OnStates_SaveOther();
			return FALSE;

			case ID_CONFIG_CPU:
                DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CPUDLG), hWnd, (DLGPROC)CpuDlgProc);
				return FALSE;

#ifdef PCSX2_DEVBUILD
			case ID_DEBUG_ENTERDEBUGGER:
                DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_DEBUG), NULL, (DLGPROC)DebuggerProc);
                return FALSE;

			case ID_DEBUG_REMOTEDEBUGGING:
				//read debugging params
				if (Config.Options & PCSX2_EEREC){
					MessageBox(hWnd, _("Nah, you have to be in\nInterpreter Mode to debug"), 0, 0);
				} else 
				{
					int remoteDebugBios=0;
					remoteDebugBios=DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_RDEBUGPARAMS), NULL, (DLGPROC)RemoteDebuggerParamsProc);
					if (remoteDebugBios)
					{
						cpuReset();
						cpuExecuteBios();

						DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_RDEBUG), NULL, (DLGPROC)RemoteDebuggerProc);
						CreateMainWindow(SW_SHOWNORMAL);
						RunGui();
					}
				}
                return FALSE;

			case ID_DEBUG_MEMORY_DUMP:
			    DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_MEMORY), hWnd, (DLGPROC)MemoryProc);
				return FALSE;

			case ID_DEBUG_LOGGING:
			    DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_LOGGING), hWnd, (DLGPROC)LogProc);
				return FALSE;
#endif

			case ID_HELP_ABOUT:
				DialogBox(gApp.hInstance, MAKEINTRESOURCE(ABOUT_DIALOG), hWnd, (DLGPROC)AboutDlgProc);
				return FALSE;

			case ID_HELP_HELP:
				//system("help\\index.html");
				system("compat_list\\compat_list.html");
				return FALSE;

			case ID_CONFIG_MEMCARDS:
				DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_MCDCONF), hWnd, (DLGPROC)ConfigureMcdsDlgProc);
				SaveConfig();
				return FALSE;
			case ID_PROCESSLOW: 
               Config.ThPriority = THREAD_PRIORITY_LOWEST;
                SaveConfig();
				CheckMenuItem(gApp.hMenu,ID_PROCESSLOW,MF_CHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSNORMAL,MF_UNCHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSHIGH,MF_UNCHECKED);
                return FALSE;
			case ID_PROCESSNORMAL:
                Config.ThPriority = THREAD_PRIORITY_NORMAL;
                SaveConfig();
				CheckMenuItem(gApp.hMenu,ID_PROCESSNORMAL,MF_CHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSLOW,MF_UNCHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSHIGH,MF_UNCHECKED);
                return FALSE;
			case ID_PROCESSHIGH:
                Config.ThPriority = THREAD_PRIORITY_HIGHEST;
                SaveConfig();
				CheckMenuItem(gApp.hMenu,ID_PROCESSHIGH,MF_CHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSNORMAL,MF_UNCHECKED);
                CheckMenuItem(gApp.hMenu,ID_PROCESSLOW,MF_UNCHECKED);
                return FALSE;

			case ID_CONSOLE:
				Config.PsxOut = !Config.PsxOut;
				if(Config.PsxOut)
				{
                   CheckMenuItem(gApp.hMenu,ID_CONSOLE,MF_CHECKED);
				   Console::Open();
				}
				else
				{
                   CheckMenuItem(gApp.hMenu,ID_CONSOLE,MF_UNCHECKED);
				   Console::Close();
				}
				SaveConfig();
				return FALSE;

			case ID_PATCHES:
				Config.Patch = !Config.Patch;
				CheckMenuItem(gApp.hMenu,ID_PATCHES,Config.Patch ? MF_CHECKED : MF_UNCHECKED);

				SaveConfig();
				return FALSE;

#ifndef _DEBUG
			case ID_PROFILER:
				Config.Profiler = !Config.Profiler;
				if( Config.Profiler )
				{
					CheckMenuItem(gApp.hMenu,ID_PROFILER,MF_CHECKED);
					ProfilerInit();
				}
				else
				{
					CheckMenuItem(gApp.hMenu,ID_PROFILER,MF_UNCHECKED);
					ProfilerTerm();
				}
				SaveConfig();
				return FALSE;
#endif

			default:
				if (LOWORD(wParam) >= ID_LANGS && LOWORD(wParam) <= (ID_LANGS + langsMax)) {
					AccBreak = true;
					DestroyWindow(gApp.hWnd);
					ChangeLanguage(langs[LOWORD(wParam) - ID_LANGS].lang);
					CreateMainWindow(SW_SHOWNORMAL);
					return TRUE;
				}
			}
			return TRUE;

		case WM_DESTROY:
			if (!AccBreak)
			{
				// [TODO] : Check if a game is active in the emulator and ask the user
				// before closing!
				DeleteObject(hbitmap_background);
				WinClose();

			} else AccBreak = false;
		return FALSE;

        case WM_SYSCOMMAND:
            if( nDisableSC && (wParam== SC_SCREENSAVE || wParam == SC_MONITORPOWER) ) {
               return FALSE;
            }
		break;
        
		case WM_QUIT:
			if (Config.PsxOut)
				Console::Close();
		return TRUE;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

#define _ADDSUBMENU(menu, menun, string) \
	submenu[menun] = CreatePopupMenu(); \
	AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)submenu[menun], string);

#define ADDSUBMENU(menun, string) \
	_ADDSUBMENU(gApp.hMenu, menun, string);

#define ADDSUBMENUS(submn, menun, string) \
	submenu[menun] = CreatePopupMenu(); \
	InsertMenu(submenu[submn], 0, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT)submenu[menun], string);

#define ADDMENUITEM(menun, string, id) \
	item.fType = MFT_STRING; \
	item.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID; \
	item.fState = MFS_ENABLED; \
	item.wID = id; \
	sprintf(buf, string); \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

#define ADDMENUITEMC(menun, string, id) \
	item.fType = MFT_STRING; \
	item.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID; \
	item.fState = MFS_ENABLED | MFS_CHECKED; \
	item.wID = id; \
	sprintf(buf, string); \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

#define ADDSEPARATOR(menun) \
	item.fMask = MIIM_TYPE; \
	item.fType = MFT_SEPARATOR; \
	InsertMenuItem(submenu[menun], 0, TRUE, &item);

void CreateMainMenu() {
	MENUITEMINFO item;
	HMENU submenu[5];
	char buf[256];
	int i;

	item.cbSize = sizeof(MENUITEMINFO);
	item.dwTypeData = buf;
	item.cch = 256;

	gApp.hMenu = CreateMenu();

	//submenu = CreatePopupMenu();
	//AppendMenu(gApp.hMenu, MF_STRING | MF_POPUP, (UINT)submenu, _("&File"));
    ADDSUBMENU(0, _("&File"));
	ADDMENUITEM(0, _("E&xit"), ID_FILE_EXIT);
	ADDSEPARATOR(0);
	ADDSUBMENUS(0, 1, _("&States"));
	ADDSEPARATOR(0);
	ADDMENUITEM(0, _("&Open ELF File"), ID_FILEOPEN);
	ADDMENUITEM(0, _("&Run CD/DVD"), ID_FILE_RUNCD);
	ADDSUBMENUS(1, 3, _("&Save"));
	ADDSUBMENUS(1, 2, _("&Load"));
	ADDMENUITEM(2, _("&Other..."), ID_FILE_STATES_LOAD_OTHER);
	ADDMENUITEM(2, _("Slot &4"), ID_FILE_STATES_LOAD_SLOT5);
	ADDMENUITEM(2, _("Slot &3"), ID_FILE_STATES_LOAD_SLOT4);
	ADDMENUITEM(2, _("Slot &2"), ID_FILE_STATES_LOAD_SLOT3);
	ADDMENUITEM(2, _("Slot &1"), ID_FILE_STATES_LOAD_SLOT2);
	ADDMENUITEM(2, _("Slot &0"), ID_FILE_STATES_LOAD_SLOT1);
	ADDMENUITEM(3, _("&Other..."), ID_FILE_STATES_SAVE_OTHER);
	ADDMENUITEM(3, _("Slot &4"), ID_FILE_STATES_SAVE_SLOT5);
	ADDMENUITEM(3, _("Slot &3"), ID_FILE_STATES_SAVE_SLOT4);
	ADDMENUITEM(3, _("Slot &2"), ID_FILE_STATES_SAVE_SLOT3);
	ADDMENUITEM(3, _("Slot &1"), ID_FILE_STATES_SAVE_SLOT2);
	ADDMENUITEM(3, _("Slot &0"), ID_FILE_STATES_SAVE_SLOT1);

    ADDSUBMENU(0, _("&Run"));

    ADDSUBMENUS(0, 1, _("&Process Priority"));
	ADDMENUITEM(1, _("&Low"), ID_PROCESSLOW );
	ADDMENUITEM(1, _("High"), ID_PROCESSHIGH);
	ADDMENUITEM(1, _("Normal"), ID_PROCESSNORMAL);
	ADDMENUITEM(0,_("&Arguments"), ID_RUN_CMDLINE);
	ADDMENUITEM(0,_("Re&set"), ID_RUN_RESET);
	ADDMENUITEM(0,_("E&xecute"), ID_RUN_EXECUTE);

	ADDSUBMENU(0,_("&Config"));
	ADDMENUITEM(0,_("Advanced"), ID_ADVANCED_OPTIONS);
	ADDMENUITEM(0,_("Speed &Hacks"), ID_HACKS);
	ADDMENUITEM(0,_("Gamefixes"), ID_GAMEFIXES);
	ADDMENUITEM(0,_("&Patches"), ID_PATCHBROWSER);
	ADDMENUITEM(0,_("C&pu"), ID_CONFIG_CPU);
	ADDMENUITEM(0,_("&Memcards"), ID_CONFIG_MEMCARDS);
	ADDSEPARATOR(0);
	ADDMENUITEM(0,_("Fire&Wire"), ID_CONFIG_FW);
	ADDMENUITEM(0,_("U&SB"), ID_CONFIG_USB);
	ADDMENUITEM(0,_("D&ev9"), ID_CONFIG_DEV9);
	ADDMENUITEM(0,_("C&dvdrom"), ID_CONFIG_CDVDROM);
	ADDMENUITEM(0,_("&Sound"), ID_CONFIG_SOUND);
	ADDMENUITEM(0,_("C&ontrollers"), ID_CONFIG_CONTROLLERS);
	ADDMENUITEM(0,_("&Graphics"), ID_CONFIG_GRAPHICS);
	ADDSEPARATOR(0);
	ADDMENUITEM(0,_("&Configure"), ID_CONFIG_CONFIGURE);

    ADDSUBMENU(0,_("&Language"));

	for (i=langsMax-1; i>=0; i--) {
		if (!strcmp(Config.Lang, langs[i].lang)) {
			ADDMENUITEMC(0,ParseLang(langs[i].lang), ID_LANGS + i);
		} else {
			ADDMENUITEM(0,ParseLang(langs[i].lang), ID_LANGS + i);
		}
	}

#ifdef PCSX2_DEVBUILD
	ADDSUBMENU(0, _("&Debug"));
	ADDMENUITEM(0,_("&Logging"), ID_DEBUG_LOGGING);
	ADDMENUITEM(0,_("Memory Dump"), ID_DEBUG_MEMORY_DUMP);
	ADDMENUITEM(0,_("&Remote Debugging"), ID_DEBUG_REMOTEDEBUGGING);
	ADDMENUITEM(0,_("Enter &Debugger..."), ID_DEBUG_ENTERDEBUGGER);
#endif

	ADDSUBMENU(0, _("&Misc"));
	if( !IsDebugBuild )
	{
		ADDMENUITEM(0,_("Enable &Profiler"), ID_PROFILER);
		ADDSEPARATOR(0);
	}
	ADDMENUITEM(0,_("Enable &Patches"), ID_PATCHES);
	ADDMENUITEM(0,_("Enable &Console"), ID_CONSOLE); 
	ADDSEPARATOR(0);
	ADDMENUITEM(0,_("Patch &Finder..."), ID_CHEAT_FINDER_SHOW); 
	ADDMENUITEM(0,_("Patch &Browser..."), ID_CHEAT_BROWSER_SHOW); 


    ADDSUBMENU(0, _("&Help"));
	ADDMENUITEM(0,_("&Compatibility List..."), ID_HELP_HELP);
	ADDMENUITEM(0,_("&About..."), ID_HELP_ABOUT);

	if( !IsDevBuild )
		EnableMenuItem(GetSubMenu(gApp.hMenu, 4), ID_DEBUG_LOGGING, MF_GRAYED);
}

void CreateMainWindow(int nCmdShow) {
	WNDCLASS wc;
	HWND hWnd;
	char buf[256];
	char COMPILER[20]="";
	BITMAP bm;
	RECT rect;
	int w, h;

#ifdef _MSC_VER
	sprintf(COMPILER, "(VC%d)", (_MSC_VER+100)/200);//hacky:) works for VC6 & VC.NET
#elif __BORLANDC__
	sprintf(COMPILER, "(BC)");
#endif
	/* Load Background Bitmap from the ressource */ 
	hbitmap_background = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(SPLASH_LOGO));

	wc.lpszClassName = "PCSX2 Main";
	wc.lpfnWndProc = MainWndProc;
	wc.style = 0;
	wc.hInstance = gApp.hInstance;
	wc.hIcon = LoadIcon(gApp.hInstance, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_MENUTEXT);
	wc.lpszMenuName = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;

	RegisterClass(&wc);
	GetObject(hbitmap_background, sizeof(bm), &bm);

	{
#ifdef PCSX2_VIRTUAL_MEM
		const char* pvm = "VM";
#else
		const char* pvm = "VTLB";
#endif

#ifdef PCSX2_DEVBUILD
		sprintf(buf, _("PCSX2 %s - %s Compile Date - %s %s"), PCSX2_VERSION, pvm, COMPILEDATE, COMPILER);
#else
		sprintf(buf, _("PCSX2 %s - %s"), PCSX2_VERSION, pvm);
#endif
	}

	hWnd = CreateWindow(
		"PCSX2 Main",
		buf, WS_OVERLAPPED | WS_SYSMENU,
		20, 20, 320, 240, NULL, NULL,
		gApp.hInstance, NULL
	);

	gApp.hWnd = hWnd;
    ResetMenuSlots();
	CreateMainMenu();
   
	SetMenu(gApp.hWnd, gApp.hMenu);
    if(Config.ThPriority==THREAD_PRIORITY_NORMAL) CheckMenuItem(gApp.hMenu,ID_PROCESSNORMAL,MF_CHECKED);
	if(Config.ThPriority==THREAD_PRIORITY_HIGHEST) CheckMenuItem(gApp.hMenu,ID_PROCESSHIGH,MF_CHECKED);
	if(Config.ThPriority==THREAD_PRIORITY_LOWEST)  CheckMenuItem(gApp.hMenu,ID_PROCESSLOW,MF_CHECKED);
	if(Config.PsxOut)	CheckMenuItem(gApp.hMenu,ID_CONSOLE,MF_CHECKED);
	if(Config.Patch)	CheckMenuItem(gApp.hMenu,ID_PATCHES,MF_CHECKED);
	if(Config.Profiler)	CheckMenuItem(gApp.hMenu,ID_PROFILER,MF_CHECKED);
	hStatusWnd = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "", hWnd, 100);
    sprintf(buf, "PCSX2 %s", PCSX2_VERSION);
	StatusSet(buf);

	w = bm.bmWidth; h = bm.bmHeight;
	GetWindowRect(hStatusWnd, &rect);
	h+= rect.bottom - rect.top;
	GetMenuItemRect(hWnd, gApp.hMenu, 0, &rect);
	h+= rect.bottom - rect.top;
	MoveWindow(hWnd, 60, 60, w, h, TRUE);

	DestroyWindow(hStatusWnd);
	hStatusWnd = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "", hWnd, 100);
	sprintf(buf, "F1 - save, F2 - next state, Shift+F2 - prev state, F3 - load, F8 - snapshot", PCSX2_VERSION);
	StatusSet(buf);
	ShowWindow(hWnd, nCmdShow);
	SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	SetForegroundWindow( hWnd );
}


WIN32_FIND_DATA lFindData;
HANDLE lFind;
int lFirst;

void InitLanguages() {
	lFind = FindFirstFile("Langs\\*", &lFindData);
	lFirst = 1;
}

char *GetLanguageNext() {
	for (;;) {
		if (!strcmp(lFindData.cFileName, ".")) {
			if (FindNextFile(lFind, &lFindData) == FALSE)
				return NULL;
			continue;
		}
		if (!strcmp(lFindData.cFileName, "..")) {
			if (FindNextFile(lFind, &lFindData) == FALSE)
				return NULL;
			continue;
		}
		break;
	}
	if (lFirst == 0) {
		if (FindNextFile(lFind, &lFindData) == FALSE)
			return NULL;
	} else lFirst = 0;
	if (lFind==INVALID_HANDLE_VALUE) return NULL;

	return lFindData.cFileName;
}

void CloseLanguages() {
	if (lFind!=INVALID_HANDLE_VALUE) FindClose(lFind);
}

void ChangeLanguage(char *lang) {
	strcpy_s(Config.Lang, lang);
	SaveConfig();
	LoadConfig();
}

//-------------------

static bool sinit=false;

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
	if (GetLastError()) 
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
