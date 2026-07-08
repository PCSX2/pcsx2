Makefiles for libpng
--------------------

    makefile.amiga     =>  Amiga makefile
    makefile.atari     =>  Atari makefile
    makefile.clang     =>  Generic clang makefile
    makefile.emcc      =>  Emscripten makefile
    makefile.gcc       =>  Generic gcc makefile
    makefile.ibmc      =>  IBM C/C++ version 3.x for Win32 and OS/2
    makefile.intel     =>  Intel C/C++ version 4.0 and later
    makefile.riscos    =>  Acorn RISCOS makefile
    makefile.std       =>  Generic UNIX makefile
    makefile.vcwin32       =>  Microsoft Visual C++ for Windows/x86
    makefile.vcwin-arm64   =>  Microsoft Visual C++ for Windows/ARM64
    makevms.com        =>  VMS build script
    descrip.mms        =>  VMS makefile for MMS or MMK
    smakefile.ppc      =>  AMIGA smakefile for SAS C V6.58/7.00 PPC compiler
                           (Requires SCOPTIONS, copied from SCOPTIONS.ppc)
    SCOPTIONS.ppc      =>  Used with smakefile.ppc

Other supporting files and scripts
----------------------------------

    libpng-config-body.in  =>  Used by several makefiles to create libpng-config
    libpng-config-head.in  =>  Used by several makefiles to create libpng-config
    libpng.pc.in       =>  Used by several makefiles to create libpng.pc
    macro.lst          =>  Used by GNU Autotools
    pngwin.rc          =>  Used by the Visual Studio project
    README.txt         =>  This file

Further information can be found in comments in the individual scripts and
makefiles.
