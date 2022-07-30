              libusb 1.0 Windows binary snapshot - README

   *********************************************************************
   *  The latest version of this snapshot can always be downloaded at: *
   *         https://sourceforge.net/projects/libusb/files/           *
   *********************************************************************

o Visual Studio:
  - Open existing or create a new project for your application
  - Copy libusb.h, from the include\libusb-1.0\ directory, into your project and
    make sure that the location where the file reside appears in the 'Additional
    Include Directories' section (Configuration Properties -> C/C++ -> General).
  - Copy the relevant .lib file from MS32\ or MS64\ and add 'libusb-1.0.lib' to
    your 'Additional Dependencies' (Configuration Properties -> Linker -> Input)
    Also make sure that the directory where libusb-1.0.lib resides is added to
    'Additional Library Directories' (Configuration Properties -> Linker
    -> General)
  - If you use the static version of the libusb library, make sure that
    'Runtime Library' is set to 'Multi-threaded DLL (/MD)' (Configuration
    Properties -> C/C++ -> Code Generation).
    NB: If your application requires /MT (Multi-threaded/libCMT), you need to
    recompile a static libusb 1.0 library from source.
  - Compile and run your application. If you use the DLL version of libusb-1.0,
    remember that you need to have a copy of the DLL either in the runtime
    directory or in system32

o WDK/DDK:
  - The following is an example of a sources files that you can use to compile
    a libusb 1.0 based console application. In this sample ..\libusb\ is the
    directory where you would have copied libusb.h as well as the relevant 
    libusb-1.0.lib

	TARGETNAME=your_app
	TARGETTYPE=PROGRAM
	USE_MSVCRT=1
	UMTYPE=console
	INCLUDES=..\libusb;$(DDK_INC_PATH)
	TARGETLIBS=..\libusb\libusb-1.0.lib
	SOURCES=your_app.c

  - Note that if you plan to use libCMT instead of MSVCRT (USE_LIBCMT=1 instead
    of USE_MSVCRT=1), you will need to recompile libusb to use libCMT. This can
    easily be achieved, in the DDK environment, by running 'ddk_build /MT'

o MinGW/cygwin
  - Copy libusb.h, from include/libusb-1.0/ to your default include directory,
    and copy the MinGW32/ or MinGW64/ .a files to your default library directory.
    Or, if you don't want to use the default locations, make sure that you feed
    the relevant -I and -L options to the compiler.
  - Add the '-lusb-1.0' linker option when compiling.

o Additional information:
  - The libusb 1.0 API documentation can be accessed at:
    http://api.libusb.info
  - For some libusb samples (including source), please have a look in examples/
  - For additional information on the libusb 1.0 Windows backend please visit:
    http://windows.libusb.info
  - The MinGW and MS generated DLLs are fully interchangeable, provided that you
    use the import libs provided or generate one from the .def also provided.
  - If you find any issue, please visit http://libusb.info/ and check the
    Support section
