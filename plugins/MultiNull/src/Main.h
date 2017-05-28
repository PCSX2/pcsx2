#ifndef MULTINULL_MAIN_H
#define MULTINULL_MAIN_H
#include "Logging.h"
#define MULTINULL_PS2E_INTERFACE
#ifdef MULTINULL_PS2E_INTERFACE
#ifdef MULTINULL_BUILD_ALL
#define MULTINULL_BUILD_GS
#define MULTINULL_BUILD_PAD
#define MULTINULL_BUILD_SPU2
#define MULTINULL_BUILD_CDVD
#define MULTINULL_BUILD_DEV9
#define MULTINULL_BUILD_USB
#define MULTINULL_BUILD_FW
#endif
#ifdef MULTINULL_BUILD_GS
#define GSdefs
#endif
#ifdef MULTINULL_BUILD_PAD
#define PADdefs
#endif
#ifdef MULTINULL_BUILD_SPU2
#define SPU2defs //no enable_new_iopdma
#endif
#ifdef MULTINULL_BUILD_CDVD
#define CDVDdefs
#endif 
#ifdef MULTINULL_BUILD_DEV9
#define DEV9defs //not here either
#endif
#ifdef MULTINULL_BUILD_USB
#define USBdefs
#endif
#ifdef MULTINULL_BUILD_FW
#define FWdefs
#endif
#include "PS2Edefs.h"
#include "PS2Eext.h"
#endif
#endif