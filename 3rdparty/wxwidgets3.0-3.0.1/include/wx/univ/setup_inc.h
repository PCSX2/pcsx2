/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/setup_inc.h
// Purpose:     wxUniversal-specific setup.h options (this file is not used
//              directly, it is injected by build/update-setup-h in the
//              generated include/wx/univ/setup0.h)
// Author:      Vadim Zeitlin
// Created:     2008-02-03
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// wxUniversal-only options
// ----------------------------------------------------------------------------

// Set to 1 to enable compilation of all themes, this is the default
#define wxUSE_ALL_THEMES    1

// Set to 1 to enable the compilation of individual theme if wxUSE_ALL_THEMES
// is unset, if it is set these options are not used; notice that metal theme
// uses Win32 one
#define wxUSE_THEME_GTK     0
#define wxUSE_THEME_METAL   0
#define wxUSE_THEME_MONO    0
#define wxUSE_THEME_WIN32   0
