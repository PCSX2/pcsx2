#ifndef CUBEB_EXPORT_H
#define CUBEB_EXPORT_H

#define CUBEB_EXPORT
#define CUBEB_NO_EXPORT

#ifdef WIN32
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")
#endif

#endif
