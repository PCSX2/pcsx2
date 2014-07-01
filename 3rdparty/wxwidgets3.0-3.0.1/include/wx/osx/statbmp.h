#if wxOSX_USE_CARBON
#include "wx/osx/carbon/statbmp.h"
#else
#define wxGenericStaticBitmap wxStaticBitmap
#include "wx/generic/statbmpg.h"
#endif
