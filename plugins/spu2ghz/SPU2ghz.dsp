# Microsoft Developer Studio Project File - Name="SPU2ghz" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=SPU2ghz - Win32 Debug Banyoles
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SPU2ghz.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SPU2ghz.mak" CFG="SPU2ghz - Win32 Debug Banyoles"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SPU2ghz - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SPU2ghz - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SPU2ghz - Win32 Debug Threaded" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SPU2ghz - Win32 Debug2" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SPU2ghz - Win32 Debug Banyoles" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SPU2ghz - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "NDEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc0a /d "NDEBUG"
# ADD RSC /l 0xc0a /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /machine:I386 /out:"../../SPU2ghz.dll"

!ELSEIF  "$(CFG)" == "SPU2ghz - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc0a /d "_DEBUG"
# ADD RSC /l 0xc0a /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib fmodvc.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "SPU2ghz - Win32 Debug Threaded"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SPU2ghz___Win32_Debug_Threaded"
# PROP BASE Intermediate_Dir "SPU2ghz___Win32_Debug_Threaded"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SPU2ghz___Win32_Debug_Threaded"
# PROP Intermediate_Dir "SPU2ghz___Win32_Debug_Threaded"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /D "USETHREAD" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc0a /d "_DEBUG"
# ADD RSC /l 0xc0a /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1.dll" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1_thread.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "SPU2ghz - Win32 Debug2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SPU2ghz___Win32_Debug2"
# PROP BASE Intermediate_Dir "SPU2ghz___Win32_Debug2"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SPU2ghz___Win32_Debug2"
# PROP Intermediate_Dir "SPU2ghz___Win32_Debug2"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /D "OLDSPECS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc0a /d "_DEBUG"
# ADD RSC /l 0xc0a /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1.dll" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1_old.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "SPU2ghz - Win32 Debug Banyoles"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SPU2ghz___Win32_Debug_Banyoles"
# PROP BASE Intermediate_Dir "SPU2ghz___Win32_Debug_Banyoles"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SPU2ghz___Win32_Debug_Banyoles"
# PROP Intermediate_Dir "SPU2ghz___Win32_Debug_Banyoles"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__WIN32__" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPU2GHZ_EXPORTS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc0a /d "_DEBUG"
# ADD RSC /l 0xc0a /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib fmodvc.lib /nologo /dll /debug /machine:I386 /out:"c:\Coses\Emulation\PS2\pcsx2\plugins\SPU2ghz1.dll" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib fmodvc.lib /nologo /dll /debug /machine:I386 /out:".\SPU2ghz1.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "SPU2ghz - Win32 Release"
# Name "SPU2ghz - Win32 Debug"
# Name "SPU2ghz - Win32 Debug Threaded"
# Name "SPU2ghz - Win32 Debug2"
# Name "SPU2ghz - Win32 Debug Banyoles"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\config.c
# End Source File
# Begin Source File

SOURCE=.\debug.c
# End Source File
# Begin Source File

SOURCE=.\dma.c
# End Source File
# Begin Source File

SOURCE=.\fmodout.c
# End Source File
# Begin Source File

SOURCE=.\mixer.c
# End Source File
# Begin Source File

SOURCE=.\sndout.c
# End Source File
# Begin Source File

SOURCE=.\spu2.c
# End Source File
# Begin Source File

SOURCE=.\SPU2ghz.def
# End Source File
# Begin Source File

SOURCE=.\waveout.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\debug.h
# End Source File
# Begin Source File

SOURCE=.\defs.h
# End Source File
# Begin Source File

SOURCE=.\dma.h
# End Source File
# Begin Source File

SOURCE=.\mixer.h
# End Source File
# Begin Source File

SOURCE=.\PS2Edefs.h
# End Source File
# Begin Source File

SOURCE=.\PS2Etypes.h
# End Source File
# Begin Source File

SOURCE=.\regs.h
# End Source File
# Begin Source File

SOURCE=.\sndout.h
# End Source File
# Begin Source File

SOURCE=.\spu2.h
# End Source File
# End Group
# Begin Group "Documents"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\LGPL.txt
# End Source File
# Begin Source File

SOURCE=.\License.txt
# End Source File
# End Group
# End Target
# End Project
